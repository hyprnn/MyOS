"""Programs of the new system that chapter 8 runs inside the chroot.

They exist as files installed by earlier packages (node "tool" = name) and are dispatched
from build._binary through TOOL_HANDLERS: pip3, ninja, meson, python3, perl, localedef,
zic, tzselect, udevadm, strip, autoreconf; plus GCC's contrib/test_summary script.
"""
from __future__ import annotations

import re

from ..shell.runner import Input, Sleep, program
from ..sim import build as bd
from ..sim import ch8
from ..sim.vfs import FsError
from .build import TOOL_HANDLERS, _build, _hint, _install, _tests8


def handler(*names):
    def deco(fn):
        for n in names:
            TOOL_HANDLERS[n] = fn
        return fn
    return deco


def _top(m, path="."):
    return bd.src_top(m, path)


# ------------------------------------------------------------------ python / pip
@handler("python3.12", "python3")
def _python(ctx, args, path, node):
    m = ctx.machine
    if "--version" in args or "-V" in args:
        ctx.write("Python 3.12.5\n")
        return 0
    if args[:1] == ["-c"]:
        code = args[1] if len(args) > 1 else ""
        if "spawn" in code:
            pts = any(mt["path"] == m.croot + "/dev/pts" and mt["type"] == "devpts"
                      for mt in m.mounts)
            if not pts:
                ctx.write("Traceback (most recent call last):\n  ...\n"
                          "OSError: out of pty devices\n", "err")
                return 1
            ctx.write("ok\n")
            return 0
        ctx.write("(python -c: simulated, nothing printed)\n", "dim")
        return 0
    if args[:1] == ["configure.py"]:
        if not m.exists("configure.py"):
            ctx.write("python3: can't open file 'configure.py': [Errno 2] No such file or "
                      "directory\n", "err")
            return 2
        for line in ("bootstrapping ninja...", "wrote build.ninja.",
                     "bootstrap complete.  rebuilding...", "[1/34] CXX build/build.o",
                     "[34/34] LINK ninja"):
            ctx.write(line + "\n")
            yield Sleep(120)
        exe = m.write_file("ninja", "")
        exe.update({"prog": "binary", "tool": "ninja", "cross": True, "ch8": True, "m": 0o755,
                    "size": 280_000, "target": ch8.TRIPLET})
        m.clock += 20
        return 0
    ctx.write("(interactive python is not simulated)\n", "dim")
    return 0


def _norm(name: str) -> str:
    return re.sub(r"[-_.]+", "-", name).lower()


@handler("pip3", "pip3.12", "pip")
def _pip(ctx, args, path, node):
    m = ctx.machine
    if not args or args[0] in ("--version", "-V"):
        ctx.write("pip 24.2 from /usr/lib/python3.12/site-packages/pip (python 3.12)\n")
        return 0
    cmd, rest = args[0], args[1:]
    if cmd == "wheel":
        outdir = "."
        positional = []
        i = 0
        while i < len(rest):
            if rest[i] == "-w" and i + 1 < len(rest):
                outdir = rest[i + 1]
                i += 2
                continue
            if not rest[i].startswith("-"):
                positional.append(rest[i])
            i += 1
        src = positional[0] if positional else "."
        top, topnode = _top(m, src)
        spec = ch8.BY_KEY.get((topnode or {}).get("srcpkg"))
        if spec is None or not m.exists(f"{src}/pyproject.toml"):
            ctx.write(f"ERROR: Directory '{src}' is not installable. Neither 'setup.py' nor "
                      f"'pyproject.toml' found.\n", "err")
            return 1
        if spec.backend and not m.exists(
                f"/usr/lib/python3.12/site-packages/{spec.backend.replace('-', '_')}"):
            ctx.write(f"ERROR: Could not build wheels for {spec.key}\n"
                      f"ModuleNotFoundError: No module named '{spec.backend}'\n", "err")
            _hint(ctx, f"install {spec.backend} first (it is the build backend)")
            return 1
        name = spec.steps[2].split()[-1]
        version = spec.title.rsplit("-", 1)[-1]
        ctx.write(f"Processing {top}\n  Building wheel for {name} (pyproject.toml) ... done\n")
        yield Sleep(300)
        m.mkdir(outdir, parents=True)
        whl = m.write_file(f"{outdir}/{name}-{version}-py3-none-any.whl", "")
        whl.update({"wheel": spec.key, "size": 90_000})
        ctx.write(f"Successfully built {name}\n")
        return 0
    if cmd == "install":
        links, names = ".", []
        i = 0
        while i < len(rest):
            a = rest[i]
            if a in ("--find-links", "-f") and i + 1 < len(rest):
                links = rest[i + 1]
                i += 2
                continue
            if a.startswith("--find-links="):
                links = a.split("=", 1)[1]
            elif not a.startswith("-"):
                names.append(a)
            i += 1
        try:
            d = m.lookup(links)
        except FsError:
            ctx.write(f"ERROR: Could not find a version that satisfies the requirement "
                      f"{' '.join(names)}\n", "err")
            return 1
        for want in names:
            whl = [(n, c) for n, c in d["c"].items() if c.get("wheel")
                   and _norm(n).startswith(_norm(want) + "-")]
            if not whl:
                ctx.write(f"ERROR: Could not find a version that satisfies the requirement "
                          f"{want} (from versions: none)\nERROR: No matching distribution "
                          f"found for {want}\n", "err")
                return 1
            key = whl[0][1]["wheel"]
            ctx.write(f"Looking in links: {links}\nProcessing ./{links}/{whl[0][0]}\n"
                      f"Installing collected packages: {want}\n")
            meta = {"component": key, "opts": {}, "srcdir": "", "ch8": True,
                    "state": "built"}
            here = m.lookup(".")
            st = yield from _install(ctx, here, meta, "", {})
            if st:
                return st
            ctx.write(f"Successfully installed {want}-{ch8.BY_KEY[key].title.rsplit('-', 1)[-1]}\n")
        return 0
    ctx.write(f"ERROR: unknown command \"{cmd}\" (simulated: wheel, install)\n", "err")
    return 1


