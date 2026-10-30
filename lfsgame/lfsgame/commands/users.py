"""User management: groupadd, useradd, passwd, su."""
from __future__ import annotations

import hashlib

from ..shell.opts import Opts
from ..shell.runner import Input, UsageError, command
from ..sim.vfs import FsError

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
    o = Opts(args, "mM", "sgkdc", {"create-home": "m"},
             {"shell": "s", "gid": "g", "skel": "k", "home-dir": "d", "comment": "c"})
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


@command("passwd", "set a user's password: passwd [USER]", G)
def _passwd(ctx, args):
    m = ctx.machine
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
    for a in args:
        if a in ("-", "-l", "--login"):
            login = True
        else:
            rest.append(a)
    name = rest[0] if rest else "root"
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
