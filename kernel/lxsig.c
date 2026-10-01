/*
 * kernel/lxsig.c - сигналы для программ Linux (этап 11). Часть MyOS.
 *
 * Сигнал - "прерывание для программы": ядро останавливает её там, где
 * она была, и вызывает её функцию-обработчик (Ctrl+C - SIGINT, ошибка
 * памяти - SIGSEGV, закончился потомок - SIGCHLD, kill...). Когда
 * обработчик возвращается, программа продолжает с того же места.
 *
 * Как это делается (как в Linux на x86-64):
 *   1. сигнал отмечается "пришедшим" у процесса или потока;
 *   2. на выходе из ядра в программу (после системного вызова или
 *      прерывания) ядро видит пришедший и не заблокированный сигнал;
 *   3. на стек программы кладётся "кадр сигнала": все её регистры
 *      (ucontext), состояние SSE, siginfo и адрес возврата - функция
 *      restorer из libc (она делает системный вызов rt_sigreturn);
 *   4. программа "просыпается" уже в обработчике: rdi - номер
 *      сигнала, rsi - siginfo, rdx - ucontext;
 *   5. обработчик вернулся -> restorer -> rt_sigreturn: ядро берёт
 *      регистры из кадра и возвращает программу туда, где она была.
 *
 * Без обработчика - действие по умолчанию: одни сигналы молча
 * пропускаются (SIGCHLD, SIGWINCH...), остальные завершают процесс
 * ("killed by signal").
 *
 * Системный вызов, прерванный сигналом (read с клавиатуры, wait4...),
 * отвечает -ERESTARTSYS: если у обработчика SA_RESTART (или сигнал
 * пропущен), вызов повторяется (rip - 2 = снова на инструкцию syscall),
 * иначе программа получает EINTR.
 */
#include "linux.h"

void lx_enter_regs(LX_REGS *r, const void *fx) __attribute__((noreturn));
const void *lx_fx_default(void);

/* Действие по умолчанию: TRUE - пропустить, FALSE - завершить процесс */
static BOOLEAN sig_default_ignore(UINT32 sig)
{
    return sig == LX_SIGCHLD || sig == LX_SIGCONT || sig == LX_SIGURG ||
           sig == LX_SIGWINCH || sig == LX_SIGSTOP || sig == LX_SIGTSTP ||
           sig == LX_SIGTTIN || sig == LX_SIGTTOU || sig > 31;
}

static const char *sig_name(UINT32 sig)
{
    static const char *const n[] = {
        "0", "SIGHUP", "SIGINT", "SIGQUIT", "SIGILL", "SIGTRAP", "SIGABRT", "SIGBUS",
        "SIGFPE", "SIGKILL", "SIGUSR1", "SIGSEGV", "SIGUSR2", "SIGPIPE", "SIGALRM",
        "SIGTERM", "SIGSTKFLT", "SIGCHLD", "SIGCONT", "SIGSTOP", "SIGTSTP", "SIGTTIN",
        "SIGTTOU", "SIGURG", "SIGXCPU", "SIGXFSZ", "SIGVTALRM", "SIGPROF", "SIGWINCH",
        "SIGIO", "SIGPWR", "SIGSYS",
    };

    return (sig < 32) ? n[sig] : "SIGRT";
}

/* Сигнал убивает процесс (действие по умолчанию или SIGKILL) */
static void sig_fatal(KPROC *p, UINT32 sig)
{
    if (p->group_exit)
        return;

    ksnprintf(p->why, sizeof(p->why), "killed by signal %s", sig_name(sig));
    p->group_exit = TRUE;
    p->exit_code = 128 + (INT64)sig;
    p->killed = TRUE;

    if (p->lx != NULL) {
        p->lx->wstatus = (INT32)sig;
        p->lx->stopped_by_signal = TRUE;
    }

    for (UINTN i = 0; i < KT_MAX; i++) {
        KTHREAD *t = &g_kthreads[i];
        if (t->proc == p && t != g_kcur)
            sched_wake_thread(t);
    }
}

