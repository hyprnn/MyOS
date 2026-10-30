"""Main menu screen (terminal-style, keyboard + mouse)."""
from __future__ import annotations

import tkinter as tk
from typing import Callable, List, Optional, Tuple

from . import save, theme
from .logo import LOGO, TAGLINE

Item = Tuple[str, str, bool]  # (label, action key, enabled)


def _swallow(event):
    """Block typing into the read-only Text, but let F-keys (F2 panel) bubble up."""
    k = event.keysym
    if len(k) > 1 and k[0] == "F" and k[1:].isdigit():
        return None
    return "break"


class MenuScreen(tk.Frame):
    def __init__(self, master, on_action: Callable[[str], None],
                 session_info: Callable[[], Tuple[bool, bool]]):
        super().__init__(master, bg=theme.BG)
        self.on_action = on_action
        self.session_info = session_info  # -> (has_session, has_unsaved_progress)
        self.index = 0
        self.items: List[Item] = []
        self.confirm: Optional[Tuple[str, str]] = None  # (question, action)
        self._first_item_line = 1
        self.text = tk.Text(
            self, bg=theme.BG, fg=theme.FG, font=theme.font(), bd=0,
            highlightthickness=0, cursor="arrow", wrap="none",
            insertwidth=0, state="disabled",
        )
        self.text.pack(fill="both", expand=True, padx=24, pady=24)
        for name, kw in (
            ("logo", dict(foreground=theme.ACCENT)),
            ("tag", dict(foreground=theme.DIM)),
            ("item", dict(foreground=theme.FG)),
            ("off", dict(foreground=theme.DIM)),
            ("sel", dict(background=theme.SELECT_BG, foreground=theme.ACCENT)),
            ("hint", dict(foreground=theme.DIM)),
            ("ask", dict(foreground=theme.WARN)),
        ):
            self.text.tag_configure(name, justify="center", **kw)
        self.text.bind("<Button-1>", self._on_click)
        self.text.bind("<Motion>", self._on_motion)
        self.text.bind("<Key>", _swallow)
        # specific bindings win over <Key> on the same widget
        for keys, fn in (
            (("<Up>", "<k>"), lambda: self.move(-1)),
            (("<Down>", "<j>"), lambda: self.move(1)),
            (("<Return>", "<KP_Enter>"), self.accept),
            (("<Escape>",), self.escape),
        ):
            for k in keys:
                self.text.bind(k, lambda e, fn=fn: (fn(), "break")[1])
        self.refresh_items()
        self.render()

    # ---- public ----
    def activate(self):
        self.confirm = None
        self.refresh_items()  # save may have changed while in the terminal
        self.text.focus_set()
        self.render()

    def ask(self, question: str, action: str):
        """Show a yes/no question; Enter runs ``action``, Esc cancels."""
        self.confirm = (question, action)
        self.render()

    def refresh_items(self):
        has_session, _ = self.session_info()
        state, info = save.status()
        if state == "ok":
            load: Item = (f"Load saved game  ({info})", "continue", True)
        elif state == "corrupt":
            load = ("Load saved game  (save unreadable)", "continue", False)
        else:
            load = ("Load saved game  (no save)", "continue", False)
        self.items = ([("Resume", "resume", True)] if has_session else []) + [
            ("New game", "new_game", True),
            load,
            ("LFS Book", "book", True),
            ("Quit", "quit", True),
        ]
        self.index = min(self.index, len(self.items) - 1)

    # ---- rendering ----
    def render(self):
        t = self.text
        t.configure(state="normal")
        t.delete("1.0", "end")
        t.insert("end", "\n" + LOGO + "\n", "logo")
        t.insert("end", "\n" + TAGLINE + "\n\n\n", "tag")
        self._first_item_line = int(t.index("end-1c").split(".")[0])
        for i, (label, _, enabled) in enumerate(self.items):
            selected = i == self.index
            marker = "▶ " if selected else "  "
            tag = "sel" if selected else ("item" if enabled else "off")
            t.insert("end", f"{marker}{label}  \n", tag)
        if self.confirm:
            t.insert("end", f"\n{self.confirm[0]}\n", "ask")
            t.insert("end", "Enter = yes · Esc = no\n", "hint")
        else:
            t.insert("end", "\n↑/↓ select · Enter confirm · F2 LFS book · Esc quit\n", "hint")
        t.configure(state="disabled")

    # ---- input ----
    def move(self, delta: int):
        self.confirm = None
        self.index = (self.index + delta) % len(self.items)
        self.render()

    def accept(self):
        if self.confirm:
            action = self.confirm[1]
            self.confirm = None
            self.render()
            self.on_action(action)
            return
        _, action, enabled = self.items[self.index]
        if enabled:
            self.on_action(action)

    def escape(self):
        if self.confirm:
            self.confirm = None
            self.render()
        else:
            self.on_action("quit")

    def _line_to_index(self, event):
        line = int(self.text.index(f"@{event.x},{event.y}").split(".")[0])
        i = line - self._first_item_line
        return i if 0 <= i < len(self.items) else None

    def _on_motion(self, event):
        i = self._line_to_index(event)
        if i is not None and i != self.index and not self.confirm:
            self.index = i
            self.render()

    def _on_click(self, event):
        i = self._line_to_index(event)
        if i is not None and not self.confirm:
            self.index = i
            self.render()
            self.accept()
        return "break"
