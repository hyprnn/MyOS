/*
 * kernel/time.c - TSC, калибровка по PIT, Local APIC timer, задержки.
 * Часть MyOS; общие объявления - в myos.h.
 */
#include "myos.h"

/*
 * TSC (Time Stamp Counter) - счётчик тактов процессора, читается
 * одной инструкцией rdtsc. Сам по себе он не знает, сколько
 * тактов в секунде, - эту частоту нужно один раз измерить
 * (откалибровать) по эталонным часам. Это делает переход в kernel
 * mode (команда "ebs", см. kx_pit_measure_tsc_hz). Пока
 * g_tsc_hz == 0, busy_wait_ms ниже работает по-старому, грубым
 * счётчиком - так, как и было до этого шага.
 */
UINT64 g_tsc_hz = 0;

void tsc_delay_us(UINT64 us)
{
    UINT64 start = rdtsc();
    UINT64 cycles = (us * g_tsc_hz) / 1000000ull;

    while ((rdtsc() - start) < cycles)
        cpu_pause();
}

void busy_wait_ms(UINTN ms)
{
    if (g_tsc_hz != 0) {

        /* таймер откалиброван - точная задержка по TSC */
        tsc_delay_us((UINT64)ms * 1000ull);
        return;
    }

    for (UINTN ms_i = 0; ms_i < ms; ms_i++) {

        for (
            volatile UINT64 spin = 0;
            spin < XHCI_SPIN_PER_MS;
            spin++
        ) { }
    }
}


/* ================================================================
 * 3. Время: TSC + PIT (калибровка) + Local APIC timer
 * ================================================================ */

UINT64 g_tsc_hz_stall = 0;   /* замер через Stall прошивки
                                        (ещё до ExitBootServices) */
UINT64 g_tsc_hz_hpet = 0;    /* замер по HPET (ACPI) */
UINT64 g_tsc_hz_pmtmr = 0;   /* замер по таймеру ACPI PM */
UINT64 g_tsc_hz_pit = 0;     /* замер через PIT - уже без
                                        прошивки */
const char *g_tsc_source = "none";

BOOLEAN g_lapic_x2 = FALSE;
UINT64  g_lapic_base = 0;
UINT64  g_lapic_hz = 0;      /* частота счётчика LAPIC
                                        timer (после делителя) */
BOOLEAN g_ktimer_ok = FALSE; /* прерывания таймера реально
                                        приходят */
UINT64  g_kboot_tsc = 0;     /* TSC в момент перехода в
                                        kernel mode */


/*
 * Замер частоты TSC по PIT. PIT (Intel 8254) тикает с
 * фиксированной, известной с 1981 года частотой 1193182 Гц -
 * это и делает его эталоном. Канал 2 (исторически - для
 * PC-спикера) можно использовать без прерываний: заряжаем его
 * на N тиков в режиме 0 ("прерывание по окончании счёта") и
 * смотрим бит 5 порта 0x61 - это выход канала 2, он становится
 * 1, когда счёт дошёл до нуля. Параллельно читаем TSC до и
 * после - получаем, сколько тактов TSC уложилось в N/1193182
 * секунды. Тот же приём использует Linux (pit_calibrate_tsc).
 *
 * Возвращает 0, если PIT не отвечает (на некоторых новых
 * платформах прошивка отключает тактирование 8254 ради
 * экономии энергии).
 */
UINT64 kx_pit_measure_tsc_hz(void)
{
    const UINT32 pit_hz = 1193182u;
    const UINT32 latch = 11932u;        /* ~10 мс */

    UINT64 best = 0;

    for (UINTN attempt = 0; attempt < 3; attempt++) {

        /* бит0 порта 0x61 = gate канала 2 (включаем),
           бит1 = сам динамик (выключаем, чтобы не пищал) */
        UINT8 p61 = io_in8(0x61);

        io_out8(0x61, (UINT8)((p61 & ~0x02u) | 0x01u));

        /* 0xB0 = канал 2, запись lo+hi байта, режим 0, двоичный */
        io_out8(0x43, 0xB0);
        io_out8(0x42, (UINT8)(latch & 0xFFu));
        io_out8(0x42, (UINT8)(latch >> 8));

        UINT64 t0 = rdtsc();
        UINT64 spins = 0;
        BOOLEAN done = FALSE;

        /*
         * Страховка на случай "мёртвого" PIT (на новых платформах
         * Intel прошивка может отключить его тактирование): ждём
         * не дольше ~100 мс по контрольному замеру через Stall.
         * Ограничивать просто числом итераций нельзя - каждое
         * чтение порта на реальном железе стоит около микросекунды,
         * и "200 миллионов попыток" растянулись бы на минуты.
         */
        UINT64 max_cycles =
            (g_tsc_hz_stall != 0) ? (g_tsc_hz_stall / 10u)
                                  : 400000000ull;

        for (;;) {

            if (io_in8(0x61) & 0x20u) {
                done = TRUE;
                break;
            }

            spins++;

            if ((spins & 0xFFu) == 0 && (rdtsc() - t0) > max_cycles)
                break;
        }

        UINT64 t1 = rdtsc();

        if (!done)
            return 0;

        UINT64 cycles = t1 - t0;
        UINT64 hz = (cycles * (UINT64)pit_hz) / (UINT64)latch;

        /* берём минимальный замер - любые задержки (например,
           SMI прошивки посреди замера) только увеличивают
           результат, никогда не уменьшают */
        if (best == 0 || hz < best)
            best = hz;
    }

    return best;
}