/* Пропустил бы процесс этот сигнал (игнорирует) - тогда и не ставим */
static BOOLEAN sig_ignored(KPROC *p, UINT32 sig)
{
    UINT64 h = p->lx->act[sig].handler;

    if (h == LX_SIG_IGN)
        return TRUE;

    return h == LX_SIG_DFL && sig_default_ignore(sig);
}

/* Сигнал процессу (любому его потоку, который не заблокировал) */
void lx_signal_send(KPROC *p, UINT32 sig)
{
    if (p == NULL || !p->used || p->exited || sig == 0 || sig > LX_NSIG)
        return;

    if (!p->is_linux || p->lx == NULL) {
        /* программа MyOS: любой "убивающий" сигнал - завершить */
        if (!sig_default_ignore(sig)) {
            ksnprintf(p->why, sizeof(p->why), "killed by signal %s", sig_name(sig));
            proc_kill(p);
        }
        return;
    }

    if (sig == LX_SIGKILL) {
        sig_fatal(p, sig);
        return;
    }

    if (sig_ignored(p, sig))
        return;

    p->lx->sig_pending |= LX_SIGBIT(sig);

    /* разбудить потоки: кто не заблокировал - доставит */
    for (UINTN i = 0; i < KT_MAX; i++) {
        KTHREAD *t = &g_kthreads[i];
        if (t->proc == p && !(t->sig_mask & LX_SIGBIT(sig)))
            sched_wake_thread(t);
    }
}

/* Сигнал конкретному потоку (tgkill, ошибка в нём) */
void lx_signal_thread(KTHREAD *t, UINT32 sig)
{
    KPROC *p = t->proc;

    if (p == NULL || p->lx == NULL || sig == 0 || sig > LX_NSIG)
        return;

    if (sig == LX_SIGKILL) {
        sig_fatal(p, sig);
        return;
    }

    if (sig_ignored(p, sig))
        return;

    t->sig_pending |= LX_SIGBIT(sig);
    sched_wake_thread(t);
}

INT64 lx_signal_send_tid(UINT32 tid, UINT32 sig)
{
    for (UINTN i = 0; i < KT_MAX; i++) {
        KTHREAD *t = &g_kthreads[i];
        if (t->proc != NULL && t->state != KT_DEAD && t->state != KT_UNUSED &&
            t->proc->is_linux && t->lx_tid == tid) {
            if (sig != 0)
                lx_signal_thread(t, sig);
            return 0;
        }
    }

    return -LX_ESRCH;
}

/* alarm: время вышло - SIGALRM */
static void check_alarm(KPROC *p)
{
    if (p->lx == NULL || p->lx->itimer_real_at == 0 || g_kticks < p->lx->itimer_real_at)
        return;

    p->lx->itimer_real_at = p->lx->itimer_real_iv ? g_kticks + p->lx->itimer_real_iv : 0;
    lx_signal_send(p, LX_SIGALRM);
}

/* Пришедшие и не заблокированные сигналы текущего потока */
static UINT64 sig_deliverable(KPROC *p, KTHREAD *t)
{
    if (p->lx == NULL)
        return 0;

    check_alarm(p);

    return (t->sig_pending | p->lx->sig_pending) &
           ~(t->sig_mask & ~(LX_SIGBIT(LX_SIGKILL) | LX_SIGBIT(LX_SIGSTOP)));
}

/* Есть ли что доставить (системный вызов, который ждёт, - прервать) */
BOOLEAN lx_signal_pending(void)
{
    KTHREAD *t = g_kcur;
    KPROC *p = t->proc;

    if (p == NULL || !p->is_linux)
        return FALSE;

    return p->killed || t->lx_die || sig_deliverable(p, t) != 0;
}

BOOLEAN lx_signal_ready(KPROC *p)
{
    return p->is_linux && sig_deliverable(p, g_kcur) != 0;
}

