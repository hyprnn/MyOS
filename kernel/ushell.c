/*
 * kernel/ushell.c - кто ведёт диалог с человеком в текстовой консоли
 * (этап 10). Часть MyOS; общие объявления - в myos.h.
 *
 * Раньше шелл был частью ядра: цикл "прочитать строку - выполнить"
 * прямо в kmain. Теперь шелл - обычная программа /bin/sh (ring 3): свой
 * редактор строки и история, запуск программ (в том числе в фоне,
 * "&"), вывод в файл (">"). Встроенные команды ядра (net, wifi,
 * battery, cpu, disk...) он выполняет системным вызовом SYS_KCMD.
 *
 * Ядро здесь только "присматривает": запускает /bin/sh и ждёт. Шелл
 * закрыли командой exit - это, как и раньше, выключение. Шелл упал -
 * запустить снова. Упал трижды подряд за 10 секунд (или его нет на
 * диске) - значит, с ним что-то всерьёз не так: тогда старый шелл
 * ядра - "аварийный", чтобы машиной всё равно можно было управлять.
 */
#include "myos.h"

/* Аварийный шелл: прежний цикл шелла ядра (shell/commands.c) */
static void __attribute__((noreturn)) emergency_shell(const char *why)
{
    SIMPLE_TEXT_OUTPUT_INTERFACE *out = g_st->ConOut;

    set_color(out, 0x0C);
    kprintf(out, "\n/bin/sh %s - this is the EMERGENCY shell built into the kernel.\n", why);
    set_color(out, 0x07);
    print(out, "All kernel commands work ('help'); programs from /bin run as before.\n\n");
    klog("ushell: emergency shell (%s)\n", why);

    CHAR16 line[LINE_MAX];

    for (;;) {

        out = g_st->ConOut;

        set_color(out, g_color);
        kprintf(out, "%s# ", g_cwd);

        read_line(g_st, line, LINE_MAX);
        run_command(g_st, line);
    }
}

void kernel_shell_main(void)
{
    UINT64 fails_since = 0;
    UINT32 fails = 0;

    for (;;) {

        INTN err = VFS_OK;
        KPROC *p = proc_spawn("/bin/sh", "", PROC_IO_CONSOLE, &err);

        if (p == NULL)
            emergency_shell("could not be started");

        /* шелл - на переднем плане: Ctrl+C он получает клавишей */
        g_fg_proc = p;

        INT64 code = proc_wait(p);

        g_fg_proc = NULL;

        if (code == 0) {
            /* exit - как и раньше в MyOS, выключение */
            CHAR16 cmd[] = L"shutdown";
            run_command(g_st, cmd);
            continue;
        }

        /* упал - записать, запустить снова */
        SIMPLE_TEXT_OUTPUT_INTERFACE *out = g_st->ConOut;

        set_color(out, 0x0C);
        kprintf(out, "\n*** /bin/sh stopped (code %lld) - starting it again.\n", code);
        set_color(out, 0x07);
        klog("ushell: /bin/sh stopped with code %lld, restarting\n", code);

        if (fails == 0 || g_kticks - fails_since > 10000u) {
            fails = 0;
            fails_since = g_kticks;
        }

        if (++fails >= 3)
            emergency_shell("keeps crashing");
    }
}
