# 0007 — LFS chapter 8: installing the basic system software
Version 0.7.0 · 2026-10-30

## Goal
User (in Russian): "continue, make chapter 8". Implement LFS 12.2 (System V) chapter 8:
8.1 introduction, 8.2 package management, 8.3–8.82 the ~80 packages of the final system
(Man-pages … Sysvinit), 8.83 about debugging symbols, 8.84 stripping, 8.85 cleaning up.
After that the book says "to be continued" (chapter 9).

IMPORTANT: the real LFS site (www.linuxfromscratch.org) was NOT reachable from the
session (proxy 403 / DNS failure), so every command was written from memory of
LFS 12.2. Versions, options and seds are believed to be right but were not checked
against the real book. To check them, allow www.linuxfromscratch.org in the session's
network settings and compare section by section.

## What changed
- NEW `lfsgame/sim/ch8.py` — the data-driven heart of the chapter.
  - `Pkg` dataclass: key, sec (book section), title, sbu, about, steps (book lines; `$`
    commands, `!` notes, plain text), bins/sbins/libs/files/links (what `make install`
    puts in place; `{P}` = prefix), need (preconditions: `("flag", f)`, `("file", p)`,
    ...), tests/test_status/tsbu (what `make check` prints), style (`autotools`,
    `make`, `meson`/`ninja`, `pip`, `perl`, `none`), confdir/builddir (e.g. glibc must
    be configured in `build/`), srcfiles, products (files left in the build tree),
    subdirs, critical (test suites the book says never to skip), patch, prefix,
    requires, from_src (files copied from the source tree on install), cond, makefile,
    backend (pip), topdir, extra_targets, frame.
  - `PKGS`: 80 packages in book order, sections 8.3–8.82: man-pages, iana-etc, glibc
    (+ nsswitch.conf, locales via localedef, tzdata via zic, tzselect, ld.so.conf),
    zlib, bzip2, xz, lz4, zstd, file, readline (8.12), m4, bc, flex, tcl, expect,
    dejagnu, pkgconf, binutils, gmp, mpfr, mpc, attr, acl, libcap, libxcrypt, shadow,
    gcc (tests as tester + the toolchain sanity checks), ncurses, sed, psmisc, gettext,
    bison, grep, bash, libtool, gdbm, gperf, expat, inetutils, less, perl, xml-parser,
    intltool, autoconf, automake, openssl, kmod, elfutils (libelf only), libffi, python,
    flit-core, wheel, setuptools, ninja, meson, coreutils, check, diffutils, gawk,
    findutils, groff, grub, gzip, iproute2, kbd, libpipeline, make, patch, tar, texinfo,
    vim, markupsafe, jinja2, systemd (udev only), man-db, procps-ng, util-linux,
    e2fsprogs, sysklogd, sysvinit.
  - `is_ch8(m, component)`: a build is chapter 8 when the chroot is active and the
    chapter-7 cleanup objective is done (flag `obj:cleanup7`).
  - `install_paths(pkg, opts, make_vars)`, `book_text()` (generates the book file),
    `walkthrough()` (list of `(command, must_succeed)`; steps written as `?$` in the
    table may fail, e.g. known failing tests), INTRO (8.1 incl. what to do about
    archives missing from old saves, 8.2), OUTRO (8.83, 8.84, 8.85, 9.0), OUTRO_CMDS,
    WALK_INPUTS (answers for interactive tzselect and passwd), TZ_REGIONS/TZ_FILES.
