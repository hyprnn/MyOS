/*
 * kernel/winproc.c - окна, которые создаёт программа (этап 7).
 * Часть MyOS; общие объявления - в myos.h, номера вызовов - sysnum.h.
 *
 * Программа в ring 3 просит окно системным вызовом win_create. Ядро:
 *   * заводит окно в композиторе (wm_open, без класса - "окно
 *     программы");
 *   * выделяет буфер пикселей и ОТОБРАЖАЕТ те же физические страницы
 *     и в память программы (по адресу MYOS_WIN_ADDR(id)), и себе
 *     (через P2V). Программа рисует прямо в этот общий буфер, ядро
 *     тот же буфер показывает на экране - копировать ничего не надо.
 * Программа рисует, зовёт win_update ("я перерисовал"), а события
 * (клик, клавиша, закрытие) забирает из очереди окна вызовом
 * win_event.
 *
 * Проверка: программа не может дать чужой номер окна - у окна
 * записан pid хозяина, ядро сверяет.
 */
#include "myos.h"

/* найти окно программы p по номеру id */
static WIN *win_of(KPROC *p, UINT64 id)
{
    for (UINTN i = 0; i < WM_MAX_WINDOWS; i++) {
        WIN *w = &g_windows[i];
        if (w->used && w->id == (UINT32)id && w->proc == p)
            return w;
    }

    return NULL;
}

/* Отобразить буфер окна (buf_phys, buf_pages) в память программы по
   адресу va с правами user+write */
extern BOOLEAN proc_map_shared(KPROC *p, UINT64 va, UINT64 phys, UINTN pages);

INT64 win_sys_create(KPROC *p, UINT64 uw, UINT64 uh, UINT64 utitle)
{
    if (!g_wm_running)
        return MYOS_ENOGUI;

    INT32 w = (INT32)uw, h = (INT32)uh;

    if (w < 40 || h < 30 || w > MYOS_WIN_MAX_W || h > MYOS_WIN_MAX_H)
        return MYOS_EINVAL;

    char title[WIN_TITLE_MAX];

    title[0] = '\0';

    if (utitle != 0) {
        for (UINTN i = 0; i < WIN_TITLE_MAX - 1; i++) {
            if (!uptr_ok(p, utitle + i, 1, FALSE))
                break;
            char c = *(volatile char *)(UINTN)(utitle + i);
            title[i] = c;
            title[i + 1] = '\0';
            if (c == '\0')
                break;
        }
    }

    if (title[0] == '\0')
        ksnprintf(title, sizeof(title), "%s", p->name);

    /* окно без класса; буфер выделим сами (общий с программой) */
    WIN *win = wm_open(NULL, w, h, title, NULL);

    if (win == NULL)
        return MYOS_ENOSPC;

    /* заменить kmalloc-буфер на страницы, которые отдадим и программе */
    kfree(win->buf);
    win->buf = NULL;

    UINTN pages = ((UINTN)w * (UINTN)h * 4u + 4095u) / 4096u;
    UINT64 phys = pmm_alloc_pages(pages, 0);

    if (phys == 0) {
        wm_close(win);
        return MYOS_ENOSPC;
    }

    win->buf = (UINT32 *)P2V(phys);
    win->buf_phys = phys;
    win->buf_pages = pages;
    win->proc = p;

    /* значок на панели задач - по имени программы ("notepad", "mines",
       "clock"...): Блокнот и Сапёр стали программами (этап 10, Д4), а
       выглядеть на панели должны как раньше */
    win->icon = gui_icon(p->name);

    for (UINTN i = 0; i < (UINTN)w * (UINTN)h; i++)
        win->buf[i] = 0x000000u;

    UINT64 va = MYOS_WIN_ADDR(win->id);

    if (!proc_map_shared(p, va, phys, pages)) {
        wm_close(win);
        return MYOS_ENOSPC;
    }

    klog("winproc: pid %u got window %u (%dx%d) at 0x%llx\n", p->pid, win->id, w, h, va);

    return (INT64)win->id;
}

INT64 win_sys_update(KPROC *p, UINT64 id, UINT64 urect)
{
    (void)urect;                     /* пока перерисовываем окно целиком */

    WIN *w = win_of(p, id);

    if (w == NULL)
        return MYOS_EINVAL;

    wm_invalidate(w);

    return 0;
}

INT64 win_sys_event(KPROC *p, UINT64 id, UINT64 uevent, UINT64 wait_ms)
{
    WIN *w = win_of(p, id);

    if (w == NULL)
        return MYOS_EINVAL;

    if (!uptr_ok(p, uevent, sizeof(struct myos_event), TRUE))
        return MYOS_EFAULT;

    UINT64 deadline = g_kticks + wait_ms;

    for (;;) {

        UINT64 fl = kx_irq_save();

        if (w->ev_head != w->ev_tail) {
            struct myos_event e = w->evq[w->ev_head];
            w->ev_head = (w->ev_head + 1u) % 32u;
            kx_irq_restore(fl);
            memcpy((void *)(UINTN)uevent, &e, sizeof(e));
            return 1;
        }

        kx_irq_restore(fl);

        if (p->killed)
            return 0;

        if (wait_ms == 0 || g_kticks >= deadline)
            return 0;

        /* подождать события или таймаута */
        UINT64 f2 = kx_irq_save();
        if (w->ev_head == w->ev_tail)
            sched_block(&w->evq, "окно: события", deadline - g_kticks);
        kx_irq_restore(f2);
    }
}

INT64 win_sys_close(KPROC *p, UINT64 id)
{
    WIN *w = win_of(p, id);

    if (w == NULL)
        return MYOS_EINVAL;

    /* отвязать от программы, чтобы wm_close не трогал её память
       повторно; страницы буфера освободит wm_close (buf_phys) */
    w->proc = NULL;
    wm_close(w);

    return 0;
}

INT64 win_sys_title(KPROC *p, UINT64 id, UINT64 utitle)
{
    WIN *w = win_of(p, id);

    if (w == NULL)
        return MYOS_EINVAL;

    char title[WIN_TITLE_MAX];

    title[0] = '\0';

    for (UINTN i = 0; i < WIN_TITLE_MAX - 1; i++) {
        if (!uptr_ok(p, utitle + i, 1, FALSE))
            break;
        char c = *(volatile char *)(UINTN)(utitle + i);
        title[i] = c;
        title[i + 1] = '\0';
        if (c == '\0')
            break;
    }

    wm_set_title(w, title);

    return 0;
}

/* Программа завершилась - закрыть её окна */
void win_proc_cleanup(KPROC *p)
{
    if (!g_wm_running)
        return;

    /* пометить окна программы "мёртвыми" - закроет их поток
       композитора (чтобы не трогать список окон из чужого потока) */
    for (UINTN i = 0; i < WM_MAX_WINDOWS; i++) {
        WIN *w = &g_windows[i];
        if (w->used && w->proc == p)
            w->dead = TRUE;
    }
}
