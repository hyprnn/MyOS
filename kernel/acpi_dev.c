/*
 * kernel/acpi_dev.c - устройства ACPI через uACPI: батарея, блок
 * питания, крышка, кнопка питания, контроллер EC, выключение (этап 9).
 * Часть MyOS; общие объявления - в myos.h.
 *
 * Как это устроено у ноутбука. Батарея - не устройство на шине, к
 * которому можно написать драйвер: её "знает" маленький процессор
 * на плате, Embedded Controller (EC). Прошивка описывает в таблицах
 * ACPI (байт-код AML) устройство BAT0 с методами:
 *   _STA - есть ли батарея;
 *   _BIF/_BIX - паспорт: ёмкость, напряжение, модель;
 *   _BST - состояние: заряжается ли, сколько осталось, ток.
 * Внутри эти методы читают поля EC ("OperationRegion
 * EmbeddedControl"). Исполнять AML умеет uACPI; а читать EC она
 * просит нас - ниже драйвер EC (два порта: данные 0x62 и
 * команды/состояние 0x66, протокол из спецификации ACPI, глава 12).
 *
 * События: кнопка питания - либо "фиксированное событие" (бит в
 * регистре PM1), либо устройство PNP0C0C с Notify(0x80). Батарея,
 * зарядка, крышка присылают Notify через EC: EC поднимает бит
 * SCI_EVT -> прерывание GPE -> мы спрашиваем у EC номер запроса
 * (команда 0x84) -> исполняем метод _Qxx из AML -> он делает
 * Notify(BAT0, 0x80). Всё это приходит нам в обработчики ниже.
 *
 * Поток "power": раз в 15 секунд (и сразу после Notify) перечитывает
 * батарею; значения лежат в g_acpid - их показывают команда battery
 * и значок на панели задач. Нажата кнопка питания - он же выключает
 * машину (как Linux и Windows по умолчанию).
 *
 * Крышка (этап 10, В): закрыли - подсветка гаснет, открыли - прежняя
 * яркость. Крышка присылает Notify(LID0, 0x80), но не у всех ноутбуков
 * эта цепочка через EC работает, поэтому поток power ещё и раз в
 * секунду спрашивает _LID сам (это быстро: один метод AML).
 */
#include "myos.h"
#include <uacpi/uacpi.h>
#include <uacpi/event.h>
#include <uacpi/notify.h>
#include <uacpi/opregion.h>
#include <uacpi/resources.h>
#include <uacpi/sleep.h>
#include <uacpi/tables.h>
#include <uacpi/utilities.h>
#include <uacpi/acpi.h>

ACPI_DEVS g_acpid;

/* узлы AML найденных устройств */
static uacpi_namespace_node *g_bat_node[ACPI_MAX_BATTERIES];
static uacpi_namespace_node *g_ac_node;
static uacpi_namespace_node *g_lid_node;
static uacpi_namespace_node *g_ec_node;

/* AML исполняет в каждый момент один поток: и опрос батареи (поток
   power), и команда battery из шелла. Замок - вокруг обращений к AML
   из нашего кода (сама uACPI внутри тоже под своими замками). */
static KMUTEX g_acpid_mutex = KMUTEX_INIT("acpi dev");

/* поток power ждёт этого "события" (адрес - как номер) */
static volatile UINT32 g_power_kick;
static volatile BOOLEAN g_power_button_pending;
static volatile BOOLEAN g_bat_dirty;
static KTHREAD *g_power_thread;

/* крышка: "lid test close|open" подменяет ответ _LID (0 - не
   подменять), чтобы проверить реакцию без настоящей крышки (в QEMU) */
static volatile UINT32 g_lid_test;      /* 0 нет, 1 закрыта, 2 открыта */
static UINT32 g_lid_notifies, g_lid_polls;

/* uACPI сообщила о предупреждении/ошибке (acpi_os.c) - запомнить
   последнее, его покажет battery */
void acpi_dev_note_log(BOOLEAN error, const char *msg)
{
    if (error)
        g_acpid.n_errors++;
    else
        g_acpid.n_warnings++;

    UINTN i = 0;

    for (; msg[i] && msg[i] != '\n' && i + 1 < sizeof(g_acpid.last_msg); i++)
        g_acpid.last_msg[i] = msg[i];

    g_acpid.last_msg[i] = '\0';
}

static void power_kick(void)
{
    UINT64 fl = kx_irq_save();

    g_power_kick++;
    sched_wake_all((const void *)&g_power_kick);

    kx_irq_restore(fl);
}

/* ================================================================
 * Контроллер EC (Embedded Controller)
 *
 * Порт команд при ЧТЕНИИ отдаёт байт состояния:
 *   бит 0 OBF - "в порту данных лежит ответ для тебя";
 *   бит 1 IBF - "я ещё не забрал то, что ты записал - подожди";
 *   бит 5 SCI_EVT - "у меня есть событие, спроси (0x84)".
 * Команды: 0x80 - прочитать байт, 0x81 - записать, 0x84 - запрос.
 * Каждый шаг - дождаться нужного бита. EC медленный (микроконтроллер
 * на 8051-подобном ядре), ответ бывает через сотни микросекунд.
 * ================================================================ */

#define EC_OBF      0x01u
#define EC_IBF      0x02u
#define EC_SCI_EVT  0x20u

