# 0004 — LFS 4.4 (environment) and chapter 5 (cross-toolchain)
Version 0.4.0 · 2026-10-30

## Goal
User (in Russian): "continue, make chapter 5 and 4.4". Implement LFS 4.4 (lfs user's
.bash_profile/.bashrc, MAKEFLAGS) and chapter 5 (Binutils pass 1, GCC pass 1 + limits.h,
Linux API headers, Glibc + sanity check, Libstdc++) as a playable simulation that follows
the real book's commands as closely as possible.

## What changed
Shell (big upgrade, needed by the real 4.4/5 commands):
- `shell/parser.py` rewritten: lexer class + recursive-descent parser. New: here-docs
  (`<<`, `<<-`, quoted/unquoted delimiter; bodies are read from the following lines of
  the same text), `$(...)` and backticks (quote kind "S" bare / "SQ" inside "..."),
  pipelines `a | b` (+ `!`), `<` redirection, newline as separator, `if/elif/else/fi`,
  `for NAME in WORDS; do ...; done`, backslash-newline continuation, word splitting of
  unquoted expansions (quote kind "u"), `expand_heredoc()`. `Incomplete` (subclass of
  ParseError) for unfinished input. `case/while/until/function` -> clear ParseError.
  `parse()` now returns a CmdList: [(connector, node)], node = Simple|Pipeline|If|For.
