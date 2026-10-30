"""User management: groupadd, useradd, passwd, su."""
from __future__ import annotations

import hashlib

from ..shell.opts import Opts
from ..shell.runner import Input, UsageError, command
from ..sim.vfs import FsError
from ..shell.runner import run_script
from .core import login_startup

G = "users"


def _hash(pw: str) -> str:
    return hashlib.sha256(("lfsgame:" + pw).encode()).hexdigest()


@command("groupadd", "create a group: groupadd NAME", G)
def _groupadd(ctx, args):
    m = ctx.machine
    if len(args) != 1:
        raise UsageError("usage: groupadd NAME")
    if not m.is_root:
        raise FsError("Permission denied.")
    m.add_group(args[0])


@command("useradd", "create a user: useradd -s SHELL -g GROUP -m -k SKEL NAME", G)
def _useradd(ctx, args):
    m = ctx.machine
    o = Opts(args, "mMD", "sgkdc", {"create-home": "m", "defaults": "D"},
             {"shell": "s", "gid": "g", "skel": "k", "home-dir": "d", "comment": "c"})
    if "D" in o:  # useradd -D --gid 999: change the defaults file
        if not m.is_root:
            raise FsError("Permission denied.")
        gid = o.vals.get("g", "100")
        m.mkdir("/etc/default", parents=True)
        m.write_file("/etc/default/useradd", f"# useradd defaults file\nGROUP={gid}\n"
                     f"HOME=/home\nINACTIVE=-1\nEXPIRE=\nSHELL=/bin/bash\nSKEL=/etc/skel\n"
                     f"CREATE_MAIL_SPOOL=yes\n")
        return 0
    if len(o.args) != 1:
        raise UsageError("usage: useradd [options] NAME")
    if not m.is_root:
        raise FsError("Permission denied.")
    name = o.args[0]
    group = o.vals.get("g")
    if group is None:
        if name in m.groups:
            raise FsError(f"group {name} exists - if you want to add this user to that group, "
                          f"use -g.")
        m.add_group(name)
        group = name
    m.add_user(name, group, o.vals.get("d", f"/home/{name}"), o.vals.get("s", "/bin/sh"),
               "m" in o)


def _shadow_set(m, name: str, pw_hash: str):
    """Inside a chroot: store the password in its /etc/shadow (created by pwconv)."""
    try:
        node = m.lookup("/etc/shadow")
    except FsError:
        raise FsError("cannot open /etc/shadow (run pwconv first)")
    lines = node.get("data", "").splitlines()
    for i, line in enumerate(lines):
        parts = line.split(":")
        if parts[0] == name:
            parts[1] = "$y$j9T$" + pw_hash[:43]
            lines[i] = ":".join(parts)
            break
    else:
        raise FsError(f"user '{name}' does not exist in /etc/shadow")
    m.write_file("/etc/shadow", "".join(ln + "\n" for ln in lines))


@command("pwconv", "create /etc/shadow from /etc/passwd", G)
def _pwconv(ctx, args):
    return _conv(ctx.machine, "passwd", "shadow", 0, lambda p: f"{p[0]}:{p[1] if p[1] != 'x' else '!'}"
                 f":20026:0:99999:7:::")


@command("grpconv", "create /etc/gshadow from /etc/group", G)
def _grpconv(ctx, args):
    return _conv(ctx.machine, "group", "gshadow", 0, lambda p: f"{p[0]}:!::{p[3] if len(p) > 3 else ''}")


def _conv(m, src, dst, _unused, fmt):
    if not m.is_root:
        raise FsError("Permission denied.")
    text = m.lookup(f"/etc/{src}").get("data", "")
    out, keep = [], []
    for line in text.splitlines():
        parts = line.split(":")
        if len(parts) < 3:
            continue
        out.append(fmt(parts))
        parts[1] = "x"
        keep.append(":".join(parts))
    m.write_file(f"/etc/{dst}", "".join(ln + "\n" for ln in out))
    m.lookup(f"/etc/{dst}")["m"] = 0o600 if dst == "shadow" else 0o400
    m.write_file(f"/etc/{src}", "".join(ln + "\n" for ln in keep))
    return 0


