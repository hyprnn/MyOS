/*
 * kernel/acpi.c - таблицы ACPI: компьютер сам рассказывает, что у
 * него есть и где.
 * Часть MyOS; общие объявления - в myos.h.
 *
 * До этого этапа многое мы угадывали: I/O APIC "обычно" на
 * 0xFEC00000, конфигурация PCI - через старые порты 0xCF8/0xCFC,
 * эталон времени - только PIT. Теперь всё это берётся из таблиц,
 * которые прошивка оставляет в памяти для ОС:
 *
 *   RSDP  - "указатель на указатель": его адрес нам дал загрузчик
 *           (UEFI кладёт его в таблицу конфигурации);
 *   XSDT  - список адресов всех остальных таблиц (в старых ACPI 1.0
 *           - RSDT, то же, но с 4-байтными адресами);
 *   APIC  - она же MADT: сколько ядер процессора, где I/O APIC, как
 *           старые IRQ (таймер, клавиатура) разведены по линиям;
 *   FACP  - она же FADT: порты управления питанием, таймер ACPI PM,
 *           регистр перезагрузки, адрес DSDT;
 *   MCFG  - где окно PCIe в памяти (ECAM);
 *   HPET  - где высокоточный таймер;
 *   DSDT/SSDT - байт-код AML (батарея, крышка, кнопка питания...);
 *           его исполняет библиотека uACPI - см. acpi_dev.c (этап 9).
 *
 * Каждая таблица начинается с одинакового 36-байтного заголовка:
 *   [0]  подпись (4 буквы)   [4]  длина всей таблицы
 *   [8]  ревизия             [9]  контрольная сумма
 *   [10] OEM ID (6 букв)     [16] OEM Table ID (8 букв)
 * Контрольная сумма: сумма ВСЕХ байт таблицы по модулю 256 = 0.
 * Таблицу с неверной суммой показываем, но не используем.
 */
#include "myos.h"

ACPI_INFO g_acpi;

/* --- чтение с проверкой границ: таблицы лежат в памяти прошивки,
   и если прошивка ошиблась в длине, лучше не читать чужое --- */

static UINT8 a_u8(const UINT8 *t, UINT32 len, UINT32 off)
{
    return (off + 1u <= len) ? t[off] : 0;
}

static UINT16 a_u16(const UINT8 *t, UINT32 len, UINT32 off)
{
    return (off + 2u <= len) ? *(const UINT16 *)(t + off) : 0;
}

static UINT32 a_u32(const UINT8 *t, UINT32 len, UINT32 off)
{
    return (off + 4u <= len) ? *(const UINT32 *)(t + off) : 0;
}

static UINT64 a_u64(const UINT8 *t, UINT32 len, UINT32 off)
{
    return (off + 8u <= len) ? *(const UINT64 *)(t + off) : 0;
}

static BOOLEAN a_sum(const UINT8 *p, UINT32 len)
{
    UINT8 s = 0;

    for (UINT32 i = 0; i < len; i++)
        s = (UINT8)(s + p[i]);

    return s == 0;
}

/* Сделать таблицу видимой (она может лежать выше последней RAM,
   куда прямое отображение не доходит) и вернуть указатель на неё.
   len = 0 - сначала заголовок, потом по длине из него. */
static const UINT8 *a_map(UINT64 phys, UINT32 *len_out)
{
    if (phys == 0)
        return NULL;

    if (!vmm_ensure_mapped(phys, 36, 0))
        return NULL;

    const UINT8 *t = (const UINT8 *)P2V(phys);
    UINT32 len = *(const UINT32 *)(t + 4);

    if (len < 36 || len > 16u * 1024u * 1024u)
        return NULL;

    if (!vmm_ensure_mapped(phys, len, 0))
        return NULL;

    if (len_out)
        *len_out = len;

    return t;
}

