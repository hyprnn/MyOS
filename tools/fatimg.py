#!/usr/bin/env python3
"""
tools/fatimg.py - образы дисков с FAT для автотеста MyOS и их проверка.

Делает то, что на Arch делают mkfs.fat / sgdisk / fsck.fat, но на
чистом Python (в среде сборки этих программ может не быть):

  * FatBuilder  - "отформатировать" FAT16/FAT32 и положить файлы и папки
                  (с длинными именами, как это делает Linux);
  * make_disk   - собрать диск: MBR или GPT + раздел с FAT;
  * FatReader   - прочитать том НЕЗАВИСИМО от кода MyOS и проверить его
                  на целостность (как fsck.fat -n): цепочки кластеров,
                  потерянные и общие кластеры, размеры, обе копии FAT,
                  контрольные суммы длинных имён, "." и "..", FSInfo.

Если MyOS записала файл так, что FatReader его читает и не находит
ошибок, - его прочитает и Linux.

    python3 tools/fatimg.py check disk.img        # проверить все разделы
    python3 tools/fatimg.py ls disk.img /myos     # посмотреть папку
"""
import os
import struct, sys, uuid, zlib

SECTOR = 512

# ----------------------------------------------------------------- запись


def lfn_checksum(sn):
    s = 0
    for b in sn:
        s = (((s & 1) << 7) + (s >> 1) + b) & 0xFF
    return s


