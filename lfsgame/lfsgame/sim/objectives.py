"""Game objectives: predicates over the Machine, each linked to an LFS book section.

Completion is sticky (stored in Game.done), so later changes never "un-complete" a goal.
"""
from __future__ import annotations

from typing import Callable, List, NamedTuple, Optional

from . import disks as dk
from . import packages
from .machine import Machine
from .vfs import FsError, node_md5

LFS = "/mnt/lfs"
MIN_ROOT = 10 * dk.GiB
EXT = ("ext2", "ext3", "ext4")


class Objective(NamedTuple):
    id: str
    title: str
    section: str          # book section id
    hint: str
    check: Callable[[Machine], bool]
    optional: bool = False


def _root_parts(m: Machine) -> List[str]:
    disk = m.disks["sdb"]
    if not disk["label"]:
        return []
    return [dk.part_dev(disk, p) for p in disk["parts"]
            if p["type"] == "linux" and dk.part_bytes(p) >= MIN_ROOT]


def _lfs_mount(m: Machine) -> Optional[str]:
    for mt in m.mounts:
        if mt["path"] == LFS and mt["dev"] in _root_parts(m) and mt["type"] in EXT:
            return mt["dev"]
    return None


def _on_lfs(m: Machine, path: str) -> bool:
    dev = _lfs_mount(m)
    try:
        return dev is not None and m.exists(path) and m.fs_of(path)[0] == dev
    except FsError:
        return False


def _node(m: Machine, path: str, follow=True):
    try:
        return m.resolve(path, follow).node
    except FsError:
        return None


def _sources_ok(m: Machine) -> bool:
    if not _on_lfs(m, LFS + "/sources"):
        return False
    node = _node(m, LFS + "/sources")
    return node is not None and node["t"] == "d" and node["m"] & 0o1777 == 0o1777


def _downloads_ok(m: Machine) -> bool:
    if not _on_lfs(m, LFS + "/sources"):
        return False
    for p in packages.PACKAGES:
        node = _node(m, f"{LFS}/sources/{p.file}")
        if node is None or node["t"] != "f" or node.get("partial") \
                or node_md5(node) != packages.fake_md5(p.file):
            return False
    return True


LAYOUT_DIRS = ("etc", "var", "usr", "usr/bin", "usr/lib", "usr/sbin", "tools", "lib64")


def _layout_ok(m: Machine) -> bool:
    for d in LAYOUT_DIRS:
        n = _node(m, f"{LFS}/{d}")
        if n is None or n["t"] != "d" or not _on_lfs(m, f"{LFS}/{d}"):
            return False
    for d in ("bin", "lib", "sbin"):
        n = _node(m, f"{LFS}/{d}", follow=False)
        if n is None or n["t"] != "l" or n["to"].strip("/") not in (f"usr/{d}", f"{LFS}/usr/{d}".strip("/")):
            return False
    return True


def _owner_ok(m: Machine) -> bool:
    if "lfs" not in m.users or not _layout_ok(m):
        return False
    return all(_node(m, f"{LFS}/{d}")["u"] == "lfs" for d in LAYOUT_DIRS)


OBJECTIVES: List[Objective] = [
    Objective("explore", "Find the empty disk meant for LFS (lsblk or fdisk -l)", "2.4",
              "Type 'lsblk'. Which disk has no partitions at all?",
              lambda m: m.has_flag("looked_at_disks")),
    Objective("partition", "Partition /dev/sdb: a Linux partition of at least 10 GiB", "2.4",
              "fdisk /dev/sdb, then: g (new GPT), n (new partition, e.g. last sector +20G), w (write).",
              lambda m: bool(_root_parts(m))),
    Objective("mkfs", "Create an ext4 filesystem on the new partition", "2.5",
              "mkfs -v -t ext4 /dev/sdb1   (use the partition you created, NOT the disk)",
              lambda m: any(m.fs.get(d, {}).get("type") in EXT for d in _root_parts(m))),
    Objective("swap", "Optional: make and enable a swap partition", "2.5",
              "Create a partition of type 'swap' with fdisk (t), then mkswap and swapon -v.",
              lambda m: any(d.startswith("/dev/sdb") for d in m.swaps), optional=True),
    Objective("env", "Set the LFS variable: export LFS=/mnt/lfs (umask 022)", "2.6",
              "export LFS=/mnt/lfs   then check it with: echo $LFS",
              lambda m: m.env.get("LFS") == LFS and m.umask == 0o022),
    Objective("mount", "Mount the LFS partition at /mnt/lfs", "2.7",
              "mkdir -pv $LFS   then   mount -v -t ext4 /dev/sdb1 $LFS",
              lambda m: _lfs_mount(m) is not None),
    Objective("sources", "Create $LFS/sources, writable and sticky (chmod a+wt)", "3.1",
              "mkdir -v $LFS/sources   then   chmod -v a+wt $LFS/sources",
              _sources_ok),
    Objective("download", "Download every source package into $LFS/sources", "3.1",
              "Get wget-list-sysv (see book 3.1), then wget --input-file=... --continue "
              "--directory-prefix=$LFS/sources",
              _downloads_ok),
    Objective("md5", "Verify the packages: md5sum -c md5sums (all OK)", "3.1",
              "pushd $LFS/sources; md5sum -c md5sums; popd",
              lambda m: m.has_flag("md5_ok")),
    Objective("layout", "Create the limited directory layout in $LFS", "4.2",
              "mkdir -pv $LFS/{etc,var} $LFS/usr/{bin,lib,sbin}, the bin/lib/sbin symlinks, "
              "$LFS/lib64 and $LFS/tools",
              _layout_ok),
    Objective("user", "Add the unprivileged 'lfs' user and group", "4.3",
              "groupadd lfs   then   useradd -s /bin/bash -g lfs -m -k /dev/null lfs",
              lambda m: "lfs" in m.users and "lfs" in m.groups),
    Objective("owner", "Give user lfs ownership of the $LFS directories", "4.3",
              "chown -v lfs $LFS/{usr{,/*},lib,var,etc,bin,sbin,tools}   and   chown -v lfs $LFS/lib64",
              _owner_ok),
    Objective("su", "Log in as lfs: su - lfs", "4.3",
              "su - lfs   (type 'exit' to return to root)",
              lambda m: m.has_flag("su_lfs")),
]

BY_ID = {o.id: o for o in OBJECTIVES}
