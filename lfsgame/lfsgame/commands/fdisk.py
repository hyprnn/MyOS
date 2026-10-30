"""Interactive fdisk session (a process generator). Changes stay in memory until 'w'."""
from __future__ import annotations

import copy

from ..shell.runner import Input, Sleep
from ..sim import disks as dk

HELP = """
Help:

  DOS (MBR) / GPT
   d   delete a partition
   l   list known partition types
   n   add a new partition
   p   print the partition table
   t   change a partition type
   F   list free unpartitioned space

  Create a new label
   g   create a new empty GPT partition table
   o   create a new empty MBR (DOS) partition table

  Save & Exit
   w   write table to disk and exit
   q   quit without saving changes

"""


def print_table(ctx, disk: dict):
    size = disk["sectors"] * dk.SECTOR
    ctx.write(f"Disk /dev/{disk['name']}: {dk.size_label(size)}, {size} bytes, "
              f"{disk['sectors']} sectors\n")
    ctx.write(f"Disk model: {disk['model']}\n"
              "Units: sectors of 1 * 512 = 512 bytes\n"
              "Sector size (logical/physical): 512 bytes / 512 bytes\n"
              "I/O size (minimum/optimal): 512 bytes / 512 bytes\n")
    if not disk["label"]:
        return
    ctx.write(f"Disklabel type: {disk['label']}\nDisk identifier: {disk['id']}\n")
    if not disk["parts"]:
        return
    rows = []
    for p in sorted(disk["parts"], key=lambda p: p["n"]):
        sectors = p["end"] - p["start"] + 1
        row = [dk.part_dev(disk, p), str(p["start"]), str(p["end"]), str(sectors),
               dk.size_label(dk.part_bytes(p)).replace(" ", "").replace("iB", "")]
        if disk["label"] == "dos":
            row.insert(1, "")
            row.append(dk.PART_TYPES[p["type"]][2])
        row.append(dk.type_name(disk["label"], p["type"]))
        rows.append(row)
    head = (["Device", "Boot", "Start", "End", "Sectors", "Size", "Id", "Type"]
            if disk["label"] == "dos" else ["Device", "Start", "End", "Sectors", "Size", "Type"])
    widths = [max(len(r[i]) for r in rows + [head]) for i in range(len(head))]
    ctx.write("\n")
    for r in [head] + rows:
        cells = [r[0].ljust(widths[0])] + [c.rjust(w) for c, w in zip(r[1:-1], widths[1:-1])]
        ctx.write(" ".join(cells) + " " + r[-1] + "\n", "accent" if r is head else None)


def _ask(prompt):
    return Input(prompt)


def _choose_existing(ctx, disk, verb):
    nums = sorted(p["n"] for p in disk["parts"])
    if not nums:
        ctx.write("No partition is defined yet!\n", "err")
        return None
    if len(nums) == 1:
        ctx.write(f"Selected partition {nums[0]}\n")
        return nums[0]
    while True:
        ans = yield _ask(f"Partition number ({','.join(map(str, nums))}, default {nums[-1]}): ")
        if ans is None:
            return None
        ans = ans.strip()
        if not ans:
            return nums[-1]
        if ans.isdigit() and int(ans) in nums:
            return int(ans)
        ctx.write("Value out of range.\n", "err")


def _new_partition(ctx, disk, rng):
    if len(disk["parts"]) >= dk.max_parts(disk):
        ctx.write("All primary partitions are in use.\n" if disk["label"] == "dos"
                  else "All partitions are in use.\n", "err")
        return
    regions = dk.free_regions(disk)
    if not regions:
        ctx.write("No free sectors available.\n", "err")
        return
    if disk["label"] == "dos":
        used = len(disk["parts"])
        ctx.write("Partition type\n"
                  f"   p   primary ({used} primary, 0 extended, {4 - used} free)\n"
                  "   e   extended (container for logical partitions)\n")
        while True:
            ans = yield _ask("Select (default p): ")
            if ans is None:
                return
            raw = ans.strip().lower()
            ans = raw or "p"
            if ans == "p":
                if not raw:
                    ctx.write("Using default response p.\n")
                break
            if ans == "e":
                ctx.write("Extended partitions are not simulated; use a primary partition "
                          "(or switch to GPT with 'g').\n", "warn")
                continue
            ctx.write(f"Invalid partition type `{ans}'.\n", "err")
    free_nums = [n for n in range(1, dk.max_parts(disk) + 1)
                 if n not in {p["n"] for p in disk["parts"]}]
    lo_n, hi_n = free_nums[0], dk.max_parts(disk)
    while True:
        ans = yield _ask(f"Partition number ({lo_n}-{hi_n}, default {lo_n}): ")
        if ans is None:
            return
        ans = ans.strip()
        num = lo_n if not ans else (int(ans) if ans.isdigit() else -1)
        if num in free_nums:
            break
        if 1 <= num <= hi_n:
            ctx.write(f"Partition {num} is already defined.  Delete it before re-adding it.\n",
                      "err")
        else:
            ctx.write("Value out of range.\n", "err")
    lo, hi = regions[0][0], regions[-1][1]
    while True:
        ans = yield _ask(f"First sector ({lo}-{hi}, default {lo}): ")
        if ans is None:
            return
        ans = ans.strip()
        first = lo if not ans else (int(ans) if ans.isdigit() else -1)
        region = dk.region_for(disk, first)
        if region:
            break
        ctx.write("Value out of range.\n", "err")
    end = region[1]
    while True:
        ans = yield _ask(f"Last sector, +/-sectors or +/-size{{K,M,G,T,P}} "
                         f"({first}-{end}, default {end}): ")
        if ans is None:
            return
        ans = ans.strip()
        last = end if not ans else dk.parse_last(ans, first)
        if last is not None and first <= last <= end:
            break
        ctx.write("Value out of range.\n", "err")
    part = {"n": num, "start": first, "end": last, "type": "linux"}
    disk["parts"].append(part)
    disk["parts"].sort(key=lambda p: p["n"])
    ctx.write(f"\nCreated a new partition {num} of type "
              f"'{dk.type_name(disk['label'], 'linux')}' and of size "
              f"{dk.size_label(dk.part_bytes(part))}.\n\n")


