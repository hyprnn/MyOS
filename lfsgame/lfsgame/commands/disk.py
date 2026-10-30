"""Disk commands: lsblk, blkid, fdisk, mkfs*, mkswap, swapon/off, mount, umount, df, free."""
from __future__ import annotations

from ..shell.opts import Opts
from ..shell.runner import Input, Sleep, UsageError, command
from ..sim import disks as dk
from ..sim.vfs import FsError, human
from . import fdisk as fdisk_mod

G = "disks"
EXT = ("ext2", "ext3", "ext4")


def _need_root(m, name):
    if not m.is_root:
        raise FsError("Permission denied (are you root?)")


def _majmin(disk_name, n=0):
    return f"8:{(ord(disk_name[2]) - ord('a')) * 16 + n}"


@command("lsblk", "list block devices (-f: show filesystems)", G)
def _lsblk(ctx, args):
    m = ctx.machine
    o = Opts(args, "fap")
    m.flag("looked_at_disks")
    if "f" in o:
        rows = [("NAME", "FSTYPE", "LABEL", "UUID", "MOUNTPOINTS")]
    else:
        rows = [("NAME", "MAJ:MIN", "RM", "SIZE", "RO", "TYPE", "MOUNTPOINTS")]
    for name in sorted(m.disks):
        disk = m.disks[name]
        parts = sorted(disk["parts"], key=lambda p: p["n"])
        entries = [(name, f"/dev/{name}", disk["sectors"] * dk.SECTOR, "disk", 0)]
        for i, p in enumerate(parts):
            branch = "└─" if i == len(parts) - 1 else "├─"
            entries.append((branch + f"{name}{p['n']}", dk.part_dev(disk, p), dk.part_bytes(p),
                            "part", p["n"]))
        for label, dev, size, kind, n in entries:
            mp = m.mount_of(dev) or ("[SWAP]" if dev in m.swaps else "")
            if "f" in o:
                fs = m.fs.get(dev, {})
                rows.append((label, fs.get("type", ""), fs.get("label", ""), fs.get("uuid", ""), mp))
            else:
                rm = "1" if "USB" in disk["model"] else "0"
                rows.append((label, _majmin(name, n), rm, human(size), "0", kind, mp))
    _table(ctx, rows, right={1, 2, 3, 4} if "f" not in o else set())


def _table(ctx, rows, right=frozenset()):
    widths = [max(len(r[i]) for r in rows) for i in range(len(rows[0]))]
    for k, r in enumerate(rows):
        cells = [c.rjust(w) if i in right else c.ljust(w) for i, (c, w) in enumerate(zip(r, widths))]
        ctx.write(" ".join(cells).rstrip() + "\n", "accent" if k == 0 else None)


@command("blkid", "show filesystem UUIDs and types", G)
def _blkid(ctx, args):
    m = ctx.machine
    devs = args or [d for d in m.devices() if d in m.fs]
    status = 2
    for d in devs:
        fs = m.fs.get(d)
        if not fs:
            continue
        status = 0
        label = f'LABEL="{fs["label"]}" ' if fs.get("label") else ""
        ctx.write(f'{d}: {label}UUID="{fs["uuid"]}" TYPE="{fs["type"]}"\n')
    return status


@command("fdisk", "partition a disk: fdisk /dev/sdX  (fdisk -l: list)", G)
def _fdisk(ctx, args):
    m = ctx.machine
    o = Opts(args, "l")
    if "l" in o:
        m.flag("looked_at_disks")
        names = [a[5:] for a in o.args] if o.args else sorted(m.disks)
        for i, name in enumerate(names):
            if name not in m.disks:
                ctx.write(f"fdisk: cannot open /dev/{name}: No such file or directory\n", "err")
                continue
            if i:
                ctx.write("\n\n")
            fdisk_mod.print_table(ctx, m.disks[name])
        return 0
    if len(o.args) != 1:
        raise UsageError("usage: fdisk /dev/sdX   or   fdisk -l")
    dev = o.args[0]
    if not m.is_root:
        raise FsError(f"cannot open {dev}: Permission denied")
    disk, part = m.find_dev(dev)
    if disk is None:
        raise FsError(f"cannot open {dev}: No such file or directory")
    if part is not None:
        raise FsError(f"{dev} is a partition — run fdisk on the whole disk "
                      f"(/dev/{disk['name']})")
    return (yield from fdisk_mod.session(ctx, disk["name"]))


