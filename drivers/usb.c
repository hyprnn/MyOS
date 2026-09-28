/*
 * drivers/usb.c - неблокирующий xHCI + USB HID драйвер kernel mode.
 * Часть MyOS; общие объявления - в myos.h.
 */
#include "myos.h"

KX_STATE g_kx;

KX_DEV g_kx_devs[KX_MAX_DEVS];
KX_HID g_kx_hid[KX_MAX_HID];


/* --- кольца, где МЫ производитель (Command/Transfer) --- */

UINT32 kx_ring_pcs(UINTN seq)
{
    return ((seq / KX_RING_USABLE) % 2u == 0) ? 1u : 0u;
}

UINT64 kx_ring_slot_addr(KX_RING *r, UINTN seq)
{
    return r->phys + (UINT64)(seq % KX_RING_USABLE) * 16u;
}

void kx_ring_init(KX_RING *r, UINT64 phys)
{
    r->phys = phys;
    r->seq = 0;

    raw_zero_mem((volatile UINT8 *)P2V(phys), 4096);

    volatile UINT32 *link =
        (volatile UINT32 *)P2V(phys + (KX_RING_TRBS - 1u) * 16u);

    link[0] = (UINT32)(phys & 0xFFFFFFFFu);
    link[1] = (UINT32)(phys >> 32);
    link[2] = 0;
    /* Link TRB (Type 6), Toggle Cycle (бит 1), Cycle=1 */
    link[3] = (6u << 10) | (1u << 1) | 1u;
}

/*
 * Положить TRB в кольцо (d3 - без Cycle-бита, его ставит сама
 * функция). Возвращает физический адрес TRB (по нему потом
 * находится Command Completion Event этой команды).
 *
 * Link TRB и Cycle-бит (важно, это ровно то место, где раньше
 * прятались баги с "замиранием" на 256-м отчёте): Cycle-бит -
 * это "флажок владения". TRB принадлежит контроллеру, только
 * если его Cycle совпадает с внутренним Cycle State контроллера,
 * который переворачивается каждый раз, когда контроллер проходит
 * по Link TRB в конце страницы. Значит, и Link TRB должен
 * "отдаваться" контроллеру так же, как обычный TRB - в момент,
 * когда МЫ переходим через конец кольца, с Cycle ТЕКУЩЕГО
 * (заканчивающегося) круга. До этого момента у Link TRB Cycle
 * от предыдущего круга, и контроллер, догнав нас, честно
 * останавливается перед ним. Так делает и Linux.
 */
UINT64 kx_ring_push(
    KX_RING *r,
    UINT32 d0, UINT32 d1, UINT32 d2, UINT32 d3
)
{
    UINTN slot = r->seq % KX_RING_USABLE;
    UINT32 pcs = kx_ring_pcs(r->seq);

    if (slot == 0 && r->seq > 0) {

        volatile UINT32 *link =
            (volatile UINT32 *)P2V
                (r->phys + (KX_RING_TRBS - 1u) * 16u);

        link[3] = (6u << 10) | (1u << 1) | (pcs ^ 1u);
    }

    UINT64 addr = r->phys + (UINT64)slot * 16u;
    volatile UINT32 *t = (volatile UINT32 *)P2V(addr);

    t[0] = d0;
    t[1] = d1;
    t[2] = d2;

    /* Cycle - строго последним: как только он записан, TRB
       может быть взят контроллером */
    __asm__ __volatile__("" ::: "memory");

    t[3] = (d3 & ~1u) | pcs;

    r->seq++;

    return addr;
}


/* --- Event Ring, где производитель - контроллер --- */

void kx_erdp_update(void)
{
    UINT64 a = g_kx.evring + (UINT64)(g_kx.ev_deq % KX_EV_TRBS) * 16u;

    /* бит 3 (EHB, Event Handler Busy) - RW1C, пишем 1, чтобы
       сбросить */
    mmio_write32(g_kx.intr0 + 0x18, (UINT32)(a & 0xFFFFFFFFu) | 0x8u);
    mmio_write32(g_kx.intr0 + 0x1C, (UINT32)(a >> 32));
}

/* Взять следующее событие, если оно есть (копией - слот в
   кольце сразу освобождается) */
BOOLEAN kx_ev_fetch(UINT32 ev[4])
{
    volatile UINT32 *t =
        (volatile UINT32 *)P2V
            (g_kx.evring + (UINT64)(g_kx.ev_deq % KX_EV_TRBS) * 16u);

    UINT32 cyc = ((g_kx.ev_deq / KX_EV_TRBS) % 2u == 0) ? 1u : 0u;
    UINT32 d3 = t[3];

    if ((d3 & 1u) != cyc)
        return FALSE;

    ev[0] = t[0];
    ev[1] = t[1];
    ev[2] = t[2];
    ev[3] = d3;

    g_kx.ev_deq++;
    g_kx.events++;

    kx_erdp_update();

    return TRUE;
}


void kx_doorbell(UINT8 slot, UINT32 target)
{
    mmio_write32(g_kx.db + (UINT64)slot * 4u, target);
}

UINT64 kx_dma_page(void)
{
    return pmm_alloc_zeroed(1, KX_DMA_LIMIT);
}


/*
 * Обработка события, которого сейчас никто синхронно не ждёт:
 * отчёты работающих устройств, шаги восстановления после ошибок,
 * изменения портов.
 */