- `shell/runner.py` rewritten: interpreter over that AST (`run_list`, `run_node`, `_exec`,
  `run_argv`), `Sub` ctx wrapper (output capture + stdin + in_script), `Expander` (with
  synchronous command substitution via `capture_output`), PATH lookup of executables in
  the simulated fs (`find_program`) + `PROGRAMS` registry (`@program(name)`, dispatched by
  a file node's "prog" key), shell scripts (`#!`) run in a subshell, `run_script`,
  `source_file`, `ScriptExit`. Interactive `_run` asks `Input("> ")` on `Incomplete` and
  re-parses the joined text. `VAR=value cmd` now works (temporary env). `/bin/bash` etc.
  map to builtins. `>` truncates before running (like bash).
New commands:
- core: `source`/`.` (current shell), `bash FILE` (subshell; `bash -c`, `bash`/`bash -l`
  read startup files), `exec`, `set` (accepts `set +h`), `test`/`[`, `nproc` (8), `time`
  (uses the simulated clock), `dirname`, `basename`, `printenv` (split from env),
  `env -i A=B ... /bin/bash` (new clean shell: reads /etc/bash.bashrc then ~/.bashrc),
  `exit N` inside scripts. `which` knows PATH programs.
- `commands/text.py`: `grep` (-i -v -c -n -E -q -F -l), `wc`, `head`, `tail`, `sed`
  (`-e`, `-i[SUFFIX]`, `-n`, `-E`; commands `s///flags` with any delimiter, `d`, `p`,
  `/regex/` addresses; BRE converted to Python re), `find` (-name -iname -type
  -maxdepth ! -delete -print).
- `commands/build.py`: `tar` (-x -t -v -f -C; checks checksum: broken download ->
  "Unexpected EOF in archive"), `patch` (-N -p -i -R, stdin; detects already-applied
  patches, wrong -p/directory -> "can't find file to patch"), `make` (-jN, -C, VAR=,
  targets all/install/clean/check; kbuild targets mrproper/headers), `readelf -l`, host
  `gcc` (version only), and programs: `configure`, `config.guess`, `binutils` tools,
  `gcc` (cross compiler: --version, -dumpmachine, -print-libgcc-file-name, compiling
  from files or stdin with -xc -, include checks against the sysroot, link checks for
  crt1.o/crti.o/libc), `elf` (a.out), `binary`.
- `cat` reads stdin (no args or `-`), `cp -r` merges into existing directories and sets
  the copier as owner, `ln -sf` into a directory, `ln -v` prints the real link path.
Simulation:
- `sim/build.py` (new): source-tree skeletons per tarball (`source_tree`), configure
  validation per component (binutils, gcc, glibc, libstdc++), build-time failures
  (`build_failure`), installed file lists (`install_files`), kernel headers tree, SBU
  model (SBU_SECONDS=133 simulated, SBU_REAL_MS=7000 real at -j1; speedup = min(j,8)^0.75).
- `sim/machine.py`: PS1 rendering in `prompt()` (\u \h \w \W \$), `CORES=8`, `clock`
  field (simulated seconds), host `/etc/bash.bashrc` with `export CC="distcc gcc"` (the
  trap of 4.4), writes to /dev/null are discarded, `upgrade()` adds bash.bashrc to old
  saves, int->float tolerance for `clock` in from_dict.
- `sim/packages.py`: glibc-2.40-fhs-1.patch added (text file with a marker line,
  `PATCH_TEXT`); `put_blob(..., data=)`.
- `sim/objectives.py`: 10 new objectives (23 total, 2 optional): bashrc, lfsenv,
  makeflags (optional), binutils1, gcc1, limits, headers, glibc, sanity, libstdcxx.
- `commands/users.py`: `su` runs the login startup (~/.bash_profile, ~/.bash_login,
  ~/.profile).
UI:
- terminal: multi-line input (pasted blocks and history entries run as one text),
  continuation prompts "> ", typeahead buffer (keys typed while a command is busy are
  replayed, Enter included), Ctrl+V keeps newlines.
- sidepanel: command blocks can be multi-line; each block has its own tag `cmd#N`; hover
  highlights the whole block; click pastes the whole block.
- book: markup `+ ` continuation lines; sections 4.4, 5.1–5.6, 6.0 (old 5.0 removed);
  appendix updated.
- `tasks` collapses finished chapters (`tasks -a` lists everything).
Tests: `test_build.py` (24 new), `test_parser.py` (new API + here-doc/if/for/pipe/
substitution/incomplete), `test_book.py` (multi-line blocks; programs like ../configure
and $LFS_TGT-gcc are skipped by the registry check), `test_sim.py` walkthrough now covers
chapters 2–5.6, helpers `CH4_4`, `CH5`. 104 tests total.

## Design decisions & rejected alternatives
- Builds are simulated by *state + validation*, not by text matching of commands: the
  configure "program" reads its options, so the player may type them in any order, on one
  or many lines, with variables — like real life. Wrong options either fail where the real
  build fails (configure or make) or install to the wrong place (objective not met).
- Failures intentionally mirror real LFS problems: `CC=distcc gcc` from the host's
  /etc/bash.bashrc -> "C compiler cannot create executables"; missing gmp/mpfr/mpc -> the
  real GCC message; pass 1 without --with-newlib/--without-headers -> "stdio.h: No such
  file"; without --disable-libstdcxx and libstdc++ before glibc -> "Link tests are not
  allowed after GCC_NO_EXECUTABLES"; glibc in-tree -> "you must configure in a separate
  build directory"; missing kernel headers -> the real glibc message; no DESTDIR as lfs ->
  Permission denied; as root -> the host's libc would be overwritten (refused, explained);
  sanity check before glibc -> "cannot find crt1.o". Messages that are game rules (not
  real tool output) are printed separately as dim "(game hint: ...)" lines.
- The real book uses `case $(uname -m) in ...` for the t-linux64 sed and the lib64 links;
  `case` is not implemented, the book text explains the machine is x86_64 and gives the
  plain commands.
- Source trees are tiny skeletons (only the files commands touch) to keep saves small.
- Real time vs game time: make sleeps SBU*7 s / speedup real time (gcc pass 1 at -j1 ≈ 26 s,
  at -j8 ≈ 5.6 s) — so MAKEFLAGS matters to the player; `time` shows simulated time.
- Command substitution runs synchronously (no sleeps, Input gets EOF) — fine for
  `$(uname -m)`, `$(nproc)`, `$(../scripts/config.guess)`, `$($LFS_TGT-gcc -print-...)`.
- Save format unchanged (SAVE_VERSION stays 2): new Machine field `clock` is optional and
  `upgrade()` patches old machines. Old book_section "5.0" no longer exists (panel simply
  keeps its current page).

## Behavior details / nuances / gotchas
- The objective `lfsenv` requires user lfs with LFS, LFS_TGT, PATH starting with
  /mnt/lfs/tools/bin, LC_ALL=POSIX, umask 022 and NO CC/CXX. If bash.bashrc was not moved,
  sourcing the profile brings CC back (test_bashrc_trap_pollutes_env).
- `env -i ... /bin/bash` replaces `m.env` and then sources /etc/bash.bashrc and ~/.bashrc
  in the current shell (there is only one shell level in the simulation; `exit` from it
  logs out of the su session).
- `source`/`bash FILE` run text with `interactive=False`: incomplete constructs are a
  syntax error instead of a "> " prompt. `exit` in a script raises ScriptExit.
- Install ownership = current user. Root builds work but create root-owned files.
- `make install` builds first if needed. `make` after a successful build: "Nothing to be
  done". Ctrl+C during make leaves the dir "configured".
- glibc's "patched" state is read from the source tree top node at install time (key
  "patches"), so patching after configure still counts; flag `glibc_fhs` is set on install.
