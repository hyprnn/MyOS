"""Tokenize, parse and expand shell input. Pure, no tkinter.

Supported: 'single' "double" \\escapes and \\<newline> continuation, # comments,
; && || | newlines, > >> < 2> 2>> 2>&1 &> redirections, here-documents (<< and <<-, quoted or not),
$VAR ${VAR} $? $$, $(...) and `...` command substitution, ~ and ~user,
brace expansion {a,b} (nested) and {1..8}, globs * ? [..], word splitting of unquoted expansions,
compound commands: if/elif/else/fi, for NAME in WORDS; do ...; done, ! pipeline.
Not supported: case, while/until, functions, subshells ( ), background &.

Incomplete input (open quote, trailing backslash or |/&&, missing fi/done, here-doc
without its terminator) raises ``Incomplete`` so an interactive shell can ask for more
lines with a "> " prompt, like bash does.

A word is kept as a list of (text, quote) pairs until expansion. quote: None (bare),
"'" / "\\" (literal), '"' (double quoted), "S"/"SQ" (command substitution outside/inside double quotes, text = command),
"v" (expansion result, quoted), "u" (expansion result, unquoted: subject to splitting).
"""
from __future__ import annotations

import fnmatch
import re
from dataclasses import dataclass, field
from typing import List, Optional, Tuple, Union

Chars = List[Tuple[str, Optional[str]]]
NAME_RE = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")
ASSIGN_RE = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*=")
OPCHARS = ";&|<>"
RESERVED = {"then", "elif", "else", "fi", "do", "done"}


class ParseError(Exception):
    pass


class Incomplete(ParseError):
    """More input is needed (bash would show the PS2 '> ' prompt)."""


@dataclass
class HereDoc:
    delim: str
    quoted: bool
    strip_tabs: bool
    body: Optional[str] = None


@dataclass
class Simple:
    words: List[Chars] = field(default_factory=list)
    redirs: List[Tuple[str, Union[Chars, HereDoc]]] = field(default_factory=list)


@dataclass
class Pipeline:
    cmds: List[Simple]
    negate: bool = False


@dataclass
class If:
    clauses: list            # [(cond CmdList, body CmdList), ...]
    else_body: Optional[list] = None


@dataclass
class For:
    var: str
    words: Optional[List[Chars]]
    body: list


Node = Union[Simple, Pipeline, If, For]
CmdList = List[Tuple[Optional[str], Node]]   # (connector before: None ; && ||, node)