- NEW `lfsgame/commands/ch8tools.py` — programs that exist only in the new system:
  `python3`/`python3.12` (`-c`, `-m pip`), `pip3 wheel` / `pip3 install` (checks that the
  build backend — flit-core/setuptools — is installed, `--no-index --find-links dist`),
  `meson setup` (checks prefix/buildtype, writes build.ninja), `ninja` (build, `test`,
  `install`), `perl Makefile.PL`, `localedef` (writes
  /usr/lib/locale/locale-archive; `-i xx_XX` fails with status 4 like the real one for an
  unknown locale), `zic`, `tzselect` (interactive menus via `Input`), `udevadm hwdb
  --update`, `strip`, `autoreconf`, and program `test_summary` (gcc's contrib script).
- `lfsgame/sim/build.py`: chapter 8 branch placed BEFORE the "built in a later chapter"
  check: `_configure8` (validates prefix/required options per Pkg), `_apply_ch8_tree`
  (source tree incl. tzdata's flat tree), `ch8_files`, `_ch8_bin`, `_ch8_content`;
  `install_files` routes `meta["ch8"]`; the old FILE_COMPILE failure check skips ch8.
- `lfsgame/commands/build.py`: generic `_patch` (PATCH_FILES; patching appends a marker
  so applying twice fails like the real `patch`), tar for flat archives and preset
  source dirs, `make -f FILE`, `_make8` (SBU timing from Pkg.sbu), `_tests8` (prints
  test summaries, refuses root where the real suite refuses — coreutils configure
  needs FORCE_UNSAFE_CONFIGURE, tests as `tester` need `chown -R tester .`, expect needs
  /dev/pts), `CHECK_TARGETS` (check, test, tests, -k check), ninja-style output,
  `_install` sets `installed8:<key>` flags and warns if glibc is installed without
  /etc/ld.so.conf, `TOOL_HANDLERS` for programs installed into the chroot, gcc verbose
  output for the sanity checks (`-v -Wl,--verbose`, `readelf -l`, grep of the log).
- `lfsgame/shell/parser.py`, `runner.py`: redirections `2>`, `2>>`, `2>&1`, `&>` (Sub
  has an `err` writer); `ulimit` added to CHROOT_BUILTINS.
- `lfsgame/commands/text.py`: `grep -A/-B/-o`, `sed` commands c/a/i and `\n` in
  replacements, new `xargs`, `tee`, `find -depth/-xdev`.
- `lfsgame/commands/files.py`: `ln -r`. `commands/chroot.py`: `install -D`, copies
  keep the ch8 metadata key. `commands/core.py`: `ulimit`; `whoami`/`id` use
  `Machine.uid()`. `commands/users.py`: `useradd -D [--gid]`, `pwconv`, `grpconv`,
  `userdel`, `passwd` and `su USER -c "..."` inside the chroot.
- `lfsgame/sim/machine.py`: `uid()` (effective uid of the current session).
- `lfsgame/sim/packages.py`: all chapter-8 archives and patches added to the package
  list / wget-list / md5sums.
- `lfsgame/sim/game.py`: completed objectives also set flags `obj:<id>` (used by
  `is_ch8` and by `need` conditions).
- `lfsgame/sim/objectives.py`: `_ch8_objectives()` builds one objective per package
  (`<key>8`), plus glibc configuration (`glibc8cfg`), optional `strip8`, `cleanup8`
  (`_cleanup8_ok`: no `*.la` files in /usr/lib(exec), no x86_64-lfs-linux-gnu leftovers
  in /usr, no tester user/home, sysvinit installed); helpers `_ch8_ok`,
  `_need_ok`, `_glibc_cfg_ok`. 136 objectives in total (4 optional).
- `lfsgame/commands/game.py`: chapter "8" in CHAPTERS, new messages;
  `lfsgame/terminal.py`: end-of-chapter message.
- NEW `tools/gen_ch8_book.py` writes `lfsgame/book/content/09-final-system.txt` from
  the table (`python tools/gen_ch8_book.py`; a test fails when the file is stale).
  `08-chroot.txt`: the "8.0 to be continued" section was removed.
- Book appendix: new redirections and commands.
- Tests: `tests/helpers.py` `ch8_walkthrough()`; `test_sim.py` walkthrough now goes to
  8.85; `test_book.py` (assignment words like `CC=gcc` skipped in the registered-command
  check; generated-book freshness test); NEW `tests/test_ch8.py` (12 tests: book
  order/count, coreutils refuses root configure, glibc needs build dir, gcc needs
  gmp/mpfr/mpc, tests as tester need chown, pip needs a backend, expect needs ptys,
  ch7 → ch8 version detection, glibc configuration incl. bad locale, man-pages libxcrypt
  pages, openssl static libs without the sed, gcc sanity checks + full chapter).

## Design decisions
- One table instead of 80 hand-written build models: the book text, the walkthrough
  test and the objectives are all generated from it, so they cannot drift apart.
- Chapter detection by flags (`obj:cleanup7`), not by version numbers: the same package
  (e.g. bison) is chapter 7 before the cleanup and chapter 8 after it.
- Tests (`make check`) are simulated as summaries ("# PASS: 1234 / # FAIL: 0"), with the
  real book's "known failures" reproduced as text where the book mentions them.
- Old saves: archives added in this version are missing from `$LFS/sources`; 8.1
  explains how to download them again (wget with the new wget-list).

## Simplifications (the book says so in "!" notes)
- Tcl: the long list of seds over tclConfig.sh is shortened.
- Shadow: the `find man -name Makefile.in -exec sed ...` steps are simplified.
- Test suites of Bash, Coreutils, Vim, Util-linux and Acl are skipped (they need
  Expect scripts / tester tricks the shell cannot express well).
- Udev: the real book builds only udev targets and installs them with a long list of
  install commands; the game uses `ninja install`.
- 8.84 stripping: only some libraries/programs, no `--only-keep-debug` dance.
- GMP: the awk-based check of the test log is replaced by a grep.
- Intltool's sed is skipped; a possible Expect patch for GCC 14 is not included.
- Most new programs only answer `--version`; real behavior exists only for the tools
  listed under ch8tools.py and the registry tools (ls, tar, make, ...).

## Tested vs NOT tested
- Headless: 136 unittest cases pass on Python 3.12 and 3.13; pyflakes clean.
  Full walkthrough 2.4 → 8.85 by book commands; all non-optional objectives complete.
  Save file after chapter 8 is ~178 KB.
- GUI (Xvfb + python3.12/tk): loaded a save after glibc, built zlib by clicking the
  book blocks, ran tzselect interactively (menus answered in the terminal), book TOC
  shows chapter 8. Screenshots checked.
- NOT tested: Wayland/Hyprland; GUI run of most chapter-8 packages (headless only);
  the commands against the real LFS 12.2 book (site unreachable).

## Known issues / limitations
- Very long `configure` blocks wrap in the 440 px book panel with visible trailing
  backslashes (cosmetic).
- Old saves (before 0.7.0) need the new archives downloaded again.
- No `case`/`while`/functions yet.

## Next steps
- Chapter 9 (system configuration: bootscripts, network, /etc/inittab, locale, inputrc,
  shells), chapter 10 (fstab, kernel build, GRUB), chapter 11 (the end, reboot).
- Verify chapter 8 commands against the real LFS 12.2 book when the site is reachable.

## How to run & verify
    sudo pacman -S tk && python main.py
    python -m unittest -v
    python tools/gen_ch8_book.py      # after editing lfsgame/sim/ch8.py
Spoiler path: tests/helpers.py walkthroughs + `ch8_walkthrough()`.