static void a_copy_str(char *dst, const UINT8 *src, UINTN n)
{
    for (UINTN i = 0; i < n; i++) {
        UINT8 c = src[i];
        dst[i] = (c >= 32 && c < 127) ? (char)c : ' ';
    }
    dst[n] = '\0';

    /* хвостовые пробелы - долой */
    for (INTN i = (INTN)n - 1; i >= 0 && dst[i] == ' '; i--)
        dst[i] = '\0';
}

static BOOLEAN a_sig(const UINT8 *t, const char *sig)
{
    return t[0] == (UINT8)sig[0] && t[1] == (UINT8)sig[1] &&
           t[2] == (UINT8)sig[2] && t[3] == (UINT8)sig[3];
}

/* Запомнить таблицу в списке (для команды acpi) */
static ACPI_TABLE_INFO *a_record(UINT64 phys, const UINT8 *t, UINT32 len)
{
    if (g_acpi.ntables >= ACPI_MAX_TABLES)
        return NULL;

    ACPI_TABLE_INFO *ti = &g_acpi.tables[g_acpi.ntables++];

    for (UINTN i = 0; i < 4; i++)
        ti->sig[i] = (t[i] >= 32 && t[i] < 127) ? (char)t[i] : '?';
    ti->sig[4] = '\0';

    ti->phys = phys;
    ti->len = len;
    ti->rev = t[8];
    ti->sum_ok = a_sum(t, len);
    a_copy_str(ti->oem, t + 10, 6);
    a_copy_str(ti->oem_table, t + 16, 8);

    return ti;
}

/* Найти таблицу по подписи (n = какую по счёту, 0 - первую).
   Таблицы с неверной контрольной суммой не выдаются. */
const UINT8 *acpi_table(const char *sig, UINTN n, UINT32 *len_out)
{
    for (UINTN i = 0; i < g_acpi.ntables; i++) {

        ACPI_TABLE_INFO *ti = &g_acpi.tables[i];

        if (ti->sig[0] != sig[0] || ti->sig[1] != sig[1] ||
            ti->sig[2] != sig[2] || ti->sig[3] != sig[3] || !ti->sum_ok)
            continue;

        if (n-- > 0)
            continue;

        if (len_out)
            *len_out = ti->len;

        return (const UINT8 *)P2V(ti->phys);
    }

    return NULL;
}

/* Старое имя (power.c) */
const UINT8 *acpi_find_table(const char *sig)
{
    return acpi_table(sig, 0, NULL);
}

/* ================================================================
 * MADT: процессоры, I/O APIC, переназначения IRQ
 * ================================================================ */

