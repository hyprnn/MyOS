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


def _ch8(name, file, url, mb):
    return Package(name, file, url, int(mb * _M))


_GH = "https://github.com"
# packages needed only by chapter 8 (LFS 12.2-era versions, sizes approximate)
PACKAGES += [
    _ch8("man-pages", "man-pages-6.9.1.tar.xz", "https://www.kernel.org/pub/linux/docs/man-pages/man-pages-6.9.1.tar.xz", 2.8),
    _ch8("iana-etc", "iana-etc-20240806.tar.gz", f"{_GH}/Mic92/iana-etc/releases/download/20240806/iana-etc-20240806.tar.gz", 0.6),
    _ch8("lz4", "lz4-1.10.0.tar.gz", f"{_GH}/lz4/lz4/releases/download/v1.10.0/lz4-1.10.0.tar.gz", 0.4),
    _ch8("readline", "readline-8.2.13.tar.gz", _gnu("readline", "readline-8.2.13.tar.gz"), 3.0),
    _ch8("flex", "flex-2.6.4.tar.gz", f"{_GH}/westes/flex/releases/download/v2.6.4/flex-2.6.4.tar.gz", 1.4),
    _ch8("tcl", "tcl8.6.14-src.tar.gz", "https://downloads.sourceforge.net/tcl/tcl8.6.14-src.tar.gz", 11.0),
    _ch8("expect", "expect5.45.4.tar.gz", "https://prdownloads.sourceforge.net/expect/expect5.45.4.tar.gz", 0.6),
    _ch8("dejagnu", "dejagnu-1.6.3.tar.gz", _gnu("dejagnu", "dejagnu-1.6.3.tar.gz"), 0.6),
    _ch8("pkgconf", "pkgconf-2.3.0.tar.xz", "https://distfiles.ariadne.space/pkgconf/pkgconf-2.3.0.tar.xz", 0.3),
    _ch8("libcap", "libcap-2.70.tar.xz", "https://www.kernel.org/pub/linux/libs/security/linux-privs/libcap2/libcap-2.70.tar.xz", 0.2),
    _ch8("libxcrypt", "libxcrypt-4.4.36.tar.xz", f"{_GH}/besser82/libxcrypt/releases/download/v4.4.36/libxcrypt-4.4.36.tar.xz", 0.6),
    _ch8("shadow", "shadow-4.16.0.tar.xz", f"{_GH}/shadow-maint/shadow/releases/download/4.16.0/shadow-4.16.0.tar.xz", 2.2),
    _ch8("psmisc", "psmisc-23.7.tar.xz", "https://sourceforge.net/projects/psmisc/files/psmisc/psmisc-23.7.tar.xz", 0.4),
    _ch8("libtool", "libtool-2.4.7.tar.xz", _gnu("libtool", "libtool-2.4.7.tar.xz"), 1.0),
    _ch8("gdbm", "gdbm-1.24.tar.gz", _gnu("gdbm", "gdbm-1.24.tar.gz"), 1.2),
    _ch8("gperf", "gperf-3.1.tar.gz", _gnu("gperf", "gperf-3.1.tar.gz"), 1.2),
    _ch8("expat", "expat-2.6.2.tar.xz", "https://prdownloads.sourceforge.net/expat/expat-2.6.2.tar.xz", 0.5),
    _ch8("inetutils", "inetutils-2.5.tar.xz", _gnu("inetutils", "inetutils-2.5.tar.xz"), 1.6),
    _ch8("less", "less-661.tar.gz", "https://www.greenwoodsoftware.com/less/less-661.tar.gz", 0.6),
    _ch8("xml-parser", "XML-Parser-2.47.tar.gz", "https://cpan.metacpan.org/authors/id/T/TO/TODDR/XML-Parser-2.47.tar.gz", 0.3),
    _ch8("intltool", "intltool-0.51.0.tar.gz", "https://launchpad.net/intltool/trunk/0.51.0/+download/intltool-0.51.0.tar.gz", 0.2),
    _ch8("openssl", "openssl-3.3.1.tar.gz", "https://www.openssl.org/source/openssl-3.3.1.tar.gz", 17.0),
    _ch8("kmod", "kmod-33.tar.xz", "https://www.kernel.org/pub/linux/utils/kernel/kmod/kmod-33.tar.xz", 0.5),
    _ch8("elfutils", "elfutils-0.191.tar.bz2", "https://sourceware.org/ftp/elfutils/0.191/elfutils-0.191.tar.bz2", 9.0),
    _ch8("libffi", "libffi-3.4.6.tar.gz", f"{_GH}/libffi/libffi/releases/download/v3.4.6/libffi-3.4.6.tar.gz", 1.3),
    _ch8("flit-core", "flit_core-3.9.0.tar.gz", "https://pypi.org/packages/source/f/flit-core/flit_core-3.9.0.tar.gz", 0.04),
    _ch8("wheel", "wheel-0.44.0.tar.gz", "https://pypi.org/packages/source/w/wheel/wheel-0.44.0.tar.gz", 0.1),
    _ch8("setuptools", "setuptools-72.2.0.tar.gz", "https://pypi.org/packages/source/s/setuptools/setuptools-72.2.0.tar.gz", 2.3),
    _ch8("ninja", "ninja-1.12.1.tar.gz", f"{_GH}/ninja-build/ninja/archive/v1.12.1/ninja-1.12.1.tar.gz", 0.24),
    _ch8("meson", "meson-1.5.1.tar.gz", f"{_GH}/mesonbuild/meson/releases/download/1.5.1/meson-1.5.1.tar.gz", 2.2),
    _ch8("check", "check-0.15.2.tar.gz", f"{_GH}/libcheck/check/releases/download/0.15.2/check-0.15.2.tar.gz", 0.76),
    _ch8("groff", "groff-1.23.0.tar.gz", _gnu("groff", "groff-1.23.0.tar.gz"), 7.1),
    _ch8("grub", "grub-2.12.tar.xz", _gnu("grub", "grub-2.12.tar.xz"), 6.6),
    _ch8("iproute2", "iproute2-6.10.0.tar.xz", "https://www.kernel.org/pub/linux/utils/net/iproute2/iproute2-6.10.0.tar.xz", 0.9),
    _ch8("kbd", "kbd-2.6.4.tar.xz", "https://www.kernel.org/pub/linux/utils/kbd/kbd-2.6.4.tar.xz", 1.4),
    _ch8("libpipeline", "libpipeline-1.5.7.tar.gz", "https://download.savannah.gnu.org/releases/libpipeline/libpipeline-1.5.7.tar.gz", 0.95),
    _ch8("vim", "vim-9.1.0660.tar.gz", f"{_GH}/vim/vim/archive/v9.1.0660/vim-9.1.0660.tar.gz", 17.0),
    _ch8("markupsafe", "MarkupSafe-2.1.5.tar.gz", "https://pypi.org/packages/source/M/MarkupSafe/MarkupSafe-2.1.5.tar.gz", 0.02),
    _ch8("jinja2", "jinja2-3.1.4.tar.gz", "https://pypi.org/packages/source/J/Jinja2/jinja2-3.1.4.tar.gz", 0.24),
    _ch8("systemd", "systemd-256.4.tar.gz", f"{_GH}/systemd/systemd/archive/v256.4/systemd-256.4.tar.gz", 15.0),
    _ch8("man-db", "man-db-2.12.1.tar.xz", "https://download.savannah.gnu.org/releases/man-db/man-db-2.12.1.tar.xz", 2.0),
    _ch8("procps-ng", "procps-ng-4.0.4.tar.xz", "https://sourceforge.net/projects/procps-ng/files/Production/procps-ng-4.0.4.tar.xz", 1.4),
    _ch8("sysklogd", "sysklogd-2.6.1.tar.gz", f"{_GH}/troglobit/sysklogd/releases/download/v2.6.1/sysklogd-2.6.1.tar.gz", 0.45),
    _ch8("sysvinit", "sysvinit-3.10.tar.xz", f"{_GH}/slicer69/sysvinit/releases/download/3.10/sysvinit-3.10.tar.xz", 0.23),
    _ch8("tzdata", "tzdata2024a.tar.gz", "https://www.iana.org/time-zones/repository/releases/tzdata2024a.tar.gz", 0.45),
    _ch8("bzip2-docs-patch", "bzip2-1.0.8-install_docs-1.patch",
         "https://www.linuxfromscratch.org/patches/lfs/12.2/bzip2-1.0.8-install_docs-1.patch", 0.002),
    _ch8("coreutils-i18n-patch", "coreutils-9.5-i18n-2.patch",
         "https://www.linuxfromscratch.org/patches/lfs/12.2/coreutils-9.5-i18n-2.patch", 0.17),
    _ch8("kbd-backspace-patch", "kbd-2.6.4-backspace-1.patch",
         "https://www.linuxfromscratch.org/patches/lfs/12.2/kbd-2.6.4-backspace-1.patch", 0.012),
    _ch8("sysvinit-patch", "sysvinit-3.10-consolidated-1.patch",
         "https://www.linuxfromscratch.org/patches/lfs/12.2/sysvinit-3.10-consolidated-1.patch", 0.003),
]