void kx_handle_async_event(UINT32 ev[4])
{
    UINT8 type = (UINT8)((ev[3] >> 10) & 0x3Fu);
    UINT8 cc = (UINT8)((ev[2] >> 24) & 0xFFu);

    if (type == 32) {

        /* Transfer Event */
        UINT8 slot = (UINT8)((ev[3] >> 24) & 0xFFu);
        UINT8 epid = (UINT8)((ev[3] >> 16) & 0x1Fu);

        KX_HID *h = NULL;

        for (UINTN i = 0; i < KX_MAX_HID; i++) {

            KX_HID *c = &g_kx_hid[i];

            if (
                c->used && c->role != KX_ROLE_NONE &&
                g_kx_devs[c->dev].slot == slot && c->dci == epid
            ) {
                h = c;
                break;
            }
        }

        UINT64 trb_ptr = (UINT64)ev[0] | ((UINT64)ev[1] << 32);

        if (h == NULL || h->state != KX_EP_RUN || trb_ptr != h->last_trb) {
            /* не наш/запоздавший/повторный - не трогаем кольцо,
               иначе на конечной точке оказалось бы два TRB */
            g_kx.stray_events++;
            return;
        }

        if (cc == 1 || cc == 13) {

            /* в младших 24 битах - сколько байт НЕ пришло */
            UINT32 residue = ev[2] & 0xFFFFFFu;
            UINTN len =
                (residue <= h->req_len) ? (h->req_len - residue) : 0;

            h->reports++;
            h->err_streak = 0;

            {
                volatile UINT8 *r = (volatile UINT8 *)P2V(h->rep_buf);

                h->last_len = len;

                for (UINTN k = 0; k < 16; k++)
                    h->last_rep[k] = (k < len) ? r[k] : 0;
            }

            kx_hid_report(h, len);
            kx_hid_queue(h);
            return;
        }

        h->errors++;
        h->last_err = cc;
        h->err_streak++;

        klog("usb: slot %u EP %u error cc=%u (streak %u)\n",
             slot, h->dci, cc, h->err_streak);

        /*
         * РАНЬШЕ: после 200 ошибок ЗА ВСЁ ВРЕМЯ конечная точка
         * выключалась навсегда. У беспроводного донгла, который
         * шлёт отчёты до 1000 раз в секунду, редкие ошибки
         * передачи - норма, и 200 штук могли набежать за
         * секунды - мышь "немного двигалась и умирала". Теперь
         * сдаёмся только после многих ошибок ПОДРЯД, без единого
         * удачного отчёта между ними.
         */
        if (h->err_streak > 64) {
            h->state = KX_EP_DEAD;
            return;
        }

        if (cc == 21) {

            /* Missed Service Error: контроллер не успел
               обслужить конечную точку в её интервал. Она при
               этом НЕ останавливается (не Halted) - Reset
               Endpoint тут не нужен и даже вернул бы ошибку
               "неверное состояние". TRB уже списан - просто
               ставим следующий. */
            kx_hid_queue(h);
            return;
        }

        /* Остальные ошибки (Transaction Error, Babble, Stall...)
           останавливают конечную точку (Halted).
           Шаг 1 восстановления - Reset Endpoint Command */
        h->recoveries++;
        h->recover_tsc = rdtsc();
        h->state = KX_EP_RESET;
        h->pending_cmd =
            kx_ring_push(
                &g_kx.cmd, 0, 0, 0,
                ((UINT32)slot << 24) | ((UINT32)h->dci << 16) |
                    (14u << 10)
            );
        kx_doorbell(0, 0);
        return;
    }

    if (type == 33) {

        /* Command Completion Event - наше ли это восстановление? */
        UINT64 ptr = (UINT64)ev[0] | ((UINT64)ev[1] << 32);

        for (UINTN i = 0; i < KX_MAX_HID; i++) {

            KX_HID *h = &g_kx_hid[i];

            if (!h->used || h->pending_cmd != ptr || ptr == 0)
                continue;

            UINT8 slot = g_kx_devs[h->dev].slot;

            h->last_cmd_cc = cc;

            if (h->state == KX_EP_RESET && cc == 19) {

                /* Context State Error: конечная точка на самом деле
                   не была остановлена (ошибка оказалась из тех,
                   после которых контроллер продолжает сам) - ни
                   Reset, ни Set TR Dequeue ей не нужны */
                h->state = KX_EP_RUN;
                h->pending_cmd = 0;
                kx_hid_queue(h);

            } else if (h->state == KX_EP_RESET) {

                /* Шаг 2 - Set TR Dequeue Pointer: сказать
                   контроллеру, что кольцо продолжается с нашего
                   следующего свободного TRB (сбойный TRB
                   пропускаем) */
                UINT64 deq = kx_ring_slot_addr(&h->ring, h->ring.seq);
                UINT32 dcs = kx_ring_pcs(h->ring.seq);

                h->state = KX_EP_SETDEQ;
                h->pending_cmd =
                    kx_ring_push(
                        &g_kx.cmd,
                        (UINT32)(deq & 0xFFFFFFF0u) | dcs,
                        (UINT32)(deq >> 32),
                        0,
                        ((UINT32)slot << 24) |
                            ((UINT32)h->dci << 16) | (16u << 10)
                    );
                kx_doorbell(0, 0);

            } else if (h->state == KX_EP_SETDEQ) {

                /* Шаг 3 - снова работаем */
                h->state = KX_EP_RUN;
                h->pending_cmd = 0;
                kx_hid_queue(h);
            }

            return;
        }

        g_kx.stray_events++;
        return;
    }

    if (type == 34) {

        /* Port Status Change Event: номер порта в битах 31:24
           первого слова. Сбрасываем флаги изменений порта
           (иначе следующих событий по этому порту не будет) */
        UINT32 port = (ev[0] >> 24) & 0xFFu;

        g_kx.port_events++;

        if (port >= 1 && port <= g_kx.cap.MaxPorts) {

            UINT64 pb = g_kx.op + 0x400u + (UINT64)(port - 1u) * 0x10u;
            UINT32 cur = mmio_read32(pb);

            mmio_write32(
                pb,
                portsc_base_for_write(cur) | (cur & PORTSC_RW1CS_MASK)
            );
        }

        return;
    }

    g_kx.stray_events++;
}


/*
 * Синхронное ожидание конкретного события (с таймаутом по TSC).
 *   type 33 (Command Completion) - совпадение по адресу TRB
 *           команды (match_ptr);
 *   type 32 (Transfer) - по SlotID + номеру конечной точки.
 * Всё постороннее - в kx_handle_async_event.
 */
BOOLEAN kx_wait_event(
    UINT8 type,
    UINT64 match_ptr,
    UINT8 slot,
    UINT8 epid,
    UINT32 out_ev[4],
    UINTN timeout_ms
)
{
    UINT64 start = rdtsc();
    UINT64 limit = (g_tsc_hz / 1000u) * (UINT64)timeout_ms;

    for (;;) {

        UINT32 ev[4];

        if (kx_ev_fetch(ev)) {

            UINT8 t = (UINT8)((ev[3] >> 10) & 0x3Fu);
            BOOLEAN match = FALSE;

            if (t == type && type == 33) {

                UINT64 ptr = (UINT64)ev[0] | ((UINT64)ev[1] << 32);
                match = (ptr == match_ptr);

            } else if (t == type && type == 32) {

                match =
                    ((UINT8)((ev[3] >> 24) & 0xFFu) == slot) &&
                    ((UINT8)((ev[3] >> 16) & 0x1Fu) == epid);
            }

            if (match) {

                out_ev[0] = ev[0];
                out_ev[1] = ev[1];
                out_ev[2] = ev[2];
                out_ev[3] = ev[3];
                return TRUE;
            }

            kx_handle_async_event(ev);
            continue;
        }

        if (rdtsc() - start > limit)
            return FALSE;

        cpu_pause();
    }
}


