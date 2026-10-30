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
