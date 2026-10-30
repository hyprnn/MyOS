"""Slide-out side panel (right edge) with the LFS book reader.

Placed with place(): anchor 'nw' at relx=1.0, x = negative offset.
Collapsed: only the HANDLE strip is visible. Open: HANDLE + width visible.
on_layout(is_open) is called when the panel finished opening / starts closing, so the
App can shrink the screen and the terminal never hides under the panel.
"""
from __future__ import annotations

import tkinter as tk
from typing import Callable, Optional

from . import theme
from .book import Book, BookError
from .sim.objectives import OBJECTIVES


class SidePanel(tk.Frame):
    WIDTH = 440
    HANDLE = 28
    STEPS = 14
    DELAY_MS = 12

    def __init__(self, master, on_command: Callable[[str], None],
                 on_layout: Callable[[bool], None], get_game: Callable[[], object],
                 on_focus_back: Callable[[], None]):
        super().__init__(master, bg=theme.PANEL_BG)
        self.on_command = on_command
        self.on_layout = on_layout
        self.on_focus_back = on_focus_back
        self.get_game = get_game
        self.is_open = False
        self._x = -self.HANDLE
        self._job = None
        self.section: Optional[str] = None   # None = table of contents
        try:
            self.book: Optional[Book] = Book.load()
            self.book_error = ""
        except (BookError, OSError) as exc:
            self.book, self.book_error = None, str(exc)

        self.handle = tk.Label(
            self, text=self._handle_text(), bg=theme.HANDLE_BG, fg=theme.ACCENT,
            font=theme.font(bold=True), width=2, cursor="hand2",
        )
        self.handle.pack(side="left", fill="y")
        self.handle.bind("<Button-1>", lambda e: self.toggle())

        body = tk.Frame(self, bg=theme.PANEL_BG, width=self.WIDTH)
        body.pack(side="left", fill="both", expand=True)
        body.pack_propagate(False)
        nav = tk.Frame(body, bg=theme.PANEL_BG)
        nav.pack(fill="x", padx=12, pady=(10, 0))
        self.nav = {}
        for key, label, cmd in (("prev", "◀ prev", lambda: self.step(-1)),
                                ("toc", "contents", self.show_toc),
                                ("next", "next ▶", lambda: self.step(1))):
            lbl = tk.Label(nav, text=label, bg=theme.PANEL_BG, fg=theme.LINK,
                           font=theme.font(), cursor="hand2")
            lbl.pack(side="left" if key != "next" else "right", padx=(0, 14))
            lbl.bind("<Button-1>", lambda e, c=cmd: c())
            self.nav[key] = lbl
        tk.Label(body, text="F2 close · F3/F4 prev/next · click $ lines to paste",
                 bg=theme.PANEL_BG, fg=theme.DIM, font=theme.font_sized(-3),
                 anchor="w").pack(side="bottom", fill="x", padx=12, pady=(0, 8))

        self.text = tk.Text(
            body, bg=theme.PANEL_BG, fg=theme.FG, font=theme.font_sized(-1), bd=0,
            highlightthickness=0, wrap="word", cursor="arrow", padx=12, pady=8,
            spacing1=2, spacing3=4, insertwidth=0,
        )
        self.text.pack(fill="both", expand=True, pady=(6, 4))
        t = self.text
        t.tag_configure("chapter", foreground=theme.DIM, font=theme.font_sized(-2))
        t.tag_configure("title", foreground=theme.ACCENT, font=theme.font_sized(3, bold=True),
                        spacing3=10)
        t.tag_configure("h2", foreground=theme.ACCENT, font=theme.font_sized(0, bold=True),
                        spacing1=10)
        t.tag_configure("para", lmargin1=0, lmargin2=0)
        t.tag_configure("bullet", lmargin1=8, lmargin2=24)
        t.tag_configure("pre", foreground=theme.DIM, lmargin1=16, lmargin2=16, wrap="none",
                        spacing1=0, spacing3=0)
        t.tag_configure("cmd", foreground=theme.ACCENT, background=theme.CMD_BG, lmargin1=8,
                        lmargin2=8, wrap="char")
        t.tag_configure("cmd_hover", background=theme.SELECT_BG)
        t.tag_configure("note", foreground=theme.WARN, lmargin1=8, lmargin2=8)
        t.tag_configure("link", foreground=theme.LINK)
        t.tag_configure("toc_ch", foreground=theme.DIM, spacing1=8)
        t.tag_configure("toc_line", lmargin1=0, lmargin2=84)
        t.tag_configure("done", foreground=theme.ACCENT)
        t.tag_configure("todo", foreground=theme.DIM)
        t.tag_configure("current", foreground=theme.WARN)
        t.tag_bind("cmd", "<Button-1>", self._click_cmd)
        t.tag_bind("cmd", "<Enter>", lambda e: t.configure(cursor="hand2"))
        t.tag_bind("cmd", "<Leave>", lambda e: (t.configure(cursor="arrow"),
                                                t.tag_remove("cmd_hover", "1.0", "end")))
        t.tag_bind("cmd", "<Motion>", self._hover_cmd)
        t.tag_bind("link", "<Enter>", lambda e: t.configure(cursor="hand2"))
        t.tag_bind("link", "<Leave>", lambda e: t.configure(cursor="arrow"))
        t.bind("<Key>", lambda e: None if e.keysym.startswith("F") else "break")
        # the reader never keeps keyboard focus: typing must keep going to the terminal
        t.bind("<ButtonRelease-1>", lambda e: self.after_idle(self.on_focus_back), add="+")
        t.configure(takefocus=0)

        self.place(relx=1.0, rely=0.0, relheight=1.0,
                   width=self.WIDTH + self.HANDLE, x=self._x, anchor="nw")
        self.show_toc()

    # ---- public ----
    def toggle(self):
        self.close() if self.is_open else self.open()

    def open(self):
        if self.is_open:
            return
        self.is_open = True
        self.handle.configure(text=self._handle_text())
        self.render()
        self._animate(-(self.WIDTH + self.HANDLE), done=lambda: self.on_layout(True))

    def close(self):
        if not self.is_open:
            return
        self.is_open = False
        self.handle.configure(text=self._handle_text())
        self.on_layout(False)
        self._animate(-self.HANDLE)

    def show_section(self, sid: Optional[str]) -> bool:
        if self.book is None:
            return False
        if sid in (None, "toc", "contents"):
            self.show_toc()
            return True
        if self.book.get(sid) is None:
            return False
        self.section = sid
        game = self.get_game()
        if game is not None:
            game.book_section = sid
        self.render()
        return True

    def show_toc(self):
        self.section = None
        self.render()

    def step(self, delta: int):
        if self.book is None:
            return
        if self.section is None:
            target = self.book.sections[0] if delta > 0 else None
        else:
            target = self.book.neighbor(self.section, delta)
        if target is not None:
            self.show_section(target.id)
        elif delta < 0:
            self.show_toc()

    def refresh(self):
        """Progress changed: re-render (checkmarks)."""
        if self.is_open:
            self.render()

    # ---- rendering ----
    def _section_state(self, sid: str):
        game = self.get_game()
        objs = [o for o in OBJECTIVES if o.section == sid]
        if game is None or not objs:
            return None
        if all(o.id in game.done for o in objs if not o.optional):
            return "done"
        cur = game.current()
        if cur is not None and cur.section == sid:
            return "current"
        return "todo"

    def render(self):
        t = self.text
        t.configure(state="normal")
        t.delete("1.0", "end")
        if self.book is None:
            t.insert("end", "LFS BOOK\n", "title")
            t.insert("end", f"The book could not be loaded:\n{self.book_error}\n", "note")
        elif self.section is None:
            self._render_toc()
        else:
            self._render_section(self.book.get(self.section))
        t.configure(state="disabled")
        t.yview_moveto(0)
        has_prev = self.section is not None
        has_next = self.book is not None and (self.section is None
                                              or self.book.neighbor(self.section, 1) is not None)
        self.nav["prev"].configure(fg=theme.LINK if has_prev else theme.DIM)
        self.nav["next"].configure(fg=theme.LINK if has_next else theme.DIM)

    def _render_toc(self):
        t = self.text
        t.insert("end", "Linux From Scratch\n", "title")
        t.insert("end", "The in-game edition. Click a section to open it.\n", "chapter")
        for chapter, sections in self.book.toc():
            t.insert("end", chapter + "\n", "toc_ch")
            for s in sections:
                state = self._section_state(s.id)
                mark = {"done": "✓ ", "current": "▶ ", "todo": "· "}.get(state, "  ")
                t.insert("end", "  " + mark, (state or "todo", "toc_line"))
                tag = f"sec:{s.id}"
                t.insert("end", f"{s.id:<4}{s.title}", ("link", tag, "toc_line"))
                t.insert("end", "\n", "toc_line")
                t.tag_bind(tag, "<Button-1>", lambda e, sid=s.id: self.show_section(sid))

    def _render_section(self, s):
        t = self.text
        t.insert("end", s.chapter + "\n", "chapter")
        t.insert("end", f"{s.id}. {s.title}\n" if s.id[0].isdigit() and s.id != "0"
                 else f"{s.title}\n", "title")
        for kind, body in s.blocks:
            if kind == "para":
                t.insert("end", body + "\n", "para")
            elif kind == "h2":
                t.insert("end", body + "\n", "h2")
            elif kind == "cmd":
                t.insert("end", "$ " + body + "\n", "cmd")
            elif kind == "pre":
                t.insert("end", body + "\n", "pre")
            elif kind == "note":
                t.insert("end", "Note: " + body + "\n", "note")
            elif kind == "bullet":
                t.insert("end", "•  " + body + "\n", "bullet")
        game = self.get_game()
        objs = [o for o in OBJECTIVES if o.section == s.id]
        if objs and game is not None:
            t.insert("end", "\nObjectives in this section\n", "h2")
            for o in objs:
                done = o.id in game.done
                t.insert("end", ("✓ " if done else "○ ") + o.title + "\n",
                         "done" if done else "todo")

    def _handle_text(self) -> str:
        return ("▶" if self.is_open else "◀") + "\n\nL\nF\nS"

    # ---- clicks ----
    def _cmd_at(self, event) -> Optional[str]:
        idx = self.text.index(f"@{event.x},{event.y}")
        line = self.text.get(f"{idx} linestart", f"{idx} lineend")
        return line[2:] if line.startswith("$ ") else None

    def _click_cmd(self, event):
        cmd = self._cmd_at(event)
        if cmd:
            self.on_command(cmd)

    def _hover_cmd(self, event):
        idx = self.text.index(f"@{event.x},{event.y}")
        self.text.tag_remove("cmd_hover", "1.0", "end")
        self.text.tag_add("cmd_hover", f"{idx} linestart", f"{idx} lineend")

    # ---- animation ----
    def _animate(self, target: int, done: Optional[Callable[[], None]] = None):
        if self._job is not None:
            self.after_cancel(self._job)
            self._job = None
        start = self._x
        step = 0

        def tick():
            nonlocal step
            step += 1
            t = step / self.STEPS
            eased = 1 - (1 - t) ** 3  # ease-out cubic
            self._x = round(start + (target - start) * eased)
            self.place_configure(x=self._x)
            if step < self.STEPS:
                self._job = self.after(self.DELAY_MS, tick)
            else:
                self._job = None
                if done is not None:
                    done()

        tick()
