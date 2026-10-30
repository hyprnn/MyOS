# 0005 — LFS chapter 6: cross compiling the temporary tools
Version 0.5.0 · 2026-10-30

## Goal
User (in Russian): "continue, make chapter 6". Implement LFS 12.2 chapter 6 (6.1–6.18):
M4, Ncurses, Bash, Coreutils, Diffutils, File, Findutils, Gawk, Grep, Gzip, Make, Patch,
Sed, Tar, Xz, Binutils pass 2, GCC pass 2 — playable with the book's real commands.

## What changed
- `sim/build.py`
  - `CH6` (15 packages), SBU values, `SBU_PASS2` (binutils 0.4, gcc 4.1), `CH6_BINS`
    (programs each package installs; `name->target` = symlink).
  - Source trees: generic packages get `build-aux/config.guess` and a top-level
    `config.guess`; bash also `support/config.guess`; ncurses `configure` contains the word
    "mawk" (for `sed -i s/mawk// configure`), `misc/run_tic.sh`, `include/`; gawk
    `Makefile.in` with `extras` in SUBDIRS; binutils `ltmain.sh` whose line 6009 is the
    libtool line LFS edits (6006 empty filler lines, ~6 KB, deleted with the tree);
    gcc `libgcc/Makefile.in` and `libstdc++-v3/include/Makefile.in` with
    `thread_header = @thread_header@`.
  - `configure()`: `--build=` empty -> "missing argument to --build" (wrong config.guess
    path inside `$(...)`); binutils/gcc with `--host` -> pass 2 (`meta["pass"] = 2`);
    chapter-6 packages need `--host=$LFS_TGT` (otherwise a clearly marked "(simulation)"
    error), the cross gcc in PATH and glibc in $LFS (`_cross_ready`: "no acceptable C
    compiler" / "C compiler cannot create executables"); ncurses and file WITHOUT --host
    are allowed = native helper build (`meta["native"]`), ncurses native also creates the
    `include/` and `progs/` sub-makefiles (`meta["subdirs"]`).
  - `build_failure(m, meta, make_vars)`: file cross build without a native
    `FILE_COMPILE` -> the real "Cannot use the installed version of file (5.46) to
    cross-compile file 5.45 / Please install file 5.45 locally first"; GCC pass 2 without
    the thread_header sed -> "bits/gthr.h: No such file or directory".
  - `install_files(m, meta, destdir, make_vars)`: `_ch6_files` (programs as
    `prog: binary, cross: True`, libraries, curses.h with the XOPEN #if line, terminfo,
    coreutils man pages incl. chroot.1, libmagic + .la, liblzma + .la, findutils
    localstatedir, gawk extras if not removed; coreutils honours
    --enable-install-program / --enable-no-install-program); ncurses install raises
    BuildError without a native `TIC_PATH` ("libncursesw.so.6: cannot open shared object
    file"); pass-2 file lists for binutils (unprefixed tools + lib{bfd,ctf,ctf-nobfd,
    opcodes,sframe}.{so,a,la}) and gcc (gcc, g++, cpp, c++, $LFS_TGT-gcc, ..., libgcc_s,
    libstdc++).
- `commands/build.py`: make passes `VAR=value` to build/install; `_sub_make` for
  `make -C include` / `make -C progs tic` (tic needs include first); native `file` build
  creates `build/src/file`; binutils pass 2 prints a libtool warning + game hint when
  ltmain.sh was not fixed and only sets flag `binutils2_ltmain` when it was; install as
  root into the host is refused for every package (not only glibc); install output shows
  symlinks as `ln -sf`; new host command `file` (tells cross binaries "for GNU/Linux
  4.19.0" from host ones "3.2.0", scripts, archives, libraries); native helpers are
  runnable (`build/src/file` behaves like `file`).
- `commands/text.py` sed: numeric line addresses (`6009s/...`, `$`), BRE `$`/`^` are
  literal when not at the end/start (needed for `s/$add_dir//`).
- `commands/core.py`: `sh FILE` / `bash FILE` dispatch to the file's program
  (`$(sh support/config.guess)`).
- `commands/files.py`: `du [-sh]`.
- `sim/objectives.py`: 17 new objectives (40 total, 38 required), one per section
  6.2–6.18, with the book's extra steps checked (sh link, chroot move + man8 page,
  libncurses.so link + curses.h fix, removed .la/.a files, locate dir, cc link,
  ltmain fix).
- Book: `07-temptools.txt` (6.1–6.18, 7.0 "to be continued"); 6.0 removed from
  `06-toolchain.txt`; appendix lists du/file. `case` constructs of the real book are
  given as plain x86_64 commands.
- Tests: `tests/helpers.py` `CH6` walkthrough (exact book commands); `test_build.py`
  `Ch6Tests` (9) + `BeforeGlibcTests`; walkthrough test covers chapters 2–6.
  114 tests.

## Design decisions
- Same model as chapter 5: validate options, fail where real builds fail, install the
  files the objectives look at. Packages without interesting traps share one generic
  path; the traps that the real chapter 6 has were modelled: native tic (ncurses), native
  file (file), ltmain.sh (binutils 2), thread_header (gcc 2), wrong config.guess path.
- `--host` missing is a hard, labelled simulation error instead of a silent native
  build: a native build would only show its problem much later (in the chroot).
- Objectives check final file state, so the player may do steps in any order.

## Tested vs NOT tested
- 114 headless tests pass (Python 3.12/3.13), pyflakes clean; full walkthrough 2.4 → 6.18
  (all 40 objectives) with every chapter 4.4–6 command returning 0; save of the whole
  machine after chapter 6 is ~70 KB.
- GUI (Xvfb, python3.12 + tk): loaded a save after chapter 5, did all of 6.3 Ncurses by
  clicking the book's blocks and pressing Enter (including the multi-line pushd block);
  objective completed; screenshots checked. Older GUI scenarios were not re-run this time
  (no UI code changed except via commands).
- NOT tested: real Wayland/Hyprland; GUI run of the other chapter-6 packages (headless only).

## Known issues / limitations
- `case`, `while`, functions are still not supported (book uses plain commands).
- No `make check`; compile output lists are short and cycled.
- `cp` cannot preserve owners; chapter 7's `chown -R root:root $LFS/...` will need it.

## Next steps
- Chapter 7: change ownership to root, mount virtual kernel filesystems (dev, proc,
  sys, run — needs bind mounts in Machine), `chroot "$LFS" ...` (a second root for path
  resolution and a new shell session), then Gettext, Bison, Perl, Python, Texinfo,
  Util-linux inside the chroot.

## How to run & verify
    sudo pacman -S tk && python main.py
    python -m unittest -v
Spoiler path: tests/helpers.py FULL_WALKTHROUGH + CH4_4 + CH5 + CH6.