def _superblocks(blocks):
    out = []
    for base in (3, 5, 7):
        p = 1
        while 32768 * p < blocks:
            out.append(32768 * p)
            p *= base
    out = sorted(set([32768] + out))
    return [b for b in out if b < blocks][:14]


def _confirm(ctx, m, dev, disk, part):
    """Ask before destroying an existing table/filesystem. Returns False to abort."""
    if part is None and disk["label"]:
        ctx.write(f"Found a {disk['label']} partition table in {dev}\n", "warn")
    elif dev in m.fs:
        fs = m.fs[dev]
        ctx.write(f"{dev} contains a {fs['type']} file system"
                  f"{' labelled ' + repr(fs['label']) if fs.get('label') else ''}\n"
                  f"\tcreated on {fs['created']}\n", "warn")
    else:
        return True
    ans = yield Input("Proceed anyway? (y,N) ")
    return bool(ans) and ans.strip().lower().startswith("y")


def make_fs(ctx, fstype, dev, verbose, label=""):
    m = ctx.machine
    _need_root(m, "mkfs")
    prog = "mkfs.fat" if fstype == "vfat" else "mke2fs"
    disk, part = m.find_dev(dev)
    if disk is None:
        ctx.write(f"{prog}: The file {dev} does not exist and no size was specified.\n", "err")
        return 1
    if m.dev_in_use(dev) or (part is None and m.disk_in_use(disk)):
        ctx.write(f"{dev} is mounted; will not make a filesystem here!\n" if dev not in m.swaps
                  else f"{prog}: {dev} is apparently in use by the system; "
                       f"will not make a filesystem here!\n", "err")
        return 1
    ok = yield from _confirm(ctx, m, dev, disk, part)
    if not ok:
        return 1
    if part is None and disk["label"]:
        m.wipe_table(disk["name"])
    size = m.dev_bytes(dev)
    fs = m.make_fs(dev, fstype, label)
    if fstype == "vfat":
        ctx.write("mkfs.fat 4.2 (2021-01-31)\n")
        yield Sleep(300)
        return 0
    blocks = size // 4096
    ctx.write("mke2fs 1.47.1 (20-May-2024)\n")
    if verbose:
        ctx.write("fs_types for mke2fs.conf resolution: '" + fstype + "'\n", "dim")
    yield Sleep(150)
    ctx.write("Discarding device blocks: done\n")
    ctx.write(f"Creating filesystem with {blocks} 4k blocks and {blocks // 4} inodes\n")
    ctx.write(f"Filesystem UUID: {fs['uuid']}\n")
    sb = _superblocks(blocks)
    if sb:
        ctx.write("Superblock backups stored on blocks: \n\t" + ", ".join(map(str, sb)) + "\n\n")
    for step, ms in (("Allocating group tables: ", 250), ("Writing inode tables: ", 400)):
        ctx.write(step)
        yield Sleep(ms)
        ctx.write("done\n")
    if fstype in ("ext3", "ext4"):
        ctx.write(f"Creating journal ({min(262144, max(1024, blocks // 32))} blocks): ")
        yield Sleep(350)
        ctx.write("done\n")
    ctx.write("Writing superblocks and filesystem accounting information: ")
    yield Sleep(300)
    ctx.write("done\n\n")
    return 0


