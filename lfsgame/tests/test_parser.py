"""Shell parser / expansion / completion tests."""
import unittest

from lfsgame.shell import parser
from lfsgame.shell.complete import complete
from lfsgame.sim.machine import Machine


def words(line, env=None, files=(), subst=None):
    env = env or {}
    ((_, cmd),) = parser.parse(line)
    out = []
    for w in cmd.words:
        out += parser.expand(w, lambda n: env.get(n, ""), lambda u: "/root" if u == "" else None,
                             lambda p: list(files), lambda p: True, subst)
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
        self.assertEqual([c for c, _ in cmds], [None, "&&", "||", ";"])
        self.assertEqual(cmds[3][1].redirs[0][0], ">")

    def test_pipeline_if_for(self):
        ((_, p),) = parser.parse("echo x | grep x | wc -l")
        self.assertIsInstance(p, parser.Pipeline)
        self.assertEqual(len(p.cmds), 3)
        ((_, node),) = parser.parse("if [ -e x ]; then a; elif b; then c; else d; fi")
        self.assertIsInstance(node, parser.If)
        self.assertEqual(len(node.clauses), 2)
        self.assertIsNotNone(node.else_body)
        ((_, node),) = parser.parse("for i in a b c\ndo\n echo $i\ndone")
        self.assertIsInstance(node, parser.For)
        self.assertEqual(len(node.words), 3)

    def test_heredoc(self):
        ((_, cmd), (conn, nxt)) = parser.parse('cat > f << "EOF"\nA $X\n\tB\nEOF\necho done')
        op, hd = cmd.redirs[1]
        self.assertEqual((op, hd.delim, hd.quoted, hd.body), ("<<", "EOF", True, "A $X\n\tB\n"))
        ((_, cmd),) = parser.parse("cat <<-END\n\tx\n\tEND")
        self.assertEqual(cmd.redirs[0][1].body, "x\n")

    def test_heredoc_expansion(self):
        body = parser.expand_heredoc("a $X ${X}y \\$X $(cmd)\n", lambda n: "v", lambda c: c.upper() + "\n")
        self.assertEqual(body, "a v vy $X CMD\n")

    def test_incomplete(self):
        for text in ("echo 'x", 'echo "x', "ls \\", "a &&", "a |", "if true; then x",
                     "for i in a; do", "cat << EOF\nabc", "echo $(ls", "echo `ls"):
            with self.assertRaises(parser.Incomplete, msg=text):
                parser.parse(text)

    def test_continuation_joins(self):
        self.assertEqual(words("echo a \\\n  b"), ["echo", "a", "b"])

    def test_command_substitution_and_splitting(self):
        self.assertEqual(words("x $(m)-lfs `m` \"$(two)\" $(two)",
                               subst=lambda c: {"m": "x86_64\n", "two": "a b\n"}[c]),
                         ["x", "x86_64-lfs", "x86_64", "a b", "a", "b"])

    def test_comment(self):
        self.assertEqual(words("echo hi # comment"), ["echo", "hi"])

    def test_errors(self):
        for bad in ("&& a", "; a", "x >", "a &", "fi", "then x", "case x in", "if; then a; fi"):
            with self.assertRaises(parser.ParseError, msg=bad):
                parser.parse(bad)


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
