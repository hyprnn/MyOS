/*
 * kernel/irq.c - прерывания от устройств: кто что прислал и кому
 * это отдать. Плюс учёт загрузки процессора.
 * Часть MyOS; общие объявления - в myos.h.
 *
 * До этапа 3 все драйверы работали ОПРОСОМ: шелл и GUI постоянно
 * спрашивали "есть что-нибудь?" у USB-контроллера и клавиатуры.
 * Теперь устройства сами "дёргают" процессор:
 *
 *   * старые устройства (PS/2-клавиатура, PS/2-мышь) - через
 *     I/O APIC: у каждой линии (GSI) - своя запись, в которой
 *     сказано, какой вектор IDT вызвать и на каком ядре. Какая
 *     линия у "IRQ 1" - говорит таблица ACPI MADT (переназначения);
 *   * PCI-устройства (USB-контроллер xHCI) - через MSI: устройство
 *     само пишет в особый адрес памяти (0xFEE00000 + номер ядра)
 *     число - номер вектора, и Local APIC превращает эту запись в
 *     прерывание. Проводов и I/O APIC тут нет вовсе.
 *
 * Векторы (номера в IDT):
 *   0x21 PS/2-клавиатура, 0x2C PS/2-мышь, 0x40 таймер,
 *   0x50 USB (xHCI), 0xFF "ложное" прерывание LAPIC.
 */
#include "myos.h"

static KX_IRQ_HANDLER g_irq_handlers[256];
UINT64 g_irq_count[256];

void kx_irq_register(UINT8 vector, KX_IRQ_HANDLER fn)
{
    g_irq_handlers[vector] = fn;
}

/* Вызывается из kx_isr_dispatch для векторов 32..255.
   TRUE - нашёлся обработчик (EOI уже отправлен). */
BOOLEAN kx_irq_dispatch(UINT8 vector)
{
    KX_IRQ_HANDLER fn = g_irq_handlers[vector];

    if (fn == NULL)
        return FALSE;

    g_irq_count[vector]++;
    fn();
    kx_lapic_eoi();

    return TRUE;
}

/* ================================================================
 * I/O APIC
 * ================================================================ */

static UINT32 ioapic_read(UINT64 base, UINT32 reg)
{
    mmio_write32(base + 0x00, reg);
    return mmio_read32(base + 0x10);
}

static void ioapic_write(UINT64 base, UINT32 reg, UINT32 v)
{
    mmio_write32(base + 0x00, reg);
    mmio_write32(base + 0x10, v);
}

/* Старый номер IRQ (как у PC-AT: 1 - клавиатура, 12 - PS/2-мышь)
   -> линия GSI и её режим, с учётом переназначений из MADT.
   По умолчанию для ISA: та же линия, по фронту, активный высокий. */
UINT32 kx_irq_to_gsi(UINT8 irq, BOOLEAN *level, BOOLEAN *active_low)
{
    *level = FALSE;
    *active_low = FALSE;

    for (UINTN i = 0; i < g_acpi.nisos; i++) {

        ACPI_ISO *s = &g_acpi.isos[i];

        if (s->bus != 0 || s->irq != irq)
            continue;

        if ((s->flags & 3u) == 3u)
            *active_low = TRUE;

        if (((s->flags >> 2) & 3u) == 3u)
            *level = TRUE;

        return s->gsi;
    }

    return irq;
}

/* Направить линию gsi на вектор vector текущего ядра. FALSE - нет
   I/O APIC с такой линией. */
BOOLEAN kx_ioapic_route(UINT32 gsi, UINT8 vector, BOOLEAN level, BOOLEAN active_low)
{
    UINT64 base = 0;
    UINT32 first = 0;

    if (g_acpi.nioapics > 0) {

        for (UINTN i = 0; i < g_acpi.nioapics; i++) {
            ACPI_IOAPIC *io = &g_acpi.ioapics[i];
            if (gsi >= io->gsi_base && gsi < io->gsi_base + io->count) {
                base = io->addr;
                first = io->gsi_base;
                break;
            }
        }

    } else {

        base = 0xFEC00000ull;          /* без MADT - стандартный адрес */
        first = 0;
    }

    if (base == 0)
        return FALSE;

    UINT32 idx = gsi - first;
    UINT32 apic = acpi_current_apic_id();

    /* нижнее слово: вектор, режим доставки 000 (Fixed), физическая
       адресация, полярность (бит 13), срабатывание (бит 15: 1 =
       по уровню), маска (бит 16) = 0; верхнее - номер ядра */
    UINT32 lo = (UINT32)vector |
                (active_low ? (1u << 13) : 0u) |
                (level ? (1u << 15) : 0u);

    ioapic_write(base, 0x10u + 2u * idx + 1u, apic << 24);
    ioapic_write(base, 0x10u + 2u * idx, lo);

    (void)ioapic_read;

    return TRUE;
}