/* Команда контроллеру через Command Ring. Возвращает Completion
   Code (1 = Success, 0 = событие так и не пришло) */
UINT8 kx_command(
    UINT32 d0, UINT32 d1, UINT32 d2, UINT32 d3,
    UINT8 *out_slot
)
{
    UINT64 trb = kx_ring_push(&g_kx.cmd, d0, d1, d2, d3);

    kx_doorbell(0, 0);

    UINT32 ev[4];

    if (!kx_wait_event(33, trb, 0, 0, ev, 1000))
        return 0;

    if (out_slot)
        *out_slot = (UINT8)((ev[3] >> 24) & 0xFFu);

    return (UINT8)((ev[2] >> 24) & 0xFFu);
}


/* Синхронное восстановление остановленной (Halted) конечной
   точки - используется для Endpoint 0 во время настройки
   устройства (например, если устройство ответило STALL на
   необязательный запрос вроде SET_IDLE) */
void kx_recover_sync(UINT8 slot, UINT8 dci, KX_RING *r)
{
    kx_command(
        0, 0, 0,
        ((UINT32)slot << 24) | ((UINT32)dci << 16) | (14u << 10),
        NULL
    );

    UINT64 deq = kx_ring_slot_addr(r, r->seq);
    UINT32 dcs = kx_ring_pcs(r->seq);

    kx_command(
        (UINT32)(deq & 0xFFFFFFF0u) | dcs,
        (UINT32)(deq >> 32),
        0,
        ((UINT32)slot << 24) | ((UINT32)dci << 16) | (16u << 10),
        NULL
    );
}


/*
 * Control transfer на Endpoint 0 (Setup + [Data] + Status) -
 * та же схема, что xhci_control_transfer выше по файлу (там
 * подробно разобраны биты IDT/IOC/TRT), только поверх kx_ring и
 * с правильным ожиданием. Возвращает Completion Code: 1 или 13
 * (Short Packet) = успех, 0 = таймаут.
 */
UINT8 kx_control(
    KX_DEV *d,
    UINT8 bm_request_type,
    UINT8 b_request,
    UINT16 w_value,
    UINT16 w_index,
    UINT16 w_length,
    UINT64 data_phys
)
{
    UINT32 trt;

    if (w_length == 0)
        trt = 0u;
    else if (bm_request_type & 0x80u)
        trt = 3u;
    else
        trt = 2u;

    kx_ring_push(
        &d->ep0,
        (UINT32)bm_request_type | ((UINT32)b_request << 8) |
            ((UINT32)w_value << 16),
        (UINT32)w_index | ((UINT32)w_length << 16),
        8u,
        (1u << 6) | (2u << 10) | (trt << 16)
    );

    if (w_length != 0) {

        UINT32 dir = (bm_request_type & 0x80u) ? 1u : 0u;

        kx_ring_push(
            &d->ep0,
            (UINT32)(data_phys & 0xFFFFFFFFu),
            (UINT32)(data_phys >> 32),
            w_length,
            (3u << 10) | (dir << 16)
        );
    }

    UINT32 status_dir;

    if (w_length == 0)
        status_dir = 1u;
    else
        status_dir = (bm_request_type & 0x80u) ? 0u : 1u;

    kx_ring_push(
        &d->ep0, 0, 0, 0,
        (1u << 5) | (4u << 10) | (status_dir << 16)
    );

    kx_doorbell(d->slot, 1);

    UINT32 ev[4];

    if (!kx_wait_event(32, 0, d->slot, 1, ev, 1000))
        return 0;

    UINT8 cc = (UINT8)((ev[2] >> 24) & 0xFFu);

    if (cc != 1 && cc != 13)
        kx_recover_sync(d->slot, 1, &d->ep0);

    return cc;
}


/* Поставить следующий Normal TRB на Interrupt IN конечную точку */
void kx_hid_queue(KX_HID *h)
{
    UINT32 len = h->maxpkt;

    if (len > 512u)
        len = 512u;

    if (len == 0)
        len = 8u;

    h->req_len = len;

    /* IOC (бит 5) - событие по завершении (в т.ч. коротким
       пакетом - отчёты часто короче maxpkt, тогда в событии
       Completion Code 13 = Short Packet). Адрес TRB запоминаем:
       событие должно прийти именно по нему. */
    h->last_trb =
        kx_ring_push(
            &h->ring,
            (UINT32)(h->rep_buf & 0xFFFFFFFFu),
            (UINT32)(h->rep_buf >> 32),
            len,
            (1u << 5) | (1u << 10)
        );

    kx_doorbell(g_kx_devs[h->dev].slot, h->dci);
}


/* Отчёт мыши, формат которого разобран из Report Descriptor
   (см. hid_parse_report_descriptor выше по файлу) */
void kx_mouse_report_layout(KX_HID *h, volatile UINT8 *rep, UINTN len)
{
    HID_MOUSE_REPORT_LAYOUT *L = &h->layout;

    /* у составных устройств в одном интерфейсе бывает несколько
       отчётов с разными Report ID (мышь + мультимедиа и т.п.) -
       чужие просто пропускаем */
    if (L->has_report_id) {

        if (len < 1 || rep[0] != L->report_id) {
            h->rejected++;
            return;
        }
    }

    UINT32 buttons = 0;

    if (L->has_buttons) {
        buttons =
            hid_extract_bits(
                rep, len, L->button_bit_offset, L->button_count
            );
    }

    UINT32 xr = hid_extract_bits(rep, len, L->x_bit_offset, L->x_bit_size);
    UINT32 yr = hid_extract_bits(rep, len, L->y_bit_offset, L->y_bit_size);

    if (L->x_is_relative) {

        g_kmouse_dx += hid_sign_extend(xr, L->x_bit_size);

    } else {

        /* абсолютная координата (планшет, QEMU usb-tablet):
           переводим в пиксели экрана и отдаём как разницу с
           прошлой позицией - GUI умеет только относительное */
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

        UINT32 wr =
            hid_extract_bits(
                rep, len, L->wheel_bit_offset, L->wheel_bit_size
            );

        g_kmouse_dz += hid_sign_extend(wr, L->wheel_bit_size);
    }

    g_kmouse_buttons = buttons;
    g_kmouse_reports++;
}


