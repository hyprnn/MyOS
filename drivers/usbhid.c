/*
 * drivers/usbhid.c - USB-клавиатуры и мыши (класс HID) и общая
 * механика "труб прерываний" (Interrupt IN), которой пользуются и
 * хабы.
 * Часть MyOS; общие объявления - в myos.h.
 *
 * Труба - конечная точка Interrupt IN: устройство отдаёт по ней
 * короткие сообщения (отчёт клавиатуры/мыши, у хаба - "на таких-то
 * портах что-то изменилось"). Мы всегда держим на ней один TRB;
 * когда контроллер его исполнит, придёт событие (в прерывании),
 * мы разберём сообщение и поставим следующий TRB.
 */
#include "myos.h"

/* Светодиоды клавиатур: бит 0 NumLock, бит 1 CapsLock, бит 2
   ScrollLock (так и в USB, и - после перестановки - в PS/2) */
volatile BOOLEAN g_kbd_leds_dirty = TRUE;

const char *kx_role_name(UINT8 r)
{
    switch (r) {
    case KX_ROLE_KBD_BOOT:   return "keyboard (boot protocol)";
    case KX_ROLE_MOUSE_RPT:  return "mouse (report descriptor)";
    case KX_ROLE_MOUSE_BOOT: return "mouse (boot protocol)";
    case KX_ROLE_HUB:        return "hub status pipe";
    case KX_ROLE_NET:        return "network data (bulk IN)";
    default:                 return "not used";
    }
}

/* Поставить следующий Normal TRB на трубу */
void kx_pipe_queue(KX_HID *h)
{
    UINT32 len = h->maxpkt;

    if (len > 512u) len = 512u;
    if (len == 0) len = 8u;

    /* USB-модем: целый кадр (или пачка кадров) за один запрос */
    if (h->role == KX_ROLE_NET)
        len = 4096u;

    h->req_len = len;

    /* IOC (бит 5) - событие по завершении (в т.ч. коротким пакетом:
       отчёты часто короче maxpkt, тогда Completion Code 13 = Short
       Packet). Адрес TRB запоминаем: событие придёт именно по нему. */
    h->last_trb =
        kx_ring_push(&h->ring,
                     (UINT32)(h->rep_buf & 0xFFFFFFFFu),
                     (UINT32)(h->rep_buf >> 32),
                     len,
                     (1u << 5) | (1u << 10));

    kx_doorbell(g_kx_devs[h->dev].slot, h->dci);
}

/* Отчёт мыши, формат которого разобран из Report Descriptor */
static void kx_mouse_report_layout(KX_HID *h, volatile UINT8 *rep, UINTN len)
{
    HID_MOUSE_REPORT_LAYOUT *L = &h->layout;

    /* у составных устройств в одном интерфейсе бывает несколько
       отчётов с разными Report ID - чужие пропускаем */
    if (L->has_report_id) {
        if (len < 1 || rep[0] != L->report_id) {
            h->rejected++;
            return;
        }
    }

    UINT32 buttons = 0;

    if (L->has_buttons)
        buttons = hid_extract_bits(rep, len, L->button_bit_offset, L->button_count);

    UINT32 xr = hid_extract_bits(rep, len, L->x_bit_offset, L->x_bit_size);
    UINT32 yr = hid_extract_bits(rep, len, L->y_bit_offset, L->y_bit_size);

    if (L->x_is_relative) {
        g_kmouse_dx += hid_sign_extend(xr, L->x_bit_size);
    } else {
        /* абсолютная координата (планшет) -> разница в пикселях */
        INT64 maxv = (L->x_logical_max > 0) ? L->x_logical_max : 32767;
        INT64 px = ((INT64)xr * (INT64)g_kfb_w) / (maxv + 1);
        g_kmouse_dx += px - h->abs_last_x;
        h->abs_last_x = px;
    }

    if (L->y_is_relative) {
        g_kmouse_dy += hid_sign_extend(yr, L->y_bit_size);
    } else {
        INT64 maxv = (L->y_logical_max > 0) ? L->y_logical_max : 32767;
        INT64 py = ((INT64)yr * (INT64)g_kfb_h) / (maxv + 1);
        g_kmouse_dy += py - h->abs_last_y;
        h->abs_last_y = py;
    }

    if (L->has_wheel) {
        UINT32 wr = hid_extract_bits(rep, len, L->wheel_bit_offset, L->wheel_bit_size);
        g_kmouse_dz += hid_sign_extend(wr, L->wheel_bit_size);
    }

    g_kmouse_buttons = buttons;
    g_kmouse_reports++;
}

