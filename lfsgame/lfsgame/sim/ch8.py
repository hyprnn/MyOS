"""LFS chapter 8 (the final system) as data.

Every package is one `Pkg`: its book section, the commands of the book (also used by the
test walkthrough), what `make install` puts on the disk and what its objective checks.
The book text of chapter 8 is GENERATED from this table (tools/gen_ch8_book.py writes
lfsgame/book/content/09-final-system.txt; a test checks the file is up to date).

The commands follow LFS 12.2 (System V) as closely as the author remembers them; where the
simulation cannot run the real command (awk, find -exec, long test procedures) the step
is simplified and the book says so. Use the real book for a real build.

Step strings: "$ cmd" command (lines joined with "\n" become "+" continuation lines),
"?$ cmd" command that may legitimately return non-zero, "## heading", "! note",
anything else is a paragraph.
"""
from __future__ import annotations

from dataclasses import dataclass, field
from typing import Dict, List, Tuple

TRIPLET = "x86_64-pc-linux-gnu"   # what the native GCC of chapter 8 calls the machine


@dataclass
class Pkg:
    key: str                  # package name in packages.PACKAGES (srcpkg of the tree)
    sec: str                  # book section
    title: str
    sbu: float
    about: str
    steps: List[str]
    bins: List[str] = field(default_factory=list)     # PREFIX/bin (or --bindir)
    sbins: List[str] = field(default_factory=list)    # PREFIX/sbin (or --sbindir)
    libs: List[str] = field(default_factory=list)     # PREFIX/lib (or --libdir)
    files: List[str] = field(default_factory=list)    # "{P}" = prefix, else absolute
    links: Dict[str, str] = field(default_factory=dict)  # installed symlinks
    need: List[tuple] = field(default_factory=list)   # extra objective conditions
    tests: str = ""          # what the test suite reports ("" = all pass)
    test_status: int = 0     # exit status of the test run
    tsbu: float = 0.0        # extra SBU for the tests
    style: str = "auto"      # auto | make | none | pip | perl | meson | ninja | config | Configure
    confdir: str = ""        # directory of the configure script (tcl: unix)
    builddir: bool = False   # configure must run in a separate directory
    srcfiles: Dict[str, str] = field(default_factory=dict)   # extra files in the tree
    products: Dict[str, List[str]] = field(default_factory=dict)  # files make leaves in the tree
    subdirs: List[str] = field(default_factory=list)  # make -C DIR works there
    critical: bool = False   # the objective requires the test suite to have run
    patch: str = ""          # patch name that must be applied
    prefix: str = "/usr/local"
    requires: List[str] = field(default_factory=list)  # files configure needs (else error)
    from_src: Dict[str, str] = field(default_factory=dict)  # tree file -> installed path
    cond: List[tuple] = field(default_factory=list)  # (file in build dir, text, [paths]) installed if text present
    makefile: str = ""       # text of the generated/preset Makefile
    backend: str = ""        # pip packages: build backend that must be installed
    topdir: str = ""
    extra_targets: Tuple[str, ...] = ("html", "install-html", "install-tex", "install-man",
                                      "install-private-headers", "check-root")
    frame: bool = True       # add the cd/tar/cd ... rm -rf frame
    tarball: str = ""        # set from packages at import time


def _pip(key, sec, title, sbu, about, name, backend, files, bins=(), extra=()):
    return Pkg(key, sec, title, sbu, about,
               ["Build a wheel (a Python package archive) and install it:",
                "$ pip3 wheel -w dist --no-cache-dir --no-build-isolation --no-deps $PWD",
                f"$ pip3 install --no-index --no-user --find-links dist {name}"] + list(extra),
               bins=list(bins), files=files, style="pip", prefix="/usr", backend=backend,
               srcfiles={"pyproject.toml": f"[project]\nname = \"{name}\"\n"})


PY = "{P}/lib/python3.12/site-packages"

