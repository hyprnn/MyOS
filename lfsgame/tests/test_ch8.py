"""Chapter 8 tests: the final system built inside the chroot."""
import unittest

from lfsgame.sim import ch8
from lfsgame.sim.game import Game
from lfsgame.sim.objectives import BY_ID
from tests.helpers import (CH4_4, CH5, CH6, CH7, CH7_HOST, FULL_WALKTHROUGH, FakeCtx,
                           ch8_walkthrough)

_BASE = None
_STEPS = ch8_walkthrough()


def fresh():
    """A copy of a game that just re-entered the chroot after chapter 7."""
    global _BASE
    if _BASE is None:
        c = FakeCtx(seed=21)
        for line, inputs in FULL_WALKTHROUGH + CH4_4 + CH5 + CH6 + CH7:
            c.run(line, inputs)
        _BASE = c.game.to_dict()
    c = FakeCtx()
    c.game = Game.from_dict(_BASE)
    c.step = 0
    for line, inputs, _ in _STEPS[:len(CH7_HOST) - 3]:  # mount again, enter the chroot
        assert c.run(line, inputs) == 0, (line, c.text())
        c.step += 1
    return c


def run_until(c, marker, include=False):
    """Run walkthrough commands until (and without) the first one containing marker."""
    while c.step < len(_STEPS):
        line, inputs, _ = _STEPS[c.step]
        if marker in line and not include:
            return
        c.run(line, inputs)
        c.step += 1
        if marker in line:
            return
    raise AssertionError(f"marker not found: {marker}")


class Ch8Tests(unittest.TestCase):
    def test_book_order_and_count(self):
        secs = [float(p.sec.split(".")[1]) for p in ch8.PKGS]
        self.assertEqual(secs, sorted(secs))
        self.assertEqual(len([o for o in BY_ID if o.endswith("8")]), 82)  # 80 packages + strip + cleanup

    def test_coreutils_refuses_root_configure(self):
        c = fresh()
        run_until(c, "FORCE_UNSAFE_CONFIGURE=1 ./configure \\")
        self.assertEqual(c.run("./configure --prefix=/usr"), 1)
        self.assertIn("you should not run configure as root", c.text())

    def test_glibc_needs_build_dir(self):
        c = fresh()
        run_until(c, "mkdir -v build")    # glibc, after the patch
        self.assertEqual(c.run("./configure --prefix=/usr"), 1)
        self.assertIn("separate build directory", c.text())

    def test_gcc_needs_gmp_mpfr_mpc(self):
        c = fresh()
        run_until(c, "tar -xf gmp-6.3.0.tar.xz")
        c.run("tar -xf gcc-14.2.0.tar.xz && cd gcc-14.2.0 && mkdir b && cd b")
        self.assertEqual(c.run("../configure --prefix=/usr"), 1)
        self.assertIn("GMP 4.2+", c.text())

    def test_tests_as_tester_need_chown(self):
        c = fresh()
        run_until(c, "tar -xf sed-4.9.tar.xz")
        c.run("tar -xf sed-4.9.tar.xz && cd sed-4.9 && ./configure --prefix=/usr && make")
        self.assertEqual(c.run('su tester -c "PATH=$PATH make check"'), 2)
        self.assertIn("Permission denied", c.text())
        c.run("chown -R tester .")
        self.assertEqual(c.run('su tester -c "PATH=$PATH make check"'), 0)
        c.run('su tester -c "whoami"')
        self.assertEqual(c.text(), "tester\n")

    def test_pip_needs_backend(self):
        c = fresh()
        run_until(c, "tar -xf flit_core-3.9.0.tar.gz")
        c.run("cd /sources && tar -xf wheel-0.44.0.tar.gz && cd wheel-0.44.0")
        self.assertEqual(c.run("pip3 wheel -w dist --no-cache-dir --no-build-isolation "
                               "--no-deps $PWD"), 1)
        self.assertIn("No module named 'flit_core'", c.text())

    def test_expect_needs_ptys(self):
        c = fresh()
        run_until(c, "python3 -c")
        c.run("exit")
        c.run("umount $LFS/dev/pts")
        from tests.helpers import CHROOT_CMD
        c.run(CHROOT_CMD)
        self.assertEqual(c.run("python3 -c 'from pty import spawn; spawn([\"echo\", \"ok\"])'"), 1)
        self.assertIn("out of pty devices", c.text())

    def test_ch7_version_then_ch8_version(self):
        c = fresh()
        run_until(c, "tar -xf gettext-0.22.5.tar.xz")
        c.run("cd /sources && tar -xf gettext-0.22.5.tar.xz && cd gettext-0.22.5")
        c.run("./configure --prefix=/usr --disable-static")
        self.assertTrue(c.machine.lookup(".")["build"].get("ch8"))

    def test_glibc_configuration(self):
        c = fresh()
        run_until(c, "tzselect", include=True)
        self.assertIn("Europe/Moscow", c.text())
        self.assertEqual(c.run("localedef -i xx_XX -f UTF-8 xx_XX.UTF-8"), 4)
        self.assertIn("cannot open locale definition file", c.text())
        run_until(c, "mkdir -pv /etc/ld.so.conf.d", include=True)
        self.assertIn("glibc8", c.game.done)
        self.assertIn("glibc8cfg", c.game.done)

    def test_man_pages_crypt(self):
        c = fresh()
        c.run("cd /sources && tar -xf man-pages-6.9.1.tar.xz && cd man-pages-6.9.1")
        c.run("make -R GIT=false prefix=/usr install")
        self.assertTrue(c.machine.exists("/usr/share/man/man3/crypt.3"))
        self.assertNotIn("man-pages8", c.game.done)

    def test_openssl_without_sed_installs_static_libs(self):
        c = fresh()
        run_until(c, "./config --prefix=/usr")
        c.run("./config --prefix=/usr --openssldir=/etc/ssl --libdir=lib shared zlib-dynamic")
        c.run("make MANSUFFIX=ssl install")
        self.assertTrue(c.machine.exists("/usr/lib/libcrypto.a"))

    def test_gcc_sanity_and_everything(self):
        c = fresh()
        run_until(c, "readelf -l a.out | grep ': /lib'", include=True)
        self.assertIn("[Requesting program interpreter: /lib64/ld-linux-x86-64.so.2]", c.text())
        self.assertIn("sanity8", c.machine.flags)
        while c.step < len(_STEPS):
            line, inputs, must = _STEPS[c.step]
            st = c.run(line, inputs)
            if must:
                self.assertEqual(st, 0, f"{line}\n{c.text()}")
            c.step += 1
        missing = [o for o in BY_ID if o not in c.game.done and not BY_ID[o].optional]
        self.assertEqual(missing, [])
        self.assertEqual(c.machine.prompt(), "(lfs chroot) root:/sources# ")
        c.run("file /usr/bin/bash")
        self.assertIn("ELF 64-bit", c.text())


if __name__ == "__main__":
    unittest.main()