#define EC_CMD_READ   0x80u
#define EC_CMD_WRITE  0x81u
#define EC_CMD_QUERY  0x84u

static KMUTEX g_ec_mutex = KMUTEX_INIT("ec");

/* EC не отвечает несколько раз подряд - считаем его "мёртвым" и дальше
   сразу отвечаем AML ошибкой: иначе каждое чтение поля ждало бы
   полсекунды, а методы прошивки читают их десятками (загрузка
   растянулась бы на минуты) */
static UINT32 g_ec_fail_row;
static BOOLEAN g_ec_dead;

static BOOLEAN ec_wait(UINT8 mask, BOOLEAN want_set)
{
    UINT64 start = kx_uptime_us();

    for (;;) {

        UINT8 st = io_in8(g_acpid.ec_cmd);

        if (((st & mask) != 0) == want_set) {
            g_ec_fail_row = 0;
            return TRUE;
        }

        /* 500 мс - с огромным запасом (обычно < 1 мс) */
        if (kx_uptime_us() - start > 500000u) {
            g_acpid.ec_timeouts++;
            if (++g_ec_fail_row >= 3 && !g_ec_dead) {
                g_ec_dead = TRUE;
                klog("acpi: EC does not answer (status 0x%x) - giving up on it\n", st);
            }
            return FALSE;
        }

        tsc_delay_us(5);
    }
}

/* выбросить случайно оставшийся в порту данных байт (например, ответ
   на прерванную команду) - иначе следующий ответ перепутается */
static void ec_drain(void)
{
    for (UINTN i = 0; i < 16 && (io_in8(g_acpid.ec_cmd) & EC_OBF); i++)
        (void)io_in8(g_acpid.ec_data);
}

static BOOLEAN ec_read_byte(UINT8 addr, UINT8 *out)
{
    ec_drain();

    if (!ec_wait(EC_IBF, FALSE)) return FALSE;
    io_out8(g_acpid.ec_cmd, EC_CMD_READ);
    if (!ec_wait(EC_IBF, FALSE)) return FALSE;
    io_out8(g_acpid.ec_data, addr);
    if (!ec_wait(EC_OBF, TRUE)) return FALSE;
    *out = io_in8(g_acpid.ec_data);

    g_acpid.ec_reads++;
    return TRUE;
}

static BOOLEAN ec_write_byte(UINT8 addr, UINT8 v)
{
    ec_drain();

    if (!ec_wait(EC_IBF, FALSE)) return FALSE;
    io_out8(g_acpid.ec_cmd, EC_CMD_WRITE);
    if (!ec_wait(EC_IBF, FALSE)) return FALSE;
    io_out8(g_acpid.ec_data, addr);
    if (!ec_wait(EC_IBF, FALSE)) return FALSE;
    io_out8(g_acpid.ec_data, v);
    if (!ec_wait(EC_IBF, FALSE)) return FALSE;

    g_acpid.ec_writes++;
    return TRUE;
}

/* Глобальный замок ACPI (_GLK = 1): EC делят ОС и прошивка (SMM) -
   прошивка тоже может в этот момент говорить с EC */
static BOOLEAN ec_lock(uacpi_u32 *seq)
{
    kmutex_lock(&g_ec_mutex);

    if (g_acpid.ec_glk &&
        uacpi_acquire_global_lock(0xFFFFu, seq) != UACPI_STATUS_OK) {
        kmutex_unlock(&g_ec_mutex);
        return FALSE;
    }

    return TRUE;
}

static void ec_unlock(uacpi_u32 seq)
{
    if (g_acpid.ec_glk)
        uacpi_release_global_lock(seq);

    kmutex_unlock(&g_ec_mutex);
}

/* Обработчик "адресного пространства EmbeddedControl": AML читает
   или пишет поле EC шириной 1..8 байт по адресу 0..255 */
static uacpi_status ec_region_handler(uacpi_region_op op, uacpi_handle data)
{
    if (op == UACPI_REGION_OP_ATTACH || op == UACPI_REGION_OP_DETACH)
        return UACPI_STATUS_OK;

    if (op != UACPI_REGION_OP_READ && op != UACPI_REGION_OP_WRITE)
        return UACPI_STATUS_INVALID_ARGUMENT;

    uacpi_region_rw_data *rw = data;
    uacpi_u32 seq = 0;
    BOOLEAN ok = TRUE;

    if (rw->byte_width == 0 || rw->byte_width > 8 || rw->offset + rw->byte_width > 256u)
        return UACPI_STATUS_INVALID_ARGUMENT;

    if (g_ec_dead)
        return UACPI_STATUS_HARDWARE_TIMEOUT;

    if (!ec_lock(&seq))
        return UACPI_STATUS_TIMEOUT;

    if (op == UACPI_REGION_OP_READ) {

        UINT64 v = 0;

        for (UINTN i = 0; i < rw->byte_width && ok; i++) {
            UINT8 b = 0;
            ok = ec_read_byte((UINT8)(rw->offset + i), &b);
            v |= (UINT64)b << (8u * i);
        }

        rw->value = v;

    } else {

        for (UINTN i = 0; i < rw->byte_width && ok; i++)
            ok = ec_write_byte((UINT8)(rw->offset + i), (UINT8)(rw->value >> (8u * i)));
    }

    ec_unlock(seq);

    return ok ? UACPI_STATUS_OK : UACPI_STATUS_HARDWARE_TIMEOUT;
}

