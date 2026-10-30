"""Source packages the player downloads in chapter 3 (shortened list, LFS 12.2-era versions).

Sizes are approximate and checksums are FAKE (md5 of a salted name) — nothing is real here.
"""
from __future__ import annotations

import hashlib
from typing import Dict, List, NamedTuple

LFS_SITE = "https://www.linuxfromscratch.org/lfs/view/stable"
WGET_LIST_URL = f"{LFS_SITE}/wget-list-sysv"
MD5SUMS_URL = f"{LFS_SITE}/md5sums"


class Package(NamedTuple):
    name: str
    file: str
    url: str
    size: int  # bytes


def _gnu(name, file):
    return f"https://ftp.gnu.org/gnu/{name}/{file}"


_M = 1024 * 1024
PACKAGES: List[Package] = [
    Package("acl", "acl-2.3.2.tar.xz", "https://download.savannah.gnu.org/releases/acl/acl-2.3.2.tar.xz", int(0.36 * _M)),
    Package("attr", "attr-2.5.2.tar.gz", "https://download.savannah.gnu.org/releases/attr/attr-2.5.2.tar.gz", int(0.48 * _M)),
    Package("autoconf", "autoconf-2.72.tar.xz", _gnu("autoconf", "autoconf-2.72.tar.xz"), int(1.4 * _M)),
    Package("automake", "automake-1.17.tar.xz", _gnu("automake", "automake-1.17.tar.xz"), int(1.6 * _M)),
    Package("bash", "bash-5.2.32.tar.gz", _gnu("bash", "bash-5.2.32.tar.gz"), int(10.7 * _M)),
    Package("bc", "bc-6.7.6.tar.xz", "https://github.com/gavinhoward/bc/releases/download/6.7.6/bc-6.7.6.tar.xz", int(0.46 * _M)),
    Package("binutils", "binutils-2.43.1.tar.xz", "https://sourceware.org/pub/binutils/releases/binutils-2.43.1.tar.xz", int(27.6 * _M)),
    Package("bison", "bison-3.8.2.tar.xz", _gnu("bison", "bison-3.8.2.tar.xz"), int(2.7 * _M)),
    Package("bzip2", "bzip2-1.0.8.tar.gz", "https://www.sourceware.org/pub/bzip2/bzip2-1.0.8.tar.gz", int(0.79 * _M)),
    Package("coreutils", "coreutils-9.5.tar.xz", _gnu("coreutils", "coreutils-9.5.tar.xz"), int(5.8 * _M)),
    Package("diffutils", "diffutils-3.10.tar.xz", _gnu("diffutils", "diffutils-3.10.tar.xz"), int(1.6 * _M)),
    Package("e2fsprogs", "e2fsprogs-1.47.1.tar.gz", "https://downloads.sourceforge.net/project/e2fsprogs/e2fsprogs/v1.47.1/e2fsprogs-1.47.1.tar.gz", int(9.6 * _M)),
    Package("file", "file-5.45.tar.gz", "https://astron.com/pub/file/file-5.45.tar.gz", int(1.2 * _M)),
    Package("findutils", "findutils-4.10.0.tar.xz", _gnu("findutils", "findutils-4.10.0.tar.xz"), int(2.2 * _M)),
    Package("gawk", "gawk-5.3.0.tar.xz", _gnu("gawk", "gawk-5.3.0.tar.xz"), int(3.4 * _M)),
    Package("gcc", "gcc-14.2.0.tar.xz", _gnu("gcc", "gcc-14.2.0/gcc-14.2.0.tar.xz"), int(88.6 * _M)),
    Package("gettext", "gettext-0.22.5.tar.xz", _gnu("gettext", "gettext-0.22.5.tar.xz"), int(10.3 * _M)),
    Package("glibc", "glibc-2.40.tar.xz", _gnu("glibc", "glibc-2.40.tar.xz"), int(18.3 * _M)),
    Package("gmp", "gmp-6.3.0.tar.xz", _gnu("gmp", "gmp-6.3.0.tar.xz"), int(2.0 * _M)),
    Package("grep", "grep-3.11.tar.xz", _gnu("grep", "grep-3.11.tar.xz"), int(1.7 * _M)),
    Package("gzip", "gzip-1.13.tar.xz", _gnu("gzip", "gzip-1.13.tar.xz"), int(0.82 * _M)),
    Package("linux", "linux-6.10.5.tar.xz", "https://www.kernel.org/pub/linux/kernel/v6.x/linux-6.10.5.tar.xz", int(138.4 * _M)),
    Package("m4", "m4-1.4.19.tar.xz", _gnu("m4", "m4-1.4.19.tar.xz"), int(1.6 * _M)),
    Package("make", "make-4.4.1.tar.gz", _gnu("make", "make-4.4.1.tar.gz"), int(2.2 * _M)),
    Package("mpc", "mpc-1.3.1.tar.gz", _gnu("mpc", "mpc-1.3.1.tar.gz"), int(0.75 * _M)),
    Package("mpfr", "mpfr-4.2.1.tar.xz", "https://ftp.gnu.org/gnu/mpfr/mpfr-4.2.1.tar.xz", int(1.4 * _M)),
    Package("ncurses", "ncurses-6.5.tar.gz", "https://invisible-mirror.net/archives/ncurses/ncurses-6.5.tar.gz", int(3.6 * _M)),
    Package("patch", "patch-2.7.6.tar.xz", _gnu("patch", "patch-2.7.6.tar.xz"), int(0.76 * _M)),
    Package("perl", "perl-5.40.0.tar.xz", "https://www.cpan.org/src/5.0/perl-5.40.0.tar.xz", int(13.4 * _M)),
    Package("python", "Python-3.12.5.tar.xz", "https://www.python.org/ftp/python/3.12.5/Python-3.12.5.tar.xz", int(19.6 * _M)),
    Package("sed", "sed-4.9.tar.xz", _gnu("sed", "sed-4.9.tar.xz"), int(1.4 * _M)),
    Package("tar", "tar-1.35.tar.xz", _gnu("tar", "tar-1.35.tar.xz"), int(2.2 * _M)),
    Package("texinfo", "texinfo-7.1.tar.xz", _gnu("texinfo", "texinfo-7.1.tar.xz"), int(5.5 * _M)),
    Package("util-linux", "util-linux-2.40.2.tar.xz", "https://www.kernel.org/pub/linux/utils/util-linux/v2.40/util-linux-2.40.2.tar.xz", int(8.5 * _M)),
    Package("xz", "xz-5.6.2.tar.xz", "https://github.com/tukaani-project/xz/releases/download/v5.6.2/xz-5.6.2.tar.xz", int(1.3 * _M)),
    Package("zlib", "zlib-1.3.1.tar.gz", "https://zlib.net/fossils/zlib-1.3.1.tar.gz", int(1.5 * _M)),
    Package("zstd", "zstd-1.5.6.tar.gz", "https://github.com/facebook/zstd/releases/download/v1.5.6/zstd-1.5.6.tar.gz", int(2.3 * _M)),
    Package("glibc-fhs-patch", "glibc-2.40-fhs-1.patch",
            "https://www.linuxfromscratch.org/patches/lfs/12.2/glibc-2.40-fhs-1.patch", 3_011),
]

