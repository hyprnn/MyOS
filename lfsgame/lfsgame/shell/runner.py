"""Command registry and the line runner.

A command is ``fn(ctx, args) -> int | None | generator``.
A generator command is a *process*: it may ``yield Sleep(ms)`` (pause, the UI keeps
running) or ``line = yield Input(prompt)`` (wait for a line typed by the player; None on
Ctrl+D). Its ``return`` value is the exit status.

ctx API (provided by the terminal, or FakeCtx in tests):
  ctx.machine, ctx.game, ctx.history (list of str),
  ctx.write(text, tag=None), ctx.clear(), ctx.goto_menu(), ctx.save_game(),
  ctx.open_book(section_id=None)
Tags: dim, err, warn, prompt, ok, accent.
"""
from __future__ import annotations

import inspect
from dataclasses import dataclass
from typing import Callable, Dict, Iterable, List, NamedTuple, Optional

from ..sim.vfs import FsError
from . import parser


class Cmd(NamedTuple):
    fn: Callable
    help: str
    hidden: bool
    group: str


REGISTRY: Dict[str, Cmd] = {}


def command(name: str, help_text: str = "", group: str = "misc", hidden: bool = False,
            aliases: Iterable[str] = ()):
    def deco(fn):
        REGISTRY[name] = Cmd(fn, help_text, hidden, group)
        for a in aliases:
            REGISTRY[a] = Cmd(fn, help_text, True, group)
        return fn
    return deco


@dataclass
class Sleep:
    ms: int


@dataclass
class Input:
    prompt: str
    secret: bool = False


class UsageError(Exception):
    """Bad options/arguments: printed as '<cmd>: <msg>', exit status 2."""


class _Capture:
    """Wraps ctx for '>' redirection: normal output goes to a buffer, errors to the screen."""

    def __init__(self, ctx):
        self._ctx = ctx
        self.buf: List[str] = []

    def __getattr__(self, name):
        return getattr(self._ctx, name)

    def write(self, text, tag=None):
        if tag == "err":
            self._ctx.write(text, tag)
        else:
            self.buf.append(text)


def _expander(m):
    def getvar(name):
        if name == "?":
            return str(m.last_status)
        if name == "$":
            return "4242"
        if name == "PWD":
            return m.cwd
        return m.env.get(name, "")

    def home_of(user):
        if user == "":
            return m.env.get("HOME", m.home())
        return m.users[user]["home"] if user in m.users else None

    def listdir(path):
        try:
            node = m.lookup(path)
        except FsError:
            return None
        if node["t"] != "d":
            return None
        names = set(node["c"])
        ap = m.resolve(path).path.rstrip("/")
        for mt in m.mounts:  # mountpoints show up even if missing in the parent fs
            head, _, tail = mt["path"].rpartition("/")
            if (head or "/").rstrip("/") == ap and tail:
                names.add(tail)
        return list(names)

    def exists(path):
        return m.exists(path, follow=False)

    return lambda word: parser.expand(word, getvar, home_of, listdir, exists)


def start_line(ctx, line: str):
    """Return a process (generator) that runs one full command line."""
    return _run(ctx, line)


def _run(ctx, line: str):
    m = ctx.machine
    try:
        cmds = parser.parse(line)
    except parser.ParseError as exc:
        ctx.write(f"bash: {exc}\n", "err")
        m.last_status = 2
        return 2
    expand = _expander(m)
    status = m.last_status
    for sc in cmds:
        if sc.conn == "&&" and status != 0:
            continue
        if sc.conn == "||" and status == 0:
            continue
        try:
            status = yield from _exec(ctx, sc, expand)
        except parser.ParseError as exc:
            ctx.write(f"bash: {exc}\n", "err")
            status = 1
        m.last_status = status
    return status


def _exec(ctx, sc: parser.Simple, expand):
    m = ctx.machine
    # leading NAME=value words: plain shell variables (stored in env for simplicity)
    words = list(sc.words)
    assigns = []
    while words and parser.is_assignment(words[0]):
        w = words.pop(0)
        text = parser.word_text(w)
        name = text.split("=", 1)[0]
        value = "".join(expand(w[len(name) + 1:]) or [""])
        assigns.append((name, value))
    argv: List[str] = []
    for w in words:
        argv += expand(w)
    if not argv:
        for name, value in assigns:
            m.env[name] = value
        if not sc.redirs:
            return 0
    if assigns and argv:
        ctx.write("bash: 'VAR=value command' is not supported; use export\n", "err")
        return 1

    out = ctx
    redir = None
    if sc.redirs:
        op, target = sc.redirs[-1]
        paths = expand(target)
        if len(paths) != 1:
            ctx.write(f"bash: {parser.word_text(target)}: ambiguous redirect\n", "err")
            return 1
        redir = (op, paths[0])
        out = _Capture(ctx)
    status = 0
    if argv:
        name, args = argv[0], argv[1:]
        cmd = REGISTRY.get(name)
        if cmd is None:
            ctx.write(f"{name}: command not found (try 'help')\n", "err")
            return 127
        try:
            res = cmd.fn(out, args)
            if inspect.isgenerator(res):
                res = yield from res
            status = 0 if res is None else int(res)
        except UsageError as exc:
            ctx.write(f"{name}: {exc}\n", "err")
            status = 2
        except FsError as exc:
            ctx.write(f"{name}: {exc}\n", "err")
            status = 1
    if redir is not None:
        op, path = redir
        try:
            m.write_file(path, "".join(out.buf), append=(op == ">>"))
        except FsError as exc:
            ctx.write(f"bash: {path}: {exc}\n", "err")
            return 1
    return status


def run_line(ctx, line: str, inputs: Optional[List[str]] = None) -> int:
    """Run a line synchronously (tests / scripts): sleeps are skipped, Input()
    requests are answered from ``inputs`` (None = EOF when they run out)."""
    proc = start_line(ctx, line)
    feed = list(inputs or [])
    value = None
    while True:
        try:
            req = proc.send(value)
        except StopIteration as stop:
            return 0 if stop.value is None else stop.value
        value = None
        if isinstance(req, Input):
            value = feed.pop(0) if feed else None
            if hasattr(ctx, "echo_input"):
                ctx.echo_input(req, value)