/* Работа в потоке acpi: спросить у EC, что случилось (0x84), и
   исполнить _Qxx. Событий может накопиться несколько - спрашиваем,
   пока EC не ответит 0 ("больше нет"). */
static void ec_query_work(uacpi_handle ctx)
{
    (void)ctx;

    for (UINTN n = 0; n < 16 && !g_ec_dead; n++) {

        uacpi_u32 seq = 0;
        UINT8 q = 0;
        BOOLEAN ok;

        if (!ec_lock(&seq))
            return;

        ec_drain();
        ok = ec_wait(EC_IBF, FALSE);
        if (ok) {
            io_out8(g_acpid.ec_cmd, EC_CMD_QUERY);
            ok = ec_wait(EC_OBF, TRUE);
        }
        if (ok)
            q = io_in8(g_acpid.ec_data);

        ec_unlock(seq);

        if (!ok || q == 0)
            break;

        g_acpid.ec_queries++;

        /* имя метода: _Q и номер двумя шестнадцатеричными цифрами */
        static const char hex[] = "0123456789ABCDEF";
        char name[5] = { '_', 'Q', hex[q >> 4], hex[q & 15u], 0 };

        uacpi_status st = uacpi_execute(g_ec_node, name, NULL);

        if (st != UACPI_STATUS_OK && st != UACPI_STATUS_NOT_FOUND)
            klog("acpi: EC query 0x%x: %s failed: %s\n", q, name,
                 uacpi_status_to_string(st));
    }
}

/* Прерывание GPE от EC (в обработчике прерывания): только поставить
   работу в очередь - сам запрос делается в потоке */
static uacpi_interrupt_ret ec_gpe_handler(uacpi_handle ctx, uacpi_namespace_node *dev, uacpi_u16 idx)
{
    (void)ctx;
    (void)dev;
    (void)idx;

    if (io_in8(g_acpid.ec_cmd) & EC_SCI_EVT)
        uacpi_kernel_schedule_work(UACPI_WORK_GPE_EXECUTION, ec_query_work, NULL);

    return UACPI_INTERRUPT_HANDLED | UACPI_GPE_REENABLE;
}

/* Порты EC - из _CRS: первый ресурс IO - данные, второй - команды */
typedef struct {
    UINT16 port[2];
    UINTN  n;
} EC_PORTS;

static uacpi_iteration_decision ec_crs_cb(void *user, uacpi_resource *r)
{
    EC_PORTS *p = user;

    if (p->n < 2 && r->type == UACPI_RESOURCE_TYPE_IO)
        p->port[p->n++] = r->io.minimum;
    else if (p->n < 2 && r->type == UACPI_RESOURCE_TYPE_FIXED_IO)
        p->port[p->n++] = r->fixed_io.address;

    return UACPI_ITERATION_DECISION_CONTINUE;
}

static uacpi_iteration_decision ec_find_cb(void *user, uacpi_namespace_node *node, uacpi_u32 depth)
{
    (void)depth;
    uacpi_namespace_node **out = user;

    *out = node;
    return UACPI_ITERATION_DECISION_BREAK;
}

/* Найти EC и повесить обработчик его адресного пространства - между
   загрузкой AML и её инициализацией (методы _INI и _REG уже могут
   читать EC). */
static void ec_init(void)
{
    uacpi_namespace_node *node = NULL;
    EC_PORTS ports = { { 0, 0 }, 0 };

    uacpi_find_devices("PNP0C09", ec_find_cb, &node);

    if (node != NULL)
        uacpi_for_each_device_resource(node, "_CRS", ec_crs_cb, &ports);

    /* нет устройства (или _CRS не прочитался) - таблица ECDT: её
       прошивка даёт как раз для такого "раннего" доступа к EC */
    uacpi_table tbl;

    if ((node == NULL || ports.n < 2) &&
        uacpi_table_find_by_signature("ECDT", &tbl) == UACPI_STATUS_OK) {

        struct acpi_ecdt *e = tbl.ptr;

        if (e->ec_control.address != 0 && e->ec_data.address != 0) {
            ports.port[0] = (UINT16)e->ec_data.address;
            ports.port[1] = (UINT16)e->ec_control.address;
            ports.n = 2;
            g_acpid.ec_from_ecdt = TRUE;

            if (node == NULL)
                uacpi_namespace_node_find(NULL, e->ec_id, &node);
            if (node == NULL)
                node = uacpi_namespace_root();
        }

        uacpi_table_unref(&tbl);
    }

    if (node == NULL || ports.n < 2)
        return;

    g_ec_node = node;
    g_acpid.ec_data = ports.port[0];
    g_acpid.ec_cmd = ports.port[1];
    g_acpid.ec_gpe = -1;

    uacpi_u64 v = 0;

    if (uacpi_eval_simple_integer(node, "_GLK", &v) == UACPI_STATUS_OK && v != 0)
        g_acpid.ec_glk = TRUE;

    uacpi_status st = uacpi_install_address_space_handler(
        node, UACPI_ADDRESS_SPACE_EMBEDDED_CONTROLLER, ec_region_handler, NULL);

    if (st != UACPI_STATUS_OK) {
        klog("acpi: EC handler: %s\n", uacpi_status_to_string(st));
        return;
    }

    g_acpid.have_ec = TRUE;

    /* номер GPE, по которому EC зовёт на запрос */
    if (uacpi_eval_simple_integer(node, "_GPE", &v) == UACPI_STATUS_OK && v < 256u) {
        g_acpid.ec_gpe = (INT32)v;
        st = uacpi_install_gpe_handler(NULL, (uacpi_u16)v, UACPI_GPE_TRIGGERING_EDGE,
                                       ec_gpe_handler, NULL);
        if (st != UACPI_STATUS_OK)
            klog("acpi: EC GPE 0x%x: %s\n", (UINT32)v, uacpi_status_to_string(st));
    }

    klog("acpi: EC data 0x%x cmd 0x%x gpe %d%s%s\n", g_acpid.ec_data, g_acpid.ec_cmd,
         g_acpid.ec_gpe, g_acpid.ec_glk ? " (global lock)" : "",
         g_acpid.ec_from_ecdt ? " (ECDT)" : "");
}

