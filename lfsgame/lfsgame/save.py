"""Save/load. Pure logic, no tkinter — testable headless.

One save slot: <save_dir>/save.json (+ save.json.bak = previous save).
save_dir: $LFSGAME_SAVE_DIR, else $XDG_DATA_HOME/lfsgame, else ~/.local/share/lfsgame.

Format v2: {"version": 2, "saved_at", "history": [str], "scrollback": [[text, tag|null], ...],
            "game": Game.to_dict()}
v1 saves (plain-text scrollback, no game) are migrated on read: scrollback becomes one
untagged run and game is None (the caller starts a fresh machine).
"""
from __future__ import annotations

import json
import os
import shutil
import tempfile
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import List, Optional, Tuple

from .sim.game import Game

SAVE_VERSION = 2
MAX_HISTORY = 200
MAX_SCROLLBACK_LINES = 500

Run = Tuple[str, Optional[str]]  # (text, tag)


class SaveError(Exception):
    """Save file exists but cannot be used (corrupt / newer version / bad shape)."""


@dataclass
class GameState:
    history: List[str] = field(default_factory=list)   # shell command history
    scrollback: List[Run] = field(default_factory=list)  # terminal text with color tags
    game: Optional[Game] = None                        # simulated machine + progress
    saved_at: str = ""                                 # "YYYY-MM-DD HH:MM" local time
    version: int = SAVE_VERSION

    def scrollback_text(self) -> str:
        return "".join(t for t, _ in self.scrollback)


def save_dir() -> Path:
    env = os.environ.get("LFSGAME_SAVE_DIR")
    if env:
        return Path(env)
    base = os.environ.get("XDG_DATA_HOME") or str(Path.home() / ".local" / "share")
    return Path(base) / "lfsgame"


def save_path() -> Path:
    return save_dir() / "save.json"


def trim_runs(runs: List[Run], max_lines: int = MAX_SCROLLBACK_LINES) -> List[Run]:
    """Keep only the last max_lines lines of tagged text."""
    total = sum(t.count("\n") for t, _ in runs)
    if total < max_lines:
        return [(t, g) for t, g in runs if t]
    drop = total - max_lines + 1  # newlines to skip from the start
    out: List[Run] = []
    for text, tag in runs:
        if drop > 0:
            n = text.count("\n")
            if n < drop:
                drop -= n
                continue
            idx = -1
            for _ in range(drop):
                idx = text.index("\n", idx + 1)
            text = text[idx + 1:]
            drop = 0
        if text:
            out.append((text, tag))
    return out


def _trim(state: GameState) -> GameState:
    state.history = [str(h) for h in state.history][-MAX_HISTORY:]
    state.scrollback = trim_runs(state.scrollback)
    return state


def write_save(state: GameState, path: Optional[Path] = None) -> Path:
    """Atomically write the save (tmp file + fsync + os.replace); keep previous as .bak."""
    path = Path(path) if path else save_path()
    path.parent.mkdir(parents=True, exist_ok=True)
    state = _trim(state)
    state.saved_at = time.strftime("%Y-%m-%d %H:%M")
    state.version = SAVE_VERSION
    data = {
        "version": state.version,
        "saved_at": state.saved_at,
        "history": state.history,
        "scrollback": [[t, g] for t, g in state.scrollback],
        "game": state.game.to_dict() if state.game is not None else None,
    }
    fd, tmp = tempfile.mkstemp(dir=path.parent, prefix=".save-", suffix=".tmp")
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as fh:
            json.dump(data, fh, ensure_ascii=False, separators=(",", ":"))
            fh.flush()
            os.fsync(fh.fileno())
        if path.exists():
            shutil.copy2(path, path.with_name(path.name + ".bak"))
        os.replace(tmp, path)
    except BaseException:
        try:
            os.unlink(tmp)
        except OSError:
            pass
        raise
    return path


def _runs(raw) -> List[Run]:
    if isinstance(raw, str):  # v1
        return [(raw, None)] if raw else []
    if not isinstance(raw, list):
        raise SaveError("invalid scrollback")
    out: List[Run] = []
    for item in raw:
        if (not isinstance(item, list) or len(item) != 2 or not isinstance(item[0], str)
                or not (item[1] is None or isinstance(item[1], str))):
            raise SaveError("invalid scrollback")
        out.append((item[0], item[1]))
    return out


def read_save(path: Optional[Path] = None) -> GameState:
    """Load and validate. Raises FileNotFoundError if absent, SaveError if unusable."""
    path = Path(path) if path else save_path()
    try:
        raw = json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError:
        raise
    except (OSError, ValueError) as exc:  # includes JSONDecodeError, UnicodeDecodeError
        raise SaveError(f"cannot read save: {exc}") from exc
    if not isinstance(raw, dict):
        raise SaveError("save is not a JSON object")
    version = raw.get("version")
    if not isinstance(version, int) or isinstance(version, bool):
        raise SaveError("missing/invalid version")
    if version > SAVE_VERSION:
        raise SaveError(f"save is from a newer game version ({version})")
    history = raw.get("history", [])
    saved_at = raw.get("saved_at", "")
    if not isinstance(history, list) or not all(isinstance(h, str) for h in history):
        raise SaveError("invalid history")
    if not isinstance(saved_at, str):
        raise SaveError("invalid saved_at")
    scrollback = _runs(raw.get("scrollback", []))
    game = None
    if raw.get("game") is not None:
        try:
            game = Game.from_dict(raw["game"])
        except (KeyError, TypeError, ValueError, AttributeError) as exc:
            raise SaveError(f"invalid game state: {exc!r}") from exc
    return _trim(GameState(history=history, scrollback=scrollback, game=game,
                           saved_at=saved_at, version=version))


def status(path: Optional[Path] = None) -> Tuple[str, str]:
    """('none', '') | ('ok', saved_at) | ('corrupt', reason)."""
    try:
        return "ok", read_save(path).saved_at
    except FileNotFoundError:
        return "none", ""
    except SaveError as exc:
        return "corrupt", str(exc)
