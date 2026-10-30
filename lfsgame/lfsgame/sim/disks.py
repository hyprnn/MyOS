"""Virtual block disks and partition tables (GPT or DOS/MBR). Pure data helpers.

Disk dict: {"name": "sdb", "sectors": int, "model": str, "label": None|"gpt"|"dos",
            "id": str, "parts": [{"n": 1, "start": int, "end": int, "type": key}]}
All sizes in 512-byte sectors; partitions are aligned to 2048 sectors (1 MiB).
"""
from __future__ import annotations

import random
import re
from typing import List, Optional, Tuple

SECTOR = 512
ALIGN = 2048
GiB = 1024 ** 3

# key -> (GPT number, GPT name, DOS hex, DOS name)
PART_TYPES = {
    "uefi":  (1, "EFI System", "ef", "EFI (FAT-12/16/32)"),
    "bios":  (4, "BIOS boot", None, None),
    "swap":  (19, "Linux swap", "82", "Linux swap / Solaris"),
    "linux": (20, "Linux filesystem", "83", "Linux"),
}


def type_name(label: str, key: str) -> str:
    gpt_n, gpt_name, _, dos_name = PART_TYPES[key]
    return gpt_name if label == "gpt" else dos_name


def parse_type(label: str, value: str) -> Optional[str]:
    """Accept alias (linux/swap/uefi/bios), GPT number or DOS hex code."""
    v = value.strip().lower()
    if v in PART_TYPES:
        key = v
    else:
        key = None
        for k, (gn, _, dh, _) in PART_TYPES.items():
            if label == "gpt" and v == str(gn):
                key = k
            if label == "dos" and dh is not None and v in (dh, "0x" + dh):
                key = k
    if key is None:
        return None
    if label == "dos" and PART_TYPES[key][2] is None:
        return None
    return key


def type_list(label: str) -> str:
    rows = []
    for key, (gn, gname, dh, dname) in sorted(PART_TYPES.items(), key=lambda kv: kv[1][0]):
        if label == "gpt":
            rows.append(f" {gn:>3} {gname:<22} (alias: {key})")
        elif dh is not None:
            rows.append(f" {dh:>3}  {dname:<22} (alias: {key})")
    rows.append("(this simulation knows only these types)")
    return "\n".join(rows) + "\n"


def new_disk(name: str, size_bytes: int, model: str) -> dict:
    return {"name": name, "sectors": size_bytes // SECTOR, "model": model,
            "label": None, "id": "", "parts": []}


def new_label_id(label: str, rng: random.Random) -> str:
    if label == "dos":
        return "0x%08x" % rng.getrandbits(32)
    h = "%032X" % rng.getrandbits(128)
    return f"{h[:8]}-{h[8:12]}-{h[12:16]}-{h[16:20]}-{h[20:]}"


def usable_range(disk: dict) -> Tuple[int, int]:
    """First and last sector a partition may use."""
    if disk["label"] == "gpt":
        return ALIGN, disk["sectors"] - 34
    return ALIGN, disk["sectors"] - 1


def max_parts(disk: dict) -> int:
    return 128 if disk["label"] == "gpt" else 4


def free_regions(disk: dict) -> List[Tuple[int, int]]:
    lo, hi = usable_range(disk)
    regions, cur = [], lo
    for p in sorted(disk["parts"], key=lambda p: p["start"]):
        if p["start"] > cur:
            regions.append((cur, p["start"] - 1))
        cur = max(cur, p["end"] + 1)
    if cur <= hi:
        regions.append((cur, hi))
    out = []
    for a, b in regions:  # align starts up to 1 MiB
        a = -(-a // ALIGN) * ALIGN
        if a <= b:
            out.append((a, b))
    return out


def region_for(disk: dict, sector: int) -> Optional[Tuple[int, int]]:
    for a, b in free_regions(disk):
        if a <= sector <= b:
            return a, b
    return None


SIZE_RE = re.compile(r"^\+(\d+)([KMGTP]?)(?:i?B)?$", re.I)


def parse_last(value: str, first: int) -> Optional[int]:
    """'' handled by caller. '+10G' / '+512M' / '+2048' (sectors) / absolute sector."""
    v = value.strip()
    if v.isdigit():
        return int(v)
    m = SIZE_RE.match(v)
    if not m:
        return None
    num, unit = int(m.group(1)), m.group(2).upper()
    if not unit:
        return first + num - 1
    size = num * 1024 ** ("KMGTP".index(unit) + 1)
    return first + size // SECTOR - 1


def part_dev(disk: dict, part: dict) -> str:
    return f"/dev/{disk['name']}{part['n']}"


def part_bytes(part: dict) -> int:
    return (part["end"] - part["start"] + 1) * SECTOR


def size_label(nbytes: int) -> str:
    """fdisk-style sizes: '10 GiB', '512 MiB'."""
    for unit, power in (("TiB", 4), ("GiB", 3), ("MiB", 2), ("KiB", 1)):
        v = nbytes / 1024 ** power
        if v >= 1:
            s = f"{v:.1f}".rstrip("0").rstrip(".")
            return f"{s} {unit}"
    return f"{nbytes} B"