void kx_hid_report(KX_HID *h, UINTN len)
{
    volatile UINT8 *rep = (volatile UINT8 *)P2V(h->rep_buf);

    if (h->role == KX_ROLE_KBD_BOOT) {

        kbd_usb_boot_report(rep, len, h->prev_keys);

    } else if (h->role == KX_ROLE_MOUSE_RPT) {

        kx_mouse_report_layout(h, rep, len);

    } else if (h->role == KX_ROLE_MOUSE_BOOT) {

        /* boot protocol мыши: байт0 кнопки, байт1 dX, байт2 dY,
           [байт3 колесо] - все знаковые */
        if (len < 3)
            h->rejected++;

        if (len >= 3) {

            g_kmouse_buttons = rep[0];
            g_kmouse_dx += (INT8)rep[1];
            g_kmouse_dy += (INT8)rep[2];

            if (len >= 4)
                g_kmouse_dz += (INT8)rep[3];

            g_kmouse_reports++;
        }
    }
}


/* Разобрать все накопившиеся события (без ожидания) */
void kx_poll(void)
{
    if (!g_kx.running)
        return;

    for (UINTN i = 0; i < 64; i++) {

        UINT32 ev[4];

        if (!kx_ev_fetch(ev))
            break;

        kx_handle_async_event(ev);
    }

    /*
     * Сторож восстановления: если команда Reset Endpoint / Set TR
     * Dequeue так и не завершилась за ~300 мс (событие потерялось
     * или контроллер повёл себя не по книжке), не ждём вечно -
     * ставим TRB заново. Иначе конечная точка молча "застряла" бы
     * в состоянии восстановления навсегда.
     */
    if (g_tsc_hz != 0) {

        UINT64 now = rdtsc();
        UINT64 limit = (g_tsc_hz / 1000u) * 300u;

        for (UINTN i = 0; i < KX_MAX_HID; i++) {

            KX_HID *h = &g_kx_hid[i];

            if (!h->used || h->role == KX_ROLE_NONE)
                continue;

            if (
                (h->state == KX_EP_RESET || h->state == KX_EP_SETDEQ) &&
                now - h->recover_tsc > limit
            ) {
                h->state = KX_EP_RUN;
                h->pending_cmd = 0;
                h->last_cmd_cc = 0xFF;   /* "не дождались" */
                kx_hid_queue(h);
            }
        }
    }
}


const char *kx_speed_name(UINT8 s)
{
    switch (s) {
    case 1:  return "Full Speed (12 Mbit/s)";
    case 2:  return "Low Speed (1.5 Mbit/s)";
    case 3:  return "High Speed (480 Mbit/s)";
    case 4:  return "SuperSpeed (5 Gbit/s)";
    case 5:  return "SuperSpeed+ (10 Gbit/s)";
    default: return "unknown speed";
    }
}

const char *kx_role_name(UINT8 r)
{
    switch (r) {
    case KX_ROLE_KBD_BOOT:   return "keyboard (boot protocol)";
    case KX_ROLE_MOUSE_RPT:  return "mouse (report descriptor)";
    case KX_ROLE_MOUSE_BOOT: return "mouse (boot protocol)";
    default:                 return "not used";
    }
}


/*
 * Перевести мышь в boot protocol И УБЕДИТЬСЯ, что она перешла.
 *
 * SET_PROTOCOL(Boot) - просьба, а не приказ: некоторые дешёвые
 * донглы заявляют поддержку boot protocol (subclass 1), отвечают
 * на SET_PROTOCOL "успешно" - и продолжают слать свой обычный
 * формат (часто с байтом Report ID в начале). Тогда мы читали бы
 * Report ID как кнопки, а кнопки как dX - курсор прыгал бы не туда
 * или "залипала" бы левая кнопка. Это и было бы угадывание.
 *
 * Поэтому после SET_PROTOCOL спрашиваем GET_PROTOCOL (bRequest
 * 0x03, ответ - 1 байт: 0 = boot, 1 = report):
 *   0            -> boot точно включён, берём фиксированный формат;
 *   1            -> мышь НЕ переключилась, возвращаем FALSE, и
 *                   вызывающий код разберёт её Report Descriptor;
 *   не ответила  -> (запрос обязателен по спеке для boot-устройств,
 *                   но встречаются и такие) доверяем SET_PROTOCOL.
 */
BOOLEAN kx_mouse_switch_to_boot(KX_DEV *d, UINT8 iface, KX_HID *h)
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


/*
 * Полная настройка одного устройства на корневом порту p:
 * Port Reset -> Enable Slot -> Address Device -> дескрипторы ->
 * SET_CONFIGURATION -> настройка HID-интерфейсов -> Configure
 * Endpoint -> первые TRB на опрос. Подробные объяснения каждого
 * шага - в старом коде (xhci_address_device_and_get_descriptor),
 * здесь - только отличия.
 */