/* Замаскировать линию gsi (устройство перешло на опрос: например,
   сетевая карта, чьё прерывание так и не пришло) */
void kx_ioapic_mask(UINT32 gsi)
{
    UINT64 base = 0;
    UINT32 first = 0;

    if (g_acpi.nioapics > 0) {
        for (UINTN i = 0; i < g_acpi.nioapics; i++) {
            ACPI_IOAPIC *io = &g_acpi.ioapics[i];
            if (gsi >= io->gsi_base && gsi < io->gsi_base + io->count) {
                base = io->addr;
                first = io->gsi_base;
                break;
            }
        }
    } else {
        base = 0xFEC00000ull;
    }

    if (base == 0)
        return;

    UINT32 idx = gsi - first;
    UINT32 lo = ioapic_read(base, 0x10u + 2u * idx);

    ioapic_write(base, 0x10u + 2u * idx, lo | (1u << 16));
}

/* ================================================================
 * MSI / MSI-X для PCI-устройств
 * ================================================================ */

/* Найти "возможность" (capability) id в списке PCI. 0 - нет. */
UINT8 pci_find_cap(UINT8 bus, UINT8 dev, UINT8 fn, UINT8 id)
{
    UINT32 st = pci_config_read32(bus, dev, fn, 0x04) >> 16;

    if (!(st & (1u << 4)))            /* нет списка возможностей */
        return 0;

    UINT8 p = (UINT8)(pci_config_read32(bus, dev, fn, 0x34) & 0xFCu);

    for (UINTN guard = 0; p != 0 && guard < 48; guard++) {

        UINT32 v = pci_config_read32(bus, dev, fn, p);

        if ((v & 0xFFu) == id)
            return p;

        p = (UINT8)((v >> 8) & 0xFCu);
    }

    return 0;
}

/*
 * Включить у устройства MSI (или, если его нет, MSI-X) на вектор
 * vector текущего ядра. Заодно выключить старое прерывание по
 * проводу (INTx), чтобы оно не прилетало второй раз.
 * Возвращает "MSI", "MSI-X" или NULL (не вышло - остаёмся на опросе).
 */
const char *kx_pci_enable_msi(UINT8 bus, UINT8 dev, UINT8 fn, UINT8 vector)
{
    UINT32 apic = acpi_current_apic_id();
    UINT32 addr = 0xFEE00000u | ((apic & 0xFFu) << 12);
    UINT8 cap = pci_find_cap(bus, dev, fn, 0x05);
    const char *kind;

    if (cap != 0) {

        kind = "MSI";

        UINT32 hdr = pci_config_read32(bus, dev, fn, cap);
        UINT16 ctrl = (UINT16)(hdr >> 16);
        BOOLEAN is64 = (ctrl & (1u << 7)) != 0;
        BOOLEAN maskable = (ctrl & (1u << 8)) != 0;

        pci_config_write32(bus, dev, fn, (UINT8)(cap + 4), addr);

        UINT8 data_off;

        if (is64) {
            pci_config_write32(bus, dev, fn, (UINT8)(cap + 8), 0);
            data_off = (UINT8)(cap + 12);
        } else {
            data_off = (UINT8)(cap + 8);
        }

        /* данные: вектор, доставка Fixed, по фронту */
        UINT32 d = pci_config_read32(bus, dev, fn, data_off);
        d = (d & 0xFFFF0000u) | vector;
        pci_config_write32(bus, dev, fn, data_off, d);

        /* маска векторов (если есть) - снять */
        if (maskable)
            pci_config_write32(bus, dev, fn, (UINT8)(data_off + 4), 0);

        /* один вектор (MME = 0) + включить */
        ctrl = (UINT16)((ctrl & ~(7u << 4)) | 1u);
        pci_config_write32(bus, dev, fn, cap,
                           (hdr & 0x0000FFFFu) | ((UINT32)ctrl << 16));

    } else {

        cap = pci_find_cap(bus, dev, fn, 0x11);

        if (cap == 0)
            return NULL;

        kind = "MSI-X";

        UINT32 hdr = pci_config_read32(bus, dev, fn, cap);
        UINT32 tbl = pci_config_read32(bus, dev, fn, (UINT8)(cap + 4));
        UINT8 bir = (UINT8)(tbl & 7u);
        UINT64 bar = pci_read_bar_address(bus, dev, fn, (UINT8)(0x10 + 4 * bir));

        if (bar == 0)
            return NULL;

        UINT64 entry = bar + (tbl & ~7u);

        if (!vmm_map_mmio(entry, 16, VMM_UC))
            return NULL;

        /* функция целиком замаскирована, пока настраиваем */
        UINT16 ctrl = (UINT16)(hdr >> 16);
        pci_config_write32(bus, dev, fn, cap,
                           (hdr & 0xFFFFu) | ((UINT32)(ctrl | (1u << 14) | (1u << 15)) << 16));

        mmio_write32(entry + 0, addr);
        mmio_write32(entry + 4, 0);
        mmio_write32(entry + 8, vector);
        mmio_write32(entry + 12, 0);          /* не замаскирован */

        pci_config_write32(bus, dev, fn, cap,
                           (hdr & 0xFFFFu) |
                           ((UINT32)((ctrl | (1u << 15)) & ~(1u << 14)) << 16));
    }

    /* PCI Command, бит 10 - Interrupt Disable (старое INTx) */
    UINT32 cmd = pci_config_read32(bus, dev, fn, 0x04);
    pci_config_write32(bus, dev, fn, 0x04, cmd | (1u << 10));

    return kind;
}