/* ================================================================
 * Батарея
 * ================================================================ */

/* число из пакета AML; FALSE - нет такого элемента или не число */
static BOOLEAN pkg_int(uacpi_object_array *a, UINTN i, UINT32 *out)
{
    uacpi_u64 v = 0;

    if (i >= a->count || uacpi_object_get_integer(a->objects[i], &v) != UACPI_STATUS_OK)
        return FALSE;

    *out = (UINT32)v;
    return TRUE;
}

/* строка из пакета (бывает и строкой, и буфером) */
static void pkg_str(uacpi_object_array *a, UINTN i, char *dst, UINTN cap)
{
    uacpi_data_view dv;
    UINTN n = 0;

    if (i < a->count &&
        uacpi_object_get_string_or_buffer(a->objects[i], &dv) == UACPI_STATUS_OK) {

        for (; n < dv.length && n + 1 < cap; n++) {
            char c = (char)dv.const_bytes[n];
            if (c == '\0')
                break;
            dst[n] = (c >= 32 && c < 127) ? c : '?';
        }
    }

    /* убрать пробелы в конце ("LION    ") */
    while (n > 0 && dst[n - 1] == ' ')
        n--;

    dst[n] = '\0';
}

#define BAT_UNKNOWN 0xFFFFFFFFu

/* Паспорт батареи: сначала новый _BIX (ACPI 4.0+), потом _BIF */
static void bat_read_info(uacpi_namespace_node *node, ACPI_BATTERY *b)
{
    uacpi_object *obj = NULL;
    uacpi_object_array a;
    UINT32 unit = 0;

    if (uacpi_eval_simple_package(node, "_BIX", &obj) == UACPI_STATUS_OK &&
        uacpi_object_get_package(obj, &a) == UACPI_STATUS_OK && a.count >= 20) {

        /* [0] ревизия, [1] единицы, [2] паспорт, [3] последняя
           полная, [5] напряжение, [8] циклы, [16] модель, [18] тип,
           [19] производитель */
        pkg_int(&a, 1, &unit);
        pkg_int(&a, 2, &b->design);
        pkg_int(&a, 3, &b->full);
        pkg_int(&a, 5, &b->design_mv);
        pkg_int(&a, 8, &b->cycles);
        pkg_str(&a, 16, b->model, sizeof(b->model));
        pkg_str(&a, 18, b->type, sizeof(b->type));
        pkg_str(&a, 19, b->oem, sizeof(b->oem));

    } else {

        if (obj != NULL) {
            uacpi_object_unref(obj);
            obj = NULL;
        }

        if (uacpi_eval_simple_package(node, "_BIF", &obj) != UACPI_STATUS_OK ||
            uacpi_object_get_package(obj, &a) != UACPI_STATUS_OK || a.count < 13) {
            if (obj != NULL)
                uacpi_object_unref(obj);
            return;
        }

        /* [0] единицы, [1] паспорт, [2] последняя полная,
           [4] напряжение, [9] модель, [11] тип, [12] производитель */
        pkg_int(&a, 0, &unit);
        pkg_int(&a, 1, &b->design);
        pkg_int(&a, 2, &b->full);
        pkg_int(&a, 4, &b->design_mv);
        b->cycles = 0;
        pkg_str(&a, 9, b->model, sizeof(b->model));
        pkg_str(&a, 11, b->type, sizeof(b->type));
        pkg_str(&a, 12, b->oem, sizeof(b->oem));
    }

    b->mah = (unit == 1);

    if (b->cycles == BAT_UNKNOWN)
        b->cycles = 0;

    uacpi_object_unref(obj);
}

