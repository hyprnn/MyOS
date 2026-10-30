"""The simulated host computer: disks, filesystems, mounts, users, shell session state.

Everything lives in plain dicts/lists (see vfs.py) so to_dict()/from_dict() is just a deep copy.
Methods raise vfs.FsError(reason) — commands prepend their own name/context.
"""
from __future__ import annotations

import copy
import random
import time
import uuid
from typing import Dict, List, NamedTuple, Optional, Tuple

from . import disks as dk
from . import packages
from .vfs import (FsError, file_node, link_node, mkdir_node, normpath, now, split,
                  tree_size)

HOSTNAME = "host"
CORES = 8
HOST_FS_BASE_USED = int(11.2 * dk.GiB)  # the host OS itself (not modelled file by file)

ROOT_ENV = {
    "HOME": "/root", "USER": "root", "LOGNAME": "root", "SHELL": "/bin/bash",
    "PATH": "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin",
    "TERM": "xterm-256color", "LANG": "en_US.UTF-8", "HOSTNAME": HOSTNAME,
}

# Versions reported by version-check.sh / --version (host tools, chapter 2.2)
HOST_TOOLS = {
    "bash": "5.2.37", "binutils": "2.43.1", "bison": "3.8.2", "coreutils": "9.5",
    "diffutils": "3.10", "findutils": "4.10.0", "gawk": "5.3.1", "gcc": "14.2.1",
    "g++": "14.2.1", "grep": "3.11", "gzip": "1.13", "m4": "1.4.19", "make": "4.4.1",
    "patch": "2.7.6", "perl": "5.40.0", "python": "3.12.7", "sed": "4.9", "tar": "1.35",
    "texinfo": "7.1.1", "xz": "5.6.3", "kernel": "6.11.5",
}

VERSION_CHECK_SH = """#!/bin/bash
# Simplified host system requirements check (chapter 2.2).
# Each line: ver_check <Name> <tool> <minimum version>
ver_check Coreutils sort 8.1
ver_check Bash      bash 3.2
ver_check Binutils  ld   2.13.1
ver_check Bison     bison 2.7
ver_check Diffutils diff 2.8.1
ver_check Findutils find 4.2.31
ver_check Gawk      gawk 4.0.1
ver_check GCC       gcc  5.2
ver_check "GCC (C++)" g++ 5.2
ver_check Grep      grep 2.5.1a
ver_check Gzip      gzip 1.3.12
ver_check M4        m4   1.4.10
ver_check Make      make 4.0
ver_check Patch     patch 2.5.4
ver_check Perl      perl 5.8.8
ver_check Python    python3 3.4
ver_check Sed       sed  4.1.5
ver_check Tar       tar  1.22
ver_check Texinfo   texi2any 5.0
ver_check Xz        xz   5.0.0
ver_check Kernel    uname 5.4
"""

BASH_BASHRC = """# /etc/bash.bashrc: system-wide settings of the host distribution,
# read by every interactive non-login bash.
alias ls='ls --color=auto'
# use the distributed compile farm at the office (needs network + distccd)
export CC="distcc gcc"
export CXX="distcc g++"
"""

TOOL_ALIASES = {"sort": "coreutils", "ld": "binutils", "diff": "diffutils", "find": "findutils",
                "python3": "python", "texi2any": "texinfo", "uname": "kernel"}


class Res(NamedTuple):
    path: str               # absolute path after resolving symlinks (as seen in a chroot)
    node: Optional[dict]    # None if the final component does not exist
    parent: Optional[dict]
    name: str
    real: str = ""          # the same path seen from the real root (differs in a chroot)


VIRTUAL_FS = ("proc", "sysfs", "devpts", "tmpfs", "devtmpfs")