def _change_type(ctx, disk):
    num = yield from _choose_existing(ctx, disk, "type")
    if num is None:
        return
    part = next(p for p in disk["parts"] if p["n"] == num)
    prompt = ("Partition type or alias (type L to list all): " if disk["label"] == "gpt"
              else "Hex code or alias (type L to list all): ")
    while True:
        ans = yield _ask(prompt)
        if ans is None:
            return
        ans = ans.strip()
        if ans.upper() == "L":
            ctx.write(dk.type_list(disk["label"]))
            continue
        key = dk.parse_type(disk["label"], ans)
        if key:
            break
        ctx.write(f"Failed to parse '{ans}' partition type.\n", "err")
    old = dk.type_name(disk["label"], part["type"])
    part["type"] = key
    ctx.write(f"Changed type of partition '{old}' to '{dk.type_name(disk['label'], key)}'.\n\n")


def session(ctx, name: str):
    """fdisk /dev/<name>"""
    m = ctx.machine
    disk = copy.deepcopy(m.disks[name])
    rng = m.rng
    ctx.write("\nWelcome to fdisk (util-linux 2.40.2).\n"
              "Changes will remain in memory only, until you decide to write them.\n"
              "Be careful before using the write command.\n\n", "dim")
    if m.disk_in_use(m.disks[name]):
        ctx.write("This disk is currently in use - repartitioning is probably a bad idea.\n"
                  "It's recommended to umount all file systems, and swapoff all swap\n"
                  "partitions on this disk.\n\n", "warn")
    if not disk["label"]:
        disk["label"], disk["id"] = "dos", dk.new_label_id("dos", rng)
        ctx.write("Device does not contain a recognized partition table.\n"
                  f"Created a new DOS (MBR) disklabel with disk identifier {disk['id']}.\n\n")
    while True:
        cmd = yield _ask("Command (m for help): ")
        if cmd is None:
            ctx.write("\n")
            return 0
        cmd = cmd.strip()
        if not cmd:
            continue
        c = cmd[0]
        if c == "m":
            ctx.write(HELP)
        elif c == "p":
            print_table(ctx, disk)
            ctx.write("\n")
        elif c == "l":
            ctx.write(dk.type_list(disk["label"]))
        elif c == "F":
            ctx.write(f"Unpartitioned space /dev/{name}:\n")
            for a, b in dk.free_regions(disk):
                ctx.write(f"  {a:>10} {b:>10} {b - a + 1:>10} sectors  "
                          f"{dk.size_label((b - a + 1) * dk.SECTOR)}\n")
        elif c == "g":
            disk["label"], disk["id"], disk["parts"] = "gpt", dk.new_label_id("gpt", rng), []
            ctx.write(f"Created a new GPT disklabel (GUID: {disk['id']}).\n\n")
        elif c == "o":
            disk["label"], disk["id"], disk["parts"] = "dos", dk.new_label_id("dos", rng), []
            ctx.write(f"Created a new DOS (MBR) disklabel with disk identifier {disk['id']}.\n\n")
        elif c == "n":
            yield from _new_partition(ctx, disk, rng)
        elif c == "d":
            num = yield from _choose_existing(ctx, disk, "delete")
            if num is not None:
                disk["parts"] = [p for p in disk["parts"] if p["n"] != num]
                ctx.write(f"Partition {num} has been deleted.\n\n")
        elif c == "t":
            yield from _change_type(ctx, disk)
        elif c == "w":
            ctx.write("The partition table has been altered.\n")
            if m.disk_in_use(m.disks[name]):
                yield Sleep(300)
                ctx.write("Calling ioctl() to re-read partition table.\n"
                          "Re-reading the partition table failed.: Device or resource busy\n",
                          "err")
                ctx.write("(simulation: the change was discarded to keep the running host "
                          "system alive — unmount/swapoff the disk first)\n", "warn")
                return 1
            m.apply_table(name, disk)
            yield Sleep(300)
            ctx.write("Calling ioctl() to re-read partition table.\nSyncing disks.\n")
            return 0
        elif c == "q":
            return 0
        else:
            ctx.write(f"{cmd}: unknown command\n", "err")