def _patch(pkg, name, files, about):
    head = (f"Submitted By:            (simulated)\nDescription:             {about}\n"
            f"lfsgame-patch: {pkg} {name}\n")
    return head + "".join(f"diff -Naur {f} {f}\n" for f in files)


# which files each patch touches ('patch' prints them)
PATCH_FILES = {
    ("glibc", "fhs"): ["Makeconfig", "nscd/nscd.h", "nss/db-Makefile", "sunrpc/rpc_main.c",
                       "sysdeps/generic/paths.h", "sysdeps/unix/sysv/linux/paths.h"],
    ("bzip2", "docs"): ["Makefile"],
    ("coreutils", "i18n"): ["bootstrap.conf", "configure.ac", "src/cut.c", "src/expand.c",
                            "src/fold.c", "src/join.c", "src/pr.c", "src/sort.c", "src/uniq.c"],
    ("kbd", "backspace"): ["data/keymaps/i386/qwerty/defkeymap.map",
                           "data/keymaps/i386/qwertz/de-latin1.map"],
    ("sysvinit", "consolidated"): ["src/Makefile", "src/bootlogd.c", "src/shutdown.c"],
}

# Patches are text files: their contents matter to the 'patch' command.
PATCH_TEXT = {
    "glibc-2.40-fhs-1.patch": (
        "Submitted By:            (simulated)\n"
        "Description:             Moves the nss_db and nscd files to FHS compliant\n"
        "                         locations (/var/lib/nss_db instead of /var/db).\n"
        "lfsgame-patch: glibc fhs\n"
        "diff -Naur glibc-2.40.orig/Makeconfig glibc-2.40/Makeconfig\n"
        "--- glibc-2.40.orig/Makeconfig\n"
        "+++ glibc-2.40/Makeconfig\n"
        "@@ -1 +1 @@\n"
        "-vardbdir = $(localstatedir)/db\n"
        "+vardbdir = $(localstatedir)/lib/nss_db\n"),
}