PKGS: List[Pkg] = [
    Pkg("man-pages", "8.3", "Man-pages-6.9.1", 0.1,
        "Over 2,400 manual pages about the C library, system calls and file formats.",
        ["The pages about password hashing functions come with Libxcrypt (8.27); remove the "
         "copies from this package:",
         "$ rm -v man3/crypt*",
         "Install. -R and GIT=false keep make from looking for git:",
         "$ make -R GIT=false prefix=/usr install"],
        files=["{P}/share/man/man2/open.2", "{P}/share/man/man3/printf.3",
               "{P}/share/man/man5/passwd.5", "{P}/share/man/man7/man-pages.7",
               "{P}/share/man/man8/ld.so.8"],
        from_src={"man3/crypt.3": "{P}/share/man/man3/crypt.3"},
        need=[("absent", "/usr/share/man/man3/crypt.3")],
        style="make", srcfiles={"man3/crypt.3": ".TH CRYPT 3\n", "man3/crypt_r.3": ".so crypt.3\n"}),
    Pkg("iana-etc", "8.4", "Iana-Etc-20240806", 0.0,
        "The lists of network services and protocols (/etc/services, /etc/protocols).",
        ["Nothing to build, just copy the two files:",
         "$ cp services protocols /etc"],
        need=[("contains", "/etc/services", "ssh"), ("file", "/etc/protocols")], style="none",
        srcfiles={"services": "# /etc/services\nssh 22/tcp\nhttp 80/tcp\nhttps 443/tcp\n",
                  "protocols": "# /etc/protocols\nip 0 IP\ntcp 6 TCP\nudp 17 UDP\n"}),
    Pkg("glibc", "8.5", "Glibc-2.40", 12.0,
        "The C library, this time for good, with its test suite, locales, name service "
        "configuration, time zones and the dynamic loader configuration.",
        ["$ patch -Np1 -i ../glibc-2.40-fhs-1.patch",
         "$ mkdir -v build",
         "$ cd build",
         "$ echo \"rootsbindir=/usr/sbin\" > configparms",
         "$ ../configure --prefix=/usr                   \\\n"
         "             --disable-werror                \\\n"
         "             --enable-kernel=4.19            \\\n"
         "             --enable-stack-protector=strong \\\n"
         "             --disable-nscd                  \\\n"
         "             libc_cv_slibdir=/usr/lib",
         "$ make",
         "The test suite is critical: do not skip it. A few failures are known and harmless "
         "(the book lists them, e.g. io/tst-lchmod in a chroot).",
         "?$ make check",
         "The installation complains if /etc/ld.so.conf is missing, and runs a sanity test "
         "that would fail in the chroot; prevent both:",
         "$ touch /etc/ld.so.conf",
         "$ sed '/test-installation/s@$(PERL)@echo not running@' -i ../Makefile",
         "$ make install",
         "$ sed '/RTLDLIST=/s@/usr@@g' -i /usr/bin/ldd",
         "## Locales",
         "Locales define language-specific behaviour (dates, sorting, messages). The test "
         "suites of later packages need some of them. The real book installs a long list; "
         "these are enough here (add your own language, e.g. ru_RU):",
         "$ localedef -i C -f UTF-8 C.UTF-8",
         "$ localedef -i en_US -f ISO-8859-1 en_US",
         "$ localedef -i en_US -f UTF-8 en_US.UTF-8",
         "$ localedef -i de_DE -f UTF-8 de_DE.UTF-8",
         "$ localedef -i ru_RU -f UTF-8 ru_RU.UTF-8",
         "## /etc/nsswitch.conf",
         "Glibc looks up users, groups and hosts according to this file:",
         "$ cat > /etc/nsswitch.conf << \"EOF\"\n# Begin /etc/nsswitch.conf\n\npasswd: files\n"
         "group: files\nshadow: files\n\nhosts: files dns\nnetworks: files\n\nprotocols: files\n"
         "services: files\nethers: files\nrpc: files\n\n# End /etc/nsswitch.conf\nEOF",
         "## Time zones",
         "Unpack the time zone data and compile it with zic:",
         "$ tar -xf ../../tzdata2024a.tar.gz",
         "$ ZONEINFO=/usr/share/zoneinfo",
         "$ mkdir -pv $ZONEINFO/{posix,right}",
         "$ for tz in etcetera southamerica northamerica europe africa antarctica  \\\n"
         "          asia australasia backward; do\n"
         "    zic -L /dev/null   -d $ZONEINFO       ${tz}\n"
         "    zic -L /dev/null   -d $ZONEINFO/posix ${tz}\n"
         "    zic -L leap-seconds -d $ZONEINFO/right ${tz}\n"
         "done",
         "$ cp -v zone.tab zone1970.tab iso3166.tab $ZONEINFO",
         "$ zic -d $ZONEINFO -p America/New_York",
         "$ unset ZONEINFO",
         "tzselect asks where you live and prints the name of your time zone:",
         "$ tzselect",
         "Then make it the system's local time (use the name tzselect printed):",
         "$ ln -sfv /usr/share/zoneinfo/Europe/Moscow /etc/localtime",
         "## The dynamic loader",
         "/etc/ld.so.conf lists additional directories where programs look for libraries:",
         "$ cat > /etc/ld.so.conf << \"EOF\"\n# Begin /etc/ld.so.conf\n/usr/local/lib\n/opt/lib\n\nEOF",
         "$ cat >> /etc/ld.so.conf << \"EOF\"\n# Add an include directory\n"
         "include /etc/ld.so.conf.d/*.conf\n\nEOF",
         "$ mkdir -pv /etc/ld.so.conf.d"],
        bins=["localedef", "locale", "zic", "zdump", "tzselect", "iconv", "getconf", "getent"],
        sbins=["ldconfig", "sln"],
        libs=["libc.so.6", "libm.so.6", "ld-linux-x86-64.so.2", "libresolv.so.2",
              "crt1.o", "Scrt1.o", "crti.o", "crtn.o", "libc_nonshared.a"],
        files=["{P}/include/stdio.h", "{P}/include/stdlib.h", "{P}/include/unistd.h",
               "{P}/share/i18n/locales/C", "{P}/share/i18n/locales/en_US",
               "{P}/share/i18n/locales/de_DE", "{P}/share/i18n/locales/ru_RU",
               "{P}/share/i18n/charmaps/UTF-8.gz", "{P}/share/i18n/charmaps/ISO-8859-1.gz",
               "/var/lib/nss_db/Makefile"],
        need=[("contains", "/usr/bin/ldd", 'RTLDLIST="/lib/')],
        tests="Summary of test results:\n      4 FAIL\n   5726 PASS\n    184 UNSUPPORTED\n"
              "     16 XFAIL\n      8 XPASS\nFAIL: io/tst-lchmod\nFAIL: misc/tst-ttyname\n"
              "FAIL: nptl/tst-thread-affinity-sched\nFAIL: elf/tst-glibc-hwcaps-cache",
        test_status=2, tsbu=10.0, builddir=True, critical=True, patch="fhs",
        srcfiles={"Makefile": "# glibc top-level Makefile\ntests: $(PERL) "
                              "scripts/test-installation.pl\n"}),
    Pkg("zlib", "8.6", "Zlib-1.3.1", 0.1,
        "Compression and decompression routines used by many programs.",
        ["$ ./configure --prefix=/usr", "$ make", "$ make check", "$ make install",
         "The static library is not needed:",
         "$ rm -fv /usr/lib/libz.a"],
        libs=["libz.so.1.3.1", "libz.a"], links={"{P}/lib/libz.so.1": "libz.so.1.3.1",
                                                 "{P}/lib/libz.so": "libz.so.1.3.1"},
        files=["{P}/include/zlib.h"], need=[("absent", "/usr/lib/libz.a")]),
    Pkg("bzip2", "8.7", "Bzip2-1.0.8", 0.1,
        "The bzip2 compressor. It has no configure script and a plain Makefile.",
        ["Apply the patch that installs the documentation:",
         "$ patch -Np1 -i ../bzip2-1.0.8-install_docs-1.patch",
         "Make the installed links relative, and put the man pages in the right place:",
         "$ sed -i 's@\\(ln -s -f \\)$(PREFIX)/bin/@\\1@' Makefile",
         "$ sed -i \"s@(PREFIX)/man@(PREFIX)/share/man@g\" Makefile",
         "First build the shared library with its own Makefile, then clean up and build "
         "the rest:",
         "$ make -f Makefile-libbz2_so",
         "$ make clean",
         "$ make",
         "$ make PREFIX=/usr install",
         "Install the shared library and the program linked against it:",
         "$ cp -av libbz2.so.* /usr/lib",
         "$ ln -sv libbz2.so.1.0.8 /usr/lib/libbz2.so",
         "$ cp -v bzip2-shared /usr/bin/bzip2",
         "$ for i in /usr/bin/{bzcat,bunzip2}; do\n  ln -sfv bzip2 $i\ndone",
         "$ rm -fv /usr/lib/libbz2.a"],
        bins=["bzip2", "bzip2recover", "bzdiff", "bzgrep", "bzmore"], libs=["libbz2.a"],
        files=["{P}/include/bzlib.h"],
        cond=[("@topdir/Makefile", "patched: docs", ["{P}/share/doc/bzip2-1.0.8/manual.html"])],
        need=[("file", "/usr/lib/libbz2.so.1.0.8"), ("link", "/usr/lib/libbz2.so", "libbz2.so.1.0.8"),
              ("link", "/usr/bin/bzcat", "bzip2"), ("absent", "/usr/lib/libbz2.a")],
        style="make", patch="docs",
        products={"-f Makefile-libbz2_so": ["libbz2.so.1.0.8", "bzip2-shared"],
                  "all": ["bzip2", "bzip2recover", "libbz2.a"]},
        makefile="# bzip2 Makefile\nPREFIX=/usr/local\ninstall: bzip2\n"
                 "\tln -s -f $(PREFIX)/bin/bzgrep $(PREFIX)/bin/bzegrep\n"
                 "\tcp -f bzip2.1 $(PREFIX)/man/man1\n",
        srcfiles={"Makefile-libbz2_so": "# shared library Makefile\n"}),
    Pkg("xz", "8.8", "Xz-5.6.2", 0.1, "The xz and lzma compressors.",
        ["$ ./configure --prefix=/usr    \\\n            --disable-static \\\n"
         "            --docdir=/usr/share/doc/xz-5.6.2",
         "$ make", "$ make check", "$ make install"],
        bins=["xz", "xzdec", "lzmainfo", "xzdiff", "xzgrep"],
        links={"{P}/bin/unxz": "xz", "{P}/bin/xzcat": "xz", "{P}/bin/lzma": "xz",
               "{P}/lib/liblzma.so.5": "liblzma.so.5.6.2", "{P}/lib/liblzma.so": "liblzma.so.5.6.2"},
        libs=["liblzma.so.5.6.2"]),
    Pkg("lz4", "8.9", "Lz4-1.10.0", 0.1, "A very fast compression library.",
        ["$ make BUILD_STATIC=no PREFIX=/usr", "$ make -j1 check",
         "$ make BUILD_STATIC=no PREFIX=/usr install"],
        bins=["lz4"], links={"{P}/bin/lz4c": "lz4", "{P}/bin/lz4cat": "lz4"},
        libs=["liblz4.so.1.10.0"], style="make"),
    Pkg("zstd", "8.10", "Zstd-1.5.6", 0.4, "Zstandard: fast compression with good ratios.",
        ["$ make prefix=/usr", "$ make check", "$ make prefix=/usr install",
         "$ rm -v /usr/lib/libzstd.a"],
        bins=["zstd"], links={"{P}/bin/zstdcat": "zstd", "{P}/bin/unzstd": "zstd"},
        libs=["libzstd.so.1.5.6", "libzstd.a"], need=[("absent", "/usr/lib/libzstd.a")],
        style="make"),
    Pkg("file", "8.11", "File-5.45", 0.1, "Determines the type of a file.",
        ["$ ./configure --prefix=/usr", "$ make", "$ make check", "$ make install"],
        bins=["file"], libs=["libmagic.so.1.0.0"], files=["{P}/share/misc/magic.mgc"]),
    Pkg("readline", "8.12", "Readline-8.2.13", 0.1,
        "Line editing and history for interactive programs (bash uses it).",
        ["Reinstalling Readline would otherwise move the old libraries to .old files, and "
         "add a hard-coded library search path; prevent both:",
         "$ sed -i '/MV.*old/d' Makefile.in",
         "$ sed -i '/{OLDSUFF}/c:' support/shlib-install",
         "$ sed -i 's/-Wl,-rpath,[^ ]*//' support/shobj-conf",
         "$ ./configure --prefix=/usr    \\\n            --disable-static \\\n"
         "            --with-curses    \\\n            --docdir=/usr/share/doc/readline-8.2.13",
         "$ make SHLIB_LIBS=\"-lncursesw\"",
         "$ make SHLIB_LIBS=\"-lncursesw\" install"],
        libs=["libreadline.so.8.2", "libhistory.so.8.2"], files=["{P}/include/readline/readline.h"],
        srcfiles={"Makefile.in": "# readline\n\t$(MV) $@ $@.old\n",
                  "support/shlib-install": "#! /bin/sh\nOLD=${OLDSUFF}\n",
                  "support/shobj-conf": "SHLIB_XLDFLAGS='-Wl,-rpath,$(libdir)'\n"}),
    Pkg("m4", "8.13", "M4-1.4.19", 0.3, "A macro processor.",
        ["$ ./configure --prefix=/usr", "$ make", "$ make check", "$ make install"],
        bins=["m4"]),
    Pkg("bc", "8.14", "Bc-6.7.6", 0.1, "An arbitrary precision calculator.",
        ["Bc has its own configure script; it wants the compiler named explicitly:",
         "$ CC=gcc ./configure --prefix=/usr -G -O3 -r", "$ make", "$ make test",
         "$ make install"],
        bins=["bc", "dc"]),
    Pkg("flex", "8.15", "Flex-2.6.4", 0.1, "Generates programs that recognize patterns in text.",
        ["$ ./configure --prefix=/usr \\\n            --docdir=/usr/share/doc/flex-2.6.4 \\\n"
         "            --disable-static",
         "$ make", "$ make check", "$ make install",
         "Some programs still call the old name lex:",
         "$ ln -sv flex   /usr/bin/lex",
         "$ ln -sv flex.1 /usr/share/man/man1/lex.1"],
        bins=["flex", "flex++"], libs=["libfl.so.2.0.0"], files=["{P}/share/man/man1/flex.1"],
        need=[("link", "/usr/bin/lex", "flex")]),
    Pkg("tcl", "8.16", "Tcl-8.6.14", 2.9,
        "The Tool Command Language. It is needed to run the test suites of Binutils, GCC "
        "and other packages (through Expect and DejaGNU).",
        ["The configure script is in the unix subdirectory:",
         "$ SRCDIR=$(pwd)",
         "$ cd unix",
         "$ ./configure --prefix=/usr           \\\n            --mandir=/usr/share/man \\\n"
         "            --disable-rpath",
         "$ make",
         "! The real book now runs several sed commands that remove references to the build "
         "directory from the configuration files; they are left out here.",
         "$ make test", "$ make install",
         "Make the library writable so it can be stripped later, and install the headers "
         "Expect needs:",
         "$ chmod -v u+w /usr/lib/libtcl8.6.so",
         "$ make install-private-headers",
         "$ ln -sfv tclsh8.6 /usr/bin/tclsh",
         "A man page name clashes with one from Perl; rename it:",
         "$ mv /usr/share/man/man3/{Thread,Tcl_Thread}.3"],
        bins=["tclsh8.6"], libs=["libtcl8.6.so", "libtclstub8.6.a"],
        files=["{P}/include/tcl.h", "{P}/share/man/man3/Thread.3"],
        need=[("link", "/usr/bin/tclsh", "tclsh8.6"), ("file", "/usr/share/man/man3/Tcl_Thread.3")],
        confdir="unix", tsbu=1.0),
    Pkg("expect", "8.17", "Expect-5.45.4", 0.2,
        "Talks to interactive programs from scripts; the test suites need it.",
        ["Expect needs working pseudo terminals (PTYs). Check them with Python; it must "
         "print ok (if not, /dev/pts is not mounted: see 7.3):",
         "$ python3 -c 'from pty import spawn; spawn([\"echo\", \"ok\"])'",
         "$ ./configure --prefix=/usr           \\\n            --with-tcl=/usr/lib     \\\n"
         "            --enable-shared         \\\n            --disable-rpath         \\\n"
         "            --mandir=/usr/share/man \\\n            --with-tclinclude=/usr/include",
         "$ make", "$ make test", "$ make install",
         "$ ln -svf expect5.45.4/libexpect5.45.4.so /usr/lib"],
        bins=["expect"], files=["{P}/lib/expect5.45.4/libexpect5.45.4.so"],
        need=[("link", "/usr/lib/libexpect5.45.4.so", "expect5.45.4/libexpect5.45.4.so")],
        requires=["/usr/lib/libtcl8.6.so"], topdir="expect5.45.4"),
    Pkg("dejagnu", "8.18", "DejaGNU-1.6.3", 0.1, "A framework for running test suites.",
        ["$ mkdir -v build", "$ cd build", "$ ../configure --prefix=/usr", "$ make install",
         "$ make check"],
        bins=["runtest"], files=["{P}/share/dejagnu/runtest.exp"]),
    Pkg("pkgconf", "8.19", "Pkgconf-2.3.0", 0.1,
        "Tells build systems where libraries and their headers are (pkg-config).",
        ["$ ./configure --prefix=/usr    \\\n            --disable-static \\\n"
         "            --docdir=/usr/share/doc/pkgconf-2.3.0",
         "$ make", "$ make install",
         "Most packages still call it pkg-config:",
         "$ ln -sv pkgconf   /usr/bin/pkg-config",
         "$ ln -sv pkgconf.1 /usr/share/man/man1/pkg-config.1"],
        bins=["pkgconf"], libs=["libpkgconf.so.5.0.0"], files=["{P}/share/man/man1/pkgconf.1"],
        need=[("link", "/usr/bin/pkg-config", "pkgconf")]),
    Pkg("binutils", "8.20", "Binutils-2.43.1", 1.6,
        "The final assembler, linker and object file tools. Its test suite is critical.",
        ["$ mkdir -v build", "$ cd build",
         "$ ../configure --prefix=/usr       \\\n             --sysconfdir=/etc   \\\n"
         "             --enable-ld=default \\\n             --enable-plugins    \\\n"
         "             --enable-shared     \\\n             --disable-werror    \\\n"
         "             --enable-64-bit-bfd \\\n             --enable-new-dtags  \\\n"
         "             --with-system-zlib  \\\n             --enable-default-hash-style=gnu",
         "$ make tooldir=/usr",
         "-k keeps going after a failing test, so you see all failures:",
         "$ make -k check",
         "List the failures (there should be none). grep finds nothing and returns 1, which "
         "is good here:",
         "?$ grep '^FAIL:' $(find -name '*.log')",
         "$ make tooldir=/usr install",
         "Remove the static libraries and a documentation directory that are not needed:",
         "$ rm -rfv /usr/lib/lib{bfd,ctf,ctf-nobfd,gprofng,opcodes,sframe}.a \\\n"
         "         /usr/share/doc/gprofng/"],
        bins=["ld", "ld.bfd", "as", "ar", "nm", "objcopy", "objdump", "readelf", "strip",
              "ranlib", "size", "strings", "addr2line", "c++filt", "elfedit", "gprof", "gprofng"],
        libs=["libbfd-2.43.1.so", "libctf.so.0.0.0", "libopcodes-2.43.1.so",
              "libsframe.so.1.0.0", "libbfd.a", "libctf.a", "libctf-nobfd.a", "libgprofng.a",
              "libopcodes.a", "libsframe.a"],
        files=["{P}/lib/bfd-plugins/libdep.so", "{P}/share/doc/gprofng/gprofng.html"],
        need=[("absent", "/usr/lib/libbfd.a"), ("absent", "/usr/share/doc/gprofng")],
        products={"check": ["ld/ld.log", "gas/testsuite/gas.log", "binutils/binutils.log"]},
        tests="=== binutils Summary ===\n# of expected passes            320\n"
              "=== gas Summary ===\n# of expected passes            1414\n"
              "=== ld Summary ===\n# of expected passes            2780\n"
              "# of unsupported tests          173",
        tsbu=2.0, builddir=True, critical=True, prefix="/usr/local"),
    Pkg("gmp", "8.21", "GMP-6.3.0", 0.3,
        "Arithmetic on arbitrary precision numbers (GCC needs it). Tests are critical.",
        ["$ ./configure --prefix=/usr    \\\n            --enable-cxx     \\\n"
         "            --disable-static \\\n            --docdir=/usr/share/doc/gmp-6.3.0",
         "$ make", "$ make html",
         "Run the tests and keep their output:",
         "$ make check 2>&1 | tee gmp-check-log",
         "All 199 tests must pass. The real book adds up the PASS lines with awk; this "
         "shell has no awk, so look at them:",
         "$ grep '# PASS:' gmp-check-log",
         "$ make install", "$ make install-html"],
        libs=["libgmp.so.10.5.0", "libgmpxx.so.4.7.0"], files=["{P}/include/gmp.h"],
        tests="# PASS:  33\n# PASS:  56\n# PASS:  62\n# PASS:  48\n(199 passed in total)",
        critical=True, tsbu=1.0),
    Pkg("mpfr", "8.22", "MPFR-4.2.1", 0.2,
        "Multiple-precision floating point math (GCC needs it). Tests are critical.",
        ["$ ./configure --prefix=/usr        \\\n            --disable-static     \\\n"
         "            --enable-thread-safe \\\n            --docdir=/usr/share/doc/mpfr-4.2.1",
         "$ make", "$ make html", "$ make check", "$ make install", "$ make install-html"],
        libs=["libmpfr.so.6.2.1"], files=["{P}/include/mpfr.h"],
        tests="# TOTAL: 198\n# PASS:  198", critical=True, tsbu=0.6),
    Pkg("mpc", "8.23", "MPC-1.3.1", 0.1, "Complex number arithmetic (GCC needs it).",
        ["$ ./configure --prefix=/usr    \\\n            --disable-static \\\n"
         "            --docdir=/usr/share/doc/mpc-1.3.1",
         "$ make", "$ make html", "$ make check", "$ make install", "$ make install-html"],
        libs=["libmpc.so.3.3.1"], files=["{P}/include/mpc.h"]),
    Pkg("attr", "8.24", "Attr-2.5.2", 0.1, "Extended attributes of filesystem objects.",
        ["$ ./configure --prefix=/usr     \\\n            --disable-static  \\\n"
         "            --sysconfdir=/etc \\\n            --docdir=/usr/share/doc/attr-2.5.2",
         "$ make", "$ make check", "$ make install"],
        bins=["attr", "getfattr", "setfattr"], libs=["libattr.so.1.1.2502"]),
    Pkg("acl", "8.25", "Acl-2.3.2", 0.1, "Access control lists: finer-grained permissions.",
        ["$ ./configure --prefix=/usr         \\\n            --disable-static      \\\n"
         "            --docdir=/usr/share/doc/acl-2.3.2",
         "$ make",
         "! The tests need a Coreutils built with ACL support; the real book runs them after "
         "Coreutils (8.58). Skipped here.",
         "$ make install"],
        bins=["getfacl", "setfacl", "chacl"], libs=["libacl.so.1.1.2302"]),
    Pkg("libcap", "8.26", "Libcap-2.70", 0.1, "POSIX capabilities: parts of root's power.",
        ["Do not install the static libraries:",
         "$ sed -i '/install -m.*STA/d' libcap/Makefile",
         "$ make prefix=/usr lib=lib", "$ make test", "$ make prefix=/usr lib=lib install"],
        sbins=["capsh", "getcap", "setcap", "getpcaps"], libs=["libcap.so.2.70", "libpsx.so.2.70"],
        style="make", srcfiles={"libcap/Makefile": "\tinstall -m 0644 $(STALIBNAME) $(LIBDIR)\n"}),
    Pkg("libxcrypt", "8.27", "Libxcrypt-4.4.36", 0.1, "Modern password hashing (yescrypt).",
        ["$ ./configure --prefix=/usr                \\\n"
         "            --enable-hashes=strong,glibc \\\n"
         "            --enable-obsolete-api=no     \\\n"
         "            --disable-static             \\\n"
         "            --disable-failure-tokens",
         "$ make", "$ make check", "$ make install"],
        libs=["libcrypt.so.2.0.0"], files=["{P}/include/crypt.h", "{P}/share/man/man3/crypt.3"]),
    Pkg("shadow", "8.28", "Shadow-4.16.0", 0.1,
        "Secure password handling: passwd, useradd, su and the /etc/shadow file.",
        ["Do not install groups (Coreutils has a better one):",
         "$ sed -i 's/groups$(EXEEXT) //' src/Makefile.in",
         "! The real book also removes the matching man pages with a find -exec command, "
         "which this shell does not support.",
         "Use yescrypt for passwords and the modern mail directory:",
         "$ sed -e 's:#ENCRYPT_METHOD DES:ENCRYPT_METHOD YESCRYPT:' \\\n"
         "    -e 's:/var/spool/mail:/var/mail:'                   \\\n    -i etc/login.defs",
         "The installation expects /usr/bin/passwd to exist:",
         "$ touch /usr/bin/passwd",
         "$ ./configure --sysconfdir=/etc   \\\n            --disable-static    \\\n"
         "            --with-{b,yes}crypt \\\n            --without-libbsd    \\\n"
         "            --with-group-name-max-length=32",
         "$ make",
         "$ make exec_prefix=/usr install",
         "## Configuration",
         "Move the passwords from /etc/passwd into /etc/shadow, readable only by root:",
         "$ pwconv",
         "$ grpconv",
         "New users get the group users (999) by default:",
         "$ mkdir -p /etc/default",
         "$ useradd -D --gid 999",
         "Give root a password (you will need it to log in to the new system):",
         "$ passwd root"],
        bins=["passwd", "chage", "chfn", "chsh", "expiry", "gpasswd", "newgrp", "su", "login"],
        sbins=["useradd", "userdel", "usermod", "groupadd", "groupdel", "groupmod", "pwconv",
               "grpconv", "chpasswd", "newusers"],
        from_src={"etc/login.defs": "/etc/login.defs"},
        need=[("contains", "/etc/login.defs", "ENCRYPT_METHOD YESCRYPT"),
              ("contains", "/etc/default/useradd", "GROUP=999"), ("file", "/etc/shadow"),
              ("flag", "chroot_passwd:root")],
        prefix="/usr",
        srcfiles={"src/Makefile.in": "ubin_PROGRAMS = groups$(EXEEXT) passwd$(EXEEXT)\n",
                  "etc/login.defs": "# login.defs\n#ENCRYPT_METHOD DES\nMAIL_DIR /var/spool/mail\n"}),
    Pkg("gcc", "8.29", "GCC-14.2.0", 11.0,
        "The final compiler. This is the longest build of the book; the test suite takes "
        "even longer and is critical. Afterwards a series of sanity checks makes sure the "
        "new toolchain uses the new system's files.",
        ["On x86_64, use lib instead of lib64:",
         "$ sed -e '/m64=/s/lib64/lib/' -i.orig gcc/config/i386/t-linux64",
         "$ mkdir -v build", "$ cd build",
         "$ ../configure --prefix=/usr            \\\n             LD=ld                    \\\n"
         "             --enable-languages=c,c++ \\\n             --enable-default-pie     \\\n"
         "             --enable-default-ssp     \\\n             --enable-host-pie        \\\n"
         "             --disable-multilib       \\\n             --disable-bootstrap      \\\n"
         "             --disable-fixincludes    \\\n             --with-system-zlib",
         "$ make",
         "## Tests",
         "Some tests need a big stack, and the tests must not run as root. Give the build "
         "tree to tester and run them as that user:",
         "$ ulimit -s -H unlimited",
         "$ chown -R tester .",
         "?$ su tester -c \"PATH=$PATH make -k -j$(nproc) check\"",
         "A summary of the results (a few unexpected failures are normal):",
         "$ ../contrib/test_summary | grep -A7 Summ",
         "$ make install",
         "The headers were installed with tester as owner; fix that:",
         "$ chown -v -R root:root \\\n    /usr/lib/gcc/$(gcc -dumpmachine)/14.2.0/include{,-fixed}",
         "Historical links (some programs call /usr/lib/cpp, cc):",
         "$ ln -svr /usr/bin/cpp /usr/lib",
         "$ ln -sv gcc.1 /usr/share/man/man1/cc.1",
         "Let ld use GCC's plugin for link-time optimization:",
         "$ ln -sfv ../../libexec/gcc/$(gcc -dumpmachine)/14.2.0/liblto_plugin.so \\\n"
         "        /usr/lib/bfd-plugins/",
         "## Sanity checks",
         "Compile a test program and keep everything the compiler and linker say:",
         "$ echo 'int main(){}' > dummy.c",
         "$ cc dummy.c -v -Wl,--verbose &> dummy.log",
         "$ readelf -l a.out | grep ': /lib'",
         "It must request /lib64/ld-linux-x86-64.so.2. The start files must come from /usr/lib:",
         "$ grep -E -o '/usr/lib.*/S?crt[1in].*succeeded' dummy.log",
         "The header search path must contain /usr/include:",
         "$ grep -B4 '^ /usr/include' dummy.log",
         "The linker must search /usr/lib:",
         "$ grep 'SEARCH.*/usr/lib' dummy.log |sed 's|; |\\n|g'",
         "It must use the right libc and dynamic loader:",
         "$ grep \"/lib.*/libc.so.6 \" dummy.log",
         "$ grep found dummy.log",
         "$ rm -v dummy.c a.out dummy.log",
         "Finally, move a file of the GDB integration where GDB looks for it:",
         "$ mkdir -pv /usr/share/gdb/auto-load/usr/lib",
         "$ mv -v /usr/lib/*gdb.py /usr/share/gdb/auto-load/usr/lib"],
        bins=["gcc", "g++", "cpp", "c++", "gcov", "gcc-ar", "gcc-nm", "gcc-ranlib", "lto-dump",
              f"{TRIPLET}-gcc", f"{TRIPLET}-g++"],
        libs=["libgcc_s.so.1", "libstdc++.so.6.0.33", "libstdc++.so.6.0.33-gdb.py",
              "libssp.so.0.0.0"],
        links={"{P}/bin/cc": "gcc", "{P}/lib/libstdc++.so.6": "libstdc++.so.6.0.33",
               "{P}/lib/libstdc++.so": "libstdc++.so.6.0.33"},
        files=[f"{{P}}/lib/gcc/{TRIPLET}/14.2.0/include/stddef.h",
               f"{{P}}/lib/gcc/{TRIPLET}/14.2.0/include-fixed/README",
               f"{{P}}/lib/gcc/{TRIPLET}/14.2.0/crtbegin.o",
               f"{{P}}/libexec/gcc/{TRIPLET}/14.2.0/liblto_plugin.so",
               f"{{P}}/libexec/gcc/{TRIPLET}/14.2.0/cc1",
               "{P}/share/man/man1/gcc.1"],
        need=[("flag", "sanity8"), ("link", "/usr/lib/cpp", "../bin/cpp"),
              ("file", "/usr/share/gdb/auto-load/usr/lib/libstdc++.so.6.0.33-gdb.py"),
              ("link", "/usr/lib/bfd-plugins/liblto_plugin.so",
               f"../../libexec/gcc/{TRIPLET}/14.2.0/liblto_plugin.so")],
        tests="\t\t=== gcc Summary ===\n# of expected passes\t\t185226\n"
              "# of unexpected failures\t12\n# of expected failures\t\t1467\n"
              "# of unsupported tests\t\t2571\n\t\t=== g++ Summary ===\n"
              "# of expected passes\t\t253409\n# of unexpected failures\t4",
        test_status=2, tsbu=35.0, builddir=True, critical=True,
        requires=["/usr/lib/libgmp.so.10.5.0", "/usr/lib/libmpfr.so.6.2.1",
                  "/usr/lib/libmpc.so.3.3.1"],
        srcfiles={"contrib/test_summary": "#!/bin/sh\n# test_summary\n"}),
    Pkg("ncurses", "8.30", "Ncurses-6.5", 0.2,
        "The terminal handling library, now for good.",
        ["$ ./configure --prefix=/usr           \\\n            --mandir=/usr/share/man \\\n"
         "            --with-shared           \\\n            --without-debug         \\\n"
         "            --without-normal        \\\n            --with-cxx-shared       \\\n"
         "            --enable-pc-files       \\\n"
         "            --with-pkg-config-libdir=/usr/lib/pkgconfig",
         "$ make",
         "Installing directly would overwrite libncursesw.so.6.5 while the running shell uses "
         "it, which can crash it. Install into a temporary directory first:",
         "$ make DESTDIR=$PWD/dest install",
         "$ install -vm755 dest/usr/lib/libncursesw.so.6.5 /usr/lib",
         "$ rm -v  dest/usr/lib/libncursesw.so.6.5",
         "$ sed -e 's/^#if.*XOPEN.*$/#if 1/' \\\n    -i dest/usr/include/curses.h",
         "$ cp -av dest/* /",
         "Many programs ask for the non-wide-character libraries; link them:",
         "$ for lib in ncurses form panel menu ; do\n    ln -sfv lib${lib}w.so /usr/lib/lib${lib}.so\n"
         "    ln -sfv ${lib}w.pc    /usr/lib/pkgconfig/${lib}.pc\ndone",
         "$ ln -sfv libncursesw.so /usr/lib/libcursesw.so",
         "$ ln -sfv libncurses.so  /usr/lib/libcurses.so"],
        bins=["tic", "infocmp", "tput", "clear", "tset", "toe", "tabs", "ncursesw6-config"],
        libs=["libncursesw.so.6.5", "libformw.so.6.5", "libmenuw.so.6.5", "libpanelw.so.6.5"],
        links={"{P}/lib/libncursesw.so.6": "libncursesw.so.6.5",
               "{P}/lib/libncursesw.so": "libncursesw.so.6", "{P}/bin/reset": "tset",
               "{P}/lib/libformw.so": "libformw.so.6.5", "{P}/lib/libmenuw.so": "libmenuw.so.6.5",
               "{P}/lib/libpanelw.so": "libpanelw.so.6.5"},
        files=["{P}/include/curses.h", "{P}/lib/pkgconfig/ncursesw.pc",
               "{P}/lib/pkgconfig/formw.pc", "{P}/lib/pkgconfig/menuw.pc",
               "{P}/lib/pkgconfig/panelw.pc", "{P}/share/terminfo/x/xterm"],
        need=[("link", "/usr/lib/libncurses.so", "libncursesw.so"),
              ("link", "/usr/lib/pkgconfig/ncurses.pc", "ncursesw.pc"),
              ("link", "/usr/lib/libcurses.so", "libncurses.so"),
              ("contains", "/usr/include/curses.h", "#if 1"),
              ("file", "/usr/lib/libncursesw.so.6.5")]),
    Pkg("sed", "8.31", "Sed-4.9", 0.3, "The stream editor.",
        ["$ ./configure --prefix=/usr", "$ make", "$ make html",
         "The tests must run as an ordinary user:",
         "$ chown -R tester .",
         "$ su tester -c \"PATH=$PATH make check\"",
         "$ make install"],
        bins=["sed"]),
    Pkg("psmisc", "8.32", "Psmisc-23.7", 0.1, "killall, fuser and pstree.",
        ["$ ./configure --prefix=/usr", "$ make", "$ make check", "$ make install"],
        bins=["killall", "fuser", "pstree", "peekfd", "prtstat"]),
    Pkg("gettext", "8.33", "Gettext-0.22.5", 1.6, "Translation tools, complete this time.",
        ["$ ./configure --prefix=/usr    \\\n            --disable-static \\\n"
         "            --docdir=/usr/share/doc/gettext-0.22.5",
         "$ make", "$ make check", "$ make install",
         "$ chmod -v 0755 /usr/lib/preloadable_libintl.so"],
        bins=["gettext", "msgfmt", "msgmerge", "xgettext", "envsubst", "autopoint", "gettextize"],
        libs=["libgettextlib-0.22.5.so", "libasprintf.so.0.0.0", "preloadable_libintl.so"],
        tsbu=1.0),
    Pkg("bison", "8.34", "Bison-3.8.2", 2.1, "The parser generator, complete this time.",
        ["$ ./configure --prefix=/usr --docdir=/usr/share/doc/bison-3.8.2", "$ make",
         "$ make check", "$ make install"],
        bins=["bison", "yacc"], libs=["liby.a"], tsbu=5.0),
    Pkg("grep", "8.35", "Grep-3.11", 0.4, "Searches files for patterns.",
        ["egrep and fgrep print a warning that they are obsolete; remove it:",
         "$ sed -i \"s/echo/#echo/\" src/egrep.sh",
         "$ ./configure --prefix=/usr", "$ make", "$ make check", "$ make install"],
        bins=["grep", "egrep", "fgrep"],
        srcfiles={"src/egrep.sh": "#!/bin/sh\necho \"$cmd: warning: $cmd is obsolescent\" >&2\n"}),
    Pkg("bash", "8.36", "Bash-5.2.32", 1.4, "The final shell.",
        ["$ ./configure --prefix=/usr             \\\n            --without-bash-malloc     \\\n"
         "            --with-installed-readline \\\n            bash_cv_strtold_broken=no \\\n"
         "            --docdir=/usr/share/doc/bash-5.2.32",
         "$ make",
         "! The real book runs the test suite as tester through an Expect script; skipped here.",
         "$ make install",
         "Start the new bash, replacing the one you are using:",
         "$ exec /usr/bin/bash --login"],
        bins=["bash", "bashbug"]),
    Pkg("libtool", "8.37", "Libtool-2.4.7", 0.6, "Generic library support scripts.",
        ["$ ./configure --prefix=/usr", "$ make", "$ make -k check", "$ make install",
         "$ rm -fv /usr/lib/libltdl.a"],
        bins=["libtool", "libtoolize"], libs=["libltdl.so.7.3.2", "libltdl.a"],
        need=[("absent", "/usr/lib/libltdl.a")], tsbu=1.0),
    Pkg("gdbm", "8.38", "GDBM-1.24", 0.1, "The GNU database manager.",
        ["$ ./configure --prefix=/usr    \\\n            --disable-static \\\n"
         "            --enable-libgdbm-compat",
         "$ make", "$ make check", "$ make install"],
        bins=["gdbmtool", "gdbm_dump", "gdbm_load"],
        libs=["libgdbm.so.6.0.0", "libgdbm_compat.so.4.0.0"]),
    Pkg("gperf", "8.39", "Gperf-3.1", 0.1, "Generates perfect hash functions.",
        ["$ ./configure --prefix=/usr --docdir=/usr/share/doc/gperf-3.1", "$ make",
         "The tests fail when run in parallel:",
         "$ make -j1 check", "$ make install"],
        bins=["gperf"]),
    Pkg("expat", "8.40", "Expat-2.6.2", 0.1, "An XML parsing library.",
        ["$ ./configure --prefix=/usr    \\\n            --disable-static \\\n"
         "            --docdir=/usr/share/doc/expat-2.6.2",
         "$ make", "$ make check", "$ make install"],
        bins=["xmlwf"], libs=["libexpat.so.1.9.2"]),
    Pkg("inetutils", "8.41", "Inetutils-2.5", 0.2, "Basic network programs (ping, ftp, ...).",
        ["Let it build with the current ncurses:",
         "$ sed -i 's/def HAVE_TERMCAP_TGETENT/ 1/' telnet/telnet.c",
         "$ ./configure --prefix=/usr        \\\n            --bindir=/usr/bin    \\\n"
         "            --localstatedir=/var \\\n            --disable-logger     \\\n"
         "            --disable-whois      \\\n            --disable-rcp        \\\n"
         "            --disable-rexec      \\\n            --disable-rlogin     \\\n"
         "            --disable-rsh        \\\n            --disable-servers",
         "$ make", "$ make check", "$ make install",
         "ifconfig belongs to /usr/sbin:",
         "$ mv -v /usr/{,s}bin/ifconfig"],
        bins=["dnsdomainname", "ftp", "hostname", "ifconfig", "ping", "ping6", "talk", "telnet",
              "tftp", "traceroute"],
        need=[("file", "/usr/sbin/ifconfig"), ("absent", "/usr/bin/ifconfig")],
        srcfiles={"telnet/telnet.c": "#ifdef HAVE_TERMCAP_TGETENT\n"}),
    Pkg("less", "8.42", "Less-661", 0.1, "A text file viewer.",
        ["$ ./configure --prefix=/usr --sysconfdir=/etc", "$ make", "$ make check",
         "$ make install"],
        bins=["less", "lessecho", "lesskey"]),
    Pkg("perl", "8.43", "Perl-5.40.0", 1.3, "Perl, complete this time.",
        ["Use the system's zlib and bzip2 instead of Perl's own copies:",
         "$ export BUILD_ZLIB=False",
         "$ export BUILD_BZIP2=0",
         "$ sh Configure -des                                          \\\n"
         "             -D prefix=/usr                                \\\n"
         "             -D vendorprefix=/usr                          \\\n"
         "             -D privlib=/usr/lib/perl5/5.40/core_perl      \\\n"
         "             -D archlib=/usr/lib/perl5/5.40/core_perl      \\\n"
         "             -D sitelib=/usr/lib/perl5/5.40/site_perl      \\\n"
         "             -D sitearch=/usr/lib/perl5/5.40/site_perl     \\\n"
         "             -D vendorlib=/usr/lib/perl5/5.40/vendor_perl  \\\n"
         "             -D vendorarch=/usr/lib/perl5/5.40/vendor_perl \\\n"
         "             -D man1dir=/usr/share/man/man1                \\\n"
         "             -D man3dir=/usr/share/man/man3                \\\n"
         "             -D pager=\"/usr/bin/less -isR\"                 \\\n"
         "             -D useshrplib                                 \\\n"
         "             -D usethreads",
         "$ make",
         "$ TEST_JOBS=$(nproc) make test_harness",
         "$ make install",
         "$ unset BUILD_ZLIB BUILD_BZIP2"],
        bins=["perl", "perl5.40.0", "cpan", "perldoc", "pod2man", "prove"],
        files=["{P}/lib/perl5/5.40/core_perl/strict.pm",
               "{P}/lib/perl5/5.40/core_perl/CORE/libperl.so"],
        style="Configure", tsbu=8.0),
    Pkg("xml-parser", "8.44", "XML::Parser-2.47", 0.1, "A Perl interface to Expat.",
        ["Perl modules are configured with perl itself:",
         "$ perl Makefile.PL", "$ make", "$ make test", "$ make install"],
        files=["/usr/lib/perl5/5.40/site_perl/XML/Parser.pm",
               "/usr/lib/perl5/5.40/site_perl/auto/XML/Parser/Expat/Expat.so"],
        style="perl", srcfiles={"Makefile.PL": "use ExtUtils::MakeMaker;\n"},
        topdir="XML-Parser-2.47"),
    Pkg("intltool", "8.45", "Intltool-0.51.0", 0.1, "Extracts translatable strings.",
        ["! The real book first fixes a warning caused by newer Perl versions with a sed "
         "command full of backslashes; it is left out here.",
         "$ ./configure --prefix=/usr", "$ make", "$ make check", "$ make install",
         "$ install -v -Dm644 doc/I18N-HOWTO /usr/share/doc/intltool-0.51.0/I18N-HOWTO"],
        bins=["intltoolize", "intltool-update", "intltool-merge", "intltool-extract"],
        need=[("file", "/usr/share/doc/intltool-0.51.0/I18N-HOWTO")],
        srcfiles={"doc/I18N-HOWTO": "Autotoolizing your package for i18n\n"}),
    Pkg("autoconf", "8.46", "Autoconf-2.72", 0.1, "Generates configure scripts.",
        ["$ ./configure --prefix=/usr", "$ make", "$ make check", "$ make install"],
        bins=["autoconf", "autoreconf", "autoheader", "autom4te", "autoscan", "autoupdate",
              "ifnames"], tsbu=1.0),
    Pkg("automake", "8.47", "Automake-1.17", 0.1, "Generates Makefiles.",
        ["$ ./configure --prefix=/usr --docdir=/usr/share/doc/automake-1.17", "$ make",
         "$ make check", "$ make install"],
        bins=["automake", "aclocal", "automake-1.17", "aclocal-1.17"], tsbu=1.0),
    Pkg("openssl", "8.48", "OpenSSL-3.3.1", 1.8, "Cryptography and the TLS protocol.",
        ["OpenSSL has its own configuration script, config:",
         "$ ./config --prefix=/usr         \\\n         --openssldir=/etc/ssl \\\n"
         "         --libdir=lib          \\\n         shared                \\\n"
         "         zlib-dynamic",
         "$ make",
         "$ HARNESS_JOBS=$(nproc) make test",
         "Do not install the static libraries:",
         "$ sed -i '/INSTALL_LIBS/s/libcrypto.a libssl.a//' Makefile",
         "$ make MANSUFFIX=ssl install",
         "Give the documentation directory a version, and add the rest of the docs:",
         "$ mv -v /usr/share/doc/openssl /usr/share/doc/openssl-3.3.1",
         "$ cp -vfr doc/* /usr/share/doc/openssl-3.3.1"],
        bins=["openssl", "c_rehash"], libs=["libssl.so.3", "libcrypto.so.3"],
        links={"{P}/lib/libssl.so": "libssl.so.3", "{P}/lib/libcrypto.so": "libcrypto.so.3"},
        files=["/etc/ssl/openssl.cnf", "{P}/include/openssl/ssl.h",
               "{P}/share/doc/openssl/README.md"],
        cond=[("Makefile", "libcrypto.a libssl.a", ["{P}/lib/libcrypto.a", "{P}/lib/libssl.a"])],
        need=[("absent", "/usr/lib/libcrypto.a"), ("file", "/usr/share/doc/openssl-3.3.1/README.md"),
              ("file", "/usr/share/doc/openssl-3.3.1/HOWTO.md")],
        style="config", tsbu=3.0,
        makefile="# Makefile generated by config\nINSTALL_LIBS=libcrypto.a libssl.a\n",
        srcfiles={"doc/HOWTO.md": "OpenSSL HOWTO\n"}),
    Pkg("kmod", "8.49", "Kmod-33", 0.1, "Loads and unloads kernel modules.",
        ["$ ./configure --prefix=/usr     \\\n            --sysconfdir=/etc \\\n"
         "            --with-openssl    \\\n            --with-xz         \\\n"
         "            --with-zstd       \\\n            --with-zlib       \\\n"
         "            --disable-manpages",
         "$ make", "$ make install",
         "All module tools are one program, kmod, under different names:",
         "$ for target in depmod insmod modinfo modprobe rmmod; do\n"
         "  ln -sfv ../bin/kmod /usr/sbin/$target\n  rm -fv /usr/bin/$target\ndone"],
        bins=["kmod"], libs=["libkmod.so.2.4.1"],
        links={"{P}/bin/lsmod": "kmod", "{P}/bin/depmod": "kmod", "{P}/bin/modprobe": "kmod",
               "{P}/bin/insmod": "kmod", "{P}/bin/rmmod": "kmod", "{P}/bin/modinfo": "kmod"},
        need=[("link", "/usr/sbin/modprobe", "../bin/kmod"), ("absent", "/usr/bin/modprobe")]),
    Pkg("elfutils", "8.50", "Libelf from Elfutils-0.191", 0.3,
        "Only the library for ELF files is needed (the kernel build uses it).",
        ["$ ./configure --prefix=/usr                \\\n            --disable-debuginfod         \\\n"
         "            --enable-libdebuginfod=dummy",
         "$ make", "$ make check",
         "Install only libelf:",
         "$ make -C libelf install",
         "$ install -vm644 config/libelf.pc /usr/lib/pkgconfig",
         "$ rm /usr/lib/libelf.a"],
        libs=["libelf-0.191.so", "libelf.a"], links={"{P}/lib/libelf.so.1": "libelf-0.191.so",
                                                     "{P}/lib/libelf.so": "libelf.so.1"},
        files=["{P}/include/libelf.h"],
        need=[("file", "/usr/lib/pkgconfig/libelf.pc"), ("absent", "/usr/lib/libelf.a")],
        subdirs=["libelf"], products={"all": ["config/libelf.pc"]}),
    Pkg("libffi", "8.51", "Libffi-3.4.6", 1.7, "Calls functions whose interface is known only at run time.",
        ["$ ./configure --prefix=/usr          \\\n            --disable-static       \\\n"
         "            --with-gcc-arch=native",
         "$ make", "$ make check", "$ make install"],
        libs=["libffi.so.8.1.4"]),
    Pkg("python", "8.52", "Python-3.12.5", 2.2, "Python 3, complete this time, with pip.",
        ["$ ./configure --prefix=/usr        \\\n            --enable-shared      \\\n"
         "            --with-system-expat  \\\n            --enable-optimizations",
         "$ make", "$ make install",
         "pip warns about running as root and about new versions; silence that:",
         "$ cat > /etc/pip.conf << EOF\n[global]\nroot-user-action = ignore\n"
         "disable-pip-version-check = true\nEOF"],
        bins=["python3.12", "pip3", "pip3.12", "pydoc3.12", "python3-config"],
        links={"{P}/bin/python3": "python3.12"},
        libs=["libpython3.12.so.1.0"], files=["{P}/lib/python3.12/os.py"],
        need=[("contains", "/etc/pip.conf", "root-user-action")], prefix="/usr/local"),
    _pip("flit-core", "8.53", "Flit-Core-3.9.0", 0.1,
         "The build backend of many Python packages.", "flit_core", "",
         [f"{PY}/flit_core/__init__.py"]),
    _pip("wheel", "8.54", "Wheel-0.44.0", 0.1, "The wheel package format tools.", "wheel",
         "flit_core", [f"{PY}/wheel/__init__.py"], bins=["wheel"]),
    _pip("setuptools", "8.55", "Setuptools-72.2.0", 0.1, "Tools to build Python packages.",
         "setuptools", "flit_core", [f"{PY}/setuptools/__init__.py"]),
    Pkg("ninja", "8.56", "Ninja-1.12.1", 0.2, "A small, fast build system (Meson uses it).",
        ["Ninja builds itself with Python:",
         "$ python3 configure.py --bootstrap",
         "$ install -vm755 ninja /usr/bin/",
         "$ install -vDm644 misc/bash-completion /usr/share/bash-completion/completions/ninja",
         "$ install -vDm644 misc/zsh-completion  /usr/share/zsh/site-functions/_ninja"],
        need=[("file", "/usr/bin/ninja"), ("file", "/usr/share/bash-completion/completions/ninja")],
        style="ninja", srcfiles={"configure.py": "#!/usr/bin/env python3\n",
                                 "misc/bash-completion": "# ninja bash completion\n",
                                 "misc/zsh-completion": "#compdef ninja\n"}),
    _pip("meson", "8.57", "Meson-1.5.1", 0.1, "A build system (systemd/udev uses it).", "meson",
         "setuptools", [f"{PY}/mesonbuild/__init__.py"], bins=["meson"],
         extra=["$ install -vDm644 data/shell-completions/bash/meson \\\n"
                "    /usr/share/bash-completion/completions/meson",
                "$ install -vDm644 data/shell-completions/zsh/_meson \\\n"
                "    /usr/share/zsh/site-functions/_meson"]),
    Pkg("coreutils", "8.58", "Coreutils-9.5", 1.0, "The basic file, text and shell utilities, for good.",
        ["The patch fixes problems with multi-byte characters (UTF-8):",
         "$ patch -Np1 -i ../coreutils-9.5-i18n-2.patch",
         "It changes the build system, so regenerate it:",
         "$ autoreconf -fiv",
         "Configure refuses to run as root unless told it is all right:",
         "$ FORCE_UNSAFE_CONFIGURE=1 ./configure \\\n            --prefix=/usr            \\\n"
         "            --enable-no-install-program=kill,uptime",
         "$ make",
         "! The real book runs the test suite partly as root and partly as tester; skipped "
         "here.",
         "$ make install",
         "$ mv -v /usr/bin/chroot /usr/sbin",
         "$ mv -v /usr/share/man/man1/chroot.1 /usr/share/man/man8/chroot.8",
         "$ sed -i 's/\"1\"/\"8\"/' /usr/share/man/man8/chroot.8"],
        bins=["[", "basename", "cat", "chgrp", "chmod", "chown", "chroot", "cp", "cut", "date",
              "dd", "df", "dirname", "du", "echo", "env", "expr", "false", "head", "id",
              "install", "kill", "ln", "ls", "md5sum", "mkdir", "mknod", "mktemp", "mv", "nproc",
              "printf", "pwd", "readlink", "realpath", "rm", "rmdir", "seq", "sha256sum", "sleep",
              "sort", "stat", "stty", "sync", "tail", "tee", "test", "touch", "tr", "true",
              "uname", "uniq", "uptime", "wc", "whoami", "yes"],
        files=["{P}/share/man/man1/chroot.1", "{P}/share/man/man1/ls.1"],
        patch="i18n",
        need=[("file", "/usr/sbin/chroot"), ("absent", "/usr/bin/chroot"),
              ("contains", "/usr/share/man/man8/chroot.8", '"8"'),
              ("absent", "/usr/bin/kill")]),
    Pkg("check", "8.59", "Check-0.15.2", 0.1, "A unit testing framework for C.",
        ["$ ./configure --prefix=/usr --disable-static", "$ make", "$ make check",
         "$ make docdir=/usr/share/doc/check-0.15.2 install"],
        bins=["checkmk"], libs=["libcheck.so.0.15.2"], tsbu=2.0),
    Pkg("diffutils", "8.60", "Diffutils-3.10", 0.4, "diff and friends, for good.",
        ["$ ./configure --prefix=/usr", "$ make", "$ make check", "$ make install"],
        bins=["diff", "cmp", "diff3", "sdiff"]),
    Pkg("gawk", "8.61", "Gawk-5.3.0", 0.2, "GNU awk, for good.",
        ["$ sed -i 's/extras//' Makefile.in",
         "$ ./configure --prefix=/usr", "$ make",
         "$ chown -R tester .",
         "$ su tester -c \"PATH=$PATH make check\"",
         "Remove the old program first so make install can replace it:",
         "$ rm -f /usr/bin/gawk-5.3.0",
         "$ make install",
         "$ ln -sv gawk.1 /usr/share/man/man1/awk.1"],
        bins=["gawk", "gawk-5.3.0"], links={"{P}/bin/awk": "gawk"},
        files=["{P}/share/man/man1/gawk.1"],
        need=[("link", "/usr/share/man/man1/awk.1", "gawk.1")],
        srcfiles={"Makefile.in": "SUBDIRS = support . extras doc awklib po test\n"}),
    Pkg("findutils", "8.62", "Findutils-4.10.0", 0.7, "find, xargs and locate, for good.",
        ["$ ./configure --prefix=/usr --localstatedir=/var/lib/locate", "$ make",
         "$ chown -R tester .", "$ su tester -c \"PATH=$PATH make check\"", "$ make install"],
        bins=["find", "xargs", "locate", "updatedb"]),
    Pkg("groff", "8.63", "Groff-1.23.0", 0.2, "Formats text, for example man pages.",
        ["Tell groff the paper size (A4 or letter):",
         "$ PAGE=A4 ./configure --prefix=/usr", "$ make", "$ make check", "$ make install"],
        bins=["groff", "nroff", "troff", "grotty", "tbl", "eqn"]),
    Pkg("grub", "8.64", "GRUB-2.12", 0.3, "The boot loader (BIOS version).",
        ["Flags from the environment could break GRUB; clear them:",
         "$ unset {C,CPP,CXX,LD}FLAGS",
         "Add a file the release tarball forgot:",
         "$ echo depends bli part_gpt > grub-core/extra_deps.lst",
         "$ ./configure --prefix=/usr          \\\n            --sysconfdir=/etc      \\\n"
         "            --disable-efiemu       \\\n            --disable-werror",
         "$ make", "$ make install",
         "$ mv -v /etc/bash_completion.d/grub /usr/share/bash-completion/completions"],
        bins=["grub-mkrescue", "grub-mkimage", "grub-editenv"],
        sbins=["grub-install", "grub-mkconfig", "grub-probe"],
        files=["{P}/lib/grub/i386-pc/modinfo.sh", "/etc/bash_completion.d/grub"],
        need=[("file", "/usr/share/bash-completion/completions/grub")],
        srcfiles={"grub-core/README": "GRUB core\n"}),
    Pkg("gzip", "8.65", "Gzip-1.13", 0.3, "gzip, for good.",
        ["$ ./configure --prefix=/usr", "$ make", "$ make check", "$ make install"],
        bins=["gzip", "gunzip", "zcat", "gzexe", "zless"]),
    Pkg("iproute2", "8.66", "IPRoute2-6.10.0", 0.1, "Network configuration: the ip command.",
        ["arpd needs Berkeley DB, which LFS does not install:",
         "$ sed -i /ARPD/d Makefile",
         "$ rm -fv man/man8/arpd.8",
         "$ make NETNS_RUN_DIR=/run/netns",
         "$ make SBINDIR=/usr/sbin install"],
        sbins=["ip", "ss", "bridge", "tc", "rtmon"], style="make", prefix="/usr",
        need=[("file", "/usr/sbin/ip")],
        makefile="# iproute2 Makefile\nSUBDIRS=lib ip tc bridge misc\nARPD=arpd\n",
        srcfiles={"man/man8/arpd.8": ".TH ARPD 8\n"}),
    Pkg("kbd", "8.67", "Kbd-2.6.4", 0.1, "Keyboard maps and console fonts.",
        ["Make Backspace and Delete behave the same in all keymaps:",
         "$ patch -Np1 -i ../kbd-2.6.4-backspace-1.patch",
         "resizecons does not work on modern systems; do not build it:",
         "$ sed -i '/RESIZECONS_PROGS=/s/yes/no/' configure",
         "$ sed -i 's/resizecons.8 //' docs/man/man8/Makefile.in",
         "$ ./configure --prefix=/usr --disable-vlock", "$ make", "$ make check",
         "$ make install"],
        bins=["loadkeys", "setfont", "kbd_mode", "showkey", "dumpkeys"],
        files=["{P}/share/keymaps/i386/qwerty/us.map.gz", "{P}/share/consolefonts/Lat2-Terminus16.psfu.gz"],
        patch="backspace", srcfiles={"docs/man/man8/Makefile.in": "man_MANS = resizecons.8 setfont.8\n"}),
    Pkg("libpipeline", "8.68", "Libpipeline-1.5.7", 0.1, "Runs pipelines of programs (Man-DB uses it).",
        ["$ ./configure --prefix=/usr", "$ make", "$ make check", "$ make install"],
        libs=["libpipeline.so.1.5.7"]),
    Pkg("make", "8.69", "Make-4.4.1", 0.7, "make, for good.",
        ["$ ./configure --prefix=/usr", "$ make", "$ chown -R tester .",
         "$ su tester -c \"PATH=$PATH make check\"", "$ make install"],
        bins=["make"]),
    Pkg("patch", "8.70", "Patch-2.7.6", 0.2, "patch, for good.",
        ["$ ./configure --prefix=/usr", "$ make", "$ make check", "$ make install"],
        bins=["patch"]),
    Pkg("tar", "8.71", "Tar-1.35", 0.6, "tar, for good.",
        ["$ FORCE_UNSAFE_CONFIGURE=1  \\\n./configure --prefix=/usr", "$ make", "$ make check",
         "$ make install"],
        bins=["tar"], tsbu=1.0),
    Pkg("texinfo", "8.72", "Texinfo-7.1", 0.3, "Texinfo, for good.",
        ["$ ./configure --prefix=/usr", "$ make", "$ make check", "$ make install",
         "$ make TEXMF=/usr/share/texmf install-tex"],
        bins=["makeinfo", "texi2any", "info", "install-info", "texindex"]),
    Pkg("vim", "8.73", "Vim-9.1.0660", 3.4, "The Vim text editor.",
        ["Put the global configuration file in /etc:",
         "$ echo '#define SYS_VIMRC_FILE \"/etc/vimrc\"' >> src/feature.h",
         "$ ./configure --prefix=/usr", "$ make",
         "! The real book runs the long test suite as tester; skipped here.",
         "$ make install",
         "Many people type vi:",
         "$ ln -sv vim /usr/bin/vi",
         "$ ln -sv vim.1 /usr/share/man/man1/vi.1",
         "$ ln -sv ../vim/vim91/doc /usr/share/doc/vim-9.1.0660",
         "A minimal configuration:",
         "$ cat > /etc/vimrc << \"EOF\"\n\" Begin /etc/vimrc\n\n\" Ensure defaults are set before "
         "customizing settings, not after\nsource $VIMRUNTIME/defaults.vim\nlet "
         "skip_defaults_vim=1\n\nset nocompatible\nset backspace=2\nset mouse=\nsyntax on\n"
         "if (&term == \"xterm\") || (&term == \"putty\")\n  set background=dark\nendif\n\n"
         "\" End /etc/vimrc\nEOF"],
        bins=["vim", "xxd"], links={"{P}/bin/vimdiff": "vim", "{P}/bin/view": "vim"},
        files=["{P}/share/vim/vim91/doc/help.txt", "{P}/share/man/man1/vim.1"],
        need=[("link", "/usr/bin/vi", "vim"), ("contains", "/etc/vimrc", "set nocompatible")],
        srcfiles={"src/feature.h": "/* feature.h */\n"}),
    _pip("markupsafe", "8.74", "MarkupSafe-2.1.5", 0.1, "Safe strings for HTML/XML (Jinja2 needs it).",
         "Markupsafe", "setuptools", [f"{PY}/markupsafe/__init__.py"]),
    _pip("jinja2", "8.75", "Jinja2-3.1.4", 0.1, "A template engine (the udev build uses it).",
         "Jinja2", "flit_core", [f"{PY}/jinja2/__init__.py"]),
    Pkg("systemd", "8.76", "Udev from Systemd-256.4", 0.3,
        "Udev creates the device files in /dev and loads drivers when hardware appears. It "
        "lives in the systemd source tree; LFS (System V version) builds only udev.",
        ["Adjust the rules to groups that exist in LFS, and remove a systemd-only rule:",
         "$ sed -e 's/GROUP=\"render\"/GROUP=\"video\"/' \\\n    -e 's/GROUP=\"sgx\", //'     "
         "     \\\n    -i rules.d/50-udev-default.rules.in",
         "$ sed '/systemd-sysctl/s/^/#/' -i rules.d/99-systemd.rules.in",
         "$ sed '/NETWORK_DIRS/s/systemd/udev/' -i src/basic/path-lookup.h",
         "$ mkdir -p build", "$ cd build",
         "$ meson setup ..                  \\\n      --prefix=/usr             \\\n"
         "      --buildtype=release       \\\n      -D mode=release           \\\n"
         "      -D dev-kvm-mode=0660      \\\n      -D link-udev-shared=false \\\n"
         "      -D logind=false           \\\n      -D vconsole=false",
         "$ ninja udevadm systemd-hwdb",
         "! The real book builds only the udev programs and copies them, the rules and the "
         "hardware database files into place with a long list of install commands. The "
         "simulation simplifies that to one ninja install.",
         "$ ninja install",
         "Build the hardware database:",
         "$ udevadm hwdb --update"],
        bins=["udevadm"], libs=["libudev.so.1.7.9"],
        from_src={"rules.d/50-udev-default.rules.in": "{P}/lib/udev/rules.d/50-udev-default.rules"},
        need=[("contains", "/usr/lib/udev/rules.d/50-udev-default.rules", 'GROUP="video"'),
              ("file", "/etc/udev/hwdb.bin")],
        style="meson", prefix="/usr",
        srcfiles={"meson.build": "project('systemd', 'c')\n",
                  "rules.d/50-udev-default.rules.in":
                      'SUBSYSTEM=="drm", KERNEL=="renderD*", GROUP="render", MODE="0666"\n'
                      'SUBSYSTEM=="misc", KERNEL=="sgx_enclave", GROUP="sgx", MODE="0660"\n',
                  "rules.d/99-systemd.rules.in": 'ACTION=="add", RUN+="systemd-sysctl"\n',
                  "src/basic/path-lookup.h": '#define NETWORK_DIRS "/etc/systemd/network"\n'},
        topdir="systemd-256.4"),
    Pkg("man-db", "8.77", "Man-DB-2.12.1", 0.3, "The man command and its index.",
        ["$ ./configure --prefix=/usr                         \\\n"
         "            --docdir=/usr/share/doc/man-db-2.12.1 \\\n"
         "            --sysconfdir=/etc                     \\\n"
         "            --disable-setuid                      \\\n"
         "            --enable-cache-owner=bin              \\\n"
         "            --with-browser=/usr/bin/lynx          \\\n"
         "            --with-vgrind=/usr/bin/vgrind         \\\n"
         "            --with-grap=/usr/bin/grap             \\\n"
         "            --with-systemdtmpfilesdir=            \\\n"
         "            --with-systemdsystemunitdir=",
         "$ make", "$ make check", "$ make install"],
        bins=["man", "mandb", "apropos", "whatis", "manpath"]),
    Pkg("procps-ng", "8.78", "Procps-ng-4.0.4", 0.1, "ps, top, free and other process tools.",
        ["$ ./configure --prefix=/usr                           \\\n"
         "            --docdir=/usr/share/doc/procps-ng-4.0.4 \\\n"
         "            --disable-static                        \\\n"
         "            --disable-kill                          \\\n"
         "            --enable-watch8bit",
         "$ make", "$ chown -R tester .", "$ su tester -c \"PATH=$PATH make check\"",
         "$ make install"],
        bins=["ps", "top", "free", "uptime", "pgrep", "pkill", "pmap", "vmstat", "w", "watch"],
        sbins=["sysctl"], libs=["libproc2.so.0.0.2"]),
    Pkg("util-linux", "8.79", "Util-linux-2.40.2", 0.5, "System utilities, for good.",
        ["$ ./configure --bindir=/usr/bin     \\\n            --libdir=/usr/lib     \\\n"
         "            --runstatedir=/run    \\\n            --sbindir=/usr/sbin   \\\n"
         "            --disable-chfn-chsh   \\\n            --disable-login       \\\n"
         "            --disable-nologin     \\\n            --disable-su          \\\n"
         "            --disable-setpriv     \\\n            --disable-runuser     \\\n"
         "            --disable-pylibmount  \\\n            --disable-liblastlog2 \\\n"
         "            --disable-static      \\\n            --without-python      \\\n"
         "            --without-systemd     \\\n            --without-systemdsystemunitdir \\\n"
         "            ADJTIME_PATH=/var/lib/hwclock/adjtime \\\n"
         "            --docdir=/usr/share/doc/util-linux-2.40.2",
         "$ make",
         "! The real book runs the tests as tester; they can harm the host if run wrongly, "
         "so they are skipped here.",
         "$ make install"],
        bins=["mount", "umount", "lsblk", "findmnt", "mountpoint", "blkid", "dmesg", "more",
              "uuidgen", "lscpu", "kill", "logger"],
        sbins=["fdisk", "sfdisk", "mkswap", "swapon", "swapoff", "hwclock", "losetup", "agetty",
               "fsck", "blockdev"],
        libs=["libmount.so.1.1.0", "libblkid.so.1.1.0", "libuuid.so.1.3.0",
              "libfdisk.so.1.1.0", "libsmartcols.so.1.1.0"], prefix="/usr"),
    Pkg("e2fsprogs", "8.80", "E2fsprogs-1.47.1", 2.4, "Tools for the ext2/3/4 filesystems.",
        ["$ mkdir -v build", "$ cd build",
         "$ ../configure --prefix=/usr           \\\n             --sysconfdir=/etc       \\\n"
         "             --enable-elf-shlibs     \\\n             --disable-libblkid      \\\n"
         "             --disable-libuuid       \\\n             --disable-uuidd         \\\n"
         "             --disable-fsck",
         "$ make", "$ make check", "$ make install",
         "$ rm -fv /usr/lib/{libcom_err,libe2p,libext2fs,libss}.a"],
        bins=["chattr", "lsattr", "compile_et", "mk_cmds"],
        sbins=["mke2fs", "e2fsck", "tune2fs", "resize2fs", "debugfs", "dumpe2fs", "e2label"],
        links={"{P}/sbin/mkfs.ext4": "mke2fs", "{P}/sbin/fsck.ext4": "e2fsck"},
        libs=["libext2fs.so.2.4", "libcom_err.so.2.1", "libe2p.so.2.3", "libss.so.2.0",
              "libext2fs.a", "libcom_err.a", "libe2p.a", "libss.a"],
        files=["/etc/mke2fs.conf"], need=[("absent", "/usr/lib/libext2fs.a")],
        builddir=True, tsbu=1.0),
    Pkg("sysklogd", "8.81", "Sysklogd-2.6.1", 0.1, "The system log daemon.",
        ["$ ./configure --prefix=/usr      \\\n            --sysconfdir=/etc  \\\n"
         "            --runstatedir=/run \\\n            --without-logger   \\\n"
         "            --disable-static   \\\n            --docdir=/usr/share/doc/sysklogd-2.6.1",
         "$ make", "$ make install",
         "Where the messages go:",
         "$ cat > /etc/syslog.conf << \"EOF\"\n# Begin /etc/syslog.conf\n\n"
         "auth,authpriv.* -/var/log/auth.log\n*.*;auth,authpriv.none -/var/log/sys.log\n"
         "daemon.* -/var/log/daemon.log\nkern.* -/var/log/kern.log\nmail.* -/var/log/mail.log\n"
         "user.* -/var/log/user.log\n*.emerg *\n\n# Do not open any internet ports.\n"
         "secure_mode 2\n\n# End /etc/syslog.conf\nEOF"],
        sbins=["syslogd"], need=[("contains", "/etc/syslog.conf", "auth,authpriv.*")]),
    Pkg("sysvinit", "8.82", "SysVinit-3.10", 0.1,
        "init, the first process; it starts and stops the system.",
        ["$ patch -Np1 -i ../sysvinit-3.10-consolidated-1.patch", "$ make", "$ make install"],
        sbins=["init", "halt", "shutdown", "runlevel", "killall5", "bootlogd", "fstab-decode"],
        links={"{P}/sbin/telinit": "init", "{P}/sbin/reboot": "halt", "{P}/sbin/poweroff": "halt"},
        bins=["last", "mesg"], style="make", prefix="/usr", patch="consolidated"),
]

