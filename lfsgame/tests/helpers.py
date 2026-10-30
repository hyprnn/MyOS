"""Shared test helpers: a fake terminal ctx (no tkinter)."""
from lfsgame.commands import run_line
from lfsgame.sim.game import Game
from lfsgame.sim.machine import Machine


class FakeCtx:
    def __init__(self, seed=1):
        self.game = Game(Machine.new(seed))
        self.out = []
        self.cleared = self.menu = False
        self.saved = 0
        self.books = []
        self.history = []

    @property
    def machine(self):
        return self.game.machine

    def write(self, text, tag=None):
        self.out.append((text, tag))

    def clear(self):
        self.cleared = True

    def goto_menu(self):
        self.menu = True

    def save_game(self):
        self.saved += 1

    def open_book(self, section=None):
        self.books.append(section)
        return section in (None, "toc", "0", "2.4", "2.5", "2.6", "2.7", "3.1", "4.2", "4.3")

    def text(self):
        return "".join(t for t, _ in self.out)

    def errors(self):
        return "".join(t for t, g in self.out if g == "err")

    def run(self, line, inputs=None):
        """Run a line, return its exit status; output collected since the last run()."""
        self.out = []
        status = run_line(self, line, inputs)
        self.game.check()
        return status


FULL_WALKTHROUGH = [
    ("lsblk", None),
    ("fdisk /dev/sdb", ["g", "n", "", "", "+20G", "n", "", "", "+2G", "t", "2", "swap", "w"]),
    ("mkfs -v -t ext4 /dev/sdb1", None),
    ("mkswap /dev/sdb2", None),
    ("swapon -v /dev/sdb2", None),
    ("export LFS=/mnt/lfs", None),
    ("mkdir -pv $LFS", None),
    ("mount -v -t ext4 /dev/sdb1 $LFS", None),
    ("mkdir -v $LFS/sources", None),
    ("chmod -v a+wt $LFS/sources", None),
    ("wget https://www.linuxfromscratch.org/lfs/view/stable/wget-list-sysv", None),
    ("wget --input-file=wget-list-sysv --continue --directory-prefix=$LFS/sources", None),
    ("wget --input-file=wget-list-sysv --continue --directory-prefix=$LFS/sources", None),
    ("wget https://www.linuxfromscratch.org/lfs/view/stable/md5sums --directory-prefix=$LFS/sources", None),
    ("pushd $LFS/sources && md5sum -c md5sums; popd", None),
    ("mkdir -pv $LFS/{etc,var} $LFS/usr/{bin,lib,sbin}", None),
    ("ln -sv usr/bin $LFS/bin", None),
    ("ln -sv usr/lib $LFS/lib", None),
    ("ln -sv usr/sbin $LFS/sbin", None),
    ("mkdir -pv $LFS/lib64 $LFS/tools", None),
    ("groupadd lfs", None),
    ("useradd -s /bin/bash -g lfs -m -k /dev/null lfs", None),
    ("chown -v lfs $LFS/{usr{,/*},lib,var,etc,bin,sbin,tools}", None),
    ("chown -v lfs $LFS/lib64", None),
    ("su - lfs", None),
]


BASH_PROFILE = '''cat > ~/.bash_profile << "EOF"
exec env -i HOME=$HOME TERM=$TERM PS1='\\u:\\w\\$ ' /bin/bash
EOF'''

BASHRC = '''cat > ~/.bashrc << "EOF"
set +h
umask 022
LFS=/mnt/lfs
LC_ALL=POSIX
LFS_TGT=$(uname -m)-lfs-linux-gnu
PATH=/usr/bin
if [ ! -L /bin ]; then PATH=/bin:$PATH; fi
PATH=$LFS/tools/bin:$PATH
CONFIG_SITE=$LFS/usr/share/config.site
export LFS LC_ALL LFS_TGT PATH CONFIG_SITE
EOF'''

MAKEFLAGS = '''cat >> ~/.bashrc << "EOF"
export MAKEFLAGS=-j$(nproc)
EOF'''

