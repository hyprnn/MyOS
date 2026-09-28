/*
 * kernel/kcmds.c - команды kinfo, usb, mousetest, mem.
 * Часть MyOS; общие объявления - в myos.h.
 */
#include "myos.h"


/* ================================================================
 * 9. Команды: kinfo, usb, mem, int3
 * ================================================================ */

void kernel_cmd_kinfo(SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    /* Пример kprintf (lib/kprintf.c): одна строка формата вместо
       цепочки print + print_uint + print_hex. Числа 64-битные -
       поэтому %llu / %llx. */
    UINT64 us = kx_uptime_us();

    kprintf(out, "Mode: MyOS kernel (started by the MyOS loader, no firmware)\n");
    kprintf(out, "Uptime: %llu.%03llu s\n",
            us / 1000000u, (us / 1000u) % 1000u);
    kprintf(out, "TSC: %llu MHz (%s)\n", g_tsc_hz / 1000000u, g_tsc_source);
    kprintf(out, "LAPIC timer: %s%llu ticks\n",
            g_ktimer_ok ? "running, 1000 Hz, " : "NOT running, ",
            (UINT64)g_kticks);

    kprintf(out, "Interrupts: spurious=%llu unexpected=%llu",
            (UINT64)g_kspurious, (UINT64)g_kstray);

    if (g_kstray)
        kprintf(out, " (last vector 0x%02llx)", (UINT64)g_kstray_last);

    kprintf(out, " breakpoints=%llu\n", (UINT64)g_kbreakpoints);

    kprintf(out, "Memory: %llu MiB free, heap blocks in use: %llu\n",
            (g_kmm_free_pages * 4u) / 1024u, g_kheap_live);

    kprintf(out, "Keyboard: %llu keys, PS/2 ", g_kbd_keys_total);

    if (g_ps2_present)
        kprintf(out, "present (%llu bytes)", g_ps2_bytes);
    else
        kprintf(out, "absent");

    kprintf(out, ", CapsLock %s\n", g_kbd_caps ? "ON" : "off");

    kprintf(out, "Mouse: %s%llu reports\n",
            g_kmouse_present ? "present, " : "none, ", g_kmouse_reports);

    kprintf(out, "Input by interrupts: PS/2 keyboard %s, PS/2 mouse %s, USB %s\n",
            g_ps2_irq ? "IRQ 1" : (g_ps2_present ? "polling" : "absent"),
            g_ps2_aux_present ? "IRQ 12" : "absent",
            g_kx.running ? g_kx.irq_mode : "off");

    if (g_cpu_load_valid)
        kprintf(out, "CPU load: %u.%u%% over the last second (see 'cpu')\n",
                g_cpu_load_permille / 10u, g_cpu_load_permille % 10u);

    kprintf(out, "Threads: %s\n",
            g_sched_on ? "on - preemptive round-robin, 10 ms quantum (see 'ps')"
                       : "off (no timer) - one flow of execution");

    kprintf(out, "Console: %llux%llu chars on %ux%u framebuffer\n",
            (UINT64)g_kcon_cols, (UINT64)g_kcon_rows, g_kfb_w, g_kfb_h);

    kprintf(out, "Serial log (COM1): %s\n",
            g_serial_ok ? "on, 115200 8N1" : "no COM port");
}


/* Состояние конечной точки ГЛАЗАМИ КОНТРОЛЛЕРА - поле EP State
   в выходном Device Context (контроллер сам его обновляет) */
const char *kx_ep_hw_state(KX_HID *h)
{
    KX_DEV *d = &g_kx_devs[h->dev];

    if (d->dev_ctx == 0)
        return "?";

    volatile UINT32 *ep =
        (volatile UINT32 *)P2V(d->dev_ctx + (UINT64)h->dci * g_kx.ctx_size);

    switch (ep[0] & 0x7u) {
    case 0:  return "Disabled";
    case 1:  return "Running";
    case 2:  return "HALTED";
    case 3:  return "Stopped";
    case 4:  return "ERROR";
    default: return "?";
    }
}

