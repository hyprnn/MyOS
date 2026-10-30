# LFS Game

Simulation game about building Linux From Scratch (NOT a real build — nothing touches your
system). Terminal-style GUI in a game window: main menu, a bash-like shell on a simulated
host computer, and an in-game LFS book in a slide-out panel.

Current content (v0.3.0): LFS chapters 2–4 — find the empty disk, partition it with an
interactive `fdisk`, `mkfs`, `mkswap`/`swapon`, `export LFS`, `mount`, create
`$LFS/sources`, download the packages with `wget`, verify with `md5sum -c`, create the
directory layout, add the `lfs` user, `chown`, `su - lfs`. 13 objectives (`tasks`, `hint`).

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
    lfsgame/sim/         simulated machine: disks, filesystems, users, packages, objectives
    lfsgame/shell/       parser/expansions, command runner, getopt, Tab completion
    lfsgame/commands/    all commands (core, files, disk, fdisk, net, users, game)
    lfsgame/book/        book parser + content/*.txt (original paraphrase, not the real LFS text)
    tests/               unittest
    reports/             one report per code change, written for another AI

## Rule
Every code change => new file in `reports/` (see reports/README.md) and an update
of `CONTEXT.txt` (handoff context for another AI).
