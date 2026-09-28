/*
 * gui/gui.c - главный цикл GUI (команда start).
 * Часть MyOS; общие объявления - в myos.h.
 */
#include "myos.h"


/*
 * "Что сейчас под курсором" в виде одного числа - для решения,
 * хватит ли при движении мыши быстрого пути (только курсор) или
 * картинка под ним тоже меняется и нужен полный кадр: пункт меню
 * Start подсвечивается под курсором, клетка Сапёра - тоже.
 * Число поменялось -> полный кадр; нет -> двигаем только курсор.
 */
INTN gui_hover_key(
    BOOLEAN in_minesweeper,
    BOOLEAN menu_open_on_desktop,
    GUI_ICON *icons,
    INTN win_x, INTN win_y, UINTN win_w, UINTN win_h,
    INTN cur_x, INTN cur_y
)
{
    INTN cx = cur_x + GUI_CURSOR_SIZE / 2;
    INTN cy = cur_y + GUI_CURSOR_SIZE / 2;

    if (in_minesweeper) {

        GUI_MS_LAYOUT L;
        gui_ms_compute_layout(win_x, win_y, win_w, win_h, &L);

        UINTN gw = GUI_MS_COLS * L.cell;
        UINTN gh = GUI_MS_ROWS * L.cell;

        if (gui_point_in_rect(cx, cy, L.grid_x, L.grid_y, gw, gh)) {

            INTN col = (INTN)((UINTN)(cx - L.grid_x) / L.cell);
            INTN row = (INTN)((UINTN)(cy - L.grid_y) / L.cell);

            return 1000 + row * GUI_MS_COLS + col;
        }

        return 0;
    }

    if (menu_open_on_desktop) {

        for (UINTN i = 0; i < GUI_ICON_COUNT; i++) {

            if (
                gui_point_in_rect(
                    cx, cy,
                    icons[i].x, icons[i].y, icons[i].w, icons[i].h
                )
            ) {
                return 1 + (INTN)i;
            }
        }
    }

    return 0;
}