const char *kx_sw_state(UINT8 st)
{
    switch (st) {
    case KX_EP_RUN:    return "run";
    case KX_EP_RESET:  return "recovering(reset)";
    case KX_EP_SETDEQ: return "recovering(set-deq)";
    case KX_EP_DEAD:   return "DEAD";
    default:           return "?";
    }
}

/* Строка про контроллер целиком: USBSTS - HCHalted (бит 0), Host
   System Error (бит 2), Host Controller Error (бит 12). HSE/HCE
   означают, что контроллер сам встал из-за внутренней ошибки -
   тогда не работает вообще ничего на USB. */
void kx_print_hc_status(SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    UINT32 sts = mmio_read32(g_kx.op + 0x04);

    print(out, "xHCI USBSTS=0x");
    print_hex(out, sts, 8);

    if (sts & 0x1u)    print(out, " HALTED");
    if (sts & 0x4u)    print(out, " HOST-SYSTEM-ERROR");
    if (sts & 0x1000u) print(out, " HOST-CONTROLLER-ERROR");
    if (!(sts & 0x1005u)) print(out, " (ok)");

    print(out, "  events ");
    print_uint(out, g_kx.events);
    print(out, "  unmatched ");
    print_uint(out, g_kx.stray_events);
    print(out, "  port-changes ");
    print_uint(out, g_kx.port_events);
    print(out, "\n");
}

void kx_print_hid_line(SIMPLE_TEXT_OUTPUT_INTERFACE *out, KX_HID *h)
{
    print(out, "    if ");
    print_uint(out, h->iface);
    print(out, ": ");
    print(out, kx_role_name(h->role));

    if (h->mode_note != NULL && h->mode_note[0] != '\0') {
        print(out, " - ");
        print(out, h->mode_note);
    }
    print(out, "\n      reports ");
    print_uint(out, h->reports);

    if (h->role != KX_ROLE_KBD_BOOT) {
        print(out, " (ignored ");
        print_uint(out, h->rejected);
        print(out, ")");
    }

    print(out, "  errors ");
    print_uint(out, h->errors);
    print(out, " (streak ");
    print_uint(out, h->err_streak);
    print(out, ", last cc=");
    print_uint(out, h->last_err);
    print(out, ")  recoveries ");
    print_uint(out, h->recoveries);
    print(out, " (last cmd cc=");
    print_uint(out, h->last_cmd_cc);
    print(out, ")\n      driver state: ");
    print(out, kx_sw_state(h->state));
    print(out, "   controller EP state: ");
    print(out, kx_ep_hw_state(h));
    print(out, "   EP 0x");
    print_hex(out, h->ep_addr, 2);
    print(out, " maxpkt ");
    print_uint(out, h->maxpkt);
    print(out, " bInterval ");
    print_uint(out, h->interval_raw);
    print(out, "\n      last report (");
    print_uint(out, h->last_len);
    print(out, " bytes):");

    UINTN n = h->last_len;

    if (n > 16)
        n = 16;

    for (UINTN k = 0; k < n; k++) {
        print(out, " ");
        print_hex(out, h->last_rep[k], 2);
    }

    print(out, "\n");

    if (h->role == KX_ROLE_MOUSE_RPT) {

        HID_MOUSE_REPORT_LAYOUT *L = &h->layout;

        print(out, "      layout: ");

        if (L->has_report_id) {
            print(out, "ID=");
            print_uint(out, L->report_id);
            print(out, " ");
        }

        print(out, "X@");
        print_uint(out, L->x_bit_offset);
        print(out, "/");
        print_uint(out, L->x_bit_size);
        print(out, L->x_is_relative ? "rel" : "ABS");
        print(out, " Y@");
        print_uint(out, L->y_bit_offset);
        print(out, "/");
        print_uint(out, L->y_bit_size);
        print(out, L->y_is_relative ? "rel" : "ABS");

        if (L->has_buttons) {
            print(out, " btn@");
            print_uint(out, L->button_bit_offset);
            print(out, "x");
            print_uint(out, L->button_count);
        }

        if (L->has_wheel) {
            print(out, " wheel@");
            print_uint(out, L->wheel_bit_offset);
        }

        print(out, "\n");
    }
}


