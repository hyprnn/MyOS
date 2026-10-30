"""Terminal screen: a tk.Text that behaves like a small shell.

Commands run as *processes* (generators, see shell/runner.py). The screen drives them:
Sleep -> after(ms), Input -> show a prompt and wait for Enter. While a process sleeps the
keyboard is locked (Ctrl+C still interrupts).
"""
from __future__ import annotations

import sys
import traceback
import tkinter as tk
from typing import Callable, List, Optional

from . import save, theme
from .commands import Input, Sleep, start_line
from .shell.complete import complete
from .sim.game import Game

TAGS = {"dim": dict(foreground=theme.DIM), "err": dict(foreground=theme.ERR),
        "warn": dict(foreground=theme.WARN), "prompt": dict(foreground=theme.ACCENT),
        "ok": dict(foreground=theme.ACCENT), "accent": dict(foreground=theme.ACCENT),
        "dir": dict(foreground=theme.DIR), "link": dict(foreground=theme.LINK),
        "banner": dict(foreground=theme.BG, background=theme.ACCENT)}


def _is_fkey(keysym: str) -> bool:
    return len(keysym) > 1 and keysym[0] == "F" and keysym[1:].isdigit()


class TerminalScreen(tk.Frame):
    def __init__(self, master, on_exit: Callable[[], None],
                 open_book: Callable[[Optional[str]], bool],
                 on_progress: Callable[[], None]):
        super().__init__(master, bg=theme.BG)
        self._on_exit = on_exit
        self._open_book = open_book
        self._on_progress = on_progress
        self.game: Optional[Game] = None
        self.history: List[str] = []
        self.dirty = False                 # progress since the last save/load
        self._hist_pos = 0
        self._proc = None                  # running process (generator)
        self._waiting: Optional[Input] = None
        self._sleep_job = None
        self._secret: List[str] = []
        self._typeahead: List[str] = []   # keys typed while a process was busy

        self.text = tk.Text(
            self, bg=theme.BG, fg=theme.FG, font=theme.font(), bd=0,
            highlightthickness=0, insertbackground=theme.ACCENT,
            insertwidth=2, wrap="word", undo=False,
        )
        self.text.pack(fill="both", expand=True, padx=12, pady=12)
        for name, kw in TAGS.items():
            self.text.tag_configure(name, **kw)
        self.text.bind("<Key>", self._on_key)
        self.text.bind("<<Paste>>", self._on_paste)
        self.text.bind("<Button-1>", lambda e: self.after(1, self._clamp_cursor))

    # ---- ctx API used by commands ----
    @property
    def machine(self):
        return self.game.machine

    def write(self, text: str, tag: Optional[str] = None):
        self.text.insert("end", text, tag or ())
        self.text.see("end")

    def clear(self):
        self.text.delete("1.0", "end")

    def goto_menu(self):
        self._on_exit()

    def open_book(self, section: Optional[str] = None) -> bool:
        return self._open_book(section)

    def save_game(self):
        """Write the single save slot; report result in the terminal."""
        try:
            path = save.write_save(self.snapshot())
        except OSError as exc:
            self.write(f"save failed: {exc}\n", "err")
            return
        self.dirty = False
        self.write(f"Game saved to {path}\n", "dim")

    # ---- session management ----
    @property
    def has_session(self) -> bool:
        return self.game is not None

    def start_new(self):
        """Fresh game (new machine, clean screen and history). Does not touch the save file."""
        self._kill_proc()
        self.game = Game()
        self.text.delete("1.0", "end")
        self.history = []
        self._hist_pos = 0
        self.dirty = False
        self.write(" LFS Game ", "banner")
        self.write("  Type 'tasks' to see what to do, 'help' for commands, F2 for the book.\n"
                   "Ctrl+S or 'save' saves. Esc returns to the menu.\n\n", "dim")
        self.text.insert("end", self.game.machine.fs["/dev/sda2"]["root"]["c"]["root"]["c"]
                         ["README"]["data"])
        self._new_prompt()

    def load_state(self, state: save.GameState):
        self._kill_proc()
        self.game = state.game or Game()
        self.text.delete("1.0", "end")
        self.history = list(state.history)
        self._hist_pos = len(self.history)
        self.dirty = False
        for text, tag in state.scrollback:
            self.text.insert("end", text, tag if tag in TAGS else ())
        if self.text.get("end-2c", "end-1c") not in ("", "\n"):
            self.text.insert("end", "\n")
        note = "" if state.game else " (old save: the machine was reset)"
        self.write(f"[game loaded: saved {state.saved_at}{note}]\n", "warn")
        self._new_prompt()

    def snapshot(self) -> save.GameState:
        """Current session as GameState. Scrollback = everything above the active prompt line."""
        runs = []
        active: List[str] = []
        for key, value, _ in self.text.dump("1.0", "input_start linestart", text=True, tag=True):
            if key == "tagon" and value in TAGS:
                active.append(value)
            elif key == "tagoff" and value in active:
                active.remove(value)
            elif key == "text":
                tag = active[-1] if active else None
                if runs and runs[-1][1] == tag:
                    runs[-1] = (runs[-1][0] + value, tag)
                else:
                    runs.append((value, tag))
        return save.GameState(history=list(self.history), scrollback=runs, game=self.game)

    # ---- screen lifecycle ----
    def activate(self):
        if self.game is None:
            self.start_new()
        self.text.focus_set()
        self.text.mark_set("insert", "end")

    def paste_input(self, line: str):
        """Put a command (clicked in the book) into the input line."""
        if self.game is None or self._sleep_job is not None or (self._waiting and self._waiting.secret):
            return
        self._set_input(line)
        self.text.focus_set()
        self.text.see("end")

    # ---- prompt / input ----
    def _new_prompt(self, prompt: Optional[str] = None, tag: str = "prompt"):
        if self.text.index("end-1c") != "1.0" and self.text.get("end-2c", "end-1c") != "\n":
            self.text.insert("end", "\n")
        self.text.insert("end", prompt if prompt is not None else self.machine.prompt(), tag)
        self.text.mark_set("input_start", "end-1c")
        self.text.mark_gravity("input_start", "left")
        self.text.mark_set("insert", "end")
        self.text.see("end")

    def _current_input(self) -> str:
        return self.text.get("input_start", "end-1c")

    def _set_input(self, value: str):
        self.text.delete("input_start", "end-1c")
        self.text.insert("end-1c", value)
        self.text.mark_set("insert", "end-1c")

    def _clamp_cursor(self):
        if self.text.compare("insert", "<", "input_start"):
            self.text.mark_set("insert", "end-1c")

    # ---- processes ----
    def _submit(self):
        if self._waiting is not None:
            line = "".join(self._secret) if self._waiting.secret else self._current_input()
            self._secret = []
            self.text.insert("end", "\n")
            self._waiting = None
            self._advance(line)
            return
        line = self._current_input()
        self.text.insert("end", "\n")
        if line.strip():
            if not self.history or self.history[-1] != line:
                self.history.append(line)
        self._hist_pos = len(self.history)
        self.dirty = True  # a 'save' inside this line resets it
        self._proc = start_line(self, line)
        self._advance(None)

    def _advance(self, value):
        self._sleep_job = None
        try:
            req = self._proc.send(value)
        except StopIteration:
            self._finish()
            return
        except Exception as exc:  # a bug in a command must not kill the game
            traceback.print_exc(file=sys.stderr)
            self.write(f"internal error: {exc!r}\n", "err")
            self._finish()
            return
        if isinstance(req, Sleep):
            self._sleep_job = self.after(max(0, req.ms), lambda: self._advance(None))
        elif isinstance(req, Input):
            self._waiting = req
            self._secret = []
            self._new_prompt(req.prompt, tag=())
            self._feed_typeahead()
        else:
            self._advance(None)

    def _finish(self):
        self._proc = None
        self._waiting = None
        if self.game is None:
            return
        new = self.game.check()
        for o in new:
            self.write("★ Objective complete: ", "ok")
            self.write(o.title + "\n")
        if new:
            if self.game.finished():
                self.write("\n All objectives of this version are done! ", "banner")
                self.write("\nChapters 9-11 (configuration, kernel, first boot) come in a future version. "
                           "Don't forget to save.\n", "ok")
            else:
                nxt = self.game.current()
                self.write(f"Next: {nxt.title}  (book {nxt.section})\n", "dim")
            self._on_progress()
        if self.winfo_exists():
            self._new_prompt()
            self._feed_typeahead()

    def _feed_typeahead(self):
        """Replay keys typed while busy: text up to the first newline goes into the input
        line; if a newline was typed, it is submitted (after the UI had a chance to draw)."""
        if not self._typeahead:
            return
        buf = "".join(self._typeahead)
        line, nl, rest = buf.partition("\n")
        self._typeahead = list(rest)
        if self._waiting is not None and self._waiting.secret:
            self._secret.extend(line)
        else:
            self.text.insert("end-1c", line)
        if nl:
            self.after(20, self._submit)

    def _kill_proc(self):
        if self._sleep_job is not None:
            self.after_cancel(self._sleep_job)
            self._sleep_job = None
        if self._proc is not None:
            self._proc.close()
        self._proc = None
        self._waiting = None
        self._secret = []
        self._typeahead = []

    def _interrupt(self):
        if self._proc is not None:
            self._kill_proc()
            self.write("^C\n")
            self.machine.last_status = 130
            self._finish()
        else:
            self.text.insert("end", "^C")
            self._new_prompt()

    def _complete(self):
        line = self._current_input()
        if self.text.compare("insert", "!=", "end-1c"):
            return
        insert, cands = complete(self.machine, line)
        if insert:
            self.text.insert("end-1c", insert)
        elif len(cands) > 1:
            self.text.insert("end", "\n")
            self.write("  ".join(cands) + "\n", "dim")
            self._new_prompt()
            self.text.insert("end-1c", line)

    # ---- keyboard ----
    def _on_paste(self, event):
        try:
            data = self.clipboard_get()
        except tk.TclError:
            return "break"
        if self._sleep_job is None and not (self._waiting and self._waiting.secret):
            self._clamp_cursor()
            self.text.insert("insert", data.rstrip("\n"))  # multi-line: runs on Enter
        return "break"

    def _on_key(self, event):
        key = event.keysym
        ctrl = bool(event.state & 0x4)
        if _is_fkey(key):
            return None  # let F2/F3/F4 reach the application bindings
        if ctrl and key.lower() == "c":
            if self.text.tag_ranges("sel") and self._proc is None and \
                    self.text.compare("sel.first", "<", "input_start"):
                return None  # copy a selection made in the scrollback
            self._interrupt()
            return "break"
        if key == "Escape":
            self._on_exit()
            return "break"
        if self._sleep_job is not None:  # a process is busy: buffer the keys like a tty
            if key in ("Return", "KP_Enter"):
                self._typeahead.append("\n")
            elif key == "BackSpace" and self._typeahead and self._typeahead[-1] != "\n":
                self._typeahead.pop()
            elif event.char and event.char.isprintable() and not ctrl:
                self._typeahead.append(event.char)
            return "break"
        if ctrl and key.lower() == "d":
            if self._waiting is not None and not self._current_input() and not self._secret:
                self.text.insert("end", "\n")
                self._waiting = None
                self._advance(None)
            return "break"
        if ctrl and key.lower() == "l":
            self.clear()
            pending = self._current_input()
            self._new_prompt(self._waiting.prompt if self._waiting else None,
                             () if self._waiting else "prompt")
            self.text.insert("end-1c", pending)
            return "break"
        if ctrl and key.lower() == "s":
            if self._proc is None:
                pending = self._current_input()
                self.text.insert("end", "\n")
                self.save_game()
                self._new_prompt()
                self.text.insert("end-1c", pending)
            return "break"
        if ctrl and key.lower() == "u":
            self._set_input("")
            return "break"
        if ctrl and key.lower() == "a":
            self.text.mark_set("insert", "input_start")
            return "break"
        if key in ("Return", "KP_Enter"):
            self._submit()
            return "break"
        if self._waiting is not None and self._waiting.secret:
            if key == "BackSpace":
                if self._secret:
                    self._secret.pop()
            elif event.char and event.char.isprintable() and not ctrl:
                self._secret.append(event.char)
            return "break"
        if key == "Tab":
            if self._waiting is None:
                self._complete()
            return "break"
        if key in ("Up", "Down"):
            if self._waiting is None:
                self._history_step(-1 if key == "Up" else 1)
            return "break"
        if key == "Home":
            self.text.mark_set("insert", "input_start")
            return "break"
        if key == "End":
            self.text.mark_set("insert", "end-1c")
            return "break"
        if key in ("BackSpace", "Left") and self.text.compare("insert", "<=", "input_start"):
            if not self.text.tag_ranges("sel"):
                return "break"
        if key == "BackSpace" and self.text.tag_ranges("sel") and \
                self.text.compare("sel.first", "<", "input_start"):
            self.text.tag_remove("sel", "1.0", "end")
            return "break"
        if key in ("Prior", "Next"):
            self.text.yview_scroll(-1 if key == "Prior" else 1, "pages")
            return "break"
        if ctrl:
            return None
        # typing anywhere else jumps back to the input line
        if len(event.char) == 1 or key == "Delete":
            if self.text.tag_ranges("sel") and self.text.compare("sel.first", "<", "input_start"):
                self.text.tag_remove("sel", "1.0", "end")  # never edit the scrollback
            self._clamp_cursor()
        return None

    def _history_step(self, delta: int):
        if delta < 0 and self._hist_pos > 0:
            self._hist_pos -= 1
            self._set_input(self.history[self._hist_pos])
        elif delta > 0:
            if self._hist_pos < len(self.history) - 1:
                self._hist_pos += 1
                self._set_input(self.history[self._hist_pos])
            else:
                self._hist_pos = len(self.history)
                self._set_input("")
