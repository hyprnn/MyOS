/*
 * sh - шелл MyOS, обычная программа в ring 3 (этап 10).
 *
 * Раньше шелл был частью ядра. Теперь ядро только запускает эту
 * программу (kernel/ushell.c) и присматривает за ней, а всё остальное
 * делает она:
 *   * редактор строки: стрелки влево/вправо, Home/End, Backspace,
 *     Delete, история стрелками вверх/вниз, Ctrl+C - стереть строку;
 *   * встроенные команды: cd, pwd, jobs, history, help;
 *   * программы из /bin (и по пути) - на переднем плане или в фоне
 *     ("primes 1000000 &"), вывод - в файл ("ls > list.txt",
 *     ">> list.txt" - дописать);
 *   * всё остальное (net, wifi, battery, cpu, disk, start...) - это
 *     встроенные команды ЯДРА: они смотрят в его внутренности, и
 *     выполняет их ядро системным вызовом kcmd. Если ядро такой
 *     команды не знает, kcmd ничего не печатает и отвечает 0 - тогда
 *     ищем программу с этим именем.
 *
 * Экран - текстовая консоль ядра: '\r' - в начало строки, '\b' - на
 * символ влево. Кириллицу консоль пока не рисует (вместо неё '?'),
 * но в строке она хранится честно и уходит в команду как UTF-8.
 */
#include "myos.h"

#define LINE_CP    120     /* символов в строке ввода */
#define HIST_N     32      /* строк истории */
#define JOBS_N     8       /* программ в фоне */
#define CMD_BYTES  512     /* строка команды в UTF-8 */

/* ---------------- вывод ---------------- */

static void out_str(const char *s)
{
    write(1, s, strlen(s));
}

static void out_ch(char c)
{
    write(1, &c, 1);
}

/* один символ строки ввода на экране: консоль знает только ASCII */
static void out_cp(unsigned int cp)
{
    out_ch(cp < 128 ? (char)cp : '?');
}

static void out_back(int n)
{
    char buf[LINE_CP + 1];

    if (n <= 0)
        return;

    if (n > LINE_CP)
        n = LINE_CP;

    memset(buf, '\b', (size_t)n);
    write(1, buf, (size_t)n);
}

/* ---------------- UTF-8 ---------------- */

static int cp_to_utf8(const unsigned int *cp, int n, char *dst, int cap)
{
    int k = 0;

    for (int i = 0; i < n; i++) {

        unsigned int c = cp[i];

        if (c < 0x80) {
            if (k + 1 >= cap) break;
            dst[k++] = (char)c;
        } else if (c < 0x800) {
            if (k + 2 >= cap) break;
            dst[k++] = (char)(0xC0 | (c >> 6));
            dst[k++] = (char)(0x80 | (c & 0x3F));
        } else {
            if (k + 3 >= cap) break;
            dst[k++] = (char)(0xE0 | (c >> 12));
            dst[k++] = (char)(0x80 | ((c >> 6) & 0x3F));
            dst[k++] = (char)(0x80 | (c & 0x3F));
        }
    }

    dst[k] = '\0';
    return k;
}

static int utf8_to_cp(const char *s, unsigned int *cp, int cap)
{
    int n = 0;
    const unsigned char *u = (const unsigned char *)s;

    while (*u && n < cap) {
        unsigned int c = *u++;
        if (c >= 0xC0 && c < 0xE0 && *u) {
            c = ((c & 0x1F) << 6) | (*u++ & 0x3F);
        } else if (c >= 0xE0 && u[0] && u[1]) {
            c = ((c & 0x0F) << 12) | ((unsigned int)(u[0] & 0x3F) << 6) | (u[1] & 0x3F);
            u += 2;
        }
        cp[n++] = c;
    }

    return n;
}

/* ---------------- история ---------------- */

static char g_hist[HIST_N][CMD_BYTES];
static int  g_hist_count;          /* всего добавлено (номер следующей) */

static void hist_add(const char *line)
{
    if (line[0] == '\0')
        return;

    /* ту же команду подряд - не повторять */
    if (g_hist_count > 0 && strcmp(g_hist[(g_hist_count - 1) % HIST_N], line) == 0)
        return;

    strncpy(g_hist[g_hist_count % HIST_N], line, CMD_BYTES - 1);
    g_hist[g_hist_count % HIST_N][CMD_BYTES - 1] = '\0';
    g_hist_count++;
}