/*
 * mousetest - живая диагностика мыши (и вообще USB-ввода):
 * экран обновляется 4 раза в секунду, двигай мышь и смотри, что
 * происходит. Выход - любая клавиша. Сделано специально для
 * отладки на реальном железе по одному скриншоту: видно, идут ли
 * отчёты, есть ли ошибки, в каком состоянии конечная точка у
 * драйвера и у самого контроллера, и что лежит в последнем отчёте
 * байт в байт.
 */
void kernel_cmd_mousetest(SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    if (!g_kernel_mode) {
        print(out, "Only in kernel mode (after 'ebs').\n");
        return;
    }

    INT64 px = (INT64)g_kfb_w / 2;
    INT64 py = (INT64)g_kfb_h / 2;
    UINT64 next = 0;

    g_kmouse_dx = 0;
    g_kmouse_dy = 0;
    g_kmouse_dz = 0;

    /* выкинуть уже накопленные клавиши (в т.ч. Enter от самой
       команды) */
    {
        EFI_INPUT_KEY k;
        kernel_poll_input();
        while (kbd_dequeue(&k)) { }
    }

    /* Курсор поверх текста: где нарисован сейчас (-1 = нигде) */
    INTN drawn_x = -1;
    INTN drawn_y = -1;

    for (;;) {

        kernel_poll_input();

        px += g_kmouse_dx;
        py += g_kmouse_dy;
        g_kmouse_dx = 0;
        g_kmouse_dy = 0;

        if (px < 0) px = 0;
        if (py < 0) py = 0;
        if (px > (INT64)g_kfb_w - GUI_CURSOR_SIZE)
            px = (INT64)g_kfb_w - GUI_CURSOR_SIZE;
        if (py > (INT64)g_kfb_h - GUI_CURSOR_SIZE)
            py = (INT64)g_kfb_h - GUI_CURSOR_SIZE;

        EFI_INPUT_KEY k;

        if (kbd_dequeue(&k))
            break;

        UINT64 now = rdtsc();

        if (now < next) {

            /* Курсор двигаем сразу, не дожидаясь обновления
               текста: стираем старый (перерисовав клетки
               консоли под ним) и рисуем новый */
            if ((INTN)px != drawn_x || (INTN)py != drawn_y) {

                if (drawn_x >= 0) {

                    for (INTN yy = drawn_y; yy < drawn_y + GUI_CURSOR_SIZE; yy += 4) {
                        for (INTN xx = drawn_x; xx < drawn_x + GUI_CURSOR_SIZE; xx += 4) {

                            if (xx < (INTN)g_kcon_x0 || yy < (INTN)g_kcon_y0)
                                continue;

                            UINTN c = ((UINTN)xx - g_kcon_x0) / (8u * g_kcon_scale);
                            UINTN r = ((UINTN)yy - g_kcon_y0) / (16u * g_kcon_scale);

                            kcon_draw_cell(r, c, FALSE);
                        }
                    }

                    /* правый/нижний край курсора */
                    for (INTN yy = drawn_y; yy < drawn_y + GUI_CURSOR_SIZE; yy += 4) {
                        INTN xx = drawn_x + GUI_CURSOR_SIZE - 1;
                        if (xx >= (INTN)g_kcon_x0 && yy >= (INTN)g_kcon_y0)
                            kcon_draw_cell(((UINTN)yy - g_kcon_y0) / (16u * g_kcon_scale),
                                           ((UINTN)xx - g_kcon_x0) / (8u * g_kcon_scale), FALSE);
                    }
                    for (INTN xx = drawn_x; xx < drawn_x + GUI_CURSOR_SIZE; xx += 4) {
                        INTN yy = drawn_y + GUI_CURSOR_SIZE - 1;
                        if (xx >= (INTN)g_kcon_x0 && yy >= (INTN)g_kcon_y0)
                            kcon_draw_cell(((UINTN)yy - g_kcon_y0) / (16u * g_kcon_scale),
                                           ((UINTN)xx - g_kcon_x0) / (8u * g_kcon_scale), FALSE);
                    }
                }

                gui_draw_cursor_at(
                    g_kfb, g_kfb_stride, g_kfb_w, g_kfb_h, g_kfb_fmt,
                    (INTN)px, (INTN)py
                );

                drawn_x = (INTN)px;
                drawn_y = (INTN)py;
            }

            cpu_pause();
            continue;
        }

        next = now + (g_tsc_hz / 1000u) * 250u;

        kcon_clear_screen(out);

        print(out, "MOUSETEST - move the mouse, press any key to exit.\n");
        print(out, "Take a screenshot if the mouse stops.\n\n");

        print(out, "position x=");
        print_int(out, (INT32)px);
        print(out, " y=");
        print_int(out, (INT32)py);
        print(out, "  buttons=0x");
        print_hex(out, g_kmouse_buttons, 2);
        print(out, "  mouse reports total ");
        print_uint(out, g_kmouse_reports);
        print(out, "  uptime ");
        print_uint(out, kx_uptime_us() / 1000000u);
        print(out, " s\n\n");

        if (!g_kx.running) {
            print(out, "USB driver is not running.\n");
        } else {
            kx_print_hc_status(out);

            for (UINTN i = 0; i < KX_MAX_DEVS; i++) {

                KX_DEV *d = &g_kx_devs[i];

                if (!d->used)
                    continue;

                char path[32];
                kx_dev_path(d, path, sizeof(path));
                print(out, "Port ");
                print(out, path);
                print(out, " ");
                print_hex(out, d->vid, 4);
                print(out, ":");
                print_hex(out, d->pid, 4);
                print(out, " - ");
                print(out, d->status ? d->status : "?");
                print(out, "\n");

                for (UINTN j = 0; j < KX_MAX_HID; j++) {

                    KX_HID *h = &g_kx_hid[j];

                    if (h->used && h->dev == i)
                        kx_print_hid_line(out, h);
                }
            }
        }

        kcon_flush();

        /* экран перерисован целиком - курсор тоже заново */
        gui_draw_cursor_at(
            g_kfb, g_kfb_stride, g_kfb_w, g_kfb_h, g_kfb_fmt,
            (INTN)px, (INTN)py
        );

        drawn_x = (INTN)px;
        drawn_y = (INTN)py;
    }

    /* убрать курсор с экрана консоли */
    kcon_clear_screen(out);

    print(out, "\n");
}


