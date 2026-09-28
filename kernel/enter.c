/*
 * kernel/enter.c - команда ebs: выход из прошивки и запуск всего kernel mode.
 * Часть MyOS; общие объявления - в myos.h.
 */
#include "myos.h"

/*
 * TRUE после команды "ebs": прошивки больше нет, ОС работает на
 * собственных драйверах (см. большой блок KERNEL MODE ближе к
 * концу файла). Объявлено здесь, в начале, потому что на него
 * смотрят и более ранние команды (например, fetch).
 */
BOOLEAN g_kernel_mode = FALSE;

/* g_kernel_mode (TRUE после "ebs") объявлен в начале файла */

/*
 * Системная таблица, которой пользуется главный цикл шелла в
 * efi_main. До "ebs" - настоящая таблица прошивки, после - наша
 * g_kst (см. конец блока).
 */
EFI_SYSTEM_TABLE *g_st = NULL;


/* ================================================================
 * 8. Сам переход: команда "ebs"
 * ================================================================ */

void kernel_ebs_and_enter(EFI_SYSTEM_TABLE *st)
{
    SIMPLE_TEXT_OUTPUT_INTERFACE *out = st->ConOut;

    if (g_kernel_mode) {
        print(out, "Already running without Boot Services.\n");
        return;
    }

    print(out, "=== Leaving the firmware for good ===\n");
    print(out, "ExitBootServices will be called, then MyOS brings up its\n");
    print(out, "own console, interrupts, timer, memory manager and USB/PS2\n");
    print(out, "input, and returns to this same shell - without firmware.\n\n");

    /* --- 1. экран --- */
    EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;

    if (
        !st->BootServices->LocateProtocol ||
        st->BootServices->LocateProtocol(
            &gop_guid, NULL, (VOID **)&gop
        ) != EFI_SUCCESS ||
        !gop || !gop->Mode || !gop->Mode->Info ||
        gop->Mode->FrameBufferBase == 0
    ) {
        print(out, "No usable Graphics Output Protocol framebuffer - aborting\n");
        print(out, "(nothing to draw the console on after the exit).\n");
        return;
    }

    if (gop->Mode->Info->PixelFormat == PixelBltOnly) {
        print(out, "GOP is Blt-only (no direct framebuffer) - aborting.\n");
        return;
    }

    g_kfb = (volatile UINT32 *)(UINTN)gop->Mode->FrameBufferBase;
    g_kfb_w = gop->Mode->Info->HorizontalResolution;
    g_kfb_h = gop->Mode->Info->VerticalResolution;
    g_kfb_stride = gop->Mode->Info->PixelsPerScanLine;
    g_kfb_fmt = gop->Mode->Info->PixelFormat;

    print(out, "Framebuffer: ");
    print_uint(out, g_kfb_w);
    print(out, "x");
    print_uint(out, g_kfb_h);
    print(out, " at 0x");
    print_hex(out, (UINT64)(UINTN)g_kfb, 16);
    print(out, "\n");

    /* --- 2. xHCI: то, что требует Boot Services, - сейчас --- */
    UINT8 xb = 0, xd = 0, xf = 0;
    UINT64 xm = 0;

    g_kx.present = FALSE;

    if (pci_find_xhci(&xb, &xd, &xf, &xm) && xm != 0) {

        print(out, "xHCI found - asking firmware to release it...\n");

        pci_enable_device(xb, xd, xf);
        xhci_disconnect_firmware_driver(st, out, xb, xd, xf);
        xhci_read_cap_regs(xm, &g_kx.cap);
        xhci_bios_handoff(st, xm, g_kx.cap.ExtCapOff, out);

        if (g_kx.cap.CapLength != 0 || g_kx.cap.HciVersion != 0) {

            g_kx.present = TRUE;
            g_kx.bus = xb;
            g_kx.devn = xd;
            g_kx.func = xf;
            g_kx.mmio = xm;
        }

    } else {

        print(out, "No xHCI controller on the PCI bus.\n");
    }

    /* --- 3. TSC по Stall прошивки (пока он есть) - как
       контрольный замер для калибровки по PIT ниже --- */
    {
        UINT64 t0 = rdtsc();
        st->BootServices->Stall(50000);
        UINT64 t1 = rdtsc();

        g_tsc_hz_stall = (t1 - t0) * 20u;
    }

    print(out, "\nStarting in 2 seconds...\n");
    st->BootServices->Stall(2000000);

    /* --- 4. карта памяти + ExitBootServices --- */
    GUI_GET_MEMORY_MAP get_map =
        (GUI_GET_MEMORY_MAP)st->BootServices->GetMemoryMap;
    GUI_ALLOCATE_POOL alloc_pool =
        (GUI_ALLOCATE_POOL)st->BootServices->AllocatePool;
    GUI_EXIT_BOOT_SERVICES exit_bs =
        (GUI_EXIT_BOOT_SERVICES)st->BootServices->ExitBootServices;

    UINTN map_size = 0;
    UINTN map_key = 0;
    UINTN desc_size = 0;
    UINT32 desc_ver = 0;

    get_map(&map_size, NULL, &map_key, &desc_size, &desc_ver);

    map_size += desc_size * 16u;

    VOID *map_buf = NULL;

    if (alloc_pool(GUI_EFI_BOOT_SERVICES_DATA, map_size, &map_buf) != EFI_SUCCESS) {
        print(out, "Could not allocate the memory map buffer - aborting.\n");
        return;
    }

    BOOLEAN exited = FALSE;
    UINTN final_size = 0;

    for (UINTN attempt = 0; attempt < 8; attempt++) {

        UINTN this_size = map_size;

        if (get_map(&this_size, map_buf, &map_key, &desc_size, &desc_ver)
                != EFI_SUCCESS)
            break;

        if (exit_bs(g_image_handle, map_key) == EFI_SUCCESS) {
            exited = TRUE;
            final_size = this_size;
            break;
        }
    }

    if (!exited) {
        print(out, "ExitBootServices failed - still in firmware mode, the\n");
        print(out, "shell keeps working as before.\n");
        return;
    }

    /*
     * ======== Прошивки больше нет. ========
     * С этой строки: никаких st->ConOut/ConIn/BootServices.
     * Первым делом - сохранить карту памяти (буфер лежит в
     * памяти, которую прошивка больше не охраняет) и поднять
     * консоль, чтобы было куда писать.
     */
    kx_cli();

    pmm_save_map(map_buf, final_size, desc_size);

    kcon_init();

    out = &g_kcon_out;

    /* отладочный лог в COM1 (в QEMU: -serial stdio) */
    serial_init();
    klog("MyOS kernel mode: ExitBootServices done, console up\n");

    set_color(out, 0x0B);
    print(out, "MyOS kernel mode - ExitBootServices: OK\n");
    set_color(out, 0x07);
    print(out, "Everything below runs on MyOS's own code, no firmware.\n\n");

    /* --- 5. CPU --- */
    set_color(out, 0x0E);
    print(out, "[cpu]\n");
    set_color(out, 0x07);

    kx_load_gdt();
    print(out, "  GDT loaded (code 0x08, data 0x10)\n");

    kx_load_idt();
    print(out, "  IDT loaded: 256 vectors, exceptions -> panic screen\n");

    kx_pic_disable();
    print(out, "  8259 PIC remapped to 0x20-0x2F and masked\n");

    UINTN ioapic_n = kx_ioapic_mask_all();

    if (ioapic_n > 0) {
        print(out, "  I/O APIC: ");
        print_uint(out, ioapic_n);
        print(out, " redirection entries masked\n");
    } else {
        print(out, "  I/O APIC: not found at 0xFEC00000 (skipped)\n");
    }

    /* --- 6. время --- */
    set_color(out, 0x0E);
    print(out, "[time]\n");
    set_color(out, 0x07);

    g_tsc_hz_pit = kx_pit_measure_tsc_hz();

    print(out, "  TSC by PIT (8254):      ");
    if (g_tsc_hz_pit >= 1000000ull) {
        print_uint(out, g_tsc_hz_pit / 1000000u);
        print(out, " MHz\n");
    } else {
        print(out, "PIT does not respond\n");
    }

    print(out, "  TSC by firmware Stall:  ");
    print_uint(out, g_tsc_hz_stall / 1000000u);
    print(out, " MHz (measured before the exit)\n");

    /* PIT - основной, firmware-независимый эталон. Если он
       молчит или явно расходится с контрольным замером - берём
       контрольный. */
    BOOLEAN pit_ok = FALSE;

    if (g_tsc_hz_pit >= 100000000ull && g_tsc_hz_pit <= 20000000000ull) {

        UINT64 a = g_tsc_hz_pit;
        UINT64 bref = g_tsc_hz_stall;

        if (bref == 0 || (a * 4u > bref * 3u && a * 3u < bref * 4u))
            pit_ok = TRUE;
    }

    if (pit_ok) {
        g_tsc_hz = g_tsc_hz_pit;
        g_tsc_source = "PIT 8254";
    } else if (g_tsc_hz_stall >= 100000000ull) {
        g_tsc_hz = g_tsc_hz_stall;
        g_tsc_source = "firmware Stall (PIT unusable)";
    } else {
        g_tsc_hz = 2000000000ull;
        g_tsc_source = "GUESS 2 GHz (no reference worked!)";
    }

    g_kboot_tsc = rdtsc();

    print(out, "  Using TSC = ");
    print_uint(out, g_tsc_hz / 1000000u);
    print(out, " MHz, source: ");
    print(out, g_tsc_source);
    print(out, "\n");

    if (kx_lapic_timer_start(out)) {

        kx_sti();

        UINT64 t_before = g_kticks;

        tsc_delay_us(50000);

        UINT64 got = g_kticks - t_before;

        g_ktimer_ok = (got >= 20u);

        print(out, "  Interrupts ON. Timer ticks in 50 ms: ");
        print_uint(out, got);
        print(out, g_ktimer_ok ? " (1 kHz tick is alive)\n" :
                                 " - TIMER NOT FIRING, using TSC only\n");
    }

    /* --- 7. память --- */
    set_color(out, 0x0E);
    print(out, "[memory]\n");
    set_color(out, 0x07);

    print(out, "  Final memory map: ");
    print_uint(out, g_kmm_map_count);
    print(out, " regions");

    if (g_kmm_map_dropped) {
        print(out, " (");
        print_uint(out, g_kmm_map_dropped);
        print(out, " dropped - table full)");
    }

    print(out, "\n");

    if (pmm_init()) {

        print(out, "  Page allocator: ");
        print_uint(out, (g_kmm_free_pages * 4u) / 1024u);
        print(out, " MiB free of ");
        print_uint(out, (g_kmm_usable_pages * 4u) / 1024u);
        print(out, " MiB usable, bitmap ");
        print_uint(out, g_kmm_bitmap_pages);
        print(out, " page(s) at 0x");
        print_hex(out, g_kmm_bitmap_phys, 8);
        print(out, "\n");

    } else {

        set_color(out, 0x0C);
        print(out, "  Page allocator FAILED to initialize - no free memory?\n");
        set_color(out, 0x07);
    }

    /* --- 8. ввод --- */
    set_color(out, 0x0E);
    print(out, "[input]\n");
    set_color(out, 0x07);

    ps2_init(out);

    if (g_kmm_ready)
        kx_usb_start(out);
    else
        print(out, "  USB skipped (no memory allocator)\n");

    /* --- 9. подмена системной таблицы и возврат в шелл --- */
    kx_install_shims(st);

    g_st = &g_kst;
    g_kernel_mode = TRUE;

    /* прокрутка истории (PageUp) - на всю высоту нашей консоли */
    if (g_kcon_rows > 2)
        g_scrollback_visible_rows = g_kcon_rows - 1;

    set_color(out, 0x0A);
    print(out, "\nDone. Back to the shell - now on MyOS drivers only.\n");
    set_color(out, 0x07);
    print(out, "Try: kinfo, usb, mem, start (GUI with the USB mouse), int3.\n\n");
}
