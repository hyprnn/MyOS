"""Build tools: tar, patch, make, readelf, gcc (host) and the programs behind
configure scripts, config.guess and the cross toolchain (see sim/build.py)."""
from __future__ import annotations

import re

from ..shell.opts import Opts
from ..shell.runner import Input, Sleep, UsageError, command, program
from ..sim import build as bd
from ..sim import packages as pk
from ..sim.vfs import FsError, mkdir_node, node_md5

G = "build"


def _chown_tree(node, user, group):
    node["u"], node["g"] = user, group
    for child in node.get("c", {}).values():
        _chown_tree(child, user, group)


def _show_lines(ctx, lines, every=3, ms=60, tag=None):
    for i, line in enumerate(lines):
        ctx.write(line + "\n", tag)
        if i % every == every - 1:
            yield Sleep(ms)


def _hint(ctx, hint):
    if hint:
        ctx.write(f"(game hint: {hint})\n", "dim")


# ------------------------------------------------------------------ tar
@command("tar", "unpack a source archive: tar -xf FILE  (-v list, -C DIR)", G)
def _tar(ctx, args):
    m = ctx.machine
    flags, files, dest = set(), [], None
    i = 0
    first = True
    while i < len(args):
        a = args[i]
        if a == "-C" and i + 1 < len(args):
            dest = args[i + 1]
            i += 2
            continue
        if a.startswith("--directory="):
            dest = a.split("=", 1)[1]
        elif a.startswith("-") or (first and re.fullmatch(r"[xtcvfzjJ]+", a)):
            for ch in a.lstrip("-"):
                if ch not in "xtcvfzjJ":
                    raise UsageError(f"invalid option -- '{ch}'")
                flags.add(ch)
        else:
            files.append(a)
        first = False
        i += 1
    if "c" in flags:
        raise UsageError("creating archives is not simulated")
    if not ({"x", "t"} & flags) or "f" not in flags or not files:
        raise UsageError("usage: tar -xf ARCHIVE [-C DIR]   or   tar -tf ARCHIVE")
    path = files[0]
    node = m.lookup(path)
    if node["t"] != "f":
        raise FsError(f"{path}: Cannot read: Is a directory")
    base = path.rsplit("/", 1)[-1]
    pkg = pk.BY_FILE.get(base)
    if pkg is None or not re.search(r"\.tar\.(xz|gz|bz2)$", base) or "md5" not in node:
        ctx.write("tar: This does not look like a tar archive\n"
                  "tar: Exiting with failure status due to previous errors\n", "err")
        return 2
    if node.get("partial") or node_md5(node) != pk.fake_md5(base):
        ctx.write("xz: (stdin): Unexpected end of input\ntar: Unexpected EOF in archive\n"
                  "tar: Error is not recoverable: exiting now\n", "err")
        _hint(ctx, "the download is broken: check it with md5sum and get it again")
        return 2
    top, tree = bd.source_tree(base)
    yield Sleep(int(min(1500, 120 + pkg.size / (90 * 1024 * 1024) * 1000)))
    m.clock += pkg.size / (40 * 1024 * 1024)
    if "t" in flags or "v" in flags:
        names = [f"{top}/"] + [f"{top}/{n}" + ("/" if c["t"] == "d" else "")
                               for n, c in sorted(tree["c"].items())]
        for n in names[:14]:
            ctx.write(n + "\n")
        ctx.write(f"(... {max(0, int(pkg.size / 9000) - len(names[:14]))} more files)\n", "dim")
    if "x" not in flags:
        return 0
    target_dir = m.resolve(dest or ".")
    if target_dir.node is None or target_dir.node["t"] != "d":
        raise FsError(f"{dest}: Cannot open: No such file or directory")
    if not m.can_write(target_dir.node):
        raise FsError(f"{top}: Cannot mkdir: Permission denied")
    u, g = m._owner()
    _chown_tree(tree, u, g)
    target_dir.node["c"][top] = tree
    return 0


# ------------------------------------------------------------------ patch
PATCHED_FILES = ["Makeconfig", "nscd/nscd.h", "nss/db-Makefile", "sunrpc/rpc_main.c",
                 "sysdeps/generic/paths.h", "sysdeps/unix/sysv/linux/paths.h"]


