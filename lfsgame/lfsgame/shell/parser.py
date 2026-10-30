"""Tokenize and expand a command line. Pure, no tkinter.

Supported: 'single' "double" \\escapes, # comments, ; && || > >>, $VAR ${VAR} $? $$,
~ and ~user, brace expansion {a,b} (nested, e.g. usr{,/*}), globs * ? [..].
Not supported (reported as errors): pipes |, <, background &, $(...), loops.

A word is kept as a list of (char, quote) pairs until expansion so we know which
characters were quoted. quote: None (bare), "'" / "\\" (literal), '"' (double), "v" (came
from a variable — not brace/glob expanded).
"""
from __future__ import annotations

import fnmatch
import re
from dataclasses import dataclass, field
from typing import List, Optional, Tuple

Chars = List[Tuple[str, Optional[str]]]
OPS = ("&&", "||", ">>", ";", "|", ">", "<", "&")
NAME_RE = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")
ASSIGN_RE = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*=")


class ParseError(Exception):
    pass


@dataclass
class Simple:
    words: List[Chars] = field(default_factory=list)
    redirs: List[Tuple[str, Chars]] = field(default_factory=list)
    conn: Optional[str] = None  # operator before this command: None, ";", "&&", "||"


def tokenize(line: str) -> List[Tuple[str, object]]:
    toks: List[Tuple[str, object]] = []
    cur: Optional[Chars] = None
    i, n = 0, len(line)

    def flush():
        nonlocal cur
        if cur is not None:
            toks.append(("word", cur))
            cur = None

    while i < n:
        c = line[i]
        if c in " \t\n":
            flush()
            i += 1
        elif c == "#" and cur is None:
            break
        elif c in ";&|<>":
            flush()
            op = line[i:i + 2] if line[i:i + 2] in OPS else c
            toks.append(("op", op))
            i += len(op)
        elif c == "'":
            j = line.find("'", i + 1)
            if j < 0:
                raise ParseError("unexpected EOF while looking for matching `''")
            cur = (cur or []) + [(ch, "'") for ch in line[i + 1:j]]
            i = j + 1
        elif c == '"':
            cur = cur or []
            i += 1
            while True:
                if i >= n:
                    raise ParseError("unexpected EOF while looking for matching `\"'")
                ch = line[i]
                if ch == '"':
                    i += 1
                    break
                if ch == "\\" and i + 1 < n and line[i + 1] in '"\\$`':
                    cur.append((line[i + 1], "\\"))
                    i += 2
                    continue
                cur.append((ch, '"'))
                i += 1
        elif c == "\\":
            cur = cur or []
            if i + 1 < n:
                cur.append((line[i + 1], "\\"))
            i += 2
        else:
            cur = (cur or []) + [(c, None)]
            i += 1
    flush()
    return toks


def parse(line: str) -> List[Simple]:
    cmds: List[Simple] = []
    cur = Simple()
    toks = tokenize(line)
    k = 0
    while k < len(toks):
        kind, val = toks[k]
        if kind == "word":
            cur.words.append(val)
        elif val in (">", ">>"):
            if k + 1 >= len(toks) or toks[k + 1][0] != "word":
                raise ParseError(f"syntax error near unexpected token `{val}'")
            cur.redirs.append((val, toks[k + 1][1]))
            k += 1
        elif val in (";", "&&", "||"):
            if not cur.words and not cur.redirs:
                raise ParseError(f"syntax error near unexpected token `{val}'")
            cmds.append(cur)
            cur = Simple(conn=val)
        elif val == "|":
            raise ParseError("pipes (|) are not supported in this simulation")
        elif val == "&":
            raise ParseError("background jobs (&) are not supported in this simulation")
        else:
            raise ParseError(f"'{val}' redirection is not supported in this simulation")
        k += 1
    if cur.words or cur.redirs:
        cmds.append(cur)
    elif cur.conn in ("&&", "||"):
        raise ParseError("syntax error: unexpected end of line")
    return cmds