static BOOLEAN bat_read_status(uacpi_namespace_node *node, ACPI_BATTERY *b)
{
    uacpi_object *obj = NULL;
    uacpi_object_array a;

    if (uacpi_eval_simple_package(node, "_BST", &obj) != UACPI_STATUS_OK)
        return FALSE;

    BOOLEAN ok = uacpi_object_get_package(obj, &a) == UACPI_STATUS_OK &&
                 pkg_int(&a, 0, &b->state) && pkg_int(&a, 1, &b->rate) &&
                 pkg_int(&a, 2, &b->remaining) && pkg_int(&a, 3, &b->mv);

    uacpi_object_unref(obj);

    if (!ok)
        return FALSE;

    /* процент - от ёмкости последней полной зарядки (так считают все
       ОС: изношенная батарея "полна" и при меньшей ёмкости) */
    UINT32 full = b->full;

    if (full == 0 || full == BAT_UNKNOWN)
        full = b->design;

    if (full == 0 || full == BAT_UNKNOWN || b->remaining == BAT_UNKNOWN) {
        b->percent = 0;
    } else {
        UINT64 p = (UINT64)b->remaining * 100u / full;
        b->percent = (UINT32)(p > 100u ? 100u : p);
    }

    /* сколько осталось: ёмкость / ток = часы */
    b->minutes = -1;

    if (b->rate != 0 && b->rate != BAT_UNKNOWN && b->remaining != BAT_UNKNOWN &&
        b->rate < 0x80000000u) {

        if (b->state & 1u)
            b->minutes = (INT32)((UINT64)b->remaining * 60u / b->rate);
        else if ((b->state & 2u) && full != BAT_UNKNOWN && full > b->remaining)
            b->minutes = (INT32)((UINT64)(full - b->remaining) * 60u / b->rate);
    }

    return TRUE;
}

/* Перечитать всё: батареи, зарядку, крышку. Зовётся из потока power
   и из команды battery. need_info - ещё и паспорт (он меняется
   только при замене батареи - Notify 0x81). */
static void acpi_dev_refresh(BOOLEAN need_info)
{
    kmutex_lock(&g_acpid_mutex);

    for (UINT32 i = 0; i < g_acpid.nbat; i++) {

        ACPI_BATTERY *b = &g_acpid.bat[i];
        uacpi_u32 sta = 0;
        BOOLEAN was = b->present;

        if (uacpi_eval_sta(g_bat_node[i], &sta) != UACPI_STATUS_OK)
            sta = 0;

        b->present = (sta & 0x10u) != 0;

        if (!b->present) {
            b->valid = FALSE;
            continue;
        }

        if (need_info || !was || b->full == 0)
            bat_read_info(g_bat_node[i], b);

        b->valid = bat_read_status(g_bat_node[i], b);
    }

    uacpi_u64 v = 0;

    if (g_ac_node != NULL && uacpi_eval_simple_integer(g_ac_node, "_PSR", &v) == UACPI_STATUS_OK)
        g_acpid.ac_online = (v != 0);

    g_acpid.bat_updated_ms = kx_uptime_us() / 1000u;

    kmutex_unlock(&g_acpid_mutex);
}

/*
 * Крышка: узнать, открыта ли (_LID или подмена "lid test"), и если
 * положение сменилось - погасить или зажечь подсветку. Зовут поток
 * power (каждую секунду и после Notify) и команды battery/lid.
 */
static void lid_check(void)
{
    BOOLEAN open;

    if (g_lid_test != 0) {

        open = (g_lid_test == 2);

    } else {

        if (g_lid_node == NULL)
            return;

        uacpi_u64 v = 0;

        kmutex_lock(&g_acpid_mutex);
        uacpi_status st = uacpi_eval_simple_integer(g_lid_node, "_LID", &v);
        kmutex_unlock(&g_acpid_mutex);

        if (st != UACPI_STATUS_OK)
            return;

        g_lid_polls++;
        open = (v != 0);
    }

    if (open == g_acpid.lid_open)
        return;

    g_acpid.lid_open = open;
    g_acpid.lid_changes++;

    if (!open) {
        BOOLEAN ok = backlight_blank(TRUE);
        klog("lid: closed - %s\n", ok ? (g_backlight.mode == BL_ACPI ? "screen dimmed (ACPI _BCM)"
                                                                  : "screen off")
                                      : "no backlight control");
    } else {
        BOOLEAN ok = backlight_blank(FALSE);
        if (ok)
            klog("lid: opened - screen on, brightness %d%%\n", backlight_get());
        else
            klog("lid: opened\n");
    }
}

/* Для значка на панели: "87%" и заряжается ли. FALSE - батареи нет. */
BOOLEAN acpi_battery_brief(char *buf, UINTN cap, BOOLEAN *charging)
{
    for (UINT32 i = 0; i < g_acpid.nbat; i++) {

        ACPI_BATTERY *b = &g_acpid.bat[i];

        if (!b->present || !b->valid)
            continue;

        ksnprintf(buf, cap, "%u%%", b->percent);

        if (charging)
            *charging = (b->state & 2u) != 0 || (g_acpid.ac_online && !(b->state & 1u));

        return TRUE;
    }

    return FALSE;
}

/* ================================================================
 * События: Notify от устройств и кнопка питания
 * ================================================================ */

static uacpi_status dev_notify(uacpi_handle ctx, uacpi_namespace_node *node, uacpi_u64 value)
{
    (void)node;
    UINTN kind = (UINTN)ctx;   /* 1 - батарея, 2 - зарядка, 3 - крышка, 4 - кнопка */

    klog("acpi: notify %s 0x%x\n",
         kind == 1 ? "battery" : kind == 2 ? "AC" : kind == 3 ? "lid" : "power button",
         (UINT32)value);

    if (kind == 4) {
        if (value == 0x80) {
            g_acpid.pwrbtn_presses++;
            g_power_button_pending = TRUE;
        }
    } else if (kind == 3) {
        g_lid_notifies++;          /* крышку перечитает поток power */
    } else {
        g_bat_dirty = TRUE;
    }

    power_kick();
    return UACPI_STATUS_OK;
}