/* Пришло сообщение по трубе (в прерывании или под замком) */
void kx_pipe_report(KX_HID *h, UINTN len)
{
    volatile UINT8 *rep = (volatile UINT8 *)P2V(h->rep_buf);

    if (h->role == KX_ROLE_KBD_BOOT) {

        kbd_usb_boot_report(rep, len, h->prev_keys);

    } else if (h->role == KX_ROLE_MOUSE_RPT) {

        kx_mouse_report_layout(h, rep, len);

    } else if (h->role == KX_ROLE_MOUSE_BOOT) {

        /* boot protocol мыши: байт0 кнопки, байт1 dX, байт2 dY,
           [байт3 колесо] - все знаковые */
        if (len < 3) {
            h->rejected++;
            return;
        }

        g_kmouse_buttons = rep[0];
        g_kmouse_dx += (INT8)rep[1];
        g_kmouse_dy += (INT8)rep[2];

        if (len >= 4)
            g_kmouse_dz += (INT8)rep[3];

        g_kmouse_reports++;

    } else if (h->role == KX_ROLE_HUB) {

        kx_hub_report(h, rep, len);

    } else if (h->role == KX_ROLE_NET) {

        kx_net_report(h, rep, len);
    }
}

/*
 * Перевести мышь в boot protocol И УБЕДИТЬСЯ, что она перешла
 * (SET_PROTOCOL, затем GET_PROTOCOL: 0 = boot). Некоторые дешёвые
 * донглы отвечают на SET_PROTOCOL "успешно" и продолжают слать свой
 * формат - тогда разбираем их Report Descriptor.
 */
static BOOLEAN kx_mouse_switch_to_boot(KX_DEV *d, UINT8 iface, KX_HID *h)
{
    UINT8 cc = kx_control(d, 0x21, 0x0B, 0, iface, 0, 0);

    if (cc != 1) {
        h->mode_note = "SET_PROTOCOL(boot) refused";
        return FALSE;
    }

    UINT64 buf = d->buf + 512u;
    volatile UINT8 *b = (volatile UINT8 *)P2V(buf);

    b[0] = 0xEE;   /* заведомо не 0 и не 1 */

    cc = kx_control(d, 0xA1, 0x03, 0, iface, 1, buf);

    if (cc != 1 && cc != 13) {
        h->mode_note = "boot (GET_PROTOCOL not answered, trusting SET)";
        return TRUE;
    }

    if (b[0] == 0) {
        h->mode_note = "boot (confirmed by GET_PROTOCOL)";
        return TRUE;
    }

    h->mode_note = "device ignored SET_PROTOCOL, report mode";
    return FALSE;
}

/* Свободная запись трубы (или -1) */
INTN kx_pipe_alloc(void)
{
    for (UINTN i = 0; i < KX_MAX_HID; i++) {
        if (!g_kx_hid[i].used) {
            raw_zero_mem((volatile UINT8 *)&g_kx_hid[i], sizeof(KX_HID));
            return (INTN)i;
        }
    }

    return -1;
}

/*
 * Подготовить HID-интерфейсы устройства: прочитать Report
 * Descriptor, выбрать формат, выделить кольцо и буфер, добавить
 * конечную точку в список для Configure Endpoint. Трубы помечаются
 * used, но опрос ещё не запущен (kx_hid_start). Возвращает, сколько
 * труб подготовлено; их индексы - в pipes.
 */
