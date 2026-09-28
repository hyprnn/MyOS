/*
 * kernel/tz.c - часовые пояса: Москва и Иерусалим.
 * Часть MyOS; общие объявления - в myos.h.
 *
 * Микросхема часов (CMOS RTC) хранит время по UTC - так её
 * настраивает Linux (и Arch на ноутбуке), и так её показывает QEMU.
 * Раньше MyOS выводила это время как есть, поэтому часы отставали
 * от настоящих на 2-3 часа. Теперь ядро переводит UTC в выбранный
 * пояс - один раз, в GetTime (shim.c), поэтому правильное время
 * сразу видят все: часы в GUI, time, date, fetch, терминал GUI.
 *
 * Пояса:
 *   Москва    - UTC+3 круглый год (перехода на летнее время нет
 *               с 2014 года), обозначение MSK;
 *   Иерусалим - зимой UTC+2 (IST, Israel Standard Time), летом
 *               UTC+3 (IDT, Israel Daylight Time). По закону Израиля
 *               2013 года летнее время идёт с пятницы перед
 *               последним воскресеньем марта (02:00) до последнего
 *               воскресенья октября (02:00).
 * Поэтому с конца марта до конца октября Москва и Иерусалим
 * показывают одно и то же время, а зимой Иерусалим на час позади.
 *
 * Переключение: команда "tz" в шелле, клик по часам на панели
 * задач GUI или клавиша T на рабочем столе.
 */
#include "myos.h"

UINTN g_tz = TZ_MOSCOW;

static BOOLEAN tz_leap(UINTN y)
{
    return (y % 4u == 0 && y % 100u != 0) || y % 400u == 0;
}

static UINTN tz_days_in_month(UINTN y, UINTN m)
{
    static const UINT8 d[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

    if (m == 2 && tz_leap(y))
        return 29;

    return (m >= 1 && m <= 12) ? d[m - 1] : 30;
}

/* День недели: 0 = воскресенье ... 6 = суббота
   (формула Сакамото, работает для любого года григорианского
   календаря) */
static UINTN tz_weekday(UINTN y, UINTN m, UINTN d)
{
    static const UINT8 t[12] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };

    if (m < 3)
        y -= 1;

    return (y + y / 4u - y / 100u + y / 400u + t[m - 1] + d) % 7u;
}

/* Число последнего воскресенья месяца */
static UINTN tz_last_sunday(UINTN y, UINTN m)
{
    UINTN last = tz_days_in_month(y, m);

    return last - tz_weekday(y, m, last);
}

/* Минут от начала года (для сравнения моментов внутри года) */
static UINT32 tz_minute_of_year(UINTN y, UINTN m, UINTN d, UINTN h, UINTN mi)
{
    UINT32 days = 0;

    for (UINTN k = 1; k < m; k++)
        days += (UINT32)tz_days_in_month(y, k);

    days += (UINT32)(d - 1u);

    return days * 1440u + (UINT32)h * 60u + (UINT32)mi;
}

/*
 * Идёт ли летнее время в Израиле в момент utc (время UTC).
 * Переходы по UTC:
 *   начало - пятница перед последним воскресеньем марта, 02:00 IST
 *            = 00:00 UTC той же пятницы;
 *   конец  - последнее воскресенье октября, 02:00 IDT = 23:00 UTC
 *            субботы накануне.
 */
static BOOLEAN tz_israel_dst(const EFI_TIME *utc)
{
    UINTN y = utc->Year;

    UINTN mar_sun = tz_last_sunday(y, 3);
    UINTN oct_sun = tz_last_sunday(y, 10);

    /* пятница = воскресенье - 2 (последнее воскресенье марта всегда
       позже 24-го, так что пятница тоже в марте) */
    UINT32 start = tz_minute_of_year(y, 3, mar_sun - 2u, 0, 0);

    /* суббота 23:00 = воскресенье 00:00 минус час */
    UINT32 end = tz_minute_of_year(y, 10, oct_sun, 0, 0) - 60u;

    UINT32 now = tz_minute_of_year(y, utc->Month, utc->Day, utc->Hour, utc->Minute);

    return now >= start && now < end;
}

/* Смещение пояса от UTC в минутах в момент utc */
INT32 tz_offset_minutes(const EFI_TIME *utc)
{
    if (g_tz == TZ_JERUSALEM)
        return tz_israel_dst(utc) ? 180 : 120;

    return 180;     /* Москва */
}

