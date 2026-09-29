/*
 * kernel/acpi_os.c - то, что библиотека uACPI просит у ядра (этап 9).
 * Часть MyOS; общие объявления - в myos.h.
 *
 * uACPI (third_party/uacpi) - готовый интерпретатор байт-кода ACPI
 * (AML). Он исполняет программы, которые прошивка ноутбука оставила
 * в таблицах DSDT/SSDT: "сколько заряда в батарее" (_BST), "открыта
 * ли крышка" (_LID), "подготовиться к выключению" (_PTS)... Сам он
 * ничего не знает о нашем ядре и зовёт функции uacpi_kernel_*:
 * отобразить физическую память, прочитать порт, взять замок, уснуть,
 * повесить обработчик на прерывание. Здесь - они все, по порядку
 * include/uacpi/kernel_api.h. Всё остальное (батарея, кнопка питания,
 * контроллер EC) - в kernel/acpi_dev.c.
 *
 * Правила uACPI, о которых стоит помнить:
 *   * замки (mutex) и ожидание событий зовутся только из потоков -
 *     не из прерывания; спин-замки и signal_event - откуда угодно;
 *   * uacpi_kernel_schedule_work зовётся и ИЗ ПРЕРЫВАНИЯ (SCI), а
 *     саму работу (метод AML _Lxx, Notify) надо сделать потом, в
 *     обычном потоке - у нас это поток "acpi".
 */
#include "myos.h"
#include <uacpi/kernel_api.h>
#include <uacpi/status.h>

/* ================================================================
 * Таблицы и память
 * ================================================================ */

uacpi_status uacpi_kernel_get_rsdp(uacpi_phys_addr *out_rsdp_address)
{
    /* адрес RSDP нам дал загрузчик (kernel/acpi.c уже проверил его) */
    if (!g_acpi.present || g_acpi.rsdp == 0)
        return UACPI_STATUS_NOT_FOUND;

    *out_rsdp_address = g_acpi.rsdp;
    return UACPI_STATUS_OK;
}

/*
 * "Отобразить" физическую память. У ядра вся RAM и так видна по
 * адресу P2V(phys) - там её и отдаём. Если страницы там ещё нет,
 * это не RAM, а регистры устройства (AML любит поля в памяти
 * чипсета - OperationRegion SystemMemory): такие страницы
 * отображаем некэшируемыми. uACPI пишет, например, в FACS (флаг
 * "глобального замка") и в поля регионов - поэтому только
 * запись разрешена (vmm_ensure_writable).
 */
void *uacpi_kernel_map(uacpi_phys_addr addr, uacpi_size len)
{
    if (len == 0)
        len = 1;

    if (!vmm_ensure_writable(addr, len, VMM_UC))
        return NULL;

    return P2V(addr);
}

void uacpi_kernel_unmap(void *addr, uacpi_size len)
{
    /* отображение общее на всё ядро - снимать нечего */
    (void)addr;
    (void)len;
}

void *uacpi_kernel_alloc(uacpi_size size)
{
    return kmalloc(size);
}

void *uacpi_kernel_alloc_zeroed(uacpi_size size)
{
    return kzalloc(size);
}

void uacpi_kernel_free(void *mem)
{
    if (mem != NULL)
        kfree(mem);
}

/* ================================================================
 * Сообщения
 * ================================================================ */

void uacpi_kernel_log(uacpi_log_level lvl, const uacpi_char *msg)
{
    static const char *const names[] = { "?", "error", "warn", "info", "trace", "debug" };
    const char *n = (lvl >= 1 && lvl <= 5) ? names[lvl] : names[0];

    /* msg уже заканчивается переводом строки */
    klog("uacpi %s: %s", n, msg);

    if (lvl <= UACPI_LOG_WARN)
        acpi_dev_note_log(lvl == UACPI_LOG_ERROR, msg);
}

uacpi_status uacpi_kernel_handle_firmware_request(uacpi_firmware_request *req)
{
    /* Breakpoint и Fatal - операторы AML "для отладки прошивки".
       Fatal означает "прошивка считает, что всё плохо"; выключать
       машину из-за этого мы не станем - только запишем. */
    if (req->type == UACPI_FIRMWARE_REQUEST_TYPE_FATAL)
        klog("uacpi: AML Fatal(type %u, code 0x%x, arg 0x%llx)\n",
             (UINT32)req->fatal.type, req->fatal.code,
             (unsigned long long)req->fatal.arg);
    else
        klog("uacpi: AML Breakpoint\n");

    return UACPI_STATUS_OK;
}

