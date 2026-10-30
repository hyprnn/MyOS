"""Virtual filesystem nodes. Nodes are plain dicts so the whole machine is JSON-serializable.

  dir:   {"t": "d", "m": mode, "u": owner, "g": group, "mt": mtime, "c": {name: node}}
  file:  {"t": "f", "m": mode, "u": owner, "g": group, "mt": mtime, "data": str, "size": int,
          optional "md5": str (downloaded tarballs carry a fake checksum instead of data)}
  link:  {"t": "l", "m": 0o777, "u": owner, "g": group, "mt": mtime, "to": target}
  block: {"t": "b", ...}   device nodes in /dev (regenerated, never edited by commands)
"""
from __future__ import annotations

import hashlib
import posixpath
import time
from typing import List


class FsError(Exception):
    """Error shown to the player as '<command>: <message>'."""


def now() -> float:
    return round(time.time())


def mkdir_node(mode=0o755, user="root", group="root") -> dict:
    return {"t": "d", "m": mode, "u": user, "g": group, "mt": now(), "c": {}}


def file_node(data="", mode=0o644, user="root", group="root", size=None, md5=None) -> dict:
    node = {"t": "f", "m": mode, "u": user, "g": group, "mt": now(), "data": data,
            "size": len(data.encode("utf-8")) if size is None else size}
    if md5 is not None:
        node["md5"] = md5
    return node


def link_node(target: str, user="root", group="root") -> dict:
    return {"t": "l", "m": 0o777, "u": user, "g": group, "mt": now(), "to": target}


def node_md5(node: dict) -> str:
    if "md5" in node:
        return node["md5"]
    return hashlib.md5(node.get("data", "").encode("utf-8")).hexdigest()


def node_size(node: dict) -> int:
    if node["t"] == "d":
        return 4096
    if node["t"] == "l":
        return len(node["to"])
    return node.get("size", 0)


def tree_size(node: dict) -> int:
    if node["t"] != "d":
        return node_size(node)
    return 4096 + sum(tree_size(ch) for ch in node["c"].values())


def split(path: str) -> List[str]:
    return [p for p in path.split("/") if p]


def normpath(path: str) -> str:
    """Lexical normalization of an absolute path ('..' handled lexically)."""
    p = posixpath.normpath(path)
    return "/" + p.lstrip("/") if p != "/" else "/"


def mode_string(node: dict) -> str:
    kind = {"d": "d", "f": "-", "l": "l", "b": "b"}[node["t"]]
    m = node["m"]
    out = []
    for shift, special, ch in ((6, 0o4000, "s"), (3, 0o2000, "s"), (0, 0o1000, "t")):
        bits = (m >> shift) & 7
        r = "r" if bits & 4 else "-"
        w = "w" if bits & 2 else "-"
        x = "x" if bits & 1 else "-"
        if m & special:
            x = ch if bits & 1 else ch.upper()
        out.append(r + w + x)
    return kind + "".join(out)


def human(n: float, base=1024.0, units="BKMGTP") -> str:
    """lsblk/df style: 512M, 9.5G, 32G."""
    i = 0
    while n >= base and i < len(units) - 1:
        n /= base
        i += 1
    if i == 0:
        return f"{int(n)}{units[0]}"
    s = f"{n:.1f}"
    if s.endswith(".0") or n >= 100:
        s = f"{n:.0f}"
    return s + units[i]