@command("patch", "apply a patch: patch -Np1 -i ../FILE.patch", G)
def _patch(ctx, args):
    m = ctx.machine
    o = Opts(args, "NRsf", "pi", {"forward": "N", "reverse": "R"}, {"input": "i", "strip": "p"})
    if "i" in o.vals:
        text = m.lookup(o.vals["i"]).get("data", "")
    elif o.args:
        text = m.lookup(o.args[-1]).get("data", "")
    else:
        text = getattr(ctx, "stdin", None) or ""
    mo = re.search(r"lfsgame-patch: (\S+)", text)
    if not text.strip():
        ctx.write("patch: **** Only garbage was found in the patch input.\n", "err")
        return 2
    if not mo:
        ctx.write("patch: **** this patch is not known to the simulation\n", "err")
        return 2
    name = mo.group(1).split("-", 1)[1]           # "fhs"
    top = m.lookup(".")
    if top.get("srcpkg") != "glibc" or o.vals.get("p", "0") != "1":
        ctx.write("can't find file to patch at input line 5\n"
                  "Perhaps you used the wrong -p or --strip option?\n"
                  "The text leading up to this was:\n"
                  "--------------------------\n"
                  "|diff -Naur glibc-2.40.orig/Makeconfig glibc-2.40/Makeconfig\n"
                  "--------------------------\n")
        if "f" not in o and "s" not in o:
            yield Input("File to patch: ")
        ctx.write("Skip this patch? [y] \nSkipping patch.\n1 out of 1 hunk ignored\n", "err")
        _hint(ctx, "run patch -Np1 from the top of the unpacked glibc source tree")
        return 1
    applied = name in top.get("patches", [])
    reverse = "R" in o
    if applied and not reverse:
        ctx.write("patching file Makeconfig\n")
        if "N" in o:
            ctx.write("Reversed (or previously applied) patch detected!  Skipping patch.\n"
                      "1 out of 1 hunk ignored -- saving rejects to file Makeconfig.rej\n", "err")
            return 1
        ans = yield Input("Reversed (or previously applied) patch detected!  Assume -R? [n] ")
        if ans and ans.strip().lower().startswith("y"):
            reverse = True
        else:
            ans = yield Input("Apply anyway? [n] ")
            ctx.write("Skipping patch.\n1 out of 1 hunk ignored -- saving rejects to file "
                      "Makeconfig.rej\n", "err")
            return 1
    if reverse and not applied:
        ctx.write("patching file Makeconfig\nUnreversed patch detected!  Ignore -R? [n] \n"
                  "Skipping patch.\n", "err")
        return 1
    for f in PATCHED_FILES:
        ctx.write(f"patching file {f}\n")
    patches = [p for p in top.get("patches", []) if p != name]
    if not reverse:
        patches.append(name)
    top["patches"] = patches
    if "Makeconfig" in top["c"]:
        mk = top["c"]["Makeconfig"]
        mk["data"] = mk["data"].replace("/db\n", "/lib/nss_db\n") if not reverse else \
            mk["data"].replace("/lib/nss_db\n", "/db\n")
    return 0


# ------------------------------------------------------------------ configure / programs
@program("configure")
def _configure(ctx, args, path, node):
    m = ctx.machine
    srcdir = path.rsplit("/", 1)[0] or "/"
    cwd = m.lookup(".")
    if not m.can_write(cwd):
        ctx.write("configure: error: cannot create config.log: Permission denied\n", "err")
        return 1
    try:
        lines, meta = bd.configure(m, node["component"], srcdir, m.cwd, args)
    except bd.BuildError as exc:
        yield from _show_lines(ctx, exc.lines)
        ctx.write(f"configure: error: {exc}\n", "err")
        _hint(ctx, exc.hint)
        m.write_file("config.log", "\n".join(exc.lines) + f"\nconfigure: error: {exc}\n")
        return 1
    yield from _show_lines(ctx, lines)
    m.write_file("config.log", "\n".join(lines) + "\n")
    m.write_file("config.status", "#! /bin/sh\n# config.status (simulated)\n")
    mk = m.write_file("Makefile", f"# Makefile generated by configure for {meta['component']}\n")
    mk["prog"] = "makefile"
    cwd["build"] = meta
    m.clock += 25 * bd.SBU.get(meta["component"], 1)
    return 0


