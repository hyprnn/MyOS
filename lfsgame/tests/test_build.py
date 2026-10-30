"""Chapter 4.4 / 5 tests: startup files, here-docs, builds and their typical mistakes."""
import unittest

from tests.helpers import BASH_PROFILE, BASHRC, CH4_4, CH5, FULL_WALKTHROUGH, FakeCtx

BINUTILS_CONF = CH5[5][0]
GCC_CONF = CH5[16][0]


def ready(upto_ch5=0, env=True):
    """A game after chapter 4.3 (+4.4 when env) and the first ``upto_ch5`` chapter-5 lines."""
    c = FakeCtx(seed=5)
    for line, inputs in FULL_WALKTHROUGH:
        c.run(line, inputs)
    for line, inputs in (CH4_4 if env else []) + CH5[:upto_ch5]:
        assert c.run(line, inputs) == 0, (line, c.text())
    return c


class EnvTests(unittest.TestCase):
    def test_interactive_heredoc(self):
        c = FakeCtx()
        self.assertEqual(c.run('cat > f << "EOF"', ["a $HOME", "b", "EOF"]), 0)
        self.assertEqual(c.machine.lookup("/root/f")["data"], "a $HOME\nb\n")
        c.run("cat > g << EOF", ["a $HOME", "EOF"])
        self.assertEqual(c.machine.lookup("/root/g")["data"], "a /root\n")

    def test_multiline_if_and_for(self):
        c = FakeCtx()
        c.run("if [ -L /bin ]", ["then echo link", "else echo dir", "fi"])
        self.assertEqual(c.text(), "link\n")
        c.run("for d in etc usr; do test -d /$d && echo $d; done")
        self.assertEqual(c.text(), "etc\nusr\n")

    def test_bashrc_trap_pollutes_env(self):
        c = ready(env=False)
        c.run("exit")
        c.run("su - lfs")
        c.run(BASH_PROFILE)
        c.run(BASHRC)
        c.run("source ~/.bash_profile")
        self.assertEqual(c.machine.env.get("CC"), "distcc gcc")
        self.assertNotIn("lfsenv", c.game.done)
        self.assertEqual(c.machine.prompt(), "lfs:~$ ")

    def test_clean_env(self):
        c = ready()
        env = c.machine.env
        self.assertEqual(env["LFS_TGT"], "x86_64-lfs-linux-gnu")
        self.assertEqual(env["PATH"], "/mnt/lfs/tools/bin:/usr/bin")
        self.assertEqual(env["MAKEFLAGS"], "-j8")
        self.assertNotIn("CC", env)
        self.assertIn("lfsenv", c.game.done)
        self.assertIn("makeflags", c.game.done)

    def test_su_reads_bash_profile(self):
        c = ready()
        c.run("exit")
        c.run("exit")  # back to root
        self.assertEqual(c.machine.user, "root")
        c.run("su - lfs")
        self.assertEqual(c.machine.env.get("LFS"), "/mnt/lfs")


class BuildTests(unittest.TestCase):
    def test_configure_with_distcc_fails(self):
        c = ready(env=False)
        c.run("exit")
        c.run("su - lfs")
        c.run(BASH_PROFILE)
        c.run(BASHRC)
        c.run("source ~/.bash_profile")
        for line, _ in CH5[:5]:
            c.run(line)
        self.assertEqual(c.run(BINUTILS_CONF), 1)
        self.assertIn("C compiler cannot create executables", c.text())

    def test_wrong_directory(self):
        c = ready(upto_ch5=3)  # inside binutils-2.43.1, no build dir
        self.assertEqual(c.run("../configure --prefix=$LFS/tools"), 127)
        self.assertIn("No such file or directory", c.text())
        self.assertEqual(c.run("make"), 2)
        self.assertIn("no makefile found", c.text())

    def test_missing_lfs_tgt(self):
        c = ready(upto_ch5=5)
        c.run("unset LFS_TGT")
        self.assertEqual(c.run(BINUTILS_CONF), 1)
        self.assertIn("missing argument to --target", c.text())

    def test_gcc_needs_binutils_and_math_libs(self):
        c = ready(upto_ch5=1)
        c.run("tar -xf gcc-14.2.0.tar.xz && cd gcc-14.2.0 && mkdir build && cd build")
        self.assertEqual(c.run(GCC_CONF), 1)
        self.assertIn("GMP 4.2+, MPFR 3.1.0+ and MPC", c.text())
        c.run("cd .. && tar -xf ../mpfr-4.2.1.tar.xz && mv mpfr-4.2.1 mpfr && "
              "tar -xf ../gmp-6.3.0.tar.xz && mv gmp-6.3.0 gmp && "
              "tar -xf ../mpc-1.3.1.tar.gz && mv mpc-1.3.1 mpc && cd build")
        self.assertEqual(c.run(GCC_CONF), 1)
        self.assertIn("cannot find an assembler", c.text())

    def test_gcc_pass1_without_newlib_fails_in_make(self):
        c = ready(upto_ch5=16)
        c.run(GCC_CONF.replace("--with-newlib ", ""))
        self.assertEqual(c.run("make"), 2)
        self.assertIn("stdio.h: No such file or directory", c.text())

    def test_glibc_in_tree_and_without_destdir(self):
        c = ready(upto_ch5=31)  # glibc patched, in glibc-2.40, no build dir yet
        conf = CH5[33][0]
        self.assertEqual(c.run(conf.replace("../configure", "./configure")
                               .replace("../scripts", "./scripts")), 1)
        self.assertIn("separate build directory", c.text())
        for line, _ in CH5[31:34]:
            self.assertEqual(c.run(line), 0, c.text())
        self.assertEqual(c.run("make install"), 2)
        self.assertIn("Permission denied", c.text())
        self.assertNotIn("glibc", c.game.done)

    def test_glibc_as_root_without_destdir_is_refused(self):
        c = ready(upto_ch5=34)
        c.machine.user = "root"
        self.assertEqual(c.run("make install"), 2)
        self.assertIn("rolled it back", c.text())
        self.assertFalse(c.machine.exists("/usr/lib/libc.so.6"))

    def test_sanity_check_before_glibc_fails(self):
        c = ready(upto_ch5=20)  # gcc pass 1 + limits.h done
        self.assertIn("limits", c.game.done)
        self.assertEqual(c.run("echo 'int main(){}' | $LFS_TGT-gcc -xc -"), 1)
        self.assertIn("cannot find crt1.o", c.text())
        self.assertNotIn("sanity", c.game.done)

    def test_patch_twice(self):
        c = ready(upto_ch5=31)
        self.assertEqual(c.run("patch -Np1 -i ../glibc-2.40-fhs-1.patch"), 1)
        self.assertIn("previously applied", c.text())
        self.assertEqual(c.run("patch -p1 -i ../glibc-2.40-fhs-1.patch", ["y"]), 0)  # reverse

    def test_libstdcxx_before_glibc(self):
        c = ready(upto_ch5=21)
        c.run("tar -xf gcc-14.2.0.tar.xz && cd gcc-14.2.0 && mkdir build && cd build")
        self.assertEqual(c.run(CH5[43][0]), 1)
        self.assertIn("GCC_NO_EXECUTABLES", c.text())

    def test_headers_without_cleanup(self):
        c = ready(upto_ch5=24)  # make headers done, find not run
        c.run("cp -r usr/include $LFS/usr")
        self.assertNotIn("headers", c.game.done)
        c.run("find $LFS/usr/include -type f ! -name '*.h' -delete")
        self.assertIn("headers", c.game.done)

    def test_broken_tarball(self):
        c = ready()
        c.run("cd $LFS/sources && echo junk > fake.tar.xz")
        self.assertEqual(c.run("tar -xf fake.tar.xz"), 2)
        self.assertIn("does not look like a tar archive", c.text())

    def test_time_and_sbu(self):
        c = ready(upto_ch5=6)
        c.run("time make")
        self.assertIn("real\t0m2", c.text())  # 1 SBU at -j8 ~ 28 s


