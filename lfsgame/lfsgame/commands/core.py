"""Shell builtins and general commands."""
from __future__ import annotations

import inspect
import time

from ..shell.opts import Opts
from ..shell.runner import (PROGRAMS, REGISTRY, ScriptExit, Sleep, UsageError, command, find_program,
                            run_argv, run_script, source_file)
from ..sim import machine as mach
from ..sim.vfs import FsError

G = "shell"


@command("help", "list commands, or 'help <command>'", G)
def _help(ctx, args):
    if args:
        for name in args:
            cmd = REGISTRY.get(name)
            if cmd is None:
                ctx.write(f"help: no help topics match '{name}'\n", "err")
                return 1
            ctx.write(f"{name} — {cmd.help}\n")
        return 0
    groups = {}
    for name, cmd in REGISTRY.items():
        if not cmd.hidden:
            groups.setdefault(cmd.group, []).append(name)
    order = ["game", "shell", "files", "text", "disks", "network", "users", "build", "misc"]
    for g in sorted(groups, key=lambda g: order.index(g) if g in order else 99):
        ctx.write(f"{g}:\n", "accent")
        for name in sorted(groups[g]):
            ctx.write(f"  {name:<10} ", "prompt")
            ctx.write(f"{REGISTRY[name].help}\n")
    ctx.write("Tab completes commands and paths. Ctrl+C interrupts. F2 opens the LFS book.\n", "dim")


@command("clear", "clear the screen", G)
def _clear(ctx, args):
    ctx.clear()


@command("echo", "print arguments (-n: no newline)", G)
def _echo(ctx, args):
    newline = True
    if args and args[0] == "-n":
        newline, args = False, args[1:]
    ctx.write(" ".join(args) + ("\n" if newline else ""))


@command("pwd", "print working directory", G)
def _pwd(ctx, args):
    ctx.write(ctx.machine.cwd + "\n")


def _chdir(ctx, m, target):
    node = m.lookup(target)
    if node["t"] != "d":
        raise FsError(f"{target}: Not a directory")
    if not m.is_root and node["u"] != m.user and not node["m"] & 0o001:
        raise FsError(f"{target}: Permission denied")
    m.env["OLDPWD"] = m.cwd
    m.cwd = m.abspath(target)


@command("cd", "change directory (cd, cd -, cd ~, cd DIR)", G)
def _cd(ctx, args):
    m = ctx.machine
    if len(args) > 1:
        raise UsageError("too many arguments")
    target = args[0] if args else m.env.get("HOME", "/")
    if target == "-":
        target = m.env.get("OLDPWD", m.cwd)
        ctx.write(target + "\n")
    try:
        _chdir(ctx, m, target)
    except FsError as exc:
        msg = str(exc)
        raise FsError(msg if msg.startswith(target) else f"{target}: {msg}")


@command("pushd", "push directory onto the stack and cd into it", G)
def _pushd(ctx, args):
    m = ctx.machine
    if len(args) != 1:
        raise UsageError("usage: pushd DIR")
    old = m.cwd
    try:
        _chdir(ctx, m, args[0])
    except FsError as exc:
        raise FsError(f"{args[0]}: {exc}" if not str(exc).startswith(args[0]) else str(exc))
    m.dirstack.insert(0, old)
    ctx.write(" ".join([m.cwd] + m.dirstack) + "\n")


@command("popd", "return to the directory saved by pushd", G)
def _popd(ctx, args):
    m = ctx.machine
    if not m.dirstack:
        raise FsError("directory stack empty")
    _chdir(ctx, m, m.dirstack.pop(0))
    ctx.write(" ".join([m.cwd] + m.dirstack) + "\n")


@command("export", "set an environment variable: export NAME=value", G)
def _export(ctx, args):
    m = ctx.machine
    if not args or args == ["-p"]:
        for k in sorted(m.env):
            ctx.write(f'declare -x {k}="{m.env[k]}"\n')
        return
    for a in args:
        name, eq, value = a.partition("=")
        if not name.replace("_", "a").isalnum() or name[0].isdigit():
            ctx.write(f"export: `{a}': not a valid identifier\n", "err")
            return 1
        if eq:
            m.env[name] = value
        else:
            m.env.setdefault(name, "")


@command("unset", "remove an environment variable", G)
def _unset(ctx, args):
    for a in args:
        ctx.machine.env.pop(a, None)