@program("config.guess")
def _config_guess(ctx, args, path, node):
    ctx.write(bd.HOST_TRIPLET + "\n")


@program("binary")
def _binary(ctx, args, path, node):
    if "--version" in args or "-V" in args:
        ctx.write(f"{path.rsplit('/', 1)[-1]} (simulated)\n")
        return 0
    ctx.write(f"{path}: this program cannot run on the host (it belongs to the new "
              f"system)\n", "dim")
    return 1


@program("elf")
def _elf(ctx, args, path, node):
    return 0  # 'int main(){}' does nothing, successfully


@program("binutils")
def _binutils(ctx, args, path, node):
    tool = node.get("tool", "ld")
    if "--version" in args or "-v" in args:
        names = {"ld": "GNU ld", "as": "GNU assembler", "ar": "GNU ar", "nm": "GNU nm"}
        ctx.write(f"{names.get(tool, 'GNU ' + tool)} (GNU Binutils) 2.43.1\n"
                  "Copyright (C) 2024 Free Software Foundation, Inc.\n")
        return 0
    if tool == "readelf":
        return _readelf(ctx, args)
    ctx.write(f"{path.rsplit('/', 1)[-1]}: only --version is simulated for this tool\n", "dim")
    return 1


def _gcc_compile(ctx, args, path, node):
    m = ctx.machine
    t = node.get("target", bd.HOST_TRIPLET)
    prefix = node.get("prefix", "/usr")
    sysroot = node.get("sysroot", "/")
    name = path.rsplit("/", 1)[-1]
    gcclib = f"{prefix}/lib/gcc/{t}/{bd.GCC_VER}"
    if "--version" in args:
        ctx.write(f"{name} (GCC) {bd.GCC_VER}\nCopyright (C) 2024 Free Software Foundation, "
                  f"Inc.\n")
        return 0
    if "-dumpmachine" in args:
        ctx.write(t + "\n")
        return 0
    if "-print-libgcc-file-name" in args:
        ctx.write(f"{gcclib}/libgcc.a\n")
        return 0
    if "-print-sysroot" in args:
        ctx.write(sysroot + "\n")
        return 0
    out, lang, compile_only, inputs = "a.out", None, False, []
    i = 0
    while i < len(args):
        a = args[i]
        if a == "-o" and i + 1 < len(args):
            out = args[i + 1]
            i += 2
            continue
        if a.startswith("-x"):
            lang = a[2:] or (args[i + 1] if i + 1 < len(args) else "")
            if a == "-x":
                i += 1
        elif a == "-c":
            compile_only = True
        elif a == "-" or not a.startswith("-"):
            inputs.append(a)
        i += 1
    if not inputs:
        ctx.write(f"{name}: fatal error: no input files\ncompilation terminated.\n", "err")
        return 1
    for src in inputs:
        if src == "-":
            if lang is None:
                ctx.write(f"{name}: error: -E or -x required when input is from standard "
                          f"input\n", "err")
                return 1
            text, label = getattr(ctx, "stdin", None) or "", "<stdin>"
        else:
            text, label = m.lookup(src).get("data", ""), src
        for n, line in enumerate(text.splitlines(), 1):
            inc = re.match(r"\s*#\s*include\s*<([^>]+)>", line)
            if inc and not (m.exists(f"{sysroot.rstrip('/')}/usr/include/{inc.group(1)}")
                            or m.exists(f"{gcclib}/include/{inc.group(1)}")):
                ctx.write(f"{label}:{n}:10: fatal error: {inc.group(1)}: No such file or "
                          f"directory\n    {n} | {line}\ncompilation terminated.\n", "err")
                return 1
    yield Sleep(250)
    if compile_only:
        stem = "-" if inputs[0] == "-" else inputs[0].rsplit("/", 1)[-1].rsplit(".", 1)[0]
        m.write_file(out if out != "a.out" else f"{stem}.o", "")
        return 0
    ld = f"{prefix}/{t}/bin/ld"
    ldpath = f"{gcclib}/../../../../{t}/bin/ld"
    if not m.exists(ld) and bd.find_tool(m, f"{t}-ld") is None:
        ctx.write("collect2: fatal error: cannot find 'ld'\ncompilation terminated.\n", "err")
        return 1
    lib = f"{sysroot.rstrip('/')}/usr/lib"
    missing = [f for f in ("crt1.o", "crti.o") if not m.exists(f"{lib}/{f}")]
    if not m.exists(f"{lib}/libc.so") and not m.exists(f"{lib}/libc.so.6"):
        missing.append("-lc")
    if missing:
        for f in missing:
            ctx.write(f"{ldpath}: cannot find {f}: No such file or directory\n", "err")
        ctx.write("collect2: error: ld returned 1 exit status\n", "err")
        return 1
    exe = m.write_file(out, "")
    exe.update({"prog": "elf", "m": 0o755, "size": 15_800, "interp": "/lib64/ld-linux-x86-64.so.2",
                "cross": t == bd.TARGET, "sysroot": sysroot})
    return 0