static void a_parse_madt(const UINT8 *t, UINT32 len)
{
    g_acpi.have_madt = TRUE;
    g_acpi.lapic_addr = a_u32(t, len, 36);
    g_acpi.pcat_compat = (a_u32(t, len, 40) & 1u) != 0;

    /* дальше - записи переменной длины: [0] тип, [1] длина */
    UINT32 off = 44;

    while (off + 2u <= len) {

        UINT8 type = t[off];
        UINT8 elen = t[off + 1];

        if (elen < 2 || off + elen > len)
            break;

        const UINT8 *e = t + off;

        if (type == 0 && elen >= 8) {                  /* Local APIC */

            if (g_acpi.ncpus < ACPI_MAX_CPUS) {
                ACPI_CPU *c = &g_acpi.cpus[g_acpi.ncpus++];
                UINT32 fl = *(const UINT32 *)(e + 4);
                c->uid = e[2];
                c->apic_id = e[3];
                c->enabled = (fl & 1u) != 0;
                c->online_capable = (fl & 2u) != 0;
                c->x2 = FALSE;
            }

        } else if (type == 9 && elen >= 16) {          /* x2APIC */

            if (g_acpi.ncpus < ACPI_MAX_CPUS) {
                ACPI_CPU *c = &g_acpi.cpus[g_acpi.ncpus++];
                UINT32 fl = *(const UINT32 *)(e + 8);
                c->apic_id = *(const UINT32 *)(e + 4);
                c->uid = *(const UINT32 *)(e + 12);
                c->enabled = (fl & 1u) != 0;
                c->online_capable = (fl & 2u) != 0;
                c->x2 = TRUE;
            }

        } else if (type == 1 && elen >= 12) {          /* I/O APIC */

            if (g_acpi.nioapics < ACPI_MAX_IOAPICS) {
                ACPI_IOAPIC *io = &g_acpi.ioapics[g_acpi.nioapics++];
                io->id = e[2];
                io->addr = *(const UINT32 *)(e + 4);
                io->gsi_base = *(const UINT32 *)(e + 8);
                io->count = 0;      /* узнаем из самого I/O APIC */
            }

        } else if (type == 2 && elen >= 10) {          /* переназначение IRQ */

            if (g_acpi.nisos < ACPI_MAX_ISOS) {
                ACPI_ISO *s = &g_acpi.isos[g_acpi.nisos++];
                s->bus = e[2];
                s->irq = e[3];
                s->gsi = *(const UINT32 *)(e + 4);
                s->flags = *(const UINT16 *)(e + 8);
            }

        } else if ((type == 4 && elen >= 6) || (type == 0x0A && elen >= 12)) {

            g_acpi.n_lapic_nmi++;                     /* NMI на LINT */

        } else if (type == 5 && elen >= 12) {          /* 64-битный адрес LAPIC */

            g_acpi.lapic_addr = *(const UINT64 *)(e + 4);
        }

        off += elen;
    }

    for (UINTN i = 0; i < g_acpi.ncpus; i++)
        if (g_acpi.cpus[i].enabled)
            g_acpi.ncpus_enabled++;
}

/* ================================================================
 * FADT: питание, таймер PM, перезагрузка, DSDT
 * ================================================================ */

/* "Generic Address Structure" (12 байт): [0] пространство
   (0 - память, 1 - порты, 2 - конфигурация PCI), [1] ширина в битах,
   [2] сдвиг, [3] размер доступа, [4] 64-битный адрес */
static void a_gas(const UINT8 *t, UINT32 len, UINT32 off,
                  UINT8 *space, UINT64 *addr)
{
    *space = a_u8(t, len, off);
    *addr = a_u64(t, len, off + 4);
}