/* Короткое обозначение (для часов GUI): MSK, IST или IDT */
const char *tz_abbrev(const EFI_TIME *utc)
{
    if (g_tz == TZ_JERUSALEM)
        return tz_israel_dst(utc) ? "IDT" : "IST";

    return "MSK";
}

const char *tz_city(void)
{
    return (g_tz == TZ_JERUSALEM) ? "Jerusalem" : "Moscow";
}

/* Сдвинуть время t на minutes минут (с переносом через полночь,
   конец месяца и года) */
static void tz_add_minutes(EFI_TIME *t, INT32 minutes)
{
    INT32 total = (INT32)t->Hour * 60 + (INT32)t->Minute + minutes;
    INT32 day_shift = 0;

    while (total < 0) {
        total += 1440;
        day_shift--;
    }

    while (total >= 1440) {
        total -= 1440;
        day_shift++;
    }

    t->Hour = (UINT8)(total / 60);
    t->Minute = (UINT8)(total % 60);

    for (; day_shift > 0; day_shift--) {
        if (t->Day < tz_days_in_month(t->Year, t->Month)) {
            t->Day++;
        } else {
            t->Day = 1;
            if (t->Month < 12) {
                t->Month++;
            } else {
                t->Month = 1;
                t->Year++;
            }
        }
    }

    for (; day_shift < 0; day_shift++) {
        if (t->Day > 1) {
            t->Day--;
        } else {
            if (t->Month > 1) {
                t->Month--;
            } else {
                t->Month = 12;
                t->Year--;
            }
            t->Day = (UINT8)tz_days_in_month(t->Year, t->Month);
        }
    }
}

/* UTC -> местное время выбранного пояса (на месте) */
void tz_to_local(EFI_TIME *t)
{
    INT32 off = tz_offset_minutes(t);
    BOOLEAN dst = (g_tz == TZ_JERUSALEM) && tz_israel_dst(t);

    tz_add_minutes(t, off);

    t->TimeZone = (INT16)off;
    t->Daylight = dst ? 0x02 : 0x00;   /* EFI_TIME_IN_DAYLIGHT */
}

/* Следующий пояс по кругу (для клика по часам) */
void tz_toggle(void)
{
    g_tz = (g_tz == TZ_MOSCOW) ? TZ_JERUSALEM : TZ_MOSCOW;
    klog("time zone: %s\n", tz_city());
}

/* Строка "Moscow (MSK, UTC+3)" - для шелла */
void tz_describe(char *buf, UINTN cap)
{
    EFI_TIME utc;

    if (!rtc_read(&utc)) {
        ksnprintf(buf, cap, "%s", tz_city());
        return;
    }

    INT32 off = tz_offset_minutes(&utc);

    ksnprintf(buf, cap, "%s (%s, UTC+%d%s)", tz_city(), tz_abbrev(&utc),
              off / 60,
              (g_tz == TZ_JERUSALEM) ?
                  (off == 180 ? ", summer time" : ", winter time") : "");
}

/* ================================================================
 * Команда tz
 * ================================================================ */

void kernel_cmd_tz(SIMPLE_TEXT_OUTPUT_INTERFACE *out, const char *arg)
{
    if (arg[0] != '\0') {

        if (kstreq(arg, "msk") || kstreq(arg, "moscow") || kstreq(arg, "m"))
            g_tz = TZ_MOSCOW;
        else if (kstreq(arg, "jer") || kstreq(arg, "jerusalem") ||
                 kstreq(arg, "israel") || kstreq(arg, "j"))
            g_tz = TZ_JERUSALEM;
        else if (kstreq(arg, "toggle") || kstreq(arg, "t"))
            tz_toggle();
        else {
            print(out, "Usage: tz              - show the time zone\n");
            print(out, "       tz msk          - Moscow time (UTC+3)\n");
            print(out, "       tz jer          - Jerusalem time (UTC+2 winter / UTC+3 summer)\n");
            print(out, "       tz toggle       - switch between them\n");
            print(out, "In the GUI: click the clock on the taskbar, or press T.\n");
            return;
        }
    }

    char d[64];
    tz_describe(d, sizeof(d));

    EFI_TIME t;

    if (krt_get_time(&t, NULL) == EFI_SUCCESS)
        kprintf(out, "Time zone: %s - now %02u:%02u:%02u, %02u.%02u.%04u\n",
                d, t.Hour, t.Minute, t.Second, t.Day, t.Month, t.Year);
    else
        kprintf(out, "Time zone: %s (clock not available)\n", d);

    if (arg[0] == '\0')
        print(out, "Switch: tz msk | tz jer | tz toggle (GUI: click the clock or press T)\n");
}