/* Ошибка в программе (cpu.c): синхронный сигнал этому потоку. Если
   его не обработать (нет обработчика или заблокирован) - конец. */
void lx_signal_fault(KPROC *p, UINT32 sig, UINT64 addr)
{
    KTHREAD *t = g_kcur;

    if (p->lx == NULL) {
        proc_kill(p);
        return;
    }

    UINT64 h = p->lx->act[sig].handler;

    if (h == LX_SIG_DFL || h == LX_SIG_IGN || (t->sig_mask & LX_SIGBIT(sig))) {
        sig_fatal(p, sig);
        return;
    }

    t->fault_addr = addr;
    t->sig_pending |= LX_SIGBIT(sig);
}

/* execve: свои обработчики пропадают (кода больше нет), "игнорировать"
   остаётся */
void lx_sig_exec_reset(KPROC *p)
{
    for (UINT32 s = 1; s <= LX_NSIG; s++) {
        if (p->lx->act[s].handler != LX_SIG_IGN)
            p->lx->act[s].handler = LX_SIG_DFL;
        p->lx->act[s].flags = 0;
        p->lx->act[s].mask = 0;
        p->lx->act[s].restorer = 0;
    }
}


/* ================================================================
 * Кадр сигнала
 * ================================================================ */

/* struct sigcontext (x86-64) - как в ядре Linux */
typedef struct {
    UINT64 r8, r9, r10, r11, r12, r13, r14, r15;
    UINT64 rdi, rsi, rbp, rbx, rdx, rax, rcx, rsp, rip, eflags;
    UINT16 cs, gs, fs, ss;
    UINT64 err, trapno, oldmask, cr2;
    UINT64 fpstate;
    UINT64 reserved1[8];
} LX_SIGCONTEXT;

typedef struct {
    UINT64 ss_sp;
    INT32  ss_flags;
    INT32  pad;
    UINT64 ss_size;
} LX_STACK_T;

typedef struct {
    UINT64        uc_flags;
    UINT64        uc_link;
    LX_STACK_T    uc_stack;
    LX_SIGCONTEXT uc_mcontext;
    UINT64        uc_sigmask;
} LX_UCONTEXT;

typedef struct {
    UINT64      pretcode;       /* адрес возврата обработчика (restorer) */
    LX_UCONTEXT uc;
    UINT8       info[128];      /* siginfo */
} LX_SIGFRAME;

/* Выбрать сигнал: сначала синхронные (ошибки), потом по номеру */
static UINT32 pick_signal(UINT64 set)
{
    static const UINT32 sync[] = { LX_SIGSEGV, LX_SIGBUS, LX_SIGILL, LX_SIGFPE, LX_SIGTRAP };

    for (UINTN i = 0; i < 5; i++)
        if (set & LX_SIGBIT(sync[i]))
            return sync[i];

    for (UINT32 s = 1; s <= LX_NSIG; s++)
        if (set & LX_SIGBIT(s))
            return s;

    return 0;
}

/*
 * Доставить сигнал и войти в программу (не возвращается). r - её
 * регистры в момент выхода из ядра; если это конец системного вызова
 * (from_syscall), sysret - его ответ, sysnr - номер (для повтора).
 */
