/*
 * drivers/usb.c - USB-контроллер xHCI: ядро драйвера.
 * Часть MyOS; общие объявления - в myos.h.
 *
 * Что здесь:
 *   * кольца TRB и разбор событий контроллера;
 *   * синхронные операции (команды контроллеру, control-запросы на
 *     Endpoint 0, bulk-передачи) - для настройки устройств;
 *   * перечисление устройства (Enable Slot, Address Device,
 *     дескрипторы) - и в порту контроллера, и за хабом;
 *   * горячее подключение и отключение;
 *   * прерывание от контроллера (MSI).
 * Отдельно: usbhid.c (клавиатуры и мыши), usbhub.c (хабы),
 * usbmsd.c (флешки).
 *
 * ДВА "ПОТОКА" И ЗАМОК.
 * Процессор один, но код драйвера выполняется в двух местах:
 *   1. в обработчике прерывания (kx_usb_irq) - контроллер сообщил о
 *      событиях: пришёл отчёт мыши, изменился порт;
 *   2. в "основном" коде - шелл/GUI спрашивают ввод, и тут же
 *      (kx_service) выполняется всё, что требует ожидания: настройка
 *      нового устройства, отключение, светодиоды клавиатуры.
 * Обработчик прерывания никогда ничего не ждёт. А основной код, пока
 * трогает общие структуры (кольца, списки устройств), держит "замок"
 * kx_lock - то есть просто запрещает прерывания. Во время долгих
 * ожиданий (сброс порта, ответ устройства) замок ненадолго
 * отпускается (kx_relax), чтобы таймер и клавиатура не простаивали;
 * если событие, которого ждёт основной код, заберёт обработчик
 * прерывания - он положит его в "ящик" g_kx.wait_*.
 */
#include "myos.h"

KX_STATE g_kx;
KX_DEV g_kx_devs[KX_MAX_DEVS];
KX_HID g_kx_hid[KX_MAX_HID];

/* ================================================================
 * Замок
 * ================================================================ */

static UINTN   g_kx_lock_depth = 0;
static BOOLEAN g_kx_lock_if = FALSE;    /* были ли прерывания включены
                                           до первого захвата */
static volatile BOOLEAN g_kx_in_irq = FALSE;

void kx_lock(void)
{
    UINT64 fl;

    __asm__ __volatile__("pushfq; popq %0; cli" : "=r"(fl) : : "memory");

    if (g_kx_lock_depth++ == 0)
        g_kx_lock_if = (fl & (1u << 9)) != 0;
}

void kx_unlock(void)
{
    if (g_kx_lock_depth == 0)
        return;

    if (--g_kx_lock_depth == 0 && g_kx_lock_if)
        __asm__ __volatile__("sti" ::: "memory");
}

/* Посреди долгого ожидания: на мгновение пустить прерывания */
static void kx_relax(void)
{
    if (g_kx_lock_depth > 0 && g_kx_lock_if && !g_kx_in_irq) {
        __asm__ __volatile__("sti; nop; nop; nop; nop; cli" ::: "memory");
    } else {
        cpu_pause();
    }
}

/* Пауза в миллисекундах, во время которой прерывания работают */
void kx_msleep(UINTN ms)
{
    UINT64 start = rdtsc();
    UINT64 cycles = (g_tsc_hz / 1000u) * (UINT64)ms;

    if (g_tsc_hz == 0) {
        busy_wait_ms(ms);
        return;
    }

    while (rdtsc() - start < cycles)
        kx_relax();
}

/* Сообщение: либо в консоль (out != NULL), либо только в лог COM1
   (фоновое подключение - в GUI писать на экран нельзя) */
void kx_out(SIMPLE_TEXT_OUTPUT_INTERFACE *out, const char *fmt, ...)
{
    char buf[256];
    va_list ap;

    va_start(ap, fmt);
    kvsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    if (out != NULL)
        print(out, buf);
    else
        klog("usb: %s", buf);
}

/* ================================================================
 * Журнал подключений (команда usb)
 * ================================================================ */

static char  g_kx_log[KX_LOG_LINES][KX_LOG_LEN];
static UINTN g_kx_log_n = 0;

void kx_event_log(const char *fmt, ...)
{
    char *dst = g_kx_log[g_kx_log_n % KX_LOG_LINES];
    va_list ap;
    UINT64 ms = kx_uptime_us() / 1000u;
    UINTN n = ksnprintf(dst, KX_LOG_LEN, "[%4llu.%01llu s] ", ms / 1000u, (ms / 100u) % 10u);

    if (n >= KX_LOG_LEN)
        n = KX_LOG_LEN - 1;

    va_start(ap, fmt);
    kvsnprintf(dst + n, KX_LOG_LEN - n, fmt, ap);
    va_end(ap);

    klog("usb event: %s\n", dst);
    g_kx_log_n++;
}

void kx_print_event_log(SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    UINTN first = (g_kx_log_n > KX_LOG_LINES) ? g_kx_log_n - KX_LOG_LINES : 0;

    if (g_kx_log_n == 0) {
        print(out, "  (no plug/unplug events since boot)\n");
        return;
    }

    for (UINTN i = first; i < g_kx_log_n; i++)
        kprintf(out, "  %s\n", g_kx_log[i % KX_LOG_LINES]);
}

/* ================================================================
 * Кольца, где МЫ производитель (Command / Transfer)
 * ================================================================ */

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
 * функция). Возвращает физический адрес TRB.
 *
 * Cycle-бит - "флажок владения": TRB принадлежит контроллеру, если
 * его Cycle совпадает с внутренним Cycle State контроллера, который
 * переворачивается при каждом проходе по Link TRB. Link TRB
 * "отдаётся" контроллеру в момент, когда МЫ переходим через конец
 * кольца, с Cycle текущего круга (так делает и Linux).
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
            (volatile UINT32 *)P2V(r->phys + (KX_RING_TRBS - 1u) * 16u);

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

/* ================================================================
 * Event Ring (производитель - контроллер)
 * ================================================================ */