/* ---------------- редактор строки ---------------- */

static char g_prompt[160];
static unsigned int g_line[LINE_CP];
static int g_len, g_pos;
static int g_drawn;                /* сколько символов строки нарисовано */

static void make_prompt(void)
{
    char cwd[128];

    if (getcwd_len(cwd, sizeof(cwd)) < 0)
        strcpy(cwd, "?");

    snprintf(g_prompt, sizeof(g_prompt), "%s> ", cwd);
}

/* Нарисовать приглашение и строку заново (после истории, PageUp) */
static void redraw(void)
{
    out_ch('\r');
    out_str(g_prompt);

    for (int i = 0; i < g_len; i++)
        out_cp(g_line[i]);

    /* стереть хвост прежней, более длинной строки */
    int extra = g_drawn - g_len;

    for (int i = 0; i < extra; i++)
        out_ch(' ');

    out_back(extra + (g_len - g_pos));
    g_drawn = g_len;
}

static void set_line_from(const char *utf8)
{
    g_len = utf8_to_cp(utf8, g_line, LINE_CP);
    g_pos = g_len;
    redraw();
}

/* Прочитать строку. Возвращает её в UTF-8. */
static void read_command(char *out, int cap)
{
    int hist_pos = g_hist_count;       /* "ниже последней" - новая строка */

    g_len = 0;
    g_pos = 0;
    g_drawn = 0;

    out_str(g_prompt);

    for (;;) {

        int k = readkey(-1);

        if (k < 0)
            continue;

        if (k == MYOS_KEY_REDRAW) {
            redraw();
            continue;
        }

        if (k == '\r' || k == '\n') {
            out_str("\n");
            break;
        }

        if (k == 3) {                          /* Ctrl+C - забыть строку */
            out_str("^C\n");
            g_len = 0;
            g_pos = 0;
            g_drawn = 0;
            hist_pos = g_hist_count;
            out_str(g_prompt);
            continue;
        }

        if (k == '\b') {                       /* Backspace */
            if (g_pos > 0) {
                memmove(&g_line[g_pos - 1], &g_line[g_pos],
                        (size_t)(g_len - g_pos) * sizeof(g_line[0]));
                g_pos--;
                g_len--;
                out_ch('\b');
                for (int i = g_pos; i < g_len; i++)
                    out_cp(g_line[i]);
                out_ch(' ');
                out_back(g_len - g_pos + 1);
            }
            continue;
        }

        if (k & MYOS_KEY_SPECIAL) {

            int sc = k & 0xFFFF;

            if (sc == MYOS_SCAN_LEFT && g_pos > 0) {
                g_pos--;
                out_ch('\b');
            } else if (sc == MYOS_SCAN_RIGHT && g_pos < g_len) {
                out_cp(g_line[g_pos]);
                g_pos++;
            } else if (sc == MYOS_SCAN_HOME) {
                out_back(g_pos);
                g_pos = 0;
            } else if (sc == MYOS_SCAN_END) {
                for (; g_pos < g_len; g_pos++)
                    out_cp(g_line[g_pos]);
            } else if (sc == MYOS_SCAN_DELETE && g_pos < g_len) {
                memmove(&g_line[g_pos], &g_line[g_pos + 1],
                        (size_t)(g_len - g_pos - 1) * sizeof(g_line[0]));
                g_len--;
                for (int i = g_pos; i < g_len; i++)
                    out_cp(g_line[i]);
                out_ch(' ');
                out_back(g_len - g_pos + 1);
            } else if (sc == MYOS_SCAN_UP) {
                int oldest = g_hist_count > HIST_N ? g_hist_count - HIST_N : 0;
                if (hist_pos > oldest) {
                    hist_pos--;
                    set_line_from(g_hist[hist_pos % HIST_N]);
                }
            } else if (sc == MYOS_SCAN_DOWN) {
                if (hist_pos < g_hist_count) {
                    hist_pos++;
                    set_line_from(hist_pos < g_hist_count ? g_hist[hist_pos % HIST_N] : "");
                }
            } else if (sc == MYOS_SCAN_ESC) {
                /* Esc - стереть строку */
                out_back(g_pos);
                g_pos = 0;
                g_len = 0;
                redraw();
            }
            continue;
        }

        if (k == '\t')
            k = ' ';

        if (k < 32 || g_len >= LINE_CP - 1)
            continue;

        /* вставка в середину: сдвинуть хвост и перерисовать его */
        memmove(&g_line[g_pos + 1], &g_line[g_pos],
                (size_t)(g_len - g_pos) * sizeof(g_line[0]));
        g_line[g_pos] = (unsigned int)k;
        g_len++;

        for (int i = g_pos; i < g_len; i++)
            out_cp(g_line[i]);

        g_pos++;
        out_back(g_len - g_pos);
    }

    cp_to_utf8(g_line, g_len, out, cap);
}

