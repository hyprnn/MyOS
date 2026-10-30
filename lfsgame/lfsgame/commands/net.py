"""wget and md5sum. The 'internet' only knows the LFS website and the package mirrors."""
from __future__ import annotations

import re
import time

from ..shell.opts import Opts
from ..shell.runner import Sleep, UsageError, command
from ..sim import packages as pk
from ..sim.vfs import FsError, human, node_md5

G = "network"
SPEED = 9.4 * 1024 * 1024        # bytes/s shown in wget output
LIST_FILES = {"wget-list-sysv": pk.wget_list_text, "wget-list": pk.wget_list_text,
              "md5sums": pk.md5sums_text}
URL_RE = re.compile(r"^(https?|ftp)://([^/]+)(/.*)?$")


def _stamp():
    return time.strftime("%Y-%m-%d %H:%M:%S")


def _ip(host):
    h = sum(map(ord, host))
    return f"{140 + h % 80}.{h % 251}.{(h * 7) % 253}.{(h * 13) % 254 + 1}"


def _lookup(url):
    """-> ('list', name) | ('pkg', Package) | None"""
    mo = URL_RE.match(url)
    if not mo:
        return None
    host, path = mo.group(2), mo.group(3) or "/"
    base = path.rsplit("/", 1)[-1]
    if host.endswith("linuxfromscratch.org") and base in LIST_FILES:
        return "list", base
    pkg = pk.BY_URL.get(url) or pk.BY_FILE.get(base)
    if pkg is not None:
        return "pkg", pkg
    return None


def _free_name(m, path):
    if not m.exists(path):
        return path
    i = 1
    while m.exists(f"{path}.{i}"):
        i += 1
    return f"{path}.{i}"


@command("wget", "download files (-c continue, -P DIR, -i LIST)", G)
def _wget(ctx, args):
    m = ctx.machine
    o = Opts(args, "cqvN", "PiO",
             {"continue": "c", "quiet": "q", "no-check-certificate": "_", "verbose": "v"},
             {"directory-prefix": "P", "input-file": "i", "output-document": "O"})
    urls = list(o.args)
    if "i" in o.vals:
        try:
            node = m.lookup(o.vals["i"])
        except FsError as exc:
            ctx.write(f"{o.vals['i']}: {exc}\nNo URLs found in {o.vals['i']}.\n", "err")
            return 1
        urls += [ln.strip() for ln in node.get("data", "").splitlines()
                 if ln.strip() and not ln.startswith("#")]
    if not urls:
        raise UsageError("missing URL\nUsage: wget [OPTION]... [URL]...")
    prefix = o.vals.get("P", ".")
    if not m.isdir(prefix):
        ctx.write(f"{prefix}: No such file or directory\n", "err")
        return 3
    quiet = "q" in o
    out = (lambda *a: None) if quiet else ctx.write
    wall = 0.0
    got_files, got_bytes, failed = 0, 0, 0
    last_host = None
    for url in urls:
        mo = URL_RE.match(url)
        out(f"--{_stamp()}--  {url}\n")
        if not mo:
            ctx.write(f"{url}: Unsupported scheme or invalid URL.\n", "err")
            failed += 1
            continue
        host = mo.group(2)
        found = _lookup(url)
        if host != last_host:
            yield Sleep(60)
            out(f"Resolving {host} ({host})... ")
            if found is None and not any(host == pk_host(p.url) for p in pk.PACKAGES) \
                    and not host.endswith("linuxfromscratch.org"):
                out("failed: Name or service not known.\n")
                ctx.write(f"wget: unable to resolve host address ‘{host}’\n"
                          "(the simulated internet only knows the LFS site and package mirrors)\n",
                          "err")
                failed += 1
                last_host = None
                continue
            out(f"{_ip(host)}\nConnecting to {host} ({host})|{_ip(host)}|:443... connected.\n")
            last_host = host
        else:
            out(f"Reusing existing connection to {host}:443.\n")
        if found is None:
            out("HTTP request sent, awaiting response... ")
            ctx.write("404 Not Found\n", "err")
            ctx.write(f"{_stamp()} ERROR 404: Not Found.\n\n", "err")
            failed += 1
            continue
        kind, what = found
        if kind == "list":
            name, size, data = what, None, LIST_FILES[what]()
            size = len(data.encode())
        else:
            name, size, data = what.file, what.size, None
        dest = (o.vals["O"] if "O" in o.vals else f"{prefix.rstrip('/')}/{name}")
        existing = m.resolve(dest).node
        resume_from = 0
        if existing is not None and existing["t"] == "f" and "c" in o:
            if existing.get("size") == size and not existing.get("partial"):
                out("HTTP request sent, awaiting response... 416 Requested Range Not Satisfiable\n\n"
                    "    The file is already fully retrieved; nothing to do.\n\n")
                continue
            resume_from = existing.get("size", 0) if existing.get("partial") else 0
        elif existing is not None and "O" not in o.vals:
            dest = _free_name(m, dest)
        if resume_from:
            out("HTTP request sent, awaiting response... 206 Partial Content\n"
                f"Length: {size} ({human(size)}), {size - resume_from} "
                f"({human(size - resume_from)}) remaining [application/x-xz]\n")
        else:
            ctype = "text/plain" if kind == "list" else "application/x-tar"
            out(f"HTTP request sent, awaiting response... 200 OK\n"
                f"Length: {size} ({human(size)}) [{ctype}]\n")
        short = dest if ("P" in o.vals or "O" in o.vals) else dest.rsplit("/", 1)[-1]
        out(f"Saving to: ‘{short}’\n\n")
        todo = size - resume_from
        secs = todo / SPEED
        wall += secs + 0.3
        glitch = (kind == "pkg" and what.file == m.glitch_file and not resume_from
                  and not m.has_flag("glitch_done"))
        yield Sleep(int(min(max(secs * 40, 40), 500)))
        if glitch:
            cut = int(size * 0.43)
            out(f"{name[:24]:<24}  43%[=======>        ] {human(cut):>6}  "
                f"{human(SPEED)}B/s    in {cut / SPEED:.1f}s\n\n")
            ctx.write(f"{_stamp()} ({human(SPEED)}B/s) - Read error at byte {cut}/{size} "
                      f"(Connection reset by peer). Giving up.\n\n", "err")
            m.flag("glitch_done")
            try:
                m.put_blob(dest, cut, "0" * 32, partial=True)
            except FsError as exc:
                ctx.write(f"{dest}: {exc}\n", "err")
            failed += 1
            continue
        try:
            if kind == "list":
                m.write_file(dest, data)
            else:
                m.put_blob(dest, size, pk.fake_md5(what.file),
                           data=pk.PATCH_TEXT.get(what.file, ""))
        except FsError as exc:
            ctx.write(f"{dest}: {exc}\nCannot write to ‘{dest}’ ({exc}).\n", "err")
            failed += 1
            continue
        out(f"{name[:24]:<24} 100%[===================>] {human(size):>6}  "
            f"{human(SPEED)}B/s    in {secs:.1f}s\n\n")
        out(f"{_stamp()} ({human(SPEED)}B/s) - ‘{short}’ saved [{size}/{size}]\n\n")
        got_files += 1
        got_bytes += todo
    if len(urls) > 1:
        out(f"FINISHED --{_stamp()}--\nTotal wall clock time: {wall:.1f}s\n"
            f"Downloaded: {got_files} files, {human(got_bytes)} in {got_bytes / SPEED:.1f}s "
            f"({human(SPEED)}B/s)\n")
    return 4 if failed else 0


