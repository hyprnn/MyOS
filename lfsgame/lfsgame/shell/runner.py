"""Command registry, program registry and the shell interpreter.

A command is ``fn(ctx, args) -> int | None | generator``.
A generator command is a *process*: it may ``yield Sleep(ms)`` (pause, the UI keeps
running) or ``line = yield Input(prompt)`` (wait for a line typed by the player; None on
Ctrl+D). Its ``return`` value is the exit status.

Commands in REGISTRY behave like programs in /usr/bin and are always found. Other names
are looked up in $PATH (or used as a path when they contain '/') in the simulated
filesystem: an executable file with a "prog" key runs PROGRAMS[prog](ctx, args, path,
node); a file starting with "#!" runs as a shell script in a subshell.

ctx API (provided by the terminal, or FakeCtx in tests):
  ctx.machine, ctx.game, ctx.history (list of str),
  ctx.write(text, tag=None), ctx.clear(), ctx.goto_menu(), ctx.save_game(),
  ctx.open_book(section_id=None)
  optional: ctx.stdin (str or None — piped/redirected input), ctx.in_script (bool)
Tags: dim, err, warn, prompt, ok, accent, dir, link.
"""
from __future__ import annotations

import inspect
from dataclasses import dataclass
from typing import Callable, Dict, Iterable, List, NamedTuple, Optional

from ..sim.vfs import FsError
from . import parser


# commands that are part of bash itself (or of the game): available even inside a chroot
CHROOT_BUILTINS = {"help", "cd", "pwd", "echo", "export", "unset", "set", "source", ".", "exec",
                   "exit", "logout", "umask", "history", "test", "[", "true", "false", "pushd",
                   "popd", "type", "time", "tasks", "status", "quest", "hint", "book", "save",
                   "menu", "version", "clear", "sleep"}
COMPILER_TOOLS = {"gcc", "g++", "cc", "c++", "cpp", "x86_64-lfs-linux-gnu-gcc",
                  "x86_64-lfs-linux-gnu-g++"}


class Cmd(NamedTuple):
    fn: Callable
    help: str
    hidden: bool
    group: str


REGISTRY: Dict[str, Cmd] = {}
PROGRAMS: Dict[str, Callable] = {}


def command(name: str, help_text: str = "", group: str = "misc", hidden: bool = False,
            aliases: Iterable[str] = ()):
    def deco(fn):
        REGISTRY[name] = Cmd(fn, help_text, hidden, group)
        for a in aliases:
            REGISTRY[a] = Cmd(fn, help_text, True, group)
        return fn
    return deco


def program(name: str):
    """Register a handler for executable files whose node has {"prog": name}."""
    def deco(fn):
        PROGRAMS[name] = fn
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


class ScriptExit(Exception):
    """'exit' inside a script: ends the script, not the session."""

    def __init__(self, status: int):
        super().__init__(status)
        self.status = status


class Sub:
    """Wraps ctx: optional output capture (pipes, >, $(...)) and stdin override."""

    def __init__(self, ctx, capture: bool = False, stdin: Optional[str] = None,
                 in_script: Optional[bool] = None):
        self._ctx = ctx
        self.capture = capture
        self.buf: List[str] = []
        self.stdin = stdin if stdin is not None else getattr(ctx, "stdin", None)
        self.in_script = in_script if in_script is not None else getattr(ctx, "in_script", False)

    def __getattr__(self, name):
        return getattr(self._ctx, name)

    def write(self, text, tag=None):
        if self.capture and tag != "err":
            self.buf.append(text)
        else:
            self._ctx.write(text, tag)

    def output(self) -> str:
        return "".join(self.buf)


# ---------------------------------------------------------------- expansion helpers
def _drive(proc):
    """Run a process synchronously: no sleeping, EOF for every Input."""
    value = None
    while True:
        try:
            proc.send(value)
        except StopIteration as stop:
            return stop.value
        value = None


def capture_output(ctx, line: str) -> str:
    sub = Sub(ctx, capture=True, in_script=True)
    _drive(_run(sub, line, interactive=False))
    return sub.output()