CH4_4 = [
    ("exit", None),
    ("[ ! -e /etc/bash.bashrc ] || mv -v /etc/bash.bashrc /etc/bash.bashrc.NOUSE", None),
    ("su - lfs", None),
    (BASH_PROFILE, None),
    (BASHRC, None),
    (MAKEFLAGS, None),
    ("source ~/.bash_profile", None),
]

CH5 = [
    ("cd $LFS/sources", None),
    ("tar -xf binutils-2.43.1.tar.xz", None),
    ("cd binutils-2.43.1", None),
    ("mkdir -v build", None),
    ("cd build", None),
    ("../configure --prefix=$LFS/tools --with-sysroot=$LFS --target=$LFS_TGT --disable-nls "
     "--enable-gprofng=no --disable-werror --enable-new-dtags --enable-default-hash-style=gnu",
     None),
    ("make", None),
    ("make install", None),
    ("cd $LFS/sources && rm -rf binutils-2.43.1", None),
    ("tar -xf gcc-14.2.0.tar.xz", None),
    ("cd gcc-14.2.0", None),
    ("tar -xf ../mpfr-4.2.1.tar.xz && mv -v mpfr-4.2.1 mpfr", None),
    ("tar -xf ../gmp-6.3.0.tar.xz && mv -v gmp-6.3.0 gmp", None),
    ("tar -xf ../mpc-1.3.1.tar.gz && mv -v mpc-1.3.1 mpc", None),
    ("sed -e '/m64=/s/lib64/lib/' -i.orig gcc/config/i386/t-linux64", None),
    ("mkdir -v build && cd build", None),
    ("../configure --target=$LFS_TGT --prefix=$LFS/tools --with-glibc-version=2.40 "
     "--with-sysroot=$LFS --with-newlib --without-headers --enable-default-pie "
     "--enable-default-ssp --disable-nls --disable-shared --disable-multilib --disable-threads "
     "--disable-libatomic --disable-libgomp --disable-libquadmath --disable-libssp "
     "--disable-libvtv --disable-libstdcxx --enable-languages=c,c++", None),
    ("make && make install", None),
    ("cd ..", None),
    ("cat gcc/limitx.h gcc/glimits.h gcc/limity.h > \\\n"
     "  `dirname $($LFS_TGT-gcc -print-libgcc-file-name)`/include/limits.h", None),
    ("cd $LFS/sources && rm -rf gcc-14.2.0", None),
    ("tar -xf linux-6.10.5.tar.xz && cd linux-6.10.5", None),
    ("make mrproper", None),
    ("make headers", None),
    ("find usr/include -type f ! -name '*.h' -delete", None),
    ("cp -rv usr/include $LFS/usr", None),
    ("cd $LFS/sources && rm -rf linux-6.10.5", None),
    ("ln -sfv ../lib/ld-linux-x86-64.so.2 $LFS/lib64", None),
    ("ln -sfv ../lib/ld-linux-x86-64.so.2 $LFS/lib64/ld-lsb-x86-64.so.3", None),
    ("tar -xf glibc-2.40.tar.xz && cd glibc-2.40", None),
    ("patch -Np1 -i ../glibc-2.40-fhs-1.patch", None),
    ("mkdir -v build && cd build", None),
    ('echo "rootsbindir=/usr/sbin" > configparms', None),
    ("../configure --prefix=/usr --host=$LFS_TGT --build=$(../scripts/config.guess) "
     "--enable-kernel=4.19 --with-headers=$LFS/usr/include --disable-nscd "
     "libc_cv_slibdir=/usr/lib", None),
    ("make", None),
    ("make DESTDIR=$LFS install", None),
    ("sed '/RTLDLIST=/s@/usr@@g' -i $LFS/usr/bin/ldd", None),
    ("echo 'int main(){}' | $LFS_TGT-gcc -xc -", None),
    ("readelf -l a.out | grep ld-linux", None),
    ("rm -v a.out", None),
    ("cd $LFS/sources && rm -rf glibc-2.40", None),
    ("tar -xf gcc-14.2.0.tar.xz && cd gcc-14.2.0", None),
    ("mkdir -v build && cd build", None),
    ("../libstdc++-v3/configure --host=$LFS_TGT --build=$(../config.guess) --prefix=/usr "
     "--disable-multilib --disable-nls --disable-libstdcxx-pch "
     "--with-gxx-include-dir=/tools/$LFS_TGT/include/c++/14.2.0", None),
    ("make", None),
    ("make DESTDIR=$LFS install", None),
    ("rm -v $LFS/usr/lib/lib{stdc++{,exp,fs},supc++}.la", None),
]