def pk_host(url):
    mo = URL_RE.match(url)
    return mo.group(2) if mo else ""


@command("md5sum", "compute or check MD5 sums (-c FILE)", G)
def _md5sum(ctx, args):
    m = ctx.machine
    o = Opts(args, "cq", "", {"check": "c", "quiet": "q"})
    if not o.args:
        raise UsageError("reading from stdin is not simulated; give a file name")
    if "c" not in o:
        status = 0
        for p in o.args:
            try:
                node = m.lookup(p)
                if node["t"] == "d":
                    raise FsError("Is a directory")
                ctx.write(f"{node_md5(node)}  {p}\n")
            except FsError as exc:
                ctx.write(f"md5sum: {p}: {exc}\n", "err")
                status = 1
        return status
    status = 0
    for listfile in o.args:
        try:
            lines = m.lookup(listfile).get("data", "").splitlines()
        except FsError as exc:
            ctx.write(f"md5sum: {listfile}: {exc}\n", "err")
            return 1
        unread = bad = good = 0
        names = set()
        for i, line in enumerate(lines):
            parts = line.split(None, 1)
            if len(parts) != 2:
                continue
            want, name = parts[0], parts[1].lstrip("*").strip()
            if i % 6 == 5:
                yield Sleep(40)
            try:
                node = m.lookup(name)
            except FsError as exc:
                ctx.write(f"md5sum: {name}: {exc}\n", "err")
                ctx.write(f"{name}: FAILED open or read\n", "err")
                unread += 1
                continue
            if node_md5(node) == want:
                good += 1
                names.add(name)
                if "q" not in o:
                    ctx.write(f"{name}: OK\n")
            else:
                ctx.write(f"{name}: FAILED\n", "err")
                bad += 1
        if unread:
            ctx.write(f"md5sum: WARNING: {unread} listed file{'s' if unread > 1 else ''} "
                      f"could not be read\n", "warn")
        if bad:
            ctx.write(f"md5sum: WARNING: {bad} computed checksum{'s' if bad > 1 else ''} "
                      f"did NOT match\n", "warn")
        if unread or bad:
            status = 1
        elif all(p.file in names for p in pk.PACKAGES):
            m.flag("md5_ok")
    return status