/* ================================================================
 * Загрузка процессора
 *
 * Когда делать нечего, ядро спит на инструкции hlt (см.
 * kx_sleep_us) - процессор останавливается до следующего
 * прерывания. Время во сне копится в g_idle_tsc; раз в секунду
 * (по таймеру) считаем, какая доля последней секунды прошла НЕ во
 * сне - это и есть загрузка.
 * ================================================================ */

volatile UINT64 g_idle_tsc = 0;
volatile UINT32 g_cpu_load_permille = 0;    /* 0..1000 за последнюю секунду */
volatile UINT32 g_cpu_load_valid = 0;
static UINT64 g_load_last_tsc = 0;
static UINT64 g_load_last_idle = 0;

/* Спать до ближайшего прерывания (только если они включены!) */
void kx_idle_hlt(void)
{
    UINT64 fl;

    __asm__ __volatile__("pushfq; popq %0" : "=r"(fl));

    if (!(fl & (1u << 9))) {           /* IF = 0: hlt был бы вечным */
        cpu_pause();
        return;
    }

    UINT64 t0 = rdtsc();
    kx_hlt();
    g_idle_tsc += rdtsc() - t0;
}

/* Из обработчика таймера, раз в 1000 тиков */
void kx_load_tick(void)
{
    UINT64 now = rdtsc();
    UINT64 idle = g_idle_tsc;

    if (g_load_last_tsc != 0 && now > g_load_last_tsc) {

        UINT64 total = now - g_load_last_tsc;
        UINT64 slept = idle - g_load_last_idle;

        if (slept > total)
            slept = total;

        g_cpu_load_permille = (UINT32)(1000u - (slept * 1000u) / total);
        g_cpu_load_valid = 1;
    }

    g_load_last_tsc = now;
    g_load_last_idle = idle;

    /* доля каждого потока - для ps */
    sched_account_load();
}

/* ================================================================
 * Команда cpu
 * ================================================================ */

void kernel_cmd_cpu(SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    smp_describe(out);

    if (g_cpu_load_valid)
        kprintf(out, "CPU load (last second, measured while this shell waited): %u.%u%%\n",
                g_cpu_load_permille / 10u, g_cpu_load_permille % 10u);
    else
        print(out, "CPU load: not measured yet (needs the 1 kHz timer)\n");

    if (g_sched_on) {
        UINTN n = 0;
        for (UINTN i = 0; i < KT_MAX; i++)
            if (g_kthreads[i].state != KT_UNUSED && g_kthreads[i].state != KT_DEAD)
                n++;
        kprintf(out, "Threads: %u, context switches: %llu (%llu by the timer) - details: 'ps'\n",
                (UINT32)n, g_sched_switches, g_sched_preempts);
    }

    print(out, "Interrupts delivered:\n");

    static const struct { UINT8 v; const char *name; } known[] = {
        { KX_VEC_TIMER,   "timer (LAPIC, 1000 Hz)" },
        { KX_VEC_PS2_KBD, "PS/2 keyboard (IRQ 1)" },
        { KX_VEC_PS2_AUX, "PS/2 mouse / touchpad (IRQ 12)" },
        { KX_VEC_XHCI,    "USB controller (xHCI)" },
    };

    for (UINTN i = 0; i < sizeof(known) / sizeof(known[0]); i++) {

        UINT64 n = (known[i].v == KX_VEC_TIMER) ? g_kticks : g_irq_count[known[i].v];
        const char *state = "";

        if (known[i].v == KX_VEC_XHCI)
            state = g_kx.irq_mode;
        else if (known[i].v != KX_VEC_TIMER && g_irq_handlers[known[i].v] == NULL)
            state = "not used";

        kprintf(out, "  vector 0x%02x  %-32s %10llu  %s\n",
                known[i].v, known[i].name, n, state);
    }

    kprintf(out, "  unexpected: %llu, spurious: %llu\n",
            (UINT64)g_kstray, (UINT64)g_kspurious);
}