def _std6(pkg, tarball, conf, extra=()):
    """The usual chapter 6 sequence for one package."""
    top = tarball.rsplit(".tar", 1)[0]
    return ([("cd $LFS/sources", None), (f"tar -xf {tarball}", None), (f"cd {top}", None),
             (conf, None), ("make", None), ("make DESTDIR=$LFS install", None)]
            + [(e, None) for e in extra]
            + [("cd $LFS/sources", None), (f"rm -rf {top}", None)])


CH6 = (
    _std6("m4", "m4-1.4.19.tar.xz",
          "./configure --prefix=/usr --host=$LFS_TGT --build=$(build-aux/config.guess)")
    + [("cd $LFS/sources", None), ("tar -xf ncurses-6.5.tar.gz", None), ("cd ncurses-6.5", None),
       ("sed -i s/mawk// configure", None),
       ("mkdir build\npushd build\n  ../configure\n  make -C include\n  make -C progs tic\npopd",
        None),
       ("./configure --prefix=/usr --host=$LFS_TGT --build=$(./config.guess) "
        "--mandir=/usr/share/man --with-manpage-format=normal --with-shared --without-normal "
        "--with-cxx-shared --without-debug --without-ada --disable-stripping", None),
       ("make", None),
       ("make DESTDIR=$LFS TIC_PATH=$(pwd)/build/progs/tic install", None),
       ("ln -sv libncursesw.so $LFS/usr/lib/libncurses.so", None),
       ("sed -e 's/^#if.*XOPEN.*$/#if 1/' -i $LFS/usr/include/curses.h", None),
       ("cd $LFS/sources && rm -rf ncurses-6.5", None)]
    + _std6("bash", "bash-5.2.32.tar.gz",
            "./configure --prefix=/usr --build=$(sh support/config.guess) --host=$LFS_TGT "
            "--without-bash-malloc", ["ln -sv bash $LFS/bin/sh"])
    + _std6("coreutils", "coreutils-9.5.tar.xz",
            "./configure --prefix=/usr --host=$LFS_TGT --build=$(build-aux/config.guess) "
            "--enable-install-program=hostname --enable-no-install-program=kill,uptime",
            ["mv -v $LFS/usr/bin/chroot $LFS/usr/sbin",
             "mkdir -pv $LFS/usr/share/man/man8",
             "mv -v $LFS/usr/share/man/man1/chroot.1 $LFS/usr/share/man/man8/chroot.8",
             "sed -i 's/\"1\"/\"8\"/' $LFS/usr/share/man/man8/chroot.8"])
    + _std6("diffutils", "diffutils-3.10.tar.xz",
            "./configure --prefix=/usr --host=$LFS_TGT --build=$(./build-aux/config.guess)")
    + [("cd $LFS/sources", None), ("tar -xf file-5.45.tar.gz", None), ("cd file-5.45", None),
       ("mkdir build\npushd build\n  ../configure --disable-bzlib --disable-libseccomp "
        "--disable-xzlib --disable-zlib\n  make\npopd", None),
       ("./configure --prefix=/usr --host=$LFS_TGT --build=$(./config.guess)", None),
       ("make FILE_COMPILE=$(pwd)/build/src/file", None),
       ("make DESTDIR=$LFS install", None),
       ("rm -v $LFS/usr/lib/libmagic.la", None),
       ("cd $LFS/sources && rm -rf file-5.45", None)]
    + _std6("findutils", "findutils-4.10.0.tar.xz",
            "./configure --prefix=/usr --localstatedir=/var/lib/locate --host=$LFS_TGT "
            "--build=$(build-aux/config.guess)")
    + [("cd $LFS/sources && tar -xf gawk-5.3.0.tar.xz && cd gawk-5.3.0", None),
       ("sed -i 's/extras//' Makefile.in", None)]
    + _std6("gawk", "gawk-5.3.0.tar.xz",
            "./configure --prefix=/usr --host=$LFS_TGT --build=$(build-aux/config.guess)")[3:]
    + _std6("grep", "grep-3.11.tar.xz",
            "./configure --prefix=/usr --host=$LFS_TGT --build=$(./build-aux/config.guess)")
    + _std6("gzip", "gzip-1.13.tar.xz", "./configure --prefix=/usr --host=$LFS_TGT")
    + _std6("make", "make-4.4.1.tar.gz",
            "./configure --prefix=/usr --without-guile --host=$LFS_TGT "
            "--build=$(build-aux/config.guess)")
    + _std6("patch", "patch-2.7.6.tar.xz",
            "./configure --prefix=/usr --host=$LFS_TGT --build=$(build-aux/config.guess)")
    + _std6("sed", "sed-4.9.tar.xz",
            "./configure --prefix=/usr --host=$LFS_TGT --build=$(./build-aux/config.guess)")
    + _std6("tar", "tar-1.35.tar.xz",
            "./configure --prefix=/usr --host=$LFS_TGT --build=$(build-aux/config.guess)")
    + _std6("xz", "xz-5.6.2.tar.xz",
            "./configure --prefix=/usr --host=$LFS_TGT --build=$(build-aux/config.guess) "
            "--disable-static --docdir=/usr/share/doc/xz-5.6.2",
            ["rm -v $LFS/usr/lib/liblzma.la"])
    + [("cd $LFS/sources && tar -xf binutils-2.43.1.tar.xz && cd binutils-2.43.1", None),
       ("sed '6009s/$add_dir//' -i ltmain.sh", None),
       ("mkdir -v build && cd build", None),
       ("../configure --prefix=/usr --build=$(../config.guess) --host=$LFS_TGT --disable-nls "
        "--enable-shared --enable-gprofng=no --disable-werror --enable-64-bit-bfd "
        "--enable-new-dtags --enable-default-hash-style=gnu", None),
       ("make", None),
       ("make DESTDIR=$LFS install", None),
       ("rm -v $LFS/usr/lib/lib{bfd,ctf,ctf-nobfd,opcodes,sframe}.{a,la}", None),
       ("cd $LFS/sources && rm -rf binutils-2.43.1", None),
       ("tar -xf gcc-14.2.0.tar.xz && cd gcc-14.2.0", None),
       ("tar -xf ../mpfr-4.2.1.tar.xz && mv -v mpfr-4.2.1 mpfr", None),
       ("tar -xf ../gmp-6.3.0.tar.xz && mv -v gmp-6.3.0 gmp", None),
       ("tar -xf ../mpc-1.3.1.tar.gz && mv -v mpc-1.3.1 mpc", None),
       ("sed -e '/m64=/s/lib64/lib/' -i.orig gcc/config/i386/t-linux64", None),
       ("sed '/thread_header =/s/@.*@/gthr-posix.h/' -i libgcc/Makefile.in "
        "libstdc++-v3/include/Makefile.in", None),
       ("mkdir -v build && cd build", None),
       ("../configure --build=$(../config.guess) --host=$LFS_TGT --target=$LFS_TGT "
        "LDFLAGS_FOR_TARGET=-L$PWD/$LFS_TGT/libgcc --prefix=/usr --with-build-sysroot=$LFS "
        "--enable-default-pie --enable-default-ssp --disable-nls --disable-multilib "
        "--disable-libatomic --disable-libgomp --disable-libquadmath --disable-libsanitizer "
        "--disable-libssp --disable-libvtv --enable-languages=c,c++", None),
       ("make", None),
       ("make DESTDIR=$LFS install", None),
       ("ln -sv gcc $LFS/usr/bin/cc", None),
       ("cd $LFS/sources && rm -rf gcc-14.2.0", None)]
)