@program("gcc")
def _gcc(ctx, args, path, node):
    return (yield from _gcc_compile(ctx, args, path, node))


@command("gcc", "the host's C compiler (only --version is simulated)", G, aliases=("cc",))
def _host_gcc(ctx, args):
    if "--version" in args or "-v" in args:
        ctx.write("gcc (GCC) 14.2.1 20240910\nCopyright (C) 2024 Free Software Foundation, Inc.\n")
        return 0
    if "-dumpmachine" in args:
        ctx.write(bd.HOST_TRIPLET + "\n")
        return 0
    ctx.write("gcc: compiling with the HOST compiler is not simulated (in LFS you use the "
              "cross compiler $LFS_TGT-gcc)\n", "dim")
    return 1


# ------------------------------------------------------------------ readelf
@command("readelf", "show ELF information: readelf -l FILE", G)
def _readelf(ctx, args):
    m = ctx.machine
    files = [a for a in args if not a.startswith("-")]
    if not files:
        raise UsageError("Usage: readelf <option(s)> elf-file(s)")
    status = 0
    for f in files:
        try:
            node = m.lookup(f)
        except FsError:
            ctx.write(f"readelf: Error: '{f}': No such file\n", "err")
            status = 1
            continue
        if node.get("prog") not in ("elf", "binutils", "gcc", "binary"):
            ctx.write("readelf: Error: Not an ELF file - it has the wrong magic bytes at the "
                      "start\n", "err")
            status = 1
            continue
        interp = node.get("interp")
        ctx.write("\nElf file type is DYN (Position-Independent Executable file)\n"
                  "Entry point 0x1020\nThere are 13 program headers, starting at offset 64\n\n"
                  "Program Headers:\n"
                  "  Type           Offset             VirtAddr           PhysAddr\n"
                  "                 FileSiz            MemSiz              Flags  Align\n"
                  "  PHDR           0x0000000000000040 0x0000000000000040 0x0000000000000040\n"
                  "                 0x00000000000002d8 0x00000000000002d8  R      0x8\n")
        if interp:
            ctx.write("  INTERP         0x0000000000000318 0x0000000000000318 0x0000000000000318\n"
                      "                 0x000000000000001c 0x000000000000001c  R      0x1\n"
                      f"      [Requesting program interpreter: {interp}]\n")
            if node.get("cross") and interp == "/lib64/ld-linux-x86-64.so.2":
                m.flag("sanity_ok")
        ctx.write("  LOAD           0x0000000000000000 0x0000000000000000 0x0000000000000000\n"
                  "                 0x0000000000000628 0x0000000000000628  R      0x1000\n")
    return status


# ------------------------------------------------------------------ make
def _make_args(args):
    targets, vars_, jflags, cdir = [], {}, [], None
    i = 0
    while i < len(args):
        a = args[i]
        if a == "-C" and i + 1 < len(args):
            cdir = args[i + 1]
            i += 2
            continue
        if re.fullmatch(r"-j\d*|--jobs(=\d+)?", a):
            jflags.append(a)
        elif a.startswith("-"):
            pass  # -s, -k, ... accepted silently
        elif "=" in a:
            k, _, v = a.partition("=")
            vars_[k] = v
        else:
            targets.append(a)
        i += 1
    return targets, vars_, jflags, cdir