# ---------------------------------------------------------------- lexer
class _Lexer:
    def __init__(self, text: str):
        self.s = text
        self.i = 0
        self.toks: list = []
        self.pending: List[HereDoc] = []

    def run(self):
        s, n = self.s, len(self.s)
        while self.i < n:
            c = s[self.i]
            if c == "\\" and s[self.i + 1:self.i + 2] == "\n":
                self.i += 2
            elif c in " \t\r":
                self.i += 1
            elif c == "\n":
                self.toks.append(("op", "\n"))
                self.i += 1
                if self.pending:
                    self._bodies()
            elif c == "#":
                while self.i < n and s[self.i] != "\n":
                    self.i += 1
            elif c == "2" and s[self.i + 1:self.i + 2] == ">" and \
                    (self.i == 0 or s[self.i - 1] in " \t\n;&|"):
                op = "2>&1" if s.startswith("2>&1", self.i) else \
                    "2>>" if s.startswith("2>>", self.i) else "2>"
                self.toks.append(("op", op))
                self.i += len(op)
            elif c in OPCHARS:
                self._operator()
            else:
                self.toks.append(("word", self._word()))
        if self.pending:
            raise Incomplete("here-document")
        return self.toks

    def _operator(self):
        s = self.s
        for op in ("<<-", "&>", "&&", "||", ">>", "<<", ";;", ";", "|", ">", "<", "&"):
            if s.startswith(op, self.i):
                break
        self.i += len(op)
        if op in ("<<", "<<-"):
            while self.i < len(s) and s[self.i] in " \t":
                self.i += 1
            if self.i >= len(s) or s[self.i] in "\n" + OPCHARS:
                raise ParseError("syntax error near unexpected token `newline'")
            w = self._word()
            hd = HereDoc(word_text(w), any(q is not None for _, q in w), op == "<<-")
            self.pending.append(hd)
            self.toks.append(("op", "<<", hd))
        elif op == ";;":
            raise ParseError("syntax error near unexpected token `;;' (case is not supported)")
        else:
            self.toks.append(("op", op))

    def _bodies(self):
        s = self.s
        for hd in self.pending:
            lines = []
            while True:
                if self.i >= len(s):
                    raise Incomplete("here-document")
                j = s.find("\n", self.i)
                line = s[self.i:] if j < 0 else s[self.i:j]
                self.i = len(s) if j < 0 else j + 1
                check = line.lstrip("\t") if hd.strip_tabs else line
                if check == hd.delim:
                    break
                lines.append(check)
            hd.body = "".join(ln + "\n" for ln in lines)
        self.pending = []

    def _subst(self, start: int, close: str) -> int:
        """Return the index of the closing ')' or '`' for a substitution starting at start."""
        s, depth, j = self.s, 1, start
        while j < len(s):
            c = s[j]
            if c == "\\":
                j += 2
                continue
            if close == ")":
                if c == "'":
                    k = s.find("'", j + 1)
                    if k < 0:
                        break
                    j = k + 1
                    continue
                if c == "(":
                    depth += 1
                elif c == ")":
                    depth -= 1
                    if depth == 0:
                        return j
            elif c == "`":
                return j
            j += 1
        raise Incomplete("command substitution")

    def _word(self) -> Chars:
        s, n = self.s, len(self.s)
        out: Chars = []
        while self.i < n:
            c = s[self.i]
            if c in " \t\r\n" or c in OPCHARS:
                break
            if c == "\\":
                if self.i + 1 >= n:
                    raise Incomplete("backslash")
                if s[self.i + 1] != "\n":
                    out.append((s[self.i + 1], "\\"))
                self.i += 2
            elif c == "'":
                j = s.find("'", self.i + 1)
                if j < 0:
                    raise Incomplete("quote")
                out += [(ch, "'") for ch in s[self.i + 1:j]]
                if j == self.i + 1:
                    out.append(("", "'"))  # keeps '' as an (empty) quoted word
                self.i = j + 1
            elif c == '"':
                self.i += 1
                start_len = len(out)
                while True:
                    if self.i >= n:
                        raise Incomplete("quote")
                    ch = s[self.i]
                    if ch == '"':
                        self.i += 1
                        break
                    if ch == "\\" and self.i + 1 < n and s[self.i + 1] in '"\\$`\n':
                        if s[self.i + 1] != "\n":
                            out.append((s[self.i + 1], "\\"))
                        self.i += 2
                    elif ch == "$" and s[self.i + 1:self.i + 2] == "(":
                        j = self._subst(self.i + 2, ")")
                        out.append((s[self.i + 2:j], "SQ"))
                        self.i = j + 1
                    elif ch == "`":
                        j = self._subst(self.i + 1, "`")
                        out.append((s[self.i + 1:j], "SQ"))
                        self.i = j + 1
                    else:
                        out.append((ch, '"'))
                        self.i += 1
                if len(out) == start_len:
                    out.append(("", '"'))
            elif c == "$" and s[self.i + 1:self.i + 2] == "(":
                j = self._subst(self.i + 2, ")")
                out.append((s[self.i + 2:j], "S"))
                self.i = j + 1
            elif c == "`":
                j = self._subst(self.i + 1, "`")
                out.append((s[self.i + 1:j], "S"))
                self.i = j + 1
            else:
                out.append((c, None))
                self.i += 1
        return out


def tokenize(text: str) -> list:
    return _Lexer(text).run()