@command("userdel", "delete a user: userdel [-r] NAME", G)
def _userdel(ctx, args):
    m = ctx.machine
    o = Opts(args, "rf")
    if len(o.args) != 1:
        raise UsageError("usage: userdel [-r] NAME")
    name = o.args[0]
    if not m.is_root:
        raise FsError("Permission denied.")
    if not m.croot:
        if name not in m.users or name == "root":
            raise FsError(f"user '{name}' does not exist")
        home = m.users.pop(name)["home"]
        m.groups.pop(name, None)
        m.sync_etc()
    else:
        if name not in m.db_users():
            raise FsError(f"user '{name}' does not exist")
        home = "/home/" + name
        for f in ("passwd", "group", "shadow", "gshadow"):
            try:
                node = m.lookup(f"/etc/{f}")
            except FsError:
                continue
            lines = [ln for ln in node.get("data", "").splitlines()
                     if ln.split(":")[0] != name]
            node["data"] = "".join(ln + "\n" for ln in lines)
    if "r" in o and m.exists(home):
        m.remove(home, recursive=True)
    return 0


@command("passwd", "set a user's password: passwd [USER]", G)
def _passwd(ctx, args):
    m = ctx.machine
    if m.croot:
        name = args[0] if args else (m.chroot_name or "root")
        if name not in m.db_users():
            raise FsError(f"user '{name}' does not exist")
        a = yield Input("New password: ", secret=True)
        b = yield Input("Retype new password: ", secret=True)
        if a is None or a != b:
            ctx.write("Sorry, passwords do not match.\npasswd: Authentication token "
                      "manipulation error\npasswd: password unchanged\n", "err")
            return 10
        _shadow_set(m, name, _hash(a))
        ctx.write("passwd: password updated successfully\n")
        m.flag(f"chroot_passwd:{name}")
        return 0
    name = args[0] if args else m.user
    if name not in m.users:
        raise FsError(f"user '{name}' does not exist")
    if name != m.user and not m.is_root:
        raise FsError("You may not view or modify password information for " + name + ".")
    if not m.is_root and m.users[name].get("pw"):
        old = yield Input("Current password: ", secret=True)
        if old is None or _hash(old) != m.users[name]["pw"]:
            ctx.write("passwd: Authentication token manipulation error\n"
                      "passwd: password unchanged\n", "err")
            return 10
    ctx.write(f"Changing password for {name}.\n" if not m.is_root else "")
    a = yield Input("New password: ", secret=True)
    if a is None:
        ctx.write("passwd: Authentication token manipulation error\npasswd: password unchanged\n",
                  "err")
        return 10
    if len(a) < 4:
        ctx.write("BAD PASSWORD: it is too short (simulation accepts it anyway)\n", "warn")
    b = yield Input("Retype new password: ", secret=True)
    if a != b:
        ctx.write("Sorry, passwords do not match.\n"
                  "passwd: Authentication token manipulation error\n"
                  "passwd: password unchanged\n", "err")
        return 10
    m.users[name]["pw"] = _hash(a)
    ctx.write("passwd: password updated successfully\n")


@command("su", "switch user: su - lfs   (exit to come back)", G)
def _su(ctx, args):
    m = ctx.machine
    login = False
    rest = []
    command_line = None
    i = 0
    while i < len(args):
        a = args[i]
        if a in ("-", "-l", "--login"):
            login = True
        elif a in ("-c", "--command") and i + 1 < len(args):
            command_line = args[i + 1]
            i += 1
        else:
            rest.append(a)
        i += 1
    name = rest[0] if rest else "root"
    if m.croot:  # users of the new system (from its /etc/passwd)
        if name not in m.db_users():
            raise FsError(f"user {name} does not exist or the user entry does not contain all "
                          f"the required fields")
        if not m.is_root:
            ctx.write("su: Authentication failure\n", "err")
            return 1
        if command_line is None:
            ctx.write("(simulation: inside the chroot only 'su USER -c COMMAND' is supported)\n",
                      "dim")
            return 1
        saved = m.user
        m.user = name
        try:
            return (yield from run_script(ctx, command_line, subshell=True))
        finally:
            m.user = saved
    if name not in m.users:
        raise FsError(f"user {name} does not exist or the user entry does not contain all the "
                      f"required fields")
    if not m.is_root:
        pw = yield Input("Password: ", secret=True)
        stored = m.users[name].get("pw")
        if pw is None or stored is None or _hash(pw) != stored:
            ctx.write("su: Authentication failure\n", "err")
            return 1
    if not login:
        ctx.write("(simulation: 'su' always behaves like 'su -', a full login)\n", "dim")
    m.su(name)
    if name == "lfs" and login:
        m.flag("su_lfs")
    yield from login_startup(ctx)
