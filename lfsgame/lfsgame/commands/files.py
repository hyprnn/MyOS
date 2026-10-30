"""File commands: ls, cat, mkdir, touch, rm, rmdir, ln, chmod, chown, mv, cp."""
from __future__ import annotations

import copy
import re
import time

from ..shell.opts import Opts
from ..shell.runner import UsageError, command
from ..sim.vfs import FsError, human, mode_string, node_size, now

G = "files"


def _say(arg: str) -> str:
    return f"'{arg}'"


# ------------------------------------------------------------------ ls
def _entries(m, path, node):
    names = set(node["c"])
    r = m.resolve(path).path.rstrip("/")
    for mt in m.mounts:
        head, _, tail = mt["path"].rpartition("/")
        if (head or "/").rstrip("/") == r and tail:
            names.add(tail)
    return names


def _child(m, dirpath, name):
    return m.resolve(dirpath.rstrip("/") + "/" + name, follow=False).node


def _tag(node):
    if node["t"] == "d":
        return "dir"
    if node["t"] == "l":
        return "link"
    if node["t"] == "b":
        return "warn"
    if node["m"] & 0o111:
        return "ok"
    if node.get("md5"):
        return "accent"
    return None


def _long(ctx, m, name, node, human_sizes):
    if node["t"] == "b":
        size = "8, 0"
    else:
        n = node_size(node)
        size = human(n) if human_sizes else str(n)
    stamp = time.strftime("%b %d %H:%M", time.localtime(node.get("mt", now())))
    nlink = 2 + sum(1 for c in node["c"].values() if c["t"] == "d") if node["t"] == "d" else 1
    ctx.write(f"{mode_string(node)} {nlink:>2} {node['u']:<5} {node['g']:<5} {size:>8} {stamp} ")
    ctx.write(name, _tag(node))
    if node["t"] == "l":
        ctx.write(f" -> {node['to']}")
    ctx.write("\n")


def _short(ctx, items):
    for i, (name, node) in enumerate(items):
        ctx.write(name, _tag(node))
        ctx.write("  " if i < len(items) - 1 else "\n")


@command("ls", "list directory contents (-l -a -d -h)", G, aliases=("ll", "dir"))
def _ls(ctx, args):
    m = ctx.machine
    o = Opts(args, "laAdh1F")
    paths = o.args or ["."]
    status = 0
    files, dirs = [], []
    for p in paths:
        try:
            node = m.lookup(p, follow=not ("l" in o and not p.endswith("/")))
            if node["t"] == "l":
                target = m.resolve(p).node
                if target is not None and target["t"] == "d" and "l" not in o:
                    node = target
        except FsError as exc:
            ctx.write(f"ls: cannot access {_say(p)}: {exc}\n", "err")
            status = 2
            continue
        if node["t"] == "d" and "d" not in o:
            dirs.append((p, node))
        else:
            files.append((p, node))
    for name, node in files:
        if "l" in o:
            _long(ctx, m, name, node, "h" in o)
    if files and "l" not in o:
        _short(ctx, files)
    for i, (p, node) in enumerate(dirs):
        if len(paths) > 1:
            ctx.write(("\n" if i or files else "") + f"{p}:\n")
        names = sorted(_entries(m, p, node), key=lambda s: s.lower().lstrip("."))
        if "a" not in o and "A" not in o:
            names = [n for n in names if not n.startswith(".")]
        items = [(n, _child(m, p, n)) for n in names]
        items = [(n, c) for n, c in items if c is not None]
        if "a" in o:
            items = [(".", node), ("..", node)] + items
        if "l" in o:
            ctx.write(f"total {sum(node_size(c) for _, c in items) // 1024}\n")
            for n, c in items:
                _long(ctx, m, n, c, "h" in o)
        elif items:
            _short(ctx, items)
    return status


@command("cat", "print file contents", G)
def _cat(ctx, args):
    m = ctx.machine
    status = 0
    for p in args:
        try:
            node = m.lookup(p)
        except FsError as exc:
            ctx.write(f"cat: {p}: {exc}\n", "err")
            status = 1
            continue
        if node["t"] == "d":
            ctx.write(f"cat: {p}: Is a directory\n", "err")
            status = 1
        elif node["t"] == "b":
            ctx.write(f"cat: {p}: reading raw devices is not simulated\n", "err")
            status = 1
        elif node.get("md5"):
            ctx.write(f"cat: {p}: compressed tarball ({human(node['size'])}), "
                      f"contents not simulated\n", "dim")
        else:
            ctx.write(node.get("data", ""))
    if not args:
        raise UsageError("reading from stdin is not simulated; give a file name")
    return status