void kx_erdp_update(void)
{
    UINT64 a = g_kx.evring + (UINT64)(g_kx.ev_deq % KX_EV_TRBS) * 16u;

    /* бит 3 (EHB, Event Handler Busy) - RW1C, пишем 1, чтобы
       сбросить */
    mmio_write32(g_kx.intr0 + 0x18, (UINT32)(a & 0xFFFFFFFFu) | 0x8u);
    mmio_write32(g_kx.intr0 + 0x1C, (UINT32)(a >> 32));
}

/* Взять следующее событие, если оно есть */
BOOLEAN kx_ev_fetch(UINT32 ev[4])
{
    volatile UINT32 *t =
        (volatile UINT32 *)P2V(g_kx.evring + (UINT64)(g_kx.ev_deq % KX_EV_TRBS) * 16u);

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

/* Страница, которая будет возвращена при отключении устройства */
UINT64 kx_dev_page(KX_DEV *d)
{
    if (d->npages >= KX_MAX_DEV_PAGES)
        return 0;

    UINT64 p = kx_dma_page();

    if (p != 0)
        d->pages[d->npages++] = p;

    return p;
}

/* Подходит ли событие тому, кто сейчас ждёт синхронно */
static BOOLEAN kx_wait_matches(UINT32 ev[4])
{
    if (!g_kx.wait_active || g_kx.wait_done)
        return FALSE;

    UINT8 t = (UINT8)((ev[3] >> 10) & 0x3Fu);

    if (t != g_kx.wait_type)
        return FALSE;

    if (t == 33) {
        UINT64 ptr = (UINT64)ev[0] | ((UINT64)ev[1] << 32);
        return ptr == g_kx.wait_ptr;
    }

    if (t == 32)
        return ((UINT8)((ev[3] >> 24) & 0xFFu) == g_kx.wait_slot) &&
               ((UINT8)((ev[3] >> 16) & 0x1Fu) == g_kx.wait_ep);

    return FALSE;
}

/*
 * Разобрать все накопившиеся события. Вызывается ТОЛЬКО с
 * запрещёнными прерываниями: из обработчика прерывания или под
 * замком.
 */
void kx_pump(void)
{
    if (!g_kx.running)
        return;

    BOOLEAN any = FALSE;

    for (UINTN i = 0; i < 256; i++) {

        UINT32 ev[4];

        if (!kx_ev_fetch(ev))
            break;

        any = TRUE;

        if (kx_wait_matches(ev)) {
            g_kx.wait_ev[0] = ev[0];
            g_kx.wait_ev[1] = ev[1];
            g_kx.wait_ev[2] = ev[2];
            g_kx.wait_ev[3] = ev[3];
            g_kx.wait_done = TRUE;
            continue;
        }

        kx_handle_async_event(ev);
    }

    /* сказать контроллеру, докуда мы дочитали (заодно сбрасывает
       бит "обработчик занят") - один раз за пачку */
    if (any)
        kx_erdp_update();
}

/*
 * Синхронное ожидание конкретного события (с таймаутом по TSC).
 *   type 33 (Command Completion) - совпадение по адресу TRB команды;
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
    kx_lock();

    g_kx.wait_type = type;
    g_kx.wait_ptr = match_ptr;
    g_kx.wait_slot = slot;
    g_kx.wait_ep = epid;
    g_kx.wait_done = FALSE;
    g_kx.wait_active = TRUE;

    UINT64 start = rdtsc();
    UINT64 limit = (g_tsc_hz / 1000u) * (UINT64)timeout_ms;
    BOOLEAN ok = FALSE;

    for (;;) {

        kx_pump();

        if (g_kx.wait_done) {
            out_ev[0] = g_kx.wait_ev[0];
            out_ev[1] = g_kx.wait_ev[1];
            out_ev[2] = g_kx.wait_ev[2];
            out_ev[3] = g_kx.wait_ev[3];
            ok = TRUE;
            break;
        }

        if (rdtsc() - start > limit)
            break;

        kx_relax();
    }

    g_kx.wait_active = FALSE;

    kx_unlock();

    return ok;
}

/* Команда контроллеру через Command Ring. Возвращает Completion
   Code (1 = Success, 0 = событие так и не пришло) */
UINT8 kx_command(
    UINT32 d0, UINT32 d1, UINT32 d2, UINT32 d3,
    UINT8 *out_slot
)
{
    kx_lock();

    UINT64 trb = kx_ring_push(&g_kx.cmd, d0, d1, d2, d3);

    kx_doorbell(0, 0);

    UINT32 ev[4];
    UINT8 cc = 0;

    if (kx_wait_event(33, trb, 0, 0, ev, 1000)) {
        if (out_slot)
            *out_slot = (UINT8)((ev[3] >> 24) & 0xFFu);
        cc = (UINT8)((ev[2] >> 24) & 0xFFu);
    }

    kx_unlock();

    return cc;
}

/* Синхронное восстановление остановленной (Halted) конечной точки:
   Reset Endpoint + Set TR Dequeue Pointer на наш следующий TRB */
void kx_recover_sync(UINT8 slot, UINT8 dci, KX_RING *r)
{
    kx_command(0, 0, 0,
               ((UINT32)slot << 24) | ((UINT32)dci << 16) | (14u << 10),
               NULL);

    UINT64 deq = kx_ring_slot_addr(r, r->seq);
    UINT32 dcs = kx_ring_pcs(r->seq);

    kx_command((UINT32)(deq & 0xFFFFFFF0u) | dcs,
               (UINT32)(deq >> 32), 0,
               ((UINT32)slot << 24) | ((UINT32)dci << 16) | (16u << 10),
               NULL);
}

/*
 * Control transfer на Endpoint 0 (Setup + [Data] + Status).
 * Возвращает Completion Code: 1 или 13 (Short Packet) = успех,
 * 0 = таймаут.
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

    kx_lock();

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

        kx_ring_push(&d->ep0,
                     (UINT32)(data_phys & 0xFFFFFFFFu),
                     (UINT32)(data_phys >> 32),
                     w_length,
                     (3u << 10) | (dir << 16));
    }

    UINT32 status_dir;

    if (w_length == 0)
        status_dir = 1u;
    else
        status_dir = (bm_request_type & 0x80u) ? 0u : 1u;

    kx_ring_push(&d->ep0, 0, 0, 0,
                 (1u << 5) | (4u << 10) | (status_dir << 16));

    kx_doorbell(d->slot, 1);

    UINT32 ev[4];
    UINT8 cc = 0;

    if (kx_wait_event(32, 0, d->slot, 1, ev, 1000)) {

        cc = (UINT8)((ev[2] >> 24) & 0xFFu);

        if (cc != 1 && cc != 13)
            kx_recover_sync(d->slot, 1, &d->ep0);
    }

    kx_unlock();

    return cc;
}

/*
 * Bulk-передача (флешки): один Normal TRB на len байт (не больше
 * одной страницы), ожидание события. actual - сколько байт реально
 * прошло. Возвращает Completion Code (1/13 - успех).
 */
UINT8 kx_bulk(
    KX_DEV *d, UINT8 dci, UINT8 ep_addr, KX_RING *r,
    UINT64 buf, UINT32 len, UINT32 *actual, UINTN timeout_ms
)
{
    kx_lock();

    kx_ring_push(r,
                 (UINT32)(buf & 0xFFFFFFFFu), (UINT32)(buf >> 32),
                 len & 0x1FFFFu,
                 (1u << 5) | (1u << 2) | (1u << 10));   /* IOC, ISP, Normal */

    kx_doorbell(d->slot, dci);

    UINT32 ev[4];
    UINT8 cc = 0;

    if (kx_wait_event(32, 0, d->slot, dci, ev, timeout_ms)) {

        cc = (UINT8)((ev[2] >> 24) & 0xFFu);

        UINT32 residue = ev[2] & 0xFFFFFFu;

        if (actual)
            *actual = (residue <= len) ? len - residue : 0;

        if (cc != 1 && cc != 13) {
            /* конечная точка остановлена (STALL и т.п.): поправить
               у контроллера и попросить устройство снять "halt"
               (CLEAR_FEATURE ENDPOINT_HALT) */
            kx_recover_sync(d->slot, dci, r);
            kx_control(d, 0x02, 0x01, 0, ep_addr, 0, 0);
        }
    } else {
        /* таймаут: остановить конечную точку и выровнять кольцо */
        kx_command(0, 0, 0,
                   ((UINT32)d->slot << 24) | ((UINT32)dci << 16) | (15u << 10),
                   NULL);
        kx_recover_sync(d->slot, dci, r);
    }

    kx_unlock();

    return cc;
}

/* ================================================================
 * События, которых никто не ждёт синхронно
 * ================================================================ */

void kx_handle_async_event(UINT32 ev[4])
{
    UINT8 type = (UINT8)((ev[3] >> 10) & 0x3Fu);
    UINT8 cc = (UINT8)((ev[2] >> 24) & 0xFFu);

    if (type == 32) {

        /* Transfer Event: отчёт HID или сообщение хаба */
        UINT8 slot = (UINT8)((ev[3] >> 24) & 0xFFu);
        UINT8 epid = (UINT8)((ev[3] >> 16) & 0x1Fu);
        KX_HID *h = NULL;

        for (UINTN i = 0; i < KX_MAX_HID; i++) {
            KX_HID *c = &g_kx_hid[i];
            if (c->used && c->role != KX_ROLE_NONE &&
                g_kx_devs[c->dev].slot == slot && c->dci == epid) {
                h = c;
                break;
            }
        }

        UINT64 trb_ptr = (UINT64)ev[0] | ((UINT64)ev[1] << 32);

        if (h == NULL || h->state != KX_EP_RUN || trb_ptr != h->last_trb) {
            /* не наш/запоздавший/повторный - не трогаем кольцо */
            g_kx.stray_events++;
            return;
        }

        if (cc == 1 || cc == 13) {

            /* в младших 24 битах - сколько байт НЕ пришло */
            UINT32 residue = ev[2] & 0xFFFFFFu;
            UINTN len = (residue <= h->req_len) ? (h->req_len - residue) : 0;
            volatile UINT8 *r = (volatile UINT8 *)P2V(h->rep_buf);

            h->reports++;
            h->err_streak = 0;
            h->last_len = len;

            for (UINTN k = 0; k < 16; k++)
                h->last_rep[k] = (k < len) ? r[k] : 0;

            kx_pipe_report(h, len);
            kx_pipe_queue(h);
            return;
        }

        h->errors++;
        h->last_err = cc;
        h->err_streak++;

        klog("usb: slot %u EP %u error cc=%u (streak %u)\n",
             slot, h->dci, cc, h->err_streak);

        /* сдаёмся только после многих ошибок ПОДРЯД (у беспроводного
           донгла редкие ошибки передачи - норма) */
        if (h->err_streak > 64) {
            h->state = KX_EP_DEAD;
            return;
        }

        if (cc == 21) {
            /* Missed Service Error: конечная точка не остановлена,
               просто ставим следующий TRB */
            kx_pipe_queue(h);
            return;
        }

        /* Остальные ошибки останавливают конечную точку (Halted).
           Шаг 1 восстановления - Reset Endpoint Command. Если
           устройство выдернули - дальше придёт отключение порта. */
        h->recoveries++;
        h->recover_tsc = rdtsc();
        h->state = KX_EP_RESET;
        h->pending_cmd =
            kx_ring_push(&g_kx.cmd, 0, 0, 0,
                         ((UINT32)slot << 24) | ((UINT32)h->dci << 16) | (14u << 10));
        kx_doorbell(0, 0);
        return;
    }

    if (type == 33) {

        /* Command Completion - наше ли это восстановление? */
        UINT64 ptr = (UINT64)ev[0] | ((UINT64)ev[1] << 32);

        for (UINTN i = 0; i < KX_MAX_HID; i++) {

            KX_HID *h = &g_kx_hid[i];

            if (!h->used || h->pending_cmd != ptr || ptr == 0)
                continue;

            UINT8 slot = g_kx_devs[h->dev].slot;

            h->last_cmd_cc = cc;

            if (h->state == KX_EP_RESET && cc == 19) {

                /* Context State Error: точка на самом деле не была
                   остановлена */
                h->state = KX_EP_RUN;
                h->pending_cmd = 0;
                kx_pipe_queue(h);

            } else if (h->state == KX_EP_RESET) {

                /* Шаг 2 - Set TR Dequeue Pointer на наш следующий TRB */
                UINT64 deq = kx_ring_slot_addr(&h->ring, h->ring.seq);
                UINT32 dcs = kx_ring_pcs(h->ring.seq);

                h->state = KX_EP_SETDEQ;
                h->pending_cmd =
                    kx_ring_push(&g_kx.cmd,
                                 (UINT32)(deq & 0xFFFFFFF0u) | dcs,
                                 (UINT32)(deq >> 32), 0,
                                 ((UINT32)slot << 24) | ((UINT32)h->dci << 16) | (16u << 10));
                kx_doorbell(0, 0);

            } else if (h->state == KX_EP_SETDEQ) {

                /* Шаг 3 - снова работаем */
                h->state = KX_EP_RUN;
                h->pending_cmd = 0;
                kx_pipe_queue(h);
            }

            return;
        }

        g_kx.stray_events++;
        return;
    }

    if (type == 34) {

        /* Port Status Change Event: номер порта в битах 31:24.
           Здесь (возможно, в прерывании) только ЗАПОМИНАЕМ, что порт
           изменился, и сбрасываем флаги изменений - кроме PRC
           ("сброс порта завершён"): его ждёт и сбрасывает сам код
           настройки порта. Разбирается - в kx_service. */
        UINT32 port = (ev[0] >> 24) & 0xFFu;

        g_kx.port_events++;

        if (port >= 1 && port <= g_kx.cap.MaxPorts) {

            UINT64 pb = g_kx.op + 0x400u + (UINT64)(port - 1u) * 0x10u;
            UINT32 cur = mmio_read32(pb);
            UINT32 clear = cur & PORTSC_RW1CS_MASK & ~PORTSC_BIT_PRC;

            if (clear)
                mmio_write32(pb, portsc_base_for_write(cur) | clear);

            g_kx.root_change[port] |= 1u;

            if (cur & PORTSC_BIT_CSC)
                g_kx.root_change[port] |= 2u;

            g_kx.any_change = TRUE;
        }

        return;
    }

    g_kx.stray_events++;
}

/* ================================================================
 * Прерывание от контроллера
 * ================================================================ */

void kx_usb_irq(void)
{
    g_kx.irqs++;

    /* USBSTS.EINT (бит 3) и IMAN.IP (бит 0) - "сбросить, записав 1";
       IMAN.IE (бит 1) оставляем включённым */
    mmio_write32(g_kx.op + 0x04, 0x8u);
    mmio_write32(g_kx.intr0 + 0x00, 0x3u);

    g_kx_in_irq = TRUE;
    kx_pump();
    g_kx_in_irq = FALSE;
}

/* ================================================================
 * Настройка конечных точек (Configure Endpoint)
 * ================================================================ */

/* Поле Interval для периодической (interrupt) конечной точки из
   bInterval дескриптора - формат зависит от скорости */
UINT8 kx_interval_field(UINT8 speed, UINT8 b_interval)
{
    if (speed == 3 || speed >= 4) {
        /* HS/SS: bInterval = степень двойки в микрокадрах (+1) */
        UINT8 iv = (b_interval >= 1) ? (UINT8)(b_interval - 1u) : 0;
        return (iv > 15) ? 15 : iv;
    }

    /* LS/FS: bInterval в миллисекундах -> степень двойки в 125 мкс */
    UINT8 v = (b_interval == 0) ? 1 : b_interval;
    UINT8 lg = 0;

    while ((v >> 1) != 0) {
        v = (UINT8)(v >> 1);
        lg++;
    }

    UINT8 iv = (UINT8)(lg + 3u);

    if (iv < 3) iv = 3;
    if (iv > 10) iv = 10;

    return iv;
}

/*
 * Одной командой описать контроллеру все конечные точки устройства.
 * hub_ports != 0 - устройство хаб: отметить это в Slot Context (Hub,
 * Number of Ports, TT Think Time) - без этого контроллер не сможет
 * работать с устройствами за ним.
 */
UINT8 kx_configure_eps(KX_DEV *d, KX_EPCFG *eps, UINTN n,
                       UINT8 hub_ports, UINT8 tt_think)
{
    UINT32 cs = g_kx.ctx_size;
    volatile UINT32 *ictl = (volatile UINT32 *)P2V(d->in_ctx);
    volatile UINT32 *islot = (volatile UINT32 *)P2V(d->in_ctx + cs);
    volatile UINT32 *oslot = (volatile UINT32 *)P2V(d->dev_ctx);
    UINT8 max_dci = 1;

    for (UINTN k = 0; k < n; k++)
        if (eps[k].dci > max_dci)
            max_dci = eps[k].dci;

    raw_zero_mem((volatile UINT8 *)P2V(d->in_ctx), 4096);

    ictl[0] = 0;
    ictl[1] = 0x1u;

    /* Slot Context - копия текущего (его заполнил контроллер),
       Context Entries = старший используемый DCI */
    islot[0] = (oslot[0] & ~(0x1Fu << 27)) | ((UINT32)max_dci << 27);
    islot[1] = oslot[1];
    islot[2] = oslot[2];
    islot[3] = 0;

    if (hub_ports != 0) {
        islot[0] |= (1u << 26);                               /* Hub */
        islot[1] = (islot[1] & 0x00FFFFFFu) | ((UINT32)hub_ports << 24);
        islot[2] = (islot[2] & ~(3u << 16)) | ((UINT32)(tt_think & 3u) << 16);
    }

    for (UINTN k = 0; k < n; k++) {

        KX_EPCFG *e = &eps[k];
        volatile UINT32 *ep = (volatile UINT32 *)P2V(d->in_ctx + (UINT64)(e->dci + 1u) * cs);

        ictl[1] |= (1u << e->dci);

        ep[0] = (UINT32)e->interval << 16;
        ep[1] = (3u << 1) | ((UINT32)e->type << 3) | ((UINT32)e->burst << 8) |
                ((UINT32)e->maxpkt << 16);
        ep[2] = (UINT32)(e->ring & 0xFFFFFFFFu) | 1u;
        ep[3] = (UINT32)(e->ring >> 32);
        ep[4] = (UINT32)e->avg | ((e->esit & 0xFFFFu) << 16);
    }

    return kx_command((UINT32)(d->in_ctx & 0xFFFFFFFFu), (UINT32)(d->in_ctx >> 32), 0,
                      ((UINT32)d->slot << 24) | (12u << 10), NULL);
}

/* ================================================================
 * Перечисление устройства
 * ================================================================ */

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

/* "3" для корневого порта, "3.2" за хабом, "3.2.1" за двумя */
void kx_dev_path(KX_DEV *d, char *buf, UINTN cap)
{
    UINTN n = ksnprintf(buf, cap, "%u", d->root_port);

    for (UINT8 t = 0; t < d->depth && n + 3 < cap; t++) {
        UINT32 p = (d->route >> (4u * t)) & 0xFu;
        n += ksnprintf(buf + n, cap - n, ".%u", p);
    }
}

/*
 * Настроить новое устройство. Порт, в котором оно сидит, уже
 * сброшен и включён.
 *   parent = -1: порт контроллера root_port;
 *   иначе: порт parent_port хаба g_kx_devs[parent].
 * Возвращает индекс устройства или -1.
 */
INTN kx_enum_device(SIMPLE_TEXT_OUTPUT_INTERFACE *out,
                    INTN parent, UINT8 parent_port,
                    UINT8 root_port, UINT8 speed)
{
    KX_DEV *d = NULL;
    INTN di;

    for (di = 0; di < KX_MAX_DEVS; di++) {
        if (!g_kx_devs[di].used) {
            d = &g_kx_devs[di];
            break;
        }
    }

    if (d == NULL) {
        kx_out(out, "    too many USB devices - skipped\n");
        return -1;
    }

    raw_zero_mem((volatile UINT8 *)d, sizeof(*d));

    d->used = TRUE;
    d->speed = speed;
    d->root_port = root_port;
    d->parent = (INT8)parent;
    d->parent_port = parent_port;
    d->hub = -1;
    d->msd = -1;
    d->status = "setup failed";

    if (parent >= 0) {

        KX_DEV *p = &g_kx_devs[parent];
        UINT8 pp = (parent_port > 15) ? 15 : parent_port;

        d->depth = (UINT8)(p->depth + 1u);
        d->route = p->route | ((UINT32)pp << (4u * p->depth));

        /* LS/FS-устройство за HS-хабом: трафик к нему переводит
           Transaction Translator этого хаба */
        if (speed == 1 || speed == 2) {
            if (p->speed == 3) {
                d->tt_slot = p->slot;
                d->tt_port = parent_port;
            } else {
                d->tt_slot = p->tt_slot;
                d->tt_port = p->tt_port;
            }
        }
    }

    /* --- Enable Slot --- */
    UINT8 slot = 0;
    UINT8 cc = kx_command(0, 0, 0, (9u << 10), &slot);

    if (cc != 1 || slot == 0) {
        kx_out(out, "    Enable Slot failed, cc=%u\n", cc);
        d->used = FALSE;
        return -1;
    }

    d->slot = slot;
    d->dev_ctx = kx_dev_page(d);
    d->in_ctx = kx_dev_page(d);
    d->buf = kx_dev_page(d);

    UINT64 ep0_ring = kx_dev_page(d);

    if (!d->dev_ctx || !d->in_ctx || !d->buf || !ep0_ring) {
        kx_out(out, "    out of memory\n");
        kx_remove_device((UINTN)di);
        return -1;
    }

    kx_ring_init(&d->ep0, ep0_ring);

    kx_lock();
    ((volatile UINT64 *)P2V(g_kx.dcbaa))[slot] = d->dev_ctx;
    kx_unlock();

    /* Стартовый Max Packet Size для EP0 (для FS - 64, и сначала
       просим только 8 байт дескриптора: влезает в любой пакет) */
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
    islot[0] = (d->route & 0xFFFFFu) | ((UINT32)speed << 20) | (1u << 27);
    islot[1] = (UINT32)root_port << 16;
    islot[2] = (UINT32)d->tt_slot | ((UINT32)d->tt_port << 8);
    islot[3] = 0;
    iep0[0] = 0;
    iep0[1] = (3u << 1) | (4u << 3) | ((UINT32)mps0 << 16);
    iep0[2] = (UINT32)(ep0_ring & 0xFFFFFFFFu) | 1u;
    iep0[3] = (UINT32)(ep0_ring >> 32);
    iep0[4] = 8;

    /* --- Address Device --- */
    cc = kx_command((UINT32)(d->in_ctx & 0xFFFFFFFFu), (UINT32)(d->in_ctx >> 32), 0,
                    ((UINT32)slot << 24) | (11u << 10), NULL);

    if (cc != 1) {
        kx_out(out, "    Address Device failed, cc=%u\n", cc);
        kx_remove_device((UINTN)di);
        return -1;
    }

    kx_msleep(5);    /* SET_ADDRESS recovery (2 мс по спеке) */

    volatile UINT8 *b = (volatile UINT8 *)P2V(d->buf);

    /* --- первые 8 байт Device Descriptor (ради bMaxPacketSize0) --- */
    cc = kx_control(d, 0x80, 0x06, 0x0100, 0, 8, d->buf);

    if (cc != 1 && cc != 13) {
        kx_out(out, "    GET_DESCRIPTOR(Device, 8) failed, cc=%u\n", cc);
        kx_remove_device((UINTN)di);
        return -1;
    }

    UINT16 real_mps0 = b[7];

    if (speed >= 4)
        real_mps0 = (UINT16)(1u << (b[7] & 0xFu));  /* у USB3 - степень двойки */

    if (real_mps0 >= 8 && real_mps0 != mps0) {

        /* Evaluate Context: обновить MPS у EP0 на настоящий */
        raw_zero_mem((volatile UINT8 *)P2V(d->in_ctx), 4096);

        ictl[1] = 0x2u;
        iep0[1] = (3u << 1) | (4u << 3) | ((UINT32)real_mps0 << 16);
        iep0[2] = (UINT32)(ep0_ring & 0xFFFFFFFFu) | 1u;
        iep0[3] = (UINT32)(ep0_ring >> 32);
        iep0[4] = 8;

        cc = kx_command((UINT32)(d->in_ctx & 0xFFFFFFFFu), (UINT32)(d->in_ctx >> 32), 0,
                        ((UINT32)slot << 24) | (13u << 10), NULL);

        if (cc != 1)
            kx_out(out, "    Evaluate Context failed, cc=%u\n", cc);

        mps0 = real_mps0;
    }

    d->mps0 = mps0;

    /* --- весь Device Descriptor --- */
    cc = kx_control(d, 0x80, 0x06, 0x0100, 0, 18, d->buf);

    if (cc != 1 && cc != 13) {
        kx_out(out, "    GET_DESCRIPTOR(Device) failed\n");
        kx_remove_device((UINTN)di);
        return -1;
    }

    d->vid = (UINT16)(b[8] | (b[9] << 8));
    d->pid = (UINT16)(b[10] | (b[11] << 8));
    d->dclass = b[4];
    d->dprotocol = b[6];

    kx_out(out, "    VID:PID = %04x:%04x, class %u, slot %u, EP0 max packet %u\n",
           d->vid, d->pid, d->dclass, slot, mps0);

    if (d->dclass == 9) {
        kx_hub_setup(out, (UINTN)di);
        return di;
    }

    /* --- Configuration Descriptor: сначала 9 байт (узнать
       полную длину), потом целиком --- */
    UINT64 cfg_phys = d->buf + 1024u;
    volatile UINT8 *cfg = (volatile UINT8 *)P2V(cfg_phys);

    cc = kx_control(d, 0x80, 0x06, 0x0200, 0, 9, cfg_phys);

    if (cc != 1 && cc != 13) {
        kx_out(out, "    GET_DESCRIPTOR(Configuration) failed\n");
        d->status = "no configuration";
        return di;
    }

    UINT16 total = (UINT16)(cfg[2] | (cfg[3] << 8));
    UINT8 cfg_value = cfg[5];

    if (total > 1024u) total = 1024u;
    if (total < 9u) total = 9u;

    cc = kx_control(d, 0x80, 0x06, 0x0200, 0, total, cfg_phys);

    if (cc != 1 && cc != 13) {
        kx_out(out, "    GET_DESCRIPTOR(Configuration, full) failed\n");
        d->status = "no configuration";
        return di;
    }

    /* --- что за интерфейсы: HID (класс 3) и флешки (класс 8) --- */
    KX_HID_CAND cand[KX_MAX_IF_PER_DEV];
    UINTN ncand = 0;
    KX_MSD_CAND msd;
    INTN cur = -1;
    BOOLEAN in_msd = FALSE;
    UINT8 last_bulk = 0;       /* 1 - последняя bulk-точка была IN,
                                  2 - OUT (для SS Companion) */
    UINTN off = 0;

    msd.found = FALSE;

    while (off + 2u <= total) {

        UINT8 dl = cfg[off];
        UINT8 dt = cfg[off + 1];

        if (dl < 2)
            break;

        if (dt == 4 && off + 9u <= total) {

            /* Interface: [2] номер, [3] alt, [5] класс, [6] подкласс,
               [7] протокол */
            cur = -1;
            in_msd = FALSE;

            if (cfg[off + 3] == 0 && cfg[off + 5] == 3 && ncand < KX_MAX_IF_PER_DEV) {

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

            } else if (cfg[off + 3] == 0 && cfg[off + 5] == 8 &&
                       cfg[off + 6] == 6 && cfg[off + 7] == 0x50 && !msd.found) {

                /* Mass Storage, SCSI transparent, Bulk-Only */
                msd.found = TRUE;
                msd.iface = cfg[off + 2];
                msd.in_addr = msd.out_addr = 0;
                msd.in_mps = msd.out_mps = 0;
                msd.in_burst = msd.out_burst = 0;
                in_msd = TRUE;
            }

        } else if (dt == 0x21 && cur >= 0 && off + 9u <= total) {

            if (cfg[off + 6] == 0x22)
                cand[cur].rdesc_len = (UINT16)(cfg[off + 7] | (cfg[off + 8] << 8));

        } else if (dt == 5 && off + 7u <= total) {

            UINT8 a = cfg[off + 2];
            UINT8 at = cfg[off + 3] & 3u;
            UINT16 mp = (UINT16)(cfg[off + 4] | (cfg[off + 5] << 8));

            if (cur >= 0 && !cand[cur].has_ep && (a & 0x80u) && at == 3u) {
                cand[cur].has_ep = TRUE;
                cand[cur].ep_addr = a;
                cand[cur].maxpkt_raw = mp;
                cand[cur].interval = cfg[off + 6];
            } else if (in_msd && at == 2u) {
                last_bulk = 0;
                if ((a & 0x80u) && msd.in_addr == 0) {
                    msd.in_addr = a;
                    msd.in_mps = (UINT16)(mp & 0x7FFu);
                    last_bulk = 1;
                } else if (!(a & 0x80u) && msd.out_addr == 0) {
                    msd.out_addr = a;
                    msd.out_mps = (UINT16)(mp & 0x7FFu);
                    last_bulk = 2;
                }
            }

        } else if (dt == 0x30 && in_msd && off + 3u <= total) {

            /* SuperSpeed Endpoint Companion идёт сразу за своей
               конечной точкой: bMaxBurst - ей */
            if (last_bulk == 1)
                msd.in_burst = cfg[off + 2];
            else if (last_bulk == 2)
                msd.out_burst = cfg[off + 2];
            last_bulk = 0;
        }

        off += dl;
    }

    if (ncand == 0 && !(msd.found && msd.in_addr && msd.out_addr)) {
        kx_out(out, "    not a keyboard, mouse, hub or USB drive - left unconfigured\n");
        d->status = "unused (unsupported class)";
        return di;
    }

    /* --- SET_CONFIGURATION --- */
    cc = kx_control(d, 0x00, 0x09, cfg_value, 0, 0, 0);

    if (cc != 1) {
        kx_out(out, "    SET_CONFIGURATION failed, cc=%u\n", cc);
        return di;
    }

    /* --- настроить интерфейсы: сначала собрать конечные точки --- */
    KX_EPCFG eps[KX_MAX_IF_PER_DEV + 2];
    UINTN neps = 0;
    UINTN pipes[KX_MAX_IF_PER_DEV];
    UINTN npipes = kx_hid_prepare(out, (UINTN)di, cand, ncand, eps, &neps, pipes);
    INTN mi = -1;

    if (msd.found && msd.in_addr && msd.out_addr)
        mi = kx_msd_prepare(out, (UINTN)di, &msd, eps, &neps);

    if (neps == 0) {
        d->status = "nothing usable";
        return di;
    }

    cc = kx_configure_eps(d, eps, neps, 0, 0);

    if (cc != 1) {
        kx_out(out, "    Configure Endpoint failed, cc=%u\n", cc);
        for (UINTN k = 0; k < npipes; k++)
            g_kx_hid[pipes[k]].used = FALSE;
        if (mi >= 0)
            g_kx_msd[mi].used = FALSE;
        d->msd = -1;
        d->status = "Configure Endpoint failed";
        return di;
    }

    /* --- поехали --- */
    kx_hid_start(pipes, npipes);

    d->status = npipes ? "HID, active" : "active";

    if (mi >= 0) {
        kx_msd_start(out, (UINTN)mi);
        d->status = g_kx_msd[mi].ready ? "USB drive, ready" : "USB drive, not ready";
    }

    kx_out(out, "    ready.\n");

    return di;
}

/* ================================================================
 * Отключение
 * ================================================================ */

/* Убрать устройство (и всё, что за ним, если это хаб): остановить
   его трубы, Disable Slot, вернуть память */
void kx_remove_device(UINTN di)
{
    KX_DEV *d = &g_kx_devs[di];

    if (!d->used)
        return;

    kx_lock();

    /* дети хаба - первыми */
    if (d->hub >= 0) {

        KX_HUB *hb = &g_kx_hubs[d->hub];

        for (UINTN p = 1; p <= KX_HUB_MAX_PORTS; p++) {
            if (hb->child[p] >= 0) {
                INT8 c = hb->child[p];
                hb->child[p] = -1;
                kx_remove_device((UINTN)c);
            }
        }

        hb->used = FALSE;
    }

    /* трубы прерываний этого устройства */
    for (UINTN i = 0; i < KX_MAX_HID; i++) {
        if (g_kx_hid[i].used && g_kx_hid[i].dev == di) {
            g_kx_hid[i].used = FALSE;
            g_kx_hid[i].role = KX_ROLE_NONE;
        }
    }

    if (d->msd >= 0)
        g_kx_msd[d->msd].used = FALSE;

    /* у родителя-хаба забыть этого ребёнка */
    if (d->parent >= 0) {
        KX_DEV *p = &g_kx_devs[d->parent];
        if (p->hub >= 0 && d->parent_port <= KX_HUB_MAX_PORTS &&
            g_kx_hubs[p->hub].child[d->parent_port] == (INT8)di)
            g_kx_hubs[p->hub].child[d->parent_port] = -1;
    }

    /* Disable Slot: контроллер забывает устройство */
    if (d->slot != 0) {
        kx_command(0, 0, 0, ((UINT32)d->slot << 24) | (10u << 10), NULL);
        ((volatile UINT64 *)P2V(g_kx.dcbaa))[d->slot] = 0;
    }

    for (UINTN k = 0; k < d->npages; k++)
        pmm_free_pages(d->pages[k], 1);

    d->npages = 0;
    d->used = FALSE;

    kx_hid_update_presence();

    kx_unlock();
}

/* ================================================================
 * Порты контроллера
 * ================================================================ */

/* Устройство, сидящее прямо в корневом порту p (или -1) */
static INTN kx_root_dev(UINTN p)
{
    for (UINTN i = 0; i < KX_MAX_DEVS; i++)
        if (g_kx_devs[i].used && g_kx_devs[i].parent < 0 &&
            g_kx_devs[i].root_port == p)
            return (INTN)i;

    return -1;
}

/* Сбросить корневой порт (если нужно) и настроить устройство в нём */
void kx_root_port_connect(SIMPLE_TEXT_OUTPUT_INTERFACE *out, UINTN p)
{
    UINT64 pb = g_kx.op + 0x400u + (UINT64)(p - 1u) * 0x10u;
    UINT32 sc = mmio_read32(pb);

    if (!(sc & PORTSC_BIT_CCS))
        return;

    kx_out(out, "\n  Port %u: device connected\n", (UINT32)p);

    if (!(sc & PORTSC_BIT_PED)) {

        /* USB2-порту нужен явный Port Reset (USB3 включается
           сам после тренировки линии) */
        mmio_write32(pb, portsc_base_for_write(sc) | PORTSC_BIT_PR);

        BOOLEAN done = FALSE;

        for (UINTN i = 0; i < 500; i++) {

            kx_msleep(1);

            UINT32 s = mmio_read32(pb);

            if (s & PORTSC_BIT_PRC) {
                done = TRUE;
                mmio_write32(pb, portsc_base_for_write(s) | PORTSC_BIT_PRC);
                break;
            }
        }

        /* "восстановление" после сброса (TRSTRCY, 10 мс по спеке) */
        kx_msleep(20);

        sc = mmio_read32(pb);

        if (!done || !(sc & PORTSC_BIT_PED)) {
            kx_out(out, "    port reset failed - skipped\n");
            return;
        }
    }

    UINT8 speed = (UINT8)((sc >> 10) & 0xFu);

    kx_out(out, "    %s\n", kx_speed_name(speed));

    INTN di = kx_enum_device(out, -1, 0, (UINT8)p, speed);

    if (out == NULL && di >= 0)
        kx_event_log("port %u: connected %04x:%04x - %s", (UINT32)p,
                     g_kx_devs[di].vid, g_kx_devs[di].pid, g_kx_devs[di].status);
}

/* ================================================================
 * Обслуживание в основном коде (из kernel_poll_input)
 * ================================================================ */

void kx_service(void)
{
    if (!g_kx.running)
        return;

    kx_lock();

    kx_pump();

    /* корневые порты: подключили / выдернули */
    if (g_kx.any_change) {

        g_kx.any_change = FALSE;

        for (UINTN p = 1; p <= g_kx.cap.MaxPorts && p < 256; p++) {

            UINT8 ch = g_kx.root_change[p];

            if (ch == 0)
                continue;

            g_kx.root_change[p] = 0;

            UINT64 pb = g_kx.op + 0x400u + (UINT64)(p - 1u) * 0x10u;
            UINT32 sc = mmio_read32(pb);
            INTN di = kx_root_dev(p);
            BOOLEAN connected = (sc & PORTSC_BIT_CCS) != 0;

            /* выдернули (или передёрнули: подключение менялось) */
            if (di >= 0 && (!connected || (ch & 2u))) {
                kx_event_log("port %u: disconnected %04x:%04x", (UINT32)p,
                             g_kx_devs[di].vid, g_kx_devs[di].pid);
                kx_remove_device((UINTN)di);
                g_kx.hot_removed++;
                di = -1;
            }

            if (connected && di < 0) {
                /* дребезг контактов: дать устройству "сесть" */
                kx_msleep(100);
                g_kx.hot_added++;
                kx_root_port_connect(NULL, p);
            }
        }
    }

    /* хабы: что-то изменилось на их портах */
    kx_hub_service();

    /* сторож восстановления конечных точек */
    kx_pipes_watchdog();

    /* светодиоды клавиатур */
    kx_hid_service_leds();

    kx_unlock();
}

/* ================================================================
 * Запуск драйвера
 * ================================================================ */

void kx_usb_start(SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    g_kx.irq_mode = "polling";

    for (UINTN i = 0; i < KX_MAX_HUBS; i++)
        g_kx_hubs[i].used = FALSE;

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
    g_kx.scratchpads = (((hcs2 >> 21) & 0x1Fu) << 5) | ((hcs2 >> 27) & 0x1Fu);

    kprintf(out, "  xHCI at %u:%u.%u, %u ports, context size %u, scratchpad buffers %u\n",
            g_kx.bus, g_kx.devn, g_kx.func, g_kx.cap.MaxPorts,
            g_kx.ctx_size, g_kx.scratchpads);

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

    /* Scratchpad Buffer Array: DCBAA[0] -> массив адресов страниц,
       каждая страница - в распоряжении контроллера */
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

    /* Event Ring: порядок по спеке - ERSTSZ, ERDP, ERSTBA */
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

    /* Дать устройствам время заново "появиться" после сброса */
    kx_msleep(200);

    print(out, "  xHCI running. Scanning root ports...\n");

    for (UINTN p = 1; p <= g_kx.cap.MaxPorts; p++)
        kx_root_port_connect(out, p);

    /* события подключения, накопившиеся за перечисление, - уже
       обработаны; забыть о них */
    kx_lock();
    kx_pump();
    for (UINTN p = 0; p < 256; p++)
        g_kx.root_change[p] = 0;
    g_kx.any_change = FALSE;
    kx_unlock();

    /* --- прерывания: MSI -> вектор KX_VEC_XHCI --- */
    kx_irq_register(KX_VEC_XHCI, kx_usb_irq);

    const char *mode = kx_pci_enable_msi(g_kx.bus, g_kx.devn, g_kx.func, KX_VEC_XHCI);

    if (mode != NULL) {

        /* IMOD: не чаще раза в 250 мкс (1000 x 250 нс) - мышь на
           1000 Гц этим не тормозится, а шквал событий не душит ядро */
        mmio_write32(g_kx.intr0 + 0x04, 1000u);
        /* IMAN: IE (бит 1) включить, IP (бит 0) сбросить */
        mmio_write32(g_kx.intr0 + 0x00, 0x3u);
        /* USBCMD.INTE (бит 2) */
        mmio_write32(g_kx.op + 0x00, mmio_read32(g_kx.op + 0x00) | 0x4u);

        g_kx.irq_mode = mode;
    }

    UINTN kbds = 0, mice = 0, hubs = 0, disks = 0;

    for (UINTN i = 0; i < KX_MAX_HID; i++) {
        if (!g_kx_hid[i].used)
            continue;
        if (g_kx_hid[i].role == KX_ROLE_KBD_BOOT)
            kbds++;
        else if (g_kx_hid[i].role == KX_ROLE_HUB)
            hubs++;
        else if (g_kx_hid[i].role != KX_ROLE_NONE)
            mice++;
    }

    for (UINTN i = 0; i < KX_MAX_MSD; i++)
        if (g_kx_msd[i].used)
            disks++;

    kprintf(out, "\n  USB summary: %u keyboard(s), %u mouse/pointer interface(s), "
                 "%u hub(s), %u drive(s)\n",
            (UINT32)kbds, (UINT32)mice, (UINT32)hubs, (UINT32)disks);
    kprintf(out, "  USB events: %s%s\n", g_kx.irq_mode,
            mode ? " interrupts (the controller signals by itself)" :
                   " - no MSI, the driver asks the controller regularly");
}

/* Опросить ВСЕ источники ввода. Зовётся нашими ConIn/SimplePointer
   при каждом обращении шелла/GUI за вводом. С прерываниями это
   почти ничего не стоит: события уже разобраны обработчиком, здесь -
   только то, что требует ожидания (подключения, светодиоды). */
void kernel_poll_input(void)
{
    kx_service();

    kx_lock();
    ps2_poll();
    kbd_repeat_tick();
    kx_unlock();
}
