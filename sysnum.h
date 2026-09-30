/*
 * sysnum.h - номера системных вызовов MyOS и структуры, которыми
 * обмениваются ядро и программы (этап 6). Этот файл включают И ядро
 * (myos.h), И программы (user/include/myos.h) - поэтому здесь только
 * простые типы C, без UINT64 и прочих типов ядра.
 *
 * Как программа зовёт ядро (соглашение как в Linux x86-64):
 *   rax = номер вызова, аргументы: rdi, rsi, rdx, r10, r8, r9;
 *   инструкция syscall; ответ - в rax. Отрицательный ответ - ошибка
 *   (коды - как VFS_E* в ядре, см. MYOS_E* ниже).
 */
#ifndef MYOS_SYSNUM_H
#define MYOS_SYSNUM_H

#define SYS_EXIT      0   /* exit(code)                         - не возвращается */
#define SYS_WRITE     1   /* write(fd, buf, n)   1,2 - экран    -> записано байт */
#define SYS_READ      2   /* read(fd, buf, n)    0 - клавиатура -> прочитано байт */
#define SYS_OPEN      3   /* open(path, flags)                  -> fd (от 3) */
#define SYS_CLOSE     4   /* close(fd) */
#define SYS_SLEEP     5   /* sleep(ms) */
#define SYS_UPTIME    6   /* uptime()                           -> мс со старта */
#define SYS_SBRK      7   /* sbrk(прибавка)                     -> старый конец кучи */
#define SYS_GETPID    8   /* getpid() */
#define SYS_TIME      9   /* time(struct myos_time *, utc)      - местное время
                             (utc = 1 - всемирное, UTC: для сертификатов TLS) */
#define SYS_READDIR  10   /* readdir(path, номер, struct myos_dirent *) -> 1 / 0 (конец) */
#define SYS_MKDIR    11   /* mkdir(path) */
#define SYS_UNLINK   12   /* unlink(path)  - файл или пустая папка */
#define SYS_RENAME   13   /* rename(from, to) */
#define SYS_YIELD    14   /* уступить процессор */
#define SYS_STAT     15   /* stat(path, struct myos_dirent *) */
#define SYS_GETKEY   16   /* getkey() -> символ, 0 - нет нажатия (не ждёт) */
/* окна (этап 7) */
#define SYS_WIN_CREATE 17 /* win_create(w, h, title) -> номер окна; его пиксели -
                             по адресу MYOS_WIN_ADDR(номер), w x h, 0x00RRGGBB */
#define SYS_WIN_UPDATE 18 /* win_update(номер, struct myos_rect * или 0 - всё) -
                             "я перерисовал, покажи" */
#define SYS_WIN_EVENT  19 /* win_event(номер, struct myos_event *, ждать мс) -> 1/0 */
#define SYS_WIN_CLOSE  20 /* win_close(номер) */
#define SYS_WIN_TITLE  21 /* win_title(номер, заголовок UTF-8) */
/* сеть (этап 8). Сокет - обычный fd: write/read/close работают и с ним */
#define SYS_SOCKET   22   /* socket(тип MYOS_SOCK_*)              -> fd */
#define SYS_CONNECT  23   /* connect(fd, struct myos_sockaddr *)   - TCP: соединиться */
#define SYS_BIND     24   /* bind(fd, struct myos_sockaddr *)      - свой порт */
#define SYS_LISTEN   25   /* listen(fd, очередь)                   - ждать входящих */
#define SYS_ACCEPT   26   /* accept(fd, struct myos_sockaddr * или 0) -> новый fd */
#define SYS_SENDTO   27   /* sendto(fd, buf, n, struct myos_sockaddr * или 0) */
#define SYS_RECVFROM 28   /* recvfrom(fd, buf, n, struct myos_sockaddr * или 0) -> байт */
#define SYS_SOCKOPT  29   /* sockopt(fd, MYOS_SO_*, значение) */
#define SYS_RESOLVE  30   /* resolve(имя, unsigned int *ip)        - DNS */
#define SYS_NETINFO  31   /* netinfo(номер, struct myos_netif *)   -> 1 есть / 0 конец */
#define SYS_NETCTL   32   /* netctl(struct myos_netctl *)          - настроить адрес */
#define SYS_GETRANDOM 33  /* getrandom(buf, n)  - случайные байты для ключей (TLS) */
/* для полной libc (picolibc, этап 9) */
#define SYS_SEEK     34   /* seek(fd, сдвиг, откуда 0/1/2)         -> новое место */
#define SYS_FSTAT    35   /* fstat(fd, struct myos_dirent *)       - is_dir: MYOS_FT_* */
#define SYS_GETCWD   36   /* getcwd(buf, размер)                   -> длина пути */
#define SYS_CHDIR    37   /* chdir(path)                           - текущая папка программы */
#define SYS_POLL     38   /* poll(struct myos_pollfd *, n, мс; -1 - сколько угодно) -> готовых */
/* этап 10: шелл - программа (/bin/sh) */
#define SYS_SPAWN    39   /* spawn(struct myos_spawn *)            -> pid */
#define SYS_WAIT     40   /* wait(pid, struct myos_waitinfo *, флаги MYOS_WAIT_*)
                             -> 1 завершилась (и убрана), 0 ещё работает */
