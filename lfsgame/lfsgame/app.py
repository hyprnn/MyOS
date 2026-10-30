"""Main window, screen switching, side panel wiring."""
from __future__ import annotations

import tkinter as tk

from . import save, theme
from .menu import MenuScreen
from .sidepanel import SidePanel
from .terminal import TerminalScreen


class App(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("LFS Game")
        self.geometry("1100x680")
        self.minsize(720, 440)
        self.configure(bg=theme.BG)
        self.current = "menu"

        self.terminal = TerminalScreen(self, lambda: self.show("menu"), self.open_book,
                                       self._on_progress)
        self.menu = MenuScreen(self, self._on_menu_action, self._session_info)
        self.screens = {"menu": self.menu, "terminal": self.terminal}
        self.panel = SidePanel(self, on_command=self._paste_command, on_layout=self._relayout,
                               get_game=lambda: self.terminal.game,
                               on_focus_back=self._focus_screen)
        self.bind_all("<F2>", lambda e: self.toggle_panel())
        self.bind_all("<F3>", lambda e: self._book_step(-1))
        self.bind_all("<F4>", lambda e: self._book_step(1))
        self.protocol("WM_DELETE_WINDOW", self._on_close)
        self.show("menu")

    # ---- panel ----
    def toggle_panel(self):
        self.panel.toggle()
        self.screens[self.current].activate()  # give keyboard focus back to the screen

    def open_book(self, section=None) -> bool:
        if not self.panel.show_section(section):
            return False
        self.panel.open()
        return True

    def _book_step(self, delta):
        if not self.panel.is_open:
            self.panel.open()
        else:
            self.panel.step(delta)

    def _paste_command(self, line: str):
        if self.current == "terminal":
            self.terminal.paste_input(line)

    def _relayout(self, panel_open: bool):
        self._pack(self.screens[self.current], panel_open)
        if self.current == "terminal":
            self.after_idle(lambda: self.terminal.text.see("end"))

    def _focus_screen(self):
        if self.current == "terminal":
            self.terminal.text.focus_set()
        else:
            self.menu.text.focus_set()

    def _on_progress(self):
        self.panel.refresh()

    # ---- screens ----
    def _pack(self, screen, panel_open: bool):
        width = self.panel.HANDLE + (self.panel.WIDTH if panel_open else 0)
        screen.pack_configure(fill="both", expand=True, padx=(0, width))

    def show(self, name: str):
        for screen in self.screens.values():
            screen.pack_forget()
        screen = self.screens[name]
        screen.pack(fill="both", expand=True)
        self._pack(screen, self.panel.is_open)
        self.current = name
        screen.activate()
        self.panel.lift()

    def _session_info(self):
        return self.terminal.has_session, self.terminal.has_session and self.terminal.dirty

    def _on_menu_action(self, action: str):
        has_session, dirty = self._session_info()
        if action == "resume":
            self.show("terminal")
        elif action == "new_game":
            if dirty:
                self.menu.ask("Start a new game? Unsaved progress will be lost.", "new_game!")
                return
            self._new_game()
        elif action == "new_game!":
            self._new_game()
        elif action == "continue":
            if dirty:
                self.menu.ask("Load the save? Unsaved progress will be lost.", "continue!")
                return
            self._load()
        elif action == "continue!":
            self._load()
        elif action == "book":
            self.panel.show_toc()
            self.panel.open()
        elif action == "quit":
            if dirty:
                self.menu.ask("Quit without saving?", "quit!")
                return
            self.destroy()
        elif action == "quit!":
            self.destroy()

    def _new_game(self):
        self.terminal.start_new()
        self.show("terminal")
        self.panel.show_section("0")

    def _load(self):
        try:
            state = save.read_save()
        except (save.SaveError, FileNotFoundError):
            self.menu.activate()  # refresh label to show the problem
            return
        self.terminal.load_state(state)
        self.show("terminal")
        self.panel.show_section(self.terminal.game.book_section)

    def _on_close(self):
        if self._session_info()[1]:
            self.show("menu")
            self.menu.ask("Quit without saving?", "quit!")
        else:
            self.destroy()


def run():
    App().mainloop()