/* кнопка питания - фиксированное событие: зовётся в прерывании SCI */
static uacpi_interrupt_ret pwrbtn_fixed_handler(uacpi_handle ctx)
{
    (void)ctx;

    g_acpid.pwrbtn_presses++;
    g_power_button_pending = TRUE;
    power_kick();

    return UACPI_INTERRUPT_HANDLED;
}

typedef struct {
    UINTN kind;
} FIND_CTX;

static uacpi_iteration_decision found_dev(void *user, uacpi_namespace_node *node, uacpi_u32 depth)
{
    (void)depth;
    FIND_CTX *f = user;

    if (f->kind == 1) {

        if (g_acpid.nbat >= ACPI_MAX_BATTERIES)
            return UACPI_ITERATION_DECISION_CONTINUE;

        ACPI_BATTERY *b = &g_acpid.bat[g_acpid.nbat];
        uacpi_object_name nm = uacpi_namespace_node_name(node);

        for (UINTN i = 0; i < 4; i++)
            b->name[i] = nm.text[i];
        b->name[4] = '\0';

        g_bat_node[g_acpid.nbat++] = node;

    } else if (f->kind == 2) {

        if (g_ac_node != NULL)
            return UACPI_ITERATION_DECISION_CONTINUE;
        g_ac_node = node;
        g_acpid.have_ac = TRUE;

    } else if (f->kind == 3) {

        if (g_lid_node != NULL)
            return UACPI_ITERATION_DECISION_CONTINUE;
        g_lid_node = node;
        g_acpid.have_lid = TRUE;
        g_acpid.lid_open = TRUE;

    } else {

        g_acpid.pwrbtn_devices++;
    }

    uacpi_install_notify_handler(node, dev_notify, (uacpi_handle)f->kind);

    return UACPI_ITERATION_DECISION_CONTINUE;
}

/* ================================================================
 * Выключение через uACPI: _PTS ("готовься ко сну S5") и запись
 * SLP_TYP|SLP_EN в регистры PM1 (значения из \_S5 - uACPI их знает
 * честно, а не поиском байтов, как power.c)
 * ================================================================ */

void acpi_dev_poweroff(void)
{
    if (!g_acpid.ok)
        return;

    uacpi_status st = uacpi_prepare_for_sleep_state(UACPI_SLEEP_STATE_S5);

    if (st != UACPI_STATUS_OK)
        klog("acpi: prepare S5: %s\n", uacpi_status_to_string(st));

    klog("acpi: entering S5 via uACPI\n");
    kcon_flush();
    kx_cli();

    st = uacpi_enter_sleep_state(UACPI_SLEEP_STATE_S5);

    /* сюда попадаем, только если машина не выключилась */
    klog("acpi: enter S5: %s\n", uacpi_status_to_string(st));
}

/* ================================================================
 * Поток power
 * ================================================================ */

static void power_thread(void *arg)
{
    (void)arg;
    UINT32 seen = g_power_kick;
    UINT64 last_bat = 0;

    for (;;) {

        if (g_power_button_pending) {

            g_power_button_pending = FALSE;

            klog("power button pressed - shutting down\n");
            if (g_st != NULL)
                kprintf(g_st->ConOut, "\nPower button pressed - shutting down...\n");
            kcon_flush();

            /* дать консоли и журналу дописаться */
            sched_sleep_ms(300);
            kx_shutdown();
        }

        lid_check();

        /* батарея - после Notify или раз в 15 с (её методы AML ходят в
           EC и небыстрые; крышка - дёшево, её каждую секунду) */
        UINT64 now = kx_uptime_us() / 1000u;

        if (g_bat_dirty || now - last_bat >= 15000u) {
            BOOLEAN dirty = g_bat_dirty;
            g_bat_dirty = FALSE;
            acpi_dev_refresh(dirty);
            last_bat = now;
        }

        UINT64 fl = kx_irq_save();

        while (g_power_kick == seen) {
            if (!sched_block((const void *)&g_power_kick, "power",
                             (g_lid_node != NULL || g_lid_test != 0) ? 1000 : 15000))
                break;   /* таймаут - плановый опрос */
        }

        seen = g_power_kick;
        kx_irq_restore(fl);
    }
}

/* ================================================================
 * Запуск
 * ================================================================ */

static BOOLEAN acpi_step(SIMPLE_TEXT_OUTPUT_INTERFACE *out, const char *what, uacpi_status st)
{
    if (st == UACPI_STATUS_OK)
        return TRUE;

    kprintf(out, "  uACPI: %s failed: %s\n", what, uacpi_status_to_string(st));
    klog("acpi: %s failed: %s\n", what, uacpi_status_to_string(st));
    g_acpid.why = what;

    return FALSE;
}

