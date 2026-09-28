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
    if (!g_kernel_mode) {
        print(out, "Mode: firmware (UEFI Boot Services active).\n");
        print(out, "Run 'ebs' to switch to MyOS's own kernel mode.\n");
        return;
    }

    /* Пример kprintf (lib/kprintf.c): одна строка формата вместо
       цепочки print + print_uint + print_hex. Числа 64-битные -
       поэтому %llu / %llx. */
    UINT64 us = kx_uptime_us();

    kprintf(out, "Mode: kernel (no Boot Services)\n");
    kprintf(out, "Uptime in kernel mode: %llu.%03llu s\n",
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

    kprintf(out, "Memory: %llu MiB free, pool blocks in use: %llu\n",
            (g_kmm_free_pages * 4u) / 1024u, g_kpool_allocs);

    kprintf(out, "Keyboard: %llu keys, PS/2 ", g_kbd_keys_total);

    if (g_ps2_present)
        kprintf(out, "present (%llu bytes)", g_ps2_bytes);
    else
        kprintf(out, "absent");

    kprintf(out, ", CapsLock %s\n", g_kbd_caps ? "ON" : "off");

    kprintf(out, "Mouse: %s%llu reports\n",
            g_kmouse_present ? "present, " : "none, ", g_kmouse_reports);

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
        (volatile UINT32 *)(UINTN)
            (d->dev_ctx + (UINT64)h->dci * g_kx.ctx_size);

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

                print(out, "Port ");
                print_uint(out, d->port);
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


void kernel_cmd_usb(SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    if (!g_kernel_mode) {
        print(out, "The MyOS USB driver starts with 'ebs' (it needs the\n");
        print(out, "controller for itself). Use 'xhci' for a read-only look.\n");
        return;
    }

    if (!g_kx.running) {
        print(out, "USB driver is not running (no xHCI or init failed).\n");
        return;
    }

    kx_print_hc_status(out);

    UINTN shown = 0;

    for (UINTN i = 0; i < KX_MAX_DEVS; i++) {

        KX_DEV *d = &g_kx_devs[i];

        if (!d->used)
            continue;

        shown++;

        print(out, "Port ");
        print_uint(out, d->port);
        print(out, "  slot ");
        print_uint(out, d->slot);
        print(out, "  ");
        print_hex(out, d->vid, 4);
        print(out, ":");
        print_hex(out, d->pid, 4);
        print(out, "  ");
        print(out, kx_speed_name(d->speed));
        print(out, "  - ");
        print(out, d->status ? d->status : "?");
        print(out, "\n");

        for (UINTN k = 0; k < KX_MAX_HID; k++) {

            KX_HID *h = &g_kx_hid[k];

            if (!h->used || h->dev != i)
                continue;

            kx_print_hid_line(out, h);
        }
    }

    if (shown == 0)
        print(out, "No USB devices were found on the root ports.\n");
}


void kernel_cmd_mem(EFI_SYSTEM_TABLE *st, SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    UINT64 by_type[16];

    for (UINTN i = 0; i < 16; i++)
        by_type[i] = 0;

    if (!g_kernel_mode) {

        /* В режиме прошивки - просто показать её карту памяти */
        GUI_GET_MEMORY_MAP get_map =
            (GUI_GET_MEMORY_MAP)st->BootServices->GetMemoryMap;
        GUI_ALLOCATE_POOL alloc_pool =
            (GUI_ALLOCATE_POOL)st->BootServices->AllocatePool;
        GUI_FREE_POOL free_pool =
            (GUI_FREE_POOL)st->BootServices->FreePool;

        UINTN size = 0, key = 0, dsize = 0;
        UINT32 dver = 0;

        get_map(&size, NULL, &key, &dsize, &dver);
        size += dsize * 8u;

        VOID *buf = NULL;

        if (alloc_pool(GUI_EFI_BOOT_SERVICES_DATA, size, &buf) != EFI_SUCCESS)
            return;

        if (get_map(&size, buf, &key, &dsize, &dver) == EFI_SUCCESS)
            pmm_save_map(buf, size, dsize);

        free_pool(buf);

        print(out, "Firmware memory map (Boot Services still active):\n");
    } else {
        print(out, "Final memory map (as handed over at ExitBootServices):\n");
    }

    for (UINTN i = 0; i < g_kmm_map_count; i++) {

        UINT32 t = g_kmm_map[i].type;

        if (t < 16)
            by_type[t] += g_kmm_map[i].pages;
    }

    for (UINT32 t = 0; t < 16; t++) {

        if (by_type[t] == 0)
            continue;

        print(out, "  ");
        print(out, kmm_type_name(t));
        print(out, ": ");

        UINT64 kib = by_type[t] * 4u;

        if (kib >= 10240u) {
            print_uint(out, kib / 1024u);
            print(out, " MiB\n");
        } else {
            print_uint(out, kib);
            print(out, " KiB\n");
        }
    }

    print(out, "  (");
    print_uint(out, g_kmm_map_count);
    print(out, " regions)\n");

    if (g_kernel_mode && g_kmm_ready) {

        print(out, "\nMyOS page allocator (bitmap, 4 KiB pages):\n  free ");
        print_uint(out, g_kmm_free_pages);
        print(out, " pages (");
        print_uint(out, (g_kmm_free_pages * 4u) / 1024u);
        print(out, " MiB), in use ");
        print_uint(out, g_kmm_usable_pages - g_kmm_free_pages);
        print(out, " pages, bitmap covers ");
        print_uint(out, (g_kmm_total_pages * 4u) / 1024u);
        print(out, " MiB\n  pool blocks in use: ");
        print_uint(out, g_kpool_allocs);
        print(out, "\n");

        /* живая проверка: выделить, записать, освободить */
        UINT64 a = pmm_alloc_pages(4, 0);

        if (a != 0) {

            volatile UINT64 *q = (volatile UINT64 *)(UINTN)a;
            q[0] = 0x1122334455667788ull;
            BOOLEAN ok = (q[0] == 0x1122334455667788ull);

            pmm_free_pages(a, 4);

            print(out, "  self-test: 4 pages at 0x");
            print_hex(out, a, 8);
            print(out, ok ? " - allocated, written, freed: OK\n" :
                            " - WRITE CHECK FAILED\n");
        } else {
            print(out, "  self-test: allocation FAILED\n");
        }
    }
}