@command("printenv", "print environment variables", G)
def _printenv(ctx, args):
    env = ctx.machine.env
    if args:
        for a in args:
            if a in env:
                ctx.write(env[a] + "\n")
        return 0 if all(a in env for a in args) else 1
    for k in sorted(env):
        ctx.write(f"{k}={env[k]}\n")


@command("env", "print the environment; env -i A=1 B=2 /bin/bash starts a clean shell", G)
def _env(ctx, args):
    m = ctx.machine
    clean = False
    i = 0
    while i < len(args) and args[i].startswith("-"):
        if args[i] in ("-i", "-", "--ignore-environment"):
            clean = True
        else:
            raise UsageError(f"invalid option -- '{args[i].lstrip('-')}'")
        i += 1
    new_env = {} if clean else dict(m.env)
    while i < len(args) and "=" in args[i] and not args[i].startswith("="):
        k, _, v = args[i].partition("=")
        new_env[k] = v
        i += 1
    rest = args[i:]
    if not rest:
        for k in sorted(new_env):
            ctx.write(f"{k}={new_env[k]}\n")
        return 0
    if rest[0].rsplit("/", 1)[-1] in ("bash", "sh") and \
            all(a in ("--login", "-l", "-i") for a in rest[1:]):
        # a new shell with exactly this environment (LFS 4.4 .bash_profile, 7.4 chroot)
        if m.croot and m.resolve(rest[0]).node is None:
            ctx.write(f"env: '{rest[0]}': No such file or directory\n", "err")
            return 127
        m.env = new_env
        m.refresh_name()
        if "--login" in rest or "-l" in rest:
            yield from login_startup(ctx)
        else:
            yield from interactive_startup(ctx)
        return 0
    old = m.env
    m.env = new_env
    try:
        return (yield from run_argv(ctx, rest))
    finally:
        if m.env is new_env:
            m.env = old


@command("umask", "show or set the file creation mask (umask 022)", G)
def _umask(ctx, args):
    m = ctx.machine
    if not args:
        ctx.write(f"{m.umask:04o}\n")
        return
    try:
        value = int(args[0], 8)
        if not 0 <= value <= 0o777:
            raise ValueError
    except ValueError:
        raise FsError(f"{args[0]}: octal number out of range")
    m.umask = value


@command("whoami", "print current user", G)
def _whoami(ctx, args):
    m = ctx.machine
    if m.croot:
        name = m.db_name_of_uid(m.uid())
        if not name:
            ctx.write(f"whoami: cannot find name for user ID {m.uid()}\n", "err")
            return 1
        ctx.write(name + "\n")
        return 0
    ctx.write(m.user + "\n")


@command("id", "print user and group ids", G)
def _id(ctx, args):
    m = ctx.machine
    if m.croot and not args:
        uid = m.uid()
        name = m.db_name_of_uid(uid)
        ctx.write(f"uid={uid}({name}) gid={uid}({name}) groups={uid}({name})\n" if name
                  else f"uid={uid} gid={uid} groups={uid}\n")
        return 0
    name = args[0] if args else m.user
    u = m.users.get(name)
    if u is None:
        raise FsError(f"'{name}': no such user")
    gid = m.groups.get(u["group"], 0)
    ctx.write(f"uid={u['uid']}({name}) gid={gid}({u['group']}) groups={gid}({u['group']})\n")


@command("uname", "print system information (-a, -m, -r)", G)
def _uname(ctx, args):
    o = Opts(args, "amrsnv")
    rel = mach.HOST_TOOLS["kernel"] + "-host"
    if "a" in o:
        ctx.write(f"Linux {mach.HOSTNAME} {rel} #1 SMP PREEMPT_DYNAMIC x86_64 GNU/Linux\n")
        return
    parts = []
    if "s" in o or not o.flags:
        parts.append("Linux")
    if "n" in o:
        parts.append(mach.HOSTNAME)
    if "r" in o:
        parts.append(rel)
    if "m" in o:
        parts.append("x86_64")
    ctx.write(" ".join(parts) + "\n")


@command("hostname", "print the host name", G)
def _hostname(ctx, args):
    ctx.write(mach.HOSTNAME + "\n")


@command("date", "print date and time", G)
def _date(ctx, args):
    ctx.write(time.strftime("%a %b %d %H:%M:%S %Z %Y") + "\n")


@command("history", "show command history", G)
def _history(ctx, args):
    for i, line in enumerate(getattr(ctx, "history", []), 1):
        ctx.write(f"{i:>5}  {line}\n")


@command("true", "do nothing, successfully", G, hidden=True)
def _true(ctx, args):
    return 0


