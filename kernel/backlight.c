/*
 * kernel/backlight.c - яркость экрана ноутбука (этап 9).
 * Часть MyOS; общие объявления - в myos.h.
 *
 * Подсветку экрана ноутбука питает ШИМ (PWM): светодиоды то горят, то
 * нет, тысячи раз в секунду; яркость - доля времени "горят" (duty
 * cycle). Генератор ШИМ у ноутбуков с графикой Intel встроен в сам
 * видеоконтроллер, и его регистры прошивка уже настроила до нас
 * (экран ведь светится). Нам остаётся менять одно число - долю.
 *
 * Два вида регистров (по книгам Intel "Display Registers" и драйверу
 * i915 в Linux), в окне регистров видеокарты (BAR0) по 0xC8250:
 *   * PCH (Skylake, Kaby Lake, Coffee/Whiskey/Comet Lake - почти все
 *     Core i3/i5 2016-2020):
 *       0xC8250 BLC_PWM_PCH_CTL1: бит 31 - ШИМ включён,
 *                                 бит 29 - "инверсия" (1 = темнее при
 *                                 большей доле);
 *       0xC8254 BLC_PWM_PCH_CTL2: [31:16] - период (максимум),
 *                                 [15:0]  - доля (яркость);
 *   * BXT (Celeron/Pentium на Apollo Lake и Gemini Lake) и все чипсеты
 *     с Cannon Point (300-я серия: Coffee/Whiskey/Comet Lake) и новее:
 *       0xC8250 - включён/инверсия (те же биты), 0xC8254 - период
 *       целиком, 0xC8258 - доля целиком.
 * Какой вид - узнаём по чипсету (мост LPC 0:1F.0, как i915 в Linux), а
 * если прочитанные числа не сходятся - пробуем другой вид.
 * HP 250 G7 бывает и с Core i3/i5 разных лет, и с Celeron N4000.
 *
 * Запасной путь - ACPI: у устройства экрана в AML бывают методы _BCL
 * (список уровней), _BCM (поставить уровень) и _BQC (какой сейчас).
 * Их зовём через uACPI, если своих регистров нет (другая видеокарта,
 * или QEMU с тестовой таблицей).
 *
 * Клавиши яркости (Fn+F2/F3 у HP) приходят не как клавиши, а через
 * ACPI: прошивка делает Notify(экран, 0x86 - ярче / 0x87 - темнее).
 * Их ловит обработчик ниже - шаг 10%.
 */
#include "myos.h"
#include <uacpi/uacpi.h>
#include <uacpi/namespace.h>
#include <uacpi/notify.h>
#include <uacpi/utilities.h>

BACKLIGHT_INFO g_backlight;

#define BLC_CTL1      0xC8250u
#define BLC_CTL2      0xC8254u    /* PCH: период|доля; BXT: период */
#define BXT_DUTY      0xC8258u
#define BLC_ENABLE    (1u << 31)
#define BLC_POLARITY  (1u << 29)

/* самая маленькая доля, которую даём поставить: при 0 экран гаснет
   совсем, и его уже не видно, чтобы вернуть яркость */
#define BL_MIN_PCT    5u

static UINT64 g_bl_mmio;   /* физический адрес регистров видеокарты */

/* узел экрана в AML с методом _BCM (запасной путь) */
static uacpi_namespace_node *g_bl_acpi_node;
static UINT32 g_bl_levels[64];     /* из _BCL, без двух первых */
static UINTN  g_bl_nlevels;

static KMUTEX g_bl_mutex = KMUTEX_INIT("backlight");

static UINT32 bl_rd(UINT32 reg)
{
    return mmio_read32(g_bl_mmio + reg);
}

static void bl_wr(UINT32 reg, UINT32 v)
{
    mmio_write32(g_bl_mmio + reg, v);
}

/* Видеокарты Intel на Apollo/Gemini Lake - регистры вида BXT */
static BOOLEAN bl_is_bxt(UINT16 did)
{
    static const UINT16 ids[] = {
        0x0A84, 0x1A84, 0x1A85, 0x5A84, 0x5A85,    /* Apollo Lake */
        0x3184, 0x3185                             /* Gemini Lake */
    };

    for (UINTN i = 0; i < sizeof(ids) / sizeof(ids[0]); i++)
        if (ids[i] == did)
            return TRUE;

    return FALSE;
}

/* Чипсет (PCH): код устройства моста LPC 0:1F.0 с маской 0xFF80 - так
   его семейство определяет i915 (intel_pch.c). 0 - не Intel. */
