"""Game = simulated Machine + progress (completed objectives). JSON-serializable."""
from __future__ import annotations

from typing import List, Optional

from .machine import Machine
from .objectives import BY_ID, OBJECTIVES, Objective


class Game:
    def __init__(self, machine: Optional[Machine] = None, done: Optional[List[str]] = None,
                 book_section: str = "0"):
        self.machine = machine or Machine.new()
        self.done: List[str] = list(done or [])
        self.book_section = book_section

    def to_dict(self) -> dict:
        return {"machine": self.machine.to_dict(), "done": list(self.done),
                "book_section": self.book_section}

    @classmethod
    def from_dict(cls, data: dict) -> "Game":
        return cls(Machine.from_dict(data["machine"]), [d for d in data.get("done", []) if d in BY_ID],
                   str(data.get("book_section", "0")))

    def check(self) -> List[Objective]:
        """Evaluate objectives; return the ones completed just now (in list order)."""
        new = []
        m = self.machine
        saved, m.croot = m.croot, ""  # objectives look at the real filesystem
        try:
            for o in OBJECTIVES:
                if o.id not in self.done and o.check(m):
                    self.done.append(o.id)
                    new.append(o)
            for oid in self.done:  # builds look at these (chapter 7 vs chapter 8 version)
                m.flag(f"obj:{oid}")
        finally:
            m.croot = saved
        return new

    def current(self) -> Optional[Objective]:
        for o in OBJECTIVES:
            if o.id not in self.done and not o.optional:
                return o
        return None

    def finished(self) -> bool:
        return self.current() is None

    def score(self):
        req = [o for o in OBJECTIVES if not o.optional]
        return sum(o.id in self.done for o in req), len(req)