/* ================================================================
 * Конфигурация PCI. У нас один сегмент (0) и 256 байт на функцию
 * (pci_config_read32 - старые порты или ECAM); поля AML дальше 256
 * байт - редкость, для них честно отвечаем "не умею".
 * ================================================================ */

typedef struct {
    UINT8 bus, dev, fn;
} ACPI_PCI_DEV;

uacpi_status uacpi_kernel_pci_device_open(uacpi_pci_address address, uacpi_handle *out_handle)
{
    if (address.segment != 0)
        return UACPI_STATUS_UNIMPLEMENTED;

    ACPI_PCI_DEV *d = kmalloc(sizeof(*d));

    if (d == NULL)
        return UACPI_STATUS_OUT_OF_MEMORY;

    d->bus = address.bus;
    d->dev = address.device;
    d->fn = address.function;

    *out_handle = d;
    return UACPI_STATUS_OK;
}

void uacpi_kernel_pci_device_close(uacpi_handle h)
{
    kfree(h);
}

/* прочитать width байт по смещению off: через выровненное 32-битное
   слово (ширина и выравнивание гарантированы uACPI) */
static uacpi_status acpi_pci_read(uacpi_handle h, uacpi_size off, UINTN width, UINT32 *out)
{
    ACPI_PCI_DEV *d = h;

    if (off + width > 256u)
        return UACPI_STATUS_UNIMPLEMENTED;

    UINT32 v = pci_config_read32(d->bus, d->dev, d->fn, (UINT8)(off & ~3u));
    UINT32 sh = (UINT32)(off & 3u) * 8u;

    v >>= sh;

    if (width == 1) v &= 0xFFu;
    if (width == 2) v &= 0xFFFFu;

    *out = v;
    return UACPI_STATUS_OK;
}

/* записать width байт: для 1 и 2 байт - прочитать слово, поменять
   свою часть, записать обратно. У настоящего чипсета побочные
   эффекты от записи соседних байт "тем же значением" бывают лишь у
   регистров статуса (бит сбрасывается записью 1) - AML в них через
   такие поля не пишет. */
