"""Shell builtins and general commands."""
from __future__ import annotations

import time

from ..shell.opts import Opts
from ..shell.runner import REGISTRY, Sleep, UsageError, command, start_line
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
    order = ["game", "shell", "files", "disks", "network", "users", "misc"]
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


@command("env", "print the environment", G, aliases=("printenv",))
def _env(ctx, args):
    env = ctx.machine.env
    if args:
        for a in args:
            if a in env:
                ctx.write(env[a] + "\n")
        return 0 if all(a in env for a in args) else 1
    for k in sorted(env):
        ctx.write(f"{k}={env[k]}\n")


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
    ctx.write(ctx.machine.user + "\n")


@command("id", "print user and group ids", G)
def _id(ctx, args):
    m = ctx.machine
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
        if a in REGISTRY:
            ctx.write(f"/usr/bin/{a}\n")
        else:
            status = 1
    return status


@command("man", "manual pages (use the LFS book instead)", G, aliases=("info",))
def _man(ctx, args):
    ctx.write("There are no man pages in this simulation. Try 'help <command>' or open the "
              "LFS book with F2 / 'book'.\n", "dim")


@command("bash", "run a shell script: bash FILE", G, aliases=("sh", "source", "."))
def _bash(ctx, args):
    m = ctx.machine
    if not args:
        ctx.write("Interactive sub-shells are not simulated. You are already in bash :)\n", "dim")
        return 0
    node = m.lookup(args[0])
    if node["t"] != "f":
        raise FsError(f"{args[0]}: Is a directory")
    status = 0
    for line in node.get("data", "").splitlines():
        if line.strip() and not line.lstrip().startswith("#"):
            status = yield from start_line(ctx, line)
    return status


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
    if ctx.machine.logout():
        ctx.write("logout\n")
        return 0
    ctx.goto_menu()


@command("reboot", "restart the machine (not now!)", G, aliases=("poweroff", "shutdown", "halt"))
def _reboot(ctx, args):
    ctx.write("Not a good idea right now: the LFS system is not bootable yet.\n"
              "(Rebooting is not simulated.)\n", "warn")
    return 1