/* ---------------- программы в фоне ---------------- */

struct job {
    int  pid;
    char cmd[64];
};

static struct job g_jobs[JOBS_N];

static void jobs_check(int print_running)
{
    for (int i = 0; i < JOBS_N; i++) {

        if (g_jobs[i].pid == 0)
            continue;

        struct myos_waitinfo wi;
        int r = waitpid_info(g_jobs[i].pid, &wi, MYOS_WAIT_NOHANG);

        if (r == 1) {
            if (wi.code == -1 && wi.why[0])
                printf("[%d] stopped: %s - %s\n", g_jobs[i].pid, g_jobs[i].cmd, wi.why);
            else
                printf("[%d] done: %s (code %d)\n", g_jobs[i].pid, g_jobs[i].cmd, wi.code);
            g_jobs[i].pid = 0;
        } else if (r < 0) {
            g_jobs[i].pid = 0;             /* её уже нет */
        } else if (print_running) {
            printf("[%d] running: %s\n", g_jobs[i].pid, g_jobs[i].cmd);
        }
    }
}

/* ---------------- выполнение ---------------- */

static char *trim(char *s)
{
    while (*s == ' ')
        s++;

    int n = (int)strlen(s);

    while (n > 0 && s[n - 1] == ' ')
        s[--n] = '\0';

    return s;
}

/* Программа закончилась - сказать, если что-то не так (как шелл ядра) */
static void report(const struct myos_waitinfo *wi)
{
    if (wi->code == -1 && wi->why[0]) {
        printf("\n*** %s (pid %d) was stopped: %s\n", wi->name, wi->pid, wi->why);
        printf("    MyOS keeps running - only the program was closed.\n");
    } else if (wi->code != 0) {
        printf("[%s exited with code %d]\n", wi->name, wi->code);
    }
}

/* Запустить программу; 1 - запустили, 0 - такой нет */
static int run_program(const char *name, const char *args, const char *out_path,
                       int append, int background, const char *whole)
{
    struct myos_spawn sp;

    sp.path = name;
    sp.args = args;
    sp.out_path = out_path;
    sp.flags = (background ? 0u : (unsigned)MYOS_SPAWN_FG) |
               (append ? (unsigned)MYOS_SPAWN_APPEND : 0u);
    sp.pad = 0;

    int pid = spawn(&sp);

    if (pid == MYOS_ENOENT)
        return 0;

    if (pid < 0) {
        printf("Cannot run %s: %s\n", name, strerror(pid));
        return 1;
    }

    if (background) {
        for (int i = 0; i < JOBS_N; i++)
            if (g_jobs[i].pid == 0) {
                g_jobs[i].pid = pid;
                strncpy(g_jobs[i].cmd, whole, sizeof(g_jobs[i].cmd) - 1);
                g_jobs[i].cmd[sizeof(g_jobs[i].cmd) - 1] = '\0';
                printf("[%d] %s\n", pid, whole);
                return 1;
            }
        /* список фоновых полон - дождаться этой */
        printf("(too many background programs - waiting for this one)\n");
    }

    struct myos_waitinfo wi;

    if (waitpid_info(pid, &wi, 0) == 1)
        report(&wi);

    return 1;
}

static void help(void)
{
    out_str("MyOS shell (/bin/sh) - a program, not part of the kernel:\n"
            "  cd DIR, pwd       - current folder     history - last commands\n"
            "  PROGRAM &         - run in background  jobs    - background programs\n"
            "  CMD > FILE        - output to a file (>> FILE - append)\n"
            "  keys: Left/Right, Home/End, Del, Up/Down (history), Ctrl+C, PageUp/Down\n"
            "Kernel commands:\n");
    kcmd("help", 0, 0);
}

