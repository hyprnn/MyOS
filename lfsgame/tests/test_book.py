"""Book parser/content tests."""
import unittest

from lfsgame.book import Book, BookError, parse
from lfsgame.commands import REGISTRY
from lfsgame.shell import parser as shparser
from lfsgame.sim.objectives import OBJECTIVES


class BookTests(unittest.TestCase):
    def test_parse_markup(self):
        secs = parse("@chapter C\n@section 1 | One\nline a\nline b\n\n## Sub\n$ ls -l\n+ -a\n> out\n"
                     "! careful\n- item\n")
        self.assertEqual(len(secs), 1)
        s = secs[0]
        self.assertEqual((s.id, s.title, s.chapter), ("1", "One", "C"))
        self.assertEqual(s.blocks, [("para", "line a line b"), ("h2", "Sub"), ("cmd", "ls -l\n-a"),
                                    ("pre", "out"), ("note", "careful"), ("bullet", "item")])

    def test_parse_errors(self):
        with self.assertRaises(BookError):
            parse("text before section\n")
        with self.assertRaises(BookError):
            parse("@section no-title\n")
        with self.assertRaises(BookError):
            parse("@section 1 | x\ntext\n+ continuation\n")

    def test_real_book_loads(self):
        book = Book.load()
        self.assertGreaterEqual(len(book.sections), 10)
        self.assertEqual(book.sections[0].id, "0")
        self.assertEqual(book.neighbor("2.4", 1).id, "2.5")
        self.assertIsNone(book.neighbor(book.sections[-1].id, 1))

    def test_objectives_point_to_existing_sections(self):
        book = Book.load()
        for o in OBJECTIVES:
            self.assertIsNotNone(book.get(o.section), o.id)

    def test_every_book_command_exists_and_parses(self):
        """Every '$ command' in the book must parse and name a simulated command."""
        from lfsgame.commands.build import TOOL_HANDLERS
        from lfsgame.sim import ch8
        known = set(REGISTRY) | set(TOOL_HANDLERS) | {"cc"}
        for p in ch8.PKGS:
            known |= set(p.bins) | set(p.sbins)
        book = Book.load()
        for s in book.sections:
            for kind, body in s.blocks:
                if kind != "cmd":
                    continue
                for _, node in shparser.parse(body):
                    simples = node.cmds if isinstance(node, shparser.Pipeline) else [node]
                    for simple in simples:
                        if not isinstance(simple, shparser.Simple) or not simple.words:
                            continue
                        words = [w for w in simple.words]
                        while words and shparser.is_assignment(words[0]):
                            words.pop(0)  # CC=gcc ./configure
                        if not words:
                            continue
                        name = shparser.word_text(words[0])
                        if name in ("[", "") or "/" in name or "$" in name or "`" in name:
                            continue  # ../configure, $LFS_TGT-gcc: programs in the fs
                        self.assertIn(name, known, f"{s.id}: {body}")

    def test_generated_chapter8_is_up_to_date(self):
        from pathlib import Path

        from lfsgame.sim import ch8
        path = Path(__file__).parent.parent / "lfsgame/book/content/09-final-system.txt"
        self.assertEqual(path.read_text(encoding="utf-8"), ch8.book_text(),
                         "run: python tools/gen_ch8_book.py")


if __name__ == "__main__":
    unittest.main()