static void a_parse_fadt(const UINT8 *t, UINT32 len)
{
    g_acpi.have_fadt = TRUE;
    g_acpi.fadt_rev = t[8];

    g_acpi.sci_irq = a_u16(t, len, 46);
    g_acpi.smi_cmd = a_u32(t, len, 48);
    g_acpi.acpi_enable = a_u8(t, len, 52);
    g_acpi.pm1a_evt = a_u32(t, len, 56);
    g_acpi.pm1a_cnt = a_u32(t, len, 64);
    g_acpi.pm1b_cnt = a_u32(t, len, 68);
    g_acpi.pm_tmr = a_u32(t, len, 76);
    g_acpi.century = a_u8(t, len, 108);
    g_acpi.boot_arch = a_u16(t, len, 109);
    g_acpi.fadt_flags = a_u32(t, len, 112);

    g_acpi.pm_tmr_32 = (g_acpi.fadt_flags & (1u << 8)) != 0;   /* TMR_VAL_EXT */
    g_acpi.hw_reduced = (g_acpi.fadt_flags & (1u << 20)) != 0;

    /* регистр перезагрузки (ACPI 2.0+, флаг RESET_REG_SUP) */
    if (len >= 129 && (g_acpi.fadt_flags & (1u << 10))) {
        a_gas(t, len, 116, &g_acpi.reset_space, &g_acpi.reset_addr);
        g_acpi.reset_value = a_u8(t, len, 128);
        g_acpi.reset_ok = g_acpi.reset_addr != 0 && g_acpi.reset_space <= 2;
    }

    /* таймер PM из 64-битного поля, если старое пустое */
    if (g_acpi.pm_tmr == 0 && len >= 220) {
        UINT8 sp; UINT64 a;
        a_gas(t, len, 208, &sp, &a);
        if (sp == 1 && a < 0x10000u)
            g_acpi.pm_tmr = (UINT32)a;
    }

    UINT64 dsdt = a_u32(t, len, 40);

    if (len >= 148 && a_u64(t, len, 140) != 0)
        dsdt = a_u64(t, len, 140);                   /* X_DSDT */

    /* DSDT не в списке XSDT - добавим сами */
    UINT32 dlen = 0;
    const UINT8 *d = a_map(dsdt, &dlen);

    if (d != NULL && a_sig(d, "DSDT")) {
        g_acpi.dsdt = dsdt;
        g_acpi.dsdt_len = dlen;
        a_record(dsdt, d, dlen);
    }

    /* FACS - тоже из FADT (нужна для сна S3, пока только показываем) */
    UINT64 facs = a_u32(t, len, 36);

    if (len >= 140 && a_u64(t, len, 132) != 0)
        facs = a_u64(t, len, 132);

    if (facs != 0 && vmm_ensure_mapped(facs, 64, 0)) {
        const UINT8 *f = (const UINT8 *)P2V(facs);
        if (a_sig(f, "FACS") && g_acpi.ntables < ACPI_MAX_TABLES) {
            /* у FACS нет обычного заголовка и контрольной суммы */
            ACPI_TABLE_INFO *ti = &g_acpi.tables[g_acpi.ntables++];
            ti->sig[0] = 'F'; ti->sig[1] = 'A'; ti->sig[2] = 'C';
            ti->sig[3] = 'S'; ti->sig[4] = '\0';
            ti->phys = facs;
            ti->len = *(const UINT32 *)(f + 4);
            ti->rev = f[32];
            ti->sum_ok = TRUE;
            ti->oem[0] = '\0';
            ti->oem_table[0] = '\0';
        }
    }
}

/* ================================================================
 * MCFG: окно конфигурации PCIe в памяти (ECAM)
 * ================================================================ */

static void a_parse_mcfg(const UINT8 *t, UINT32 len)
{
    /* записи по 16 байт с 44-го: база(8), сегмент(2), шины от/до */
    for (UINT32 off = 44; off + 16u <= len; off += 16u) {

        if (g_acpi.n_mcfg == 0) {
            g_acpi.ecam_base = *(const UINT64 *)(t + off);
            g_acpi.ecam_seg = *(const UINT16 *)(t + off + 8);
            g_acpi.ecam_bus_start = t[off + 10];
            g_acpi.ecam_bus_end = t[off + 11];
        }

        g_acpi.n_mcfg++;
    }
}

/* ================================================================
 * HPET
 * ================================================================ */

static void a_parse_hpet(const UINT8 *t, UINT32 len)
{
    UINT8 space = a_u8(t, len, 40);
    UINT64 addr = a_u64(t, len, 44);

    if (space != 0 || addr == 0)      /* HPET всегда в памяти */
        return;

    g_acpi.have_hpet = TRUE;
    g_acpi.hpet_addr = addr;
    g_acpi.hpet_min_tick = a_u16(t, len, 53);
}

/* Включить счётчик HPET и прочитать его параметры. FALSE - не
   отвечает (все единицы - устройства по этому адресу нет). */
