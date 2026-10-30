# 0006 — LFS chapter 7: chroot and the additional temporary tools
Version 0.6.0 · 2026-10-30

## Goal
User (in Russian): "continue, make chapter 7". Implement LFS 12.2 (System V) chapter 7:
7.2 ownership back to root, 7.3 virtual kernel file systems, 7.4 entering the chroot,
7.5 directories, 7.6 essential files (passwd/group/hosts/mtab/logs, tester user),
7.7 Gettext, 7.8 Bison, 7.9 Perl, 7.10 Python, 7.11 Texinfo, 7.12 Util-linux,
7.13 cleanup and the optional backup.

## What changed
- `sim/machine.py`
  - chroot: fields `croot` (real path of the new root, "" = none) and `chroot_name` (the
    user name bash found when it started). `resolve()` walks to `croot` first; `..` and
    absolute symlinks cannot leave it; `Res.path` is the path as seen inside, the new
    `Res.real` is the path from the real root (mount table comparisons use `.real`).
    `real_path()`, `_resolve_real()`, `chroot()`, `refresh_name()` (sets flag
    `chroot_named` when a name was found), `_push_session()` (su and chroot share the
    session stack; it now saves croot/chroot_name too).
  - User database inside a chroot = the chroot's /etc/passwd and /etc/group
    (`db_users`, `db_groups`, `db_name_of_uid`, `display_owner` -> numbers for unknown
    owners, like `ls -l` shows uid 1000 for files of lfs).
  - Mounts: bind mounts (`{"bind": src}`, resolved through the source path) and virtual
    file systems proc/sysfs/devpts/tmpfs/devtmpfs (`VIRTUAL_FS`, own tree in
    `fs["TYPE@PATH"]`, entry key "fs"); `refresh_proc()` keeps /proc/self/mounts in
    sync; `umount` resolves the mount point, frees virtual fs, busy checks with the real
    cwd; `fs_of()` follows bind mounts; `fs_usage()` for virtual fs; host /dev now has
    `pts/` and `shm/` (`upgrade()` regenerates /dev for old saves).
- `shell/parser.py`: brace sequences `{1..8}`, `{a..c}`.
- `shell/runner.py`: inside a chroot only `CHROOT_BUILTINS` (bash builtins + game
  commands + clear/sleep) are always available; everything else must exist as a file in
  the chroot's PATH (so host-only tools such as wget, and util-linux tools before 7.12,
  are "command not found"). Registry commands that were installed as files (ls, tar,
  make, ...) run their normal implementation; compilers (`COMPILER_TOOLS`) dispatch to
  the installed program. `find_program` no longer raises for missing path components.
- New `commands/chroot.py`: `chroot` (root only; pushes a session; failure to start the
  command pops it again; sets flag `chroot_ok` for the book's env -i/PATH/PS1 setup),
  `install` (-d -m -o -g -v, and file copies), `chgrp`, `realpath`.
- `commands/disk.py`: `mount --bind`, `mount -t proc|sysfs|devpts|tmpfs`, "bound on"
  message, listing inside a chroot shows only its mounts; `mountpoint [-q]`; df skips
  bind mounts and handles virtual fs.
- `commands/files.py`: `chown --from=USER[:GROUP]`, users/groups from the chroot db,
  `-R` does not follow symlinks met during recursion; `ls -l` shows owners via
  `display_owner`.
- `commands/core.py`: `env -i ... /bin/bash --login` (login startup, checks the shell
  exists inside a chroot), `bash --login`/`exec /usr/bin/bash --login` refresh the user
  name, `whoami`/`id` inside a chroot use the chroot's passwd ("cannot find name for
  user ID 0").
- `commands/build.py`: in a chroot `tar` needs xz/gzip installed there; `tar -c[J]pf
  ARCHIVE PATHS` (refuses while mounts are below the archived tree; node gets
  "backup"); products left in the build tree (gettext's msgfmt/msgmerge/xgettext); the
  Python "optional modules were not found" warning; installed programs run inside the
  chroot (gcc compiles with sysroot "/", others answer --version); no host-install guard
  inside a chroot.
- `sim/build.py`: `CH7` packages (native builds, must be inside the chroot, need
  /usr/bin/gcc and /usr/lib/libc.so.6), `CH7_BINS`, `PRODUCTS`, perl's `Configure`
  (`sh Configure -des -D k=v ...`; -D values become opts, `prefix` -> `--prefix`,
  `privlib`/`archlib` decide where modules go), util-linux default prefix /usr.
- `sim/game.py`: objectives are evaluated with `croot` cleared (they look at the real
  filesystem).
- `sim/objectives.py`: 13 new objectives (53 total, 3 optional): rootowner, vkfs,
  chroot, dirs7, files7, gettext7, bison7, perl7, python7, texinfo7, utillinux7,
  cleanup7, backup7 (optional).
- `commands/game.py`: `tasks` collapses chapters that are not started yet (except the
  current one).
- Book: `08-chroot.txt` (7.1–7.13, 8.0 "to be continued"); 7.0 removed from
  `07-temptools.txt`; appendix updated.
- Tests: `tests/helpers.py` `CH7_HOST`, `CH7` walkthrough (book commands, including the
  backup); `test_build.py` `Ch7Tests` (9). Walkthrough test covers chapters 2–7.

## Design decisions
- One shell session stack for su and chroot; `exit` leaves the innermost one.
- Paths inside a chroot are "virtual" everywhere commands see them (prompt, pwd, cwd,
  build metadata); only the mount table uses real paths.
- "I have no name!" is reproduced faithfully: the prompt name is cached when the shell
  starts, so writing /etc/passwd is not enough — `exec /usr/bin/bash --login` is
  (book 7.6). `whoami` reads the file live, like the real one.
- The 7.2 chown is important in the real book because lfs's uid has no name inside;
  the game shows that via numeric owners in `ls -l`.

## Tested vs NOT tested
- Headless: 123 unittest cases pass on Python 3.12 and 3.13,
  pyflakes clean; full walkthrough 2.4 → 7.13 including the backup, all 53 objectives.
- GUI (Xvfb): loaded a save after chapter 6, did 7.2, 7.3, 7.4 and 7.6 by clicking the
  book's blocks: mounts listed, chroot entered ("I have no name!"), `wget: command not
  found` inside, passwd/group heredocs, `exec /usr/bin/bash --login` -> "(lfs chroot)
  root:/#". Screenshots checked.
- NOT tested: Wayland/Hyprland; GUI run of 7.7–7.13 (headless only).

## Known issues / limitations
- No `case`; the book gives the x86_64 command directly.
- Programs of the new system mostly only answer `--version` (except gcc/cc, and the
  registry tools such as ls/tar/make which behave like the host ones).
- Restoring the backup (`tar -xpf` of it) is not simulated.

## Next steps
- Chapter 8 (final system, ~80 packages): needs a lighter, data-driven package
  description (most packages: configure/make/make check/make install), test suites
  (`make check` results), and Glibc/GCC/Binutils specifics.

## How to run & verify
    sudo pacman -S tk && python main.py
    python -m unittest -v
Spoiler path: tests/helpers.py FULL_WALKTHROUGH + CH4_4 + CH5 + CH6 + CH7.