/* Одно устройство и (рекурсивно) всё, что за ним, - деревом */
static void kx_print_dev_tree(SIMPLE_TEXT_OUTPUT_INTERFACE *out, UINTN di, UINTN indent)
{
    KX_DEV *d = &g_kx_devs[di];
    char path[32];

    kx_dev_path(d, path, sizeof(path));

    for (UINTN i = 0; i < indent; i++)
        print(out, "  ");

    kprintf(out, "Port %s  slot %u  %04x:%04x  %s  - %s\n",
            path, d->slot, d->vid, d->pid, kx_speed_name(d->speed),
            d->status ? d->status : "?");

    for (UINTN k = 0; k < KX_MAX_HID; k++) {
        KX_HID *h = &g_kx_hid[k];
        if (h->used && h->dev == di && h->role != KX_ROLE_HUB) {
            for (UINTN i = 0; i < indent; i++)
                print(out, "  ");
            kx_print_hid_line(out, h);
        }
    }

    if (d->msd >= 0) {
        KX_MSD *m = &g_kx_msd[d->msd];
        for (UINTN i = 0; i < indent; i++)
            print(out, "  ");
        kprintf(out, "    drive \"%s %s\": %s", m->vendor, m->product, m->note);
        if (m->ready)
            kprintf(out, ", %llu MiB", (m->blocks * m->block_size) >> 20);
        kprintf(out, ", reads %llu, errors %llu\n", m->reads, m->errors);
    }

    if (d->hub >= 0) {

        KX_HUB *hb = &g_kx_hubs[d->hub];

        for (UINTN i = 0; i < indent; i++)
            print(out, "  ");
        kprintf(out, "    hub: %u ports, %llu port-change messages\n",
                hb->nports, hb->events);

        for (UINTN p = 1; p <= hb->nports; p++)
            if (hb->child[p] >= 0)
                kx_print_dev_tree(out, (UINTN)hb->child[p], indent + 2);
    }
}

static void kernel_cmd_usb_locked(SIMPLE_TEXT_OUTPUT_INTERFACE *out);

