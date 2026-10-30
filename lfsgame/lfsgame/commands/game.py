"""Game commands: tasks, hint, book, save, menu, version."""
from __future__ import annotations

from .. import __version__
from ..shell.runner import command
from ..sim.objectives import OBJECTIVES

G = "game"


CHAPTERS = {"2": "Chapter 2: partition, filesystem, mount", "3": "Chapter 3: packages",
            "4": "Chapter 4: final preparations", "5": "Chapter 5: cross-toolchain",
            "6": "Chapter 6: cross compiling temporary tools",
            "7": "Chapter 7: chroot and additional temporary tools"}


@command("tasks", "show your LFS objectives and progress (-a: list finished ones too)", G,
         aliases=("status", "quest"))
def _tasks(ctx, args):
    game = ctx.game
    cur = game.current()
    done, total = game.score()
    ctx.write(f"LFS progress: {done}/{total}\n", "accent")
    groups = {}
    for o in OBJECTIVES:
        groups.setdefault(o.section.split(".")[0], []).append(o)
    cur_chapter = cur.section.split(".")[0] if cur else None
    for chapter, objs in groups.items():
        title = CHAPTERS.get(chapter, "Chapter " + chapter)
        if all(o.id in game.done or o.optional for o in objs) and "-a" not in args:
            ctx.write("  [x] ", "ok")
            ctx.write(f"{title}: all done  (tasks -a shows them)\n", "dim")
            continue
        if chapter != cur_chapter and not any(o.id in game.done for o in objs) \
                and "-a" not in args:
            ctx.write(f"  [ ] {title}: {len(objs)} objectives\n", "dim")
            continue
        for o in objs:
            if o.id in game.done:
                ctx.write("  [x] ", "ok")
                ctx.write(f"{o.title}\n", "dim")
            else:
                mark = "  >> " if cur is not None and o.id == cur.id else "  [ ] "
                ctx.write(mark, "warn" if mark.strip() == ">>" else None)
                ctx.write(o.title)
                ctx.write(f"   (book {o.section}{', optional' if o.optional else ''})\n", "dim")
    if cur is None:
        ctx.write("\nAll objectives done! Chapter 8 (the final system) is not in the game yet.\n",
                  "ok")
    else:
        ctx.write("\nStuck? Type 'hint' or open the book with F2 / 'book'.\n", "dim")


@command("hint", "a hint for the current objective", G)
def _hint(ctx, args):
    cur = ctx.game.current()
    if cur is None:
        ctx.write("Nothing left to do in this version. Well done!\n", "ok")
        return
    ctx.write(f"{cur.title}\n", "accent")
    ctx.write(f"  {cur.hint}\n")
    ctx.write(f"  Read more: book {cur.section}\n", "dim")


@command("book", "open the LFS book: book [SECTION|toc]", G)
def _book(ctx, args):
    target = args[0] if args else None
    if target is None:
        cur = ctx.game.current()
        target = cur.section if cur else None
    if not ctx.open_book(target):
        ctx.write(f"book: no section '{target}' (try 'book toc')\n", "err")
        return 1


@command("save", "save the game (Ctrl+S also works)", G)
def _save(ctx, args):
    ctx.save_game()


@command("menu", "return to main menu", G)
def _menu(ctx, args):
    ctx.goto_menu()


@command("version", "show game version", G)
def _version(ctx, args):
    ctx.write(f"LFS Game {__version__}\n")
