/*
 * kernel/kmain.c - точка входа ядра и порядок его запуска.
 * Часть MyOS; общие объявления - в myos.h.
 *
 * Сюда прыгает загрузчик (loader/loader.c), уже после
 * ExitBootServices: прерывания выключены, работают временные
 * таблицы страниц загрузчика, стек - 64 КиБ, выделенные
 * загрузчиком, в RDI - адрес паспорта загрузки (bootinfo.h).
 *
 * Порядок важен, и у каждого шага есть причина:
 *   1. COM1 - чтобы с самой первой строчки было куда писать лог;
 *   2. свои GDT и IDT - старые лежат в памяти прошивки, которую
 *      мы сейчас же отдадим аллокатору (а исключение с IDT,
 *      затёртой нашими же данными, - это мгновенная перезагрузка);
 *   3. карта памяти и аллокатор страниц (pmm);
 *   4. свои таблицы страниц (vmm), после чего - освободить таблицы
 *      загрузчика;
 *   5. свой стек с защитной страницей и TSS с запасными стеками;
 *   6. экран, таймеры, куча, ACPI, ввод;
 *   7. шелл.
 */
#include "myos.h"

/* Копия паспорта загрузки - в данных ядра (оригинал загрузчика
   лежит в памяти, которую можно было бы потом освободить) */
MYOS_BOOT_INFO g_boot;

/* Шелл и GUI получают таблицу функций ядра через g_st */
EFI_SYSTEM_TABLE *g_st = NULL;

/* Всегда TRUE: MyOS больше не бывает "под прошивкой". Флаг
   оставлен, потому что на него смотрят шелл и драйверы. */
BOOLEAN g_kernel_mode = TRUE;

void kmain_stage2(void) __attribute__((noreturn, used));
static void kmain_halt(const char *why) __attribute__((noreturn));

static void kmain_halt(const char *why)
{
    klog("FATAL: %s\n", why);

    if (g_kfb != NULL) {
        UINT32 bg = gui_pack(g_kfb_fmt, 120, 0, 0);
        UINT32 fg = gui_pack(g_kfb_fmt, 255, 255, 255);
        gui_fill_rect(g_kfb, g_kfb_stride, g_kfb_w, g_kfb_h, 0, 0,
                      g_kfb_w < 640 ? g_kfb_w : 640, 48, bg);
        kx_raw_text(16, 16, why, fg, bg);
    }

    for (;;) {
        kx_cli();
        kx_hlt();
    }
}

/* Точка входа. Отдельная секция .text.kmain - чтобы в kernel.elf
   она шла первой (так удобнее смотреть дизассемблером). */
__attribute__((section(".text.kmain"), noreturn))
void kmain(MYOS_BOOT_INFO *bi)
{
    kx_cli();

    /* --- 1. лог --- */
    serial_init();
    klog("MyOS kernel: entered kmain, boot info at %p\n", bi);

    /* --- паспорт --- */
    if (bi == NULL || bi->magic != MYOS_BOOT_MAGIC ||
        bi->version != MYOS_BOOT_VERSION) {
        klog("FATAL: bad boot info (loader and kernel from different builds?)\n");
        for (;;)
            kx_hlt();
    }

    {
        const UINT8 *src = (const UINT8 *)bi;
        UINT8 *dst = (UINT8 *)&g_boot;
        for (UINTN i = 0; i < sizeof(MYOS_BOOT_INFO); i++)
            dst[i] = src[i];
    }

    /* --- 2. GDT и IDT --- */
    kx_load_gdt();
    kx_load_idt();

    /* экран - уже сейчас (он отображён и в таблицах загрузчика,
       через прямое отображение), чтобы экран паники работал */
    g_kfb = (volatile UINT32 *)P2V(g_boot.fb_phys);
    g_kfb_w = g_boot.fb_width;
    g_kfb_h = g_boot.fb_height;
    g_kfb_stride = g_boot.fb_stride;
    g_kfb_fmt = (EFI_GRAPHICS_PIXEL_FORMAT)g_boot.fb_format;

    /* --- 3. память --- */
    pmm_save_map(P2V(g_boot.mmap_phys), (UINTN)g_boot.mmap_size,
                 (UINTN)g_boot.mmap_desc_size);

    if (!pmm_init())
        kmain_halt("page allocator failed: no free memory in the map?");

    klog("pmm: %llu MiB free\n", (g_kmm_free_pages * 4u) / 1024u);

    /* --- 4. свои таблицы страниц --- */
    if (!vmm_init())
        kmain_halt("could not build kernel page tables");

    pmm_reclaim_type(MYOS_MEM_LOADER_TEMP);

    klog("vmm: own page tables on, CR3=0x%llx\n", g_vmm_pml4_phys);

    /* --- 5. свой стек (256 КиБ, с защитной страницей) --- */
    UINT64 top = vmm_alloc_stack(64, "main kernel stack");

    if (top == 0)
        kmain_halt("could not allocate the kernel stack");

    /* переход на новый стек: дальше - kmain_stage2, назад дороги
       нет (старый стек загрузчика больше не нужен) */
    __asm__ __volatile__(
        "mov %0, %%rsp\n\t"
        "xor %%rbp, %%rbp\n\t"
        "call kmain_stage2\n\t"
        :
        : "r"(top)
        : "memory"
    );

    for (;;)
        kx_hlt();
}