BY_KEY: Dict[str, Pkg] = {p.key: p for p in PKGS}
CH7_OBJECTIVE = {"gettext": "gettext7", "bison": "bison7", "perl": "perl7", "python": "python7",
                 "texinfo": "texinfo7", "util-linux": "utillinux7"}


def is_ch8(m, component: str) -> bool:
    """Is a build of this package (now, in this state) the chapter-8 one?"""
    if not m.croot or component not in BY_KEY:
        return False
    obj = CH7_OBJECTIVE.get(component)
    return obj is None or m.has_flag(f"obj:{obj}")


def install_paths(pkg: Pkg, opts: Dict[str, str], make_vars: Dict[str, str]):
    """(prefix, bindir, sbindir, libdir) for an install, like autoconf/Makefiles decide."""
    P = (make_vars.get("PREFIX") or make_vars.get("prefix") or opts.get("--prefix")
         or opts.get("prefix") or pkg.prefix).rstrip("/") or "/"
    ep = make_vars.get("exec_prefix", P).rstrip("/")
    bindir = opts.get("--bindir") or make_vars.get("BINDIR") or f"{ep}/bin"
    sbindir = opts.get("--sbindir") or make_vars.get("SBINDIR") or f"{ep}/sbin"
    libdir = opts.get("--libdir") or make_vars.get("LIBDIR") or f"{P}/lib"
    if not libdir.startswith("/"):
        libdir = f"{P}/{libdir}"
    return P, bindir, sbindir, libdir