static UINT16 bl_pch_family(void)
{
    UINT32 id = pci_config_read32(0, 0x1F, 0, 0x00);

    if ((id & 0xFFFFu) != 0x8086u)
        return 0;

    return (UINT16)((id >> 16) & 0xFF80u);
}

/* Старый вид регистров (период и доля в одном 0xC8254): Lynx Point,
   Wildcat Point (Haswell, Broadwell), Sunrise Point и Union Point
   (Skylake, Kaby Lake). Всё новее - вид BXT. */
static BOOLEAN bl_pch_old_layout(UINT16 fam)
{
    static const UINT16 old[] = {
        0x8C00, 0x9C00,          /* Lynx Point, -LP */
        0x8C80, 0x9C80,          /* Wildcat Point, -LP */
        0xA100, 0x9D00,          /* Sunrise Point, -LP */
        0xA280                   /* Union (Kaby Lake) Point */
    };

    for (UINTN i = 0; i < sizeof(old) / sizeof(old[0]); i++)
        if (old[i] == fam)
            return TRUE;

    return FALSE;
}

/* Прочитать максимум и долю из регистров (с учётом инверсии) */
static BOOLEAN bl_native_read(UINT32 *max, UINT32 *duty)
{
    UINT32 ctl1 = bl_rd(BLC_CTL1);
    UINT32 m, d;

    if (g_backlight.bxt) {
        m = bl_rd(BLC_CTL2);
        d = bl_rd(BXT_DUTY);
    } else {
        UINT32 ctl2 = bl_rd(BLC_CTL2);
        m = ctl2 >> 16;
        d = ctl2 & 0xFFFFu;
    }

    if (m == 0 || m == 0xFFFFFFFFu || d > m)
        return FALSE;

    g_backlight.inverted = (ctl1 & BLC_POLARITY) != 0;

    if (g_backlight.inverted)
        d = m - d;

    *max = m;
    *duty = d;
    return TRUE;
}

static void bl_native_write(UINT32 max, UINT32 duty)
{
    if (g_backlight.inverted)
        duty = max - duty;

    if (g_backlight.bxt) {
        bl_wr(BXT_DUTY, duty);
    } else {
        UINT32 ctl2 = bl_rd(BLC_CTL2);
        bl_wr(BLC_CTL2, (ctl2 & 0xFFFF0000u) | (duty & 0xFFFFu));
    }
}

/* ---------------- запасной путь: ACPI _BCL/_BCM/_BQC ---------------- */

static uacpi_iteration_decision bl_find_bcm(void *user, uacpi_namespace_node *node, uacpi_u32 depth)
{
    (void)user;
    (void)depth;

    uacpi_namespace_node *m = NULL;

    if (g_bl_acpi_node == NULL &&
        uacpi_namespace_node_find(node, "_BCM", &m) == UACPI_STATUS_OK && m != NULL) {
        g_bl_acpi_node = node;
        return UACPI_ITERATION_DECISION_BREAK;
    }

    return UACPI_ITERATION_DECISION_CONTINUE;
}

static void bl_acpi_init(void)
{
    if (!g_acpid.ok)
        return;

    uacpi_namespace_for_each_child_simple(uacpi_namespace_root(), bl_find_bcm, NULL);

    if (g_bl_acpi_node == NULL)
        return;

    /* _BCL: [0] уровень "от сети" по умолчанию, [1] "от батареи",
       дальше - все допустимые уровни (0..100) */
    uacpi_object *obj = NULL;
    uacpi_object_array a;

    g_bl_nlevels = 0;

    if (uacpi_eval_simple_package(g_bl_acpi_node, "_BCL", &obj) == UACPI_STATUS_OK &&
        uacpi_object_get_package(obj, &a) == UACPI_STATUS_OK) {

        for (UINTN i = 2; i < a.count && g_bl_nlevels < 64; i++) {
            uacpi_u64 v = 0;
            if (uacpi_object_get_integer(a.objects[i], &v) == UACPI_STATUS_OK && v <= 100u)
                g_bl_levels[g_bl_nlevels++] = (UINT32)v;
        }
    }

    if (obj != NULL)
        uacpi_object_unref(obj);

    if (g_bl_nlevels == 0) {
        g_bl_acpi_node = NULL;
        return;
    }
}

/* ближайший допустимый уровень _BCL к pct */
static UINT32 bl_acpi_nearest(UINT32 pct)
{
    UINT32 best = g_bl_levels[0];
    UINT32 bd = 1000;

    for (UINTN i = 0; i < g_bl_nlevels; i++) {
        UINT32 d = (g_bl_levels[i] > pct) ? g_bl_levels[i] - pct : pct - g_bl_levels[i];
        if (d < bd) {
            bd = d;
            best = g_bl_levels[i];
        }
    }

    return best;
}