void gui_start(EFI_SYSTEM_TABLE *st)
{
    SIMPLE_TEXT_OUTPUT_INTERFACE *out = st->ConOut;

    EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;

    if (
        !st->BootServices->LocateProtocol ||
        st->BootServices->LocateProtocol(
            &gop_guid, NULL, (VOID **)&gop
        ) != EFI_SUCCESS ||
        !gop || !gop->Mode || !gop->Mode->Info
    ) {

        print(
            out,
            "GUI unavailable: no Graphics Output Protocol.\n"
        );

        return;
    }

    UINT32 fb_w    = gop->Mode->Info->HorizontalResolution;
    UINT32 fb_h    = gop->Mode->Info->VerticalResolution;
    UINT32 stride  = gop->Mode->Info->PixelsPerScanLine;
    EFI_GRAPHICS_PIXEL_FORMAT fmt = gop->Mode->Info->PixelFormat;

    volatile UINT32 *fb =
        (volatile UINT32 *)gop->Mode->FrameBufferBase;

    /*
     * Мышь: EFI_SIMPLE_POINTER_PROTOCOL.
     *
     * ВАЖНО про "фундаментальное ограничение": эта ОС никогда
     * не вызывает ExitBootServices - она всё время работает как
     * обычное UEFI-приложение поверх прошивки. Значит, доступ
     * к устройствам возможен только через протоколы, которые
     * САМА прошивка успела опубликовать в DXE-фазе, - своего
     * USB-стека и HID-драйвера у ОС нет и без выхода из Boot
     * Services быть не может.
     *
     * Для beспроводной мыши (в т.ч. Onikuma) это работает так:
     * USB-приёмник (донгл) на шине выглядит как обычная
     * USB HID-мышь - сама беспроводная часть скрыта внутри
     * донгла. Поэтому прошивке, а значит и нам, безразлично,
     * беспроводная мышь или нет: если у прошивки есть
     * встроенный USB HID mouse driver (как в OVMF/QEMU - см.
     * UsbMouseDxe), он публикует EFI_SIMPLE_POINTER_PROTOCOL,
     * и LocateProtocol ниже её найдёт.
     *
     * Собственно ограничение: на РЕАЛЬНОМ железе (не в QEMU)
     * многие прошивки ноутбуков/материнских плат вообще не
     * публикуют этот протокол для USB-мышей - либо потому что
     * USB HID driver в DXE не подключён вендором прошивки,
     * либо потому что подключён только PS/2. В таком случае
     * LocateProtocol честно вернёт "не найдено", и это не баг
     * в коде ниже - это значит, что у конкретной прошивки нет
     * поддержки мыши на уровне DXE. Единственный настоящий
     * выход в такой ситуации - писать собственный драйвер
     * USB-контроллера (xHCI) и HID-класса поверх него, что
     * требует уже выхода из Boot Services (ExitBootServices)
     * и заметно больше кода. Поэтому ниже сделан аккуратный
     * fallback: если протокола нет, GUI остаётся полностью
     * рабочим на стрелках/Enter.
     */
    EFI_GUID pointer_guid = EFI_SIMPLE_POINTER_PROTOCOL_GUID;
    EFI_SIMPLE_POINTER_PROTOCOL *pointer = NULL;
    BOOLEAN have_mouse = FALSE;

    if (
        st->BootServices->LocateProtocol &&
        st->BootServices->LocateProtocol(
            &pointer_guid, NULL, (VOID **)&pointer
        ) == EFI_SUCCESS &&
        pointer != NULL &&
        pointer->GetState != NULL
    ) {

        if (pointer->Reset != NULL)
            pointer->Reset(pointer, FALSE);

        have_mouse = TRUE;

        print(
            out,
            "Mouse found (EFI_SIMPLE_POINTER_PROTOCOL).\n"
        );

    } else {

        print(
            out,
            "No mouse exposed by firmware "
            "(EFI_SIMPLE_POINTER_PROTOCOL not found) - "
            "this UEFI does not publish USB HID mouse input "
            "at boot-services level. Falling back to keyboard "
            "cursor (arrows + Enter).\n"
        );

        if (!g_kernel_mode) {

            print(
                out,
                "TIP: type 'ebs' first - then MyOS uses its OWN USB "
                "mouse driver and the mouse works in the GUI even "
                "on machines like this one.\n"
            );

        } else {

            print(
                out,
                "MyOS's USB driver found no mouse - see 'usb'.\n"
            );
        }
    }

    /*
     * Двойная буферизация: без неё каждый кадр
     * собирался прямо в видимой памяти GOP по частям
     * (фон, иконки, панель задач, курсор отдельными
     * вызовами) — глаз успевал заметить промежуточные
     * состояния, отсюда и мерцание при каждом движении
     * курсора. Теперь кадр целиком собирается в
     * невидимом буфере в обычной RAM, и в видеопамять
     * копируется уже готовым, одним проходом.
     */
    volatile UINT32 *back_buf = NULL;

    {
        GUI_ALLOCATE_POOL AllocatePool =
            (GUI_ALLOCATE_POOL)st->BootServices->AllocatePool;

        UINTN back_size =
            (UINTN)stride * (UINTN)fb_h * sizeof(UINT32);

        VOID *back_raw = NULL;

        if (
            AllocatePool != NULL &&
            AllocatePool(
                GUI_EFI_BOOT_SERVICES_DATA,
                back_size,
                &back_raw
            ) == EFI_SUCCESS
        ) {
            back_buf = (volatile UINT32 *)back_raw;
        }
    }

    /* Буфер, в который реально рисуем каждый кадр. */
    volatile UINT32 *draw_buf =
        (back_buf != NULL) ? back_buf : fb;

    /* Есть задний буфер - курсор живёт отдельно от картинки
       (см. комментарий у g_gui_draw_cursor) */
    g_gui_draw_cursor = (back_buf == NULL);

    BOOLEAN cursor_moved = FALSE;   /* сдвинулся только курсор */
    INTN drawn_cur_x = 0;           /* где курсор нарисован сейчас */
    INTN drawn_cur_y = 0;
    INTN last_hover = -1;

    if (back_buf == NULL) {

        print(
            out,
            "Note: no back buffer, drawing may flicker.\n"
        );
    }

    if (have_mouse) {

        print(
            out,
            "Controls: move the mouse or use arrows, click or "
            "Enter to click, Esc to go back/exit.\n"
        );

    } else {

        print(
            out,
            "Controls: arrows to move cursor, Enter to click, Esc to go back/exit.\n"
        );
    }

    print(
        out,
        "Click Start (top-left) for About, Fetch, Help, Notepad, Explorer, Terminal, Minesweeper, Exit.\n"
    );

    print(
        out,
        "Press any key to enter the GUI...\n"
    );

    /*
     * Реальная пауза перед переходом в графику,
     * иначе это сообщение мгновенно перекроется
     * первым же кадром и его не увидеть.
     */
    {
        EFI_INPUT_KEY key;

        while (
            st->ConIn->ReadKeyStroke(
                st->ConIn, &key
            ) != EFI_SUCCESS
        ) {
            st->BootServices->Stall(10000);
        }
    }

    /* Геометрия модального окна приложений */
    UINTN win_w = fb_w / 2;
    UINTN win_h = fb_h / 2;
    INTN  win_x = ((INTN)fb_w - (INTN)win_w) / 2;
    INTN  win_y = ((INTN)fb_h - (INTN)win_h) / 2;

    UINTN btn_size = 18;
    INTN  btn_x = win_x + (INTN)win_w - (INTN)btn_size - 4;
    INTN  btn_y = win_y + 3;

    /* Геометрия кнопки "Start" на панели задач (сверху слева) */
    INTN  sbtn_x = 3;
    INTN  sbtn_y = 3;
    UINTN sbtn_w = 56;
    UINTN sbtn_h = (UINTN)GUI_TASKBAR_H - 6;

    /* Пункты меню "Start" - все программы, которые раньше
       лежали значками на столе, теперь только здесь. */
    GUI_ICON icons[GUI_ICON_COUNT];

    icons[0].label = "ABOUT";
    icons[0].action = GUI_ACT_ABOUT;

    icons[1].label = "FETCH";
    icons[1].action = GUI_ACT_FETCH;

    icons[2].label = "HELP";
    icons[2].action = GUI_ACT_HELP;

    icons[3].label = "NOTEPAD";
    icons[3].action = GUI_ACT_NOTEPAD;

    icons[4].label = "TERMINAL";
    icons[4].action = GUI_ACT_TERMINAL;

    icons[5].label = "EXPLORER";
    icons[5].action = GUI_ACT_EXPLORER;

    icons[6].label = "MINESWEEPER";
    icons[6].action = GUI_ACT_MINESWEEPER;

    icons[7].label = "EXIT";
    icons[7].action = GUI_ACT_EXIT;

    /* Список выпадает вниз прямо из-под кнопки "Start" */
    for (UINTN i = 0; i < GUI_ICON_COUNT; i++) {

        icons[i].x = sbtn_x;
        icons[i].y = GUI_TASKBAR_H + (INTN)(i * GUI_ICON_H);
        icons[i].w = GUI_ICON_W;
        icons[i].h = GUI_ICON_H;
    }

    BOOLEAN menu_open = FALSE;

    /* Содержимое окон About / Fetch / Help */
    static const char *about_lines[] = {
        "MYOS 0.1",
        "MINIMAL UEFI OS BUILT",
        "FROM SCRATCH.",
        "NO LINUX NO WINDOWS.",
        "ARROWS MOVE CURSOR,",
        "ENTER CLICKS."
    };

    static const char *help_lines[] = {
        "CONSOLE COMMANDS:",
        "HELP ABOUT FETCH VER",
        "TIME DATE BANNER LS",
        "CAT SIZE REBOOT",
        "TYPE THEM IN THE",
        "SHELL, NOT HERE."
    };

    char fetch_res[32];
    char fetch_fmt[24];

    {
        UINTN n = gui_uint_to_str(fb_w, fetch_res);

        fetch_res[n++] = ' ';
        fetch_res[n++] = 'X';
        fetch_res[n++] = ' ';

        n += gui_uint_to_str(fb_h, fetch_res + n);

        fetch_res[n] = '\0';
    }

    {
        const char *fname;

        if (fmt == PixelRedGreenBlueReserved8BitPerColor)
            fname = "FORMAT: RGB";
        else
            fname = "FORMAT: BGR";

        UINTN i = 0;

        while (fname[i] != '\0' && i < sizeof(fetch_fmt) - 1) {
            fetch_fmt[i] = fname[i];
            i++;
        }

        fetch_fmt[i] = '\0';
    }

    const char *fetch_lines[3];
    fetch_lines[0] = "GOP FRAMEBUFFER:";
    fetch_lines[1] = fetch_res;
    fetch_lines[2] = fetch_fmt;

    static const char *notepad_lines[] = {
        "NOTEPAD",
        "",
        "SIMPLE TEXT VIEWER.",
        "TYPING IS NOT YET",
        "SUPPORTED IN GUI MODE,",
        "ONLY ARROWS + ENTER."
    };

    GUI_WINDOW_CONTENT windows[9];

    windows[GUI_ACT_ABOUT].title = "ABOUT";
    windows[GUI_ACT_ABOUT].lines = about_lines;
    windows[GUI_ACT_ABOUT].line_count =
        sizeof(about_lines) / sizeof(about_lines[0]);

    windows[GUI_ACT_FETCH].title = "FETCH";
    windows[GUI_ACT_FETCH].lines = fetch_lines;
    windows[GUI_ACT_FETCH].line_count = 3;

    windows[GUI_ACT_HELP].title = "HELP";
    windows[GUI_ACT_HELP].lines = help_lines;
    windows[GUI_ACT_HELP].line_count =
        sizeof(help_lines) / sizeof(help_lines[0]);

    windows[GUI_ACT_NOTEPAD].title = "NOTEPAD";
    windows[GUI_ACT_NOTEPAD].lines = notepad_lines;
    windows[GUI_ACT_NOTEPAD].line_count =
        sizeof(notepad_lines) / sizeof(notepad_lines[0]);

    /* "Проводник": показывает содержимое той же RAM-FS,
       что и команды ls/cat/write в терминале. Строки
       собираются в этот буфер заново каждый раз, когда
       окно открывают, - чтобы список не отставал от
       файлов, созданных/изменённых через терминал.
       explorer_row_fs[i] - индекс файла в g_fs для
       i-й строки списка (или -1 для строк-заглушек
       вроде "(EMPTY)"/"..."), чтобы клик по строке
       открывал именно этот файл. */
    char explorer_buf[GUI_EXPLORER_MAX_ROWS + 1][GUI_EXPLORER_LINE_LEN];
    const char *explorer_lines[GUI_EXPLORER_MAX_ROWS + 1];
    int explorer_row_fs[GUI_EXPLORER_MAX_ROWS + 1];
    UINTN explorer_line_count = 0;

    windows[GUI_ACT_EXPLORER].title = "FILE EXPLORER";
    windows[GUI_ACT_EXPLORER].lines = explorer_lines;
    windows[GUI_ACT_EXPLORER].line_count = 0;

    /* Окно просмотра файла, открываемое кликом по строке
       в Проводнике. Как и explorer_buf, пересобирается
       заново при каждом открытии файла. */
    char fileview_buf[GUI_FILEVIEW_MAX_ROWS][GUI_FILEVIEW_LINE_LEN];
    const char *fileview_lines[GUI_FILEVIEW_MAX_ROWS];
    char fileview_title[FS_NAME_MAX + 1];

    windows[GUI_ACT_FILEVIEW].title = fileview_title;
    windows[GUI_ACT_FILEVIEW].lines = fileview_lines;
    windows[GUI_ACT_FILEVIEW].line_count = 0;

    INTN cur_x = (INTN)fb_w / 2;
    INTN cur_y = (INTN)fb_h / 2;

    /* Накопитель дробного остатка движения мыши (чтобы не
       терять медленные/мелкие смещения при делении на
       разрешение устройства) и состояние левой кнопки на
       предыдущем опросе (для детектирования "нажал только
       что", а не "зажата уже давно"). */
    INTN mouse_rem_x = 0;
    INTN mouse_rem_y = 0;
    BOOLEAN mouse_left_prev = FALSE;
    BOOLEAN mouse_click_pending = FALSE;

    BOOLEAN in_window = FALSE;
    UINTN open_action = 0;
    BOOLEAN want_cmd = FALSE;

    /* Терминал - отдельное состояние окна: своё окно
       на столе, как приложение (kitty), а не режим,
       выбрасывающий из GUI обратно в консоль. */
    BOOLEAN in_terminal = FALSE;
    BOOLEAN term_started = FALSE;

    /* Сапёр - тоже отдельное состояние окна, как терминал:
       своё окно на столе, а не запись в windows[]. Поле
       заводится один раз при первом открытии и живёт дальше,
       пока не нажат смайлик (рестарт) или Esc (просто закрыть
       окно, партия не сбрасывается). */
    BOOLEAN in_minesweeper = FALSE;
    BOOLEAN ms_inited = FALSE;
    GUI_MS_STATE ms;

    char  term_lines[GUI_TERM_MAX_LINES][GUI_TERM_LINE_LEN + 1];
    UINTN term_line_count = 0;

    char  term_input[GUI_TERM_LINE_LEN + 1];
    UINTN term_input_len = 0;

    term_input[0] = '\0';

    BOOLEAN dirty = TRUE;
    UINTN clock_tick = 0;
    char clock_text[9];

    clock_text[0] = '-';
    clock_text[1] = '-';
    clock_text[2] = ':';
    clock_text[3] = '-';
    clock_text[4] = '-';
    clock_text[5] = ':';
    clock_text[6] = '-';
    clock_text[7] = '-';
    clock_text[8] = '\0';

    for (;;) {

        /*
         * Раз примерно в секунду обновляем часы
         * в панели задач (даже без нажатий клавиш).
         */
        if (clock_tick == 0) {

            EFI_TIME now;

            if (
                st->RuntimeServices->GetTime &&
                st->RuntimeServices->GetTime(
                    &now, NULL
                ) == EFI_SUCCESS
            ) {

                gui_uint2_to_str(now.Hour, clock_text);
                gui_uint2_to_str(now.Minute, clock_text + 3);
                gui_uint2_to_str(now.Second, clock_text + 6);

                if (!in_window)
                    dirty = TRUE;
            }

            /* Таймер Сапёра тикает раз в секунду тем же
               способом, что и часы на панели задач - только
               пока партия идёт (мины уже расставлены и игра
               ещё не закончена). */
            if (in_minesweeper && ms.generated && !ms.over) {
                ms.timer++;
                dirty = TRUE;
            }
        }

        clock_tick++;

        if (clock_tick >= 250)
            clock_tick = 0;

        /* Опрос мыши - каждый кадр, независимо от клавиатуры.
           GetState неблокирующий: если новых данных с донгла/
           устройства не было, он просто вернёт последнее же
           состояние, поэтому дельта окажется нулевой и ничего
           лишнего не произойдёт. */
        if (have_mouse) {

            EFI_SIMPLE_POINTER_STATE pst;

            if (pointer->GetState(pointer, &pst) == EFI_SUCCESS) {

                INT64 res_x =
                    (pointer->Mode != NULL &&
                     pointer->Mode->ResolutionX != 0)
                        ? (INT64)pointer->Mode->ResolutionX
                        : 1;

                INT64 res_y =
                    (pointer->Mode != NULL &&
                     pointer->Mode->ResolutionY != 0)
                        ? (INT64)pointer->Mode->ResolutionY
                        : 1;

                /* RelativeMovement* приходит в "счётчиках",
                   ResolutionX/Y - счётчиков на мм (по спеке
                   UEFI). Переводим в пиксели через условную
                   чувствительность в пикселях на мм, а остаток
                   от деления копим, чтобы медленные движения
                   мыши не "съедались" округлением. */
                INT64 dx_num =
                    (INT64)pst.RelativeMovementX *
                        GUI_MOUSE_PIXELS_PER_MM +
                    (INT64)mouse_rem_x;

                INT64 dy_num =
                    (INT64)pst.RelativeMovementY *
                        GUI_MOUSE_PIXELS_PER_MM +
                    (INT64)mouse_rem_y;

                INTN dx = (INTN)(dx_num / res_x);
                INTN dy = (INTN)(dy_num / res_y);

                mouse_rem_x = (INTN)(dx_num - (INT64)dx * res_x);
                mouse_rem_y = (INTN)(dy_num - (INT64)dy * res_y);

                if (dx != 0 || dy != 0) {

                    cur_x += dx;
                    cur_y += dy;

                    if (cur_x < 0)
                        cur_x = 0;

                    if (cur_y < 0)
                        cur_y = 0;

                    if (cur_x > (INTN)fb_w - GUI_CURSOR_SIZE)
                        cur_x = (INTN)fb_w - GUI_CURSOR_SIZE;

                    if (cur_y > (INTN)fb_h - GUI_CURSOR_SIZE)
                        cur_y = (INTN)fb_h - GUI_CURSOR_SIZE;

                    if (g_gui_draw_cursor) {

                        /* нет заднего буфера - только полный кадр */
                        dirty = TRUE;

                    } else {

                        INTN hk = gui_hover_key(
                            in_minesweeper,
                            !in_window && !in_terminal &&
                                !in_minesweeper && menu_open,
                            icons,
                            win_x, win_y, win_w, win_h,
                            cur_x, cur_y
                        );

                        if (hk != last_hover)
                            dirty = TRUE;
                        else
                            cursor_moved = TRUE;
                    }
                }

                /* Клик засчитываем по фронту нажатия (кнопка
                   была отпущена, теперь нажата), а не по факту
                   "кнопка сейчас зажата" - иначе удержание кнопки
                   спамило бы кликами каждый кадр. */
                if (pst.LeftButton && !mouse_left_prev)
                    mouse_click_pending = TRUE;

                mouse_left_prev = pst.LeftButton;
            }
        }

        EFI_INPUT_KEY key;

        BOOLEAN got_event = (
            st->ConIn->ReadKeyStroke(
                st->ConIn, &key
            ) == EFI_SUCCESS
        );

        /* Если в этом кадре не было клавиши, но есть неотданный
           клик мышью - превращаем его в тот же самый Enter,
           который весь код ниже уже умеет обрабатывать как клик.
           Так не пришлось дублировать все хит-тесты под мышь
           отдельно. */
        if (!got_event && mouse_click_pending) {

            key.ScanCode = 0x00;
            key.UnicodeChar = CHAR_CARRIAGE_RETURN;
            got_event = TRUE;
            mouse_click_pending = FALSE;
        }

        if (got_event) {

            /* Esc: закрыть окно, либо меню Start,
               либо выйти из GUI */
            if (key.ScanCode == 0x17) {

                if (in_terminal) {
                    in_terminal = FALSE;
                    dirty = TRUE;
                    continue;
                }

                if (in_minesweeper) {
                    in_minesweeper = FALSE;
                    dirty = TRUE;
                    continue;
                }

                if (in_window) {
                    in_window = FALSE;
                    dirty = TRUE;
                    continue;
                }

                if (menu_open) {
                    menu_open = FALSE;
                    dirty = TRUE;
                    continue;
                }

                break;
            }

            /* Пока открыт терминал, клавиши идут не на
               управление курсором, а прямо в строку
               ввода - как в обычном приложении-терминале,
               а не в режиме "стрелки+Enter" остального GUI. */
            if (in_terminal) {

                if (key.UnicodeChar == CHAR_BACKSPACE) {

                    if (term_input_len > 0) {
                        term_input_len--;
                        term_input[term_input_len] = '\0';
                        dirty = TRUE;
                    }

                    continue;
                }

                if (
                    key.ScanCode == 0x00 &&
                    key.UnicodeChar == CHAR_CARRIAGE_RETURN
                ) {

                    BOOLEAN want_close =
                        gui_term_exec(
                            st, term_input,
                            term_lines, &term_line_count
                        );

                    term_input_len = 0;
                    term_input[0] = '\0';
                    dirty = TRUE;

                    if (want_close) {
                        in_terminal = FALSE;
                        term_started = FALSE;
                    }

                    continue;
                }

                if (
                    key.UnicodeChar != 0 &&
                    term_input_len < GUI_TERM_LINE_LEN
                ) {

                    char c = (char)key.UnicodeChar;

                    if (c >= 'a' && c <= 'z')
                        c = (char)(c - 'a' + 'A');

                    if (gui_find_glyph(c) != NULL) {

                        term_input[term_input_len++] = c;
                        term_input[term_input_len] = '\0';
                        dirty = TRUE;
                    }
                }

                continue;
            }

            /* Пока открыт Сапёр, обрабатываем клавиши сами -
               те же стрелки+Enter, что и везде, плюс F для
               флажка. Свой блок, а не общий ниже, потому что
               тут нужны свои хит-тесты (клетки/смайлик), а не
               иконки меню Start. */
            if (in_minesweeper) {

                if (key.ScanCode == 0x01) {
                    cur_y -= GUI_CURSOR_STEP;
                    dirty = TRUE;

                } else if (key.ScanCode == 0x02) {
                    cur_y += GUI_CURSOR_STEP;
                    dirty = TRUE;

                } else if (key.ScanCode == 0x03) {
                    cur_x += GUI_CURSOR_STEP;
                    dirty = TRUE;

                } else if (key.ScanCode == 0x04) {
                    cur_x -= GUI_CURSOR_STEP;
                    dirty = TRUE;
                }

                if (cur_x < 0)
                    cur_x = 0;

                if (cur_y < 0)
                    cur_y = 0;

                if (cur_x > (INTN)fb_w - GUI_CURSOR_SIZE)
                    cur_x = (INTN)fb_w - GUI_CURSOR_SIZE;

                if (cur_y > (INTN)fb_h - GUI_CURSOR_SIZE)
                    cur_y = (INTN)fb_h - GUI_CURSOR_SIZE;

                INTN mcx = cur_x + GUI_CURSOR_SIZE / 2;
                INTN mcy = cur_y + GUI_CURSOR_SIZE / 2;

                GUI_MS_LAYOUT L;
                gui_ms_compute_layout(win_x, win_y, win_w, win_h, &L);

                UINTN grid_w_px = GUI_MS_COLS * L.cell;
                UINTN grid_h_px = GUI_MS_ROWS * L.cell;

                char kc = (char)key.UnicodeChar;

                if (kc >= 'a' && kc <= 'z')
                    kc = (char)(kc - 'a' + 'A');

                if (
                    kc == 'F' &&
                    gui_point_in_rect(
                        mcx, mcy, L.grid_x, L.grid_y,
                        grid_w_px, grid_h_px
                    )
                ) {
                    int col = (int)((UINTN)(mcx - L.grid_x) / L.cell);
                    int row = (int)((UINTN)(mcy - L.grid_y) / L.cell);

                    gui_ms_toggle_flag(&ms, row, col);
                    dirty = TRUE;
                }

                if (
                    key.ScanCode == 0x00 &&
                    key.UnicodeChar == CHAR_CARRIAGE_RETURN
                ) {

                    if (
                        gui_point_in_rect(
                            mcx, mcy, btn_x, btn_y, btn_size, btn_size
                        )
                    ) {

                        in_minesweeper = FALSE;
                        dirty = TRUE;

                    } else if (
                        gui_point_in_rect(
                            mcx, mcy, L.smile_x, L.smile_y,
                            L.smile_size, L.smile_size
                        )
                    ) {

                        gui_ms_reset(&ms);
                        dirty = TRUE;

                    } else if (
                        !ms.over &&
                        gui_point_in_rect(
                            mcx, mcy, L.grid_x, L.grid_y,
                            grid_w_px, grid_h_px
                        )
                    ) {

                        int col =
                            (int)((UINTN)(mcx - L.grid_x) / L.cell);
                        int row =
                            (int)((UINTN)(mcy - L.grid_y) / L.cell);

                        gui_ms_reveal(&ms, row, col);
                        dirty = TRUE;
                    }
                }

                continue;
            }

            /* Стрелки двигают курсор */
            if (key.ScanCode == 0x01) {
                cur_y -= GUI_CURSOR_STEP;
                dirty = TRUE;

            } else if (key.ScanCode == 0x02) {
                cur_y += GUI_CURSOR_STEP;
                dirty = TRUE;

            } else if (key.ScanCode == 0x03) {
                cur_x += GUI_CURSOR_STEP;
                dirty = TRUE;

            } else if (key.ScanCode == 0x04) {
                cur_x -= GUI_CURSOR_STEP;
                dirty = TRUE;
            }

            if (cur_x < 0)
                cur_x = 0;

            if (cur_y < 0)
                cur_y = 0;

            if (cur_x > (INTN)fb_w - GUI_CURSOR_SIZE)
                cur_x = (INTN)fb_w - GUI_CURSOR_SIZE;

            if (cur_y > (INTN)fb_h - GUI_CURSOR_SIZE)
                cur_y = (INTN)fb_h - GUI_CURSOR_SIZE;

            /* Enter работает как клик левой кнопкой мыши */
            if (
                key.ScanCode == 0x00 &&
                key.UnicodeChar == CHAR_CARRIAGE_RETURN
            ) {

                INTN cx = cur_x + GUI_CURSOR_SIZE / 2;
                INTN cy = cur_y + GUI_CURSOR_SIZE / 2;

                if (!in_window && menu_open) {

                    /* Клик по кнопке "Start" пока меню
                       открыто - просто закрыть его. */
                    if (
                        gui_point_in_rect(
                            cx, cy,
                            sbtn_x, sbtn_y, sbtn_w, sbtn_h
                        )
                    ) {

                        menu_open = FALSE;
                        dirty = TRUE;

                    } else {

                        for (UINTN i = 0; i < GUI_ICON_COUNT; i++) {

                            if (
                                gui_point_in_rect(
                                    cx, cy,
                                    icons[i].x, icons[i].y,
                                    icons[i].w, icons[i].h
                                )
                            ) {

                                if (icons[i].action == GUI_ACT_EXIT) {

                                    in_window = FALSE;
                                    want_cmd = FALSE;
                                    goto gui_exit_loop;
                                }

                                if (icons[i].action == GUI_ACT_TERMINAL) {

                                    in_window = FALSE;

                                    if (!term_started) {

                                        term_started = TRUE;

                                        gui_term_push(
                                            term_lines,
                                            &term_line_count,
                                            "MYOS TERMINAL. TYPE HELP."
                                        );
                                    }

                                    in_terminal = TRUE;
                                    menu_open = FALSE;
                                    dirty = TRUE;
                                    break;
                                }

                                if (icons[i].action == GUI_ACT_MINESWEEPER) {

                                    in_window = FALSE;

                                    if (!ms_inited) {

                                        gui_ms_seed(st);
                                        gui_ms_reset(&ms);
                                        ms_inited = TRUE;
                                    }

                                    in_minesweeper = TRUE;
                                    menu_open = FALSE;
                                    dirty = TRUE;
                                    break;
                                }

                                if (icons[i].action == GUI_ACT_EXPLORER) {

                                    /* Пересобрать список файлов
                                       из RAM-FS прямо перед
                                       открытием окна. */
                                    explorer_line_count = 0;

                                    for (int fi = 0;
                                         fi < FS_MAX_FILES &&
                                         explorer_line_count <
                                             GUI_EXPLORER_MAX_ROWS;
                                         fi++) {

                                        if (!g_fs[fi].used)
                                            continue;

                                        char *row =
                                            explorer_buf[
                                                explorer_line_count
                                            ];
                                        UINTN p = 0;

                                        for (UINTN c = 0;
                                             g_fs[fi].name[c] != 0 &&
                                             p < GUI_EXPLORER_LINE_LEN
                                                 - 16;
                                             c++) {

                                            CHAR16 wc =
                                                g_fs[fi].name[c];

                                            row[p++] =
                                                (wc < 128)
                                                    ? (char)wc
                                                    : ' ';
                                        }

                                        row[p++] = ' ';
                                        row[p++] = '-';
                                        row[p++] = ' ';

                                        p += gui_uint_to_str(
                                            g_fs[fi].size,
                                            row + p
                                        );

                                        row[p++] = 'B';
                                        row[p] = '\0';

                                        explorer_lines[
                                            explorer_line_count
                                        ] = row;

                                        explorer_row_fs[
                                            explorer_line_count
                                        ] = fi;

                                        explorer_line_count++;
                                    }

                                    if (explorer_line_count == 0) {

                                        explorer_lines[0] =
                                            "EMPTY - NO FILES";
                                        explorer_row_fs[0] = -1;
                                        explorer_line_count = 1;

                                    } else {

                                        int more = 0;

                                        for (int fi = 0;
                                             fi < FS_MAX_FILES;
                                             fi++) {

                                            if (g_fs[fi].used)
                                                more++;
                                        }

                                        if ((UINTN)more >
                                            explorer_line_count &&
                                            explorer_line_count <
                                                GUI_EXPLORER_MAX_ROWS
                                                    + 1) {

                                            char *row =
                                                explorer_buf[
                                                    explorer_line_count
                                                ];

                                            UINTN p = 0;
                                            row[p++] = '.';
                                            row[p++] = '.';
                                            row[p++] = '.';
                                            row[p] = '\0';

                                            explorer_lines[
                                                explorer_line_count
                                            ] = row;

                                            explorer_row_fs[
                                                explorer_line_count
                                            ] = -1;

                                            explorer_line_count++;
                                        }
                                    }

                                    windows[GUI_ACT_EXPLORER]
                                        .line_count =
                                        explorer_line_count;
                                }

                                open_action = icons[i].action;
                                in_window = TRUE;
                                break;
                            }
                        }

                        /* Клик по пункту или мимо меню -
                           в любом случае закрыть список. */
                        menu_open = FALSE;
                        dirty = TRUE;
                    }

                } else if (!in_window) {

                    /* Меню закрыто: единственное, на что
                       можно кликнуть на столе - кнопка Start. */
                    if (
                        gui_point_in_rect(
                            cx, cy,
                            sbtn_x, sbtn_y, sbtn_w, sbtn_h
                        )
                    ) {

                        menu_open = TRUE;
                        dirty = TRUE;
                    }

                } else if (
                    cx >= btn_x &&
                    cx <  btn_x + (INTN)btn_size &&
                    cy >= btn_y &&
                    cy <  btn_y + (INTN)btn_size
                ) {

                    /* "клик" по "X" — закрыть окно */
                    in_window = FALSE;
                    dirty = TRUE;

                } else if (open_action == GUI_ACT_EXPLORER) {

                    /* Клик по строке файла в Проводнике -
                       открыть его в окне просмотра. Та же
                       геометрия строк, что и в gui_draw_window:
                       первая строка на win_y+40+GUI_MENU_H,
                       дальше каждая следующая на +14. */
                    INTN rows_y0 =
                        win_y + 40 + (INTN)GUI_MENU_H;

                    if (
                        cx >= win_x + 9 &&
                        cx <  win_x + (INTN)win_w - 9 &&
                        cy >= rows_y0
                    ) {

                        UINTN row =
                            (UINTN)(cy - rows_y0) / 14;

                        if (row < explorer_line_count &&
                            explorer_row_fs[row] >= 0) {

                            windows[GUI_ACT_FILEVIEW]
                                .line_count =
                                gui_open_fileview(
                                    explorer_row_fs[row],
                                    fileview_buf,
                                    fileview_lines,
                                    fileview_title
                                );

                            open_action = GUI_ACT_FILEVIEW;
                            dirty = TRUE;
                        }
                    }
                }
            }
        }

        if (dirty) {

            if (in_terminal) {

                gui_draw_terminal(
                    draw_buf, stride, fb_w, fb_h, fmt,
                    win_x, win_y, win_w, win_h,
                    btn_x, btn_y, btn_size,
                    (const char (*)[GUI_TERM_LINE_LEN + 1])
                        term_lines,
                    term_line_count,
                    term_input
                );

            } else if (in_minesweeper) {

                gui_draw_minesweeper(
                    draw_buf, stride, fb_w, fb_h, fmt,
                    win_x, win_y, win_w, win_h,
                    btn_x, btn_y, btn_size,
                    &ms,
                    cur_x, cur_y
                );

            } else if (in_window) {

                gui_draw_window(
                    draw_buf, stride, fb_w, fb_h, fmt,
                    win_x, win_y, win_w, win_h,
                    btn_x, btn_y, btn_size,
                    &windows[open_action],
                    cur_x, cur_y
                );

            } else {

                gui_draw_desktop(
                    draw_buf, stride, fb_w, fb_h, fmt,
                    icons, GUI_ICON_COUNT,
                    menu_open,
                    sbtn_x, sbtn_y, sbtn_w, sbtn_h,
                    cur_x, cur_y,
                    clock_text
                );
            }

            /*
             * Готовый кадр из невидимого буфера —
             * одним проходом в реальную видеопамять.
             */
            if (back_buf != NULL) {

                UINTN total = (UINTN)stride * (UINTN)fb_h;

                for (UINTN i = 0; i < total; i++)
                    fb[i] = back_buf[i];

                /* курсор - только в видеопамяти, поверх */
                gui_draw_cursor_at(fb, stride, fb_w, fb_h, fmt, cur_x, cur_y);
                drawn_cur_x = cur_x;
                drawn_cur_y = cur_y;

                last_hover = gui_hover_key(
                    in_minesweeper,
                    !in_window && !in_terminal &&
                        !in_minesweeper && menu_open,
                    icons,
                    win_x, win_y, win_w, win_h,
                    cur_x, cur_y
                );
            }

            dirty = FALSE;
            cursor_moved = FALSE;

        } else if (cursor_moved) {

            /* Быстрый путь: вернуть картинку под старым местом
               курсора из заднего буфера и нарисовать курсор на
               новом. Два квадратика 12x12 вместо всего экрана. */
            gui_blit_rect(
                fb, back_buf, stride, fb_w, fb_h,
                drawn_cur_x, drawn_cur_y,
                GUI_CURSOR_SIZE, GUI_CURSOR_SIZE
            );

            gui_draw_cursor_at(fb, stride, fb_w, fb_h, fmt, cur_x, cur_y);

            drawn_cur_x = cur_x;
            drawn_cur_y = cur_y;
            cursor_moved = FALSE;
        }

        /*
         * Пауза между итерациями. Раньше - 4 мс всегда. Теперь
         * меньше, если мышь двигается прямо сейчас (быстрый путь
         * стоит копейки, а чем чаще опрос - тем плавнее курсор),
         * и по-прежнему 4 мс в покое, чтобы не молотить впустую.
         */
        st->BootServices->Stall(have_mouse ? 1000 : 4000);
    }

gui_exit_loop:

    g_gui_draw_cursor = TRUE;

    if (back_buf != NULL) {

        GUI_FREE_POOL FreePool =
            (GUI_FREE_POOL)st->BootServices->FreePool;

        if (FreePool != NULL)
            FreePool((VOID *)back_buf);
    }

    /* Возврат в текстовый режим */
    out->ClearScreen(out);
    set_color(out, g_color);

    if (want_cmd) {

        print(
            out,
            "Command line. Type 'start' to return to the GUI.\n"
        );

    } else {

        print(
            out,
            "Left GUI shell.\n"
        );
    }
}