void acpi_dev_init(SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    g_acpid.ok = FALSE;
    g_acpid.ec_gpe = -1;

    if (!g_acpi.present || !g_acpi.have_fadt) {
        g_acpid.why = "no ACPI tables";
        kprintf(out, "  uACPI: not started (%s)\n", g_acpid.why);
        return;
    }

    UINT64 t0 = kx_uptime_us();

    /* работа из прерываний пойдёт сразу, как включится SCI */
    acpi_os_start();

    if (!acpi_step(out, "initialize", uacpi_initialize(0)))
        return;

    /* загрузить DSDT и SSDT в пространство имён (ещё не исполняя _INI) */
    if (!acpi_step(out, "namespace load", uacpi_namespace_load()))
        return;

    /* сказать прошивке, что прерывания идут через I/O APIC (\_PIC(1)) */
    uacpi_set_interrupt_model(UACPI_INTERRUPT_MODEL_IOAPIC);

    /* EC - до инициализации: её методы (_INI, _REG) уже читают EC */
    ec_init();

    if (!acpi_step(out, "namespace init", uacpi_namespace_initialize()))
        return;

    g_acpid.ok = TRUE;
    g_acpid.why = "ok";

    /* устройства */
    FIND_CTX f;

    f.kind = 1; uacpi_find_devices("PNP0C0A", found_dev, &f);    /* батарея */
    f.kind = 2; uacpi_find_devices("ACPI0003", found_dev, &f);   /* блок питания */
    f.kind = 3; uacpi_find_devices("PNP0C0D", found_dev, &f);    /* крышка */
    f.kind = 4; uacpi_find_devices("PNP0C0C", found_dev, &f);    /* кнопка питания */

    /* кнопка питания как фиксированное событие (если прошивка не
       сказала, что кнопки такого вида нет - флаг PWR_BUTTON в FADT) */
    if (!(g_acpi.fadt_flags & (1u << 4)) &&
        uacpi_install_fixed_event_handler(UACPI_FIXED_EVENT_POWER_BUTTON,
                                          pwrbtn_fixed_handler, NULL) == UACPI_STATUS_OK)
        g_acpid.pwrbtn_fixed = TRUE;

    /* включить GPE, у которых есть методы _Lxx/_Exx (и EC) */
    uacpi_finalize_gpe_initialization();

    if (g_acpid.ec_gpe >= 0)
        uacpi_enable_gpe(NULL, (uacpi_u16)g_acpid.ec_gpe);

    g_acpid.init_ms = (UINT32)((kx_uptime_us() - t0) / 1000u);

    acpi_dev_refresh(TRUE);

    kprintf(out, "  uACPI: AML loaded in %u ms; batteries: %u, AC adapter: %s, lid: %s, "
                 "EC: %s, power button: %s\n",
            g_acpid.init_ms, g_acpid.nbat, g_acpid.have_ac ? "yes" : "no",
            g_acpid.have_lid ? "yes" : "no", g_acpid.have_ec ? "yes" : "no",
            g_acpid.pwrbtn_fixed ? "fixed" : g_acpid.pwrbtn_devices ? "device" : "none");

    for (UINT32 i = 0; i < g_acpid.nbat; i++) {
        ACPI_BATTERY *b = &g_acpid.bat[i];
        if (b->present && b->valid)
            kprintf(out, "  battery %s: %u%%%s\n", b->name, b->percent,
                    (b->state & 2u) ? ", charging" : (b->state & 1u) ? ", discharging" : "");
    }

    klog("acpi: uACPI ready in %u ms, %u batteries\n", g_acpid.init_ms, g_acpid.nbat);

    g_power_thread = kthread_create("power", power_thread, NULL, 8);
}

/* ================================================================
 * Команда battery
 * ================================================================ */

static void print_cap(SIMPLE_TEXT_OUTPUT_INTERFACE *out, UINT32 v, BOOLEAN mah, BOOLEAN rate)
{
    if (v == BAT_UNKNOWN) {
        print(out, "?");
        return;
    }

    kprintf(out, "%u %s", v, mah ? (rate ? "mA" : "mAh") : (rate ? "mW" : "mWh"));
}