void kx_lapic_write(UINT32 reg, UINT32 v)
{
    if (g_lapic_x2)
        kx_wrmsr(0x800u + (reg >> 4), v);
    else
        mmio_write32(g_lapic_base + reg, v);
}

UINT32 kx_lapic_read(UINT32 reg)
{
    if (g_lapic_x2)
        return (UINT32)kx_rdmsr(0x800u + (reg >> 4));

    return mmio_read32(g_lapic_base + reg);
}

void kx_lapic_eoi(void)
{
    if (g_lapic_base == 0 && !g_lapic_x2)
        return;

    kx_lapic_write(0xB0, 0);
}


/*
 * Local APIC - встроенный в каждое ядро процессора контроллер
 * прерываний; в нём есть свой таймер, который мы и используем
 * как системный "тик" (1000 раз в секунду). Регистры:
 *   0x0F0 SVR  - включение APIC + вектор "ложного" прерывания
 *   0x080 TPR  - порог приоритета (0 = принимать всё)
 *   0x320 LVT Timer - вектор таймера + режим (бит17=периодический)
 *                     + маска (бит16)
 *   0x3E0 делитель, 0x380 начальный счёт, 0x390 текущий счёт
 *   0x0B0 EOI  - "прерывание обработано"
 * Доступ - либо как к памяти (xAPIC, адрес из MSR 0x1B), либо
 * через MSR 0x800+ (режим x2APIC - его прошивка могла включить
 * на новых машинах, тогда MMIO-окно уже не работает).
 */
BOOLEAN kx_lapic_timer_start(SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    UINT64 apic_msr = kx_rdmsr(0x1B);

    if (!(apic_msr & (1ull << 11))) {

        /* APIC глобально выключен - включаем */
        apic_msr |= (1ull << 11);
        kx_wrmsr(0x1B, apic_msr);
    }

    g_lapic_x2 = (apic_msr & (1ull << 10)) ? TRUE : FALSE;
    g_lapic_base = apic_msr & 0x000FFFFFFFFFF000ull;

    print(out, "  Local APIC: ");
    print(out, g_lapic_x2 ? "x2APIC mode (MSR access)" :
                            "xAPIC mode, MMIO at 0x");

    if (!g_lapic_x2)
        print_hex(out, g_lapic_base, 8);

    print(out, "\n");

    kx_lapic_write(0x0F0, 0x100u | KX_VEC_SPURIOUS);
    kx_lapic_write(0x080, 0);

    /* Калибровка: разовый счёт с маской, 50 мс по TSC (чем
       длиннее окно, тем меньше влияет случайная задержка в
       начале/конце замера) */
    kx_lapic_write(0x3E0, 0x3);                 /* делитель 16 */
    kx_lapic_write(0x320, (1u << 16) | KX_VEC_TIMER);
    kx_lapic_write(0x380, 0xFFFFFFFFu);

    tsc_delay_us(50000);

    UINT32 left = kx_lapic_read(0x390);

    kx_lapic_write(0x380, 0);

    UINT64 ticks = 0xFFFFFFFFull - (UINT64)left;

    g_lapic_hz = ticks * 20u;

    print(out, "  LAPIC timer: ");
    print_uint(out, g_lapic_hz / 1000u);
    print(out, " kHz after divide-by-16\n");

    if (g_lapic_hz < 1000u) {

        print(out, "  LAPIC timer does not count - no timer interrupts.\n");
        return FALSE;
    }

    /* Периодический режим, 1000 Гц */
    kx_lapic_write(0x320, (1u << 17) | KX_VEC_TIMER);
    kx_lapic_write(0x380, (UINT32)(g_lapic_hz / 1000u));

    return TRUE;
}


/* Микросекунды с момента перехода в kernel mode (по TSC) */
UINT64 kx_uptime_us(void)
{
    if (g_tsc_hz == 0)
        return 0;

    UINT64 d = rdtsc() - g_kboot_tsc;

    /* без 128-битной арифметики (её поддержка - функции
       libgcc, которых у нас нет): целая часть секунд отдельно,
       остаток отдельно - так ничего не переполняется */
    return (d / g_tsc_hz) * 1000000ull +
           ((d % g_tsc_hz) * 1000000ull) / g_tsc_hz;
}


/*
 * Пауза. Если таймер реально тикает - процессор между тиками
 * спит на инструкции hlt (просыпается от каждого прерывания,
 * раз в миллисекунду), а не молотит впустую: в QEMU это видно
 * по загрузке CPU хоста. Точность всё равно по TSC.
 */
void kx_sleep_us(UINT64 us)
{
    if (g_tsc_hz == 0) {
        busy_wait_ms((UINTN)((us + 999u) / 1000u));
        return;
    }

    UINT64 start = rdtsc();
    UINT64 cycles = (us * g_tsc_hz) / 1000000ull;

    while ((rdtsc() - start) < cycles) {

        /* hlt - процессор спит до ближайшего прерывания: таймера
           (раз в 1 мс) или устройства (клавиша, движение мыши). Время
           сна идёт в учёт загрузки процессора (irq.c). Для совсем
           коротких пауз - просто ждём. */
        if (g_ktimer_ok && us >= 500u)
            kx_idle_hlt();
        else
            cpu_pause();
    }
}