def fake_md5(file: str) -> str:
    return hashlib.md5(f"lfsgame:{file}".encode()).hexdigest()


def wget_list_text() -> str:
    return "".join(p.url + "\n" for p in PACKAGES)


def md5sums_text() -> str:
    return "".join(f"{fake_md5(p.file)}  {p.file}\n" for p in PACKAGES)


def total_size() -> int:
    return sum(p.size for p in PACKAGES)

PATCH_TEXT.update({
    "bzip2-1.0.8-install_docs-1.patch": _patch("bzip2", "docs", PATCH_FILES[("bzip2", "docs")],
                                               "Installs the documentation for bzip2"),
    "coreutils-9.5-i18n-2.patch": _patch("coreutils", "i18n", PATCH_FILES[("coreutils", "i18n")],
                                         "Fixes internationalization bugs in some programs"),
    "kbd-2.6.4-backspace-1.patch": _patch("kbd", "backspace", PATCH_FILES[("kbd", "backspace")],
                                          "Makes Backspace and Delete behave consistently"),
    "sysvinit-3.10-consolidated-1.patch": _patch("sysvinit", "consolidated",
                                                 PATCH_FILES[("sysvinit", "consolidated")],
                                                 "Fixes for bootlogd and shutdown"),
})

BY_FILE: Dict[str, Package] = {p.file: p for p in PACKAGES}
BY_URL: Dict[str, Package] = {p.url: p for p in PACKAGES}