@command("mkdir", "make directories (-p parents, -v verbose)", G)
def _mkdir(ctx, args):
    m = ctx.machine
    o = Opts(args, "pv", "m", {"parents": "p", "verbose": "v"}, {"mode": "m"})
    if not o.args:
        raise UsageError("missing operand")
    status = 0
    for p in o.args:
        try:
            created = m.mkdir(p, parents="p" in o)
        except FsError as exc:
            ctx.write(f"mkdir: cannot create directory {_say(p)}: {exc}\n", "err")
            status = 1
            continue
        if "m" in o.vals:
            for c in created:
                m.lookup(c)["m"] = int(o.vals["m"], 8)
        if "v" in o:
            base = m.cwd.rstrip("/") + "/"
            for c in created:
                shown = c if p.startswith("/") else (c[len(base):] if c.startswith(base) else c)
                ctx.write(f"mkdir: created directory {_say(shown)}\n")
    return status


@command("touch", "create empty files / update times", G)
def _touch(ctx, args):
    m = ctx.machine
    if not args:
        raise UsageError("missing file operand")
    for p in args:
        r = m.resolve(p)
        if r.node is not None:
            r.node["mt"] = now()
        else:
            try:
                m.write_file(p, "")
            except FsError as exc:
                ctx.write(f"touch: cannot touch {_say(p)}: {exc}\n", "err")
                return 1


@command("rm", "remove files (-r recursive, -f force, -v)", G)
def _rm(ctx, args):
    m = ctx.machine
    o = Opts(args, "rRfvi", "", {"recursive": "r", "force": "f", "verbose": "v"})
    if not o.args and "f" not in o:
        raise UsageError("missing operand")
    status = 0
    for p in o.args:
        if m.abspath(p) == "/" and ("r" in o or "R" in o):
            ctx.write("rm: it is dangerous to operate recursively on '/'\n"
                      "rm: use --no-preserve-root to override this failsafe\n", "err")
            return 1
        try:
            node = m.lookup(p, follow=False)
            m.remove(p, recursive="r" in o or "R" in o)
        except FsError as exc:
            if "f" in o and str(exc) == "No such file or directory":
                continue
            msg = f"cannot remove {_say(p)}: {exc}"
            ctx.write(f"rm: {msg}\n", "err")
            status = 1
            continue
        if "v" in o:
            kind = "removed directory" if node["t"] == "d" else "removed"
            ctx.write(f"{kind} {_say(p)}\n")
    return status


@command("rmdir", "remove empty directories", G)
def _rmdir(ctx, args):
    m = ctx.machine
    for p in args:
        try:
            m.rmdir(p)
        except FsError as exc:
            ctx.write(f"rmdir: failed to remove {_say(p)}: {exc}\n", "err")
            return 1


@command("ln", "make links: ln -s TARGET LINK (-v verbose)", G)
def _ln(ctx, args):
    m = ctx.machine
    o = Opts(args, "svfn", "", {"symbolic": "s", "verbose": "v", "force": "f"})
    if "s" not in o:
        raise UsageError("hard links are not supported in this simulation (use ln -s)")
    if len(o.args) != 2:
        raise UsageError("usage: ln -s TARGET LINK_NAME")
    target, link = o.args
    if "f" in o and m.exists(link, follow=False) and m.lookup(link, follow=False)["t"] != "d":
        m.remove(link)
    try:
        m.symlink(target, link)
    except FsError as exc:
        ctx.write(f"ln: failed to create symbolic link {_say(link)}: {exc}\n", "err")
        return 1
    if "v" in o:
        ctx.write(f"{_say(link)} -> {_say(target)}\n")


SYM_RE = re.compile(r"^([ugoa]*)([-+=])([rwxXst]*)$")


def _apply_mode(old: int, spec: str, is_dir: bool) -> int:
    if re.fullmatch(r"[0-7]{1,4}", spec):
        value = int(spec, 8)
        if len(spec) <= 3 and is_dir:
            value |= old & 0o6000  # GNU chmod keeps setuid/setgid on dirs
        return value
    mode = old
    for clause in spec.split(","):
        mo = SYM_RE.match(clause)
        if not mo:
            raise FsError(f"invalid mode: {_say(spec)}")
        who, op, perms = mo.groups()
        who = who or "a"
        if "a" in who:
            who = "ugo"
        bits = 0
        for w in who:
            shift = {"u": 6, "g": 3, "o": 0}[w]
            for p in perms:
                if p in "rwx":
                    bits |= {"r": 4, "w": 2, "x": 1}[p] << shift
                elif p == "X" and (is_dir or old & 0o111):
                    bits |= 1 << shift
                elif p == "s" and w in "ug":
                    bits |= 0o4000 if w == "u" else 0o2000
        if "t" in perms:
            bits |= 0o1000
        if op == "+":
            mode |= bits
        elif op == "-":
            mode &= ~bits
        else:
            mask = 0
            for w in who:
                mask |= 0o7 << {"u": 6, "g": 3, "o": 0}[w]
            mode = (mode & ~mask) | bits
    return mode


def _walk(m, path, recursive):
    yield path
    if recursive:
        node = m.lookup(path)
        if node["t"] == "d":
            for name in sorted(node["c"]):
                child = path.rstrip("/") + "/" + name
                yield from _walk(m, child, True) if node["c"][name]["t"] == "d" else [child]