static BOOLEAN a_hpet_start(void)
{
    if (!g_acpi.have_hpet)
        return FALSE;

    if (!vmm_map_mmio(g_acpi.hpet_addr, 1024, VMM_UC))
        return FALSE;

    UINT64 cap = mmio_read64(g_acpi.hpet_addr + 0x00);

    if (cap == ~0ull || cap == 0)
        return FALSE;

    UINT32 period = (UINT32)(cap >> 32);    /* фемтосекунд на тик */

    /* по спеке период не больше 100 нс (10^8 фс) */
    if (period == 0 || period > 100000000u)
        return FALSE;

    g_acpi.hpet_period_fs = period;
    g_acpi.hpet_timers = (UINT8)(((cap >> 8) & 0x1Fu) + 1u);
    g_acpi.hpet_64 = (cap & (1u << 13)) != 0;

    /* GEN_CONF (0x10), бит 0 - запустить главный счётчик.
       Бит 1 (замена PIT/RTC) не трогаем. */
    UINT64 conf = mmio_read64(g_acpi.hpet_addr + 0x10);

    if (!(conf & 1u))
        mmio_write64(g_acpi.hpet_addr + 0x10, conf | 1u);

    /* счётчик должен идти */
    UINT64 c0 = mmio_read64(g_acpi.hpet_addr + 0xF0);
    for (volatile UINTN i = 0; i < 10000; i++)
        cpu_pause();
    UINT64 c1 = mmio_read64(g_acpi.hpet_addr + 0xF0);

    g_acpi.hpet_ok = (c1 != c0);

    return g_acpi.hpet_ok;
}

UINT64 acpi_hpet_counter(void)
{
    return g_acpi.hpet_ok ? mmio_read64(g_acpi.hpet_addr + 0xF0) : 0;
}

/*
 * Частота TSC по HPET: сколько тактов TSC уложилось в ~10 мс по
 * HPET. Три попытки, берём минимум (задержки только добавляют).
 */
UINT64 acpi_hpet_measure_tsc_hz(void)
{
    if (!g_acpi.hpet_ok)
        return 0;

    UINT64 period = g_acpi.hpet_period_fs;
    UINT64 want = 10000000000000ull / period;   /* 10 мс в тиках */
    UINT64 best = 0;

    /* 32-битный счётчик сравниваем по модулю 2^32 */
    UINT64 m = g_acpi.hpet_64 ? ~0ull : 0xFFFFFFFFull;

    for (UINTN attempt = 0; attempt < 3; attempt++) {

        UINT64 c0 = mmio_read64(g_acpi.hpet_addr + 0xF0) & m;
        UINT64 t0 = rdtsc();
        UINT64 ticks;
        UINT64 guard = 0;

        do {
            ticks = ((mmio_read64(g_acpi.hpet_addr + 0xF0) & m) - c0) & m;
            /* страховка по времени: не дольше ~0.2 с (по замеру
               загрузчика), иначе на медленном железе "лимит
               попыток" растянулся бы на десятки секунд */
            if ((++guard & 0xFFu) == 0 && g_tsc_hz_stall != 0 &&
                rdtsc() - t0 > g_tsc_hz_stall / 5u)
                return 0;
            if (guard > 50000000ull)
                return 0;
        } while (ticks < want);

        UINT64 t1 = rdtsc();
        UINT64 ns = (ticks * period) / 1000000ull;

        if (ns == 0)
            continue;

        UINT64 hz = ((t1 - t0) * 1000000000ull) / ns;

        if (best == 0 || hz < best)
            best = hz;
    }

    return best;
}

/*
 * Частота TSC по таймеру ACPI PM: он тикает с частотой 3 579 545 Гц
 * (как и PIT, кратно старому телевизионному кварцу), 24 или 32 бита,
 * читается из порта.
 */
UINT64 acpi_pmtimer_measure_tsc_hz(void)
{
    if (g_acpi.pm_tmr == 0 || g_acpi.pm_tmr > 0xFFFFu)
        return 0;

    UINT16 port = (UINT16)g_acpi.pm_tmr;
    UINT32 mask = g_acpi.pm_tmr_32 ? 0xFFFFFFFFu : 0x00FFFFFFu;
    UINT32 want = 35795u;              /* ~10 мс */
    UINT64 best = 0;

    /* живой ли он вообще */
    UINT32 a = io_in32(port) & mask;
    for (volatile UINTN i = 0; i < 20000; i++)
        cpu_pause();
    if ((io_in32(port) & mask) == a)
        return 0;

    for (UINTN attempt = 0; attempt < 3; attempt++) {

        UINT32 c0 = io_in32(port) & mask;
        UINT64 t0 = rdtsc();
        UINT32 d;
        UINT64 guard = 0;

        do {
            d = ((io_in32(port) & mask) - c0) & mask;
            if ((++guard & 0xFFu) == 0 && g_tsc_hz_stall != 0 &&
                rdtsc() - t0 > g_tsc_hz_stall / 5u)
                return 0;
            if (guard > 20000000ull)
                return 0;
        } while (d < want);

        UINT64 t1 = rdtsc();
        UINT64 hz = ((t1 - t0) * 3579545ull) / d;

        if (best == 0 || hz < best)
            best = hz;
    }

    return best;
}