def book_text() -> str:
    """The chapter 8 book file (see tools/gen_ch8_book.py)."""
    from . import packages
    out = ["@chapter 8. Installing Basic System Software"]
    out.append(INTRO)
    for p in PKGS:
        out.append(f"@section {p.sec} | {p.title}\n")
        out.append(p.about + f" Build time: {p.sbu + p.tsbu:g} SBU.\n")
        if p.frame:
            out.append("$ cd /sources")
            out.append(f"$ tar -xf {pkg_file(p)}")
            out.append(f"$ cd {top_dir(p)}\n")
        for step in p.steps:
            out.append(_book_item(step))
        if p.frame:
            out.append("\n$ cd /sources")
            out.append(f"$ rm -rf {top_dir(p)}\n")
        if p.tests and "fail" in p.tests.lower():
            out.append("! The test suite reports some failures; the real book lists which "
                       "ones are expected.\n")
    out.append(OUTRO)
    del packages
    return "\n".join(out).rstrip() + "\n"


def _book_item(step: str) -> str:
    if step.startswith("?$ "):
        step = step[1:]
    if step.startswith("$ "):
        first, *rest = step.split("\n")
        return "\n".join([first] + [f"+ {r}" if r else "+" for r in rest])
    if step.startswith(("## ", "! ")):
        return "\n" + step + "\n"
    return "\n" + step + "\n"