# ---------------------------------------------------------------- parser
class _Parser:
    def __init__(self, toks):
        self.t = toks
        self.k = 0

    def peek(self):
        return self.t[self.k] if self.k < len(self.t) else None

    def kw(self) -> Optional[str]:
        tok = self.peek()
        if tok and tok[0] == "word" and tok[1] and all(q is None for _, q in tok[1]):
            return word_text(tok[1])
        return None

    def is_op(self, *ops) -> bool:
        tok = self.peek()
        return tok is not None and tok[0] == "op" and tok[1] in ops

    def skip_newlines(self):
        while self.is_op("\n"):
            self.k += 1

    def parse_list(self, stops=frozenset()) -> Tuple[CmdList, Optional[str]]:
        items: CmdList = []
        conn: Optional[str] = None
        self.skip_newlines()
        while True:
            tok = self.peek()
            if tok is None:
                if stops or conn in ("&&", "||"):
                    raise Incomplete("more input")
                return items, None
            kw = self.kw()
            if kw in stops:
                if not items:
                    raise ParseError(f"syntax error near unexpected token `{kw}'")
                return items, kw
            if tok[0] == "op" and tok[1] not in (">", ">>", "<", "<<", "2>", "2>>", "&>"):
                raise ParseError(f"syntax error near unexpected token `{_tok_text(tok)}'")
            items.append((conn, self.parse_command()))
            conn = None
            tok = self.peek()
            if tok is None:
                continue
            if tok[0] == "op" and tok[1] in (";", "\n"):
                self.k += 1
                conn = ";"
                self.skip_newlines()
            elif tok[0] == "op" and tok[1] in ("&&", "||"):
                self.k += 1
                conn = tok[1]
                self.skip_newlines()
            elif tok[0] == "op" and tok[1] == "&":
                raise ParseError("background jobs (&) are not supported in this simulation")
            else:
                raise ParseError(f"syntax error near unexpected token `{_tok_text(tok)}'")

    def parse_command(self) -> Node:
        kw = self.kw()
        if kw == "if":
            return self.parse_if()
        if kw == "for":
            return self.parse_for()
        if kw in RESERVED:
            raise ParseError(f"syntax error near unexpected token `{kw}'")
        if kw in ("case", "while", "until", "function", "select"):
            raise ParseError(f"'{kw}' is not supported in this simulation (if and for are)")
        negate = False
        if kw == "!":
            self.k += 1
            negate = True
        cmds = [self.parse_simple()]
        while self.is_op("|"):
            self.k += 1
            self.skip_newlines()
            if self.peek() is None:
                raise Incomplete("|")
            cmds.append(self.parse_simple())
        if len(cmds) == 1 and not negate:
            return cmds[0]
        return Pipeline(cmds, negate)

    def parse_simple(self) -> Simple:
        sc = Simple()
        while True:
            tok = self.peek()
            if tok is None:
                break
            if tok[0] == "word":
                sc.words.append(tok[1])
                self.k += 1
            elif tok[1] == "2>&1":
                sc.redirs.append(("2>&1", []))
                self.k += 1
            elif tok[1] in (">", ">>", "<", "2>", "2>>", "&>"):
                self.k += 1
                nxt = self.peek()
                if nxt is None or nxt[0] != "word":
                    raise ParseError(f"syntax error near unexpected token `"
                                     f"{_tok_text(nxt) if nxt else 'newline'}'")
                sc.redirs.append((tok[1], nxt[1]))
                self.k += 1
            elif tok[1] == "<<":
                sc.redirs.append(("<<", tok[2]))
                self.k += 1
            else:
                break
        if not sc.words and not sc.redirs:
            tok = self.peek()
            raise ParseError(f"syntax error near unexpected token `"
                             f"{_tok_text(tok) if tok else 'newline'}'")
        return sc

    def expect(self, word: str):
        if self.peek() is None:
            raise Incomplete(word)
        if self.kw() != word:
            raise ParseError(f"syntax error near unexpected token `{_tok_text(self.peek())}'")
        self.k += 1

    def parse_if(self) -> If:
        self.k += 1
        clauses = []
        cond, _ = self.parse_list({"then"})
        self.expect("then")
        body, kw = self.parse_list({"elif", "else", "fi"})
        clauses.append((cond, body))
        else_body = None
        while kw == "elif":
            self.k += 1
            cond, _ = self.parse_list({"then"})
            self.expect("then")
            body, kw = self.parse_list({"elif", "else", "fi"})
            clauses.append((cond, body))
        if kw == "else":
            self.k += 1
            else_body, kw = self.parse_list({"fi"})
        self.expect("fi")
        return If(clauses, else_body)

    def parse_for(self) -> For:
        self.k += 1
        tok = self.peek()
        if tok is None:
            raise Incomplete("for")
        name = self.kw()
        if tok[0] != "word" or not name or not NAME_RE.fullmatch(name):
            raise ParseError(f"`{_tok_text(tok)}': not a valid identifier")
        self.k += 1
        words: Optional[List[Chars]] = None
        self.skip_newlines()
        if self.kw() == "in":
            self.k += 1
            words = []
            while self.peek() is not None and self.peek()[0] == "word":
                words.append(self.peek()[1])
                self.k += 1
        if self.peek() is None:
            raise Incomplete("for")
        if self.is_op(";", "\n"):
            self.k += 1
        self.skip_newlines()
        self.expect("do")
        body, _ = self.parse_list({"done"})
        self.expect("done")
        return For(name, words, body)


def _tok_text(tok) -> str:
    if tok is None:
        return "newline"
    if tok[0] == "word":
        return word_text(tok[1])
    return "newline" if tok[1] == "\n" else tok[1]


