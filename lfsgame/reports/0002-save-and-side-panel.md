# 0002 — Save/Continue, slide-out LFS side panel, CONTEXT.txt
Version 0.2.0 · 2026-10-30 · builds on 0001

## Goal (user's words, Russian, paraphrased)
"Add saving, and a placeholder LFS panel that slides out from the side; also save all
context in a txt file for another AI." Rule from 0001 still applies: every code change
gets a report; CONTEXT.txt is the handoff file and must be kept up to date too.

## What changed
NEW
- `lfsgame/save.py` — pure logic. `GameState(history, scrollback, saved_at, version)`.
  `write_save` (atomic: mkstemp in same dir -> fsync -> copy old to `save.json.bak` -> os.replace),
  `read_save` (validates; raises `FileNotFoundError` or `SaveError`), `status()` ->
  ('none'|'ok'|'corrupt', info). Single slot `save.json`. Dir: `$LFSGAME_SAVE_DIR`, else
  `$XDG_DATA_HOME/lfsgame`, else `~/.local/share/lfsgame`. Limits: 200 history entries,
  500 scrollback lines (older dropped on write and on read). SAVE_VERSION = 1; newer
  versions are refused (no migration code yet).
- `lfsgame/sidepanel.py` — `SidePanel(tk.Frame)`: right-edge slide-out, 320px body + 28px
  handle strip. Uses `place(relx=1.0, anchor="nw", x=offset)`; collapsed x=-28, open
  x=-348. 14-step ease-out-cubic animation via `after(12ms)`; re-toggling mid-animation
  cancels the running job and animates from the current x. Handle label (◀/▶ + "LFS")
  is clickable. Content is a placeholder ("LFS BOOK / placeholder / Coming soon").
- `tests/test_save.py` — 9 tests (roundtrip with Cyrillic, .bak, no tmp leftovers,
  corrupt JSON, wrong shapes, newer version, trimming, env/XDG dir resolution).
- `CONTEXT.txt` — handoff context for another AI (project root).

MODIFIED
- `lfsgame/terminal.py` — no more welcome text in `__init__`; new `start_new()`,
  `load_state(state)`, `snapshot()`, `save_game()`; Ctrl+S saves. Snapshot scrollback =
  text from 1.0 to `input_start linestart` (everything above the active prompt line, so
  the `save` command line itself is NOT in the saved scrollback).
- `lfsgame/menu.py` — items now built per instance in `refresh_items()` from
  `save.status()`: "Continue (saved YYYY-MM-DD HH:MM)" enabled; "(no save)" or
  "(save unreadable)" disabled. "LFS Book" is now ENABLED and opens the side panel
  (my decision, previously "(soon)"). `activate()` re-reads save status each time the
  menu is shown. `<Key>` swallow handler now lets F-keys through (see nuances).
- `lfsgame/app.py` — creates `SidePanel`, `bind_all("<F2>")` toggles it; screens packed
  with `padx=(0, 28)` so the collapsed handle never overlaps content; `panel.lift()`
  after every screen switch. Menu actions: `new_game` -> `terminal.start_new()` then
  show; `continue` -> `read_save()` + `terminal.load_state()`; `book` -> `panel.open()`.
- `lfsgame/commands.py` — new `save` command (calls `ctx.save_game()`); ctx API doc updated.
- `lfsgame/theme.py` — `PANEL_BG`. `lfsgame/__init__.py` — version 0.2.0.
- `tests/test_commands.py` — FakeCtx has `save_game`; added `test_save_command`.

## Design decisions / rejected alternatives
- JSON, one slot, human-readable, versioned. SQLite rejected (overkill). Multiple slots
  not requested.
- Atomic write + `.bak` so a crash mid-save cannot destroy the previous save.
- Saved state is only history + plain scrollback for now, because the game has no real
  state yet. When simulation state exists (current LFS chapter, built packages, ...),
  add fields and bump SAVE_VERSION with a migration in `read_save`.
- Panel as an overlay (place) rather than a split pane: slides over content, no relayout
  of the terminal while animating.
- Manual save only (command / Ctrl+S). No autosave, no save on quit.

## Nuances / gotchas
- F2 must reach `bind_all`. Tk bindtags are widget -> class -> toplevel -> all, and a
  handler returning "break" stops the chain. That is why the menu's catch-all `<Key>`
  handler returns None for F1..F12, and the terminal's handler falls through (None) for
  keys it doesn't consume.
- Scrollback is restored as plain text: colors (err/warn/dim tags) are lost after load.
  The "[game loaded: saved ...]" line is shown in the warn color.
- New game DROPS the in-memory session without confirmation (the save file is untouched
  until the user saves). Leaving via `exit`/`menu` keeps the session in memory but
  "New game" resets it; "Continue" replaces it from the file.
- Corrupt save: Continue is disabled and labeled; file is left on disk (not deleted).
  `.bak` is never read automatically.
- `snapshot()` uses the Text mark `input_start`; mark gravity is left. If prompt logic is
  changed, re-check `snapshot()`.
- Window width < ~400px: handle strip + panel width assumptions are fixed pixels.
- Panel focus: after F2 the current screen's `activate()` is called to restore keyboard
  focus (menu re-renders, terminal moves cursor to end).

## Tested vs NOT tested
- Tested headless: all of `save.py` (16 tests total pass), `save` command wiring via
  FakeCtx, `py_compile` of every module.
- NOT tested: ANY tkinter code (sandbox has no tkinter): panel animation and placement,
  handle click, F2 toggle from menu/terminal, Ctrl+S, snapshot/load_state round trip in
  a real Text widget, Continue flow, padx reservation. First thing to do: run it.
  Likely trouble spots: `place` stacking/`lift`, F2 propagation, `text.get(...,
  "input_start linestart")`.

## Known issues / limitations
- Panel content is a placeholder; menu "LFS Book" only opens it.
- No save-slot selection, no delete-save UI, no confirm on New game.
- Fixed pixel sizes; no HiDPI scaling work.

## Next steps (not requested yet)
- Real LFS book reader inside the panel. Game state + simulated build steps.
- Real state in GameState + SAVE_VERSION migration.

## How to run & verify
    sudo pacman -S tk && python main.py      # tests: python -m unittest -v
Manual checks: menu shows "Continue (no save)" greyed; New game -> `echo hi`, `save`;
Esc -> menu now shows "Continue (saved ...)"; Continue restores text and Up-arrow
history; F2 or the LFS handle slides the panel in/out; menu "LFS Book" opens it.
Save file location: `~/.local/share/lfsgame/save.json`.