/* Под мьютексом контроллера: если поток usb прямо сейчас
   настраивает новое устройство - дождаться, а не печатать дерево
   на середине перестройки */
void kernel_cmd_usb(SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    kmutex_lock(&g_usb_mutex);
    kernel_cmd_usb_locked(out);
    kmutex_unlock(&g_usb_mutex);
}

static void kernel_cmd_usb_locked(SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    kernel_poll_input();       /* подобрать свежие подключения */

    if (!g_kx.running) {
        print(out, "USB driver is not running (no xHCI or init failed).\n");
        return;
    }

    kx_print_hc_status(out);

    kprintf(out, "Events delivered by: %s", g_kx.irq_mode);
    if (g_kx.irqs)
        kprintf(out, " (%llu interrupts)", g_kx.irqs);
    kprintf(out, "; hot-plug: %llu connected, %llu removed since boot\n\n",
            g_kx.hot_added, g_kx.hot_removed);

    UINTN shown = 0;

    for (UINTN i = 0; i < KX_MAX_DEVS; i++) {

        if (!g_kx_devs[i].used || g_kx_devs[i].parent >= 0)
            continue;

        kx_print_dev_tree(out, i, 0);
        shown++;
    }

    if (shown == 0)
        print(out, "No USB devices. Plug something in - it is picked up automatically.\n");

    print(out, "\nPlug / unplug log:\n");
    kx_print_event_log(out);
}


static void kx_print_size(SIMPLE_TEXT_OUTPUT_INTERFACE *out, UINT64 pages)
{
    UINT64 kib = pages * 4u;

    if (kib >= 10240u)
        kprintf(out, "%llu MiB", kib / 1024u);
    else
        kprintf(out, "%llu KiB", kib);
}

void kernel_cmd_mem(EFI_SYSTEM_TABLE *st, SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    (void)st;

    UINT64 by_type[16];

    for (UINTN i = 0; i < 16; i++)
        by_type[i] = 0;

    for (UINTN i = 0; i < g_kmm_map_count; i++) {

        UINT32 t = g_kmm_map[i].type;

        if (t < 16)
            by_type[t] += g_kmm_map[i].pages;
    }

    print(out, "Memory map from the loader (what the firmware left us):\n");

    for (UINT32 t = 0; t < 16; t++) {

        if (by_type[t] == 0)
            continue;

        kprintf(out, "  %-24s ", kmm_type_name(t));
        kx_print_size(out, by_type[t]);
        print(out, pmm_free_type(t) ? "  -> free RAM for MyOS\n" : "\n");
    }

    UINT64 keep = 0, temp = 0;

    for (UINT32 r = 0; r < g_boot.nreserved && r < MYOS_MAX_RESERVED; r++) {
        if (g_boot.reserved[r].kind == MYOS_RES_KEEP)
            keep += g_boot.reserved[r].pages;
        else
            temp += g_boot.reserved[r].pages;
    }

    print(out, "  of LoaderData: kernel image + boot info + stack ");
    kx_print_size(out, keep);
    print(out, " kept, loader page tables ");
    kx_print_size(out, temp);
    print(out, " freed\n");

    kprintf(out, "  (%llu regions)\n", (UINT64)g_kmm_map_count);

    if (!g_kmm_ready)
        return;

    print(out, "\nPage allocator (bitmap, 4 KiB pages):\n  free ");
    kx_print_size(out, g_kmm_free_pages);
    print(out, " of ");
    kx_print_size(out, g_kmm_usable_pages);
    print(out, " usable, in use ");
    kx_print_size(out, g_kmm_usable_pages - g_kmm_free_pages);
    print(out, "\n  taken back from the firmware and the loader: ");
    kx_print_size(out, g_kmm_reclaimed_pages);
    print(out, "\n");

    /* живая проверка: выделить, записать, освободить */
    UINT64 a = pmm_alloc_pages(4, 0);

    if (a != 0) {

        volatile UINT64 *q = (volatile UINT64 *)P2V(a);
        q[0] = 0x1122334455667788ull;
        BOOLEAN ok = (q[0] == 0x1122334455667788ull);

        pmm_free_pages(a, 4);

        kprintf(out, "  self-test: 4 pages at 0x%08llx - %s\n", a,
                ok ? "allocated, written, freed: OK" : "WRITE CHECK FAILED");
    } else {
        print(out, "  self-test: allocation FAILED\n");
    }

    print(out, "\nKernel heap (kmalloc: slabs 16..1024 bytes + whole pages):\n");
    kprintf(out, "  blocks in use: %llu (~%llu bytes), slab pages %llu, big-block pages %llu\n",
            g_kheap_live, g_kheap_live_bytes, g_kheap_slab_pages, g_kheap_big_pages);

    if (g_kheap_bad_frees)
        kprintf(out, "  WARNING: %llu bad kfree() calls (see COM1 log)\n", g_kheap_bad_frees);

    char rep[96];
    kmalloc_selftest(rep, sizeof(rep));
    kprintf(out, "  self-test: %s\n", rep);
}