@command("make", "build with the generated Makefile: make, make install, make DESTDIR=$LFS install", G)
def _make(ctx, args):
    m = ctx.machine
    targets, vars_, jflags, cdir = _make_args(args)
    old_cwd = m.cwd
    if cdir:
        m.cwd = m.abspath(cdir)
    try:
        here = m.lookup(".")
        mk = None
        for name in ("GNUmakefile", "makefile", "Makefile"):
            if name in here["c"]:
                mk = here["c"][name]
                break
        if mk is None:
            ctx.write("make: *** No targets specified and no makefile found.  Stop.\n", "err")
            return 2
        if mk.get("prog") == "kbuild":
            return (yield from _kbuild(ctx, here, targets or ["all"], jflags))
        meta = here.get("build")
        if mk.get("prog") != "makefile" or not meta:
            ctx.write("make: nothing to do here in the simulation (this Makefile was not "
                      "generated by a simulated configure)\n", "err")
            return 2
        n = bd.jobs(m, jflags)
        for target in targets or ["all"]:
            if target == "all":
                st = yield from _build(ctx, meta, n)
            elif target == "install":
                st = 0
                if meta["state"] == "configured":
                    st = yield from _build(ctx, meta, n)
                if st == 0:
                    st = yield from _install(ctx, here, meta, vars_.get("DESTDIR", ""))
            elif target in ("clean", "distclean"):
                meta["state"] = "configured"
                ctx.write(f"rm -f *.o *.a *.so (simulated {target})\n")
                st = 0
            elif target in ("check", "test"):
                ctx.write("(test suites are not run in chapter 5: there is nothing to test "
                          "against yet)\n", "dim")
                st = 0
            else:
                ctx.write(f"make: *** No rule to make target '{target}'.  Stop.\n", "err")
                st = 2
            if st:
                return st
        return 0
    finally:
        if cdir:
            m.cwd = old_cwd


def _build(ctx, meta, jobs_n):
    m = ctx.machine
    comp = meta["component"]
    if meta["state"] in ("built", "installed"):
        ctx.write("make: Nothing to be done for 'all'.\n")
        return 0
    sbu = bd.SBU.get(comp, 1.0)
    speed = bd.speedup(jobs_n)
    total_ms = sbu * bd.SBU_REAL_MS / speed
    files = bd.COMPILE_FILES.get(comp, ["main.c"])
    steps = 24
    failure = bd.build_failure(m, meta)
    ctx.write(f"make[1]: Entering directory '{m.cwd}'\n", "dim")
    for k in range(steps):
        if failure is not None and k == int(steps * 0.6):
            for line in failure.lines:
                ctx.write(line + "\n", "err")
            ctx.write(f"{failure}\nmake: *** [Makefile:1006: all] Error 2\n", "err")
            _hint(ctx, failure.hint)
            m.clock += sbu * bd.SBU_SECONDS * 0.6 / speed
            return 2
        ctx.write(bd.compile_line(comp, files[k % len(files)], meta, m.cwd) + "\n")
        yield Sleep(int(total_ms / steps))
    ctx.write(f"make[1]: Leaving directory '{m.cwd}'\n", "dim")
    secs = sbu * bd.SBU_SECONDS / speed
    m.clock += secs
    meta["state"] = "built"
    ctx.write(f"(simulated build time: {sbu:g} SBU / {jobs_n} job{'s' if jobs_n > 1 else ''} "
              f"= {int(secs // 60)}m {int(secs % 60)}s)\n", "dim")
    return 0