# ------------------------------------------------------------------ meson / ninja
@handler("meson")
def _meson(ctx, args, path, node):
    m = ctx.machine
    if not args or args[0] in ("--version", "-v"):
        ctx.write("1.5.1\n")
        return 0
    if args[0] != "setup":
        ctx.write("meson: only 'meson setup' is simulated\n", "err")
        return 1
    rest = args[1:]
    opts = {}
    positional = []
    i = 0
    while i < len(rest):
        a = rest[i]
        if a == "-D" and i + 1 < len(rest):
            k, _, v = rest[i + 1].partition("=")
            opts[f"-D{k}"] = v
            i += 2
            continue
        if a.startswith("--"):
            k, _, v = a.partition("=")
            opts[k] = v
        elif not a.startswith("-"):
            positional.append(a)
        i += 1
    arg = positional[0] if positional else "."
    if m.exists(f"{arg}/meson.build"):
        srcdir, builddir = arg, "."
    elif m.exists("meson.build"):
        srcdir, builddir = ".", arg
    else:
        ctx.write(f"ERROR: Neither source directory '{arg}' nor build directory '.' contain "
                  f"a build file meson.build.\n", "err")
        return 1
    top, topnode = _top(m, srcdir)
    key = (topnode or {}).get("srcpkg")
    if key not in ch8.BY_KEY:
        ctx.write("ERROR: this project is not simulated\n", "err")
        return 1
    ctx.write(f"The Meson build system\nVersion: 1.5.1\nSource dir: {top}\n"
              f"Build dir: {m.abspath(builddir)}\nBuild type: native build\n"
              f"Project name: {key}\nC compiler for the host machine: cc (gcc 14.2.0)\n")
    yield Sleep(400)
    m.mkdir(builddir, parents=True)
    bn = m.write_file(f"{builddir}/build.ninja", "# This is the build file for project\n")
    bn["prog"] = "ninjafile"
    m.lookup(builddir)["build"] = {"component": key, "opts": opts, "srcdir": top,
                                   "state": "configured", "ch8": True}
    ctx.write("Build targets in project: 1342\n\nFound ninja-1.12.1 at /usr/bin/ninja\n")
    return 0


@handler("ninja")
def _ninja(ctx, args, path, node):
    m = ctx.machine
    if "--version" in args:
        ctx.write("1.12.1\n")
        return 0
    here = m.lookup(".")
    meta = here.get("build")
    if "build.ninja" not in here["c"] or not meta:
        ctx.write("ninja: error: loading 'build.ninja': No such file or directory\n", "err")
        return 1
    targets = [a for a in args if not a.startswith("-")] or ["all"]
    spec = ch8.BY_KEY[meta["component"]]
    if "install" in targets:
        if meta["state"] == "configured":
            st = yield from _build(ctx, meta, 8, {})
            if st:
                return st
        return (yield from _install(ctx, here, meta, "", {}))
    if "test" in targets:
        return (yield from _tests8(ctx, here, meta, spec, 8, {}))
    return (yield from _build(ctx, meta, 8, {}))