def _std7(tarball, steps):
    top = tarball.rsplit(".tar", 1)[0]
    return ([("cd /sources", None), (f"tar -xf {tarball}", None), (f"cd {top}", None)]
            + [(s, None) for s in steps]
            + [("cd /sources", None), (f"rm -rf {top}", None)])


CHROOT_CMD = ('chroot "$LFS" /usr/bin/env -i   \\\n'
              '    HOME=/root                  \\\n'
              '    TERM="$TERM"                \\\n'
              "    PS1='(lfs chroot) \\u:\\w\\$ ' \\\n"
              '    PATH=/usr/bin:/usr/sbin     \\\n'
              '    MAKEFLAGS="-j$(nproc)"      \\\n'
              '    TESTSUITEFLAGS="-j$(nproc)" \\\n'
              '    /bin/bash --login')

CH7_HOST = [
    ("exit", None),   # back to root
    ("chown --from lfs -R root:root $LFS/{usr,lib,var,etc,bin,sbin,tools}", None),
    ("chown --from lfs -R root:root $LFS/lib64", None),
    ("mkdir -pv $LFS/{dev,proc,sys,run}", None),
    ("mount -v --bind /dev $LFS/dev", None),
    ("mount -vt devpts devpts -o gid=5,mode=0620 $LFS/dev/pts", None),
    ("mount -vt proc proc $LFS/proc", None),
    ("mount -vt sysfs sysfs $LFS/sys", None),
    ("mount -vt tmpfs tmpfs $LFS/run", None),
    ("if [ -h $LFS/dev/shm ]; then\n  install -v -d -m 1777 $LFS$(realpath /dev/shm)\nelse\n"
     "  mount -vt tmpfs -o nosuid,nodev tmpfs $LFS/dev/shm\nfi", None),
    (CHROOT_CMD, None),
]