def pkg_file(p: Pkg) -> str:
    from . import packages
    for pk in packages.PACKAGES:
        if pk.name == p.key:
            return pk.file
    raise KeyError(p.key)


def top_dir(p: Pkg) -> str:
    import re
    return p.topdir or re.sub(r"\.tar\.(xz|gz|bz2)$", "", pkg_file(p))


def walkthrough() -> List[Tuple[str, bool]]:
    """All chapter 8 commands in book order: (command, must_succeed)."""
    out: List[Tuple[str, bool]] = [(c, True) for c in INTRO_CMDS]
    for p in PKGS:
        if p.frame:
            out += [("cd /sources", True), (f"tar -xf {pkg_file(p)}", True),
                    (f"cd {top_dir(p)}", True)]
        for step in p.steps:
            if step.startswith("$ "):
                out.append((step[2:], True))
            elif step.startswith("?$ "):
                out.append((step[3:], False))
        if p.frame:
            out += [("cd /sources", True), (f"rm -rf {top_dir(p)}", True)]
    out += [(c, ok) for c, ok in OUTRO_CMDS]
    return out


INTRO_CMDS: List[str] = []

INTRO = """@section 8.1 | Introduction

Now the real system is built, inside the chroot, package by package. Every package is installed for good into /usr, replacing the temporary tools of chapters 6 and 7. Most of them follow the familiar pattern:

- ./configure --prefix=/usr (plus options)
- make
- make check (the test suite)
- make install

From here on there is no --host and no DESTDIR: the compiler is the native one built in chapter 6, and everything is installed directly into the running chroot.

! The order matters: many packages depend on the ones before them. Follow the sections in order.

## Test suites
Running the tests tells you whether the build works. For Glibc, Binutils, GCC, GMP and MPFR they are critical: never skip them. Some packages must not be tested as root; the book then gives the build tree to the user tester (created in 7.6) and runs the tests with su tester -c "...".

## Missing archives
The game's package list grew for chapter 8. If you started an earlier version of the game, some archives may be missing in /sources. The download objective of chapter 3 was already done, so nothing tells you: check with ls /sources. To fetch the rest, leave the chroot (exit), and as root on the host (LFS mounted):

> wget https://www.linuxfromscratch.org/lfs/view/stable/wget-list-sysv
> wget --input-file=wget-list-sysv --continue --directory-prefix=$LFS/sources

Then enter the chroot again (7.3, 7.4). The commands in this chapter follow LFS 12.2 as closely as the game author remembers them; where a real command cannot work in this simulated shell it is simplified, and a note says so. For a real build always use the real book.

@section 8.2 | Package Management

LFS does not use a package manager: you install by hand, and nothing records which file belongs to which package. The real book discusses the ways people track that (installing into separate directories with symlinks, timestamps, LD_PRELOAD tricks, DESTDIR packaging). For upgrades the general rule is: when Glibc, GCC or Binutils change in a big way, rebuilding everything is safest.
"""

