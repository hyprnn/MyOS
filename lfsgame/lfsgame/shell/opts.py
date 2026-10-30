"""Minimal getopt for simulated commands."""
from __future__ import annotations

from typing import Dict, List, Set

from .runner import UsageError


class Opts:
    """Parse ``args``.

    short:     flag letters without value, e.g. "pvR"
    short_val: letters taking a value, e.g. "tP"
    long:      {"parents": "p"} long flag -> short letter (or any key)
    long_val:  {"type": "t"} long option with value (--type=x or --type x)
    Result: .flags (set of letters/keys), .vals (dict), .args (positionals).
    A lone "-" is positional (for 'su -'); "--" ends options.
    """

    def __init__(self, args: List[str], short: str = "", short_val: str = "",
                 long: Dict[str, str] = None, long_val: Dict[str, str] = None):
        long, long_val = long or {}, long_val or {}
        self.flags: Set[str] = set()
        self.vals: Dict[str, str] = {}
        self.args: List[str] = []
        i = 0
        while i < len(args):
            a = args[i]
            if a == "--":
                self.args += args[i + 1:]
                break
            if a.startswith("--"):
                name, eq, val = a[2:].partition("=")
                if name in long and not eq:
                    self.flags.add(long[name])
                elif name in long_val:
                    if not eq:
                        i += 1
                        if i >= len(args):
                            raise UsageError(f"option '--{name}' requires an argument")
                        val = args[i]
                    self.vals[long_val[name]] = val
                else:
                    raise UsageError(f"unrecognized option '{a}'")
            elif a.startswith("-") and a != "-" and not _is_number(a):
                j = 1
                while j < len(a):
                    c = a[j]
                    if c in short:
                        self.flags.add(c)
                    elif c in short_val:
                        val = a[j + 1:]
                        if not val:
                            i += 1
                            if i >= len(args):
                                raise UsageError(f"option requires an argument -- '{c}'")
                            val = args[i]
                        self.vals[c] = val
                        break
                    else:
                        raise UsageError(f"invalid option -- '{c}'")
                    j += 1
            else:
                self.args.append(a)
            i += 1

    def __contains__(self, flag: str) -> bool:
        return flag in self.flags


def _is_number(a: str) -> bool:
    return a[1:].isdigit()