/* ================================================================
 * boot - что сделал загрузчик
 * ================================================================ */

void kernel_cmd_boot(SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    kprintf(out, "Loaded by: MyOS loader (BOOTX64.EFI)\n");
    kprintf(out, "Kernel file: %S, %llu KiB\n",
            g_boot.kernel_path, g_boot.kernel_file_size / 1024u);
    kprintf(out, "Kernel image: %llu KiB at phys 0x%llx, virt 0x%llx\n",
            g_boot.kernel_size / 1024u, g_boot.kernel_phys, g_boot.kernel_virt);
    kprintf(out, "Firmware: %S rev 0x%x, UEFI %u.%u\n",
            g_boot.fw_vendor, g_boot.fw_revision,
            g_boot.uefi_revision >> 16, (g_boot.uefi_revision & 0xFFFFu) / 10u);
    kprintf(out, "Screen: %ux%u (stride %u), framebuffer 0x%llx, %llu KiB\n",
            g_boot.fb_width, g_boot.fb_height, g_boot.fb_stride,
            g_boot.fb_phys, g_boot.fb_size / 1024u);
    kprintf(out, "ACPI RSDP: 0x%llx (ACPI %u)\n", g_boot.rsdp_phys, g_boot.acpi_version);
    kprintf(out, "USB (xHCI): %s, firmware driver %s, BIOS handoff %s\n",
            g_boot.xhci_found ? "found" : "not found",
            g_boot.xhci_disconnected ? "disconnected" : "-",
            g_boot.xhci_handoff_ok ? "OK" : "not confirmed");

    if (g_boot.have_boot_time)
        kprintf(out, "Boot time: %04u-%02u-%02u %02u:%02u:%02u\n",
                g_boot.boot_time.Year, g_boot.boot_time.Month,
                g_boot.boot_time.Day, g_boot.boot_time.Hour,
                g_boot.boot_time.Minute, g_boot.boot_time.Second);

    print(out, "\nLoader log:\n");

    /* журнал - построчно, с отступом */
    char line[128];
    UINTN n = 0;

    for (UINTN i = 0; i < g_boot.log_len && i < sizeof(g_boot.log); i++) {

        char c = g_boot.log[i];

        if (c == '\n' || n + 1 >= sizeof(line)) {
            line[n] = '\0';
            kprintf(out, "  | %s\n", line);
            n = 0;
            if (c == '\n')
                continue;
        }

        line[n++] = c;
    }

    if (n > 0) {
        line[n] = '\0';
        kprintf(out, "  | %s\n", line);
    }
}


/* ================================================================
 * vm - виртуальная память
 * ================================================================ */