/* ---------------- общее ---------------- */

/* Текущая яркость, 0..100; -1 - управлять нечем */
INT32 backlight_get(void)
{
    if (g_backlight.mode == BL_NATIVE) {
        UINT32 m = 0, d = 0;
        if (!bl_native_read(&m, &d))
            return -1;
        return (INT32)(((UINT64)d * 100u + m / 2u) / m);
    }

    if (g_backlight.mode == BL_ACPI) {
        uacpi_u64 v = 0;
        if (uacpi_eval_simple_integer(g_bl_acpi_node, "_BQC", &v) == UACPI_STATUS_OK && v <= 100u)
            return (INT32)v;
        return (INT32)g_backlight.acpi_last;
    }

    return -1;
}

/* Поставить яркость pct (0..100; меньше BL_MIN_PCT - не даём).
   Возвращает, что получилось, или -1. */
INT32 backlight_set(INT32 pct)
{
    /* яркость поставили сами (команда, Fn) - экран уже не "погашен" */
    g_backlight.blanked = FALSE;

    if (pct < (INT32)BL_MIN_PCT)
        pct = (INT32)BL_MIN_PCT;
    if (pct > 100)
        pct = 100;

    INT32 got = -1;

    kmutex_lock(&g_bl_mutex);

    if (g_backlight.mode == BL_NATIVE) {

        UINT32 m = 0, d = 0;

        if (bl_native_read(&m, &d)) {
            UINT32 duty = (UINT32)(((UINT64)m * (UINT32)pct + 50u) / 100u);
            if (duty == 0)
                duty = 1;
            bl_native_write(m, duty);
            got = backlight_get();
        }

    } else if (g_backlight.mode == BL_ACPI) {

        UINT32 lvl = bl_acpi_nearest((UINT32)pct);
        uacpi_object *arg = uacpi_object_create_integer(lvl);

        if (arg != NULL) {
            uacpi_object_array args = { &arg, 1 };
            if (uacpi_execute(g_bl_acpi_node, "_BCM", &args) == UACPI_STATUS_OK) {
                g_backlight.acpi_last = lvl;
                got = (INT32)lvl;
            }
            uacpi_object_unref(arg);
        }
    }

    kmutex_unlock(&g_bl_mutex);

    if (got >= 0)
        klog("backlight: %d%%\n", got);

    return got;
}

/*
 * Погасить подсветку (off) или вернуть прежнюю яркость - крышка
 * ноутбука (этап 10, В). TRUE - получилось.
 *
 * Свой ШИМ: доля 0 - светодиоды не горят вовсе. Бит "ШИМ включён"
 * (CTL1, бит 31) не трогаем: выключение и включение ШИМ у разных
 * панелей требует ещё и порядка подачи питания (так делает i915), а
 * доля 0 гасит экран так же, и вернуть её - одна запись в регистр.
 * Нижний предел BL_MIN_PCT здесь не действует: он защищает от
 * "невидимого" экрана, а при закрытой крышке экран и не нужен.
 *
 * ACPI (_BCM): погасить совсем нельзя - ставим самый тусклый уровень
 * из _BCL (так делают и другие ОС без драйвера видеокарты).
 */
BOOLEAN backlight_blank(BOOLEAN off)
{
    if (g_backlight.mode == BL_NONE)
        return FALSE;

    if (!off) {

        if (!g_backlight.blanked)
            return TRUE;

        INT32 pct = g_backlight.saved_pct > 0 ? g_backlight.saved_pct : 100;
        return backlight_set(pct) >= 0;     /* снимет и blanked */
    }

    if (g_backlight.blanked)
        return TRUE;

    INT32 now = backlight_get();

    if (now < 0)
        return FALSE;

    BOOLEAN ok = FALSE;

    kmutex_lock(&g_bl_mutex);

    if (g_backlight.mode == BL_NATIVE) {

        UINT32 m = 0, d = 0;

        if (bl_native_read(&m, &d)) {
            bl_native_write(m, 0);
            ok = TRUE;
        }

    } else {

        /* самый тусклый уровень _BCL */
        UINT32 lvl = g_bl_levels[0];

        for (UINTN i = 1; i < g_bl_nlevels; i++)
            if (g_bl_levels[i] < lvl)
                lvl = g_bl_levels[i];

        uacpi_object *arg = uacpi_object_create_integer(lvl);

        if (arg != NULL) {
            uacpi_object_array args = { &arg, 1 };
            if (uacpi_execute(g_bl_acpi_node, "_BCM", &args) == UACPI_STATUS_OK) {
                g_backlight.acpi_last = lvl;
                ok = TRUE;
            }
            uacpi_object_unref(arg);
        }
    }

    kmutex_unlock(&g_bl_mutex);

    if (ok) {
        g_backlight.saved_pct = now;
        g_backlight.blanked = TRUE;
    }

    return ok;
}