class Expander:
    def __init__(self, ctx):
        self.ctx = ctx
        self.m = ctx.machine

    def getvar(self, name):
        m = self.m
        if name == "?":
            return str(m.last_status)
        if name == "$":
            return "4242"
        if name == "PWD":
            return m.cwd
        return m.env.get(name, "")

    def home_of(self, user):
        m = self.m
        if user == "":
            return m.env.get("HOME", m.home())
        return m.users[user]["home"] if user in m.users else None

    def listdir(self, path):
        m = self.m
        try:
            node = m.lookup(path)
        except FsError:
            return None
        if node["t"] != "d":
            return None
        names = set(node["c"])
        ap = m.resolve(path).real.rstrip("/")
        for mt in m.mounts:  # mountpoints show up even if missing in the parent fs
            head, _, tail = mt["path"].rpartition("/")
            if (head or "/").rstrip("/") == ap and tail:
                names.add(tail)
        return list(names)

    def exists(self, path):
        return self.m.exists(path, follow=False)

    def subst(self, cmd):
        return capture_output(self.ctx, cmd)

    def __call__(self, word) -> List[str]:
        return parser.expand(word, self.getvar, self.home_of, self.listdir, self.exists,
                             self.subst)

    def heredoc(self, hd: parser.HereDoc) -> str:
        if hd.quoted:
            return hd.body or ""
        return parser.expand_heredoc(hd.body or "", self.getvar, self.subst)


# ---------------------------------------------------------------- interpreter
def start_line(ctx, text: str, interactive: bool = True):
    """Return a process (generator) that runs a command line / script text."""
    return _run(ctx, text, interactive)


def _run(ctx, text: str, interactive: bool):
    m = ctx.machine
    while True:
        try:
            tree = parser.parse(text)
            break
        except parser.Incomplete:
            more = (yield Input("> ")) if interactive else None
            if more is None:
                ctx.write("bash: syntax error: unexpected end of file\n", "err")
                m.last_status = 2
                return 2
            text += "\n" + more
        except parser.ParseError as exc:
            ctx.write(f"bash: {exc}\n", "err")
            m.last_status = 2
            return 2
    return (yield from run_list(ctx, tree))


def run_list(ctx, items):
    m = ctx.machine
    status = m.last_status
    for conn, node in items:
        if conn == "&&" and status != 0:
            continue
        if conn == "||" and status == 0:
            continue
        try:
            status = yield from run_node(ctx, node)
        except parser.ParseError as exc:
            ctx.write(f"bash: {exc}\n", "err")
            status = 1
        m.last_status = status
    return status


def run_node(ctx, node):
    m = ctx.machine
    if isinstance(node, parser.Simple):
        return (yield from _exec(ctx, node))
    if isinstance(node, parser.Pipeline):
        data = None
        status = 0
        for i, sc in enumerate(node.cmds):
            last = i == len(node.cmds) - 1
            sub = Sub(ctx, capture=not last, stdin=data if i else None)
            if i:
                sub.stdin = data
            status = yield from _exec(sub, sc)
            data = sub.output()
        if node.negate:
            status = 0 if status else 1
        return status
    if isinstance(node, parser.If):
        for cond, body in node.clauses:
            if (yield from run_list(ctx, cond)) == 0:
                return (yield from run_list(ctx, body))
        if node.else_body is not None:
            return (yield from run_list(ctx, node.else_body))
        return 0
    if isinstance(node, parser.For):
        expand = Expander(ctx)
        values: List[str] = []
        for w in node.words or []:
            values += expand(w)
        status = 0
        for v in values:
            m.env[node.var] = v
            status = yield from run_list(ctx, node.body)
        return status
    raise parser.ParseError("unknown command node")


def find_program(m, name: str):
    """-> (path, node) of an executable in the simulated fs, or None."""
    if "/" in name:
        try:
            r = m.resolve(name)
        except FsError:
            return None
        return (r.path, r.node) if r.node is not None else None
    for d in m.env.get("PATH", "").split(":"):
        path = (d or ".").rstrip("/") + "/" + name
        try:
            r = m.resolve(path)
        except FsError:
            continue
        if r.node is not None and r.node["t"] == "f" and r.node["m"] & 0o111:
            return r.path, r.node
    return None