# ------------------------------------------------------------------ perl
@handler("perl", "perl5.40.0")
def _perl(ctx, args, path, node):
    m = ctx.machine
    if "-v" in args or "--version" in args:
        ctx.write("This is perl 5, version 40, subversion 0 (v5.40.0) built for "
                  "x86_64-linux-thread-multi\n")
        return 0
    if args[:1] == ["Makefile.PL"]:
        if not m.exists("Makefile.PL"):
            ctx.write("Can't open perl script \"Makefile.PL\": No such file or directory\n", "err")
            return 2
        top, topnode = _top(m)
        key = (topnode or {}).get("srcpkg")
        ctx.write("Checking if your kit is complete...\nLooks good\n"
                  f"Generating a Unix-style Makefile\nWriting Makefile for {key}\n")
        yield Sleep(200)
        mk = m.write_file("Makefile", f"# Makefile generated by MakeMaker for {key}\n")
        mk["prog"] = "makefile"
        m.lookup(".")["build"] = {"component": key, "opts": {"--prefix": "/usr"},
                                  "srcdir": top, "state": "configured", "ch8": True}
        return 0
    ctx.write("(perl scripts are not simulated)\n", "dim")
    return 0


# ------------------------------------------------------------------ glibc tools
@handler("localedef")
def _localedef(ctx, args, path, node):
    m = ctx.machine
    vals, names = {}, []
    i = 0
    while i < len(args):
        a = args[i]
        if a in ("-i", "-f") and i + 1 < len(args):
            vals[a] = args[i + 1]
            i += 2
            continue
        if not a.startswith("-"):
            names.append(a)
        i += 1
    if not names or "-i" not in vals:
        ctx.write("localedef: usage: localedef -i INPUT -f CHARMAP NAME\n", "err")
        return 1
    if not m.exists(f"/usr/share/i18n/locales/{vals['-i']}"):
        ctx.write(f"localedef: cannot open locale definition file `{vals['-i']}': No such file "
                  f"or directory\n", "err")
        return 4
    if not m.exists(f"/usr/share/i18n/charmaps/{vals.get('-f', 'UTF-8')}.gz"):
        ctx.write(f"localedef: character map file `{vals.get('-f')}' not found\n", "err")
        return 4
    yield Sleep(150)
    m.mkdir("/usr/lib/locale", parents=True)
    try:
        arch = m.lookup("/usr/lib/locale/locale-archive")
        data = arch.get("data", "")
    except FsError:
        data = ""
    if names[0] + "\n" not in data:
        data += names[0] + "\n"
    m.write_file("/usr/lib/locale/locale-archive", data)
    return 0


@handler("zic")
def _zic(ctx, args, path, node):
    m = ctx.machine
    outdir, files, posixrules = "/usr/share/zoneinfo", [], None
    i = 0
    while i < len(args):
        a = args[i]
        if a in ("-L", "-d", "-p") and i + 1 < len(args):
            if a == "-d":
                outdir = args[i + 1]
            elif a == "-p":
                posixrules = args[i + 1]
            i += 2
            continue
        files.append(a)
        i += 1
    m.mkdir(outdir, parents=True)
    if posixrules:
        if not m.exists(f"{outdir}/{posixrules}"):
            ctx.write(f"zic: Can't open {posixrules}: No such file or directory\n", "err")
            return 1
        r = m.resolve(f"{outdir}/posixrules", follow=False)
        if r.node is not None:
            m.remove(f"{outdir}/posixrules")
        m.symlink(posixrules, f"{outdir}/posixrules")
    for f in files:
        if not m.exists(f):
            ctx.write(f"zic: Can't open {f}: No such file or directory\n", "err")
            return 1
        for zone in ch8.TZ_REGIONS.get(f.rsplit("/", 1)[-1], []):
            target = f"{outdir}/{zone}"
            m.mkdir(target.rsplit("/", 1)[0], parents=True)
            m.write_file(target, "")["size"] = 1_500
    return 0


TZ_MENU = [("Africa", [("Egypt", "Africa/Cairo"), ("South Africa", "Africa/Johannesburg")]),
           ("Americas", [("United States", "America/New_York"), ("Brazil", "America/Sao_Paulo")]),
           ("Antarctica", [("Antarctica", "Antarctica/Troll")]),
           ("Asia", [("Japan", "Asia/Tokyo"), ("China", "Asia/Shanghai"),
                     ("India", "Asia/Kolkata")]),
           ("Atlantic Ocean", [("Iceland", "Etc/GMT")]),
           ("Australia", [("Australia", "Australia/Sydney")]),
           ("Europe", [("Germany", "Europe/Berlin"), ("Russia", None),
                       ("United Kingdom", "Europe/London"), ("Belarus", "Europe/Minsk"),
                       ("Ukraine", "Europe/Kyiv")])]
