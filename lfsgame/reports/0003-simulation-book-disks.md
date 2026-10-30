# 0003 — Simulated machine, partitioning, LFS book, 50+ commands
Version 0.3.0 · 2026-10-30

## Goal
User (in Russian): "continue the game, add the book, more commands, make disk
partitioning, etc." Interpretation: turn the placeholder shell into actual gameplay that
follows LFS chapters 2–4 (find disk → fdisk → mkfs → $LFS → mount → sources → wget →
md5sum → directory layout → lfs user → su - lfs), put a readable LFS book into the side
panel, and grow the command set. Still a simulation: nothing touches the real system.

## What changed
New packages (all pure Python, no tkinter, headless-testable):
- `lfsgame/sim/` — the simulated computer.
  - `vfs.py` — filesystem nodes as plain dicts (`t` d/f/l/b, `m` mode, `u`/`g` owner,
    `mt` mtime, `c` children, `data`, `size`, optional `md5`/`partial`), `FsError`, helpers
    (`mode_string`, `human` sizes, `normpath`).
  - `disks.py` — disk dicts with GPT/DOS tables (512 B sectors, 1 MiB alignment),
    partition type table (`linux`, `swap`, `uefi`, `bios` with GPT numbers 20/19/1/4 and DOS
    hex 83/82/ef), free-region math, `+20G` style size parsing.
  - `machine.py` — `Machine`: disks (sda 64G host GPT: sda1 EFI vfat on /boot/efi, sda2
    ext4 on /; sdb 32G EMPTY = LFS target; sdc 8G USB stick with a FAT partition = red
    herring), filesystems keyed by device path, mount table (path resolution follows
    mounts + symlinks), swaps, users/groups (regenerates /etc/passwd and /etc/group), env,
    cwd, umask, su stack, pushd stack, flags, the "glitch" package. `to_dict`/`from_dict`
    (from_dict validates types and the root mount; raises ValueError/TypeError).
  - `packages.py` — 37 packages (LFS 12.2-era versions, approximate sizes, plausible
    URLs), `wget-list-sysv` / `md5sums` text, FAKE md5 = md5("lfsgame:"+filename).
  - `objectives.py` — 13 objectives (1 optional: swap), each a predicate over Machine and
    linked to a book section id. Notable checks: root partition must be on **sdb**, type
    linux, ≥10 GiB, ext2/3/4, mounted exactly at /mnt/lfs; sources dir mode 1777 on the
    LFS fs; every package present, complete, with matching checksum; layout =
    etc,var,usr/{bin,lib,sbin},tools,lib64 dirs + bin/lib/sbin symlinks to usr/*; owner
    lfs on those dirs; `su - lfs` sets flag `su_lfs`.
  - `game.py` — `Game` = Machine + `done` list (STICKY: once done, never undone) +
    `book_section`. `check()` returns newly completed objectives.
- `lfsgame/shell/` — the bash-like shell.
  - `parser.py` — own tokenizer (char-level, remembers quoting), `;` `&&` `||` `>` `>>`,
    `$VAR ${VAR} $? $$`, `~`/`~user`, nested brace expansion (`usr{,/*}`), globs. Pipes, `<`,
    `&`, `$(...)` → ParseError with a clear "not supported in this simulation" message.
  - `runner.py` — `REGISTRY`, `@command(name, help, group, hidden, aliases)`, process
    protocol (see below), `start_line` (generator), `run_line` (synchronous driver for
    tests), redirection via a capturing ctx wrapper, `UsageError` (status 2).
  - `opts.py` — tiny getopt (`Opts(args, short, short_val, long, long_val)`).
  - `complete.py` — Tab completion of command names and paths (expands `$VAR`/`~` in the
    directory part, includes mount points).
- `lfsgame/commands/` (package; replaces old `lfsgame/commands.py`, which was deleted):
  `core.py` (help, clear, echo, pwd, cd, pushd/popd, export, unset, env/printenv, umask,
  whoami, id, uname, hostname, date, history, sleep, which/type, man, bash/sh/source/.,
  hidden ver_check/true/false, exit/logout, reboot*), `files.py` (ls/ll, cat, mkdir,
  touch, rm, rmdir, ln -s, chmod incl. symbolic `a+wt`, chown, cp, mv), `disk.py`
  (lsblk [-f], blkid, fdisk [-l], mkfs, mkfs.ext2/3/4, mke2fs, mkfs.fat/vfat, mkswap,
  swapon/swapoff, mount, umount, df, free, sync), `fdisk.py` (interactive session),
  `net.py` (wget, md5sum), `users.py` (groupadd, useradd, passwd, su), `game.py` (tasks/
  status/quest, hint, book, save, menu, version).
- `lfsgame/book/` — `__init__.py` (markup parser, `Book.load()`, toc, neighbors) and
  `content/*.txt` (sections 0, 1.1, 2.2–2.7, 3.1, 3.2, 4.2, 4.3, 5.0, A). Text is an
  ORIGINAL paraphrase written for the game (the real LFS book has its own license; do not
  paste real book text in).

Modified:
- `save.py` — SAVE_VERSION 2: scrollback is now a list of `[text, tag]` runs (colors
  survive load), plus `game` (Game.to_dict()). v1 saves migrate (plain text → one run,
  game None → caller creates a fresh machine and prints "old save: the machine was
  reset"). `trim_runs()` trims by lines while keeping tags. JSON written compact.
- `terminal.py` — rewritten: drives processes, dynamic prompt from Machine, Tab
  completion, secret input for passwords, Ctrl+C/Ctrl+D/Ctrl+U/Ctrl+A/PgUp/PgDn,
  objective notifications ("★ Objective complete"), `paste_input()` for book clicks,
  tagged snapshot via `Text.dump`, `dirty` flag. No game until `start_new()`/`load_state()`.
- `sidepanel.py` — real book reader: nav (prev/contents/next), TOC with ✓/▶/· per
  section from objectives, clickable `$` command lines (hover highlight), objectives
  list at the end of each section. Width 440. Calls `on_layout(open)` and gives focus
  back to the screen after clicks.
- `app.py` — window 1100x680; F2 toggle, F3/F4 prev/next section; screen gets right
  padding = handle (+ panel width when open) so the terminal is never hidden; menu
  confirms for New game / Load / Quit when there is unsaved progress; window close (X)
  asks too; new game opens book section 0, load restores the saved book section.
- `menu.py` — "Resume" item (when a session is in memory), "Load saved game (date)",
  yes/no confirm line (Enter = yes, Esc = no).
- `theme.py` — LINK, DIR, CMD_BG, HANDLE_BG, `font_sized()`.
- `__init__.py` — 0.3.0.
- tests: `helpers.py` (FakeCtx + FULL_WALKTHROUGH), `test_commands.py` (rewritten),
  `test_parser.py`, `test_sim.py`, `test_book.py`, `test_save.py` (v2, migration).

## Design decisions & rejected alternatives
- **Process protocol instead of callbacks**: a command may be a generator that yields
  `Sleep(ms)` or `Input(prompt, secret)` and returns its exit status. A whole command line
  is one generator (`yield from` per simple command), so `fdisk /dev/sdb && lsblk` and
  `bash script` with interactive lines just work. The UI drives it with `after()`, tests
  with `run_line(ctx, line, inputs=[...])` (sleeps skipped, inputs fed in order,
  None = EOF when they run out). Rejected: threads (tk is single-threaded), separate
  "interactive mode" objects per program.
- Machine state = plain dicts → JSON save is trivial; no class hierarchy for nodes.
- Each filesystem has its own tree; mounting splices it into path resolution, so a
  directory created in /mnt/lfs before mounting really lives on the host disk and is
  hidden after mounting (like real Linux). Tests check this.
- fdisk keeps a deep copy and only `w` applies it. Writing a table to a disk with mounted
  or swapped partitions is REFUSED in the simulation (real Linux writes it and the kernel
  keeps the old table); this protects the host disk sda and is explained in the output.
- Rewriting a partition (deleted, moved or label changed) drops its filesystem.
- fdisk on a disk without a label auto-creates a DOS label in memory, like real fdisk.
  DOS extended partitions are not simulated (message suggests GPT).
- wget: only LFS URLs and the package mirror hosts resolve; others fail with "unable to
  resolve host" (+ a dim explanation). Without `-c` an existing file gets `.1`, `.2`
  suffixes (real behavior). With `-c` a complete file → "nothing to do"; a partial one →
  "206 Partial Content" and completion.
- **Network glitch** (gameplay): `Machine.glitch_file` (random package >5 MiB, chosen at
  new game) breaks off at 43% the first time it is downloaded ("Connection reset by peer.
  Giving up."), leaving a partial file with a zero md5. md5sum -c then reports FAILED; the
  fix is `wget --continue` (exactly what the book recommends). Flag `glitch_done` makes it
  happen only once per game.
- md5sum -c sets flag `md5_ok` only if every package name verified OK in one run.
- `su` as root never asks a password; as non-root it asks (works if `passwd` was set).
  `su` without `-` behaves like `su -` (noted in output). `exit` in an su session logs
  out; at the login shell it returns to the menu.
- Objectives are sticky so that e.g. `unset LFS` later does not "uncomplete" anything.
- The book's for-loop and `case $(uname -m)` from real LFS are replaced by plain
  commands because loops/command substitution are not implemented; the book says so.
- Tab completion only completes at the end of the line.

## Behavior details / nuances / gotchas
- Terminal key handler must return None for F-keys (F2/F3/F4 are `bind_all`) — kept.
- While a process sleeps, all keys except Ctrl+C, Esc and F-keys are swallowed. Esc
  during a sleeping process switches to the menu while the process keeps running in the
  background (after() callbacks); New game/Load kill it (`_kill_proc`).
- Secret input (passwords) is never inserted into the Text widget; it's kept in
  `self._secret` and handled in `_on_key` (BackSpace works).
- Selection safety: typing/BackSpace while a selection reaches into the scrollback
  first removes the selection, so the scrollback is never edited. Ctrl+C with a
  selection in the scrollback (and no process) copies instead of interrupting.
- `snapshot()` dumps text+tags from 1.0 to `input_start linestart` (the current prompt
  line is not saved). Only tags listed in `terminal.TAGS` are kept.
- `dirty` is set when a line is submitted and cleared by a successful save inside it.
- Book clicks: Text tag bindings use the "current" mark, which updates on Motion; the
  panel's Text has takefocus=0 and returns focus to the screen on ButtonRelease (found
  in GUI testing: before the fix, clicking the book stole keyboard focus).
- `ls` shows mount points even if the parent dir entry is missing; `/dev` is regenerated
  from the disk table (`sync_dev`) after partition changes.
- df: ext overhead 1.6% of the partition; host root has a fixed 11.2 GiB "base_used".
- Book IDs are strings ("2.4"); objective `section` must exist (test enforces it). A test
  also parses every `$` line in the book and checks the command exists in REGISTRY.

## Tested vs NOT tested
- Headless: 73 unittest cases pass on Python 3.12 and 3.13 (parser, completion, fdisk
  flows incl. EOF/quit/delete/DOS/bad values/host-disk refusal, mkfs prompts, mount
  semantics + busy umount, swap, wget glitch → md5 fail → --continue → ok, users/su/
  passwd, the FULL walkthrough completing all 13 objectives, save v2 + v1 migration +
  validation, book parsing/consistency). pyflakes clean.
- GUI: THIS TIME the GUI was actually run (python3.12 + tk 8.6 under Xvfb, events sent with
  `event_generate`, screenshots inspected): menu, New game, typing, Tab completion, F2
  panel, `open_book`, interactive fdisk session, mkfs with sleeps, wget with Ctrl+C
  interruption and re-run, passwd secret input, Ctrl+S save, TOC with progress marks,
  Esc to menu with "Resume", Load saved game (colors restored), clicking a `$` line pastes
  into the terminal and focus returns to it, F3, Esc→quit confirm→Enter quits.
- NOT tested: real Wayland/Hyprland (XWayland) rendering and fonts on the user's Arch,
  mouse-wheel scrolling of the book, HiDPI, very small windows, typing speed/IME, real
  clipboard paste (<<Paste>> handler written but not exercised).

## Known issues / limitations
- No pipes, loops, `$(...)`, `<`, here-docs; no `.bash_profile` handling (book 4.4 not
  implemented); `su` env does not read any rc files.
- Permissions are simplified (root bypasses all; group/other bits partly modeled; no
  exec checks except `cd`).
- `cp`/`mv` are minimal (no merging into existing dirs, no -i).
- wget progress is not animated line by line (one sleep per file).
- Window/panel sizes are fixed pixels; panel width 440 assumes a ≥1000 px wide window.
- The save file grows with the machine (~40 KB after chapter 3).

## Next steps / open questions
- Section 4.4 (lfs `.bash_profile`/`.bashrc` via `cat > file << "EOF"`) needs here-docs.
- Chapter 5: simulated package builds (tar xf, cd, ../configure, make, make install)
  with time, errors from wrong order/options, and a SBU clock.
- Maybe: loops in the shell, `tar`, `grep`, `wc`, pipes for `| tee`.
- Ask the user whether the book should be in Russian (UI/book are English now).

## How to run & verify
    sudo pacman -S tk && python main.py
    python -m unittest -v
Quick manual path: New game → `tasks` → F2 → follow section 2.4 → ... → `su - lfs`.
Spoiler path: tests/helpers.py `FULL_WALKTHROUGH`.
Save: ~/.local/share/lfsgame/save.json (v2). `LFSGAME_SAVE_DIR=/tmp/x python main.py`
uses a throwaway save dir.