UINTN kx_hid_prepare(SIMPLE_TEXT_OUTPUT_INTERFACE *out, UINTN di,
                     KX_HID_CAND *cand, UINTN ncand,
                     KX_EPCFG *eps, UINTN *neps, UINTN *pipes)
{
    KX_DEV *d = &g_kx_devs[di];
    UINTN nh = 0;

    for (UINTN k = 0; k < ncand; k++) {

        KX_HID_CAND *c = &cand[k];

        kx_out(out, "    HID interface %u (subclass %u, protocol %u): ",
               c->iface, c->subclass, c->protocol);

        if (!c->has_ep) {
            kx_out(out, "no interrupt IN endpoint - skipped\n");
            continue;
        }

        INTN hi = kx_pipe_alloc();

        if (hi < 0) {
            kx_out(out, "too many HID interfaces - skipped\n");
            continue;
        }

        KX_HID *h = &g_kx_hid[hi];

        h->dev = (UINT8)di;
        h->iface = c->iface;
        h->subclass = c->subclass;
        h->protocol = c->protocol;
        h->role = KX_ROLE_NONE;
        h->ep_addr = c->ep_addr;
        h->dci = (UINT8)((c->ep_addr & 0x0Fu) * 2u + 1u);
        h->maxpkt = (UINT16)(c->maxpkt_raw & 0x7FFu);
        h->burst = (UINT8)((c->maxpkt_raw >> 11) & 0x3u);
        h->interval_raw = c->interval;
        h->rdesc_len = c->rdesc_len;
        h->state = KX_EP_RUN;
        h->abs_last_x = (INT64)g_kfb_w / 2;
        h->abs_last_y = (INT64)g_kfb_h / 2;
        h->layout.valid = FALSE;
        h->mode_note = "";

        /* Report Descriptor читаем у ВСЕХ HID-интерфейсов: так
           делает любая ОС, и некоторые донглы не начинают слать
           отчёты, пока его не прочитали */
        UINT16 rlen = c->rdesc_len ? c->rdesc_len : 256u;

        if (rlen > 2048u)
            rlen = 2048u;

        UINT64 rd_phys = d->buf + 2048u;

        raw_zero_mem((volatile UINT8 *)P2V(rd_phys), 2048);

        UINT8 rcc = kx_control(d, 0x81, 0x06, 0x2200, c->iface, rlen, rd_phys);

        if (c->subclass == 1 && c->protocol == 1) {

            /* Клавиатура: SET_PROTOCOL(Boot) - единый 8-байтный
               формат; SET_IDLE(0) - отчёт только при изменениях */
            kx_control(d, 0x21, 0x0B, 0, c->iface, 0, 0);
            kx_control(d, 0x21, 0x0A, 0, c->iface, 0, 0);
            h->role = KX_ROLE_KBD_BOOT;

        } else if (c->subclass == 1 && c->protocol == 2 &&
                   kx_mouse_switch_to_boot(d, c->iface, h)) {

            h->role = KX_ROLE_MOUSE_BOOT;

        } else {

            if (c->subclass == 1 && c->protocol == 2)
                kx_out(out, "\n    mouse stayed in report mode - reading its descriptor");

            if (rcc == 1 || rcc == 13) {
                kx_out(out, "\n");
                hid_parse_report_descriptor(out, (volatile UINT8 *)P2V(rd_phys),
                                            rlen, &h->layout);
                kx_out(out, "    -> ");
            }

            if (h->layout.valid)
                h->role = KX_ROLE_MOUSE_RPT;
        }

        if (h->role == KX_ROLE_NONE) {
            kx_out(out, "not a keyboard/mouse we understand - skipped\n");
            continue;
        }

        h->interval_field = kx_interval_field(d->speed, c->interval);

        UINT64 ring = kx_dev_page(d);
        h->rep_buf = kx_dev_page(d);

        if (!ring || !h->rep_buf) {
            kx_out(out, "out of memory\n");
            continue;
        }

        kx_ring_init(&h->ring, ring);

        KX_EPCFG *e = &eps[(*neps)++];

        e->dci = h->dci;
        e->type = 7;                                  /* Interrupt IN */
        e->maxpkt = h->maxpkt;
        e->burst = h->burst;
        e->interval = h->interval_field;
        e->ring = ring;
        e->avg = h->maxpkt;
        e->esit = (UINT32)h->maxpkt * (UINT32)(h->burst + 1u);

        h->used = TRUE;
        pipes[nh++] = (UINTN)hi;

        kx_out(out, "%s, EP 0x%02x\n", kx_role_name(h->role), h->ep_addr);
    }

    return nh;
}

