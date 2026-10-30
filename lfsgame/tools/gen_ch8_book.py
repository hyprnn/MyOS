"""Write lfsgame/book/content/09-final-system.txt from the chapter 8 table (lfsgame/sim/ch8.py).

Run after changing lfsgame/sim/ch8.py:   python tools/gen_ch8_book.py
(tests/test_book.py fails if the file is out of date)
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))

from lfsgame.sim import ch8  # noqa: E402

TARGET = os.path.join(os.path.dirname(HERE), "lfsgame", "book", "content", "09-final-system.txt")

if __name__ == "__main__":
    with open(TARGET, "w", encoding="utf-8") as fh:
        fh.write(ch8.book_text())
    print("wrote", TARGET)