/* ================================================================
 * Главное: пройти по всем таблицам
 * ================================================================ */

BOOLEAN acpi_init(void)
{
    UINT8 *raw = (UINT8 *)&g_acpi;

    for (UINTN i = 0; i < sizeof(g_acpi); i++)
        raw[i] = 0;

    g_acpi.why = "no RSDP from the loader";

    UINT64 rp = g_boot.rsdp_phys;

    if (rp == 0 || !vmm_ensure_mapped(rp, 36, 0))
        return FALSE;

    const UINT8 *rsdp = (const UINT8 *)P2V(rp);

    if (rsdp[0] != 'R' || rsdp[1] != 'S' || rsdp[2] != 'D' || rsdp[3] != ' ' ||
        rsdp[4] != 'P' || rsdp[5] != 'T' || rsdp[6] != 'R' || rsdp[7] != ' ') {
        g_acpi.why = "RSDP signature is wrong";
        return FALSE;
    }

    /* контрольная сумма первых 20 байт (ACPI 1.0), для 2.0+ ещё и
       всей расширенной структуры (36 байт) */
    if (!a_sum(rsdp, 20) || (rsdp[15] >= 2 && !a_sum(rsdp, 36))) {
        g_acpi.why = "RSDP checksum is wrong";
        return FALSE;
    }

    g_acpi.rsdp = rp;
    g_acpi.revision = rsdp[15];
    a_copy_str(g_acpi.oem, rsdp + 9, 6);

    UINT64 root;
    UINT32 esize;

    if (rsdp[15] >= 2 && *(const UINT64 *)(rsdp + 24) != 0) {
        root = *(const UINT64 *)(rsdp + 24);
        esize = 8;
        g_acpi.xsdt = TRUE;
    } else {
        root = *(const UINT32 *)(rsdp + 16);
        esize = 4;
        g_acpi.xsdt = FALSE;
    }

    UINT32 rlen = 0;
    const UINT8 *rt = a_map(root, &rlen);

    if (rt == NULL || !a_sig(rt, esize == 8 ? "XSDT" : "RSDT")) {
        g_acpi.why = "root table (XSDT/RSDT) is missing";
        return FALSE;
    }

    g_acpi.root = root;
    a_record(root, rt, rlen);

    for (UINT32 off = 36; off + esize <= rlen; off += esize) {

        UINT64 a = (esize == 8) ? *(const UINT64 *)(rt + off)
                                : *(const UINT32 *)(rt + off);
        UINT32 len = 0;
        const UINT8 *t = a_map(a, &len);

        if (t != NULL)
            a_record(a, t, len);
    }

    /* разбор нужных таблиц (только с верной суммой) */
    UINT32 len;
    const UINT8 *t;

    if ((t = acpi_table("APIC", 0, &len)) != NULL)
        a_parse_madt(t, len);

    if ((t = acpi_table("FACP", 0, &len)) != NULL)
        a_parse_fadt(t, len);

    if ((t = acpi_table("MCFG", 0, &len)) != NULL)
        a_parse_mcfg(t, len);

    if ((t = acpi_table("HPET", 0, &len)) != NULL)
        a_parse_hpet(t, len);

    a_hpet_start();

    /* сколько байт AML (DSDT + все SSDT) - для команды acpi */
    g_acpi.aml_bytes = g_acpi.dsdt_len > 36 ? g_acpi.dsdt_len - 36 : 0;

    for (UINTN i = 0; i < g_acpi.ntables; i++) {
        if (g_acpi.tables[i].sig[0] == 'S' && g_acpi.tables[i].sig[1] == 'S' &&
            g_acpi.tables[i].sig[2] == 'D' && g_acpi.tables[i].sig[3] == 'T') {
            g_acpi.n_ssdt++;
            if (g_acpi.tables[i].len > 36)
                g_acpi.aml_bytes += g_acpi.tables[i].len - 36;
        }
    }

    g_acpi.present = TRUE;
    g_acpi.why = "ok";

    return TRUE;
}

