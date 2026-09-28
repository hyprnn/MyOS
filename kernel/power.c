/*
 * kernel/power.c - часы реального времени (CMOS), перезагрузка и
 * выключение - своими силами, без Runtime Services прошивки.
 * Часть MyOS; общие объявления - в myos.h.
 *
 * Раньше команды time/date, часы в GUI, reboot и shutdown звали
 * RuntimeServices прошивки. Это законно и после ExitBootServices,
 * но код Runtime Services живёт по физическим адресам прошивки, а
 * у ядра теперь свои таблицы страниц, где нижняя половина пуста -
 * звать его стало некуда. Да и настоящая ОС не должна зависеть от
 * прошивки в таких простых вещах.
 */
#include "myos.h"

/* ================================================================
 * Часы: микросхема CMOS RTC (порты 0x70 - номер регистра,
 * 0x71 - значение). Есть в любом PC с 1984 года.
 * ================================================================ */

static UINT8 rtc_reg(UINT8 r)
{
    /* бит 7 порта 0x70 - запрет NMI; не трогаем (держим 0) */
    io_out8(0x70, r);
    return io_in8(0x71);
}

static BOOLEAN rtc_updating(void)
{
    /* регистр A, бит 7: "идёт обновление, значения неверны" */
    return (rtc_reg(0x0A) & 0x80u) != 0;
}

static UINT8 rtc_bcd(UINT8 v, BOOLEAN binary)
{
    return binary ? v : (UINT8)((v & 0x0Fu) + (v >> 4) * 10u);
}

/*
 * Прочитать время. Часы обновляются раз в секунду, и посреди
 * обновления значения врут - поэтому ждём конца обновления и
 * читаем дважды подряд, пока два чтения не совпадут.
 * Время - то, что стоит в CMOS: на ноутбуке с Windows это
 * местное время, с Linux - обычно UTC (так же показывала и
 * прошивка).
 */
BOOLEAN rtc_read(EFI_TIME *t)
{
    UINT8 s, m, h, d, mo, y, cent;
    UINT8 s2, m2, h2, d2, mo2, y2;

    for (UINTN tries = 0; tries < 8; tries++) {

        UINTN guard = 0;

        while (rtc_updating() && guard++ < 100000u)
            cpu_pause();

        s = rtc_reg(0x00); m = rtc_reg(0x02); h = rtc_reg(0x04);
        d = rtc_reg(0x07); mo = rtc_reg(0x08); y = rtc_reg(0x09);

        guard = 0;

        while (rtc_updating() && guard++ < 100000u)
            cpu_pause();

        s2 = rtc_reg(0x00); m2 = rtc_reg(0x02); h2 = rtc_reg(0x04);
        d2 = rtc_reg(0x07); mo2 = rtc_reg(0x08); y2 = rtc_reg(0x09);

        if (s == s2 && m == m2 && h == h2 && d == d2 && mo == mo2 && y == y2)
            break;
    }

    UINT8 b = rtc_reg(0x0B);
    BOOLEAN binary = (b & 0x04u) != 0;   /* иначе BCD: 0x59 = 59 */
    BOOLEAN h24 = (b & 0x02u) != 0;

    BOOLEAN pm = (h & 0x80u) != 0;
    h &= 0x7Fu;

    UINT8 hour = rtc_bcd(h, binary);

    if (!h24) {
        if (hour == 12) hour = 0;
        if (pm) hour = (UINT8)(hour + 12u);
    }

    /* регистр века (0x32) есть не везде; если там чушь - 20xx */
    cent = rtc_bcd(rtc_reg(0x32), binary);

    if (cent < 19 || cent > 21)
        cent = 20;

    UINT8 *raw = (UINT8 *)t;
    for (UINTN i = 0; i < sizeof(EFI_TIME); i++)
        raw[i] = 0;

    t->Second = rtc_bcd(s, binary);
    t->Minute = rtc_bcd(m, binary);
    t->Hour = hour;
    t->Day = rtc_bcd(d, binary);
    t->Month = rtc_bcd(mo, binary);
    t->Year = (UINT16)(cent * 100u + rtc_bcd(y, binary));
    t->TimeZone = 2047;   /* EFI_UNSPECIFIED_TIMEZONE */

    /* совсем неправдоподобное значение - значит микросхемы нет */
    return t->Month >= 1 && t->Month <= 12 && t->Day >= 1 &&
           t->Day <= 31 && t->Hour < 24 && t->Minute < 60 &&
           t->Second < 60;
}