def parse(text: str) -> CmdList:
    p = _Parser(tokenize(text))
    items, _ = p.parse_list()
    return items


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
            if j < n and all(q is None for _, q in chars[i + 1:j]):
                inner = "".join(c for c, _ in chars[i + 1:j])
                seq = re.fullmatch(r"(-?\d+)\.\.(-?\d+)|([a-zA-Z])\.\.([a-zA-Z])", inner)
                if seq:
                    if seq.group(1) is not None:
                        a, b = int(seq.group(1)), int(seq.group(2))
                        items = [str(k) for k in (range(a, b + 1) if a <= b else range(a, b - 1, -1))]
                    else:
                        a, b = ord(seq.group(3)), ord(seq.group(4))
                        items = [chr(k) for k in (range(a, b + 1) if a <= b else range(a, b - 1, -1))]
                    pre, post = chars[:i], chars[j + 1:]
                    out = []
                    for item in items:
                        out += _brace(pre + [(c, None) for c in item] + post)
                    return out
        i += 1
    return [chars]


def _vars(chars: Chars, getvar, subst) -> Chars:
    out: Chars = []
    i, n = 0, len(chars)
    while i < n:
        c, q = chars[i]
        if q in ("S", "SQ"):
            out += [(ch, "u" if q == "S" else "v") for ch in subst(c).rstrip("\n")]
            i += 1
            continue
        if c == "$" and q in (None, '"') and i + 1 < n and chars[i + 1][1] == q:
            mark = "u" if q is None else "v"
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
            else:
                m = NAME_RE.match("".join(ch for ch, _ in chars[j:]))
                if m:
                    name, j = m.group(0), j + len(m.group(0))
            if name is not None:
                out += [(ch, mark) for ch in getvar(name)]
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


def _split(chars: Chars) -> List[Chars]:
    fields: List[Chars] = [[]]
    for c, q in chars:
        if q == "u" and c in " \t\n":
            if fields[-1]:
                fields.append([])
            continue
        fields[-1].append((c, q))
    return [f for f in fields if f]


def _is_glob(chars: Chars) -> bool:
    return any(q is None and c in "*?[" for c, q in chars)


def _pattern(chars: Chars) -> str:
    return "".join(f"[{c}]" if q is not None and c in "*?[]" else c for c, q in chars)


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


def expand(word: Chars, getvar, home_of, listdir, exists, subst=None) -> List[str]:
    """Expand one word into zero or more argument strings."""
    subst = subst or (lambda cmd: "")
    results: List[str] = []
    quoted = any(q is not None and q != "S" for _, q in word)
    for alt in _brace(word):
        chars = _tilde(_vars(alt, getvar, subst), home_of)
        fields = _split(chars)
        if not fields:
            if quoted:
                results.append("")
            continue
        for f in fields:
            if _is_glob(f):
                matches = [p for p in _glob(f, listdir) if exists(p)]
                if matches:
                    results += matches
                    continue
            results.append("".join(c for c, _ in f))
    return results


def expand_heredoc(body: str, getvar, subst) -> str:
    """Expansion inside an unquoted here-document: $VAR ${VAR} $(cmd) `cmd` and \\$ \\` \\\\."""
    out = []
    i, n = 0, len(body)
    while i < n:
        c = body[i]
        if c == "\\" and i + 1 < n and body[i + 1] in "$`\\":
            out.append(body[i + 1])
            i += 2
        elif c == "$" and body[i + 1:i + 2] == "(":
            lx = _Lexer(body)
            j = lx._subst(i + 2, ")")
            out.append(subst(body[i + 2:j]).rstrip("\n"))
            i = j + 1
        elif c == "`":
            j = body.find("`", i + 1)
            if j < 0:
                out.append(c)
                i += 1
                continue
            out.append(subst(body[i + 1:j]).rstrip("\n"))
            i = j + 1
        elif c == "$" and body[i + 1:i + 2] == "{":
            j = body.find("}", i)
            out.append(getvar(body[i + 2:j]) if j > 0 else "$")
            i = j + 1 if j > 0 else i + 1
        elif c == "$":
            m = NAME_RE.match(body, i + 1)
            if m:
                out.append(getvar(m.group(0)))
                i = m.end()
            elif body[i + 1:i + 2] in ("?", "$"):
                out.append(getvar(body[i + 1]))
                i += 2
            else:
                out.append(c)
                i += 1
        else:
            out.append(c)
            i += 1
    return "".join(out)


def word_text(word: Chars) -> str:
    return "".join(c for c, q in word if q not in ("S", "SQ"))


def is_assignment(word: Chars) -> bool:
    text = "".join(c if q not in ("S", "SQ") else "\0" for c, q in word)
    m = ASSIGN_RE.match(text)
    return bool(m) and all(q is None for _, q in word[:m.end()])