void kx_enum_port(SIMPLE_TEXT_OUTPUT_INTERFACE *out, UINTN p)
{
    UINT64 pb = g_kx.op + 0x400u + (UINT64)(p - 1u) * 0x10u;
    UINT32 sc = mmio_read32(pb);

    if (!(sc & PORTSC_BIT_CCS))
        return;

    print(out, "\n  Port ");
    print_uint(out, p);
    print(out, ": device connected\n");

    if (!(sc & PORTSC_BIT_PED)) {

        /* USB2-порту нужен явный Port Reset (USB3 включается
           сам после тренировки линии) */
        mmio_write32(pb, portsc_base_for_write(sc) | PORTSC_BIT_PR);

        BOOLEAN done = FALSE;

        for (UINTN i = 0; i < 500; i++) {

            busy_wait_ms(1);

            UINT32 s = mmio_read32(pb);

            if (s & PORTSC_BIT_PRC) {
                done = TRUE;
                mmio_write32(pb, portsc_base_for_write(s) | PORTSC_BIT_PRC);
                break;
            }
        }

        /* 10 мс "восстановления" после сброса - требование
           спеки USB 2.0 (TRSTRCY) */
        busy_wait_ms(20);

        sc = mmio_read32(pb);

        if (!done || !(sc & PORTSC_BIT_PED)) {
            print(out, "    port reset failed - skipped\n");
            return;
        }
    }

    UINT8 speed = (UINT8)((sc >> 10) & 0xFu);

    print(out, "    ");
    print(out, kx_speed_name(speed));
    print(out, "\n");

    KX_DEV *d = NULL;
    UINTN di = 0;

    for (di = 0; di < KX_MAX_DEVS; di++) {
        if (!g_kx_devs[di].used) {
            d = &g_kx_devs[di];
            break;
        }
    }

    if (d == NULL) {
        print(out, "    too many devices - skipped\n");
        return;
    }

    d->used = TRUE;
    d->port = (UINT8)p;
    d->speed = speed;
    d->status = "setup failed";

    /* --- Enable Slot --- */
    UINT8 slot = 0;
    UINT8 cc = kx_command(0, 0, 0, (9u << 10), &slot);

    if (cc != 1 || slot == 0) {
        print(out, "    Enable Slot failed, cc=");
        print_uint(out, cc);
        print(out, "\n");
        return;
    }

    d->slot = slot;

    d->dev_ctx = kx_dma_page();
    d->in_ctx = kx_dma_page();
    d->buf = kx_dma_page();

    UINT64 ep0_ring = kx_dma_page();

    if (!d->dev_ctx || !d->in_ctx || !d->buf || !ep0_ring) {
        print(out, "    out of memory\n");
        return;
    }

    kx_ring_init(&d->ep0, ep0_ring);

    ((volatile UINT64 *)P2V(g_kx.dcbaa))[slot] = d->dev_ctx;

    /* Стартовый Max Packet Size для EP0. Раньше для Full Speed
       брали 8 - это работает с QEMU, но на настоящем FS-
       устройстве с bMaxPacketSize0=64 ответ длиннее 8 байт
       пришёл бы одним "слишком большим" пакетом (Babble). Как в
       Linux: для FS стартуем с 64 и просим сначала только 8
       байт дескриптора - такой ответ влезает в любой пакет. */
    UINT16 mps0;

    if (speed == 2)
        mps0 = 8;
    else if (speed == 1 || speed == 3)
        mps0 = 64;
    else
        mps0 = 512;

    UINT32 cs = g_kx.ctx_size;
    volatile UINT32 *ictl = (volatile UINT32 *)P2V(d->in_ctx);
    volatile UINT32 *islot = (volatile UINT32 *)P2V(d->in_ctx + cs);
    volatile UINT32 *iep0 = (volatile UINT32 *)P2V(d->in_ctx + 2u * cs);

    ictl[0] = 0;
    ictl[1] = 0x3u;                          /* A0 Slot + A1 EP0 */
    islot[0] = ((UINT32)speed << 20) | (1u << 27);
    islot[1] = (UINT32)p << 16;
    islot[2] = 0;
    islot[3] = 0;
    iep0[0] = 0;
    iep0[1] = (3u << 1) | (4u << 3) | ((UINT32)mps0 << 16);
    iep0[2] = (UINT32)(ep0_ring & 0xFFFFFFFFu) | 1u;
    iep0[3] = (UINT32)(ep0_ring >> 32);
    iep0[4] = 8;

    /* --- Address Device --- */
    cc = kx_command(
        (UINT32)(d->in_ctx & 0xFFFFFFFFu),
        (UINT32)(d->in_ctx >> 32),
        0,
        ((UINT32)slot << 24) | (11u << 10),
        NULL
    );

    if (cc != 1) {
        print(out, "    Address Device failed, cc=");
        print_uint(out, cc);
        print(out, "\n");
        return;
    }

    busy_wait_ms(5);    /* SET_ADDRESS recovery (2 мс по спеке) */

    volatile UINT8 *b = (volatile UINT8 *)P2V(d->buf);

    /* --- первые 8 байт Device Descriptor (ради bMaxPacketSize0) --- */
    cc = kx_control(d, 0x80, 0x06, 0x0100, 0, 8, d->buf);

    if (cc != 1 && cc != 13) {
        print(out, "    GET_DESCRIPTOR(Device, 8) failed, cc=");
        print_uint(out, cc);
        print(out, "\n");
        return;
    }

    UINT16 real_mps0 = b[7];

    if (speed >= 4)
        real_mps0 = (UINT16)(1u << (b[7] & 0xFu));  /* у USB3 это
                                                        степень двойки */

    if (real_mps0 >= 8 && real_mps0 != mps0) {

        /* Evaluate Context: обновить MPS у EP0 на настоящий */
        raw_zero_mem((volatile UINT8 *)P2V(d->in_ctx), 4096);

        ictl[1] = 0x2u;                      /* только A1 = EP0 */
        iep0[1] = (3u << 1) | (4u << 3) | ((UINT32)real_mps0 << 16);
        iep0[2] = (UINT32)(ep0_ring & 0xFFFFFFFFu) | 1u;
        iep0[3] = (UINT32)(ep0_ring >> 32);
        iep0[4] = 8;

        cc = kx_command(
            (UINT32)(d->in_ctx & 0xFFFFFFFFu),
            (UINT32)(d->in_ctx >> 32),
            0,
            ((UINT32)slot << 24) | (13u << 10),
            NULL
        );

        if (cc != 1) {
            print(out, "    Evaluate Context failed, cc=");
            print_uint(out, cc);
            print(out, "\n");
        }

        mps0 = real_mps0;
    }

    d->mps0 = mps0;

    /* --- весь Device Descriptor --- */
    cc = kx_control(d, 0x80, 0x06, 0x0100, 0, 18, d->buf);

    if (cc != 1 && cc != 13) {
        print(out, "    GET_DESCRIPTOR(Device) failed\n");
        return;
    }

    d->vid = (UINT16)(b[8] | (b[9] << 8));
    d->pid = (UINT16)(b[10] | (b[11] << 8));
    d->dclass = b[4];

    print(out, "    VID:PID = ");
    print_hex(out, d->vid, 4);
    print(out, ":");
    print_hex(out, d->pid, 4);
    print(out, ", class ");
    print_uint(out, d->dclass);
    print(out, ", slot ");
    print_uint(out, slot);
    print(out, ", EP0 max packet ");
    print_uint(out, mps0);
    print(out, "\n");

    if (d->dclass == 9) {
        print(out, "    this is a USB hub - hubs are not supported yet\n");
        d->status = "hub (not supported yet)";
        return;
    }

    /* --- Configuration Descriptor: сначала 9 байт (узнать
       полную длину), потом целиком --- */
    UINT64 cfg_phys = d->buf + 1024u;
    volatile UINT8 *cfg = (volatile UINT8 *)P2V(cfg_phys);

    cc = kx_control(d, 0x80, 0x06, 0x0200, 0, 9, cfg_phys);

    if (cc != 1 && cc != 13) {
        print(out, "    GET_DESCRIPTOR(Configuration) failed\n");
        return;
    }

    UINT16 total = (UINT16)(cfg[2] | (cfg[3] << 8));
    UINT8 cfg_value = cfg[5];

    if (total > 1024u)
        total = 1024u;

    if (total < 9u)
        total = 9u;

    cc = kx_control(d, 0x80, 0x06, 0x0200, 0, total, cfg_phys);

    if (cc != 1 && cc != 13) {
        print(out, "    GET_DESCRIPTOR(Configuration, full) failed\n");
        return;
    }

    /* --- найти HID-интерфейсы --- */
    KX_HID_CAND cand[KX_MAX_IF_PER_DEV];
    UINTN ncand = 0;
    INTN cur = -1;
    UINTN off = 0;

    while (off + 2u <= total) {

        UINT8 dl = cfg[off];
        UINT8 dt = cfg[off + 1];

        if (dl < 2)
            break;

        if (dt == 4 && off + 9u <= total) {

            /* Interface: [2]=номер, [3]=alt setting, [5]=класс,
               [6]=подкласс, [7]=протокол */
            cur = -1;

            if (cfg[off + 3] == 0 && cfg[off + 5] == 3 &&
                ncand < KX_MAX_IF_PER_DEV) {

                cand[ncand].iface = cfg[off + 2];
                cand[ncand].subclass = cfg[off + 6];
                cand[ncand].protocol = cfg[off + 7];
                cand[ncand].has_ep = FALSE;
                cand[ncand].ep_addr = 0;
                cand[ncand].maxpkt_raw = 0;
                cand[ncand].interval = 0;
                cand[ncand].rdesc_len = 0;
                cur = (INTN)ncand;
                ncand++;
            }

        } else if (dt == 0x21 && cur >= 0 && off + 9u <= total) {

            if (cfg[off + 6] == 0x22)
                cand[cur].rdesc_len =
                    (UINT16)(cfg[off + 7] | (cfg[off + 8] << 8));

        } else if (dt == 5 && cur >= 0 && off + 7u <= total) {

            UINT8 a = cfg[off + 2];
            UINT8 at = cfg[off + 3];

            if (!cand[cur].has_ep && (a & 0x80u) && (at & 0x3u) == 3u) {

                cand[cur].has_ep = TRUE;
                cand[cur].ep_addr = a;
                cand[cur].maxpkt_raw =
                    (UINT16)(cfg[off + 4] | (cfg[off + 5] << 8));
                cand[cur].interval = cfg[off + 6];
            }
        }

        off += dl;
    }

    if (ncand == 0) {
        print(out, "    not a HID device - left unconfigured\n");
        d->status = "not HID (unused)";
        return;
    }

    /* --- SET_CONFIGURATION --- */
    cc = kx_control(d, 0x00, 0x09, cfg_value, 0, 0, 0);

    if (cc != 1) {
        print(out, "    SET_CONFIGURATION failed, cc=");
        print_uint(out, cc);
        print(out, "\n");
        return;
    }

    /* --- каждый HID-интерфейс --- */
    UINTN hid_idx[KX_MAX_IF_PER_DEV];
    UINTN nh = 0;
    UINT8 max_dci = 1;

    for (UINTN k = 0; k < ncand; k++) {

        KX_HID_CAND *c = &cand[k];

        print(out, "    HID interface ");
        print_uint(out, c->iface);
        print(out, " (subclass ");
        print_uint(out, c->subclass);
        print(out, ", protocol ");
        print_uint(out, c->protocol);
        print(out, "): ");

        if (!c->has_ep) {
            print(out, "no interrupt IN endpoint - skipped\n");
            continue;
        }

        KX_HID *h = NULL;

        for (UINTN i = 0; i < KX_MAX_HID; i++) {
            if (!g_kx_hid[i].used) {
                h = &g_kx_hid[i];
                hid_idx[nh] = i;
                break;
            }
        }

        if (h == NULL) {
            print(out, "too many HID interfaces - skipped\n");
            continue;
        }

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
        h->pending_cmd = 0;
        h->reports = 0;
        h->errors = 0;
        h->last_err = 0;
        h->abs_last_x = (INT64)g_kfb_w / 2;
        h->abs_last_y = (INT64)g_kfb_h / 2;
        h->layout.valid = FALSE;

        for (UINTN i = 0; i < 6; i++)
            h->prev_keys[i] = 0;

        h->err_streak = 0;
        h->mode_note = "";
        h->rejected = 0;
        h->last_cmd_cc = 0;
        h->recoveries = 0;
        h->recover_tsc = 0;
        h->last_len = 0;

        /* Report Descriptor читаем у ВСЕХ HID-интерфейсов, даже
           у клавиатур (где он нам не нужен): так делает любая
           ОС, и некоторые устройства (особенно донглы) не
           начинают слать отчёты, пока его не прочитали */
        UINT16 rlen = c->rdesc_len ? c->rdesc_len : 256u;

        if (rlen > 2048u)
            rlen = 2048u;

        UINT64 rd_phys = d->buf + 2048u;

        raw_zero_mem((volatile UINT8 *)P2V(rd_phys), 2048);

        UINT8 rcc = kx_control(d, 0x81, 0x06, 0x2200, c->iface, rlen, rd_phys);

        if (c->subclass == 1 && c->protocol == 1) {

            /* Клавиатура: SET_PROTOCOL(Boot) - гарантированный
               8-байтный формат, одинаковый у всех клавиатур;
               SET_IDLE(0) - слать отчёт только при изменениях.
               Ошибки SET_IDLE не критичны (восстановление EP0
               делает kx_control) */
            kx_control(d, 0x21, 0x0B, 0, c->iface, 0, 0);
            kx_control(d, 0x21, 0x0A, 0, c->iface, 0, 0);

            h->role = KX_ROLE_KBD_BOOT;

        } else if (
            c->subclass == 1 && c->protocol == 2 &&
            kx_mouse_switch_to_boot(d, c->iface, h)
        ) {

            /*
             * Мышь с поддержкой boot protocol (subclass 1,
             * protocol 2 - это заявляет сама мышь): переключаем
             * её SET_PROTOCOL(Boot) в стандартный формат, одинаковый
             * у всех мышей: байт0 кнопки, байт1 dX, байт2 dY,
             * [байт3 колесо]. Так делают BIOS и загрузчики.
             *
             * РАНЬШЕ основным путём был разбор Report Descriptor.
             * В QEMU он работал, но на реальном донгле (Onikuma)
             * отчёты приходили, а курсор стоял на месте - значит,
             * дескриптор этого устройства разбирался неверно.
             * Boot protocol от дескриптора не зависит вообще.
             * Разбор дескриптора остаётся для устройств без boot
             * protocol (например, планшет с абсолютными
             * координатами) и на случай, если мышь отказалась
             * переключаться (SET_PROTOCOL вернул ошибку).
             */
            h->role = KX_ROLE_MOUSE_BOOT;

        } else {

            if (c->subclass == 1 && c->protocol == 2)
                print(out, "\n    mouse stayed in report mode - reading its descriptor");

            if (rcc == 1 || rcc == 13) {

                print(out, "\n");

                hid_parse_report_descriptor(
                    out,
                    (volatile UINT8 *)P2V(rd_phys),
                    rlen,
                    &h->layout
                );

                print(out, "    -> ");
            }

            if (h->layout.valid)
                h->role = KX_ROLE_MOUSE_RPT;
        }

        if (h->role == KX_ROLE_NONE) {
            print(out, "not a keyboard/mouse we understand - skipped\n");
            continue;
        }

        /* Interval в формате xHCI (см. подробный разбор в
           старом коде выше) */
        UINT8 iv;

        if (speed == 3 || speed >= 4) {

            iv = (c->interval >= 1) ? (UINT8)(c->interval - 1u) : 0;

            if (iv > 15)
                iv = 15;

        } else {

            UINT8 v = (c->interval == 0) ? 1 : c->interval;
            UINT8 lg = 0;

            while ((v >> 1) != 0) {
                v = (UINT8)(v >> 1);
                lg++;
            }

            iv = (UINT8)(lg + 3u);

            if (iv < 3)
                iv = 3;

            if (iv > 10)
                iv = 10;
        }

        h->interval_field = iv;

        UINT64 ring = kx_dma_page();
        h->rep_buf = kx_dma_page();

        if (!ring || !h->rep_buf) {
            print(out, "out of memory\n");
            h->role = KX_ROLE_NONE;
            continue;
        }

        kx_ring_init(&h->ring, ring);

        h->used = TRUE;
        nh++;

        if (h->dci > max_dci)
            max_dci = h->dci;

        print(out, kx_role_name(h->role));
        print(out, ", EP 0x");
        print_hex(out, h->ep_addr, 2);
        print(out, "\n");
    }

    if (nh == 0) {
        d->status = "HID, nothing usable";
        return;
    }

    /* --- Configure Endpoint: все найденные конечные точки
       устройства одной командой --- */
    raw_zero_mem((volatile UINT8 *)P2V(d->in_ctx), 4096);

    volatile UINT32 *oslot = (volatile UINT32 *)P2V(d->dev_ctx);

    ictl[0] = 0;
    ictl[1] = 0x1u;

    /* Slot Context - копия текущего (из Device Context, его
       заполнил контроллер после Address Device), с новым
       Context Entries = старший используемый DCI */
    islot[0] = (oslot[0] & ~(0x1Fu << 27)) | ((UINT32)max_dci << 27);
    islot[1] = oslot[1];
    islot[2] = oslot[2];
    islot[3] = 0;

    for (UINTN k = 0; k < nh; k++) {

        KX_HID *h = &g_kx_hid[hid_idx[k]];

        ictl[1] |= (1u << h->dci);

        volatile UINT32 *ep =
            (volatile UINT32 *)P2V
                (d->in_ctx + (UINT64)(h->dci + 1u) * cs);

        UINT32 esit = (UINT32)h->maxpkt * (UINT32)(h->burst + 1u);

        ep[0] = (UINT32)h->interval_field << 16;
        ep[1] = (3u << 1) | (7u << 3) | ((UINT32)h->burst << 8) |
                ((UINT32)h->maxpkt << 16);
        ep[2] = (UINT32)(h->ring.phys & 0xFFFFFFFFu) | 1u;
        ep[3] = (UINT32)(h->ring.phys >> 32);
        /* Average TRB Length + Max ESIT Payload (некоторые
           контроллеры отвергают периодическую конечную точку с
           нулевым Max ESIT Payload) */
        ep[4] = (UINT32)h->maxpkt | ((esit & 0xFFFFu) << 16);
    }

    cc = kx_command(
        (UINT32)(d->in_ctx & 0xFFFFFFFFu),
        (UINT32)(d->in_ctx >> 32),
        0,
        ((UINT32)slot << 24) | (12u << 10),
        NULL
    );

    if (cc != 1) {

        print(out, "    Configure Endpoint failed, cc=");
        print_uint(out, cc);
        print(out, "\n");

        for (UINTN k = 0; k < nh; k++)
            g_kx_hid[hid_idx[k]].role = KX_ROLE_NONE;

        return;
    }

    /* --- поехали: первый TRB на каждую конечную точку --- */
    for (UINTN k = 0; k < nh; k++) {

        KX_HID *h = &g_kx_hid[hid_idx[k]];

        if (h->role == KX_ROLE_MOUSE_RPT || h->role == KX_ROLE_MOUSE_BOOT)
            g_kmouse_present = TRUE;

        kx_hid_queue(h);
    }

    d->status = "HID, active";

    print(out, "    ready.\n");
}


