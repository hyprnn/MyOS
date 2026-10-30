"""Text tools that work on files or piped input: grep, wc, head, tail, sed, find."""
from __future__ import annotations

import fnmatch
import re

from ..shell.opts import Opts
from ..shell.runner import UsageError, command
from ..sim.vfs import FsError

G = "text"


def _inputs(ctx, files, name):
    """Yield (label, text) for each file, or the piped input when no files are given."""
    m = ctx.machine
    if not files or files == ["-"]:
        yield "(standard input)", getattr(ctx, "stdin", None) or ""
        return
    for f in files:
        try:
            node = m.lookup(f)
        except FsError as exc:
            ctx.write(f"{name}: {f}: {exc}\n", "err")
            continue
        if node["t"] == "d":
            ctx.write(f"{name}: {f}: Is a directory\n", "err")
            continue
        yield f, node.get("data", "")


def _bre(pattern: str, extended: bool) -> str:
    """Basic regular expression -> Python regex (good enough for LFS commands)."""
    if extended:
        return pattern
    out, i = [], 0
    while i < len(pattern):
        c = pattern[i]
        if c == "\\" and i + 1 < len(pattern):
            n = pattern[i + 1]
            out.append(n if n in "(){}|+?" else "\\" + n)
            i += 2
            continue
        out.append("\\" + c if c in "(){}|+?" else c)
        i += 1
    return "".join(out)


@command("grep", "print matching lines (-i -v -c -n -E -q)", G)
def _grep(ctx, args):
    o = Opts(args, "ivcnEqFl")
    if not o.args:
        raise UsageError("usage: grep [OPTION]... PATTERNS [FILE]...")
    pat, files = o.args[0], o.args[1:]
    flags = re.I if "i" in o else 0
    try:
        rx = re.compile(re.escape(pat) if "F" in o else _bre(pat, "E" in o), flags)
    except re.error as exc:
        raise UsageError(f"invalid regular expression: {exc}")
    found = False
    for label, text in _inputs(ctx, files, "grep"):
        count = 0
        for n, line in enumerate(text.splitlines(), 1):
            if bool(rx.search(line)) != ("v" in o):
                count += 1
                found = True
                if "q" in o or "c" in o or "l" in o:
                    continue
                prefix = f"{label}:" if len(files) > 1 else ""
                prefix += f"{n}:" if "n" in o else ""
                ctx.write(prefix + line + "\n")
        if "c" in o:
            ctx.write((f"{label}:" if len(files) > 1 else "") + f"{count}\n")
        elif "l" in o and count:
            ctx.write(label + "\n")
    return 0 if found else 1


@command("wc", "count lines, words, bytes (-l -w -c)", G)
def _wc(ctx, args):
    o = Opts(args, "lwcm")
    which = [f for f in "lwc" if f in o] or ["l", "w", "c"]
    for label, text in _inputs(ctx, o.args, "wc"):
        nums = {"l": text.count("\n"), "w": len(text.split()), "c": len(text.encode())}
        cols = " ".join(f"{nums[f]:>7}" if len(which) > 1 else str(nums[f]) for f in which)
        ctx.write(cols + (f" {label}" if o.args else "") + "\n")


def _count_arg(args):
    out, n = [], 10
    i = 0
    while i < len(args):
        a = args[i]
        if a == "-n" and i + 1 < len(args):
            n = int(args[i + 1])
            i += 2
            continue
        if a.startswith("-n"):
            n = int(a[2:])
        elif a.startswith("-") and a[1:].isdigit():
            n = int(a[1:])
        else:
            out.append(a)
        i += 1
    return n, out


@command("head", "first lines of a file (-n N)", G)
def _head(ctx, args):
    n, files = _count_arg(args)
    for _, text in _inputs(ctx, files, "head"):
        ctx.write("".join(text.splitlines(True)[:n]))


@command("tail", "last lines of a file (-n N)", G)
def _tail(ctx, args):
    n, files = _count_arg(args)
    for _, text in _inputs(ctx, files, "tail"):
        lines = text.splitlines(True)
        ctx.write("".join(lines[-n:] if n else []))


# ------------------------------------------------------------------ sed
def _sed_parse(script: str):
    """Parse 'ADDR? CMD' items separated by ';' or newlines. CMD: s/a/b/flags, d, p."""
    cmds = []
    i, n = 0, len(script)
    while i < n:
        while i < n and script[i] in " ;\n\t":
            i += 1
        if i >= n:
            break
        addr = None
        if script[i] == "/" or script[i] == "\\":
            delim = "/"
            if script[i] == "\\":
                delim = script[i + 1]
                i += 1
            j = i + 1
            while j < n and script[j] != delim:
                j += 2 if script[j] == "\\" else 1
            if j >= n:
                raise UsageError("-e expression #1: unterminated address regex")
            addr = script[i + 1:j]
            i = j + 1
        if i >= n:
            raise UsageError("-e expression #1: missing command")
        c = script[i]
        if c == "s":
            delim = script[i + 1]
            parts, j, cur = [], i + 2, []
            while j < n and len(parts) < 2:
                ch = script[j]
                if ch == "\\" and j + 1 < n:
                    cur.append(script[j:j + 2] if script[j + 1] != delim else delim)
                    j += 2
                    continue
                if ch == delim:
                    parts.append("".join(cur))
                    cur = []
                else:
                    cur.append(ch)
                j += 1
            if len(parts) < 2:
                raise UsageError("-e expression #1: unterminated `s' command")
            k = j
            while k < n and script[k] not in ";\n":
                k += 1
            cmds.append((addr, "s", parts[0], parts[1], script[j:k].strip()))
            i = k
        elif c in "dp":
            cmds.append((addr, c, None, None, ""))
            i += 1
        else:
            raise UsageError(f"-e expression #1: unknown command: `{c}' "
                             f"(the simulated sed knows s, d and p)")
    return cmds