@command("false", "do nothing, unsuccessfully", G, hidden=True)
def _false(ctx, args):
    return 1


@command("sleep", "wait N seconds (max 10)", G)
def _sleep(ctx, args):
    try:
        secs = float(args[0])
    except (IndexError, ValueError):
        raise UsageError("usage: sleep SECONDS")
    yield Sleep(int(min(max(secs, 0), 10) * 1000))


@command("which", "show where a command lives", G, aliases=("type",))
def _which(ctx, args):
    status = 0
    for a in args:
        found = find_program(ctx.machine, a)
        if found is not None:
            ctx.write(found[0] + "\n")
        elif a in REGISTRY:
            ctx.write(f"/usr/bin/{a}\n")
        else:
            status = 1
    return status


@command("man", "manual pages (use the LFS book instead)", G, aliases=("info",))
def _man(ctx, args):
    ctx.write("There are no man pages in this simulation. Try 'help <command>' or open the "
              "LFS book with F2 / 'book'.\n", "dim")


def interactive_startup(ctx):
    """What an interactive non-login bash reads: /etc/bash.bashrc, then ~/.bashrc."""
    m = ctx.machine
    for path in ("/etc/bash.bashrc", m.env.get("HOME", m.home()) + "/.bashrc"):
        if m.exists(path) and m.lookup(path)["t"] == "f":
            yield from source_file(ctx, path)


def login_startup(ctx):
    """What a login bash reads (su -): the first of ~/.bash_profile, ~/.bash_login, ~/.profile."""
    m = ctx.machine
    home = m.env.get("HOME", m.home())
    for name in (".bash_profile", ".bash_login", ".profile"):
        if m.exists(f"{home}/{name}"):
            yield from source_file(ctx, f"{home}/{name}")
            return


@command("bash", "run a script in a subshell: bash FILE  (no FILE: new shell)", G,
         aliases=("sh",))
def _bash(ctx, args):
    m = ctx.machine
    if args and args[0] == "-c" and len(args) > 1:
        return (yield from run_script(ctx, args[1], subshell=True))
    if not args or args[0] in ("-i", "-l", "--login"):
        ctx.write("(a new interactive shell: reading the startup files)\n", "dim")
        m.refresh_name()
        if args and args[0] in ("-l", "--login"):
            yield from login_startup(ctx)
        else:
            yield from interactive_startup(ctx)
        return 0
    node = m.lookup(args[0])
    if node["t"] != "f":
        raise FsError(f"{args[0]}: Is a directory")
    if node.get("prog") in PROGRAMS:  # e.g. sh support/config.guess
        return (yield from _as_gen(PROGRAMS[node["prog"]](ctx, args[1:], m.abspath(args[0]),
                                                          node)))
    return (yield from run_script(ctx, node.get("data", ""), subshell=True))


def _as_gen(res):
    if inspect.isgenerator(res):
        res = yield from res
    return res


@command("source", "run a file in the CURRENT shell: source FILE  (also: . FILE)", G,
         aliases=(".",))
def _source(ctx, args):
    if not args:
        raise UsageError("filename argument required")
    m = ctx.machine
    if not m.exists(args[0]):
        raise FsError(f"{args[0]}: No such file or directory")
    return (yield from source_file(ctx, args[0]))


@command("exec", "replace the shell with a command (here: just run it)", G)
def _exec(ctx, args):
    if not args:
        return 0
    return (yield from run_argv(ctx, args))


@command("set", "shell options (set +h is accepted; no args: list variables)", G)
def _set(ctx, args):
    if not args:
        for k in sorted(ctx.machine.env):
            ctx.write(f"{k}={ctx.machine.env[k]}\n")
    return 0


