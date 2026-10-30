"""The in-game LFS book: a tiny markup format parsed into sections. Pure, no tkinter.

Files: lfsgame/book/content/*.txt, read in file-name order. Markup (one per line):
  @chapter Title           chapter heading for the following sections
  @section ID | Title      start a new section (ID like "2.4")
  ## text                  sub-heading
  $ command                shell command (clickable in the reader: pastes into the terminal)
  > text                   preformatted output / example
  ! text                   note / warning
  - text                   bullet item
  (blank line)             paragraph break; other lines are joined into paragraphs
The text is an original paraphrase written for this game, not a copy of the real LFS book.
"""
from __future__ import annotations

from pathlib import Path
from typing import Dict, List, NamedTuple, Optional, Tuple

CONTENT_DIR = Path(__file__).with_name("content")

Block = Tuple[str, str]  # (kind, text): para, h2, cmd, pre, note, bullet


class Section(NamedTuple):
    id: str
    title: str
    chapter: str
    blocks: List[Block]


class BookError(Exception):
    pass


MARKERS = {"$ ": "cmd", "> ": "pre", "! ": "note", "- ": "bullet"}


def parse(text: str, chapter: str = "") -> List[Section]:
    sections: List[Section] = []
    para: List[str] = []

    def flush():
        if para and sections:
            sections[-1].blocks.append(("para", " ".join(para)))
        para.clear()

    for lineno, raw in enumerate(text.splitlines(), 1):
        line = raw.rstrip()
        if line.startswith("@chapter "):
            flush()
            chapter = line[9:].strip()
        elif line.startswith("@section "):
            flush()
            sid, sep, title = line[9:].partition("|")
            if not sep:
                raise BookError(f"line {lineno}: '@section ID | Title' expected")
            sections.append(Section(sid.strip(), title.strip(), chapter, []))
        elif not sections:
            if line.strip():
                raise BookError(f"line {lineno}: text before the first @section")
        elif not line.strip():
            flush()
        else:
            if line.startswith("## "):
                kind, body = "h2", line[3:]
            elif line[:2] in MARKERS:
                kind, body = MARKERS[line[:2]], line[2:]
            else:
                para.append(line.strip())
                continue
            flush()
            sections[-1].blocks.append((kind, body))
    flush()
    return sections


class Book:
    def __init__(self, sections: List[Section]):
        self.sections = sections
        self.index: Dict[str, int] = {}
        for i, s in enumerate(sections):
            if s.id in self.index:
                raise BookError(f"duplicate section id {s.id}")
            self.index[s.id] = i

    @classmethod
    def load(cls, directory: Optional[Path] = None) -> "Book":
        directory = Path(directory or CONTENT_DIR)
        sections: List[Section] = []
        for f in sorted(directory.glob("*.txt")):
            try:
                sections += parse(f.read_text(encoding="utf-8"))
            except BookError as exc:
                raise BookError(f"{f.name}: {exc}") from None
        return cls(sections)

    def get(self, sid: str) -> Optional[Section]:
        i = self.index.get(sid)
        return None if i is None else self.sections[i]

    def neighbor(self, sid: str, delta: int) -> Optional[Section]:
        i = self.index.get(sid, 0) + delta
        return self.sections[i] if 0 <= i < len(self.sections) else None

    def toc(self) -> List[Tuple[str, List[Section]]]:
        """[(chapter, [sections...]), ...] in book order."""
        out: List[Tuple[str, List[Section]]] = []
        for s in self.sections:
            if not out or out[-1][0] != s.chapter:
                out.append((s.chapter, []))
            out[-1][1].append(s)
        return out