#define SYS_READKEY  41   /* readkey(мс; -1 - ждать сколько угодно) -> клавиша:
                             символ Юникода или MYOS_KEY_SPECIAL | скан-код;
                             MYOS_KEY_REDRAW - "перерисуй строку ввода";
                             -1 - нет клавиши */
#define SYS_KCMD     42   /* kcmd(строка команды, путь для вывода или 0, флаги)
                             - встроенная команда ядра (net, wifi, battery...) */
#define SYS_COUNT    43

/* флаги open - те же, что VFS_O_* в ядре */
#define MYOS_O_READ    0x01
#define MYOS_O_WRITE   0x02
#define MYOS_O_CREATE  0x04
#define MYOS_O_TRUNC   0x08
#define MYOS_O_APPEND  0x10

/* короткие имена - для ядра и программ на мини-libc; у программ на
   полной libc (MYOS_POSIX) O_TRUNC/O_APPEND - свои, из <fcntl.h> */
#ifndef MYOS_POSIX
#define O_READ    MYOS_O_READ
#define O_WRITE   MYOS_O_WRITE
#define O_CREATE  MYOS_O_CREATE
#define O_TRUNC   MYOS_O_TRUNC
#define O_APPEND  MYOS_O_APPEND
#endif

/* ошибки (отрицательные ответы) - те же, что VFS_E* в ядре */
#define MYOS_ENOENT     -2
#define MYOS_EEXIST     -3
#define MYOS_ENOTDIR    -4
#define MYOS_EISDIR     -5
#define MYOS_ENOTEMPTY  -6
#define MYOS_ENOSPC     -7
#define MYOS_EROFS      -8
#define MYOS_EIO        -9
#define MYOS_EINVAL    -10
#define MYOS_EBADF     -11
#define MYOS_EMFILE    -12
#define MYOS_ENOSYS    -13
#define MYOS_EGONE     -14
#define MYOS_EXDEV     -15
#define MYOS_EFAULT    -16   /* плохой указатель от программы */
#define MYOS_ENOGUI    -17   /* графика не запущена (нужно 'start') */
/* сеть (этап 8) */
#define MYOS_ETIMEDOUT     -18   /* нет ответа */
#define MYOS_ECONNREFUSED  -19   /* на том порту никто не слушает */
#define MYOS_ECONNRESET    -20   /* собеседник оборвал соединение */
#define MYOS_ENETUNREACH   -21   /* нет сети: нет адреса или шлюза */
#define MYOS_EADDRINUSE    -22   /* порт уже занят */
#define MYOS_ENOTCONN      -23   /* сокет не соединён */
#define MYOS_EHOSTNOTFOUND -24   /* DNS: нет такого имени */
#define MYOS_EAGAIN        -25   /* за отведённое время ничего не пришло */
#define MYOS_EINTR         -26   /* прервано (Ctrl+C) */
#define MYOS_EINPROGRESS   -27   /* неблокирующий connect: соединение устанавливается */

