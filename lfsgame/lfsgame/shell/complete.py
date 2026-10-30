"""Tab completion for the simulated shell. Pure, no tkinter."""
from __future__ import annotations

import os
import re
from typing import List, Tuple

from ..sim.vfs import FsError
from .runner import REGISTRY

VAR_RE = re.compile(r"\$\{?([A-Za-z_][A-Za-z0-9_]*)\}?")


def _common_prefix(items: List[str]) -> str:
    return os.path.commonprefix(items) if items else ""


def complete(machine, line: str) -> Tuple[str, List[str]]:
    """Complete the last word of ``line`` (cursor at the end).

    Returns (text_to_insert, candidates). text_to_insert may be "" when the candidates are
    ambiguous; the caller shows the candidates in that case (when there are 2+).
    """
    m = re.search(r"[^\s;&|]*$", line)
    word = m.group(0)
    before = line[:m.start()].rstrip()
    is_cmd = before == "" or before.endswith((";", "&&", "||"))
    if is_cmd and "/" not in word:
        names = sorted(n for n, c in REGISTRY.items() if n.startswith(word) and not c.hidden)
        if len(names) == 1:
            return names[0][len(word):] + " ", names
        return _common_prefix(names)[len(word):], names

    dir_part, _, base = word.rpartition("/")
    if "/" in word:
        dir_typed = dir_part + "/"
    else:
        dir_typed = ""
    lookup_dir = VAR_RE.sub(lambda mo: machine.env.get(mo.group(1), ""), dir_typed)
    if lookup_dir.startswith("~"):
        lookup_dir = machine.env.get("HOME", "/") + lookup_dir[1:]
    if not lookup_dir:
        lookup_dir = "."
    try:
        node = machine.lookup(lookup_dir)
    except FsError:
        return "", []
    if node["t"] != "d":
        return "", []
    names = set(node["c"])
    abs_dir = machine.resolve(lookup_dir).real.rstrip("/")
    for mt in machine.mounts:
        head, _, tail = mt["path"].rpartition("/")
        if (head or "/").rstrip("/") == abs_dir and tail:
            names.add(tail)
    matches = sorted(n for n in names if n.startswith(base) and (base.startswith(".")
                                                              or not n.startswith(".")))
    if len(matches) == 1:
        name = matches[0]
        full = (lookup_dir.rstrip("/") + "/" + name) if lookup_dir != "." else name
        suffix = "/" if machine.isdir(full) else " "
        return name[len(base):] + suffix, matches
    return _common_prefix(matches)[len(base):], matches
