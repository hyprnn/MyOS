# LFS Game

Simulation game about building Linux From Scratch (NOT a real build — nothing touches your
system). Terminal-style GUI in a game window: main menu, a bash-like shell on a simulated
host computer, and an in-game LFS book in a slide-out panel.

Current content (v0.6.0): LFS chapters 2–7 — partition the empty disk with an interactive
`fdisk`, `mkfs`, `mount`, download and verify the sources, prepare the `lfs` user and its
clean environment (`.bash_profile`/`.bashrc` via here-docs), then build the
cross-toolchain: Binutils pass 1, GCC pass 1, Linux API headers, Glibc (+ sanity check),
Libstdc++ — with `tar`, `patch`, `../configure`, `make`, `make DESTDIR=$LFS install`;
then chapter 6: cross compile M4, Ncurses, Bash, Coreutils, Diffutils, File, Findutils,
Gawk, Grep, Gzip, Make, Patch, Sed, Tar, Xz and Binutils/GCC pass 2; chapter 7: mount the
virtual kernel file systems, `chroot` into the new system ("I have no name!" included),
create the directory tree and essential files, build Gettext, Bison, Perl, Python, Texinfo
and Util-linux inside, clean up and back up.
Builds are simulated but fail for the same reasons real ones do. 53 objectives
(`tasks`, `hint`).

## Run
    sudo pacman -S tk        # tkinter on Arch
    python main.py

## Test (headless, no tkinter)
    python -m unittest -v

## Keys
F2 book panel · F3/F4 previous/next book section · Tab completion · Up/Down history ·
Ctrl+C interrupt · Ctrl+S save · Ctrl+L clear · Esc back to menu.
Click a `$ command` line in the book to paste it into the terminal.
Save file: ~/.local/share/lfsgame/save.json (override with LFSGAME_SAVE_DIR).

## Layout
    main.py              entry point
    lfsgame/app.py       window, screens, panel wiring
    lfsgame/menu.py      main menu
    lfsgame/terminal.py  terminal screen (drives command processes)
    lfsgame/sidepanel.py slide-out LFS book reader
    lfsgame/save.py      save/load (JSON v2)
    lfsgame/sim/         simulated machine: disks, filesystems, users, packages, builds, objectives
    lfsgame/shell/       parser/expansions, command runner, getopt, Tab completion
    lfsgame/commands/    all commands (core, files, text, disk, fdisk, net, users, build, chroot, game)
    lfsgame/book/        book parser + content/*.txt (original paraphrase, not the real LFS text)
    tests/               unittest
    reports/             one report per code change, written for another AI

## Rule
Every code change => new file in `reports/` (see reports/README.md) and an update
of `CONTEXT.txt` (handoff context for another AI).