void lx_signal_deliver(KPROC *p, LX_REGS *r, const void *fx, INT64 sysret, UINT64 sysnr,
                       BOOLEAN from_syscall)
{
    KTHREAD *t = g_kcur;
    UINT8 fxbuf[512] __attribute__((aligned(16)));

    memcpy(fxbuf, fx, 512);

    for (;;) {

        UINT64 set = sig_deliverable(p, t);
        UINT32 sig = pick_signal(set);

        if (sig == 0)
            break;

        /* снять отметку: сначала у потока, иначе у процесса */
        if (t->sig_pending & LX_SIGBIT(sig))
            t->sig_pending &= ~LX_SIGBIT(sig);
        else
            p->lx->sig_pending &= ~LX_SIGBIT(sig);

        LSIGACT *a = &p->lx->act[sig];
        BOOLEAN run_handler = (a->handler != LX_SIG_DFL && a->handler != LX_SIG_IGN);

        /* прерванный системный вызов: повторить или EINTR */
        if (from_syscall && (sysret == -LX_ERESTARTSYS)) {
            if (!run_handler || (a->flags & LX_SA_RESTART)) {
                r->rip -= 2;                    /* снова на syscall */
                r->rax = sysnr;
            } else {
                r->rax = (UINT64)(INT64)-LX_EINTR;
            }
            sysret = 0;
            from_syscall = FALSE;
        }

        if (!run_handler) {
            if (a->handler == LX_SIG_IGN || sig_default_ignore(sig))
                continue;
            /* по умолчанию - смерть процесса */
            sig_fatal(p, sig);
            proc_exit_current(-1);
        }

        if (!(a->flags & LX_SA_RESTORER) || a->restorer == 0) {
            sig_fatal(p, LX_SIGSEGV);
            proc_exit_current(-1);
        }

        /* где строить кадр: на своём стеке или на запасном (sigaltstack) */
        UINT64 sp = r->rsp;
        BOOLEAN on_alt = (t->alt_size != 0 && sp > t->alt_sp && sp <= t->alt_sp + t->alt_size);

        if ((a->flags & LX_SA_ONSTACK) && t->alt_size != 0 && !(t->alt_flags & LX_SS_DISABLE) && !on_alt)
            sp = t->alt_sp + t->alt_size;
        else
            sp -= 128;                          /* "красная зона" ABI под rsp */

        sp -= 512;
        sp &= ~63ull;
        UINT64 fx_at = sp;

        sp -= sizeof(LX_SIGFRAME);
        sp = ((sp + 8u) & ~15ull) - 8u;         /* как после call: rsp+8 кратен 16 */

        LX_SIGFRAME fr;
        memset(&fr, 0, sizeof(fr));

        fr.pretcode = a->restorer;
        fr.uc.uc_stack.ss_sp = t->alt_sp;
        fr.uc.uc_stack.ss_size = t->alt_size;
        fr.uc.uc_stack.ss_flags = (INT32)((t->alt_size == 0) ? LX_SS_DISABLE :
                                          (on_alt ? LX_SS_ONSTACK : 0));

        LX_SIGCONTEXT *m = &fr.uc.uc_mcontext;
        m->r8 = r->r8; m->r9 = r->r9; m->r10 = r->r10; m->r11 = r->r11;
        m->r12 = r->r12; m->r13 = r->r13; m->r14 = r->r14; m->r15 = r->r15;
        m->rdi = r->rdi; m->rsi = r->rsi; m->rbp = r->rbp; m->rbx = r->rbx;
        m->rdx = r->rdx; m->rax = r->rax; m->rcx = r->rcx; m->rsp = r->rsp;
        m->rip = r->rip; m->eflags = r->rflags;
        m->cs = 0x33;
        m->ss = 0x2B;
        m->trapno = (sig == LX_SIGSEGV) ? 14 : 0;
        m->cr2 = t->fault_addr;
        m->oldmask = t->sig_restore_mask ? t->sig_saved : t->sig_mask;
        m->fpstate = fx_at;
        fr.uc.uc_sigmask = m->oldmask;

        /* siginfo: номер, код, адрес/кто прислал */
        INT32 *si = (INT32 *)fr.info;
        si[0] = (INT32)sig;
        if (t->fault_addr != 0 && (sig == LX_SIGSEGV || sig == LX_SIGBUS)) {
            si[2] = 1;                          /* SEGV_MAPERR */
            memcpy(fr.info + 16, &t->fault_addr, 8);
        } else {
            si[2] = 0;                          /* SI_USER */
        }
        t->fault_addr = 0;

        if (!uptr_ok(p, sp, sizeof(fr), TRUE) || !uptr_ok(p, fx_at, 512, TRUE)) {
            /* стек испорчен - обработчик не вызвать */
            sig_fatal(p, LX_SIGSEGV);
            proc_exit_current(-1);
        }

        memcpy((void *)(UINTN)sp, &fr, sizeof(fr));
        memcpy((void *)(UINTN)fx_at, fxbuf, 512);

        /* маска на время обработчика */
        t->sig_restore_mask = FALSE;
        t->sig_mask |= a->mask;
        if (!(a->flags & LX_SA_NODEFER))
            t->sig_mask |= LX_SIGBIT(sig);
        t->sig_mask &= ~(LX_SIGBIT(LX_SIGKILL) | LX_SIGBIT(LX_SIGSTOP));

        UINT64 handler = a->handler;

        if (a->flags & LX_SA_RESETHAND)
            a->handler = LX_SIG_DFL;

        LX_REGS n = *r;
        n.rip = handler;
        n.rsp = sp;
        n.rdi = sig;
        n.rsi = sp + __builtin_offsetof(LX_SIGFRAME, info);
        n.rdx = sp + __builtin_offsetof(LX_SIGFRAME, uc);
        n.rax = 0;
        n.rflags &= ~((1ull << 8) | (1ull << 10));     /* TF, DF */

        lx_enter_regs(&n, fxbuf);
    }

    /* сигналов не осталось (все пропущены) */
    if (from_syscall)
        r->rax = (UINT64)((sysret == -LX_ERESTARTSYS) ? -LX_EINTR : sysret);

    lx_enter_regs(r, fxbuf);
}