/* На сколько шагов сдвинуть: +1/-1 = 10% (у ACPI - к соседнему уровню) */
INT32 backlight_step(INT32 dir)
{
    /* Fn+яркость на погашенном экране: считать от прежней яркости */
    INT32 cur = g_backlight.blanked ? g_backlight.saved_pct : backlight_get();

    if (cur < 0)
        return -1;

    if (g_backlight.mode == BL_ACPI) {

        /* соседний уровень из _BCL в нужную сторону */
        INT32 best = cur;

        for (UINTN i = 0; i < g_bl_nlevels; i++) {
            INT32 v = (INT32)g_bl_levels[i];
            if (dir > 0 && v > cur && (best == cur || v < best))
                best = v;
            if (dir < 0 && v < cur && (best == cur || v > best))
                best = v;
        }

        return backlight_set(best);
    }

    return backlight_set(cur + dir * 10);
}

/* Notify от прошивки: 0x86 - "ярче", 0x87 - "темнее" (клавиши Fn),
   на любом узле с методом _BCM (так устроены устройства экрана) */
static uacpi_status bl_notify(uacpi_handle ctx, uacpi_namespace_node *node, uacpi_u64 value)
{
    (void)ctx;

    if (value != 0x86 && value != 0x87)
        return UACPI_STATUS_OK;

    uacpi_namespace_node *m = NULL;

    if (uacpi_namespace_node_find(node, "_BCM", &m) != UACPI_STATUS_OK || m == NULL)
        return UACPI_STATUS_OK;

    g_backlight.hotkeys++;
    backlight_step(value == 0x86 ? +1 : -1);

    return UACPI_STATUS_OK;
}

void backlight_init(SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    g_backlight.mode = BL_NONE;

    /* 1. видеокарта Intel - устройство 0:2.0, класс 03 (дисплей) */
    UINT32 id = pci_config_read32(0, 2, 0, 0x00);
    UINT32 cls = pci_config_read32(0, 2, 0, 0x08);

    if ((id & 0xFFFFu) == 0x8086u && (cls >> 24) == 0x03u) {

        UINT16 did = (UINT16)(id >> 16);
        UINT64 bar = pci_read_bar_address(0, 2, 0, 0x10);

        g_backlight.gpu_id = did;

        g_backlight.bar = bar;
        g_backlight.pch = bl_pch_family();

        if (bar != 0) {
            /* нужна одна страница регистров: 0xC8000..0xC8FFF */
            vmm_map_mmio(bar + 0xC8000u, 0x1000u, VMM_UC);
            g_bl_mmio = bar;

            g_backlight.ctl1 = bl_rd(BLC_CTL1);
            g_backlight.r54 = bl_rd(BLC_CTL2);
            g_backlight.r58 = bl_rd(BXT_DUTY);

            /* вид регистров: Apollo/Gemini Lake - BXT; иначе по чипсету
               (старые - период|доля в одном регистре, с Cannon Point - BXT) */
            g_backlight.bxt = bl_is_bxt(did) ||
                              (g_backlight.pch != 0 && !bl_pch_old_layout(g_backlight.pch));

            UINT32 m = 0, d = 0;
            BOOLEAN ok = bl_native_read(&m, &d);

            /* числа не сходятся (доля больше периода, период 0) - значит,
               вид угадан неверно: пробуем другой */
            if (!ok) {
                g_backlight.bxt = !g_backlight.bxt;
                ok = bl_native_read(&m, &d);
                if (!ok)
                    g_backlight.bxt = !g_backlight.bxt;
            }

            if (!(g_backlight.ctl1 & BLC_ENABLE))
                g_backlight.why = "PWM is switched off (CTL1 bit 31 = 0)";
            else if (!ok)
                g_backlight.why = "PWM period/duty registers read as zero or garbage";

            if ((g_backlight.ctl1 & BLC_ENABLE) && ok) {
                g_backlight.mode = BL_NATIVE;
                g_backlight.pwm_max = m;
            }
        } else {
            g_backlight.why = "GPU has no memory BAR0";
        }
    }

    /* 2. нет - ACPI (_BCM) */
    bl_acpi_init();

    if (g_backlight.mode == BL_NONE && g_bl_acpi_node != NULL) {
        g_backlight.mode = BL_ACPI;
        g_backlight.acpi_last = g_bl_levels[g_bl_nlevels - 1];
    }

    /* клавиши яркости - в любом случае, если есть ACPI */
    if (g_acpid.ok)
        uacpi_install_notify_handler(uacpi_namespace_root(), bl_notify, NULL);

    if (g_backlight.mode == BL_NATIVE)
        kprintf(out, "  backlight: Intel GPU %04x, chipset %04x (%s registers), PWM max %u, now %d%%\n",
                g_backlight.gpu_id, g_backlight.pch, g_backlight.bxt ? "BXT" : "PCH",
                g_backlight.pwm_max, backlight_get());
    else if (g_backlight.mode == BL_ACPI)
        kprintf(out, "  backlight: ACPI _BCM, %u levels, now %d%%%s%s\n",
                (UINT32)g_bl_nlevels, backlight_get(),
                g_backlight.why ? " - Intel PWM not used: " : "",
                g_backlight.why ? g_backlight.why : "");
    else
        print(out, "  backlight: no control (not a laptop screen, or unknown GPU)\n");
}