/*
 * Запуск драйвера целиком (после ExitBootServices): сброс
 * контроллера, структуры, запуск, питание портов, перечисление
 * всех устройств.
 */
void kx_usb_start(SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    if (!g_kx.present) {
        print(out, "  No xHCI controller - USB input disabled.\n");
        return;
    }

    UINT64 mmio = g_kx.mmio;

    g_kx.op = mmio + g_kx.cap.CapLength;
    g_kx.rt = mmio + g_kx.cap.RtsOff;
    g_kx.db = mmio + g_kx.cap.DbOff;
    g_kx.intr0 = g_kx.rt + 0x20u;

    UINT32 hcc1 = mmio_read32(mmio + 0x10);
    g_kx.ctx_size = (hcc1 & 0x4u) ? 64u : 32u;

    UINT32 hcs2 = mmio_read32(mmio + 0x08);
    g_kx.scratchpads =
        (((hcs2 >> 21) & 0x1Fu) << 5) | ((hcs2 >> 27) & 0x1Fu);

    print(out, "  xHCI at ");
    print_uint(out, g_kx.bus);
    print(out, ":");
    print_uint(out, g_kx.devn);
    print(out, ".");
    print_uint(out, g_kx.func);
    print(out, ", ");
    print_uint(out, g_kx.cap.MaxPorts);
    print(out, " ports, context size ");
    print_uint(out, g_kx.ctx_size);
    print(out, ", scratchpad buffers ");
    print_uint(out, g_kx.scratchpads);
    print(out, "\n");

    if (!xhci_reset_controller(g_kx.op)) {
        print(out, "  xHCI reset failed - USB input disabled.\n");
        return;
    }

    g_kx.dcbaa = kx_dma_page();
    g_kx.evring = kx_dma_page();
    g_kx.erst = kx_dma_page();

    UINT64 cmd_page = kx_dma_page();

    if (!g_kx.dcbaa || !g_kx.evring || !g_kx.erst || !cmd_page) {
        print(out, "  out of memory for xHCI structures\n");
        return;
    }

    /* Scratchpad Buffer Array: DCBAA[0] -> массив адресов
       страниц, каждая страница - в распоряжении контроллера */
    if (g_kx.scratchpads > 0) {

        UINT64 arr = kx_dma_page();

        if (!arr) {
            print(out, "  out of memory for scratchpads\n");
            return;
        }

        volatile UINT64 *a = (volatile UINT64 *)P2V(arr);

        for (UINT32 i = 0; i < g_kx.scratchpads && i < 512u; i++) {

            UINT64 pg = kx_dma_page();

            if (!pg) {
                print(out, "  out of memory for scratchpads\n");
                return;
            }

            a[i] = pg;
        }

        ((volatile UINT64 *)P2V(g_kx.dcbaa))[0] = arr;
    }

    kx_ring_init(&g_kx.cmd, cmd_page);

    mmio_write32(g_kx.op + 0x30, (UINT32)(g_kx.dcbaa & 0xFFFFFFFFu));
    mmio_write32(g_kx.op + 0x34, (UINT32)(g_kx.dcbaa >> 32));

    UINT64 crcr = (cmd_page & ~0x3Full) | 0x1u;

    mmio_write32(g_kx.op + 0x18, (UINT32)(crcr & 0xFFFFFFFFu));
    mmio_write32(g_kx.op + 0x1C, (UINT32)(crcr >> 32));

    mmio_write32(g_kx.op + 0x38, g_kx.cap.MaxSlots);

    /* Event Ring: порядок по спеке - ERSTSZ, ERDP, ERSTBA
       (запись ERSTBA заставляет контроллер прочитать таблицу) */
    volatile UINT32 *erst = (volatile UINT32 *)P2V(g_kx.erst);

    erst[0] = (UINT32)(g_kx.evring & 0xFFFFFFFFu);
    erst[1] = (UINT32)(g_kx.evring >> 32);
    erst[2] = KX_EV_TRBS;
    erst[3] = 0;

    g_kx.ev_deq = 0;

    mmio_write32(g_kx.intr0 + 0x08, 1);
    kx_erdp_update();
    mmio_write32(g_kx.intr0 + 0x10, (UINT32)(g_kx.erst & 0xFFFFFFFFu));
    mmio_write32(g_kx.intr0 + 0x14, (UINT32)(g_kx.erst >> 32));

    /* Run */
    mmio_write32(g_kx.op + 0x00, mmio_read32(g_kx.op + 0x00) | 0x1u);

    BOOLEAN started = FALSE;

    for (UINTN i = 0; i < 200; i++) {

        busy_wait_ms(1);

        if ((mmio_read32(g_kx.op + 0x04) & 0x1u) == 0) {
            started = TRUE;
            break;
        }
    }

    if (!started) {
        print(out, "  xHCI did not start - USB input disabled.\n");
        return;
    }

    g_kx.running = TRUE;

    /* Питание портов: после сброса контроллера с Port Power
       Control (HCCPARAMS1 бит 3) порты могут быть обесточены */
    for (UINTN p = 1; p <= g_kx.cap.MaxPorts; p++) {

        UINT64 pb = g_kx.op + 0x400u + (UINT64)(p - 1u) * 0x10u;
        UINT32 s = mmio_read32(pb);

        if (!(s & (1u << 9)))
            mmio_write32(pb, portsc_base_for_write(s) | (1u << 9));
    }

    /* Дать устройствам время заново "появиться" после сброса
       (в QEMU мгновенно, на железе - десятки миллисекунд) */
    busy_wait_ms(200);

    print(out, "  xHCI running. Scanning root ports...\n");

    for (UINTN p = 1; p <= g_kx.cap.MaxPorts; p++)
        kx_enum_port(out, p);

    /* подобрать события, накопившиеся за перечисление */
    kx_poll();

    UINTN kbds = 0, mice = 0;

    for (UINTN i = 0; i < KX_MAX_HID; i++) {

        if (!g_kx_hid[i].used)
            continue;

        if (g_kx_hid[i].role == KX_ROLE_KBD_BOOT)
            kbds++;
        else if (g_kx_hid[i].role != KX_ROLE_NONE)
            mice++;
    }

    print(out, "\n  USB summary: ");
    print_uint(out, kbds);
    print(out, " keyboard(s), ");
    print_uint(out, mice);
    print(out, " mouse/pointer interface(s)\n");
}


/* Опросить ВСЕ источники ввода. Зовётся нашими ConIn/
   SimplePointer при каждом обращении шелла/GUI за вводом */
void kernel_poll_input(void)
{
    kx_poll();
    ps2_poll();
    kbd_repeat_tick();
}