OUTRO_CMDS: List[Tuple[str, bool]] = [
    ("strip --strip-unneeded /usr/lib/lib{z,bz2,lzma,zstd,lz4}.so.*", True),
    ("strip --strip-all /usr/bin/{bzip2,xz,zstd,lz4,file,m4}", True),
    ("rm -rf /tmp/{*,.*}", True),
    ("find /usr/lib /usr/libexec -name \\*.la -delete", True),
    ("find /usr -depth -name $(uname -m)-lfs-linux-gnu\\* | xargs rm -rf", True),
    ("userdel -r tester", True),
]

OUTRO = """
@section 8.83 | About Debugging Symbols

Most programs and libraries are compiled with debugging symbols: extra information that lets a debugger show function names and line numbers. They make the files much bigger (Bash: about 1.2 MB with them, 0.5 MB without). A normal user does not need them.

@section 8.84 | Stripping

Removing the debugging symbols ("stripping") saves a lot of space. It is optional, and it is easy to break the system by stripping the wrong file, for example a library that the running shell uses. The real book uses a careful script: it keeps the debug information of a few libraries in separate files (objcopy --only-keep-debug), strips libraries with --strip-unneeded and programs with --strip-all, and replaces the files instead of editing them in place.

This simplified version strips only some compression libraries and programs:

$ strip --strip-unneeded /usr/lib/lib{z,bz2,lzma,zstd,lz4}.so.*
$ strip --strip-all /usr/bin/{bzip2,xz,zstd,lz4,file,m4}

@section 8.85 | Cleaning Up

Remove what the tests and builds left behind in /tmp:

$ rm -rf /tmp/{*,.*}

The libtool archives would cause trouble for packages built later:

$ find /usr/lib /usr/libexec -name \\*.la -delete

The rest of the cross-compiler from chapters 5 and 6 is no longer needed. find prints the paths and xargs passes them to rm:

$ find /usr -depth -name $(uname -m)-lfs-linux-gnu\\* | xargs rm -rf

The tester user has done its job:

$ userdel -r tester

@chapter 9. Coming Next
@section 9.0 | To Be Continued

All the software of the new system is installed. In the real book chapter 9 configures it (boot scripts, network, /etc/inittab, the console, locale settings), chapter 10 builds the Linux kernel, writes /etc/fstab and installs GRUB, and chapter 11 is the first boot.

That is planned for a later version of the game. Save now!

$ tasks
$ df -h /
"""