/* Запустить опрос подготовленных труб */
void kx_hid_start(UINTN *pipes, UINTN n)
{
    kx_lock();

    for (UINTN k = 0; k < n; k++) {

        KX_HID *h = &g_kx_hid[pipes[k]];

        if (!h->used || h->role == KX_ROLE_NONE)
            continue;

        if (h->role == KX_ROLE_KBD_BOOT)
            g_kbd_leds_dirty = TRUE;    /* выставить ему текущие огоньки */

        kx_pipe_queue(h);
    }

    kx_hid_update_presence();

    kx_unlock();
}

/* Есть ли вообще мышь (USB или PS/2) - для GUI */
void kx_hid_update_presence(void)
{
    BOOLEAN m = g_ps2_aux_present;

    for (UINTN i = 0; i < KX_MAX_HID; i++)
        if (g_kx_hid[i].used &&
            (g_kx_hid[i].role == KX_ROLE_MOUSE_RPT || g_kx_hid[i].role == KX_ROLE_MOUSE_BOOT))
            m = TRUE;

    g_kmouse_present = m;
}

/*
 * Сторож восстановления: если команда Reset Endpoint / Set TR
 * Dequeue так и не завершилась за ~300 мс, не ждём вечно - ставим
 * TRB заново. (Под замком.)
 */
void kx_pipes_watchdog(void)
{
    if (g_tsc_hz == 0)
        return;

    UINT64 now = rdtsc();
    UINT64 limit = (g_tsc_hz / 1000u) * 300u;

    for (UINTN i = 0; i < KX_MAX_HID; i++) {

        KX_HID *h = &g_kx_hid[i];

        if (!h->used || h->role == KX_ROLE_NONE)
            continue;

        if ((h->state == KX_EP_RESET || h->state == KX_EP_SETDEQ) &&
            now - h->recover_tsc > limit) {
            h->state = KX_EP_RUN;
            h->pending_cmd = 0;
            h->last_cmd_cc = 0xFF;   /* "не дождались" */
            kx_pipe_queue(h);
        }
    }
}

/*
 * Светодиоды: CapsLock/NumLock/ScrollLock переключаются в драйвере
 * клавиатуры (в том числе в прерывании), а зажечь огонёк - это
 * запрос к устройству с ожиданием ответа. Поэтому здесь, в основном
 * коде: USB-клавиатурам - SET_REPORT (Output, 1 байт), PS/2 - команда
 * 0xED.
 */
void kx_hid_service_leds(void)
{
    if (!g_kbd_leds_dirty)
        return;

    g_kbd_leds_dirty = FALSE;

    UINT8 leds = kbd_led_bits();

    for (UINTN i = 0; i < KX_MAX_HID; i++) {

        KX_HID *h = &g_kx_hid[i];

        if (!h->used || h->role != KX_ROLE_KBD_BOOT)
            continue;

        KX_DEV *d = &g_kx_devs[h->dev];
        UINT64 buf = d->buf + 3072u;

        *(volatile UINT8 *)P2V(buf) = leds;

        /* SET_REPORT: тип Output (2), Report ID 0 */
        kx_control(d, 0x21, 0x09, 0x0200, h->iface, 1, buf);
    }

    ps2_set_leds(leds);
}