def _sed_repl(rep: str) -> str:
    out, i = [], 0
    while i < len(rep):
        c = rep[i]
        if c == "\\" and i + 1 < len(rep):
            out.append("\\" + rep[i + 1] if rep[i + 1].isdigit() else rep[i + 1].replace("\\", "\\\\"))
            i += 2
        elif c == "&":
            out.append("\\g<0>")
            i += 1
        else:
            out.append(c.replace("\\", "\\\\"))
            i += 1
    return "".join(out)


def sed_apply(script_cmds, text: str, extended=False, quiet=False) -> str:
    out = []
    for line in text.splitlines():
        deleted = False
        for addr, c, a, b, flags in script_cmds:
            if addr is not None and not re.search(_bre(addr, extended), line):
                continue
            if c == "d":
                deleted = True
                break
            if c == "p":
                out.append(line)
            elif c == "s":
                count = 0 if "g" in flags else 1
                line = re.sub(_bre(a, extended), _sed_repl(b), line, count=count)
        if not deleted and not quiet:
            out.append(line)
    return "".join(ln + "\n" for ln in out)


@command("sed", "stream editor: sed -e 's/a/b/' -i FILE  (s, d, p commands)", G)
def _sed(ctx, args):
    m = ctx.machine
    scripts, files, inplace, ext, quiet = [], [], None, False, False
    i = 0
    while i < len(args):
        a = args[i]
        if a == "-e" and i + 1 < len(args):
            scripts.append(args[i + 1])
            i += 2
            continue
        if a.startswith("-i"):
            inplace = a[2:]
        elif a in ("-E", "-r"):
            ext = True
        elif a == "-n":
            quiet = True
        elif a.startswith("-") and a != "-":
            raise UsageError(f"invalid option -- '{a[1:]}'")
        else:
            files.append(a)
        i += 1
    if not scripts:
        if not files:
            raise UsageError("usage: sed [-i] [-e] SCRIPT [FILE]...")
        scripts.append(files.pop(0))
    cmds = []
    for sc in scripts:
        cmds += _sed_parse(sc)
    if inplace is not None:
        if not files:
            raise UsageError("no input files")
        for f in files:
            node = m.lookup(f)
            if node["t"] != "f":
                raise FsError(f"couldn't edit {f}: not a regular file")
            if not m.can_write(node):
                raise FsError(f"couldn't open file {f}: Permission denied")
            if inplace:
                m.write_file(f + inplace, node.get("data", ""))
            node["data"] = sed_apply(cmds, node.get("data", ""), ext, quiet)
            node["size"] = len(node["data"].encode())
        return 0
    for _, text in _inputs(ctx, files, "sed"):
        ctx.write(sed_apply(cmds, text, ext, quiet))


# ------------------------------------------------------------------ find
@command("find", "find files: find DIR -type f ! -name '*.h' -delete", G)
def _find(ctx, args):
    m = ctx.machine
    roots = []
    i = 0
    while i < len(args) and not args[i].startswith("-") and args[i] != "!":
        roots.append(args[i])
        i += 1
    roots = roots or ["."]
    tests, action, maxdepth = [], "print", None
    neg = False
    while i < len(args):
        a = args[i]
        if a in ("!", "-not"):
            neg = not neg
        elif a in ("-name", "-iname", "-type") and i + 1 < len(args):
            tests.append((a, args[i + 1], neg))
            neg = False
            i += 1
        elif a == "-maxdepth" and i + 1 < len(args):
            maxdepth = int(args[i + 1])
            i += 1
        elif a in ("-delete", "-print"):
            action = a[1:]
        else:
            raise UsageError(f"unknown predicate `{a}' (the simulated find knows "
                             f"-name -type -maxdepth ! -delete -print)")
        i += 1

    def match(name, node):
        for t, v, n in tests:
            if t == "-type":
                ok = {"f": "f", "d": "d", "l": "l", "b": "b"}.get(v) == node["t"]
            elif t == "-name":
                ok = fnmatch.fnmatchcase(name, v)
            else:
                ok = fnmatch.fnmatch(name.lower(), v.lower())
            if ok == n:
                return False
        return True

    status = 0
    for root in roots:
        try:
            top = m.lookup(root)
        except FsError as exc:
            ctx.write(f"find: '{root}': {exc}\n", "err")
            status = 1
            continue
        found = []

        def walk(path, node, depth):
            found.append((path, node, depth))
            if node["t"] == "d" and (maxdepth is None or depth < maxdepth):
                for name in sorted(node["c"]):
                    walk(path.rstrip("/") + "/" + name, node["c"][name], depth + 1)

        walk(root, top, 0)
        if action == "delete":
            for path, node, depth in reversed(found):  # depth-first: children before dirs
                name = path.rsplit("/", 1)[-1]
                if depth == 0 or not match(name, node):
                    continue
                try:
                    if node["t"] == "d":
                        m.rmdir(path)
                    else:
                        m.remove(path)
                except FsError as exc:
                    ctx.write(f"find: cannot delete '{path}': {exc}\n", "err")
                    status = 1
        else:
            for path, node, depth in found:
                if match(path.rsplit("/", 1)[-1], node):
                    ctx.write(path + "\n")
    return status