@command("mkfs", "make a filesystem: mkfs -v -t ext4 /dev/sdXN", G)
def _mkfs(ctx, args):
    o = Opts(args, "vqF", "tLn", {"verbose": "v"}, {"type": "t"})
    fstype = o.vals.get("t", "ext2")
    if fstype in ("fat", "vfat", "msdos"):
        fstype = "vfat"
    if fstype not in EXT + ("vfat",):
        raise UsageError(f"unknown filesystem type '{fstype}' (simulated: ext2/ext3/ext4/vfat)")
    if len(o.args) != 1:
        raise UsageError("usage: mkfs [-v] -t TYPE DEVICE")
    return (yield from make_fs(ctx, fstype, o.args[0], "v" in o, o.vals.get("L", "")))


def _mkfs_variant(fstype):
    def fn(ctx, args):
        if fstype == "vfat":
            o = Opts(args, "vq", "tLnF", {"verbose": "v"})
        else:
            o = Opts(args, "vqF", "tLb", {"verbose": "v"})
        if len(o.args) != 1:
            raise UsageError("usage: mkfs.* [-v] [-L LABEL] DEVICE")
        label = o.vals.get("L", o.vals.get("n", ""))
        return (yield from make_fs(ctx, o.vals.get("t", fstype), o.args[0], "v" in o, label))
    return fn


for _t in EXT:
    command(f"mkfs.{_t}", f"make an {_t} filesystem", G, hidden=_t != "ext4")(_mkfs_variant(_t))
command("mke2fs", "make an ext2/3/4 filesystem (-t TYPE)", G, hidden=True)(_mkfs_variant("ext2"))
command("mkfs.fat", "make a FAT filesystem", G, hidden=True, aliases=("mkfs.vfat",))(
    _mkfs_variant("vfat"))


@command("mkswap", "set up a swap area: mkswap /dev/sdXN", G)
def _mkswap(ctx, args):
    m = ctx.machine
    o = Opts(args, "vf", "L")
    if len(o.args) != 1:
        raise UsageError("usage: mkswap DEVICE")
    dev = o.args[0]
    _need_root(m, "mkswap")
    disk, part = m.find_dev(dev)
    if disk is None:
        raise FsError(f"cannot open {dev}: No such file or directory")
    if m.dev_in_use(dev):
        raise FsError(f"error: {dev} is mounted; will not make swapspace")
    if part is None and disk["label"] and "f" not in o:
        raise FsError(f"error: {dev}: probing initialization failed: Device or resource busy "
                      f"(it has a partition table; use a partition)")
    if part is not None and part["type"] != "swap":
        ctx.write(f"mkswap: warning: {dev} partition type is not 'Linux swap' "
                  f"(set it with fdisk 't')\n", "warn")
    size = m.dev_bytes(dev)
    fs = m.make_fs(dev, "swap", o.vals.get("L", ""))
    ctx.write(f"Setting up swapspace version 1, size = {dk.size_label(size - 4096)} "
              f"({size - 4096} bytes)\nno label, UUID={fs['uuid']}\n")


@command("swapon", "enable swap: swapon -v /dev/sdXN  (no args: list)", G)
def _swapon(ctx, args):
    m = ctx.machine
    o = Opts(args, "vsa", "", {"show": "s", "summary": "s", "verbose": "v"})
    if not o.args:
        if m.swaps:
            ctx.write("NAME      TYPE       SIZE USED PRIO\n", "accent")
            for i, d in enumerate(m.swaps):
                ctx.write(f"{d:<9} partition {human(m.dev_bytes(d)):>5}   0B   -{i + 2}\n")
        return 0
    _need_root(m, "swapon")
    for dev in o.args:
        fs = m.fs.get(dev)
        if m.find_dev(dev)[0] is None:
            raise FsError(f"cannot open {dev}: No such file or directory")
        if not fs or fs["type"] != "swap":
            raise FsError(f"{dev}: read swap header failed")
        if dev in m.swaps:
            raise FsError(f"{dev}: swapon failed: Device or resource busy")
        if "v" in o:
            size = m.dev_bytes(dev)
            ctx.write(f"swapon: {dev}: found signature [pagesize=4096, signature=swap]\n"
                      f"swapon: {dev}: pagesize=4096, swapsize={size}, devsize={size}\n"
                      f"swapon {dev}\n")
        m.swaps.append(dev)


