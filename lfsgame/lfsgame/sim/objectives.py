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


TGT = "x86_64-lfs-linux-gnu"
TOOLS = f"{LFS}/tools"
GCCLIB = f"{TOOLS}/lib/gcc/{TGT}/14.2.0"


def _exists(m: Machine, path: str, kind: Optional[str] = None) -> bool:
    n = _node(m, path)
    return n is not None and (kind is None or n["t"] == kind)


def _lfs_env_ok(m: Machine) -> bool:
    e = m.env
    return (m.user == "lfs" and e.get("LFS") == LFS and e.get("LFS_TGT") == TGT
            and e.get("PATH", "").split(":")[0] == f"{TOOLS}/bin" and e.get("LC_ALL") == "POSIX"
            and "CC" not in e and "CXX" not in e and m.umask == 0o022)


def _limits_ok(m: Machine) -> bool:
    n = _node(m, f"{GCCLIB}/include/limits.h")
    if n is None or n["t"] != "f":
        return False
    d = n.get("data", "")
    return all(k in d for k in ("LIMITX", "GLIMITS", "LIMITY")) and \
        d.index("LIMITX") < d.index("GLIMITS") < d.index("LIMITY")


def _headers_ok(m: Machine) -> bool:
    inc = _node(m, f"{LFS}/usr/include")
    if inc is None or not _exists(m, f"{LFS}/usr/include/linux/version.h", "f"):
        return False

    def clean(node):
        for name, ch in node["c"].items():
            if ch["t"] == "d" and not clean(ch):
                return False
            if ch["t"] == "f" and not name.endswith(".h"):
                return False
        return True
    return clean(inc)


def _glibc_ok(m: Machine) -> bool:
    if not _exists(m, f"{LFS}/usr/lib/libc.so.6", "f"):
        return False
    for link in (f"{LFS}/lib64/ld-linux-x86-64.so.2", f"{LFS}/lib64/ld-lsb-x86-64.so.3"):
        n = _node(m, link, follow=False)
        if n is None or n["t"] != "l":
            return False
    ldd = _node(m, f"{LFS}/usr/bin/ldd")
    return ldd is not None and 'RTLDLIST="/lib/' in ldd.get("data", "") and m.has_flag("glibc_fhs")


def _libstdcxx_ok(m: Machine) -> bool:
    return (_exists(m, f"{LFS}/usr/lib/libstdc++.so.6.0.33", "f")
            and _exists(m, f"{TOOLS}/{TGT}/include/c++/14.2.0/iostream", "f")
            and not any(_exists(m, f"{LFS}/usr/lib/{la}") for la in
                        ("libstdc++.la", "libstdc++exp.la", "libstdc++fs.la", "libsupc++.la")))


USR = f"{LFS}/usr"


def _cross_bin(m: Machine, path: str) -> bool:
    n = _node(m, path)
    return n is not None and n["t"] == "f" and bool(n.get("cross"))


def _absent(m: Machine, *paths: str) -> bool:
    return not any(_node(m, p, follow=False) is not None for p in paths)


def _link_to(m: Machine, path: str, target: str) -> bool:
    n = _node(m, path, follow=False)
    return n is not None and n["t"] == "l" and n["to"] == target


def _ncurses_ok(m: Machine) -> bool:
    h = _node(m, f"{USR}/include/curses.h")
    lines = (h or {}).get("data", "").splitlines()
    return (_exists(m, f"{USR}/lib/libncursesw.so.6.5", "f")
            and _link_to(m, f"{USR}/lib/libncurses.so", "libncursesw.so")
            and _exists(m, f"{USR}/share/terminfo/x/xterm", "f")
            and "#if 1" in lines
            and not any(ln.startswith("#if") and "XOPEN" in ln for ln in lines))