/* ================================================================
 * Перезагрузка
 * ================================================================ */

void kx_reboot(void)
{
    kx_cli();
    kcon_flush();
    klog("reboot requested\n");

    /* 1. Reset Control Register чипсета (порт 0xCF9): 0x02 -
          подготовить, 0x06 - "горячий" сброс всей платформы.
          Так перезагружаются почти все Intel/AMD с 2000-х. */
    io_out8(0xCF9, 0x02);
    busy_wait_ms(1);
    io_out8(0xCF9, 0x06);
    busy_wait_ms(50);

    /* 2. Старый способ: команда 0xFE контроллеру клавиатуры 8042
          ("дёрни линию RESET процессора") */
    for (UINTN i = 0; i < 100000u && (io_in8(0x64) & 0x02u); i++)
        cpu_pause();
    io_out8(0x64, 0xFE);
    busy_wait_ms(50);

    /* 3. Последнее средство: пустая IDT + исключение = тройная
          ошибка, процессор сбрасывается сам */
    KX_DTR zero = { 0, 0 };
    __asm__ __volatile__("lidt %0; int3" : : "m"(zero));

    for (;;)
        kx_hlt();
}

/* ================================================================
 * Выключение через ACPI
 *
 * Способ, которым выключают все ОС: записать в регистр PM1a_CNT
 * (его адрес - в таблице FADT) число SLP_TYP для состояния S5
 * ("мягкое выключение") и бит SLP_EN. Само число SLP_TYP лежит
 * в байт-коде AML внутри таблицы DSDT, в объекте "_S5_". Полный
 * интерпретатор AML - это этап 2; здесь - классический короткий
 * путь: найти в DSDT байты "_S5_" и прочитать два числа за ними.
 * Работает на подавляющем большинстве машин.
 * ================================================================ */

ACPI_POWER g_acpi_power;

static BOOLEAN acpi_sum_ok(const UINT8 *p, UINT32 len)
{
    UINT8 sum = 0;

    for (UINT32 i = 0; i < len; i++)
        sum = (UINT8)(sum + p[i]);

    return sum == 0;
}

/* Найти таблицу с подписью sig ("FACP", "APIC", ...) по RSDP */
const UINT8 *acpi_find_table(const char *sig)
{
    if (g_boot.rsdp_phys == 0)
        return NULL;

    const UINT8 *rsdp = (const UINT8 *)P2V(g_boot.rsdp_phys);

    if (rsdp[0] != 'R' || rsdp[1] != 'S' || rsdp[2] != 'D' || rsdp[3] != ' ')
        return NULL;

    UINT8 rev = rsdp[15];
    UINT64 root;
    UINTN entry;

    if (rev >= 2 && *(const UINT64 *)(rsdp + 24) != 0) {
        root = *(const UINT64 *)(rsdp + 24);       /* XSDT: 8-байтные ссылки */
        entry = 8;
    } else {
        root = *(const UINT32 *)(rsdp + 16);       /* RSDT: 4-байтные */
        entry = 4;
    }

    const UINT8 *rt = (const UINT8 *)P2V(root);
    UINT32 len = *(const UINT32 *)(rt + 4);

    if (len < 36 || len > 0x100000u)
        return NULL;

    for (UINT32 off = 36; off + entry <= len; off += (UINT32)entry) {

        UINT64 a = (entry == 8) ? *(const UINT64 *)(rt + off)
                                : *(const UINT32 *)(rt + off);
        const UINT8 *t = (const UINT8 *)P2V(a);

        if (t[0] == (UINT8)sig[0] && t[1] == (UINT8)sig[1] &&
            t[2] == (UINT8)sig[2] && t[3] == (UINT8)sig[3])
            return t;
    }

    return NULL;
}