def _fmt_mode(mode):
    return f"{mode & 0o7777:04o} ({mode_string({'t': 'f', 'm': mode})[1:]})"


@command("chmod", "change permissions: chmod 755 F, chmod a+wt DIR (-v -R)", G)
def _chmod(ctx, args):
    m = ctx.machine
    # a mode like -w would look like an option; take the first non-flag as the mode
    o = Opts(args, "vRcf", "", {"verbose": "v", "recursive": "R"})
    if len(o.args) < 2:
        raise UsageError("usage: chmod MODE FILE...")
    spec, paths = o.args[0], o.args[1:]
    status = 0
    for p in paths:
        try:
            for q in _walk(m, p, "R" in o):
                node = m.lookup(q)
                if not m.is_root and node["u"] != m.user:
                    raise FsError(f"changing permissions of {_say(q)}: Operation not permitted")
                old = node["m"]
                new = _apply_mode(old, spec, node["t"] == "d")
                node["m"] = new
                if "v" in o or ("c" in o and old != new):
                    if old == new:
                        ctx.write(f"mode of {_say(q)} retained as {_fmt_mode(new)}\n")
                    else:
                        ctx.write(f"mode of {_say(q)} changed from {_fmt_mode(old)} "
                                  f"to {_fmt_mode(new)}\n")
        except FsError as exc:
            msg = str(exc)
            if "No such file" in msg:
                msg = f"cannot access {_say(p)}: {msg}"
            ctx.write(f"chmod: {msg}\n", "err")
            status = 1
    return status


@command("chown", "change owner: chown USER[:GROUP] FILE... (-v -R)", G)
def _chown(ctx, args):
    m = ctx.machine
    o = Opts(args, "vRch", "", {"verbose": "v", "recursive": "R"})
    if len(o.args) < 2:
        raise UsageError("usage: chown USER[:GROUP] FILE...")
    spec, paths = o.args[0], o.args[1:]
    user, _, group = spec.partition(":")
    if user and user not in m.users:
        raise FsError(f"invalid user: {_say(spec)}")
    if group and group not in m.groups:
        raise FsError(f"invalid group: {_say(spec)}")
    if not m.is_root:
        raise FsError(f"changing ownership of {_say(paths[0])}: Operation not permitted")
    status = 0
    for p in paths:
        try:
            for q in _walk(m, p, "R" in o):
                node = m.lookup(q)
                old = f"{node['u']}:{node['g']}" if group else node["u"]
                if user:
                    node["u"] = user
                if group:
                    node["g"] = group
                new = f"{node['u']}:{node['g']}" if group else node["u"]
                if "v" in o or ("c" in o and old != new):
                    if old == new:
                        ctx.write(f"ownership of {_say(q)} retained as {new}\n")
                    else:
                        ctx.write(f"changed ownership of {_say(q)} from {old} to {new}\n")
        except FsError as exc:
            ctx.write(f"chown: cannot access {_say(p)}: {exc}\n", "err")
            status = 1
    return status


def _copy_or_move(ctx, args, move):
    m = ctx.machine
    name = "mv" if move else "cp"
    o = Opts(args, "vrRf", "", {"verbose": "v", "recursive": "r"})
    if len(o.args) < 2:
        raise UsageError(f"usage: {name} SOURCE... DEST")
    *srcs, dest = o.args
    dest_is_dir = m.isdir(dest)
    if len(srcs) > 1 and not dest_is_dir:
        raise FsError(f"target {_say(dest)} is not a directory")
    for s in srcs:
        node = m.lookup(s, follow=not move)
        if node["t"] == "d" and not move and not ("r" in o or "R" in o):
            raise FsError(f"-r not specified; omitting directory {_say(s)}")
        target = dest.rstrip("/") + "/" + s.rstrip("/").rsplit("/", 1)[-1] if dest_is_dir else dest
        r = m.resolve(target, follow=False)
        if r.parent is None:
            raise FsError(f"cannot {'move' if move else 'copy'} to {_say(target)}")
        if r.node is not None and r.node["t"] == "d":
            raise FsError(f"cannot overwrite directory {_say(target)}")
        if not m.can_write(r.parent):
            raise FsError(f"cannot create {_say(target)}: Permission denied")
        if move:
            m.remove(s, recursive=True)
            r = m.resolve(target, follow=False)
            r.parent["c"][r.name] = node
        else:
            r.parent["c"][r.name] = copy.deepcopy(node)
        if "v" in o:
            ctx.write(f"{'renamed ' if move else ''}{_say(s)} -> {_say(target)}\n")


@command("mv", "move/rename files", G)
def _mv(ctx, args):
    _copy_or_move(ctx, args, True)


@command("cp", "copy files (-r for directories)", G)
def _cp(ctx, args):
    _copy_or_move(ctx, args, False)
