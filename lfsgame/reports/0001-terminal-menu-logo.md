# 0001 — Initial project: terminal, main menu, LFS logo
Version 0.1.0 · 2026-10-30

## Goal
User (Russian-speaking, Arch + Hyprland, hobbyist) wants a **simulation** game about
building Linux From Scratch (not a real build): terminal-style, but inside a game
window with a menu and an in-game LFS book. Must be a real multi-file project, not one
Python file. Every code change must be accompanied by a report file for another AI.
This step only: terminal + menu + LFS logo. The LFS book and actual game logic come later.

## What changed (all files new)
- `main.py` — entry, calls `lfsgame.app.run()`.
- `lfsgame/app.py` — `App(tk.Tk)`, 960x640 window; holds screens dict
  (`menu`, `terminal`), `show(name)` packs one screen and calls its `activate()`.
  Menu actions: `new_game` -> terminal, `quit` -> destroy. `continue`/`book` are disabled.
- `lfsgame/menu.py` — `MenuScreen`: read-only `tk.Text` showing logo, tagline, items
  (New game, Continue (soon), LFS Book (soon), Quit). Keys: Up/Down (also j/k), Enter, Esc=quit.
  Mouse: hover selects, click confirms. Disabled items are dimmed and ignore confirm.
- `lfsgame/terminal.py` — `TerminalScreen`: `tk.Text` shell. Prompt `root@lfs:/mnt/lfs# `.
  Mark `input_start` (gravity left) protects the prompt/output from editing. History via
  Up/Down, Home -> input start, Ctrl+L clear, Esc -> back to menu. Exposes the ctx API
  (`write(text, tag)`, `clear()`, `goto_menu()`) used by commands.
- `lfsgame/commands.py` — decorator registry `@command(name, help)`, `run_line(ctx, line)`
  using `shlex.split`. Commands: help, clear, echo, whoami, pwd, ls (placeholder),
  version, menu, exit. No tkinter import => unit-testable.
- `lfsgame/logo.py` — block-letter "LFS" + tagline.
- `lfsgame/theme.py` — dark green-on-black palette, `font()` helper (DejaVu Sans Mono 13).
- `tests/test_commands.py` — 6 unittest cases for the fake shell (all pass).
- `README.md`, `reports/README.md` (report rules/template).

## Design decisions
- **tkinter** chosen: stdlib, no pip deps (on Arch needs `sudo pacman -S tk`). User also has
  Python/GTK experience (butterfly widget), so GTK is a possible later migration, but
  tk.Text gives terminal feel with least code. Pygame rejected (extra dependency).
- Logic/UI split: commands are pure functions over a ctx object so later game logic
  (build steps, packages, scoring) can be tested without a display.
- Menu key bindings live on the Text widget itself: the widget's `<Key>` -> "break"
  would swallow toplevel bindings, so specific keys are bound on the widget (more
  specific sequence wins over `<Key>`).
- Menu is rendered by clearing/re-inserting the whole Text on each change (simple; fine at this size).

## Nuances / gotchas
- Typing handlers return "break" to stop Text's default behavior where needed; plain
  characters pass through, preceded by `_clamp_cursor()` so typing after clicking into
  old output jumps back to the input line.
- `exit`/`menu` leave the terminal session alive (hidden). `_submit` still prints a new
  prompt afterwards, and "New game" reopens that same old session. Decide later whether
  New game should reset the session.
- `ls` and `pwd` are hard-coded placeholders; no virtual filesystem yet.
- Disabled menu items are shown with "(soon)" and silently ignore Enter/click.
- Language: UI strings are English for now; the user writes Russian. Localization not designed.

## Tested vs NOT tested
- Tested: `python -m unittest` (6 commands tests pass), `py_compile` of all modules.
- NOT tested: any GUI behavior. The dev sandbox has no tkinter, so the window, menu
  navigation, mouse hover/click, prompt protection, history and Ctrl+L have never run.
  Expect possible small bugs there; first thing to do is launch it and click through.

## Known issues / limitations
- No LFS book, no game state, no save/load, no real build simulation.
- Menu click position mapping assumes one Text line per item (no wrapping).
- Fixed font family; if missing, tk falls back silently.

## Next steps (not started, not requested yet)
- LFS Book screen (reader inside the game window).
- Game state model + simulated build steps in the terminal.
- Save/Continue.

## How to run & verify
    sudo pacman -S tk && python main.py
Check: menu shows logo; arrows/mouse move selection; New game opens terminal; `help`,
`echo hi`, Up-arrow history, Ctrl+L, `exit` back to menu; Quit/Esc closes.