def _test(m, a):
    """Evaluate a test expression (list of strings). Returns bool or raises UsageError."""
    if not a:
        return False
    if a[0] == "!":
        return not _test(m, a[1:])
    if len(a) == 1:
        return a[0] != ""
    if len(a) == 2:
        op, x = a
        if op == "-z":
            return x == ""
        if op == "-n":
            return x != ""
        if op in ("-e", "-f", "-d", "-L", "-h", "-x", "-s", "-r", "-w"):
            try:
                node = m.resolve(x, follow=op not in ("-L", "-h")).node
            except FsError:
                return False
            if node is None:
                return False
            return {"-e": True, "-r": True, "-w": True, "-f": node["t"] == "f",
                    "-d": node["t"] == "d", "-L": node["t"] == "l", "-h": node["t"] == "l",
                    "-x": bool(node["m"] & 0o111), "-s": node.get("size", 4096) > 0}[op]
        raise UsageError(f"{op}: unary operator expected")
    if len(a) == 3:
        x, op, y = a
        if op in ("=", "=="):
            return x == y
        if op == "!=":
            return x != y
        if op in ("-eq", "-ne", "-lt", "-le", "-gt", "-ge"):
            try:
                xi, yi = int(x), int(y)
            except ValueError:
                raise UsageError(f"{x}: integer expression expected")
            return {"-eq": xi == yi, "-ne": xi != yi, "-lt": xi < yi, "-le": xi <= yi,
                    "-gt": xi > yi, "-ge": xi >= yi}[op]
        raise UsageError(f"{op}: binary operator expected")
    if "-a" in a:
        i = a.index("-a")
        return _test(m, a[:i]) and _test(m, a[i + 1:])
    if "-o" in a:
        i = a.index("-o")
        return _test(m, a[:i]) or _test(m, a[i + 1:])
    raise UsageError("too many arguments")


@command("test", "evaluate a condition: test -e FILE, [ -L /bin ], [ \"$A\" = b ]", G,
         aliases=("[",))
def _test_cmd(ctx, args):
    if args and args[-1] == "]":
        args = args[:-1]
    return 0 if _test(ctx.machine, args) else 1


@command("ulimit", "resource limits (accepted, not simulated)", G, hidden=True)
def _ulimit(ctx, args):
    if not args:
        ctx.write("unlimited\n")
    return 0


@command("nproc", "number of CPU cores", G)
def _nproc(ctx, args):
    ctx.write(f"{mach.CORES}\n")


@command("time", "time a command: time make", G)
def _time(ctx, args):
    m = ctx.machine
    start = m.clock
    status = 0
    if args:
        status = yield from run_argv(ctx, args)
    real = m.clock - start + 0.004
    ctx.write(f"\nreal\t{int(real // 60)}m{real % 60:.3f}s\n"
              f"user\t{int(real * 0.9 // 60)}m{real * 0.9 % 60:.3f}s\n"
              f"sys\t0m{real * 0.08 % 60:.3f}s\n", "dim")
    return status


@command("dirname", "strip the last component from a path", G)
def _dirname(ctx, args):
    for a in args or [""]:
        a = a.rstrip("/") or "/"
        head = a.rsplit("/", 1)[0] if "/" in a else "."
        ctx.write((head or "/") + "\n")


@command("basename", "strip the directory (and a suffix) from a path", G)
def _basename(ctx, args):
    if not args:
        raise UsageError("missing operand")
    name = args[0].rstrip("/").rsplit("/", 1)[-1] or "/"
    if len(args) > 1 and name.endswith(args[1]) and name != args[1]:
        name = name[:-len(args[1])]
    ctx.write(name + "\n")


def _ver_tuple(v: str):
    out = []
    for part in v.replace("-", ".").split("."):
        num = "".join(ch for ch in part if ch.isdigit())
        out.append(int(num) if num else 0)
    return tuple(out)


@command("ver_check", "helper used by version-check.sh", G, hidden=True)
def _ver_check(ctx, args):
    if len(args) != 3:
        raise UsageError("usage: ver_check NAME TOOL MIN_VERSION")
    name, tool, minimum = args
    have = mach.HOST_TOOLS.get(mach.TOOL_ALIASES.get(tool, tool))
    if have is None:
        ctx.write(f"ERROR: Cannot find {tool} ({name})\n", "err")
        return 1
    if _ver_tuple(have) >= _ver_tuple(minimum):
        ctx.write("OK:    ", "ok")
        ctx.write(f"{name:<10} {have:<8} >= {minimum}\n")
        ctx.machine.flag("version_check")
        return 0
    ctx.write(f"ERROR: {name:<10} is TOO OLD ({minimum} or later required)\n", "err")
    return 1


@command("exit", "leave 'su' session, or return to the main menu", G, aliases=("logout",))
def _exit(ctx, args):
    status = int(args[0]) if args and args[0].isdigit() else 0
    if getattr(ctx, "in_script", False):
        raise ScriptExit(status)
    if ctx.machine.logout():
        ctx.write("logout\n")
        return 0
    ctx.goto_menu()


@command("reboot", "restart the machine (not now!)", G, aliases=("poweroff", "shutdown", "halt"))
def _reboot(ctx, args):
    ctx.write("Not a good idea right now: the LFS system is not bootable yet.\n"
              "(Rebooting is not simulated.)\n", "warn")
    return 1
