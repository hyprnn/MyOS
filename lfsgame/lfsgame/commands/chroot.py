"""chroot and the small tools chapter 7 needs: install, chgrp, realpath."""
from __future__ import annotations

from ..shell.opts import Opts
from ..shell.runner import UsageError, command, find_program, run_argv
from ..sim.vfs import FsError

G = "users"


@command("chroot", "run a shell with a different root: chroot DIR CMD...", G)
def _chroot(ctx, args):
    m = ctx.machine
    if not args:
        raise UsageError("missing operand")
    newroot, cmd = args[0], args[1:] or [m.env.get("SHELL", "/bin/bash"), "-i"]
    if not m.is_root:
        ctx.write(f"chroot: cannot change root directory to '{newroot}': Operation not "
                  f"permitted\n", "err")
        return 125
    try:
        r = m.resolve(newroot)
    except FsError as exc:
        raise FsError(f"cannot change root directory to '{newroot}': {exc}")
    if r.node is None or r.node["t"] != "d":
        ctx.write(f"chroot: cannot change root directory to '{newroot}': No such file or "
                  f"directory\n", "err")
        return 125
    m.chroot(r.real)
    if cmd[0].startswith("/") and find_program(m, cmd[0]) is None:
        m.logout()
        ctx.write(f"chroot: failed to run command '{cmd[0]}': No such file or directory\n", "err")
        return 127
    status = yield from run_argv(ctx, cmd)
    if status in (126, 127) and m.croot == r.real.rstrip("/"):
        m.logout()  # the command could not be started: chroot ends right away
        return status
    if m.croot == r.real.rstrip("/") and m.env.get("PATH") == "/usr/bin:/usr/sbin" \
            and "lfs chroot" in m.env.get("PS1", ""):
        m.flag("chroot_ok")
    return status


@command("install", "copy files / create dirs with mode and owner: install -dv -m 1777 DIR", G)
def _install(ctx, args):
    m = ctx.machine
    o = Opts(args, "dvDcsp", "mog", {"directory": "d", "verbose": "v"},
             {"mode": "m", "owner": "o", "group": "g"})
    mode = int(o.vals["m"], 8) if "m" in o.vals else None
    owner, group = o.vals.get("o"), o.vals.get("g")
    if owner and owner not in m.db_users():
        raise FsError(f"invalid user '{owner}'")
    if group and group not in m.db_groups():
        raise FsError(f"invalid group '{group}'")
    if (owner or group) and not m.is_root:
        raise FsError("cannot change ownership: Operation not permitted")

    def finish(node):
        if mode is not None:
            node["m"] = mode
        if owner:
            node["u"] = owner
        if group:
            node["g"] = group

    if "d" in o:
        if not o.args:
            raise UsageError("missing operand")
        for d in o.args:
            created = m.mkdir(d, parents=True)
            if "v" in o:
                for c in created:
                    ctx.write(f"install: creating directory '{c}'\n")
            finish(m.lookup(d))
        return 0
    if len(o.args) < 2:
        raise UsageError("missing destination file operand")
    *srcs, dest = o.args
    for src in srcs:
        node = m.lookup(src)
        if node["t"] == "d":
            raise FsError(f"omitting directory '{src}'")
        target = dest.rstrip("/") + "/" + src.rsplit("/", 1)[-1] if m.isdir(dest) else dest
        new = m.write_file(target, node.get("data", ""))
        for k in ("prog", "tool", "cross", "native", "size", "md5", "target"):
            if k in node:
                new[k] = node[k]
        new["m"] = 0o755
        finish(new)
        if "v" in o:
            ctx.write(f"'{src}' -> '{target}'\n")
    return 0


@command("chgrp", "change group: chgrp [-v] GROUP FILE...", G)
def _chgrp(ctx, args):
    m = ctx.machine
    o = Opts(args, "vRc", "", {"verbose": "v", "recursive": "R"})
    if len(o.args) < 2:
        raise UsageError("usage: chgrp GROUP FILE...")
    group, files = o.args[0], o.args[1:]
    if group not in m.db_groups():
        raise FsError(f"invalid group: '{group}'")
    for f in files:
        node = m.lookup(f)
        if not m.is_root and node["u"] != m.user:
            raise FsError(f"changing group of '{f}': Operation not permitted")
        old = m.display_owner(node["g"], group=True)
        node["g"] = group
        if "v" in o:
            ctx.write(f"changed group of '{f}' from {old} to {group}\n" if old != group
                      else f"group of '{f}' retained as {group}\n")
    return 0


@command("realpath", "print the resolved absolute path", G)
def _realpath(ctx, args):
    m = ctx.machine
    status = 0
    for a in args:
        try:
            r = m.resolve(a)
            if r.node is None:
                raise FsError("No such file or directory")
            ctx.write(r.path + "\n")
        except FsError as exc:
            ctx.write(f"realpath: {a}: {exc}\n", "err")
            status = 1
    return status

