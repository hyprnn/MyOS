"""Shell parser / expansion / completion tests."""
import unittest

from lfsgame.shell import parser
from lfsgame.shell.complete import complete
from lfsgame.sim.machine import Machine


def words(line, env=None, files=()):
    env = env or {}
    (cmd,) = parser.parse(line)
    out = []
    for w in cmd.words:
        out += parser.expand(w, lambda n: env.get(n, ""), lambda u: "/root" if u == "" else None,
                             lambda p: list(files), lambda p: True)
    return out


class ParserTests(unittest.TestCase):
    def test_quotes_and_escapes(self):
        self.assertEqual(words("echo 'a b' \"c  d\" e\\ f"), ["echo", "a b", "c  d", "e f"])

    def test_vars(self):
        env = {"LFS": "/mnt/lfs"}
        self.assertEqual(words('x $LFS ${LFS}/s "$LFS" \'$LFS\'', env),
                         ["x", "/mnt/lfs", "/mnt/lfs/s", "/mnt/lfs", "$LFS"])

    def test_unset_var_disappears_unless_quoted(self):
        self.assertEqual(words('echo $NOPE "$NOPE"'), ["echo", ""])

    def test_braces(self):
        self.assertEqual(words("mkdir $L/{etc,var} $L/usr/{bin,lib}", {"L": "/l"}),
                         ["mkdir", "/l/etc", "/l/var", "/l/usr/bin", "/l/usr/lib"])

    def test_nested_braces(self):
        self.assertEqual(words("x a{b{,1},c}"), ["x", "ab", "ab1", "ac"])

    def test_brace_without_comma_is_literal(self):
        self.assertEqual(words("x {a} ${HOME}", {"HOME": "/h"}), ["x", "{a}", "/h"])

    def test_tilde(self):
        self.assertEqual(words("ls ~ ~/x a~"), ["ls", "/root", "/root/x", "a~"])

    def test_glob(self):
        self.assertEqual(words("ls *.xz", files=["a.xz", "b.gz", "c.xz"]), ["ls", "a.xz", "c.xz"])
        self.assertEqual(words("ls '*.xz'", files=["a.xz"]), ["ls", "*.xz"])
        self.assertEqual(words("ls *.zip", files=["a.xz"]), ["ls", "*.zip"])

    def test_operators(self):
        cmds = parser.parse("a && b || c; d > f")
        self.assertEqual([c.conn for c in cmds], [None, "&&", "||", ";"])
        self.assertEqual(cmds[3].redirs[0][0], ">")

    def test_comment(self):
        self.assertEqual(words("echo hi # comment"), ["echo", "hi"])

    def test_errors(self):
        for bad in ("a |b", "&& a", "a &&", "echo $(ls)", "x >", "a &"):
            with self.assertRaises(parser.ParseError, msg=bad):
                (cmd, *_) = parser.parse(bad)
                for w in cmd.words:
                    parser.expand(w, lambda n: "", lambda u: None, lambda p: [], lambda p: True)


class CompleteTests(unittest.TestCase):
    def setUp(self):
        self.m = Machine.new(1)

    def test_command(self):
        self.assertEqual(complete(self.m, "fdi")[0], "sk ")
        ins, cands = complete(self.m, "mk")
        self.assertIn("mkdir", cands)
        self.assertIn("mkswap", cands)

    def test_path(self):
        self.assertEqual(complete(self.m, "cat version-")[0], "check.sh ")
        self.assertEqual(complete(self.m, "ls /et")[0], "c/")
        self.assertEqual(complete(self.m, "fdisk /dev/sd")[1], ["sda", "sda1", "sda2", "sdb", "sdc", "sdc1"])

    def test_var_path(self):
        self.m.env["X"] = "/etc"
        self.assertEqual(complete(self.m, "cat $X/host")[0], "name ")

    def test_after_operator(self):
        self.assertEqual(complete(self.m, "true && lsb")[0], "lk ")


if __name__ == "__main__":
    unittest.main()