# Patches are text files: their contents matter to the 'patch' command.
PATCH_TEXT = {
    "glibc-2.40-fhs-1.patch": (
        "Submitted By:            (simulated)\n"
        "Description:             Moves the nss_db and nscd files to FHS compliant\n"
        "                         locations (/var/lib/nss_db instead of /var/db).\n"
        "lfsgame-patch: glibc-fhs\n"
        "diff -Naur glibc-2.40.orig/Makeconfig glibc-2.40/Makeconfig\n"
        "--- glibc-2.40.orig/Makeconfig\n"
        "+++ glibc-2.40/Makeconfig\n"
        "@@ -1 +1 @@\n"
        "-vardbdir = $(localstatedir)/db\n"
        "+vardbdir = $(localstatedir)/lib/nss_db\n"),
}

BY_FILE: Dict[str, Package] = {p.file: p for p in PACKAGES}
BY_URL: Dict[str, Package] = {p.url: p for p in PACKAGES}


def fake_md5(file: str) -> str:
    return hashlib.md5(f"lfsgame:{file}".encode()).hexdigest()


def wget_list_text() -> str:
    return "".join(p.url + "\n" for p in PACKAGES)


def md5sums_text() -> str:
    return "".join(f"{fake_md5(p.file)}  {p.file}\n" for p in PACKAGES)


def total_size() -> int:
    return sum(p.size for p in PACKAGES)