static uacpi_status acpi_pci_write(uacpi_handle h, uacpi_size off, UINTN width, UINT32 val)
{
    ACPI_PCI_DEV *d = h;

    if (off + width > 256u)
        return UACPI_STATUS_UNIMPLEMENTED;

    UINT8 base = (UINT8)(off & ~3u);

    if (width == 4) {
        pci_config_write32(d->bus, d->dev, d->fn, base, val);
        return UACPI_STATUS_OK;
    }

    UINT32 mask = (width == 1) ? 0xFFu : 0xFFFFu;
    UINT32 sh = (UINT32)(off & 3u) * 8u;
    UINT32 v = pci_config_read32(d->bus, d->dev, d->fn, base);

    v = (v & ~(mask << sh)) | ((val & mask) << sh);
    pci_config_write32(d->bus, d->dev, d->fn, base, v);

    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_read8(uacpi_handle h, uacpi_size off, uacpi_u8 *v)
{
    UINT32 x = 0;
    uacpi_status st = acpi_pci_read(h, off, 1, &x);
    *v = (uacpi_u8)x;
    return st;
}

uacpi_status uacpi_kernel_pci_read16(uacpi_handle h, uacpi_size off, uacpi_u16 *v)
{
    UINT32 x = 0;
    uacpi_status st = acpi_pci_read(h, off, 2, &x);
    *v = (uacpi_u16)x;
    return st;
}

uacpi_status uacpi_kernel_pci_read32(uacpi_handle h, uacpi_size off, uacpi_u32 *v)
{
    UINT32 x = 0;
    uacpi_status st = acpi_pci_read(h, off, 4, &x);
    *v = x;
    return st;
}

uacpi_status uacpi_kernel_pci_write8(uacpi_handle h, uacpi_size off, uacpi_u8 v)
{
    return acpi_pci_write(h, off, 1, v);
}

uacpi_status uacpi_kernel_pci_write16(uacpi_handle h, uacpi_size off, uacpi_u16 v)
{
    return acpi_pci_write(h, off, 2, v);
}

uacpi_status uacpi_kernel_pci_write32(uacpi_handle h, uacpi_size off, uacpi_u32 v)
{
    return acpi_pci_write(h, off, 4, v);
}

/* ================================================================
 * Порты ввода-вывода: "ручка" - просто номер первого порта
 * ================================================================ */

uacpi_status uacpi_kernel_io_map(uacpi_io_addr base, uacpi_size len, uacpi_handle *out_handle)
{
    if (base + len > 0x10000u)
        return UACPI_STATUS_INVALID_ARGUMENT;

    *out_handle = (uacpi_handle)(UINTN)base;
    return UACPI_STATUS_OK;
}

void uacpi_kernel_io_unmap(uacpi_handle h)
{
    (void)h;
}

#define ACPI_PORT(h, off) ((UINT16)((UINTN)(h) + (off)))

uacpi_status uacpi_kernel_io_read8(uacpi_handle h, uacpi_size off, uacpi_u8 *v)
{
    *v = io_in8(ACPI_PORT(h, off));
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_read16(uacpi_handle h, uacpi_size off, uacpi_u16 *v)
{
    *v = io_in16(ACPI_PORT(h, off));
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_read32(uacpi_handle h, uacpi_size off, uacpi_u32 *v)
{
    *v = io_in32(ACPI_PORT(h, off));
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write8(uacpi_handle h, uacpi_size off, uacpi_u8 v)
{
    io_out8(ACPI_PORT(h, off), v);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write16(uacpi_handle h, uacpi_size off, uacpi_u16 v)
{
    io_out16(ACPI_PORT(h, off), v);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write32(uacpi_handle h, uacpi_size off, uacpi_u32 v)
{
    io_out32(ACPI_PORT(h, off), v);
    return UACPI_STATUS_OK;
}

/* ================================================================
 * Время
 * ================================================================ */

uacpi_u64 uacpi_kernel_get_nanoseconds_since_boot(void)
{
    /* uACPI требует "строго растёт": микросекунды TSC * 1000 плюс
       счётчик вызовов внутри одной микросекунды */
    static UINT64 last;
    UINT64 ns = kx_uptime_us() * 1000u;

    if (ns <= last)
        ns = last + 1u;

    last = ns;
    return ns;
}

void uacpi_kernel_stall(uacpi_u8 usec)
{
    /* Stall в AML - "подождать, НЕ отдавая процессор" (зовут и при
       запрещённых прерываниях): только крутимся */
    tsc_delay_us(usec);
}

void uacpi_kernel_sleep(uacpi_u64 msec)
{
    if (sched_can_block())
        sched_sleep_ms(msec);
    else
        busy_wait_ms((UINTN)msec);
}

/* ================================================================
 * Замки и события
 * ================================================================ */

/* Мьютекс uACPI - НЕ рекурсивный и с таймаутом, поэтому не наш
   KMUTEX, а своя маленькая структура на тех же sched_block/wake */
typedef struct {
    KTHREAD *owner;
} ACPI_MUTEX;

typedef struct {
    volatile UINT32 count;
} ACPI_EVENT;

uacpi_handle uacpi_kernel_create_mutex(void)
{
    return kzalloc(sizeof(ACPI_MUTEX));
}

void uacpi_kernel_free_mutex(uacpi_handle h)
{
    kfree(h);
}

uacpi_handle uacpi_kernel_create_event(void)
{
    return kzalloc(sizeof(ACPI_EVENT));
}

void uacpi_kernel_free_event(uacpi_handle h)
{
    kfree(h);
}

uacpi_thread_id uacpi_kernel_get_thread_id(void)
{
    return g_kcur;
}

uacpi_interrupt_state uacpi_kernel_disable_interrupts(void)
{
    return (uacpi_interrupt_state)kx_irq_save();
}

void uacpi_kernel_restore_interrupts(uacpi_interrupt_state state)
{
    kx_irq_restore((UINT64)state);
}

/* Общая часть ожидания: сколько ещё можно спать (мс); 0xFFFF -
   вечно (sched_block понимает 0 как "без таймаута") */
static BOOLEAN acpi_wait_left(UINT16 timeout, UINT64 start_us, UINT64 *left_ms)
{
    if (timeout == 0xFFFFu) {
        *left_ms = 0;
        return TRUE;
    }

    UINT64 spent = (kx_uptime_us() - start_us) / 1000u;

    if (spent >= timeout)
        return FALSE;

    *left_ms = timeout - spent;
    return TRUE;
}

uacpi_status uacpi_kernel_acquire_mutex(uacpi_handle h, uacpi_u16 timeout)
{
    ACPI_MUTEX *m = h;
    UINT64 start = kx_uptime_us();
    UINT64 fl = kx_irq_save();

    while (m->owner != NULL) {

        UINT64 left = 0;

        /* не можем спать (ещё нет потоков или запрещены
           прерывания перед выключением) - ждать некого */
        if (timeout == 0 || !sched_can_block() ||
            !acpi_wait_left(timeout, start, &left)) {
            kx_irq_restore(fl);
            return UACPI_STATUS_TIMEOUT;
        }

        sched_block(m, "acpi mutex", left);
    }

    m->owner = g_kcur;
    kx_irq_restore(fl);

    return UACPI_STATUS_OK;
}

void uacpi_kernel_release_mutex(uacpi_handle h)
{
    ACPI_MUTEX *m = h;
    UINT64 fl = kx_irq_save();

    m->owner = NULL;
    sched_wake_all(m);

    kx_irq_restore(fl);
}

uacpi_bool uacpi_kernel_wait_for_event(uacpi_handle h, uacpi_u16 timeout)
{
    ACPI_EVENT *e = h;
    UINT64 start = kx_uptime_us();
    UINT64 fl = kx_irq_save();

    while (e->count == 0) {

        UINT64 left = 0;

        if (timeout == 0 || !acpi_wait_left(timeout, start, &left)) {
            kx_irq_restore(fl);
            return UACPI_FALSE;
        }

        if (sched_can_block()) {
            sched_block(e, "acpi event", left);
        } else {
            /* без потоков - подождать чуть-чуть и проверить снова */
            kx_irq_restore(fl);
            busy_wait_ms(1);
            fl = kx_irq_save();
        }
    }

    e->count--;
    kx_irq_restore(fl);

    return UACPI_TRUE;
}

void uacpi_kernel_signal_event(uacpi_handle h)
{
    ACPI_EVENT *e = h;
    UINT64 fl = kx_irq_save();

    e->count++;
    sched_wake_all(e);

    kx_irq_restore(fl);
}

void uacpi_kernel_reset_event(uacpi_handle h)
{
    ACPI_EVENT *e = h;
    e->count = 0;
}

uacpi_handle uacpi_kernel_create_spinlock(void)
{
    return kzalloc(sizeof(KSPINLOCK));
}

void uacpi_kernel_free_spinlock(uacpi_handle h)
{
    kfree(h);
}

uacpi_cpu_flags uacpi_kernel_lock_spinlock(uacpi_handle h)
{
    return (uacpi_cpu_flags)kspin_lock(h);
}

void uacpi_kernel_unlock_spinlock(uacpi_handle h, uacpi_cpu_flags fl)
{
    kspin_unlock(h, (UINT64)fl);
}

/* ================================================================
 * Прерывание SCI
 *
 * ACPI сообщает о событиях (нажата кнопка питания, EC хочет
 * внимания, вставили зарядку) одним прерыванием - SCI. Его номер
 * (обычно IRQ 9) - в FADT; uACPI просит "повесь обработчик на
 * IRQ n". По стандарту SCI - "по уровню, активный низкий", если
 * MADT не переназначила иначе.
 * ================================================================ */

static uacpi_interrupt_handler g_sci_fn;
static uacpi_handle g_sci_ctx;
static UINT32 g_sci_gsi;
UINT64 g_acpi_sci_count;

static void acpi_sci_irq(void)
{
    g_acpi_sci_count++;

    if (g_sci_fn != NULL)
        g_sci_fn(g_sci_ctx);
}

uacpi_status uacpi_kernel_install_interrupt_handler(
    uacpi_u32 irq, uacpi_interrupt_handler fn, uacpi_handle ctx,
    uacpi_handle *out_irq_handle)
{
    if (g_sci_fn != NULL)
        return UACPI_STATUS_ALREADY_EXISTS;

    /* режим линии: из переназначения MADT (флаги "как у шины" для
       SCI означают уровень и активный низкий) */
    UINT32 gsi = irq;
    BOOLEAN level = TRUE, low = TRUE;

    for (UINTN i = 0; i < g_acpi.nisos; i++) {

        ACPI_ISO *s = &g_acpi.isos[i];

        if (s->bus != 0 || s->irq != irq)
            continue;

        gsi = s->gsi;

        if ((s->flags & 3u) == 1u)
            low = FALSE;
        if (((s->flags >> 2) & 3u) == 1u)
            level = FALSE;
        break;
    }

    g_sci_fn = fn;
    g_sci_ctx = ctx;
    g_sci_gsi = gsi;

    kx_irq_register(KX_VEC_ACPI, acpi_sci_irq);

    if (!kx_ioapic_route(gsi, KX_VEC_ACPI, level, low)) {
        kx_irq_register(KX_VEC_ACPI, NULL);
        g_sci_fn = NULL;
        return UACPI_STATUS_INTERNAL_ERROR;
    }

    klog("acpi: SCI irq %u -> gsi %u (%s, active %s), vector 0x%x\n",
         irq, gsi, level ? "level" : "edge", low ? "low" : "high", KX_VEC_ACPI);

    *out_irq_handle = (uacpi_handle)(UINTN)(gsi + 1u);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_uninstall_interrupt_handler(
    uacpi_interrupt_handler fn, uacpi_handle irq_handle)
{
    (void)fn;
    (void)irq_handle;

    kx_ioapic_mask(g_sci_gsi);
    kx_irq_register(KX_VEC_ACPI, NULL);
    g_sci_fn = NULL;

    return UACPI_STATUS_OK;
}

/* ================================================================
 * Отложенная работа: поток "acpi"
 *
 * Из прерывания SCI uACPI просит "исполни метод _L6F" или "передай
 * Notify(BAT0, 0x80)" - это может занять миллисекунды (EC отвечает
 * медленно), в прерывании так нельзя. Кладём в кольцевую очередь,
 * поток acpi исполняет по одному.
 * ================================================================ */

#define ACPI_WORK_MAX 64

typedef struct {
    uacpi_work_handler fn;
    uacpi_handle ctx;
} ACPI_WORK;

static ACPI_WORK g_work[ACPI_WORK_MAX];
static volatile UINT32 g_work_head, g_work_tail;   /* head - кто следующий */
static volatile BOOLEAN g_work_busy;
static KTHREAD *g_work_thread;
UINT64 g_acpi_work_done, g_acpi_work_lost;

static void acpi_work_thread(void *arg)
{
    (void)arg;

    for (;;) {

        UINT64 fl = kx_irq_save();

        while (g_work_head == g_work_tail) {
            g_work_busy = FALSE;
            sched_wake_all((const void *)&g_work_busy);    /* кто ждёт "всё сделано" */
            sched_block((const void *)&g_work_tail, "acpi work", 0);
        }

        ACPI_WORK w = g_work[g_work_head % ACPI_WORK_MAX];
        g_work_head++;
        g_work_busy = TRUE;

        kx_irq_restore(fl);

        w.fn(w.ctx);
        g_acpi_work_done++;
    }
}

uacpi_status uacpi_kernel_schedule_work(uacpi_work_type type, uacpi_work_handler fn, uacpi_handle ctx)
{
    (void)type;   /* у нас одно ядро процессора - "на CPU0" и так */

    UINT64 fl = kx_irq_save();

    if (g_work_tail - g_work_head >= ACPI_WORK_MAX) {
        g_acpi_work_lost++;
        kx_irq_restore(fl);
        return UACPI_STATUS_OUT_OF_MEMORY;
    }

    g_work[g_work_tail % ACPI_WORK_MAX].fn = fn;
    g_work[g_work_tail % ACPI_WORK_MAX].ctx = ctx;
    g_work_tail++;
    g_work_busy = TRUE;

    sched_wake_all((const void *)&g_work_tail);
    kx_irq_restore(fl);

    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_wait_for_work_completion(void)
{
    UINT64 fl = kx_irq_save();

    while (g_work_busy || g_work_head != g_work_tail) {

        if (!sched_can_block() || g_kcur == g_work_thread)
            break;

        sched_block((const void *)&g_work_busy, "acpi work done", 50);
    }

    kx_irq_restore(fl);
    return UACPI_STATUS_OK;
}

/* Запустить поток acpi - до uacpi_initialize (события могут прийти
   сразу после включения SCI) */
void acpi_os_start(void)
{
    if (g_work_thread == NULL)
        g_work_thread = kthread_create("acpi", acpi_work_thread, NULL, 16);
}