void kernel_cmd_vm(SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    extern char __kernel_start[], __kernel_text_end[];
    extern char __kernel_rodata_end[], __kernel_end[];

    kprintf(out, "Page tables: PML4 at phys 0x%llx, %llu pages of tables\n",
            g_vmm_pml4_phys, g_vmm_table_pages);
    kprintf(out, "CPU: NX %s, PAT %s (framebuffer is %s)\n",
            g_vmm_nx ? "on" : "NOT SUPPORTED",
            g_vmm_pat ? "reprogrammed" : "not available",
            g_vmm_pat ? "write-combining" : "uncached");

    print(out, "\nAddress space:\n");
    kprintf(out, "  0x0000000000000000  lower half - empty (future programs)\n");
    kprintf(out, "  0xFFFF800000000000  all physical memory up to %llu GiB\n",
            g_vmm_hhdm_top >> 30);
    kprintf(out, "                      (%llu x 2 MiB + %llu x 4 KiB pages)\n",
            g_vmm_pages_2m, g_vmm_pages_4k);
    kprintf(out, "  0xFFFFFE8000000000  kernel stacks with guard pages\n");
    kprintf(out, "  0xFFFFFFFF80000000  kernel image:\n");
    kprintf(out, "      code     %p .. %p  read + execute\n",
            __kernel_start, __kernel_text_end);
    kprintf(out, "      rodata   %p .. %p  read only\n",
            __kernel_text_end, __kernel_rodata_end);
    kprintf(out, "      data/bss %p .. %p  read + write, no execute\n",
            __kernel_rodata_end, __kernel_end);

    print(out, "\nKernel stacks:\n");

    for (UINTN i = 0; i < g_kstack_count; i++) {
        kprintf(out, "  %-28s %3llu KiB, guard page at %p\n",
                g_kstacks[i].name,
                (g_kstacks[i].top - g_kstacks[i].bottom) / 1024u,
                (VOID *)(UINTN)g_kstacks[i].guard);
    }

    UINT64 rsp;
    __asm__ __volatile__("mov %%rsp, %0" : "=r"(rsp));
    kprintf(out, "  (this command runs with RSP = %p)\n", (VOID *)(UINTN)rsp);

    /* проверка перевода адресов туда и обратно */
    UINT64 pk = vmm_virt_to_phys((UINT64)(UINTN)&g_boot);
    kprintf(out, "\nTranslation check: &g_boot = %p -> phys 0x%llx (%s)\n",
            &g_boot, pk,
            (pk >= g_boot.kernel_phys &&
             pk < g_boot.kernel_phys + g_boot.kernel_size) ? "inside the kernel image: OK" : "WRONG");
}


/* ================================================================
 * crash - нарочно уронить ядро, чтобы увидеть экран паники
 * ================================================================ */

/* рекурсия без дна - для "crash stack". noinline и volatile-
   массив, чтобы компилятор не превратил её в цикл */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winfinite-recursion"
__attribute__((noinline))
static UINT64 kx_recurse(UINT64 n)
{
    volatile UINT8 pad[256];

    pad[0] = (UINT8)n;
    pad[255] = (UINT8)(n >> 8);

    return kx_recurse(n + 1) + pad[0] + pad[255];
}
#pragma GCC diagnostic pop

void kernel_cmd_crash(SIMPLE_TEXT_OUTPUT_INTERFACE *out, const char *what)
{
    if (what[0] == '\0') {

        print(out, "Executing an invalid instruction (ud2) on purpose...\n");
        kcon_flush();
        __asm__ __volatile__("ud2");

    } else if (kstreq(what, "null")) {

        print(out, "Reading from a NULL pointer on purpose...\n");
        kcon_flush();
        volatile UINT64 *p = (volatile UINT64 *)(UINTN)0;
        (void)*p;

    } else if (kstreq(what, "write")) {

        print(out, "Writing into the kernel's own CODE on purpose\n");
        print(out, "(it is mapped read-only - the CPU must refuse)...\n");
        kcon_flush();
        volatile UINT8 *code = (volatile UINT8 *)(UINTN)&kernel_cmd_crash;
        code[0] = 0x90;

    } else if (kstreq(what, "stack")) {

        print(out, "Infinite recursion on purpose - the stack will hit its\n");
        print(out, "guard page, and the Double Fault handler (own IST stack)\n");
        print(out, "must still show the panic screen...\n");
        kcon_flush();
        kx_recurse(0);

    } else {

        print(out, "Usage: crash [null|write|stack]\n");
        print(out, "  crash        - invalid instruction (#UD)\n");
        print(out, "  crash null   - read a NULL pointer (Page Fault)\n");
        print(out, "  crash write  - write into kernel code (Page Fault, read-only)\n");
        print(out, "  crash stack  - kernel stack overflow (Double Fault)\n");
        return;
    }

    print(out, "...and nothing happened?! Protection is NOT working.\n");
}