/* rt_sigreturn: регистры, маска и SSE - из кадра на стеке */
static void __attribute__((noreturn)) sys_rt_sigreturn(KPROC *p, UINT64 *f)
{
    KTHREAD *t = g_kcur;
    UINT64 ucp = f[15];                         /* rsp программы = &uc */
    LX_UCONTEXT uc;
    UINT8 fxbuf[512] __attribute__((aligned(16)));

    if (!uptr_ok(p, ucp, sizeof(uc), FALSE)) {
        sig_fatal(p, LX_SIGSEGV);
        proc_exit_current(-1);
    }

    memcpy(&uc, (const void *)(UINTN)ucp, sizeof(uc));

    LX_SIGCONTEXT *m = &uc.uc_mcontext;
    LX_REGS r;

    r.r8 = m->r8; r.r9 = m->r9; r.r10 = m->r10; r.r11 = m->r11;
    r.r12 = m->r12; r.r13 = m->r13; r.r14 = m->r14; r.r15 = m->r15;
    r.rdi = m->rdi; r.rsi = m->rsi; r.rbp = m->rbp; r.rbx = m->rbx;
    r.rdx = m->rdx; r.rax = m->rax; r.rcx = m->rcx; r.rsp = m->rsp;
    r.rip = m->rip; r.rflags = m->eflags;
    r.cs = 0x2B;
    r.ss = 0x23;

    if (m->fpstate != 0 && uptr_ok(p, m->fpstate, 512, FALSE))
        memcpy(fxbuf, (const void *)(UINTN)m->fpstate, 512);
    else
        memcpy(fxbuf, lx_fx_default(), 512);

    /* MXCSR с запрещёнными битами - fxrstor упал бы в ядре */
    UINT32 mx;
    memcpy(&mx, fxbuf + 24, 4);
    mx &= 0xFFFFu;
    memcpy(fxbuf + 24, &mx, 4);

    t->sig_mask = uc.uc_sigmask & ~(LX_SIGBIT(LX_SIGKILL) | LX_SIGBIT(LX_SIGSTOP));

    /* за время обработчика могли прийти ещё */
    if (lx_signal_ready(p))
        lx_signal_deliver(p, &r, fxbuf, 0, 0, FALSE);

    lx_enter_regs(&r, fxbuf);
}


/* ================================================================
 * Системные вызовы сигналов
 * ================================================================ */