def _exec(ctx, sc: parser.Simple):
    m = ctx.machine
    expand = Expander(ctx)
    words = list(sc.words)
    assigns = []
    while words and parser.is_assignment(words[0]):
        w = words.pop(0)
        name = parser.word_text(w).split("=", 1)[0]
        assigns.append((name, "".join(expand(w[len(name) + 1:]) or [""])))
    argv: List[str] = []
    for w in words:
        argv += expand(w)

    out_redir = None
    stdin = None
    for op, target in sc.redirs:
        if op == "<<":
            stdin = expand.heredoc(target)
            continue
        paths = expand(target)
        if len(paths) != 1:
            ctx.write(f"bash: {parser.word_text(target)}: ambiguous redirect\n", "err")
            return 1
        if op == "<":
            try:
                node = m.lookup(paths[0])
                if node["t"] == "d":
                    raise FsError("Is a directory")
            except FsError as exc:
                ctx.write(f"bash: {paths[0]}: {exc}\n", "err")
                return 1
            stdin = node.get("data", "")
        else:
            out_redir = (op, paths[0])
            if op == ">":  # bash truncates/creates the file before running the command
                try:
                    m.write_file(paths[0], "")
                except FsError as exc:
                    ctx.write(f"bash: {paths[0]}: {exc}\n", "err")
                    return 1

    if not argv:
        for name, value in assigns:
            m.env[name] = value
        return 0

    run_ctx = ctx
    if out_redir or stdin is not None:
        run_ctx = Sub(ctx, capture=out_redir is not None, stdin=stdin)
    saved = {}
    for name, value in assigns:  # VAR=value command: only for this command
        saved[name] = m.env.get(name)
        m.env[name] = value
    try:
        status = yield from run_argv(run_ctx, argv)
    finally:
        for name, old in saved.items():
            if old is None:
                m.env.pop(name, None)
            else:
                m.env[name] = old
    if out_redir is not None:
        op, path = out_redir
        try:
            m.write_file(path, run_ctx.output(), append=True)
        except FsError as exc:
            ctx.write(f"bash: {path}: {exc}\n", "err")
            return 1
    return status


def run_argv(ctx, argv: List[str]):
    """Run one already-expanded command (builtin, PATH program or script)."""
    m = ctx.machine
    name, args = argv[0], argv[1:]
    cmd = REGISTRY.get(name)
    found = None
    if m.croot and name not in CHROOT_BUILTINS:
        # inside a chroot only what is installed there exists
        found = find_program(m, name)
        if found is None:
            if "/" in name:
                ctx.write(f"bash: {name}: No such file or directory\n", "err")
            else:
                ctx.write(f"bash: {name}: command not found\n", "err")
            return 127
        node = found[1]
        base = name.rsplit("/", 1)[-1]
        if node.get("prog") in (None, "binary") and node.get("tool") not in COMPILER_TOOLS \
                and base in REGISTRY:
            cmd, found = REGISTRY[base], None   # e.g. /usr/bin/ls built in chapter 6
        else:
            cmd = None
    elif cmd is None and "/" in name:
        head, _, base = name.rpartition("/")
        if head in ("/bin", "/usr/bin", "/sbin", "/usr/sbin") and base in REGISTRY:
            cmd = REGISTRY[base]  # /bin/bash etc. are the built-in commands
    try:
        if cmd is not None:
            res = cmd.fn(ctx, args)
        else:
            found = found or find_program(m, name)
            if found is None:
                if "/" in name:
                    ctx.write(f"bash: {name}: No such file or directory\n", "err")
                else:
                    ctx.write(f"{name}: command not found (try 'help')\n", "err")
                return 127
            path, node = found
            if node["t"] == "d":
                ctx.write(f"bash: {name}: Is a directory\n", "err")
                return 126
            if not node["m"] & 0o111:
                ctx.write(f"bash: {name}: Permission denied\n", "err")
                return 126
            prog = node.get("prog")
            if prog in PROGRAMS:
                res = PROGRAMS[prog](ctx, args, path, node)
            elif node.get("data", "").startswith("#!"):
                res = run_script(ctx, node["data"], subshell=True, args=args)
            else:
                ctx.write(f"bash: {name}: cannot execute binary file: Exec format error\n",
                          "err")
                return 126
        if inspect.isgenerator(res):
            res = yield from res
        return 0 if res is None else int(res)
    except UsageError as exc:
        ctx.write(f"{name}: {exc}\n", "err")
        return 2
    except FsError as exc:
        ctx.write(f"{name}: {exc}\n", "err")
        return 1


def run_script(ctx, text: str, subshell: bool, args=()):
    """Run script text. subshell=True: env/cwd/umask changes do not survive (bash FILE)."""
    m = ctx.machine
    saved = (dict(m.env), m.cwd, m.umask, list(m.dirstack), m.user) if subshell else None
    sub = Sub(ctx, in_script=True)
    try:
        status = yield from _run(sub, text, interactive=False)
    except ScriptExit as exc:
        status = exc.status
    finally:
        if saved is not None and m.user == saved[4]:
            m.env, m.cwd, m.umask, m.dirstack = saved[0], saved[1], saved[2], saved[3]
    return status


def source_file(ctx, path: str):
    """Run a file in the current shell (like '. FILE'). Missing file -> status 1."""
    m = ctx.machine
    node = m.lookup(path)
    if node["t"] != "f":
        raise FsError(f"{path}: is not a regular file")
    return (yield from run_script(ctx, node.get("data", ""), subshell=False))


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