static void execute(char *line)
{
    line = trim(line);

    if (line[0] == '\0')
        return;

    char whole[CMD_BYTES];
    strncpy(whole, line, sizeof(whole) - 1);
    whole[sizeof(whole) - 1] = '\0';

    /* "... &" - в фоне */
    int background = 0;
    int n = (int)strlen(line);

    if (n > 0 && line[n - 1] == '&') {
        background = 1;
        line[n - 1] = '\0';
        line = trim(line);
        whole[strlen(whole) - 1] = '\0';
        trim(whole);
    }

    /* "... > файл" / "... >> файл" */
    char *out_path = 0;
    int append = 0;
    char *gt = strchr(line, '>');

    if (gt != 0) {
        *gt = '\0';
        if (gt[1] == '>') {
            append = 1;
            gt++;
        }
        out_path = trim(gt + 1);
        line = trim(line);
        if (out_path[0] == '\0') {
            out_str("Where to? Example: ls > files.txt\n");
            return;
        }
    }

    if (line[0] == '\0')
        return;

    /* имя и аргументы */
    char name[128];
    int k = 0;

    while (line[k] && line[k] != ' ' && k + 1 < (int)sizeof(name)) {
        name[k] = line[k];
        k++;
    }

    name[k] = '\0';

    char *args = trim(line + k);

    /* ---- встроенные ---- */

    if (strcmp(name, "cd") == 0) {
        int r = chdir(args[0] ? args : "/");
        if (r < 0)
            printf("cd: %s: %s\n", args[0] ? args : "/", strerror(r));
        return;
    }

    if (strcmp(name, "pwd") == 0) {
        char cwd[128];
        if (getcwd_len(cwd, sizeof(cwd)) >= 0)
            printf("%s\n", cwd);
        return;
    }

    if (strcmp(name, "jobs") == 0) {
        int any = 0;
        for (int i = 0; i < JOBS_N; i++)
            if (g_jobs[i].pid)
                any = 1;
        if (!any)
            out_str("No background programs.\n");
        jobs_check(1);
        return;
    }

    if (strcmp(name, "history") == 0) {
        int first = g_hist_count > HIST_N ? g_hist_count - HIST_N : 0;
        for (int i = first; i < g_hist_count; i++)
            printf("%3d  %s\n", i + 1, g_hist[i % HIST_N]);
        return;
    }

    if (strcmp(name, "help") == 0 && args[0] == '\0' && out_path == 0) {
        help();
        return;
    }

    /* "run ПУТЬ аргументы" - точно программа */
    if (strcmp(name, "run") == 0) {
        char prog[128];
        int j = 0;
        while (args[j] && args[j] != ' ' && j + 1 < (int)sizeof(prog)) {
            prog[j] = args[j];
            j++;
        }
        prog[j] = '\0';
        if (!run_program(prog, trim(args + j), out_path, append, background, whole))
            printf("Cannot run %s: no such program\n", prog);
        return;
    }

    /* ---- в фоне - только программы ---- */
    if (background) {
        if (run_program(name, args, out_path, append, 1, whole))
            return;
        out_str("(this is a kernel command - it runs in the foreground)\n");
    }

    /* ---- команда ядра? ---- */
    char cmdline[CMD_BYTES];
    snprintf(cmdline, sizeof(cmdline), "%s%s%s", name, args[0] ? " " : "", args);

    int r = kcmd(cmdline, out_path, append ? MYOS_KCMD_APPEND : 0);

    if (r == 1)
        return;

    if (r < 0 && r != MYOS_ENOSYS) {
        printf("%s: %s\n", out_path ? out_path : name, strerror(r));
        return;
    }

    /* ---- программа ---- */
    if (run_program(name, args, out_path, append, 0, whole))
        return;

    printf("Unknown command: %s\n(type 'help')\n", whole);
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    char line[CMD_BYTES];

    for (;;) {

        jobs_check(0);
        make_prompt();
        read_command(line, sizeof(line));
        hist_add(line);
        execute(line);
    }
}