# ---------------------------------------------------------------- expansion
def _brace(chars: Chars) -> List[Chars]:
    i, n = 0, len(chars)
    while i < n:
        c, q = chars[i]
        if c == "{" and q is None:
            if i > 0 and chars[i - 1] == ("$", None):  # ${VAR} is not brace expansion
                j = i
                while j < n and chars[j] != ("}", None):
                    j += 1
                i = j + 1
                continue
            depth, commas, j = 0, [], i
            while j < n:
                cj, qj = chars[j]
                if qj is None and cj == "{":
                    depth += 1
                elif qj is None and cj == "}":
                    depth -= 1
                    if depth == 0:
                        break
                elif qj is None and cj == "," and depth == 1:
                    commas.append(j)
                j += 1
            if j < n and commas:
                bounds = [i] + commas + [j]
                pre, post = chars[:i], chars[j + 1:]
                out: List[Chars] = []
                for a, b in zip(bounds, bounds[1:]):
                    out += _brace(pre + chars[a + 1:b] + post)
                return out
        i += 1
    return [chars]


def _vars(chars: Chars, getvar) -> Chars:
    out: Chars = []
    i, n = 0, len(chars)
    while i < n:
        c, q = chars[i]
        if c == "$" and q in (None, '"') and i + 1 < n and chars[i + 1][1] in (None, '"'):
            nxt = chars[i + 1][0]
            name, j = None, i + 1
            if nxt == "{":
                k = j + 1
                while k < n and chars[k][0] != "}":
                    k += 1
                if k >= n:
                    raise ParseError("bad substitution")
                name, j = "".join(ch for ch, _ in chars[j + 1:k]), k + 1
                if not NAME_RE.fullmatch(name) and name not in ("?", "$"):
                    raise ParseError(f"${{{name}}}: bad substitution")
            elif nxt in "?$":
                name, j = nxt, j + 1
            elif nxt == "(":
                raise ParseError("command substitution $(...) is not supported in this simulation")
            else:
                m = NAME_RE.match("".join(ch for ch, _ in chars[j:]))
                if m:
                    name, j = m.group(0), j + len(m.group(0))
            if name is not None:
                out += [(ch, "v") for ch in getvar(name)]
                i = j
                continue
        out.append((c, q))
        i += 1
    return out


def _tilde(chars: Chars, home_of) -> Chars:
    if not chars or chars[0] != ("~", None):
        return chars
    k = 1
    while k < len(chars) and chars[k][0] != "/" and chars[k][1] is None:
        k += 1
    user = "".join(c for c, _ in chars[1:k])
    home = home_of(user)
    if home is None:
        return chars
    return [(c, "v") for c in home] + chars[k:]


def _is_glob(chars: Chars) -> bool:
    return any(q is None and c in "*?[" for c, q in chars)


def _pattern(chars: Chars) -> str:
    out = []
    for c, q in chars:
        if q is not None and c in "*?[]":
            out.append(f"[{c}]")
        else:
            out.append(c)
    return "".join(out)


def _glob(chars: Chars, listdir) -> List[str]:
    """listdir(path) -> list of names or None if not a directory."""
    comps: List[Chars] = [[]]
    for ch in chars:
        if ch[0] == "/":
            comps.append([])
        else:
            comps[-1].append(ch)
    absolute = not comps[0]
    if absolute:
        comps = comps[1:]
    bases = ["/" if absolute else ""]
    for idx, comp in enumerate(comps):
        last = idx == len(comps) - 1
        nxt = []
        for base in bases:
            if _is_glob(comp):
                names = listdir(base or ".")
                if names is None:
                    continue
                pat = _pattern(comp)
                for name in sorted(names):
                    if name.startswith(".") and not pat.startswith("."):
                        continue
                    if fnmatch.fnmatchcase(name, pat):
                        nxt.append(_join(base, name))
            else:
                name = "".join(c for c, _ in comp)
                if name == "" and last:
                    nxt.append(base if base.endswith("/") else base + "/")
                else:
                    nxt.append(_join(base, name))
        bases = nxt
    return bases


def _join(base: str, name: str) -> str:
    if base == "":
        return name
    return base + name if base.endswith("/") else base + "/" + name


def expand(word: Chars, getvar, home_of, listdir, exists) -> List[str]:
    """Expand one word into zero or more argument strings."""
    results: List[str] = []
    quoted = any(q is not None for _, q in word)
    for alt in _brace(word):
        chars = _tilde(_vars(alt, getvar), home_of)
        if _is_glob(chars):
            matches = [p for p in _glob(chars, listdir) if exists(p)]
            if matches:
                results += matches
                continue
        s = "".join(c for c, _ in chars)
        if s == "" and not quoted:
            continue
        results.append(s)
    return results


def word_text(word: Chars) -> str:
    return "".join(c for c, _ in word)


def is_assignment(word: Chars) -> bool:
    return bool(ASSIGN_RE.match(word_text(word))) and all(
        q is None for _, q in word[:word_text(word).index("=") + 1])