void kernel_cmd_battery(SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    if (!g_acpid.ok) {
        kprintf(out, "ACPI (uACPI) is not running: %s\n", g_acpid.why ? g_acpid.why : "?");
        return;
    }

    acpi_dev_refresh(FALSE);

    if (g_acpid.nbat == 0)
        print(out, "No battery found (desktop or virtual machine).\n");

    for (UINT32 i = 0; i < g_acpid.nbat; i++) {

        ACPI_BATTERY *b = &g_acpid.bat[i];

        if (!b->present) {
            kprintf(out, "Battery %s: not inserted\n", b->name);
            continue;
        }

        if (!b->valid) {
            kprintf(out, "Battery %s: present, but _BST failed\n", b->name);
            continue;
        }

        kprintf(out, "Battery %s: %u%%, %s", b->name, b->percent,
                (b->state & 2u) ? "charging" : (b->state & 1u) ? "discharging" : "not charging");

        if (b->minutes >= 0)
            kprintf(out, ", %d:%02d %s", b->minutes / 60, b->minutes % 60,
                    (b->state & 1u) ? "left" : "to full");

        if (b->state & 4u)
            print(out, ", CRITICAL");

        print(out, "\n  now: ");
        print_cap(out, b->remaining, b->mah, FALSE);
        print(out, " of ");
        print_cap(out, b->full, b->mah, FALSE);
        print(out, " (design ");
        print_cap(out, b->design, b->mah, FALSE);
        print(out, "), rate ");
        print_cap(out, b->rate, b->mah, TRUE);
        if (b->mv != BAT_UNKNOWN)
            kprintf(out, ", %u.%03u V", b->mv / 1000u, b->mv % 1000u);
        print(out, "\n");

        if (b->full != 0 && b->full != BAT_UNKNOWN && b->design != 0 && b->design != BAT_UNKNOWN)
            kprintf(out, "  health: %u%% of design", (UINT32)((UINT64)b->full * 100u / b->design));
        else
            print(out, "  health: ?");
        if (b->cycles)
            kprintf(out, ", %u cycles", b->cycles);
        kprintf(out, "; model '%s', type '%s', maker '%s'\n", b->model, b->type, b->oem);
    }

    if (g_acpid.have_ac)
        kprintf(out, "AC adapter: %s\n", g_acpid.ac_online ? "plugged in" : "unplugged");

    lid_check();

    if (g_acpid.have_lid)
        kprintf(out, "Lid: %s\n", g_acpid.lid_open ? "open" : "closed");

    if (g_acpid.have_ec) {
        kprintf(out, "EC: ports 0x%x/0x%x, GPE %d%s; %llu reads, %llu writes, "
                     "%llu events, %llu timeouts\n",
                g_acpid.ec_data, g_acpid.ec_cmd, g_acpid.ec_gpe,
                g_acpid.ec_glk ? ", global lock" : "",
                g_acpid.ec_reads, g_acpid.ec_writes, g_acpid.ec_queries, g_acpid.ec_timeouts);
        if (g_ec_dead)
            print(out, "  EC stopped answering - MyOS gave up on it (battery data may be missing)\n");
    } else {
        print(out, "EC: none\n");
    }

    kprintf(out, "Power button: %s, pressed %u times\n",
            g_acpid.pwrbtn_fixed ? "fixed event" : g_acpid.pwrbtn_devices ? "PNP0C0C device" : "none",
            g_acpid.pwrbtn_presses);

    kprintf(out, "uACPI: AML loaded in %u ms, SCI interrupts %llu, events handled %llu",
            g_acpid.init_ms, g_acpi_sci_count, g_acpi_work_done);
    if (g_acpi_work_lost)
        kprintf(out, " (%llu lost)", g_acpi_work_lost);
    kprintf(out, ", warnings %u, errors %u\n", g_acpid.n_warnings, g_acpid.n_errors);

    if (g_acpid.last_msg[0])
        kprintf(out, "  last message: %s\n", g_acpid.last_msg);
}

/*
 * Команда lid: положение крышки и что с экраном.
 *   lid                  - состояние
 *   lid test close|open  - притвориться, что крышку закрыли/открыли
 *                          (проверка без настоящей крышки, в QEMU)
 *   lid test off         - снова слушать настоящую крышку
 */
void kernel_cmd_lid(SIMPLE_TEXT_OUTPUT_INTERFACE *out, const char *arg)
{
    if (!g_acpid.ok) {
        kprintf(out, "ACPI (uACPI) is not running: %s\n", g_acpid.why ? g_acpid.why : "?");
        return;
    }

    while (*arg == ' ')
        arg++;

    if (arg[0] == 't' && arg[1] == 'e' && arg[2] == 's' && arg[3] == 't') {

        const char *w = arg + 4;
        while (*w == ' ')
            w++;

        if (kstreq(w, "close") || kstreq(w, "closed")) {
            g_lid_test = 1;
        } else if (kstreq(w, "open")) {
            g_lid_test = 2;
        } else if (kstreq(w, "off")) {
            g_lid_test = 0;
            /* настоящей крышки нет - считать открытой (и зажечь экран) */
            if (g_lid_node == NULL && !g_acpid.lid_open) {
                g_lid_test = 2;
                lid_check();
                g_lid_test = 0;
            }
        } else {
            print(out, "Usage: lid test close | open | off\n");
            return;
        }

        /* сразу, не дожидаясь потока power */
        lid_check();
        power_kick();
    } else if (*arg != '\0') {
        print(out, "Usage: lid [test close | open | off]\n");
        return;
    } else {
        lid_check();
    }

    if (!g_acpid.have_lid && g_lid_test == 0) {
        print(out, "No lid found (not a laptop, or the firmware has no PNP0C0D device).\n");
        return;
    }

    kprintf(out, "Lid: %s%s; %u changes, %u notifies from the firmware, polled %u times\n",
            g_acpid.lid_open ? "open" : "closed",
            g_lid_test ? " (test - 'lid test off' to use the real lid)" : "",
            g_acpid.lid_changes, g_lid_notifies, g_lid_polls);

    if (g_backlight.mode == BL_NONE)
        print(out, "Screen: no backlight control - it stays on\n");
    else if (g_backlight.blanked)
        kprintf(out, "Screen: %s (brightness %d%% comes back when the lid opens)\n",
                g_backlight.mode == BL_ACPI ? "dimmed" : "off", g_backlight.saved_pct);
    else
        kprintf(out, "Screen: on, brightness %d%%\n", backlight_get());
}