BY_KEY["meson"].srcfiles.update({"data/shell-completions/bash/meson": "# meson completion\n",
                                 "data/shell-completions/zsh/_meson": "#compdef meson\n"})

# contents of some installed text files (the rest are empty "binary" files)
CONTENT = {
    "chroot.1": '.TH CHROOT "1" "August 2024" "GNU coreutils 9.5" "User Commands"\n',
    "ls.1": '.TH LS "1" "August 2024" "GNU coreutils 9.5" "User Commands"\n',
    "mke2fs.conf": "[defaults]\n\tbase_features = sparse_super,large_file,filetype,"
                   "resize_inode,dir_index,ext_attr\n",
    "openssl.cnf": "# OpenSSL configuration\nopenssl_conf = openssl_init\n",
}

WALK_INPUTS = {"tzselect": ["7", "2", "2", "1"], "passwd root": ["lfs", "lfs"]}

TZ_REGIONS = {
    "europe": ["Europe/Moscow", "Europe/Kaliningrad", "Europe/Berlin", "Europe/London",
               "Europe/Paris", "Europe/Kyiv", "Europe/Minsk"],
    "asia": ["Asia/Tokyo", "Asia/Shanghai", "Asia/Kolkata", "Asia/Yekaterinburg",
             "Asia/Novosibirsk", "Asia/Vladivostok"],
    "northamerica": ["America/New_York", "America/Chicago", "America/Denver",
                     "America/Los_Angeles"],
    "southamerica": ["America/Sao_Paulo", "America/Argentina/Buenos_Aires"],
    "africa": ["Africa/Cairo", "Africa/Johannesburg", "Africa/Lagos"],
    "antarctica": ["Antarctica/Troll", "Antarctica/McMurdo"],
    "australasia": ["Australia/Sydney", "Pacific/Auckland"],
    "etcetera": ["Etc/UTC", "Etc/GMT", "UTC", "GMT"],
    "backward": ["US/Eastern", "W-SU", "Japan"],
}
TZ_FILES = list(TZ_REGIONS) + ["leap-seconds", "zone.tab", "zone1970.tab", "iso3166.tab"]
