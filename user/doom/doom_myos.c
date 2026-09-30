/*
 * user/doom/doom_myos.c - DOOM на MyOS (этап 10, Б): "платформа" для
 * doomgeneric (third_party/doomgeneric - сам DOOM 1993 года в переносе
 * Chocolate Doom, GPL v2).
 *
 * doomgeneric просит у платформы пять вещей, всё остальное - сам:
 *   DG_Init        - завести окно;
 *   DG_DrawFrame   - показать готовый кадр (640x400, 0x00RRGGBB);
 *   DG_GetKey      - очередную клавишу: нажата/отпущена и какая;
 *   DG_GetTicksMs, DG_SleepMs - часы.
 * Звук - в doom_sound.c.
 *
 * Клавиши: окну программы рабочий стол присылает EV_RAWKEY - какую
 * физическую клавишу (HID Usage) нажали или ОТПУСТИЛИ (игре нужно
 * знать, что стрелку всё ещё держат - обычные EV_KEY этого не скажут).
 *
 * Управление как в оригинале: стрелки - идти/поворачивать, Ctrl -
 * стрелять, пробел - открыть дверь, Shift - бежать, Alt - шаг вбок,
 * 1..7 - оружие, Esc - меню, Tab - карта.
 *
 * Где файл игры (WAD): doom -iwad путь; иначе ищем doom1.wad, doom.wad,
 * doom2.wad, freedoom1.wad, freedoom2.wad в текущей папке, в корне
 * каждого диска и в папках EFI/MyOS и doom на них. Бесплатный doom1.wad
 * (shareware) кладёт на флешку make (esp/DOOM1.WAD) и установщик.
 */
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "myos_sys.h"

#include "doomgeneric.h"

/* у MyOS (sysnum.h) и у DOOM есть одноимённые KEY_* с другими
   значениями - здесь нужны DOOMовские */
#undef KEY_HOME
#undef KEY_END
#undef KEY_PGUP
#undef KEY_PGDN
#undef KEY_DEL
#include "doomkeys.h"

static int g_win;
static unsigned int *g_px;

/* очередь клавиш: (нажата << 8) | код DOOM */
#define KQ 64
static unsigned short g_keyq[KQ];
static int g_kq_head, g_kq_tail;

/* HID Usage (физическая клавиша) -> код клавиши DOOM (doomkeys.h) */
static int hid_to_doom(unsigned int u)
{
    if (u >= 0x04 && u <= 0x1D)
        return 'a' + (int)(u - 0x04);
    if (u >= 0x1E && u <= 0x26)
        return '1' + (int)(u - 0x1E);
    if (u == 0x27)
        return '0';
    if (u >= 0x3A && u <= 0x43)
        return KEY_F1 + (int)(u - 0x3A);

    switch (u) {
    case 0x28: return KEY_ENTER;
    case 0x29: return KEY_ESCAPE;
    case 0x2A: return KEY_BACKSPACE;
    case 0x2B: return KEY_TAB;
    case 0x2C: return KEY_USE;          /* пробел - открыть/нажать */
    case 0x2D: return KEY_MINUS;
    case 0x2E: return KEY_EQUALS;
    case 0x44: return KEY_F11;
    case 0x45: return KEY_F12;
    case 0x48: return KEY_PAUSE;
    case 0x4F: return KEY_RIGHTARROW;
    case 0x50: return KEY_LEFTARROW;
    case 0x51: return KEY_DOWNARROW;
    case 0x52: return KEY_UPARROW;
    case 0xE0: case 0xE4: return KEY_FIRE;      /* Ctrl - огонь */
    case 0xE1: case 0xE5: return KEY_RSHIFT;    /* Shift - бег */
    case 0xE2: case 0xE6: return KEY_RALT;      /* Alt - вбок */
    default:   return 0;
    }
}

/* Забрать события окна (не ждать): клавиши - в очередь, крестик - выход */
static void pump_events(void)
{
    struct myos_event e;

    while (myos_syscall3(SYS_WIN_EVENT, g_win, (long)&e, 0) == 1) {

        if (e.type == EV_CLOSE) {
            printf("doom: window closed\n");
            exit(0);
        }

        if (e.type != EV_RAWKEY)
            continue;

        int k = hid_to_doom(e.scan);
        int next = (g_kq_tail + 1) % KQ;

        if (k != 0 && next != g_kq_head) {
            g_keyq[g_kq_tail] = (unsigned short)((e.key ? 0x100 : 0) | (k & 0xFF));
            g_kq_tail = next;
        }
    }
}