struct myos_time {
    unsigned short year;
    unsigned char  month, day, hour, minute, second;
    unsigned char  pad;
};

/* что за fd (поле is_dir у fstat; stat и readdir дают только 0/1) */
#define MYOS_FT_FILE     0
#define MYOS_FT_DIR      1
#define MYOS_FT_CONSOLE  2   /* 0, 1, 2 - клавиатура и экран */
#define MYOS_FT_SOCKET   3

struct myos_dirent {
    char               name[128];
    unsigned long long size;
    unsigned int       is_dir;
    unsigned int       pad;
};

/* --- сеть (этап 8) --- */
#define MYOS_SOCK_STREAM  1   /* TCP */
#define MYOS_SOCK_DGRAM   2   /* UDP */
#define MYOS_SOCK_PING    3   /* ICMP Echo (ping): программа даёт 8 байт заголовка
                                 ICMP + данные; номер id и сумму ставит ядро */
#define MYOS_SO_TIMEOUT   1   /* сколько мс ждать connect/accept/recv (0 - сколько угодно) */
#define MYOS_SO_NONBLOCK  2   /* 1 - не ждать вовсе: нет данных/места - MYOS_EAGAIN,
                                 connect - MYOS_EINPROGRESS (итог - MYOS_SO_ERROR) */
#define MYOS_SO_ERROR     3   /* (значение не нужно) -> 0 соединён, MYOS_EINPROGRESS
                                 ещё соединяется, иначе - почему не вышло */
#define MYOS_SO_LOCALADDR 4   /* (значение не нужно) -> свой адрес: ip << 16 | порт */

/* poll: как struct pollfd в POSIX (и те же биты) */
struct myos_pollfd {
    int   fd;
    short events;
    short revents;
};
#define MYOS_POLLIN    0x01   /* есть что прочитать (или конец потока) */
#define MYOS_POLLOUT   0x04   /* можно писать */
#define MYOS_POLLERR   0x08   /* ошибка соединения */
#define MYOS_POLLHUP   0x10   /* соединение закрыто */
#define MYOS_POLLNVAL  0x20   /* нет такого fd */

/* адрес: IPv4 в порядке процессора (10.0.2.15 = 0x0A00020F) и порт */
struct myos_sockaddr {
    unsigned int   ip;
    unsigned short port;
    unsigned char  ttl;     /* recvfrom ping-сокета: TTL ответа */
    unsigned char  pad;
};

/* интерфейс (для ifconfig) */
#define MYOS_NETCFG_NONE    0
#define MYOS_NETCFG_DHCP    1
#define MYOS_NETCFG_STATIC  2
struct myos_netif {
    char               name[8];        /* lo, eth0, usb0 */
    char               driver[16];
    char               model[48];
    char               irq[24];        /* как карта сообщает о кадрах */
    char               state[48];      /* что делает DHCP */
    unsigned char      mac[6];
    unsigned char      link;           /* кабель / модем на связи */
    unsigned char      up;             /* адрес есть */
    unsigned int       cfg;            /* MYOS_NETCFG_* */
    unsigned int       ip, mask, gw, dns, dns2;
    unsigned int       speed_mbps;
    unsigned int       lease_left_s;   /* сколько ещё действует адрес от DHCP */
    unsigned long long rx_packets, rx_bytes, rx_dropped;
    unsigned long long tx_packets, tx_bytes, tx_errors;
};

#define MYOS_NETCTL_DHCP    1   /* адрес - от DHCP (заново) */
#define MYOS_NETCTL_STATIC  2   /* адрес вручную: ip, mask, gw, dns */
#define MYOS_NETCTL_DNS     3   /* только сервер DNS */
#define MYOS_NETCTL_DOWN    4   /* забыть адрес */
struct myos_netctl {
    char         name[8];
    unsigned int cmd;
    unsigned int ip, mask, gw, dns;
};