- Sanity flag `sanity_ok` is set by readelf when it shows a cross-compiled ELF whose
  interpreter is /lib64/ld-linux-x86-64.so.2.
- Typeahead: printable keys and Enter typed during a Sleep are queued and replayed after
  the next prompt; Ctrl+C/kill clears the queue.
- Book: in `$` blocks, `+ ` lines are joined with "\n"; the terminal receives the whole
  block and parses it as one text (here-docs inside work without prompts).

## Tested vs NOT tested
- 104 headless unittest cases pass on Python 3.12 and 3.13; pyflakes clean. They include
  the full walkthrough 2.4 → 5.6 (every chapter 4.4/5 line must return status 0) and the
  failure modes listed above.
- GUI run under Xvfb (python3.12 + tk 8.6), screenshots inspected: loading a prepared save
  (after 4.3), `source ~/.bash_profile` -> prompt `lfs:~$ `, interactive here-doc with
  "> " prompts, typeahead during tar, clicking the multi-line Binutils configure block in
  book 5.2 (pasted whole), Enter, `time make` with progress, `make install`, objective
  completion, collapsed `tasks`. Older scenarios (menu, fdisk, wget + Ctrl+C, save/load,
  book clicks, quit confirm) re-run and pass.
- NOT tested: real Wayland/Hyprland, HiDPI, mouse wheel, real clipboard; GCC/glibc/
  libstdc++ builds were exercised in the GUI only up to Binutils (the rest headless).

## Known issues / limitations
- No `case`, `while`, functions, subshells, `2>`; `$((...))` arithmetic not supported.
- Only chapter-5 components can be configured; other packages say "built in a later
  chapter".
- `sed` supports s/d/p only; `find` a small subset; `grep` has no -r.
- Compile output is a fixed list of file names per package (cycled).
- The panel width (440 px) makes long compile lines wrap; closing the panel (F2) helps.

## Next steps / open questions
- Chapter 6 (cross compiling temporary tools: M4, Ncurses, Bash, Coreutils, ... ,
  Binutils/GCC pass 2) reusing sim/build.py with more components.
- Chapter 7 (chroot) would need a second root / mount namespace in Machine.
- Possibly `case` (needed for exact book commands), a Russian book translation.

## How to run & verify
    sudo pacman -S tk && python main.py
    python -m unittest -v
Spoiler path: tests/helpers.py FULL_WALKTHROUGH + CH4_4 + CH5.