class FatBuilder:
    """Новый том FAT в памяти (bytearray) с последовательным выделением
    кластеров - как свежая флешка, на которую Linux скопировал файлы."""

    def __init__(self, sectors, bits=32, label='MYOSTEST'):
        self.bits = bits
        self.total = sectors
        if bits == 32:
            self.spc = 1
            self.reserved = 32
            self.root_entries = 0
        else:
            self.spc = 2 if sectors <= 131072 else 4
            self.reserved = 4
            self.root_entries = 512
        self.nfats = 2
        root_secs = self.root_entries * 32 // SECTOR
        ent = 4 if bits == 32 else 2
        fatsz = 1
        while True:
            data = sectors - self.reserved - self.nfats * fatsz - root_secs
            clusters = data // self.spc
            need = ((clusters + 2) * ent + SECTOR - 1) // SECTOR
            if need <= fatsz:
                break
            fatsz = need
        self.fatsz = fatsz
        self.root_sector = self.reserved + self.nfats * fatsz
        self.data_start = self.root_sector + root_secs
        self.clusters = (sectors - self.data_start) // self.spc
        if bits == 32:
            assert self.clusters >= 65525, 'too small for FAT32'
        else:
            assert 4085 <= self.clusters < 65525, 'wrong size for FAT16'
        self.cb = self.spc * SECTOR
        self.img = bytearray(sectors * SECTOR)
        self.fat = [0] * (self.clusters + 2)
        self.fat[0] = 0x0FFFFFF8 if bits == 32 else 0xFFF8
        self.fat[1] = 0x0FFFFFFF if bits == 32 else 0xFFFF
        self.next = 2
        self.label = label
        # содержимое папок: ключ - кластер папки (0 - корень FAT16)
        self.dirs = {}
        if bits == 32:
            self.root = self.alloc_chain(1)
        else:
            self.root = 0
        self.dirs[self.root] = []

    def eoc(self):
        return 0x0FFFFFFF if self.bits == 32 else 0xFFFF

    def alloc_chain(self, n):
        first = self.next
        for i in range(n):
            c = self.next + i
            self.fat[c] = c + 1 if i < n - 1 else self.eoc()
        self.next += n
        return first

    def cl_off(self, c):
        return (self.data_start + (c - 2) * self.spc) * SECTOR

    def short_name(self, name, dcl):
        base, _, ext = name.rpartition('.') if '.' in name else (name, '', '')
        ok = lambda s, n: 0 < len(s) <= n and all(ch.isalnum() or ch in "_-!#$%&'()@^`{}~" for ch in s)
        if ok(base, 8) and (ext == '' or ok(ext, 3)) and name.count('.') <= 1 \
                and (base.isupper() or not any(c.isalpha() for c in base)) \
                and (ext.isupper() or not any(c.isalpha() for c in ext)):
            sn = (base.upper().ljust(8) + ext.upper().ljust(3)).encode()
            return sn, False
        clean = lambda s: ''.join(ch if (ch.isalnum() or ch in '_-') else '_' for ch in s.upper().replace(' ', ''))
        b, e = clean(base)[:6] or '_', clean(ext)[:3]
        used = {x[0] for x in self.dirs[dcl]}
        for n in range(1, 100):
            tail = '~%d' % n
            sn = ((b[:8 - len(tail)] + tail).ljust(8) + e.ljust(3)).encode()
            if sn not in used:
                return sn, True
        raise RuntimeError('no alias')

    def add(self, dcl, name, attr, first, size):
        sn, lfn = self.short_name(name, dcl)
        self.dirs[dcl].append((sn, name if lfn else None, attr, first, size))

    def mkdir(self, dcl, name):
        c = self.alloc_chain(1)
        self.dirs[c] = []
        self.add(dcl, name, 0x10, c, 0)
        self.parent = getattr(self, 'parent', {})
        self.parent[c] = dcl
        return c

    def add_file(self, dcl, name, data):
        n = (len(data) + self.cb - 1) // self.cb
        first = self.alloc_chain(n) if n else 0
        for i in range(n):
            o = self.cl_off(first + i)
            chunk = data[i * self.cb:(i + 1) * self.cb]
            self.img[o:o + len(chunk)] = chunk
        self.add(dcl, name, 0x20, first, len(data))

    def entries(self, dcl):
        out = []
        if dcl != self.root:
            par = self.parent[dcl]
            pc = 0 if par == self.root else par
            out.append(self.raw_ent(b'.          ', 0x10, dcl, 0))
            out.append(self.raw_ent(b'..         ', 0x10, pc, 0))
        elif self.label:
            out.append(self.raw_ent(self.label.upper().ljust(11)[:11].encode(), 0x08, 0, 0))
        for sn, lfn, attr, first, size in self.dirs[dcl]:
            if lfn:
                chk = lfn_checksum(sn)
                u = lfn.encode('utf-16-le')
                chars = [u[i:i + 2] for i in range(0, len(u), 2)]
                chars.append(b'\0\0')
                while len(chars) % 13:
                    chars.append(b'\xff\xff')
                parts = [chars[i:i + 13] for i in range(0, len(chars), 13)]
                if len(lfn) % 13 == 0:
                    parts = parts[:-1] if all(c == b'\xff\xff' or c == b'\0\0' for c in parts[-1]) else parts
                for k in range(len(parts), 0, -1):
                    p = parts[k - 1]
                    e = bytearray(32)
                    e[0] = k | (0x40 if k == len(parts) else 0)
                    e[1:11] = b''.join(p[0:5])
                    e[11] = 0x0F
                    e[13] = chk
                    e[14:26] = b''.join(p[5:11])
                    e[28:32] = b''.join(p[11:13])
                    out.append(bytes(e))
            out.append(self.raw_ent(sn, attr, first, size))
        return out

    def raw_ent(self, sn, attr, first, size):
        e = bytearray(32)
        e[0:11] = sn
        e[11] = attr
        date = ((2026 - 1980) << 9) | (9 << 5) | 1
        struct.pack_into('<HHHHHHHI', e, 14, 0x6000, date, date, (first >> 16) if self.bits == 32 else 0,
                         0x6000, date, first & 0xFFFF, size)
        return bytes(e)

    def build(self):
        # папки: разложить записи (корень FAT32 и подпапки могут удлиниться)
        for dcl in list(self.dirs):
            ents = self.entries(dcl)
            data = b''.join(ents)
            if dcl == 0:
                assert len(ents) <= self.root_entries
                o = self.root_sector * SECTOR
                self.img[o:o + len(data)] = data
                continue
            per = self.cb // 32
            need = max(1, (len(ents) + per - 1) // per)
            chain = [dcl]
            while len(chain) < need:
                c = self.alloc_chain(1)
                self.fat[chain[-1]] = c
                chain.append(c)
            for i, c in enumerate(chain):
                o = self.cl_off(c)
                part = data[i * self.cb:(i + 1) * self.cb]
                self.img[o:o + len(part)] = part
        bs = bytearray(SECTOR)
        bs[0:3] = b'\xEB\x58\x90' if self.bits == 32 else b'\xEB\x3C\x90'
        bs[3:11] = b'MSWIN4.1'
        struct.pack_into('<HBHBHHBHHHII', bs, 11, SECTOR, self.spc, self.reserved, self.nfats,
                         self.root_entries, self.total if self.total < 65536 else 0, 0xF8,
                         0 if self.bits == 32 else self.fatsz, 63, 255, 0,
                         0 if self.total < 65536 else self.total)
        lab = (self.label or 'NO NAME').upper().ljust(11)[:11].encode()
        if self.bits == 32:
            struct.pack_into('<IHHIHH', bs, 36, self.fatsz, 0, 0, self.root, 1, 6)
            struct.pack_into('<BBBI11s8s', bs, 64, 0x80, 0, 0x29, 0x1234ABCD, lab, b'FAT32   ')
        else:
            struct.pack_into('<BBBI11s8s', bs, 36, 0x80, 0, 0x29, 0x1234ABCD, lab, b'FAT16   ')
        bs[510:512] = b'\x55\xAA'
        self.img[0:SECTOR] = bs
        if self.bits == 32:
            self.img[6 * SECTOR:7 * SECTOR] = bs
            fi = bytearray(SECTOR)
            free = sum(1 for c in range(2, self.clusters + 2) if self.fat[c] == 0)
            struct.pack_into('<I', fi, 0, 0x41615252)
            struct.pack_into('<III', fi, 484, 0x61417272, free, self.next)
            fi[510:512] = b'\x55\xAA'
            self.img[SECTOR:2 * SECTOR] = fi
        fmt = '<%dI' % len(self.fat) if self.bits == 32 else '<%dH' % len(self.fat)
        fatb = struct.pack(fmt, *self.fat)
        for i in range(self.nfats):
            o = (self.reserved + i * self.fatsz) * SECTOR
            self.img[o:o + len(fatb)] = fatb
        return self.img


def make_disk(path, total_mib, part_bytes, scheme='mbr', gpt_type='basic'):
    """Диск total_mib МиБ: таблица scheme ('mbr', 'gpt', 'none') и
    один раздел с 1 МиБ (сектор 2048) - содержимое part_bytes."""
    total = total_mib * 1024 * 1024 // SECTOR
    img = bytearray(total * SECTOR)
    if scheme == 'none':
        img[0:len(part_bytes)] = part_bytes
        open(path, 'wb').write(img)
        return
    start = 2048
    count = len(part_bytes) // SECTOR
    img[start * SECTOR:start * SECTOR + len(part_bytes)] = part_bytes
    if scheme == 'mbr':
        e = bytearray(16)
        bits = 32 if struct.unpack_from('<H', part_bytes, 22)[0] == 0 else 16
        e[4] = 0x0C if bits == 32 else 0x0E
        struct.pack_into('<II', e, 8, start, count)
        img[446:462] = e
        img[510], img[511] = 0x55, 0xAA
    else:
        e = bytearray(16)
        e[4] = 0xEE
        struct.pack_into('<II', e, 8, 1, min(total - 1, 0xFFFFFFFF))
        img[446:462] = e
        img[510], img[511] = 0x55, 0xAA
        types = {'basic': 'EBD0A0A2-B9E5-4433-87C0-68B6B72699C7',
                 'efi': 'C12A7328-F81F-11D2-BA4B-00A0C93EC93B'}
        ents = bytearray(128 * 128)
        ents[0:16] = uuid.UUID(types[gpt_type]).bytes_le
        ents[16:32] = uuid.uuid4().bytes_le
        struct.pack_into('<QQQ', ents, 32, start, start + count - 1, 0)
        ents[56:56 + 2 * len('MyOS test')] = 'MyOS test'.encode('utf-16-le')
        ecrc = zlib.crc32(ents) & 0xFFFFFFFF

        def header(my, alt, ent_lba):
            h = bytearray(92)
            h[0:8] = b'EFI PART'
            struct.pack_into('<IIIIQQQQ16sQIII', h, 8, 0x00010000, 92, 0, 0, my, alt,
                             34, total - 34, uuid.uuid4().bytes_le, ent_lba, 128, 128, ecrc)
            struct.pack_into('<I', h, 16, zlib.crc32(h) & 0xFFFFFFFF)
            return h
        img[SECTOR:SECTOR + 92] = header(1, total - 1, 2)
        img[2 * SECTOR:2 * SECTOR + len(ents)] = ents
        img[(total - 33) * SECTOR:(total - 33) * SECTOR + len(ents)] = ents
        img[(total - 1) * SECTOR:(total - 1) * SECTOR + 92] = header(total - 1, 1, total - 33)
    open(path, 'wb').write(img)


def gpt_wrap(path, part_path, gpt_type, name='MyOS test'):
    """Диск path: GPT и один раздел с сектора 2048, содержимое - файл
    part_path (большой - например, ext4 с корнем Linux на сотни МиБ:
    поэтому без чтения в память, а копированием кусками и с "дырами")."""
    psize = os.path.getsize(part_path)
    start = 2048
    count = (psize + SECTOR - 1) // SECTOR
    total = start + count + 2048
    types = {'basic': 'EBD0A0A2-B9E5-4433-87C0-68B6B72699C7',
             'efi': 'C12A7328-F81F-11D2-BA4B-00A0C93EC93B',
             'linux': '0FC63DAF-8483-4772-8E79-3D69D8477DE4'}
    ents = bytearray(128 * 128)
    ents[0:16] = uuid.UUID(types[gpt_type]).bytes_le
    ents[16:32] = uuid.uuid4().bytes_le
    struct.pack_into('<QQQ', ents, 32, start, start + count - 1, 0)
    ents[56:56 + 2 * len(name)] = name.encode('utf-16-le')
    ecrc = zlib.crc32(ents) & 0xFFFFFFFF

    def header(my, alt, ent_lba):
        h = bytearray(92)
        h[0:8] = b'EFI PART'
        struct.pack_into('<IIIIQQQQ16sQIII', h, 8, 0x00010000, 92, 0, 0, my, alt,
                         34, total - 34, uuid.uuid4().bytes_le, ent_lba, 128, 128, ecrc)
        struct.pack_into('<I', h, 16, zlib.crc32(h) & 0xFFFFFFFF)
        return h
    mbr = bytearray(SECTOR)
    e = bytearray(16)
    e[4] = 0xEE
    struct.pack_into('<II', e, 8, 1, min(total - 1, 0xFFFFFFFF))
    mbr[446:462] = e
    mbr[510], mbr[511] = 0x55, 0xAA
    with open(path, 'wb') as f:
        f.truncate(total * SECTOR)
        f.seek(0)
        f.write(mbr)
        f.write(header(1, total - 1, 2) + bytes(SECTOR - 92))
        f.write(ents)
        f.seek((total - 33) * SECTOR)
        f.write(ents)
        f.seek((total - 1) * SECTOR)
        f.write(header(total - 1, 1, total - 33))
        with open(part_path, 'rb') as src:
            off = start * SECTOR
            while True:
                chunk = src.read(1 << 20)
                if not chunk:
                    break
                if chunk.count(0) != len(chunk):      # нули - "дыра", не пишем
                    f.seek(off)
                    f.write(chunk)
                off += len(chunk)


# ----------------------------------------------------------------- чтение


class FsckError(Exception):
    pass


class FatReader:
    def __init__(self, img, offset=0):
        self.img = img
        self.off = offset
        b = self.sec(0)
        if b[510:512] != b'\x55\xAA':
            raise FsckError('no boot signature')
        (bps, self.spc, self.reserved, self.nfats, self.root_entries, t16, _,
         f16, _, _, _, t32) = struct.unpack_from('<HBHBHHBHHHII', b, 11)
        if bps != SECTOR:
            raise FsckError('bytes per sector %d' % bps)
        self.total = t16 or t32
        self.fatsz = f16 or struct.unpack_from('<I', b, 36)[0]
        root_secs = (self.root_entries * 32 + SECTOR - 1) // SECTOR
        self.root_sector = self.reserved + self.nfats * self.fatsz
        self.data_start = self.root_sector + root_secs
        self.clusters = (self.total - self.data_start) // self.spc
        self.bits = 16 if self.clusters < 65525 else 32
        self.cb = self.spc * SECTOR
        self.root = struct.unpack_from('<I', b, 44)[0] if self.bits == 32 else 0
        self.fsinfo = struct.unpack_from('<H', b, 48)[0] if self.bits == 32 else 0
        ent = self.bits // 8
        fats = []
        for i in range(self.nfats):
            o = self.off + (self.reserved + i * self.fatsz) * SECTOR
            raw = bytes(self.img[o:o + (self.clusters + 2) * ent])
            fats.append(raw)
        if any(f != fats[0] for f in fats[1:]):
            raise FsckError('the FAT copies differ')
        fmt = '<%d%s' % (self.clusters + 2, 'I' if self.bits == 32 else 'H')
        self.fat = list(struct.unpack(fmt, fats[0]))
        if self.bits == 32:
            self.fat = [x & 0x0FFFFFFF for x in self.fat]

    def sec(self, n):
        o = self.off + n * SECTOR
        return bytes(self.img[o:o + SECTOR])

    def is_eoc(self, x):
        return x >= (0x0FFFFFF8 if self.bits == 32 else 0xFFF8)

    def chain(self, first):
        out, seen, c = [], set(), first
        while True:
            if c < 2 or c >= self.clusters + 2:
                raise FsckError('chain from %d goes to bad cluster %d' % (first, c))
            if c in seen:
                raise FsckError('loop in chain from %d' % first)
            seen.add(c)
            out.append(c)
            n = self.fat[c]
            if self.is_eoc(n):
                return out
            if n == 0:
                raise FsckError('chain from %d hits a free cluster' % first)
            c = n

    def cl_data(self, c):
        o = self.off + (self.data_start + (c - 2) * self.spc) * SECTOR
        return bytes(self.img[o:o + self.cb])

    def dir_raw(self, dcl):
        if dcl == 0 and self.bits == 16:
            o = self.off + self.root_sector * SECTOR
            return bytes(self.img[o:o + self.root_entries * 32]), None
        ch = self.chain(dcl)
        return b''.join(self.cl_data(c) for c in ch), ch

    def listdir(self, dcl):
        """[(имя, атрибут, первый кластер, размер, короткое имя)]"""
        raw, _ = self.dir_raw(dcl)
        out, lfn, want, chk = [], {}, 0, None
        for i in range(0, len(raw), 32):
            e = raw[i:i + 32]
            if e[0] == 0:
                break
            if e[0] == 0xE5:
                lfn = {}
                continue
            if e[11] == 0x0F:
                seq = e[0] & 0x1F
                if e[0] & 0x40:
                    lfn, want, chk = {}, seq, e[13]
                elif chk is None or e[13] != chk:
                    raise FsckError('orphan long-name entry')
                if e[13] != chk:
                    raise FsckError('long-name checksum mismatch inside one name')
                lfn[seq] = e[1:11] + e[14:26] + e[28:32]
                continue
            if e[11] & 0x08:
                lfn = {}
                continue
            sn = e[0:11]
            name = None
            if lfn:
                if lfn_checksum(sn) != chk or sorted(lfn) != list(range(1, want + 1)):
                    raise FsckError('long name does not match %r' % sn)
                u = b''.join(lfn[k] for k in range(1, want + 1)).decode('utf-16-le')
                name = u.split('\0')[0]
            else:
                b = sn[0:8].decode('latin-1').rstrip()
                x = sn[8:11].decode('latin-1').rstrip()
                if e[12] & 0x08:
                    b = b.lower()
                if e[12] & 0x10:
                    x = x.lower()
                name = b + ('.' + x if x else '')
            lfn = {}
            hi = struct.unpack_from('<H', e, 20)[0] if self.bits == 32 else 0
            first = (hi << 16) | struct.unpack_from('<H', e, 26)[0]
            size = struct.unpack_from('<I', e, 28)[0]
            out.append((name, e[11], first, size, sn))
        return out

    def lookup(self, path):
        dcl = self.root
        parts = [p for p in path.split('/') if p]
        ent = ('/', 0x10, self.root, 0, b'')
        for i, p in enumerate(parts):
            for e in self.listdir(dcl):
                if e[0].lower() == p.lower():
                    ent = e
                    break
            else:
                return None
            dcl = ent[2]
        return ent

    def read(self, path):
        e = self.lookup(path)
        if e is None:
            return None
        if e[3] == 0:
            return b''
        data = b''.join(self.cl_data(c) for c in self.chain(e[2]))
        return data[:e[3]]

    def fsck(self):
        """Проверка целостности; возвращает список проблем (пусто - ок)"""
        problems = []
        owner = {}

        def claim(chain, who):
            for c in chain:
                if c in owner:
                    problems.append('cluster %d used by %s and %s' % (c, owner[c], who))
                owner[c] = who

        def walk(dcl, path, parent):
            try:
                raw, ch = self.dir_raw(dcl)
            except FsckError as e:
                problems.append('%s: %s' % (path, e))
                return
            if ch:
                claim(ch, path)
            if ch and dcl != self.root:
                # "." и ".."
                d0, d1 = raw[0:32], raw[32:64]
                if d0[0:11] != b'.          ' or d1[0:11] != b'..         ':
                    problems.append('%s: no . / .. entries' % path)
                else:
                    f0 = ((struct.unpack_from('<H', d0, 20)[0] << 16) if self.bits == 32 else 0) | struct.unpack_from('<H', d0, 26)[0]
                    f1 = ((struct.unpack_from('<H', d1, 20)[0] << 16) if self.bits == 32 else 0) | struct.unpack_from('<H', d1, 26)[0]
                    if f0 != dcl:
                        problems.append('%s: "." points to %d, not %d' % (path, f0, dcl))
                    want = 0 if parent == self.root else parent
                    if f1 != want:
                        problems.append('%s: ".." points to %d, not %d' % (path, f1, want))
            try:
                ents = self.listdir(dcl)
            except FsckError as e:
                problems.append('%s: %s' % (path, e))
                return
            names = set()
            for name, attr, first, size, sn in ents:
                if name in ('.', '..'):
                    continue
                if name.lower() in names:
                    problems.append('%s: duplicate name %s' % (path, name))
                names.add(name.lower())
                p = path.rstrip('/') + '/' + name
                if attr & 0x10:
                    if first == 0:
                        problems.append('%s: folder without clusters' % p)
                        continue
                    walk(first, p, dcl)
                else:
                    if size == 0:
                        if first != 0:
                            problems.append('%s: empty file owns clusters' % p)
                        continue
                    try:
                        ch = self.chain(first)
                    except FsckError as e:
                        problems.append('%s: %s' % (p, e))
                        continue
                    need = (size + self.cb - 1) // self.cb
                    if len(ch) != need:
                        problems.append('%s: size %d needs %d clusters, chain has %d' % (p, size, need, len(ch)))
                    claim(ch, p)

        walk(self.root, '/', self.root)
        used = {c for c in range(2, self.clusters + 2) if self.fat[c] != 0}
        lost = used - set(owner)
        if lost:
            problems.append('%d lost cluster(s) (allocated but no file uses them)' % len(lost))
        if self.bits == 32 and self.fsinfo:
            fi = self.sec(self.fsinfo)
            freec = struct.unpack_from('<I', fi, 488)[0]
            real = self.clusters - len(used)
            if freec != 0xFFFFFFFF and freec != real:
                problems.append('FSInfo says %d free clusters, really %d' % (freec, real))
        return problems


def partitions(img):
    """[(смещение в байтах, размер)] - FAT-тома на диске"""
    if img[510:512] != b'\x55\xAA':
        return []
    b0 = img[0:SECTOR]
    if b0[0] in (0xEB, 0xE9) and b0[82:85] == b'FAT' or b0[54:57] == b'FAT':
        return [(0, len(img))]
    out = []
    if img[446 + 4] == 0xEE:
        h = img[SECTOR:2 * SECTOR]
        ent_lba, n, esz = struct.unpack_from('<QII', h, 72)
        for i in range(n):
            o = ent_lba * SECTOR + i * esz
            if img[o:o + 16] == b'\0' * 16:
                continue
            first, last = struct.unpack_from('<QQ', img, o + 32)
            out.append((first * SECTOR, (last - first + 1) * SECTOR))
    else:
        for i in range(4):
            e = img[446 + i * 16:462 + i * 16]
            if e[4]:
                s, c = struct.unpack_from('<II', e, 8)
                out.append((s * SECTOR, c * SECTOR))
    return out


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 1
    cmd, path = sys.argv[1], sys.argv[2]
    img = bytearray(open(path, 'rb').read())
    bad = 0
    for off, size in partitions(img):
        r = FatReader(img, off)
        if cmd == 'check':
            p = r.fsck()
            print('volume at %d MiB: FAT%d, %s' % (off >> 20, r.bits, 'OK' if not p else 'PROBLEMS:'))
            for x in p:
                print('   ', x)
            bad += len(p)
        elif cmd == 'ls':
            where = sys.argv[3] if len(sys.argv) > 3 else '/'
            e = r.lookup(where)
            if e is None:
                print('no', where)
                continue
            for name, attr, first, sz, sn in r.listdir(e[2]):
                print('%s%-40s %8d  (%s)' % ('d ' if attr & 0x10 else '  ', name, sz, sn.decode('latin-1')))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