/* Разобрать FADT и _S5_ - один раз, при старте ядра */
void acpi_power_init(void)
{
    ACPI_POWER *ap = &g_acpi_power;

    ap->ok = FALSE;

    const UINT8 *fadt = acpi_find_table("FACP");

    if (fadt == NULL) {
        ap->why = "no FADT table";
        return;
    }

    UINT32 flen = *(const UINT32 *)(fadt + 4);

    ap->smi_cmd = *(const UINT32 *)(fadt + 48);
    ap->acpi_enable = fadt[52];
    ap->pm1a_cnt = *(const UINT32 *)(fadt + 64);
    ap->pm1b_cnt = *(const UINT32 *)(fadt + 68);

    UINT64 dsdt = *(const UINT32 *)(fadt + 40);

    if (flen >= 148 && *(const UINT64 *)(fadt + 140) != 0)
        dsdt = *(const UINT64 *)(fadt + 140);    /* X_DSDT (ACPI 2.0+) */

    if (ap->pm1a_cnt == 0) {
        ap->why = "FADT has no PM1a control port (hardware-reduced ACPI)";
        return;
    }

    if (dsdt == 0) {
        ap->why = "no DSDT";
        return;
    }

    const UINT8 *d = (const UINT8 *)P2V(dsdt);
    UINT32 dlen = *(const UINT32 *)(d + 4);

    if (d[0] != 'D' || d[1] != 'S' || d[2] != 'D' || d[3] != 'T' ||
        dlen < 36 || dlen > 4u * 1024u * 1024u || !acpi_sum_ok(d, dlen)) {
        ap->why = "DSDT is damaged";
        return;
    }

    /* Ищем "_S5_" и за ним: 0x12 (Package), длина пакета (1-4
       байта), число элементов, затем SLP_TYPa и SLP_TYPb - каждое
       либо 0x0A <байт> (BytePrefix), либо голые 0x00 / 0x01
       (ZeroOp / OneOp). Перед "_S5_" стоит 0x08 (NameOp), иногда
       с префиксом пути '\\'. */
    for (UINT32 i = 36; i + 8 < dlen; i++) {

        if (d[i] != '_' || d[i + 1] != 'S' || d[i + 2] != '5' || d[i + 3] != '_')
            continue;

        if (!(d[i - 1] == 0x08 || (d[i - 2] == 0x08 && d[i - 1] == '\\')))
            continue;

        UINT32 p = i + 4;

        if (d[p] != 0x12)
            continue;

        p++;
        p += (UINT32)((d[p] >> 6) & 3u) + 1u;   /* длина пакета */
        p++;                                     /* число элементов */

        UINT8 v[2];

        for (UINTN k = 0; k < 2; k++) {
            if (d[p] == 0x0A) {
                v[k] = d[p + 1];
                p += 2;
            } else {
                v[k] = (d[p] == 0x01) ? 1 : 0;
                p += 1;
            }
        }

        ap->slp_typa = v[0];
        ap->slp_typb = v[1];
        ap->ok = TRUE;
        ap->why = "ok";
        return;
    }

    ap->why = "no _S5_ object in DSDT";
}

static void io_out16(UINT16 port, UINT16 v)
{
    __asm__ __volatile__("outw %0, %1" : : "a"(v), "Nd"(port));
}

static UINT16 io_in16(UINT16 port)
{
    UINT16 v;
    __asm__ __volatile__("inw %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

void kx_shutdown(void)
{
    ACPI_POWER *ap = &g_acpi_power;

    kcon_flush();
    klog("shutdown requested (ACPI: %s)\n", ap->why ? ap->why : "?");

    kx_cli();

    if (ap->ok) {

        /* ACPI ещё не включён (бит SCI_EN) - попросить прошивку
           (SMM) переключиться в режим ACPI */
        if (!(io_in16((UINT16)ap->pm1a_cnt) & 1u) &&
            ap->smi_cmd != 0 && ap->acpi_enable != 0) {

            io_out8((UINT16)ap->smi_cmd, ap->acpi_enable);

            for (UINTN i = 0; i < 300; i++) {
                if (io_in16((UINT16)ap->pm1a_cnt) & 1u)
                    break;
                busy_wait_ms(10);
            }
        }

        io_out16((UINT16)ap->pm1a_cnt,
                 (UINT16)((ap->slp_typa << 10) | (1u << 13)));

        if (ap->pm1b_cnt != 0)
            io_out16((UINT16)ap->pm1b_cnt,
                     (UINT16)((ap->slp_typb << 10) | (1u << 13)));

        busy_wait_ms(500);
    }

    /* Виртуальные машины понимают и "волшебные" порты */
    io_out16(0x604, 0x2000);    /* QEMU (q35 и новый pc) */
    io_out16(0xB004, 0x2000);   /* старые QEMU / Bochs */
    io_out16(0x4004, 0x3400);   /* VirtualBox */

    busy_wait_ms(200);
}