CH7_DIRS = [
    "mkdir -pv /{boot,home,mnt,opt,srv}",
    "mkdir -pv /etc/{opt,sysconfig}",
    "mkdir -pv /lib/firmware",
    "mkdir -pv /media/{floppy,cdrom}",
    "mkdir -pv /usr/{,local/}{include,src}",
    "mkdir -pv /usr/lib/locale",
    "mkdir -pv /usr/local/{bin,lib,sbin}",
    "mkdir -pv /usr/{,local/}share/{color,dict,doc,info,locale,man}",
    "mkdir -pv /usr/{,local/}share/{misc,terminfo,zoneinfo}",
    "mkdir -pv /usr/{,local/}share/man/man{1..8}",
    "mkdir -pv /var/{cache,local,log,mail,opt,spool}",
    "mkdir -pv /var/lib/{color,misc,locate}",
    "ln -sfv /run /var/run",
    "ln -sfv /run/lock /var/lock",
    "install -dv -m 0750 /root",
    "install -dv -m 1777 /tmp /var/tmp",
]

PASSWD = '''cat > /etc/passwd << "EOF"
root:x:0:0:root:/root:/bin/bash
bin:x:1:1:bin:/dev/null:/usr/bin/false
daemon:x:6:6:Daemon User:/dev/null:/usr/bin/false
messagebus:x:18:18:D-Bus Message Daemon User:/run/dbus:/usr/bin/false
uuidd:x:80:80:UUID Generation Daemon User:/dev/null:/usr/bin/false
nobody:x:65534:65534:Unprivileged User:/dev/null:/usr/bin/false
EOF'''