def _coreutils_ok(m: Machine) -> bool:
    man = _node(m, f"{USR}/share/man/man8/chroot.8")
    return (_cross_bin(m, f"{USR}/sbin/chroot") and _absent(m, f"{USR}/bin/chroot")
            and _cross_bin(m, f"{USR}/bin/hostname")
            and _absent(m, f"{USR}/bin/kill", f"{USR}/bin/uptime")
            and man is not None and '"8"' in man.get("data", ""))


def _simple(pkg_bin: str):
    return lambda m: _cross_bin(m, f"{USR}/bin/{pkg_bin}")


def _ch6(id_, section, name, check, hint=""):
    return Objective(id_, f"Cross compile {name}", section,
                     hint or f"book {section}: tar -xf, ./configure --prefix=/usr "
                             f"--host=$LFS_TGT --build=..., make, make DESTDIR=$LFS install",
                     check)


def _root_owned(m: Machine) -> bool:
    for d in ("usr", "usr/lib", "usr/bin", "usr/bin/bash", "var", "etc", "lib64", "tools",
              f"tools/bin/{TGT}-gcc"):
        n = _node(m, f"{LFS}/{d}")
        if d.startswith("tools") and n is None:
            continue  # removed in 7.13 (objective is sticky anyway)
        if n is None or n["u"] != "root":
            return False
    return True


def _vkfs_ok(m: Machine) -> bool:
    want = {f"{LFS}/dev": "none", f"{LFS}/dev/pts": "devpts", f"{LFS}/proc": "proc",
            f"{LFS}/sys": "sysfs", f"{LFS}/run": "tmpfs", f"{LFS}/dev/shm": "tmpfs"}
    have = {mt["path"]: mt["type"] for mt in m.mounts}
    bind_ok = any(mt["path"] == f"{LFS}/dev" and mt.get("bind") == "/dev" for mt in m.mounts)
    return bind_ok and all(have.get(p) == t for p, t in want.items())


DIRS7 = ("boot", "home", "mnt", "opt", "srv", "etc/opt", "etc/sysconfig", "usr/lib/firmware",
         "media/floppy", "media/cdrom", "usr/include", "usr/local/include", "usr/src",
         "usr/lib/locale", "usr/local/bin", "usr/local/sbin", "usr/share/zoneinfo",
         "usr/share/man/man1", "usr/share/man/man8", "usr/local/share/man/man8",
         "var/cache", "var/log", "var/spool", "var/lib/color", "var/lib/misc", "var/lib/locate")


def _dirs7_ok(m: Machine) -> bool:
    for d in DIRS7:
        if not _exists(m, f"{LFS}/{d}", "d"):
            return False
    root, tmp, vtmp = (_node(m, f"{LFS}/{d}") for d in ("root", "tmp", "var/tmp"))
    return (_link_to(m, f"{LFS}/var/run", "/run") and _link_to(m, f"{LFS}/var/lock", "/run/lock")
            and root is not None and root["m"] & 0o7777 == 0o750
            and tmp is not None and tmp["m"] & 0o7777 == 0o1777
            and vtmp is not None and vtmp["m"] & 0o7777 == 0o1777)


def _text(m: Machine, path: str) -> str:
    n = _node(m, path)
    return (n or {}).get("data", "") if n and n["t"] == "f" else ""


def _files7_ok(m: Machine) -> bool:
    passwd, group = _text(m, f"{LFS}/etc/passwd"), _text(m, f"{LFS}/etc/group")
    lastlog, btmp = _node(m, f"{LFS}/var/log/lastlog"), _node(m, f"{LFS}/var/log/btmp")
    home = _node(m, f"{LFS}/home/tester")
    return (_link_to(m, f"{LFS}/etc/mtab", "/proc/self/mounts")
            and "localhost" in _text(m, f"{LFS}/etc/hosts")
            and "root:x:0:0:" in passwd and "tester:x:101:101:" in passwd
            and "utmp:x:13:" in group and "tester:x:101:" in group
            and home is not None and home["u"] == "tester"
            and m.has_flag("chroot_named")
            and lastlog is not None and lastlog["g"] == "utmp" and lastlog["m"] & 0o777 == 0o664
            and btmp is not None and btmp["m"] & 0o777 == 0o600
            and all(_exists(m, f"{LFS}/var/log/{f}") for f in ("faillog", "wtmp")))