static void kmain_section(SIMPLE_TEXT_OUTPUT_INTERFACE *out, const char *name)
{
    set_color(out, 0x0E);
    print(out, name);
    print(out, "\n");
    set_color(out, 0x07);
}

void kmain_stage2(void)
{
    /* --- 5б. TSS: запасные стеки для Double Fault, NMI, Machine
       Check (по 16 КиБ, тоже с защитными страницами) --- */
    UINT64 ist_df = vmm_alloc_stack(4, "double fault stack (IST1)");
    UINT64 ist_nmi = vmm_alloc_stack(4, "NMI stack (IST2)");
    UINT64 ist_mc = vmm_alloc_stack(4, "machine check stack (IST3)");

    kx_load_tss(ist_df, ist_nmi, ist_mc, 0);

    /* --- 6. консоль --- */
    kcon_init();

    SIMPLE_TEXT_OUTPUT_INTERFACE *out = &g_kcon_out;

    set_color(out, 0x0B);
    print(out, "MyOS kernel - started by the MyOS loader\n");
    set_color(out, 0x07);

    kmain_section(out, "[boot]");
    kprintf(out, "  kernel.elf: %S, %llu KiB, at phys 0x%llx -> virt 0x%llx\n",
            g_boot.kernel_path, g_boot.kernel_file_size / 1024u,
            g_boot.kernel_phys, g_boot.kernel_virt);
    kprintf(out, "  firmware: %S (UEFI %u.%u) - gone after the loader\n",
            g_boot.fw_vendor, g_boot.uefi_revision >> 16,
            (g_boot.uefi_revision & 0xFFFFu) / 10u);
    kprintf(out, "  screen: %ux%u, framebuffer at 0x%llx\n",
            g_boot.fb_width, g_boot.fb_height, g_boot.fb_phys);

    kmain_section(out, "[cpu]");
    print(out, "  GDT loaded (code 0x08, data 0x10, TSS 0x18)\n");
    print(out, "  IDT loaded: 256 vectors, exceptions -> panic screen\n");
    kprintf(out, "  TSS: separate stacks for Double Fault, NMI, Machine Check\n");

    kx_pic_disable();
    print(out, "  8259 PIC remapped to 0x20-0x2F and masked\n");

    /* --- ACPI: что за компьютер. Нужно раньше таймеров (HPET,
       таймер PM) и контроллеров прерываний (адреса I/O APIC). --- */
    kmain_section(out, "[acpi]");

    if (acpi_init()) {

        UINT32 ntab = (UINT32)g_acpi.ntables;
        UINT32 bad = 0;

        for (UINTN i = 0; i < g_acpi.ntables; i++)
            if (!g_acpi.tables[i].sum_ok)
                bad++;

        kprintf(out, "  ACPI %s by \"%s\": %u tables%s\n",
                g_acpi.revision >= 2 ? "2.0+" : "1.0", g_acpi.oem, ntab,
                bad ? " (some with BAD checksums - ignored)" : ", all checksums OK");

        if (g_acpi.have_madt)
            kprintf(out, "  CPU cores: %u (MADT); MyOS uses one of them for now\n",
                    (UINT32)g_acpi.ncpus_enabled);
    } else {
        kprintf(out, "  no usable ACPI tables: %s\n", g_acpi.why);
    }

    /* I/O APIC: все из MADT; без MADT - стандартный адрес */
    if (g_acpi.nioapics > 0) {

        for (UINTN i = 0; i < g_acpi.nioapics; i++) {
            UINTN n = kx_ioapic_mask_all(g_acpi.ioapics[i].addr);
            g_acpi.ioapics[i].count = (UINT32)n;
            kprintf(out, "  I/O APIC %u at 0x%llx (from MADT): %u lines, all masked\n",
                    g_acpi.ioapics[i].id, g_acpi.ioapics[i].addr, (UINT32)n);
        }

    } else {

        UINTN n = kx_ioapic_mask_all(0xFEC00000ull);

        if (n > 0)
            kprintf(out, "  I/O APIC at 0xFEC00000 (guessed, no MADT): %u lines masked\n",
                    (UINT32)n);
        else
            print(out, "  I/O APIC: not found (skipped)\n");
    }

    /* PCIe через память */
    if (g_acpi.n_mcfg > 0) {
        if (pci_use_ecam(g_acpi.ecam_base, g_acpi.ecam_bus_start, g_acpi.ecam_bus_end))
            kprintf(out, "  PCIe config space via ECAM at 0x%llx (MCFG), buses %u..%u\n",
                    g_acpi.ecam_base, g_acpi.ecam_bus_start, g_acpi.ecam_bus_end);
        else
            print(out, "  MCFG present, but ECAM check failed - PCI via ports 0xCF8/0xCFC\n");
    } else {
        print(out, "  no MCFG - PCI config via ports 0xCF8/0xCFC\n");
    }

    acpi_power_init();

    kprintf(out, "  shutdown: %s\n", g_acpi_power.ok ? "ACPI S5 ready" : g_acpi_power.why);

    kmain_section(out, "[memory]");
    kprintf(out, "  page tables: kernel at 0xFFFFFFFF80000000, all RAM at 0xFFFF800000000000\n");
    kprintf(out, "  kernel code read-only, data no-execute%s, framebuffer %s\n",
            g_vmm_nx ? "" : " (CPU has no NX!)",
            g_vmm_pat ? "write-combining" : "uncached (no PAT)");
    kprintf(out, "  lower half unmapped: NULL pointer = clear Page Fault\n");
    kprintf(out, "  page allocator: %llu MiB free of %llu MiB, %llu MiB taken back from\n"
                 "    the firmware and the loader\n",
            (g_kmm_free_pages * 4u) / 1024u, (g_kmm_usable_pages * 4u) / 1024u,
            (g_kmm_reclaimed_pages * 4u) / 1024u);

    {
        char rep[96];
        BOOLEAN ok = kmalloc_selftest(rep, sizeof(rep));
        kprintf(out, "  kmalloc: %s\n", rep);
        (void)ok;
    }

    /* --- таймеры --- */
    kmain_section(out, "[time]");

    /*
     * Частота TSC - по нескольким независимым эталонам:
     *   HPET        - высокоточный таймер из ACPI (обычно 14-25 МГц);
     *   PIT 8254    - старый таймер, 1.193182 МГц;
     *   ACPI PM     - таймер управления питанием, 3.579545 МГц;
     *   Stall       - замер загрузчика через прошивку (контроль).
     * Берём первый по точности, который согласуется с контролем
     * (расхождение меньше 25%). Раньше эталон был один - PIT, и
     * если прошивка его выключила, оставалось только гадать.
     */
    g_tsc_hz_stall = g_boot.tsc_hz_stall;
    g_tsc_hz_hpet = acpi_hpet_measure_tsc_hz();
    g_tsc_hz_pit = kx_pit_measure_tsc_hz();
    g_tsc_hz_pmtmr = acpi_pmtimer_measure_tsc_hz();

    {
        const char *names[3] = { "HPET", "PIT 8254", "ACPI PM timer" };
        UINT64 vals[3] = { g_tsc_hz_hpet, g_tsc_hz_pit, g_tsc_hz_pmtmr };

        for (UINTN i = 0; i < 3; i++) {
            if (vals[i] >= 1000000ull)
                kprintf(out, "  TSC by %-14s %llu.%03llu MHz\n", names[i],
                        vals[i] / 1000000u, (vals[i] / 1000u) % 1000u);
            else
                kprintf(out, "  TSC by %-14s - (not available)\n", names[i]);
        }

        kprintf(out, "  TSC by firmware Stall  %llu MHz (measured by the loader)\n",
                g_tsc_hz_stall / 1000000u);

        g_tsc_hz = 0;

        for (UINTN i = 0; i < 3 && g_tsc_hz == 0; i++) {

            UINT64 a = vals[i];
            UINT64 bref = g_tsc_hz_stall;

            if (a < 100000000ull || a > 20000000000ull)
                continue;

            if (bref == 0 || (a * 4u > bref * 3u && a * 3u < bref * 4u)) {
                g_tsc_hz = a;
                g_tsc_source = names[i];
            }
        }

        if (g_tsc_hz == 0 && g_tsc_hz_stall >= 100000000ull) {
            g_tsc_hz = g_tsc_hz_stall;
            g_tsc_source = "firmware Stall (no hardware timer agreed)";
        } else if (g_tsc_hz == 0) {
            g_tsc_hz = 2000000000ull;
            g_tsc_source = "GUESS 2 GHz (no reference worked!)";
        }
    }

    g_kboot_tsc = rdtsc();

    kprintf(out, "  Using TSC = %llu MHz, source: %s\n",
            g_tsc_hz / 1000000u, g_tsc_source);

    if (kx_lapic_timer_start(out)) {

        kx_sti();

        UINT64 t_before = g_kticks;

        tsc_delay_us(50000);

        UINT64 got = g_kticks - t_before;

        g_ktimer_ok = (got >= 20u);

        kprintf(out, "  Interrupts ON. Timer ticks in 50 ms: %llu%s\n", got,
                g_ktimer_ok ? " (1 kHz tick is alive)" :
                              " - TIMER NOT FIRING, using TSC only");
    }

    /* часы: время загрузки - от загрузчика, сейчас - из CMOS */
    if (g_boot.have_boot_time) {
        g_boot_time = g_boot.boot_time;
        g_have_boot_time = TRUE;
    }

    {
        EFI_TIME now;
        if (rtc_read(&now))
            kprintf(out, "  CMOS clock: %04u-%02u-%02u %02u:%02u:%02u\n",
                    now.Year, now.Month, now.Day, now.Hour, now.Minute, now.Second);
        else
            print(out, "  CMOS clock: not responding\n");
    }

    /* --- ввод --- */
    kmain_section(out, "[input]");

    ps2_init(out);

    {
        UINT8 xb = 0, xd = 0, xf = 0;
        UINT64 xm = 0;

        g_kx.present = FALSE;

        if (pci_find_xhci(&xb, &xd, &xf, &xm) && xm != 0) {

            /* регистры контроллера - некэшируемыми (ниже 4 ГиБ
               это уже так, но BAR может лежать и выше) */
            vmm_map_mmio(xm, 0x20000u, VMM_UC);

            pci_enable_device(xb, xd, xf);
            xhci_read_cap_regs(xm, &g_kx.cap);

            UINT64 need = g_kx.cap.DbOff + 4u * 256u;
            if (g_kx.cap.RtsOff + 0x20u + 32u * 1024u > need)
                need = g_kx.cap.RtsOff + 0x20u + 32u * 1024u;
            if (need > 0x20000u && need < 0x1000000u)
                vmm_map_mmio(xm, need, VMM_UC);

            if (g_kx.cap.CapLength != 0 || g_kx.cap.HciVersion != 0) {
                g_kx.present = TRUE;
                g_kx.bus = xb;
                g_kx.devn = xd;
                g_kx.func = xf;
                g_kx.mmio = xm;
            }

            if (!g_boot.xhci_handoff_ok)
                print(out, "  (note: the BIOS did not confirm the USB handoff)\n");
        }
    }

    kx_usb_start(out);

    /* --- таблица функций ядра для шелла и GUI --- */
    kx_install_shims();

    g_st = &g_kst;

    /* прокрутка истории (PageUp) - на всю высоту нашей консоли */
    if (g_kcon_rows > 2)
        g_scrollback_visible_rows = g_kcon_rows - 1;

    klog("kernel ready, starting the shell\n");

    /* --- 7. шелл --- */
    set_color(out, 0x0A);
    print(out, "\nMyOS is running on its own kernel - no firmware underneath.\n");
    set_color(out, 0x07);
    print(out, "Type 'help' for the list of commands, or 'fetch' for a system summary.\n");
    print(out, "New: acpi (what the ACPI tables say), boot, vm, crash null|write|stack.\n\n");

    CHAR16 line[LINE_MAX];

    for (;;) {

        out = g_st->ConOut;

        set_color(out, g_color);
        print(out, "> ");

        read_line(g_st, line, LINE_MAX);
        run_command(g_st, line);
    }
}