enum {
    NR_rt_sigaction = 13, NR_rt_sigprocmask = 14, NR_rt_sigreturn = 15, NR_pause = 34,
    NR_getitimer = 36, NR_alarm = 37, NR_setitimer = 38, NR_kill = 62,
    NR_rt_sigpending = 127, NR_rt_sigtimedwait = 128, NR_rt_sigqueueinfo = 129,
    NR_rt_sigsuspend = 130, NR_sigaltstack = 131, NR_tkill = 200, NR_tgkill = 234,
};

/* kill: pid > 0 - процесс; 0 - своя группа; -1 - все; < -1 - группа -pid */
static INT64 sys_kill(KPROC *p, INT64 pid, UINT32 sig)
{
    if (sig > LX_NSIG)
        return -LX_EINVAL;

    BOOLEAN found = FALSE;

    for (UINTN i = 0; i < PROC_MAX; i++) {

        KPROC *q = &g_procs[i];

        if (!q->used || q->exited)
            continue;

        BOOLEAN m;

        if (pid > 0)
            m = (INT64)q->pid == pid;
        else if (pid == -1)
            m = q->is_linux && q != p;
        else {
            UINT32 pg = (pid == 0) ? p->lx->pgid : (UINT32)(-pid);
            m = q->lx != NULL && q->lx->pgid == pg;
        }

        if (!m)
            continue;

        found = TRUE;

        if (sig != 0)
            lx_signal_send(q, sig);
    }

    return found ? 0 : -LX_ESRCH;
}