/* --- шелл-программа (этап 10) --- */
#define MYOS_SPAWN_FG      1   /* на переднем плане: Ctrl+C остановит её */
#define MYOS_SPAWN_APPEND  2   /* вывод в файл - дописывать (>>), а не заново (>) */
struct myos_spawn {
    const char *path;          /* файл программы: "/bin/ls" */
    const char *args;          /* аргументы одной строкой (или 0) */
    const char *out_path;      /* вывод (fd 1 и 2) - в этот файл (или 0 - как у нас) */
    unsigned int flags;        /* MYOS_SPAWN_* */
    unsigned int pad;
};

#define MYOS_WAIT_NOHANG   1   /* не ждать: ещё работает - сразу 0 */
struct myos_waitinfo {
    int  pid;
    int  code;                 /* код выхода; -1 - её остановило ядро */
    char name[32];
    char why[128];             /* почему остановлена (ошибка, Ctrl+C) */
};

#define MYOS_KEY_SPECIAL   0x10000   /* | скан-код: стрелки, Home, End, Del... */
#define MYOS_KEY_REDRAW    0x20000   /* экран перерисован (PageUp) - строку заново */
#define MYOS_SCAN_UP       0x01
#define MYOS_SCAN_DOWN     0x02
#define MYOS_SCAN_RIGHT    0x03
#define MYOS_SCAN_LEFT     0x04
#define MYOS_SCAN_HOME     0x05
#define MYOS_SCAN_END      0x06
#define MYOS_SCAN_DELETE   0x08
#define MYOS_SCAN_ESC      0x17

#define MYOS_KCMD_APPEND   1   /* вывод в файл - дописывать */

/* --- окна (этап 7) --- */
struct myos_rect {
    int x, y, w, h;
};

/* событие окна */
#define EV_KEY    1   /* key - символ Unicode (0 - особая клавиша, см. scan) */
#define EV_DOWN   2   /* кнопка мыши нажата: x, y - в окне, buttons: 1 левая, 2 правая */
#define EV_UP     3
#define EV_MOVE   4   /* мышь сдвинулась (над окном или с зажатой кнопкой) */
#define EV_WHEEL  5   /* колесо: wheel > 0 - от себя */
#define EV_CLOSE  6   /* нажали крестик окна */
#define EV_FOCUS  7   /* key = 1 - окно стало активным, 0 - перестало */

/* scan для особых клавиш (как в UEFI) */
#define KEY_UP     0x01
#define KEY_DOWN   0x02
#define KEY_RIGHT  0x03
#define KEY_LEFT   0x04
#define KEY_HOME   0x05
#define KEY_END    0x06
#define KEY_DEL    0x08
#define KEY_PGUP   0x09
#define KEY_PGDN   0x0A
#define KEY_ESC    0x17

struct myos_event {
    unsigned int type;
    unsigned int key;
    unsigned int scan;
    int          x, y;
    unsigned int buttons;
    int          wheel;
    unsigned int mods;      /* EV_KEY: какие клавиши-модификаторы зажаты
                               (MYOS_MOD_*, этап 9); иначе 0 */
};
#define MYOS_MOD_CTRL   0x01
#define MYOS_MOD_SHIFT  0x02
#define MYOS_MOD_ALT    0x04

#define MYOS_WIN_BASE     0x0000600000000000ull
#define MYOS_WIN_SPAN     0x0000000001000000ull      /* 16 МиБ на окно */
#define MYOS_WIN_ADDR(id) (MYOS_WIN_BASE + ((unsigned long long)(id) - 1ull) * MYOS_WIN_SPAN)
#define MYOS_WIN_MAX_W    1024
#define MYOS_WIN_MAX_H    768

/* Адреса программы (нижняя половина адресного пространства) */
#define MYOS_USER_BASE       0x0000000000400000ull   /* сюда линкуются программы */
#define MYOS_USER_STACK_TOP  0x00007FFFFFFF0000ull   /* вершина стека */
#define MYOS_USER_STACK_SIZE (1024ull * 1024ull)    /* 1 МиБ: разборщикам HTML/CSS
                                                      нужна глубокая рекурсия */
#define MYOS_USER_LIMIT      0x00007FFF00000000ull   /* выше - только стек */

#endif