def _cleanup7_ok(m: Machine) -> bool:
    for d in ("info", "man", "doc"):
        n = _node(m, f"{USR}/share/{d}")
        if n is not None and n["t"] == "d" and n["c"]:
            return False

    def has_la(node):
        for name, ch in node.get("c", {}).items():
            if ch["t"] == "f" and name.endswith(".la"):
                return True
            if ch["t"] == "d" and has_la(ch):
                return True
        return False
    for d in ("lib", "libexec"):
        n = _node(m, f"{USR}/{d}")
        if n is not None and has_la(n):
            return False
    return _node(m, f"{LFS}/tools", follow=False) is None and m.has_flag("chroot_ok")


def _backup_ok(m: Machine) -> bool:
    for home in ("/root", "/home/lfs"):
        n = _node(m, home)
        for name, ch in ((n or {}).get("c") or {}).items():
            if ch.get("backup") and LFS in ch["backup"] and name.endswith((".tar.xz", ".tar.gz")):
                return True
    return False


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
    Objective("bashrc", "As root, move the host's /etc/bash.bashrc out of the way", "4.4",
              "exit (back to root), then: [ ! -e /etc/bash.bashrc ] || "
              "mv -v /etc/bash.bashrc /etc/bash.bashrc.NOUSE",
              lambda m: not _exists(m, "/etc/bash.bashrc")),
    Objective("lfsenv", "Give lfs a clean build environment (.bash_profile + .bashrc)", "4.4",
              "as lfs: write ~/.bash_profile and ~/.bashrc as in book 4.4, then: "
              "source ~/.bash_profile",
              _lfs_env_ok),
    Objective("makeflags", "Optional: build in parallel (MAKEFLAGS=-j$(nproc))", "4.4",
              "cat >> ~/.bashrc << \"EOF\" / export MAKEFLAGS=-j$(nproc) / EOF, then "
              "source ~/.bash_profile",
              lambda m: m.env.get("MAKEFLAGS", "").startswith("-j"), optional=True),
    Objective("binutils1", "Build and install Binutils pass 1", "5.2",
              "cd $LFS/sources; tar -xf binutils-2.43.1.tar.xz; cd binutils-2.43.1; "
              "mkdir -v build; cd build; ../configure ... (book 5.2)",
              lambda m: _exists(m, f"{TOOLS}/bin/{TGT}-ld", "f")
              and _exists(m, f"{TOOLS}/{TGT}/bin/as", "f")),
    Objective("gcc1", "Build and install GCC pass 1", "5.3",
              "unpack gcc, put mpfr/gmp/mpc inside it, build in gcc-14.2.0/build (book 5.3)",
              lambda m: _exists(m, f"{TOOLS}/bin/{TGT}-gcc", "f")),
    Objective("limits", "Create the full internal limits.h of GCC", "5.3",
              "cd .. (to gcc-14.2.0), then the 'cat gcc/limitx.h ... > .../limits.h' "
              "command from book 5.3",
              _limits_ok),
    Objective("headers", "Install the Linux API headers into $LFS/usr/include", "5.4",
              "tar -xf linux-6.10.5.tar.xz; cd linux-6.10.5; make mrproper; make headers; "
              "find usr/include -type f ! -name '*.h' -delete; cp -rv usr/include $LFS/usr",
              _headers_ok),
    Objective("glibc", "Build and install Glibc (FHS patch, DESTDIR=$LFS, ldd fix)", "5.5",
              "symlinks in $LFS/lib64, patch -Np1 -i ../glibc-2.40-fhs-1.patch, build dir, "
              "configure, make, make DESTDIR=$LFS install, sed on ldd (book 5.5)",
              _glibc_ok),
    Objective("sanity", "Sanity check: compile a test program with the cross toolchain", "5.5",
              "echo 'int main(){}' | $LFS_TGT-gcc -xc -   then   readelf -l a.out | grep ld-linux",
              lambda m: m.has_flag("sanity_ok")),
    Objective("libstdcxx", "Build and install Libstdc++ (and remove the .la files)", "5.6",
              "fresh gcc tree, build dir, ../libstdc++-v3/configure ..., make, "
              "make DESTDIR=$LFS install, rm -v $LFS/usr/lib/lib{stdc++{,exp,fs},supc++}.la",
              _libstdcxx_ok),
    _ch6("m4", "6.2", "M4", _simple("m4")),
    _ch6("ncurses", "6.3", "Ncurses (with a native tic, plus the libncurses.so link and curses.h fix)",
         _ncurses_ok, "book 6.3: native tic in build/, cross ./configure, make, "
                      "make DESTDIR=$LFS TIC_PATH=$(pwd)/build/progs/tic install, ln, sed"),
    _ch6("bash", "6.4", "Bash (and the sh link)",
         lambda m: _cross_bin(m, f"{USR}/bin/bash") and _link_to(m, f"{USR}/bin/sh", "bash")),
    _ch6("coreutils", "6.5", "Coreutils (chroot moved to /usr/sbin, man page to man8)",
         _coreutils_ok, "book 6.5: configure with --enable-install-program=hostname "
                        "--enable-no-install-program=kill,uptime, then the mv/mkdir/sed steps"),
    _ch6("diffutils", "6.6", "Diffutils", _simple("diff")),
    _ch6("file", "6.7", "File (with a native file first)",
         lambda m: _cross_bin(m, f"{USR}/bin/file") and _absent(m, f"{USR}/lib/libmagic.la"),
         "book 6.7: native build in build/, then make FILE_COMPILE=$(pwd)/build/src/file, "
         "install, rm libmagic.la"),
    _ch6("findutils", "6.8", "Findutils",
         lambda m: _cross_bin(m, f"{USR}/bin/find") and _exists(m, f"{LFS}/var/lib/locate", "d")),
    _ch6("gawk", "6.9", "Gawk", _simple("gawk")),
    _ch6("grep", "6.10", "Grep", _simple("grep")),
    _ch6("gzip", "6.11", "Gzip", _simple("gzip")),
    _ch6("make", "6.12", "Make", _simple("make")),
    _ch6("patch", "6.13", "Patch", _simple("patch")),
    _ch6("sed", "6.14", "Sed", _simple("sed")),
    _ch6("tar", "6.15", "Tar", _simple("tar")),
    _ch6("xz", "6.16", "Xz (and remove liblzma.la)",
         lambda m: _cross_bin(m, f"{USR}/bin/xz") and _absent(m, f"{USR}/lib/liblzma.la")),
    Objective("binutils2", "Build Binutils pass 2 (ltmain.sh fix, remove static libs)", "6.17",
              "book 6.17: sed on ltmain.sh, build dir, ../configure --host=$LFS_TGT ..., "
              "make DESTDIR=$LFS install, rm the .a/.la files",
              lambda m: _cross_bin(m, f"{USR}/bin/ld") and m.has_flag("binutils2_ltmain")
              and _absent(m, *[f"{USR}/lib/lib{n}.{e}" for n in
                               ("bfd", "ctf", "ctf-nobfd", "opcodes", "sframe")
                               for e in ("a", "la")])),
    Objective("gcc2", "Build GCC pass 2 (and the cc link)", "6.18",
              "book 6.18: math libs, t-linux64 and thread_header seds, build dir, configure, "
              "make DESTDIR=$LFS install, ln -sv gcc $LFS/usr/bin/cc",
              lambda m: _cross_bin(m, f"{USR}/bin/gcc") and _link_to(m, f"{USR}/bin/cc", "gcc")),
    Objective("rootowner", "As root, give $LFS back to root (chown --from lfs -R root:root)",
              "7.2", "exit to root, then: chown --from lfs -R root:root "
                     "$LFS/{usr,lib,var,etc,bin,sbin,tools}   and the same for $LFS/lib64",
              _root_owned),
    Objective("vkfs", "Mount the virtual kernel file systems (dev, dev/pts, proc, sys, run, "
                      "dev/shm)", "7.3",
              "mkdir -pv $LFS/{dev,proc,sys,run}, mount -v --bind /dev $LFS/dev, then the "
              "devpts, proc, sysfs and tmpfs mounts from book 7.3",
              _vkfs_ok),
    Objective("chroot", "Enter the chroot environment", "7.4",
              "chroot \"$LFS\" /usr/bin/env -i HOME=/root TERM=\"$TERM\" PS1='(lfs chroot) "
              "\\u:\\w\\$ ' PATH=/usr/bin:/usr/sbin ... /bin/bash --login (book 7.4)",
              lambda m: m.has_flag("chroot_ok")),
    Objective("dirs7", "Create the full directory tree (inside the chroot)", "7.5",
              "the mkdir -pv lines, ln -sfv /run /var/run, ln -sfv /run/lock /var/lock, "
              "install -dv -m 0750 /root, install -dv -m 1777 /tmp /var/tmp",
              _dirs7_ok),
    Objective("files7", "Create the essential files: mtab, hosts, passwd, group, log files",
              "7.6", "the cat > /etc/... << \"EOF\" blocks of book 7.6, the tester user, "
                     "exec /usr/bin/bash --login, touch/chgrp/chmod the log files",
              _files7_ok),
    Objective("gettext7", "Gettext (only msgfmt, msgmerge, xgettext)", "7.7",
              "cd /sources, unpack, ./configure --disable-shared, make, "
              "cp -v gettext-tools/src/{msgfmt,msgmerge,xgettext} /usr/bin",
              lambda m: all(_cross_bin(m, f"{USR}/bin/{p}")
                            for p in ("msgfmt", "msgmerge", "xgettext"))),
    Objective("bison7", "Bison", "7.8", "./configure --prefix=/usr --docdir=..., make, make install",
              _simple("bison")),
    Objective("perl7", "Perl", "7.9", "sh Configure -des -D prefix=/usr ... (book 7.9), make, "
                                      "make install",
              lambda m: _cross_bin(m, f"{USR}/bin/perl")
              and _exists(m, f"{USR}/lib/perl5/5.40/core_perl/strict.pm", "f")),
    Objective("python7", "Python", "7.10", "./configure --prefix=/usr --enable-shared "
                                           "--without-ensurepip, make, make install",
              lambda m: _cross_bin(m, f"{USR}/bin/python3.12")
              and _exists(m, f"{USR}/lib/libpython3.12.so.1.0", "f")),
    Objective("texinfo7", "Texinfo", "7.11", "./configure --prefix=/usr, make, make install",
              _simple("makeinfo")),
    Objective("utillinux7", "Util-linux", "7.12",
              "mkdir -pv /var/lib/hwclock, ./configure --libdir=/usr/lib ... (book 7.12), make, "
              "make install",
              lambda m: _cross_bin(m, f"{USR}/bin/mount") and _exists(m, f"{LFS}/var/lib/hwclock", "d")),
    Objective("cleanup7", "Clean up the temporary system (docs, .la files, /tools)", "7.13",
              "inside the chroot: rm -rf /usr/share/{info,man,doc}/*; "
              "find /usr/{lib,libexec} -name \\*.la -delete; rm -rf /tools",
              _cleanup7_ok),
    Objective("backup7", "Optional: back up the temporary system", "7.13",
              "exit the chroot, unmount the virtual file systems, cd $LFS, "
              "tar -cJpf $HOME/lfs-temp-tools-12.2.tar.xz .",
              _backup_ok, optional=True),
]

BY_ID = {o.id: o for o in OBJECTIVES}