INT64 lx_sig_syscall(KPROC *p, UINT64 nr, UINT64 *a, UINT64 *f, BOOLEAN *handled)
{
    KTHREAD *t = g_kcur;

    *handled = TRUE;

    switch (nr) {

    case NR_rt_sigaction: {
        UINT32 sig = (UINT32)a[0];
        if (sig == 0 || sig > LX_NSIG || a[3] != 8)
            return -LX_EINVAL;
        if (a[2] != 0) {
            if (!uptr_ok(p, a[2], sizeof(LSIGACT), TRUE))
                return -LX_EFAULT;
            memcpy((void *)(UINTN)a[2], &p->lx->act[sig], sizeof(LSIGACT));
        }
        if (a[1] != 0) {
            if (sig == LX_SIGKILL || sig == LX_SIGSTOP)
                return -LX_EINVAL;
            if (!uptr_ok(p, a[1], sizeof(LSIGACT), FALSE))
                return -LX_EFAULT;
            memcpy(&p->lx->act[sig], (const void *)(UINTN)a[1], sizeof(LSIGACT));
            /* стал "игнорировать" - уже пришедший пропадает */
            if (sig_ignored(p, sig)) {
                p->lx->sig_pending &= ~LX_SIGBIT(sig);
                for (UINTN i = 0; i < KT_MAX; i++)
                    if (g_kthreads[i].proc == p)
                        g_kthreads[i].sig_pending &= ~LX_SIGBIT(sig);
            }
        }
        return 0;
    }

    case NR_rt_sigprocmask: {
        if (a[3] != 8)
            return -LX_EINVAL;
        UINT64 old = t->sig_mask;
        if (a[1] != 0) {
            if (!uptr_ok(p, a[1], 8, FALSE))
                return -LX_EFAULT;
            UINT64 set;
            memcpy(&set, (const void *)(UINTN)a[1], 8);
            if (a[0] == LX_SIG_BLOCK)
                t->sig_mask |= set;
            else if (a[0] == LX_SIG_UNBLOCK)
                t->sig_mask &= ~set;
            else if (a[0] == LX_SIG_SETMASK)
                t->sig_mask = set;
            else
                return -LX_EINVAL;
            t->sig_mask &= ~(LX_SIGBIT(LX_SIGKILL) | LX_SIGBIT(LX_SIGSTOP));
        }
        if (a[2] != 0) {
            if (!uptr_ok(p, a[2], 8, TRUE))
                return -LX_EFAULT;
            memcpy((void *)(UINTN)a[2], &old, 8);
        }
        return 0;
    }

    case NR_rt_sigreturn:
        sys_rt_sigreturn(p, f);

    case NR_rt_sigpending: {
        if (!uptr_ok(p, a[0], 8, TRUE))
            return -LX_EFAULT;
        UINT64 s = (t->sig_pending | p->lx->sig_pending) & t->sig_mask;
        memcpy((void *)(UINTN)a[0], &s, 8);
        return 0;
    }

    case NR_rt_sigsuspend: {
        if (!uptr_ok(p, a[0], 8, FALSE))
            return -LX_EFAULT;
        UINT64 m;
        memcpy(&m, (const void *)(UINTN)a[0], 8);
        t->sig_saved = t->sig_mask;
        t->sig_restore_mask = TRUE;
        t->sig_mask = m & ~(LX_SIGBIT(LX_SIGKILL) | LX_SIGBIT(LX_SIGSTOP));
        while (!lx_signal_pending())
            sched_sleep_ms(50);
        /* обработчик вернёт старую маску (uc_sigmask); если сигналов к
           доставке нет - вернём сами */
        if (!lx_signal_ready(p)) {
            t->sig_mask = t->sig_saved;
            t->sig_restore_mask = FALSE;
        }
        return -LX_EINTR;
    }

    case NR_pause:
        while (!lx_signal_pending())
            sched_sleep_ms(50);
        return -LX_EINTR;

    case NR_rt_sigtimedwait: {
        if (!uptr_ok(p, a[0], 8, FALSE))
            return -LX_EFAULT;
        UINT64 want;
        memcpy(&want, (const void *)(UINTN)a[0], 8);
        INT64 ms = -1;
        if (a[2] != 0) {
            if (!uptr_ok(p, a[2], sizeof(LX_TIMESPEC), FALSE))
                return -LX_EFAULT;
            LX_TIMESPEC ts;
            memcpy(&ts, (const void *)(UINTN)a[2], sizeof(ts));
            ms = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
        }
        UINT64 t0 = g_kticks;
        for (;;) {
            UINT64 have = (t->sig_pending | p->lx->sig_pending) & want;
            if (have) {
                UINT32 sig = pick_signal(have);
                if (t->sig_pending & LX_SIGBIT(sig))
                    t->sig_pending &= ~LX_SIGBIT(sig);
                else
                    p->lx->sig_pending &= ~LX_SIGBIT(sig);
                if (a[1] != 0 && uptr_ok(p, a[1], 128, TRUE)) {
                    memset((void *)(UINTN)a[1], 0, 128);
                    *(volatile INT32 *)(UINTN)a[1] = (INT32)sig;
                }
                return sig;
            }
            if (ms >= 0 && g_kticks - t0 >= (UINT64)ms)
                return -LX_EAGAIN;
            if (p->killed || (sig_deliverable(p, t) & ~want))
                return -LX_EINTR;
            sched_sleep_ms(10);
        }
    }

    case NR_sigaltstack: {
        if (a[1] != 0) {
            if (!uptr_ok(p, a[1], sizeof(LX_STACK_T), TRUE))
                return -LX_EFAULT;
            LX_STACK_T o;
            o.ss_sp = t->alt_sp;
            o.ss_size = t->alt_size;
            o.ss_flags = (INT32)((t->alt_size == 0 || (t->alt_flags & LX_SS_DISABLE)) ? LX_SS_DISABLE : 0);
            o.pad = 0;
            memcpy((void *)(UINTN)a[1], &o, sizeof(o));
        }
        if (a[0] != 0) {
            if (!uptr_ok(p, a[0], sizeof(LX_STACK_T), FALSE))
                return -LX_EFAULT;
            LX_STACK_T n;
            memcpy(&n, (const void *)(UINTN)a[0], sizeof(n));
            if (n.ss_flags & LX_SS_DISABLE) {
                t->alt_sp = 0;
                t->alt_size = 0;
                t->alt_flags = LX_SS_DISABLE;
            } else {
                if (n.ss_size < 2048)
                    return -12;             /* ENOMEM: слишком мал */
                t->alt_sp = n.ss_sp;
                t->alt_size = n.ss_size;
                t->alt_flags = 0;
            }
        }
        return 0;
    }

    case NR_kill:
        return sys_kill(p, (INT64)(INT32)a[0], (UINT32)a[1]);

    case NR_tkill:
        return lx_signal_send_tid((UINT32)a[0], (UINT32)a[1]);

    case NR_tgkill:
        return lx_signal_send_tid((UINT32)a[1], (UINT32)a[2]);

    case NR_rt_sigqueueinfo:
        return sys_kill(p, (INT64)(INT32)a[0], (UINT32)a[1]);

    case NR_alarm: {
        UINT64 left = 0;
        if (p->lx->itimer_real_at > g_kticks)
            left = (p->lx->itimer_real_at - g_kticks + 999u) / 1000u;
        p->lx->itimer_real_at = a[0] ? g_kticks + a[0] * 1000u : 0;
        p->lx->itimer_real_iv = 0;
        return (INT64)left;
    }

    case NR_setitimer:
    case NR_getitimer: {
        if (a[0] != 0)                          /* только ITIMER_REAL */
            return (nr == NR_getitimer) ? 0 : 0;
        UINT64 ou = (nr == NR_getitimer) ? a[1] : a[2];
        if (ou != 0 && uptr_ok(p, ou, 32, TRUE)) {
            UINT64 left = (p->lx->itimer_real_at > g_kticks) ? p->lx->itimer_real_at - g_kticks : 0;
            LX_TIMEVAL v[2] = {
                { (INT64)(p->lx->itimer_real_iv / 1000u), (INT64)(p->lx->itimer_real_iv % 1000u) * 1000 },
                { (INT64)(left / 1000u), (INT64)(left % 1000u) * 1000 },
            };
            memcpy((void *)(UINTN)ou, v, 32);
        }
        if (nr == NR_setitimer && a[1] != 0) {
            if (!uptr_ok(p, a[1], 32, FALSE))
                return -LX_EFAULT;
            LX_TIMEVAL v[2];
            memcpy(v, (const void *)(UINTN)a[1], 32);
            UINT64 iv = (UINT64)v[0].tv_sec * 1000u + (UINT64)v[0].tv_usec / 1000u;
            UINT64 val = (UINT64)v[1].tv_sec * 1000u + ((UINT64)v[1].tv_usec + 999u) / 1000u;
            p->lx->itimer_real_iv = iv;
            p->lx->itimer_real_at = val ? g_kticks + val : 0;
        }
        return 0;
    }
    }

    *handled = FALSE;
    return -LX_ENOSYS;
}

/*
 * Ctrl+C в терминале, где на переднем плане программа Linux: SIGINT
 * всей её группе процессов (шелл и то, что он запустил). Если
 * программа сама читает Ctrl+C (ISIG выключен - редактор строки,
 * vi), - ей просто клавиша 3.
 */
void lx_ctrl_c(KPROC *p)
{
    if (!lx_term_isig(p)) {
        if (p->io == PROC_IO_TTY && p->tty != NULL)
            return;                 /* окно само положит клавишу 3 в очередь */
        kbd_enqueue(0, 3);
        return;
    }

    UINT32 pg = (p->lx != NULL) ? p->lx->pgid : p->pid;
    BOOLEAN any = FALSE;

    for (UINTN i = 0; i < PROC_MAX; i++) {
        KPROC *q = &g_procs[i];
        if (q->used && !q->exited && q->is_linux && q->lx != NULL && q->lx->pgid == pg &&
            q->tty == p->tty && q->io == p->io) {
            lx_signal_send(q, LX_SIGINT);
            any = TRUE;
        }
    }

    if (!any)
        lx_signal_send(p, LX_SIGINT);
}