class ToolTests(unittest.TestCase):
    def setUp(self):
        self.c = FakeCtx()

    def test_sed(self):
        c = self.c
        c.run("printf_x 2>/dev/null; echo 'm64=../lib64 x' > f")
        c.run("sed -e '/m64=/s/lib64/lib/' -i.orig f")
        self.assertEqual(c.machine.lookup("/root/f")["data"], "m64=../lib x\n")
        self.assertEqual(c.machine.lookup("/root/f.orig")["data"], "m64=../lib64 x\n")
        c.run("echo 'RTLDLIST=\"/usr/lib /usr/lib64\"' | sed '/RTLDLIST=/s@/usr@@g'")
        self.assertEqual(c.text(), 'RTLDLIST="/lib /lib64"\n')
        c.run("echo abc | sed 's/\\(b\\)/[\\1&]/'")
        self.assertEqual(c.text(), "a[bb]c\n")

    def test_find_grep_wc_head(self):
        c = self.c
        c.run("mkdir -p d/e && touch d/a.h d/b.txt d/e/c.h")
        c.run("find d -type f -name '*.h'")
        self.assertEqual(c.text(), "d/a.h\nd/e/c.h\n")
        c.run("find d -type f ! -name '*.h' -delete")
        self.assertFalse(c.machine.exists("/root/d/b.txt"))
        c.run("grep -c x /etc/passwd")
        self.assertEqual(c.text(), "1\n")
        c.run("head -n 1 /etc/fstab")
        self.assertTrue(c.text().startswith("# <device>"))

    def test_test_builtin(self):
        c = self.c
        for expr, want in (("-e /etc", 0), ("-f /etc", 1), ("-L /bin", 0), ("! -L /bin", 1),
                           ("a = a", 0), ("1 -lt 2", 0), ("-z ''", 0)):
            self.assertEqual(c.run(f"[ {expr} ]"), want, expr)

    def test_env_i_and_prompt(self):
        c = self.c
        c.run("env -i A=1 env")
        self.assertEqual(c.text(), "A=1\n")
        self.assertIn("HOME", c.machine.env)
        c.run("export PS1='\\u@\\h \\W\\$ '")
        self.assertEqual(c.machine.prompt(), "root@host ~# ")

    def test_command_substitution_runs(self):
        c = self.c
        c.run("echo $(uname -m)-lfs-linux-gnu `nproc`")
        self.assertEqual(c.text(), "x86_64-lfs-linux-gnu 8\n")

    def test_script_exit_and_subshell(self):
        c = self.c
        c.run("echo 'cd /etc; export X=1; exit 3; echo no' > s.sh")
        self.assertEqual(c.run("bash s.sh"), 3)
        self.assertEqual(c.machine.cwd, "/root")
        self.assertNotIn("X", c.machine.env)
        self.assertEqual(c.run("source s.sh"), 3)
        self.assertEqual(c.machine.cwd, "/etc")
        self.assertFalse(c.menu)


if __name__ == "__main__":
    unittest.main()