void DG_Init(void)
{
    g_win = (int)myos_syscall3(SYS_WIN_CREATE, DOOMGENERIC_RESX, DOOMGENERIC_RESY, (long)"DOOM");

    if (g_win < 0) {
        printf("doom: no window (%d) - start the desktop first ('start')\n", g_win);
        exit(1);
    }

    g_px = (unsigned int *)(MYOS_WIN_BASE + (unsigned long long)(g_win - 1) * MYOS_WIN_SPAN);
}

void DG_DrawFrame(void)
{
    memcpy(g_px, DG_ScreenBuffer, DOOMGENERIC_RESX * DOOMGENERIC_RESY * 4);
    myos_syscall3(SYS_WIN_UPDATE, g_win, 0, 0);
    pump_events();
}

void DG_SleepMs(uint32_t ms)
{
    myos_syscall3(SYS_SLEEP, ms, 0, 0);
}

uint32_t DG_GetTicksMs(void)
{
    return (uint32_t)myos_syscall3(SYS_UPTIME, 0, 0, 0);
}

int DG_GetKey(int *pressed, unsigned char *key)
{
    pump_events();

    if (g_kq_head == g_kq_tail)
        return 0;

    unsigned short v = g_keyq[g_kq_head];
    g_kq_head = (g_kq_head + 1) % KQ;

    *pressed = (v >> 8) & 1;
    *key = (unsigned char)(v & 0xFF);
    return 1;
}

void DG_SetWindowTitle(const char *title)
{
    myos_syscall3(SYS_WIN_TITLE, g_win, (long)title, 0);
}

/* DOOM зовёт system() только чтобы показать окно ошибки через zenity
   (Linux) - в MyOS такого нет: "не получилось" */
int system(const char *cmd)
{
    (void)cmd;
    return -1;
}

/* ---------------- где файл игры ---------------- */

static const char *const g_wads[] = {
    "doom1.wad", "doom.wad", "doom2.wad", "freedoom1.wad", "freedoom2.wad", NULL
};

static int is_file(const char *p)
{
    struct stat st;
    return stat(p, &st) == 0 && S_ISREG(st.st_mode);
}

/* в папке dir (с '/' на конце или пустая) - один из g_wads */
static int find_in(const char *dir, char *out, size_t cap)
{
    for (int i = 0; g_wads[i]; i++) {
        snprintf(out, cap, "%s%s", dir, g_wads[i]);
        if (is_file(out))
            return 1;
    }
    return 0;
}

static int find_wad(char *out, size_t cap)
{
    if (find_in("", out, cap))
        return 1;

    /* тома: /usb0p1, /nvme0p1... (FAT без учёта регистра букв) */
    DIR *d = opendir("/");

    if (d == NULL)
        return 0;

    struct dirent *e;
    int found = 0;

    while (!found && (e = readdir(d)) != NULL) {

        if (e->d_name[0] == '.' || strcmp(e->d_name, "bin") == 0)
            continue;

        char dir[300];
        static const char *const sub[] = { "", "EFI/MyOS/", "doom/", NULL };

        for (int k = 0; sub[k] && !found; k++) {
            snprintf(dir, sizeof(dir), "/%s/%s", e->d_name, sub[k]);
            found = find_in(dir, out, cap);
        }
    }

    closedir(d);
    return found;
}

int main(int argc, char **argv)
{
    static char wad[320];
    int have_iwad = 0;

    for (int i = 1; i < argc; i++)
        if (strcmp(argv[i], "-iwad") == 0)
            have_iwad = 1;

    /* нет -iwad - найти самим и дописать в аргументы */
    char **av = argv;
    int ac = argc;

    if (!have_iwad) {

        if (!find_wad(wad, sizeof(wad))) {
            printf("doom: no game file found. Put doom1.wad (free shareware) or doom.wad /\n"
                   "doom2.wad / freedoom1.wad on a USB stick (root or doom/ folder), or run\n"
                   "  doom -iwad /usb0p1/path/to/file.wad\n");
            return 1;
        }

        av = malloc(sizeof(char *) * (size_t)(argc + 3));
        for (int i = 0; i < argc; i++)
            av[i] = argv[i];
        av[argc] = "-iwad";
        av[argc + 1] = wad;
        av[argc + 2] = NULL;
        ac = argc + 2;
    }

    printf("doom: game file %s\n", have_iwad ? "(from -iwad)" : wad);

    /* настройки и сохранения - на RAM-диск: внутренние диски MyOS без
       разрешения не пишет, а /bin только для чтения */
    chdir("/ram");

    doomgeneric_Create(ac, av);

    for (;;)
        doomgeneric_Tick();
}