def _install(ctx, here, meta, destdir):
    m = ctx.machine
    comp = meta["component"]
    if comp == "glibc":
        top, topnode = bd.src_top(m, meta["srcdir"])
        meta["patched"] = "fhs" in (topnode or {}).get("patches", [])
    files = bd.install_files(m, meta, destdir)
    lfs = "/mnt/lfs"
    host_lib = [p for p, _ in files if not p.startswith(lfs + "/")]
    if comp in ("glibc", "libstdc++") and host_lib:
        if m.is_root:
            ctx.write(f"/usr/bin/install -c {comp} files into {host_lib[0].rsplit('/', 1)[0]}\n"
                      "Segmentation fault (core dumped)\nmake[2]: *** [Makefile:1234: install] "
                      "Error 139\n", "err")
            ctx.write("*** You just installed the NEW C library over the HOST's own one. On a "
                      "real machine every program would now crash and the host would be "
                      "broken. The simulation rolled it back. Use DESTDIR=$LFS. ***\n", "warn")
            return 2
    u, g = m._owner()
    shown = 0
    for path, node in files:
        parent = path.rsplit("/", 1)[0] or "/"
        try:
            m.mkdir(parent, parents=True)
            r = m.resolve(path, follow=False)
            if not m.can_write(r.parent):
                raise FsError("Permission denied")
        except FsError as exc:
            ctx.write(f"/usr/bin/install: cannot create regular file '{path}': {exc}\n"
                      "make[1]: *** [Makefile:1789: install] Error 1\n"
                      "make: *** [Makefile:2453: install] Error 2\n", "err")
            if not destdir and comp in ("glibc", "libstdc++"):
                _hint(ctx, "it tried to install into the host's /usr: use make DESTDIR=$LFS install")
            return 2
        node["u"], node["g"] = u, g
        r.parent["c"][r.name] = node
        if shown < 6:
            ctx.write(f"/usr/bin/install -c -m {node['m'] & 0o777:o} {path.rsplit('/', 1)[-1]} "
                      f"'{path}'\n")
            shown += 1
        if len(files) > 12 and files.index((path, node)) % 6 == 5:
            yield Sleep(40)
    if len(files) > shown:
        ctx.write(f"(... {len(files) - shown} more files installed)\n", "dim")
    meta["state"] = "installed"
    m.clock += 10 * bd.SBU.get(comp, 1)
    if comp == "glibc" and meta.get("patched"):
        m.flag("glibc_fhs")
    return 0


def _kbuild(ctx, here, targets, jflags):
    m = ctx.machine
    for target in targets:
        if target == "mrproper":
            yield Sleep(300)
            ctx.write("  CLEAN   scripts/basic\n  CLEAN   scripts/kconfig\n  CLEAN   usr/include\n")
            usr = here["c"].get("usr")
            if usr is not None:
                usr["c"].pop("include", None)
            here.pop("headers_built", None)
            m.clock += 5
        elif target == "headers":
            if "usr" not in here["c"]:
                here["c"]["usr"] = mkdir_node()
            for line in ("  HOSTCC  scripts/basic/fixdep", "  WRAP    arch/x86/include/generated/uapi/asm/bpf_perf_event.h",
                         "  SYSHDR  arch/x86/include/generated/uapi/asm/unistd_32.h",
                         "  SYSHDR  arch/x86/include/generated/uapi/asm/unistd_64.h",
                         "  HOSTCC  scripts/unifdef", "  HDRINST usr/include/asm-generic/errno-base.h",
                         "  HDRINST usr/include/linux/version.h", "  HDRINST usr/include/linux/types.h",
                         "  HDRINST usr/include/asm/unistd.h"):
                ctx.write(line + "\n")
                yield Sleep(int(bd.SBU["linux"] * bd.SBU_REAL_MS / 9 / bd.speedup(bd.jobs(m, jflags))))
            u, g = m._owner()
            if not m.can_write(here["c"]["usr"]):
                ctx.write("mkdir: cannot create directory 'usr/include': Permission denied\n", "err")
                return 2
            here["c"]["usr"]["c"]["include"] = bd.kernel_headers_tree(u, g)
            here["headers_built"] = True
            m.clock += bd.SBU["linux"] * bd.SBU_SECONDS
        elif target in ("all", "vmlinux", "bzImage", "modules", "modules_install", "install"):
            ctx.write("make: building the kernel itself belongs to chapter 10 and is not "
                      "simulated yet\n", "err")
            _hint(ctx, "for chapter 5 you only need: make mrproper; make headers")
            return 2
        elif target in ("headers_install",):
            ctx.write("make: use 'make headers' and copy usr/include (book 5.4)\n", "err")
            return 2
        else:
            ctx.write(f"make: *** No rule to make target '{target}'.  Stop.\n", "err")
            return 2
    return 0