GROUP = '''cat > /etc/group << "EOF"
root:x:0:
bin:x:1:daemon
sys:x:2:
kmem:x:3:
tape:x:4:
tty:x:5:
daemon:x:6:
floppy:x:7:
disk:x:8:
lp:x:9:
dialout:x:10:
audio:x:11:
video:x:12:
utmp:x:13:
cdrom:x:15:
adm:x:16:
messagebus:x:18:
input:x:24:
mail:x:34:
kvm:x:61:
uuidd:x:80:
wheel:x:97:
users:x:999:
nogroup:x:65534:
EOF'''

CH7_FILES = [
    "ln -sv /proc/self/mounts /etc/mtab",
    "cat > /etc/hosts << EOF\n127.0.0.1  localhost $(hostname)\n::1        localhost\nEOF",
    PASSWD,
    GROUP,
    'echo "tester:x:101:101::/home/tester:/bin/bash" >> /etc/passwd',
    'echo "tester:x:101:" >> /etc/group',
    "install -o tester -d /home/tester",
    "exec /usr/bin/bash --login",
    "touch /var/log/{btmp,lastlog,faillog,wtmp}",
    "chgrp -v utmp /var/log/lastlog",
    "chmod -v 664  /var/log/lastlog",
    "chmod -v 600  /var/log/btmp",
]

CH7 = (
    CH7_HOST
    + [(c, None) for c in CH7_DIRS + CH7_FILES]
    + _std7("gettext-0.22.5.tar.xz", [
        "./configure --disable-shared", "make",
        "cp -v gettext-tools/src/{msgfmt,msgmerge,xgettext} /usr/bin"])
    + _std7("bison-3.8.2.tar.xz", [
        "./configure --prefix=/usr --docdir=/usr/share/doc/bison-3.8.2", "make", "make install"])
    + _std7("perl-5.40.0.tar.xz", [
        "sh Configure -des -D prefix=/usr -D vendorprefix=/usr -D useshrplib "
        "-D privlib=/usr/lib/perl5/5.40/core_perl -D archlib=/usr/lib/perl5/5.40/core_perl "
        "-D sitelib=/usr/lib/perl5/5.40/site_perl -D sitearch=/usr/lib/perl5/5.40/site_perl "
        "-D vendorlib=/usr/lib/perl5/5.40/vendor_perl "
        "-D vendorarch=/usr/lib/perl5/5.40/vendor_perl", "make", "make install"])
    + _std7("Python-3.12.5.tar.xz", [
        "./configure --prefix=/usr --enable-shared --without-ensurepip", "make", "make install"])
    + _std7("texinfo-7.1.tar.xz", ["./configure --prefix=/usr", "make", "make install"])
    + [("mkdir -pv /var/lib/hwclock", None)]
    + _std7("util-linux-2.40.2.tar.xz", [
        "./configure --libdir=/usr/lib --runstatedir=/run --disable-chfn-chsh --disable-login "
        "--disable-nologin --disable-su --disable-setpriv --disable-runuser "
        "--disable-pylibmount --disable-static --disable-liblastlog2 --without-python "
        "ADJTIME_PATH=/var/lib/hwclock/adjtime --docdir=/usr/share/doc/util-linux-2.40.2",
        "make", "make install"])
    + [("rm -rf /usr/share/{info,man,doc}/*", None),
       ("find /usr/{lib,libexec} -name \\*.la -delete", None),
       ("rm -rf /tools", None),
       ("exit", None),
       ("mountpoint -q $LFS/dev/shm && umount $LFS/dev/shm", None),
       ("umount $LFS/dev/pts", None),
       ("umount $LFS/{sys,proc,run,dev}", None),
       ("cd $LFS", None),
       ("tar -cJpf $HOME/lfs-temp-tools-12.2.tar.xz .", None)]
)


def ch8_walkthrough():
    """Re-mount, enter the chroot again, then every chapter 8 command:
    [(line, inputs, must_succeed)]."""
    from lfsgame.sim import ch8
    out = [(line, inputs, True) for line, inputs in CH7_HOST[3:]]
    out += [(line, ch8.WALK_INPUTS.get(line), must) for line, must in ch8.walkthrough()]
    return out