TZ_RUSSIA = [("MSK-01 - Kaliningrad", "Europe/Kaliningrad"), ("MSK+00 - Moscow area",
             "Europe/Moscow"), ("MSK+02 - Urals", "Asia/Yekaterinburg"),
             ("MSK+04 - Novosibirsk", "Asia/Novosibirsk"), ("MSK+07 - Vladivostok",
             "Asia/Vladivostok")]


def _choose(ctx, items):
    for i, label in enumerate(items, 1):
        ctx.write(f"{i:>2}) {label}\n")
    while True:
        ans = yield Input("#? ")
        if ans is None:
            return None
        if ans.strip().isdigit() and 1 <= int(ans) <= len(items):
            return int(ans) - 1


@handler("tzselect")
def _tzselect(ctx, args, path, node):
    ctx.write("Please identify a location so that time zone rules can be set correctly.\n"
              "Please select a continent, ocean, \"coord\", or \"TZ\".\n")
    k = yield from _choose(ctx, [c for c, _ in TZ_MENU])
    if k is None:
        return 1
    continent, countries = TZ_MENU[k]
    ctx.write("Please select a country whose clocks agree with yours.\n")
    k = yield from _choose(ctx, [c for c, _ in countries])
    if k is None:
        return 1
    country, zone = countries[k]
    detail = ""
    if zone is None:
        ctx.write("Please select one of the following timezones.\n")
        k = yield from _choose(ctx, [z for z, _ in TZ_RUSSIA])
        if k is None:
            return 1
        detail, zone = TZ_RUSSIA[k]
    ctx.write(f"\nThe following information has been given:\n\n\t{country}\n"
              + (f"\t{detail}\n" if detail else "")
              + f"\nTherefore TZ='{zone}' will be used.\nIs the above information OK?\n")
    k = yield from _choose(ctx, ["Yes", "No"])
    if k != 0:
        return 1
    ctx.write(f"\nYou can make this change permanent for yourself by appending the line\n"
              f"\tTZ='{zone}'; export TZ\nto the file '.profile' in your home directory; "
              f"then log out and log in again.\n\nHere is that TZ value again, this time on "
              f"standard output so that you\ncan use the /usr/bin/tzselect command in shell "
              f"scripts:\n{zone}\n")
    return 0


# ------------------------------------------------------------------ others
@handler("udevadm")
def _udevadm(ctx, args, path, node):
    m = ctx.machine
    if args[:2] == ["hwdb", "--update"]:
        m.mkdir("/etc/udev", parents=True)
        m.write_file("/etc/udev/hwdb.bin", "")["size"] = 11_000_000
        return 0
    if "--version" in args:
        ctx.write("256\n")
        return 0
    ctx.write("udevadm: only 'udevadm hwdb --update' is simulated\n", "dim")
    return 0


@handler("strip")
def _strip(ctx, args, path, node):
    m = ctx.machine
    files = [a for a in args if not a.startswith("-")]
    status = 0
    for f in files:
        try:
            n = m.lookup(f)
        except FsError as exc:
            ctx.write(f"strip: '{f}': {exc}\n", "err")
            status = 1
            continue
        if n["t"] != "f" or n.get("data"):
            ctx.write(f"strip: {f}: file format not recognized\n", "err")
            status = 1
            continue
        if not n.get("stripped"):
            n["size"] = int(n.get("size", 0) * 0.55)
            n["stripped"] = True
    if files and status == 0:
        m.flag("stripped")
    return status


@handler("autoreconf")
def _autoreconf(ctx, args, path, node):
    for line in ("autoreconf: export WARNINGS=", "autoreconf: Entering directory '.'",
                 "autoreconf: running: aclocal --force -I m4", "autoreconf: running: "
                 "/usr/bin/autoconf --force", "autoreconf: running: /usr/bin/autoheader --force",
                 "autoreconf: running: automake --add-missing --copy --force-missing",
                 "autoreconf: Leaving directory '.'"):
        ctx.write(line + "\n")
        yield Sleep(80)
    return 0


@program("test_summary")
def _test_summary(ctx, args, path, node):
    spec = ch8.BY_KEY["gcc"]
    m = ctx.machine
    if not m.has_flag("tested:gcc"):
        ctx.write("(no test results found: run the test suite first)\n", "dim")
        return 1
    ctx.write("cat <<'EOF' |\nLAST_UPDATED: Obtained from git: releases/gcc-14.2.0\n\n"
              "Native configuration is x86_64-pc-linux-gnu\n\n" + spec.tests + "\nEOF\n")
    return 0