class Machine:
    FIELDS = ("disks", "fs", "mounts", "swaps", "users", "groups", "user", "env", "cwd",
              "umask", "su_stack", "dirstack", "flags", "glitch_file", "clock", "croot",
              "chroot_name")

    def __init__(self):
        self.disks: Dict[str, dict] = {}
        self.fs: Dict[str, dict] = {}          # device path -> filesystem
        self.mounts: List[dict] = []           # {"dev", "path", "type"} in mount order
        self.swaps: List[str] = []
        self.users: Dict[str, dict] = {}
        self.groups: Dict[str, int] = {}
        self.user = "root"
        self.env: Dict[str, str] = dict(ROOT_ENV)
        self.cwd = "/root"
        self.umask = 0o022
        self.su_stack: List[dict] = []
        self.dirstack: List[str] = []
        self.flags: List[str] = []
        self.glitch_file = ""
        self.clock = 0.0                       # simulated seconds spent (make, tar, ...)
        self.croot = ""                        # real path of the chroot ("" = not chrooted)
        self.chroot_name = ""                  # user name bash found at start (in a chroot)
        self.last_status = 0
        self.rng = random.Random()

    # ------------------------------------------------------------ creation / persistence
    @classmethod
    def new(cls, seed: Optional[int] = None) -> "Machine":
        m = cls()
        rng = random.Random(seed)
        m.rng = rng
        sda = dk.new_disk("sda", 64 * dk.GiB, "Samsung SSD 870")
        sda["label"], sda["id"] = "gpt", dk.new_label_id("gpt", rng)
        efi_end = dk.ALIGN + 512 * 2048 - 1
        sda["parts"] = [
            {"n": 1, "start": dk.ALIGN, "end": efi_end, "type": "uefi"},
            {"n": 2, "start": efi_end + 1, "end": sda["sectors"] - 34, "type": "linux"},
        ]
        sdb = dk.new_disk("sdb", 32 * dk.GiB, "WDC WD320 (LFS target)")
        sdc = dk.new_disk("sdc", 8 * dk.GiB, "USB Flash Drive")
        sdc["label"], sdc["id"] = "dos", dk.new_label_id("dos", rng)
        sdc["parts"] = [{"n": 1, "start": dk.ALIGN, "end": sdc["sectors"] - 1, "type": "uefi"}]
        m.disks = {"sda": sda, "sdb": sdb, "sdc": sdc}

        m.fs["/dev/sda2"] = m._new_fs("ext4", rng, label="hostroot")
        m.fs["/dev/sda2"]["base_used"] = HOST_FS_BASE_USED
        m.fs["/dev/sda1"] = m._new_fs("vfat", rng, label="EFI")
        m.fs["/dev/sdc1"] = m._new_fs("vfat", rng, label="USB")
        m.mounts = [{"dev": "/dev/sda2", "path": "/", "type": "ext4"},
                    {"dev": "/dev/sda1", "path": "/boot/efi", "type": "vfat"}]
        m._build_host_tree()
        m.fs["/dev/sda1"]["root"]["c"]["EFI"] = mkdir_node()
        m.fs["/dev/sdc1"]["root"]["c"]["photos"] = mkdir_node()
        m.fs["/dev/sdc1"]["root"]["c"]["README.txt"] = file_node(
            "This USB stick holds someone's holiday photos. Do not format it!\n")
        m.users = {"root": {"uid": 0, "group": "root", "home": "/root", "shell": "/bin/bash"}}
        m.groups = {"root": 0, "disk": 6, "users": 100}
        m.sync_etc()
        m.sync_dev()
        m.glitch_file = rng.choice([p.file for p in packages.PACKAGES if p.size > 5 * 1024 * 1024])
        return m

    def _new_fs(self, fstype: str, rng=None, label="") -> dict:
        rng = rng or self.rng
        if fstype == "vfat":
            u = "%04X-%04X" % (rng.getrandbits(16), rng.getrandbits(16))
        else:
            u = str(uuid.UUID(int=rng.getrandbits(128), version=4))
        fs = {"type": fstype, "uuid": u, "label": label,
              "created": time.strftime("%a %b %d %H:%M:%S %Y")}
        if fstype != "swap":
            root = mkdir_node()
            if fstype.startswith("ext"):
                root["c"]["lost+found"] = mkdir_node(0o700)
            fs["root"] = root
        return fs

    def _build_host_tree(self):
        root = self.fs["/dev/sda2"]["root"]
        root["c"].pop("lost+found", None)
        c = root["c"]
        for name in ("boot", "dev", "etc", "home", "mnt", "opt", "proc", "root", "srv", "sys",
                     "usr", "var"):
            c[name] = mkdir_node(0o700 if name == "root" else 0o755)
        c["tmp"] = mkdir_node(0o1777)
        for name in ("bin", "lib", "sbin"):
            c[name] = link_node(f"usr/{name}")
        c["lib64"] = link_node("usr/lib")
        c["boot"]["c"]["efi"] = mkdir_node()
        c["boot"]["c"]["vmlinuz-linux"] = file_node(size=13_900_000)
        for name in ("bin", "lib", "sbin", "share", "include", "local"):
            c["usr"]["c"][name] = mkdir_node()
        for name in ("log", "cache", "tmp"):
            c["var"]["c"][name] = mkdir_node()
        etc = c["etc"]["c"]
        etc["hostname"] = file_node(HOSTNAME + "\n")
        etc["bash.bashrc"] = file_node(BASH_BASHRC)
        etc["os-release"] = file_node('NAME="Host Linux"\nPRETTY_NAME="Host Linux (simulated)"\n'
                                      'ID=hostlinux\n')
        etc["fstab"] = file_node("# <device>  <dir>  <type>  <options>  <dump> <pass>\n"
                                 "/dev/sda2   /          ext4  defaults  0 1\n"
                                 "/dev/sda1   /boot/efi  vfat  defaults  0 2\n")
        home = c["root"]["c"]
        home[".bashrc"] = file_node("# ~/.bashrc of the host's root user\nalias ll='ls -l'\n")
        home["version-check.sh"] = file_node(VERSION_CHECK_SH, mode=0o755)
        home["README"] = file_node(
            "You are root on the HOST system. The empty disk for your LFS system\n"
            "is somewhere in /dev. Read the LFS book (F2) and type 'tasks'.\n")

    def to_dict(self) -> dict:
        return copy.deepcopy({k: getattr(self, k) for k in self.FIELDS})

    @classmethod
    def from_dict(cls, data: dict) -> "Machine":
        """Restore from to_dict() output. Raises ValueError/TypeError on a malformed dict."""
        if not isinstance(data, dict):
            raise TypeError("machine is not a dict")
        m = cls()
        for k in cls.FIELDS:
            if k in data:
                want = type(getattr(m, k))
                if want is float and isinstance(data[k], int):
                    data = dict(data, **{k: float(data[k])})
                if not isinstance(data[k], want) or isinstance(data[k], bool):
                    raise ValueError(f"machine.{k}: expected {want.__name__}")
                setattr(m, k, copy.deepcopy(data[k]))
        for k in ("disks", "fs", "mounts", "users"):
            if not getattr(m, k):
                raise ValueError(f"machine.{k} is missing")
        root = [mt for mt in m.mounts if mt.get("path") == "/"]
        if not root or "root" not in m.fs.get(root[0].get("dev"), {}):
            raise ValueError("machine has no root filesystem")
        if m.user not in m.users:
            raise ValueError("unknown current user")
        m.upgrade()
        return m

    def upgrade(self):
        """Bring machines from older saves up to date (new host files etc.)."""
        etc = self.fs["/dev/sda2"]["root"]["c"].get("etc", {}).get("c")
        if etc is not None and "bash.bashrc" not in etc and "bash.bashrc.NOUSE" not in etc:
            etc["bash.bashrc"] = file_node(BASH_BASHRC)
        dev = self.fs["/dev/sda2"]["root"]["c"].get("dev", {}).get("c", {})
        if "pts" not in dev:
            self.sync_dev()

    # ------------------------------------------------------------ session
    @property
    def is_root(self) -> bool:
        return self.user == "root"

    def home(self, user: Optional[str] = None) -> str:
        return self.users.get(user or self.user, {}).get("home", "/")

    def prompt(self) -> str:
        cwd = self.cwd
        h = self.env.get("HOME") or self.home()
        if h != "/" and (cwd == h or cwd.startswith(h + "/")):
            cwd = "~" + cwd[len(h):]
        sign = "#" if self.is_root else "$"
        ps1 = self.env.get("PS1")
        uname = self.user
        if self.croot:  # bash looked the name up in the chroot's /etc/passwd when it started
            uname = self.chroot_name or "I have no name!"
        if not ps1:
            return f"{uname}@{HOSTNAME}:{cwd}{sign} "
        out, i = [], 0
        while i < len(ps1):
            c = ps1[i]
            if c == "\\" and i + 1 < len(ps1):
                e = ps1[i + 1]
                out.append({"u": uname, "h": HOSTNAME, "H": HOSTNAME, "w": cwd,
                            "W": cwd.rstrip("/").rsplit("/", 1)[-1] or "/", "$": sign,
                            "\\": "\\", "[": "", "]": "", "n": " "}.get(e, "\\" + e))
                i += 2
            else:
                out.append(c)
                i += 1
        return "".join(out)

    def flag(self, name: str):
        if name not in self.flags:
            self.flags.append(name)

    def has_flag(self, name: str) -> bool:
        return name in self.flags

    # ------------------------------------------------------------ paths
    def abspath(self, path: str) -> str:
        if not path.startswith("/"):
            path = self.cwd.rstrip("/") + "/" + path
        return normpath(path)

    def _mount_root(self, path: str) -> Optional[dict]:
        for m in reversed(self.mounts):
            if m["path"] == path:
                if m.get("bind"):
                    return self._resolve_real(m["bind"]).node
                return self.fs[m.get("fs", m["dev"])]["root"]
        return None

    def _resolve_real(self, path: str) -> Res:
        saved, self.croot = self.croot, ""
        try:
            return self.resolve(path)
        finally:
            self.croot = saved

    def real_path(self, path: str) -> str:
        """Path as seen from the real root (for mount tables etc.)."""
        p = self.abspath(path)
        return normpath(self.croot + p) if self.croot else p

    def resolve(self, path: str, follow: bool = True) -> Res:
        queue = split(self.abspath(path))
        root = self._mount_root("/")
        names: List[str] = []
        nodes: List[dict] = [root]
        if self.croot:  # walk down to the chroot directory first; '..' cannot leave it
            base = self._resolve_real(self.croot)
            if base.node is None or base.node["t"] != "d":
                raise FsError("No such file or directory")
            names = split(base.path)
            nodes = [root]
            for i in range(len(names)):
                nodes.append(self._resolve_real("/" + "/".join(names[:i + 1])).node)
        floor = len(names)
        floor_names, floor_nodes = list(names), list(nodes)
        hops = 0
        while queue:
            comp = queue.pop(0)
            if comp == ".":
                continue
            if comp == "..":
                if len(names) > floor:
                    names.pop()
                    nodes.pop()
                continue
            cur = nodes[-1]
            if cur["t"] != "d":
                raise FsError("Not a directory")
            p = "/" + "/".join(names + [comp])
            child = self._mount_root(p) or cur["c"].get(comp)
            if child is None:
                if queue:
                    raise FsError("No such file or directory")
                return Res("/" + "/".join(names[floor:] + [comp]), None, cur, comp, p)
            if child["t"] == "l" and (queue or follow):
                hops += 1
                if hops > 40:
                    raise FsError("Too many levels of symbolic links")
                target = child["to"]
                if target.startswith("/"):
                    names, nodes = list(floor_names), list(floor_nodes)
                queue = split(target) + queue
                continue
            names.append(comp)
            nodes.append(child)
        real = "/" + "/".join(names)
        return Res("/" + "/".join(names[floor:]), nodes[-1],
                   nodes[-2] if len(nodes) > 1 else None, names[-1] if len(names) > floor else "",
                   real)

    def lookup(self, path: str, follow: bool = True) -> dict:
        r = self.resolve(path, follow)
        if r.node is None:
            raise FsError("No such file or directory")
        return r.node

    def exists(self, path: str, follow: bool = True) -> bool:
        try:
            return self.resolve(path, follow).node is not None
        except FsError:
            return False

    def isdir(self, path: str) -> bool:
        try:
            return self.lookup(path)["t"] == "d"
        except FsError:
            return False

    def fs_of(self, path: str) -> Tuple[str, dict]:
        """Device and filesystem that hold a path (the deepest mount that is a prefix)."""
        p = self.resolve(path).real
        best = None
        for m in self.mounts:
            mp = m["path"]
            if p == mp or mp == "/" or p.startswith(mp + "/"):
                if best is None or len(mp) >= len(best["path"]):
                    best = m
        if best.get("bind"):
            rest = p[len(best["path"]):]
            saved, self.croot = self.croot, ""
            try:
                return self.fs_of(best["bind"] + rest)
            finally:
                self.croot = saved
        key = best.get("fs", best["dev"])
        return key, self.fs[key]

    # ------------------------------------------------------------ permissions
    def can_write(self, node: dict) -> bool:
        if self.is_root:
            return True
        m = node["m"]
        if node["u"] == self.user and m & 0o200:
            return True
        if node["g"] == self.users.get(self.user, {}).get("group") and m & 0o020:
            return True
        return bool(m & 0o002)

    def _check_parent(self, r: Res):
        if r.parent is None:
            raise FsError("File exists")
        if not self.can_write(r.parent):
            raise FsError("Permission denied")

    def _owner(self) -> Tuple[str, str]:
        return self.user, self.users.get(self.user, {}).get("group", self.user)

    # ------------------------------------------------------------ file operations
    def mkdir(self, path: str, parents: bool = False) -> List[str]:
        """Return the list of created directories (absolute, unresolved spelling)."""
        target = self.abspath(path)
        todo = [target]
        if parents:
            todo = []
            acc = ""
            for comp in split(target):
                acc += "/" + comp
                todo.append(acc)
        created = []
        for p in todo:
            r = self.resolve(p)
            if r.node is not None:
                if p == target and not parents:
                    raise FsError("File exists")
                if r.node["t"] != "d":
                    raise FsError("File exists" if p == target else "Not a directory")
                continue
            self._check_parent(r)
            u, g = self._owner()
            r.parent["c"][r.name] = mkdir_node(0o777 & ~self.umask, u, g)
            r.parent["mt"] = now()
            created.append(p)
        return created

    def write_file(self, path: str, data: str, append: bool = False, **extra) -> dict:
        r = self.resolve(path)
        if r.node is not None and r.node["t"] == "b":
            if r.path == "/dev/null":
                return r.node
            raise FsError("writing to raw devices is not simulated")
        if r.node is not None:
            if r.node["t"] == "d":
                raise FsError("Is a directory")
            if not self.can_write(r.node):
                raise FsError("Permission denied")
            if append:
                r.node["data"] = r.node.get("data", "") + data
            else:
                r.node["data"] = data
                r.node.pop("md5", None)
            r.node["size"] = len(r.node["data"].encode("utf-8"))
            r.node["mt"] = now()
            r.node.update(extra)
            return r.node
        self._check_parent(r)
        u, g = self._owner()
        node = file_node(data, 0o666 & ~self.umask, u, g)
        node.update(extra)
        r.parent["c"][r.name] = node
        return node

    def put_blob(self, path: str, size: int, md5: str, partial: bool = False,
                 data: str = "") -> dict:
        """Create/replace a downloaded file (contents are not modelled, only size + checksum)."""
        node = self.write_file(path, data)
        node["size"] = size
        node["md5"] = md5
        if partial:
            node["partial"] = True
        else:
            node.pop("partial", None)
        return node

    def remove(self, path: str, recursive: bool = False):
        r = self.resolve(path, follow=False)
        if r.node is None:
            raise FsError("No such file or directory")
        if r.parent is None or self.is_mountpoint(r.real):
            raise FsError("Device or resource busy")
        if r.node["t"] == "d":
            if not recursive:
                raise FsError("Is a directory")
            if self._has_mount_below(r.real):
                raise FsError("Device or resource busy")
        if not self.can_write(r.parent):
            raise FsError("Permission denied")
        if r.parent["m"] & 0o1000 and not self.is_root and r.node["u"] != self.user:
            raise FsError("Operation not permitted")
        del r.parent["c"][r.name]

    def rmdir(self, path: str):
        r = self.resolve(path, follow=False)
        if r.node is None:
            raise FsError("No such file or directory")
        if r.node["t"] != "d":
            raise FsError("Not a directory")
        if r.node["c"]:
            raise FsError("Directory not empty")
        self.remove(path, recursive=True)

    def symlink(self, target: str, path: str):
        r = self.resolve(path, follow=False)
        if r.node is not None and r.node["t"] == "d":  # ln -s x dir/  => dir/x
            r = self.resolve(path.rstrip("/") + "/" + split(target)[-1], follow=False)
        if r.node is not None:
            raise FsError("File exists")
        self._check_parent(r)
        u, g = self._owner()
        r.parent["c"][r.name] = link_node(target, u, g)
        return r.path

    def is_mountpoint(self, path: str) -> bool:
        return any(m["path"] == path for m in self.mounts)

    def _has_mount_below(self, path: str) -> bool:
        return any(m["path"].startswith(path.rstrip("/") + "/") for m in self.mounts)

    # ------------------------------------------------------------ devices
    def devices(self) -> List[str]:
        out = []
        for name in sorted(self.disks):
            disk = self.disks[name]
            out.append(f"/dev/{name}")
            out += [dk.part_dev(disk, p) for p in sorted(disk["parts"], key=lambda p: p["n"])]
        return out

    def find_dev(self, dev: str) -> Tuple[Optional[dict], Optional[dict]]:
        """('/dev/sdb2') -> (disk, part); ('/dev/sdb') -> (disk, None); unknown -> (None, None)."""
        dev = dev if dev.startswith("/dev/") else self.abspath(dev)
        for name, disk in self.disks.items():
            if dev == f"/dev/{name}":
                return disk, None
            for p in disk["parts"]:
                if dev == dk.part_dev(disk, p):
                    return disk, p
        return None, None

    def dev_bytes(self, dev: str) -> int:
        disk, part = self.find_dev(dev)
        if disk is None:
            return 0
        return dk.part_bytes(part) if part else disk["sectors"] * dk.SECTOR

    def mount_of(self, dev: str) -> Optional[str]:
        for m in self.mounts:
            if m["dev"] == dev:
                return m["path"]
        return None

    def dev_in_use(self, dev: str) -> bool:
        return self.mount_of(dev) is not None or dev in self.swaps

    def disk_in_use(self, disk: dict) -> bool:
        devs = [f"/dev/{disk['name']}"] + [dk.part_dev(disk, p) for p in disk["parts"]]
        return any(self.dev_in_use(d) for d in devs)

    def sync_dev(self):
        """Regenerate /dev on the host root filesystem from the disk table."""
        dev = mkdir_node()
        for name in ("null", "zero", "random", "urandom", "tty", "console"):
            dev["c"][name] = {"t": "b", "m": 0o666, "u": "root", "g": "root", "mt": now(),
                              "size": 0, "data": ""}
        for path in self.devices():
            dev["c"][path[5:]] = {"t": "b", "m": 0o660, "u": "root", "g": "disk", "mt": now(),
                                  "size": 0, "data": ""}
        dev["c"]["pts"] = mkdir_node()
        dev["c"]["shm"] = mkdir_node(0o1777)
        self.fs["/dev/sda2"]["root"]["c"]["dev"] = dev

    def apply_table(self, name: str, new_disk: dict):
        """fdisk 'w': store the new table; drop filesystems whose partitions moved or vanished."""
        old = self.disks[name]
        old_parts = {p["n"]: p for p in old["parts"]}
        new_parts = {p["n"]: p for p in new_disk["parts"]}
        for n, p in old_parts.items():
            q = new_parts.get(n)
            if q is None or q["start"] != p["start"] or q["end"] != p["end"] \
                    or new_disk["label"] != old["label"]:
                self.fs.pop(f"/dev/{name}{n}", None)
        self.fs.pop(f"/dev/{name}", None)  # a whole-disk filesystem is gone once a table exists
        self.disks[name] = copy.deepcopy(new_disk)
        self.sync_dev()

    def wipe_table(self, name: str):
        disk = self.disks[name]
        for p in disk["parts"]:
            self.fs.pop(dk.part_dev(disk, p), None)
        disk["label"], disk["id"], disk["parts"] = None, "", []
        self.sync_dev()

    def make_fs(self, dev: str, fstype: str, label: str = "") -> dict:
        fs = self._new_fs(fstype, label=label)
        self.fs[dev] = fs
        return fs

    def _virtual_tree(self, fstype: str) -> dict:
        root = mkdir_node(0o555 if fstype in ("proc", "sysfs") else 0o755)
        if fstype == "proc":
            for name, data in (("version", f"Linux version {HOST_TOOLS['kernel']}-host (gcc "
                                           f"14.2.1) #1 SMP PREEMPT_DYNAMIC\n"),
                               ("cpuinfo", "processor\t: 0\nmodel name\t: Simulated x86-64 "
                                           "CPU\n" * 1),
                               ("meminfo", "MemTotal:       16384000 kB\n"),
                               ("filesystems", "nodev\tsysfs\nnodev\tproc\nnodev\ttmpfs\n"
                                               "nodev\tdevpts\n\text4\n\tvfat\n")):
                root["c"][name] = file_node(data, 0o444)
            root["c"]["self"] = mkdir_node(0o555)
            root["c"]["self"]["c"]["mounts"] = file_node("", 0o444)
        elif fstype == "sysfs":
            for name in ("block", "bus", "class", "devices", "firmware", "fs", "kernel"):
                root["c"][name] = mkdir_node()
        elif fstype == "devpts":
            root["c"]["ptmx"] = {"t": "b", "m": 0o666, "u": "root", "g": "tty", "mt": now(),
                                 "size": 0, "data": ""}
        return root

    def refresh_proc(self):
        """Keep /proc/self/mounts of every mounted proc in sync with the mount table."""
        text = "".join(f"{mt['dev']} {mt['path']} {mt['type']} rw,relatime 0 0\n"
                       for mt in self.mounts)
        for mt in self.mounts:
            if mt["type"] == "proc":
                self.fs[mt["fs"]]["root"]["c"]["self"]["c"]["mounts"]["data"] = text

    def mount(self, dev: str, path: str, fstype: Optional[str] = None, bind: bool = False):
        target = self.resolve(path)
        if target.node is None:
            raise FsError(f"{path}: mount point does not exist.")
        if target.node["t"] != "d":
            raise FsError(f"{path}: mount point is not a directory.")
        if bind:
            src = self.resolve(dev)
            if src.node is None:
                raise FsError(f"{path}: special device {dev} does not exist.")
            self.mounts.append({"dev": src.real, "path": target.real, "type": "none",
                                "bind": src.real})
            self.refresh_proc()
            return
        if fstype in VIRTUAL_FS:
            key = f"{fstype}@{target.real}"
            self.fs[key] = {"type": fstype, "uuid": "", "label": "", "virtual": True,
                            "root": self._virtual_tree(fstype)}
            self.mounts.append({"dev": dev, "path": target.real, "type": fstype, "fs": key})
            self.refresh_proc()
            return
        disk, part = self.find_dev(dev)
        if disk is None:
            raise FsError(f"{path}: special device {dev} does not exist.")
        fs = self.fs.get(dev)
        bad = (f"{path}: wrong fs type, bad option, bad superblock on {dev}, missing codepage "
               f"or helper program, or other error.")
        if fs is None or fs["type"] == "swap":
            raise FsError(bad)
        if fstype and fstype != fs["type"] and not (fstype.startswith("ext")
                                                   and fs["type"].startswith("ext")):
            raise FsError(bad)
        where = self.mount_of(dev)
        if where is not None:
            raise FsError(f"{path}: {dev} already mounted on {where}.")
        self.mounts.append({"dev": dev, "path": target.real, "type": fs["type"]})
        self.refresh_proc()

    def umount(self, what: str) -> str:
        p = self.real_path(what) if not what.startswith("/dev/") else what
        try:  # umount resolves symlinks in the mount point path
            p = self.resolve(what).real if not what.startswith("/dev/") else p
        except FsError:
            pass
        cwd = self.real_path(self.cwd)
        for m in reversed(self.mounts):
            if m["path"] == p or (m["dev"] == p and not m.get("bind")):
                if m["path"] == "/":
                    raise FsError("/: target is busy.")
                if cwd == m["path"] or cwd.startswith(m["path"].rstrip("/") + "/"):
                    raise FsError(f"{what}: target is busy.")
                if self._has_mount_below(m["path"]):
                    raise FsError(f"{what}: target is busy.")
                self.mounts.remove(m)
                if m.get("fs"):
                    self.fs.pop(m["fs"], None)
                self.refresh_proc()
                return what
        raise FsError(f"{what}: not mounted.")

    def fs_usage(self, dev: str) -> Tuple[int, int]:
        """(size, used) in bytes for df."""
        fs = self.fs[dev]
        if fs.get("virtual"):
            return 8 * dk.GiB, tree_size(fs["root"]) if fs["type"] == "tmpfs" else 0
        size = self.dev_bytes(dev)
        overhead = int(size * 0.016) if fs["type"].startswith("ext") else 4096
        used = overhead + fs.get("base_used", 0) + tree_size(fs["root"])
        return size, min(used, size)

    # ------------------------------------------------------------ users
    def sync_etc(self):
        etc = self.fs["/dev/sda2"]["root"]["c"]["etc"]["c"]
        pw = "".join(f"{n}:x:{u['uid']}:{self.groups.get(u['group'], 0)}::{u['home']}:{u['shell']}\n"
                     for n, u in sorted(self.users.items(), key=lambda kv: kv[1]["uid"]))
        gr = "".join(f"{n}:x:{gid}:\n" for n, gid in sorted(self.groups.items(), key=lambda kv: kv[1]))
        etc["passwd"] = file_node(pw)
        etc["group"] = file_node(gr)

    def add_group(self, name: str):
        if name in self.groups:
            raise FsError(f"group '{name}' already exists")
        self.groups[name] = max([g for g in self.groups.values() if g < 60000] + [999]) + 1
        self.sync_etc()

    def add_user(self, name: str, group: str, home: str, shell: str, create_home: bool):
        if name in self.users:
            raise FsError(f"user '{name}' already exists")
        if group not in self.groups:
            raise FsError(f"group '{group}' does not exist")
        uid = max([u["uid"] for u in self.users.values() if u["uid"] < 60000] + [999]) + 1
        self.users[name] = {"uid": uid, "group": group, "home": home, "shell": shell}
        if create_home:
            r = self.resolve(home)
            if r.node is None and r.parent is not None:
                r.parent["c"][r.name] = mkdir_node(0o700, name, group)
        self.sync_etc()

    def _push_session(self):
        self.su_stack.append({"user": self.user, "env": self.env, "cwd": self.cwd,
                              "umask": self.umask, "dirstack": self.dirstack,
                              "croot": self.croot, "chroot_name": self.chroot_name})

    def chroot(self, newroot: str):
        """Start a session whose '/' is newroot (a real path). Root only."""
        self._push_session()
        self.croot = newroot.rstrip("/")
        self.cwd = "/"
        self.dirstack = []
        self.env = dict(self.env)
        self.refresh_name()

    def refresh_name(self):
        """What bash does at start: find the name of our uid in /etc/passwd."""
        if self.croot:
            self.chroot_name = self.db_name_of_uid(self.users.get(self.user, {}).get("uid", 0))
            if self.chroot_name:
                self.flag("chroot_named")

    # inside a chroot the user database is the chroot's /etc/passwd and /etc/group
    def _db_file(self, name: str) -> str:
        try:
            node = self.resolve(f"/etc/{name}").node
        except FsError:
            return ""
        return (node or {}).get("data", "") if node and node["t"] == "f" else ""

    def db_users(self) -> Dict[str, int]:
        if not self.croot:
            return {n: u["uid"] for n, u in self.users.items()}
        out = {}
        for line in self._db_file("passwd").splitlines():
            parts = line.split(":")
            if len(parts) >= 3 and parts[2].isdigit():
                out[parts[0]] = int(parts[2])
        return out

    def db_groups(self) -> Dict[str, int]:
        if not self.croot:
            return dict(self.groups)
        out = {}
        for line in self._db_file("group").splitlines():
            parts = line.split(":")
            if len(parts) >= 3 and parts[2].isdigit():
                out[parts[0]] = int(parts[2])
        return out

    def db_name_of_uid(self, uid: int) -> str:
        for name, u in self.db_users().items():
            if u == uid:
                return name
        return ""

    def display_owner(self, name: str, group: bool = False) -> str:
        """Owner name as 'ls -l' shows it: a number if the name is unknown (chroot)."""
        if not self.croot:
            return name
        db = self.db_groups() if group else self.db_users()
        if name in db:
            return name
        host = self.groups.get(name) if group else self.users.get(name, {}).get("uid")
        return str(host if host is not None else name)

    def su(self, name: str):
        self._push_session()
        u = self.users[name]
        self.user = name
        self.env = {"HOME": u["home"], "USER": name, "LOGNAME": name, "SHELL": u["shell"],
                    "PATH": "/usr/local/bin:/usr/bin:/bin", "TERM": ROOT_ENV["TERM"],
                    "LANG": ROOT_ENV["LANG"], "HOSTNAME": HOSTNAME}
        self.cwd = u["home"] if self.isdir(u["home"]) else "/"
        self.umask = 0o022
        self.dirstack = []

    def logout(self) -> bool:
        """Leave an 'su' session. False if this is the login shell."""
        if not self.su_stack:
            return False
        s = self.su_stack.pop()
        self.user, self.env, self.cwd = s["user"], s["env"], s["cwd"]
        self.umask, self.dirstack = s["umask"], s["dirstack"]
        self.croot, self.chroot_name = s.get("croot", ""), s.get("chroot_name", "")
        return True