/* Номер APIC ядра, на котором мы сейчас работаем */
UINT32 acpi_current_apic_id(void)
{
    if (g_lapic_x2)
        return (UINT32)kx_rdmsr(0x802u);

    if (g_lapic_base != 0)
        return kx_lapic_read(0x20) >> 24;

    UINT32 a, b, c, d;
    __asm__ __volatile__("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(1u), "c"(0));
    (void)a; (void)c; (void)d;

    return b >> 24;
}

/* ================================================================
 * Команда acpi
 * ================================================================ */

static const char *a_polarity(UINT16 f)
{
    switch (f & 3u) {
    case 1:  return "active high";
    case 3:  return "active low";
    default: return "bus default";
    }
}

static const char *a_trigger(UINT16 f)
{
    switch ((f >> 2) & 3u) {
    case 1:  return "edge";
    case 3:  return "level";
    default: return "bus default";
    }
}

void kernel_cmd_acpi(SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    if (!g_acpi.present) {
        kprintf(out, "ACPI: not available (%s)\n", g_acpi.why);
        return;
    }

    kprintf(out, "ACPI %s, OEM \"%s\", RSDP at 0x%llx, %s at 0x%llx\n",
            g_acpi.revision >= 2 ? "2.0+" : "1.0", g_acpi.oem,
            g_acpi.rsdp, g_acpi.xsdt ? "XSDT" : "RSDT", g_acpi.root);

    kprintf(out, "\nTables (%u):\n", (UINT32)g_acpi.ntables);

    for (UINTN i = 0; i < g_acpi.ntables; i++) {
        ACPI_TABLE_INFO *ti = &g_acpi.tables[i];
        kprintf(out, "  %s  at 0x%09llx  %6u bytes  rev %u  %-6s %-8s  %s\n",
                ti->sig, ti->phys, ti->len, ti->rev, ti->oem, ti->oem_table,
                ti->sum_ok ? "" : "BAD CHECKSUM");
    }

    if (g_acpi.have_madt) {

        UINT32 me = acpi_current_apic_id();

        kprintf(out, "\nCPU cores (MADT): %u enabled", (UINT32)g_acpi.ncpus_enabled);
        if (g_acpi.ncpus > g_acpi.ncpus_enabled)
            kprintf(out, " + %u disabled/hot-plug slots",
                    (UINT32)(g_acpi.ncpus - g_acpi.ncpus_enabled));
        kprintf(out, "; MyOS runs on APIC id %u (only this one for now)\n", me);

        print(out, "  APIC ids:");
        for (UINTN i = 0; i < g_acpi.ncpus; i++) {
            if (!g_acpi.cpus[i].enabled)
                continue;
            kprintf(out, " %u%s", g_acpi.cpus[i].apic_id,
                    g_acpi.cpus[i].apic_id == me ? "*" : "");
        }
        print(out, "\n");

        kprintf(out, "Local APIC: 0x%llx (from MADT)%s\n", g_acpi.lapic_addr,
                g_acpi.pcat_compat ? ", old 8259 PIC also present" : "");

        for (UINTN i = 0; i < g_acpi.nioapics; i++) {
            ACPI_IOAPIC *io = &g_acpi.ioapics[i];
            kprintf(out, "I/O APIC #%u: id %u at 0x%llx, interrupt lines (GSI) %u..%u\n",
                    (UINT32)i, io->id, io->addr, io->gsi_base,
                    io->gsi_base + (io->count ? io->count - 1u : 0u));
        }

        if (g_acpi.nisos > 0) {
            print(out, "Legacy IRQ -> GSI overrides:\n");
            for (UINTN i = 0; i < g_acpi.nisos; i++) {
                ACPI_ISO *s = &g_acpi.isos[i];
                kprintf(out, "  IRQ %2u -> GSI %2u  (%s, %s)\n", s->irq, s->gsi,
                        a_trigger(s->flags), a_polarity(s->flags));
            }
        }
    } else {
        print(out, "\nNo MADT - CPU count and I/O APIC unknown.\n");
    }

    print(out, "\n");

    if (g_acpi.n_mcfg > 0)
        kprintf(out, "PCIe ECAM (MCFG): 0x%llx, segment %u, buses %u..%u%s\n",
                g_acpi.ecam_base, g_acpi.ecam_seg, g_acpi.ecam_bus_start,
                g_acpi.ecam_bus_end,
                g_pci_ecam_active ? " - MyOS uses it" : " - not used (check failed)");
    else
        print(out, "PCIe ECAM: none (PCI config via ports 0xCF8/0xCFC)\n");

    if (g_acpi.hpet_ok) {
        UINT64 khz = 1000000000000ull / g_acpi.hpet_period_fs;
        kprintf(out, "HPET: 0x%llx, %llu.%03llu MHz, %u timers, %s counter\n",
                g_acpi.hpet_addr, khz / 1000u, khz % 1000u,
                g_acpi.hpet_timers, g_acpi.hpet_64 ? "64-bit" : "32-bit");
    } else if (g_acpi.have_hpet) {
        kprintf(out, "HPET: listed at 0x%llx but does not respond\n", g_acpi.hpet_addr);
    } else {
        print(out, "HPET: none\n");
    }

    if (g_acpi.have_fadt) {
        kprintf(out, "FADT rev %u: SCI on IRQ %u, PM1a control port 0x%x",
                g_acpi.fadt_rev, g_acpi.sci_irq, g_acpi.pm1a_cnt);
        if (g_acpi.pm_tmr)
            kprintf(out, ", PM timer port 0x%x (%s)", g_acpi.pm_tmr,
                    g_acpi.pm_tmr_32 ? "32-bit" : "24-bit");
        print(out, "\n");

        if (g_acpi.reset_ok)
            kprintf(out, "  reset register: %s 0x%llx <- 0x%02x (used by 'reboot')\n",
                    g_acpi.reset_space == 1 ? "port" :
                    g_acpi.reset_space == 0 ? "memory" : "PCI config",
                    g_acpi.reset_addr, g_acpi.reset_value);
        else
            print(out, "  reset register: not provided (reboot via 0xCF9 / 8042)\n");

        kprintf(out, "  RTC century register: %s", g_acpi.century ? "" : "none");
        if (g_acpi.century)
            kprintf(out, "0x%02x", g_acpi.century);
        kprintf(out, "; 8042 keyboard controller: %s%s\n",
                (g_acpi.boot_arch & 2u) ? "present" : "not declared",
                g_acpi.hw_reduced ? "; HARDWARE-REDUCED ACPI" : "");
    }

    kprintf(out, "Shutdown: %s", g_acpi_power.ok ? "ACPI S5" : "no ACPI S5");
    if (g_acpi_power.ok)
        kprintf(out, " (SLP_TYP %u/%u)", g_acpi_power.slp_typa, g_acpi_power.slp_typb);
    kprintf(out, " - %s\n", g_acpi_power.why ? g_acpi_power.why : "?");

    kprintf(out, "AML byte code: DSDT + %u SSDT, %u KiB - not executed yet\n",
            (UINT32)g_acpi.n_ssdt, g_acpi.aml_bytes / 1024u);
    print(out, "  (battery, lid, power button need an AML interpreter - see the plan)\n");
}