/* brightness debug: что прочитали из видеокарты - для фото, если
   яркость не меняется */
static void backlight_debug(SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    kprintf(out, "Intel GPU: %04x, BAR0 0x%llx, chipset family (LPC & 0xFF80): %04x\n",
            g_backlight.gpu_id, g_backlight.bar, g_backlight.pch);

    if (g_bl_mmio != 0) {
        UINT32 c1 = bl_rd(BLC_CTL1), r54 = bl_rd(BLC_CTL2), r58 = bl_rd(BXT_DUTY);
        kprintf(out, "at boot: C8250=%08x C8254=%08x C8258=%08x\n",
                g_backlight.ctl1, g_backlight.r54, g_backlight.r58);
        kprintf(out, "now:     C8250=%08x C8254=%08x C8258=%08x\n", c1, r54, r58);
        kprintf(out, "PWM %s, polarity %s; as PCH: max %u duty %u; as BXT: max %u duty %u\n",
                (c1 & BLC_ENABLE) ? "on" : "off", (c1 & BLC_POLARITY) ? "inverted" : "normal",
                r54 >> 16, r54 & 0xFFFFu, r54, r58);
    }

    kprintf(out, "mode: %s (%s layout)%s%s; ACPI _BCM levels: %u; Fn key events: %u\n",
            g_backlight.mode == BL_NATIVE ? "Intel PWM" :
            g_backlight.mode == BL_ACPI ? "ACPI _BCM" : "none",
            g_backlight.bxt ? "BXT" : "PCH",
            g_backlight.why ? " - " : "", g_backlight.why ? g_backlight.why : "",
            (UINT32)g_bl_nlevels, g_backlight.hotkeys);
}

/* Команда brightness [N | + | - | debug] */
void kernel_cmd_brightness(SIMPLE_TEXT_OUTPUT_INTERFACE *out, const char *arg)
{
    if (g_backlight.mode == BL_NONE) {
        print(out, "No backlight control on this machine (GPU registers and ACPI _BCM not found).\n");
        if (g_backlight.gpu_id)
            kprintf(out, "Intel GPU %04x: PWM registers read as disabled or zero.\n",
                    g_backlight.gpu_id);
        return;
    }

    while (*arg == ' ')
        arg++;

    if (arg[0] == 'd' && arg[1] == 'e' && arg[2] == 'b' && arg[3] == 'u' && arg[4] == 'g') {
        backlight_debug(out);
        return;
    }

    if (*arg == '+' || *arg == '-') {
        backlight_step(*arg == '+' ? +1 : -1);
    } else if (*arg >= '0' && *arg <= '9') {
        INT32 v = 0;
        while (*arg >= '0' && *arg <= '9')
            v = v * 10 + (*arg++ - '0');
        backlight_set(v);
    } else if (*arg != '\0') {
        print(out, "Usage: brightness [0..100 | + | - | debug]\n");
        return;
    }

    kprintf(out, "Brightness: %d%% (%s", backlight_get(),
            g_backlight.mode == BL_NATIVE ? "Intel PWM" : "ACPI _BCM");
    if (g_backlight.hotkeys)
        kprintf(out, ", Fn keys pressed %u times", g_backlight.hotkeys);
    print(out, ")\n");
}
