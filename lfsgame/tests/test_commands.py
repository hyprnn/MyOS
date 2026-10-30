"""Headless tests for the fake shell (no tkinter needed). Run: python -m unittest"""
import unittest

from lfsgame.commands import REGISTRY
from tests.helpers import FakeCtx


class CommandTests(unittest.TestCase):
    def setUp(self):
        self.c = FakeCtx()

    def test_echo(self):
        self.c.run('echo "hello  world" x')
        self.assertEqual(self.c.text(), "hello  world x\n")

    def test_unknown(self):
        self.assertEqual(self.c.run("nope"), 127)
        self.assertIn("command not found", self.c.text())
        self.assertEqual(self.c.out[0][1], "err")

    def test_bad_quotes(self):
        self.assertEqual(self.c.run('echo "x'), 2)
        self.assertIn("unexpected end of file", self.c.text())

    def test_empty(self):
        self.c.run("   ")
        self.assertEqual(self.c.out, [])

    def test_clear_and_menu(self):
        self.c.run("clear")
        self.c.run("menu")
        self.assertTrue(self.c.cleared and self.c.menu)

    def test_exit_from_root_goes_to_menu(self):
        self.c.run("exit")
        self.assertTrue(self.c.menu)

    def test_save_command(self):
        self.c.run("save")
        self.assertEqual(self.c.saved, 1)

    def test_help_lists_visible(self):
        self.c.run("help")
        for name, cmd in REGISTRY.items():
            if not cmd.hidden:
                self.assertIn(name, self.c.text())

    def test_help_single(self):
        self.assertEqual(self.c.run("help fdisk"), 0)
        self.assertIn("partition", self.c.text())

    def test_book_command(self):
        self.c.run("book 2.4")
        self.assertEqual(self.c.books[-1], "2.4")
        self.assertEqual(self.c.run("book 99.9"), 1)
        self.c.run("book")  # current objective's section
        self.assertEqual(self.c.books[-1], "2.4")

    def test_tasks_and_hint(self):
        self.c.run("tasks")
        self.assertIn("0/50", self.c.text())
        self.assertIn("Chapter 7: chroot and additional temporary tools: 13 objectives", self.c.text())
        self.c.run("hint")
        self.assertIn("lsblk", self.c.text())

    def test_cd_pwd_and_prompt(self):
        m = self.c.machine
        self.assertEqual(m.prompt(), "root@host:~# ")
        self.c.run("cd /etc && pwd")
        self.assertEqual(self.c.text(), "/etc\n")
        self.assertEqual(m.prompt(), "root@host:/etc# ")
        self.c.run("cd -")
        self.assertEqual(m.cwd, "/root")
        self.assertEqual(self.c.run("cd /nope"), 1)

    def test_files(self):
        c = self.c
        c.run("mkdir -pv a/b/c")
        self.assertIn("created directory 'a/b/c'", c.text())
        c.run("echo hello > a/f.txt; echo world >> a/f.txt")
        c.run("cat a/f.txt")
        self.assertEqual(c.text(), "hello\nworld\n")
        c.run("ln -sv f.txt a/link")
        c.run("cat a/link")
        self.assertEqual(c.text(), "hello\nworld\n")
        c.run("chmod -v 600 a/f.txt")
        self.assertIn("0644 (rw-r--r--) to 0600 (rw-------)", c.text())
        c.run("mv a/f.txt a/g.txt")
        self.assertFalse(c.machine.exists("/root/a/f.txt"))
        self.assertEqual(c.run("rm a"), 1)
        c.run("rm -rv a")
        self.assertFalse(c.machine.exists("/root/a"))

    def test_ls_and_glob(self):
        c = self.c
        c.run("touch x1 x2 y1")
        c.run("echo x*")
        self.assertEqual(c.text(), "x1 x2\n")
        c.run("ls -l /")
        self.assertIn("bin -> usr/bin", c.text())
        c.run("ls /dev/sd*")
        self.assertIn("/dev/sdb", c.text())

    def test_rm_root_refused(self):
        self.assertEqual(self.c.run("rm -rf /"), 1)
        self.assertIn("dangerous", self.c.text())

    def test_env_and_status(self):
        c = self.c
        c.run("false; echo $?")
        self.assertEqual(c.text(), "1\n")
        c.run("false || echo yes; true && echo ok")
        self.assertEqual(c.text(), "yes\nok\n")
        c.run("export FOO=bar; echo ${FOO}/x '$FOO'")
        self.assertEqual(c.text(), "bar/x $FOO\n")

    def test_version_check(self):
        self.assertEqual(self.c.run("bash version-check.sh"), 0)
        self.assertNotIn("ERROR", self.c.text())
        self.assertEqual(self.c.text().count("OK:"), 21)

    def test_pipes(self):
        self.c.run("printf_missing | wc -l")
        self.assertIn("command not found", self.c.errors())
        self.c.run("cat /etc/passwd | grep root | wc -l")
        self.assertEqual(self.c.text(), "1\n")
        self.assertEqual(self.c.run("echo abc | grep -q x"), 1)
        self.assertEqual(self.c.run("! echo abc | grep -q x"), 0)

    def test_background_rejected(self):
        self.assertEqual(self.c.run("ls &"), 2)
        self.assertIn("not supported", self.c.errors())


if __name__ == "__main__":
    unittest.main()
