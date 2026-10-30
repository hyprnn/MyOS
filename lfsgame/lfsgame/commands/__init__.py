"""All simulated commands. Importing this package registers them in REGISTRY.

Modules: core (shell builtins), files, disk (+ fdisk session), net (wget/md5sum),
users, game (tasks/hint/book/save/menu).
"""
from ..shell.runner import REGISTRY, Input, Sleep, command, run_line, start_line
from . import core, disk, files, game, net, users

__all__ = ["REGISTRY", "Input", "Sleep", "command", "run_line", "start_line",
           "core", "disk", "files", "game", "net", "users"]