@command("swapoff", "disable swap on a device", G)
def _swapoff(ctx, args):
    m = ctx.machine
    _need_root(m, "swapoff")
    for dev in args:
        if dev not in m.swaps:
            raise FsError(f"{dev}: swapoff failed: Invalid argument")
        m.swaps.remove(dev)


@command("mount", "mount -v -t ext4 /dev/sdXN DIR  (no args: list mounts)", G)
def _mount(ctx, args):
    m = ctx.machine
    o = Opts(args, "vr", "to", {"verbose": "v"}, {"types": "t", "options": "o"})
    if not o.args:
        for mt in m.mounts:
            ctx.write(f"{mt['dev']} on {mt['path']} type {mt['type']} (rw,relatime)\n")
        ctx.write("proc on /proc type proc (rw,nosuid,nodev,noexec,relatime)\n"
                  "sysfs on /sys type sysfs (rw,nosuid,nodev,noexec,relatime)\n", "dim")
        return 0
    if not m.is_root:
        raise FsError("only root can do that")
    if len(o.args) == 1:
        raise FsError(f"{o.args[0]}: can't find in /etc/fstab.")
    dev, path = o.args[0], o.args[1]
    try:
        m.mount(dev, path, o.vals.get("t"))
    except FsError as exc:
        ctx.write(f"mount: {exc}\n", "err")
        return 32
    if "v" in o:
        ctx.write(f"mount: {dev} mounted on {m.abspath(path)}.\n")


@command("umount", "unmount a filesystem: umount DIR", G)
def _umount(ctx, args):
    m = ctx.machine
    o = Opts(args, "vlR", "", {"verbose": "v"})
    if not o.args:
        raise UsageError("usage: umount DIR|DEVICE")
    if not m.is_root:
        raise FsError("must be superuser to unmount.")
    for a in o.args:
        try:
            p = m.umount(a)
        except FsError as exc:
            ctx.write(f"umount: {exc}\n", "err")
            return 32
        if "v" in o:
            ctx.write(f"umount: {p} unmounted\n")


@command("df", "show free disk space (-h human-readable)", G)
def _df(ctx, args):
    m = ctx.machine
    o = Opts(args, "hTk")
    h = "h" in o
    rows = [("Filesystem", "Size" if h else "1K-blocks", "Used", "Avail" if h else "Available",
             "Use%", "Mounted on")]
    mounts = m.mounts
    if o.args:
        mounts = []
        for a in o.args:
            dev, _ = m.fs_of(a)
            mounts += [mt for mt in m.mounts if mt["dev"] == dev][:1]
    for mt in mounts:
        size, used = m.fs_usage(mt["dev"])
        avail = max(size - used, 0)
        fmt = (lambda n: human(n)) if h else (lambda n: str(n // 1024))
        pct = f"{-(-used * 100 // size)}%" if size else "-"
        rows.append((mt["dev"], fmt(size), fmt(used), fmt(avail), pct, mt["path"]))
    _table(ctx, rows, right={1, 2, 3, 4})


@command("free", "show memory and swap usage (-h)", G)
def _free(ctx, args):
    m = ctx.machine
    o = Opts(args, "hmgk")
    total, used = 16 * dk.GiB, int(2.3 * dk.GiB)
    swap = sum(m.dev_bytes(d) for d in m.swaps)
    fmt = human if "h" in o else (lambda n: str(n // 1024))
    rows = [("", "total", "used", "free"),
            ("Mem:", fmt(total), fmt(used), fmt(total - used)),
            ("Swap:", fmt(swap), fmt(0), fmt(swap))]
    _table(ctx, rows, right={1, 2, 3})


@command("sync", "flush filesystem buffers", G, hidden=True)
def _sync(ctx, args):
    return 0
