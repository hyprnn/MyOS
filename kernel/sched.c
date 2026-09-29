/*
 * kernel/sched.c - потоки ядра и планировщик (этап 4: многозадачность).
 * Часть MyOS; общие объявления - в myos.h.
 *
 * ЧТО ТАКОЕ ПОТОК
 * ---------------
 * Поток - это "отдельная линия выполнения": свой стек и свои
 * значения регистров. Процессор (ядро процессора) в каждый момент
 * выполняет ровно один поток; ощущение "всё работает одновременно"
 * создаётся тем, что планировщик сотни раз в секунду останавливает
 * один поток и продолжает другой. Остановленный поток ничего не
 * замечает: когда до него снова дойдёт очередь, все его регистры и
 * стек будут точно такими же, как в момент остановки.
 *
 * КАК ПЕРЕКЛЮЧАЕТСЯ ПОТОК (kx_switch, ниже на ассемблере)
 * -------------------------------------------------------
 * По соглашению о вызовах x86-64 (System V) функция обязана
 * сохранить только rbx, rbp, r12-r15 (и rsp) - остальные регистры
 * "портятся" при любом вызове, компилятор это знает. Поэтому
 * переключение - обычный вызов функции kx_switch(&old->rsp, new_rsp):
 *   1. кладём в стек СТАРОГО потока rbx, rbp, r12-r15 и флаги;
 *   2. запоминаем rsp старого потока в его структуре;
 *   3. загружаем rsp НОВОГО потока - теперь мы на его стеке;
 *   4. снимаем со стека его флаги и регистры и делаем ret - то есть
 *      возвращаемся туда, откуда НОВЫЙ поток когда-то сам вызвал
 *      kx_switch.
 * Для нового, ещё ни разу не работавшего потока стек заранее
 * подготовлен так, будто он уже вызывал kx_switch: ret уводит его в
 * kt_trampoline, а тот вызывает функцию потока.
 *
 * Регистры SSE (xmm) сохранять не нужно: при вызове функции они
 * по соглашению тоже "портятся", а при прерывании их уже сохранил
 * kx_isr_common (fxsave в стек прерванного потока, cpu.c).
 *
 * ВЫТЕСНЕНИЕ (PREEMPTION)
 * -----------------------
 * Таймер LAPIC тикает 1000 раз в секунду (time.c). На каждом тике
 * sched_tick() будит уснувших, у кого подошло время, и отсчитывает
 * квант текущего потока (KT_QUANTUM_MS = 10 мс). Кончился квант и
 * есть кто-то ещё готовый - ставим g_need_resched. В самом конце
 * обработчика прерывания (sched_isr_exit) планировщик переключает
 * поток прямо оттуда: рамка прерывания и все регистры остаются в
 * стеке прерванного потока, и когда до него снова дойдёт очередь,
 * он "вернётся из прерывания" как ни в чём не бывало.
 * Так поток, который крутит вечный цикл и сам процессор не отдаёт,
 * всё равно не может захватить машину.
 *
 * ОЧЕРЕДЬ
 * -------
 * Круг (round-robin) по принципу "кто раньше встал в очередь":
 * у каждого готового потока есть номер ready_seq, и выбирается
 * самый маленький. Поток, у которого кончился квант, встаёт в
 * конец очереди - так потоки, крутящие процессор без остановки,
 * получают его поровну (это проверяет команда threadtest).
 * Исключение - только что проснувшиеся (boost): они идут первыми.
 * Это потоки, которые почти всё время ждут (шелл ждёт клавишу,
 * GUI - следующего кадра, usb - события), им нужно совсем немного
 * процессора, но СРАЗУ - иначе курсор дёргался бы.
 * Если готовых нет - поток idle, который останавливает процессор
 * инструкцией hlt до ближайшего прерывания (так процессор и
 * "отдыхает" - загрузка ~0%).
 *
 * ОДНО ЯДРО
 * ---------
 * Пока работает одно ядро процессора (запуск остальных - SMP -
 * отдельный шаг). На одном ядре "запретить прерывания" (cli) =
 * "никто другой сейчас не выполнится": ни обработчик, ни другой
 * поток (переключает только таймер, а он - прерывание). На этом
 * держится kx_lock. Спин-замок KSPINLOCK уже написан так, чтобы
 * работать и на нескольких ядрах.
 */
#include "myos.h"


/* ================================================================
 * Таблица потоков
 * ================================================================ */

/*
 * Слот 0 - "главный" поток: тот, что выполнял kmain с самого
 * старта. Он существует с первой инструкции ядра - поэтому
 * g_kcur указывает на него сразу, ещё до sched_start (kx_lock
 * пользуется g_kcur с самого начала). После запуска это шелл.
 */
KTHREAD g_kthreads[KT_MAX] = {
    [0] = { .tid = 1, .slot = 0, .state = KT_RUNNING, .name = "shell" },
};

KTHREAD *g_kcur = &g_kthreads[0];      /* кто работает сейчас */
KTHREAD *g_kidle = NULL;               /* поток простоя */

volatile BOOLEAN g_sched_on = FALSE;   /* планировщик запущен */
volatile BOOLEAN g_need_resched = FALSE;
volatile UINT32  g_kx_isr_depth = 0;   /* >0 - мы внутри обработчика
                                          прерывания: там спать нельзя */

UINT64 g_sched_switches = 0;           /* всего переключений */
UINT64 g_sched_preempts = 0;           /* ...из них - принудительных
                                          (кончился квант) */

static UINT32 g_next_tid = 2;
static UINT64 g_ready_seq = 0;         /* счётчик мест в очереди */
static UINT32 g_quantum_left = KT_QUANTUM_MS;
static UINT64 g_slice_start = 0;       /* rdtsc начала работы g_kcur */
static UINT64 g_acct_last = 0;         /* rdtsc прошлого пересчёта % */


/* ================================================================
 * Переключение контекста (ассемблер)
 *
 * void kx_switch(UINT64 *save_rsp, UINT64 new_rsp)
 *   rdi = куда сохранить rsp текущего потока
 *   rsi = rsp потока, который надо продолжить
 * ================================================================ */
void kx_switch(UINT64 *save_rsp, UINT64 new_rsp);

__asm__(
    ".text\n"
    ".globl kx_switch\n"
    ".hidden kx_switch\n"
    "kx_switch:\n"
    "  pushq %rbp\n"
    "  pushq %rbx\n"
    "  pushq %r12\n"
    "  pushq %r13\n"
    "  pushq %r14\n"
    "  pushq %r15\n"
    "  pushfq\n"                 /* флаги (в т.ч. IF - разрешены ли
                                    прерывания) - у каждого потока свои */
    "  movq %rsp, (%rdi)\n"      /* старый поток: запомнить rsp */
    "  movq %rsi, %rsp\n"        /* новый поток: его стек */
    "  popfq\n"
    "  popq %r15\n"
    "  popq %r14\n"
    "  popq %r13\n"
    "  popq %r12\n"
    "  popq %rbx\n"
    "  popq %rbp\n"
    "  ret\n"                    /* продолжить новый поток */
);


/* ================================================================
 * Запрет прерываний
 * ================================================================ */

UINT64 kx_irq_save(void)
{
    UINT64 fl;

    __asm__ __volatile__("pushfq; popq %0; cli" : "=r"(fl) : : "memory");

    return fl;
}

void kx_irq_restore(UINT64 fl)
{
    if (fl & (1u << 9))
        __asm__ __volatile__("sti" ::: "memory");
}

/*
 * kx_lock / kx_unlock - "никто не мешает": запрещает прерывания,
 * вложенно (kx_lock внутри kx_lock можно). Раньше счётчик
 * вложенности был один на всю систему; теперь он у каждого потока
 * свой (g_kcur->lock_depth). Почему это важно: USB-поток может
 * уснуть ВНУТРИ kx_lock (ждёт 100 мс, пока устройство "сядет"
 * после подключения). Пока он спит, работают другие потоки - и у
 * них свой счётчик, ноль. Иначе шелл после чужого kx_lock остался
 * бы с запрещёнными прерываниями навсегда.
 */
void kx_lock(void)
{
    UINT64 fl;
    KTHREAD *t;

    __asm__ __volatile__("pushfq; popq %0; cli" : "=r"(fl) : : "memory");

    t = g_kcur;

    if (t->lock_depth++ == 0)
        t->lock_if = (fl & (1u << 9)) != 0;
}

void kx_unlock(void)
{
    KTHREAD *t = g_kcur;

    if (t->lock_depth == 0)
        return;

    if (--t->lock_depth == 0 && t->lock_if)
        __asm__ __volatile__("sti" ::: "memory");
}

/* Можно ли внутри kx_lock ненадолго разрешить прерывания (usb.c,
   kx_relax): только если до kx_lock они были разрешены и мы не
   в обработчике прерывания */
BOOLEAN kx_lock_relaxable(void)
{
    KTHREAD *t = g_kcur;

    return t->lock_depth > 0 && t->lock_if && g_kx_isr_depth == 0;
}


/* ================================================================
 * Спин-замок
 * ================================================================ */

/*
 * Для коротких участков, которые могут встретиться и в обработчике
 * прерывания (например, вывод строки в COM1 целиком, чтобы строки
 * разных потоков не перемешивались по буквам).
 *
 * Сначала запрещаем прерывания на своём ядре (иначе обработчик
 * прерывания мог бы прийти за тем же замком и крутиться вечно),
 * потом атомарно ставим флаг. На одном ядре флаг всегда свободен;
 * на нескольких - другое ядро подождёт. На всякий случай ждём не
 * вечно: на одном ядре занятый замок - это ошибка (например,
 * паника посреди вывода), и лучше напечатать криво, чем зависнуть.
 */
UINT64 kspin_lock(KSPINLOCK *l)
{
    UINT64 fl = kx_irq_save();

    for (UINTN i = 0; i < 10000000u; i++) {
        if (__atomic_exchange_n(&l->locked, 1u, __ATOMIC_ACQUIRE) == 0)
            return fl;
        cpu_pause();
    }

    return fl;
}

void kspin_unlock(KSPINLOCK *l, UINT64 fl)
{
    __atomic_store_n(&l->locked, 0u, __ATOMIC_RELEASE);
    kx_irq_restore(fl);
}


/* ================================================================
 * Выбор следующего потока и переключение
 * ================================================================ */

/* Встать в конец очереди готовых (с прерываниями запрещёнными) */
static void kt_make_ready(KTHREAD *t, BOOLEAN boost)
{
    t->state = KT_READY;
    t->ready_seq = ++g_ready_seq;
    t->boost = boost;
}

/*
 * Кто следующий: из готовых сначала проснувшиеся (boost), среди
 * равных - кто раньше встал в очередь. Текущий поток, если он ещё
 * хочет работать, - только когда больше никого нет. Никого - idle.
 */
static KTHREAD *sched_pick_next(void)
{
    KTHREAD *best = NULL;

    for (UINTN i = 0; i < KT_MAX; i++) {

        KTHREAD *t = &g_kthreads[i];

        if (t == g_kidle || t->state != KT_READY)
            continue;

        if (best == NULL ||
            (t->boost && !best->boost) ||
            (t->boost == best->boost && t->ready_seq < best->ready_seq))
            best = t;
    }

    if (best != NULL)
        return best;

    if (g_kcur->state == KT_RUNNING && g_kcur != g_kidle)
        return g_kcur;

    return g_kidle;
}

/* Есть ли кто-то готовый, кроме текущего и idle */
static BOOLEAN sched_others_ready(void)
{
    for (UINTN i = 0; i < KT_MAX; i++) {

        KTHREAD *t = &g_kthreads[i];

        if (t != g_kcur && t != g_kidle && t->state == KT_READY)
            return TRUE;
    }

    return FALSE;
}

/*
 * Отдать процессор следующему потоку. Вызывается:
 *   - потоком, который засыпает/ждёт (его state уже не RUNNING);
 *   - sched_yield (поток сам уступает очередь);
 *   - из конца обработчика прерывания (вытеснение).
 * Возвращается, когда до этого потока снова дошла очередь.
 */
static void sched_switch(BOOLEAN from_isr)
{
    UINT64 fl = kx_irq_save();

    g_need_resched = FALSE;

    KTHREAD *prev = g_kcur;
    BOOLEAN expired = (g_quantum_left == 0);

    /*
     * Текущий поток, если он ещё хочет работать, снова встаёт в
     * очередь:
     *   - квант кончился или уступил сам (sched_yield) - в конец;
     *   - его вытеснил проснувшийся (прерывание разбудило шелл, а
     *     квант ещё не вышел) - сохраняет своё место: первым среди
     *     "крутящих", сразу после проснувшихся.
     * Сравнивать с остальными надо уже с новым местом, поэтому -
     * до выбора следующего.
     */
    if (prev->state == KT_RUNNING && prev != g_kidle) {

        if (!from_isr || expired) {
            prev->ready_seq = ++g_ready_seq;
            prev->quantum_left = KT_QUANTUM_MS;
        } else {
            /* вытеснен проснувшимся: остаток кванта - за ним */
            prev->quantum_left = g_quantum_left;
        }

        prev->boost = FALSE;
        prev->state = KT_READY;
    }

    KTHREAD *next = sched_pick_next();

    /* никого другого - продолжаем тот же поток */
    if (next == prev && prev->state == KT_READY)
        prev->state = KT_RUNNING;

    /* квант следующего: остаток, если он есть, иначе полный */
    g_quantum_left = (next->quantum_left != 0 && next->quantum_left <= KT_QUANTUM_MS)
                         ? next->quantum_left : KT_QUANTUM_MS;
    next->quantum_left = 0;

    if (next != NULL && next != prev) {

        /* учёт времени: всё с начала "смены" - на счёт prev */
        UINT64 now = rdtsc();

        prev->cpu_tsc += now - g_slice_start;
        g_slice_start = now;

        if (from_isr && prev->state == KT_READY)
            g_sched_preempts++;

        if (prev == g_kidle && prev->state == KT_RUNNING)
            prev->state = KT_READY;

        next->state = KT_RUNNING;
        next->switches++;
        g_sched_switches++;

        g_kcur = next;

        /* стек ядра для прерываний/syscall и таблицы страниц нового
           потока (у потоков программ - свои, этап 6) */
        proc_switch_hook(next);

        kx_switch(&prev->rsp, next->rsp);

        /* ...сюда поток prev попадёт, когда его снова выберут */
    }

    kx_irq_restore(fl);
}

void schedule(void)
{
    if (!g_sched_on)
        return;

    sched_switch(FALSE);
}

void sched_yield(void)
{
    if (!sched_can_block())
        return;

    sched_switch(FALSE);
}

/* Вызывается из самого конца обработчика прерывания (cpu.c), когда
   счётчик вложенности уже уменьшен: если надо - сменить поток */
void sched_isr_exit(void)
{
    if (g_sched_on && g_need_resched && g_kx_isr_depth == 0)
        sched_switch(TRUE);
}

/* Можно ли сейчас уснуть/ждать: планировщик работает, мы не в
   обработчике прерывания и не поток idle (ему ждать нечего - он и
   есть "ожидание") */
BOOLEAN sched_can_block(void)
{
    return g_sched_on && g_kx_isr_depth == 0 && g_kcur != g_kidle;
}


/* ================================================================
 * Тик таймера (из обработчика прерывания, 1000 раз в секунду)
 * ================================================================ */

void sched_tick(void)
{
    if (!g_sched_on)
        return;

    UINT64 now = g_kticks;

    for (UINTN i = 0; i < KT_MAX; i++) {

        KTHREAD *t = &g_kthreads[i];

        if (t->state == KT_SLEEPING && now >= t->wake_tick) {

            kt_make_ready(t, TRUE);
            g_need_resched = TRUE;

        } else if (t->state == KT_BLOCKED && t->wake_tick != 0 &&
                   now >= t->wake_tick) {

            kt_make_ready(t, TRUE);
            t->timed_out = TRUE;
            t->wait_on = NULL;
            g_need_resched = TRUE;
        }
    }

    /* квант текущего потока */
    if (g_kcur != g_kidle && g_quantum_left > 0) {

        g_quantum_left--;

        if (g_quantum_left == 0) {

            if (sched_others_ready())
                g_need_resched = TRUE;
            else
                g_quantum_left = KT_QUANTUM_MS;   /* работай дальше */
        }
    }
}


/* ================================================================
 * Сон и ожидание
 * ================================================================ */

/* Уснуть на ms миллисекунд - процессор тем временем достаётся
   другим. "Не меньше ms": будим на тик позже (текущий тик мог
   почти закончиться). Только когда sched_can_block(). */
void sched_sleep_ms(UINT64 ms)
{
    UINT64 fl = kx_irq_save();
    KTHREAD *t = g_kcur;

    t->state = KT_SLEEPING;
    t->wake_tick = g_kticks + ms + 1u;
    t->wait_what = "sleep";

    sched_switch(FALSE);

    t->wait_what = NULL;
    kx_irq_restore(fl);
}

/*
 * Ждать события obj (любой адрес - "номер" события), не дольше
 * timeout_ms (0 - сколько угодно). TRUE - разбудили, FALSE - вышел
 * таймаут.
 *
 * Правило пользования (иначе событие можно "проспать"):
 *     fl = kx_irq_save();
 *     while (!условие)
 *         sched_block(&условие, "что ждём", 0);
 *     kx_irq_restore(fl);
 * Проверка условия и засыпание идут при запрещённых прерываниях,
 * поэтому тот, кто выставит условие и позовёт sched_wake_*, не
 * может вклиниться между ними.
 */
BOOLEAN sched_block(const void *obj, const char *what, UINT64 timeout_ms)
{
    if (!sched_can_block()) {

        /* планировщика нет (или мы в обработчике) - спать не
           умеем; даём прерываниям шанс и возвращаемся: вызывающий
           снова проверит условие */
        __asm__ __volatile__("sti; nop; pause; cli" ::: "memory");
        return TRUE;
    }

    UINT64 fl = kx_irq_save();
    KTHREAD *t = g_kcur;

    t->state = KT_BLOCKED;
    t->wait_on = obj;
    t->wait_what = what;
    t->timed_out = FALSE;
    t->wake_tick = (timeout_ms != 0) ? g_kticks + timeout_ms + 1u : 0;

    sched_switch(FALSE);

    t->wait_on = NULL;
    t->wait_what = NULL;

    BOOLEAN woken = !t->timed_out;

    kx_irq_restore(fl);

    return woken;
}

/* Разбудить всех, кто ждёт obj. Можно звать и из обработчика
   прерывания. Возвращает, сколько разбудили. */
UINTN sched_wake_all(const void *obj)
{
    UINT64 fl = kx_irq_save();
    UINTN n = 0;

    for (UINTN i = 0; i < KT_MAX; i++) {

        KTHREAD *t = &g_kthreads[i];

        if (t->state == KT_BLOCKED && t->wait_on == obj) {
            kt_make_ready(t, TRUE);
            t->wait_on = NULL;
            n++;
        }
    }

    /* из обработчика прерывания - переключиться сразу на выходе
       из него (например, USB-поток просыпается на подключение
       мыши, пока процессор спал в idle) */
    if (n > 0 && (g_kx_isr_depth > 0 || g_kcur == g_kidle))
        g_need_resched = TRUE;

    kx_irq_restore(fl);

    return n;
}

/* Разбудить одного (первого по таблице) - для мьютекса */
BOOLEAN sched_wake_one(const void *obj)
{
    UINT64 fl = kx_irq_save();
    BOOLEAN any = FALSE;

    for (UINTN i = 0; i < KT_MAX; i++) {

        KTHREAD *t = &g_kthreads[i];

        if (t->state == KT_BLOCKED && t->wait_on == obj) {
            kt_make_ready(t, TRUE);
            t->wait_on = NULL;
            any = TRUE;
            break;
        }
    }

    if (any && g_kx_isr_depth > 0)
        g_need_resched = TRUE;

    kx_irq_restore(fl);

    return any;
}


/* ================================================================
 * Мьютекс
 * ================================================================ */

/*
 * Если замок свободен или уже наш - берём. Если занят другим
 * потоком - засыпаем, пока владелец не отпустит (kmutex_unlock
 * будит одного ждущего; тот снова проверяет - вдруг замок успел
 * перехватить кто-то третий).
 *
 * В обработчике прерывания мьютекс брать нельзя (там спать нельзя)
 * - для таких мест есть kx_lock и KSPINLOCK.
 */
void kmutex_lock(KMUTEX *m)
{
    if (g_kx_isr_depth > 0)
        return;

    UINT64 fl = kx_irq_save();
    KTHREAD *me = g_kcur;

    if (m->owner == me) {
        m->count++;
        kx_irq_restore(fl);
        return;
    }

    if (m->owner != NULL)
        m->waits++;

    while (m->owner != NULL)
        sched_block(m, m->name ? m->name : "mutex", 0);

    m->owner = me;
    m->count = 1;

    kx_irq_restore(fl);
}

void kmutex_unlock(KMUTEX *m)
{
    if (g_kx_isr_depth > 0)
        return;

    UINT64 fl = kx_irq_save();

    if (m->owner == g_kcur && m->count > 0) {

        m->count--;

        if (m->count == 0) {
            m->owner = NULL;
            sched_wake_one(m);
        }
    }

    kx_irq_restore(fl);
}


/* ================================================================
 * Создание и завершение потоков
 * ================================================================ */

/* С этого начинает каждый новый поток (сюда ведёт "ret" в
   kx_switch - см. подготовку стека в kthread_create) */
static void kt_trampoline(void)
{
    KTHREAD *t = g_kcur;

    /* планировщик переключил нас с запрещёнными прерываниями -
       новый поток работает с разрешёнными */
    kx_sti();

    t->entry(t->arg);

    kthread_exit();
}

static void kt_set_name(KTHREAD *t, const char *name)
{
    UINTN i = 0;

    while (name != NULL && name[i] != '\0' && i + 1 < KT_NAME_LEN) {
        t->name[i] = name[i];
        i++;
    }

    t->name[i] = '\0';
}

/* Переименовать поток (шелл, запустивший GUI, в ps - "gui") */
void kthread_rename(KTHREAD *t, const char *name)
{
    UINT64 fl = kx_irq_save();
    kt_set_name(t, name);
    kx_irq_restore(fl);
}

/*
 * Создать поток: он начнёт выполнять fn(arg) при первой же своей
 * очереди. Стек - stack_pages страниц по 4 КиБ (с защитной
 * страницей снизу). Слоты и стеки завершившихся потоков
 * используются повторно. NULL - нет места или памяти.
 */
KTHREAD *kthread_create(const char *name, void (*fn)(void *), void *arg, UINTN stack_pages)
{
    if (!g_sched_on || fn == NULL)
        return NULL;

    if (stack_pages < 4)
        stack_pages = 4;

    UINT64 fl = kx_irq_save();

    /* 1) мёртвый поток со стеком не меньше нужного; 2) пустой слот */
    KTHREAD *t = NULL;

    for (UINTN i = 0; i < KT_MAX && t == NULL; i++) {
        KTHREAD *c = &g_kthreads[i];
        if (c->state == KT_DEAD && c != g_kcur && c->stack_pages >= stack_pages)
            t = c;
    }

    for (UINTN i = 0; i < KT_MAX && t == NULL; i++)
        if (g_kthreads[i].state == KT_UNUSED)
            t = &g_kthreads[i];

    if (t == NULL) {
        kx_irq_restore(fl);
        return NULL;
    }

    /* пока готовим - пусть никто не считает слот свободным */
    KT_STATE was = t->state;
    t->state = KT_BLOCKED;
    t->wait_on = t;
    t->wake_tick = 0;

    kx_irq_restore(fl);

    if (was == KT_UNUSED || t->stack_top == 0) {

        UINT64 top = vmm_alloc_stack(stack_pages, "thread");

        if (top == 0) {
            t->state = KT_UNUSED;
            return NULL;
        }

        t->stack_top = top;
        t->stack_bottom = top - (UINT64)stack_pages * 4096u;
        t->stack_pages = stack_pages;

    } else {

        /* повторно используемый стек - обнулить: так ps честно
           посчитает, сколько стека израсходовал новый хозяин */
        volatile UINT64 *p = (volatile UINT64 *)t->stack_bottom;
        UINTN n = (UINTN)((t->stack_top - t->stack_bottom) / 8u);

        for (UINTN i = 0; i < n; i++)
            p[i] = 0;
    }

    /* имя стека для экрана паники ("GUARD PAGE below ...") */
    for (UINTN i = 0; i < g_kstack_count; i++)
        if (g_kstacks[i].top == t->stack_top)
            g_kstacks[i].name = "thread";

    kt_set_name(t, name);

    t->tid = g_next_tid++;
    t->slot = (UINT32)(t - g_kthreads);
    t->entry = fn;
    t->arg = arg;
    t->lock_depth = 0;
    t->lock_if = FALSE;
    t->cpu_tsc = 0;
    t->cpu_tsc_prev = 0;
    t->load_permille = 0;
    t->switches = 0;
    t->started_ms = g_kticks;
    t->wait_what = NULL;
    t->timed_out = FALSE;
    t->cr3 = 0;
    t->proc = NULL;

    /*
     * Стек нового потока - так, будто он уже побывал в kx_switch:
     *
     *   top-8   : 0                 (выравнивание: в kt_trampoline
     *                                 rsp должен быть = 8 по модулю 16,
     *                                 как у любой только что вызванной
     *                                 функции)
     *   top-16  : kt_trampoline     (адрес для ret)
     *   top-24..top-64 : rbp, rbx, r12, r13, r14, r15 = 0
     *   top-72  : флаги = 0x2       (бит 1 всегда 1; IF = 0 -
     *                                 trampoline включит сам)
     */
    UINT64 *sp = (UINT64 *)(t->stack_top & ~0xFull);

    *--sp = 0;
    *--sp = (UINT64)(UINTN)kt_trampoline;
    *--sp = 0;     /* rbp */
    *--sp = 0;     /* rbx */
    *--sp = 0;     /* r12 */
    *--sp = 0;     /* r13 */
    *--sp = 0;     /* r14 */
    *--sp = 0;     /* r15 */
    *--sp = 0x2;   /* rflags */

    t->rsp = (UINT64)(UINTN)sp;

    fl = kx_irq_save();
    t->wait_on = NULL;
    kt_make_ready(t, FALSE);
    kx_irq_restore(fl);

    klog("sched: thread %u '%s' created (stack %u KiB)\n",
         t->tid, t->name, (UINT32)(t->stack_pages * 4u));

    return t;
}

/* Завершить текущий поток. Его стек и слот достанутся следующему
   kthread_create. */
void kthread_exit(void)
{
    kx_irq_save();

    KTHREAD *t = g_kcur;

    t->state = KT_DEAD;
    t->lock_depth = 0;

    /* кто-то мог ждать нашего завершения */
    sched_wake_all(t);

    sched_switch(FALSE);

    /* сюда не возвращаются: мёртвый поток больше не выбирают */
    for (;;)
        kx_hlt();
}

/* Жив ли ещё поток t с номером tid (слот мог уже достаться другому) */
BOOLEAN kthread_alive(KTHREAD *t, UINT32 tid)
{
    return t != NULL && t->tid == tid &&
           t->state != KT_DEAD && t->state != KT_UNUSED;
}


/* ================================================================
 * Поток idle и запуск
 * ================================================================ */

/* Когда никому не нужен процессор - спим до прерывания. Время в
   hlt идёт в учёт "простоя" (irq.c), отсюда загрузка в cpu. */
static void kt_idle(void *arg)
{
    (void)arg;

    for (;;) {

        /* кто-то проснулся, а переключения ещё не было (например,
           разбудили не из прерывания) - отдать процессор */
        if (sched_others_ready()) {
            sched_switch(FALSE);
            continue;
        }

        kx_idle_hlt();
    }
}

void sched_start(SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    if (!g_ktimer_ok) {
        print(out, "  Timer is not running - no threads: everything runs in one flow.\n");
        return;
    }

    KTHREAD *me = &g_kthreads[0];

    /* стек главного потока - тот, на котором мы сейчас */
    UINT64 rsp;

    __asm__ __volatile__("movq %%rsp, %0" : "=r"(rsp));

    for (UINTN i = 0; i < g_kstack_count; i++) {
        if (rsp >= g_kstacks[i].bottom && rsp < g_kstacks[i].top) {
            me->stack_bottom = g_kstacks[i].bottom;
            me->stack_top = g_kstacks[i].top;
            me->stack_pages = (UINTN)((g_kstacks[i].top - g_kstacks[i].bottom) / 4096u);
        }
    }

    me->started_ms = g_kticks;
    me->switches = 1;

    g_slice_start = rdtsc();
    g_acct_last = g_slice_start;

    /* с этого момента kthread_create работает */
    g_sched_on = TRUE;

    g_kidle = kthread_create("idle", kt_idle, NULL, 4);

    if (g_kidle == NULL) {
        g_sched_on = FALSE;
        print(out, "  Could not create the idle thread - no threads.\n");
        return;
    }

    kprintf(out, "  Scheduler: preemptive round-robin, %u ms quantum, "
                 "threads switch on the %s\n",
            (UINT32)KT_QUANTUM_MS, "LAPIC timer (1 kHz)");
    print(out, "  Threads: 'shell' (this code), 'idle' (halts the CPU when nobody needs it)\n");
}


/* ================================================================
 * Учёт процессорного времени (раз в секунду, из таймера)
 * ================================================================ */

void sched_account_load(void)
{
    if (!g_sched_on)
        return;

    UINT64 now = rdtsc();

    g_kcur->cpu_tsc += now - g_slice_start;
    g_slice_start = now;

    UINT64 total = now - g_acct_last;

    g_acct_last = now;

    if (total == 0)
        return;

    for (UINTN i = 0; i < KT_MAX; i++) {

        KTHREAD *t = &g_kthreads[i];

        if (t->state == KT_UNUSED)
            continue;

        UINT64 d = t->cpu_tsc - t->cpu_tsc_prev;

        t->cpu_tsc_prev = t->cpu_tsc;
        t->load_permille = (d >= total) ? 1000u : (UINT32)((d * 1000u) / total);

        if (t->state == KT_DEAD)
            t->load_permille = 0;
    }
}


/* ================================================================
 * Для ps
 * ================================================================ */

const char *kthread_state_name(KT_STATE s)
{
    switch (s) {
    case KT_READY:    return "ready";
    case KT_RUNNING:  return "running";
    case KT_SLEEPING: return "sleeping";
    case KT_BLOCKED:  return "waiting";
    case KT_DEAD:     return "finished";
    default:          return "-";
    }
}

/* Сколько байт стека поток уже использовал (самое глубокое место):
   стек выдаётся обнулённым - ищем снизу первое ненулевое слово */
UINTN kthread_stack_used(KTHREAD *t)
{
    if (t->stack_top == 0 || t->stack_bottom == 0)
        return 0;

    volatile UINT64 *p = (volatile UINT64 *)t->stack_bottom;
    UINTN n = (UINTN)((t->stack_top - t->stack_bottom) / 8u);

    for (UINTN i = 0; i < n; i++)
        if (p[i] != 0)
            return (n - i) * 8u;

    return 0;
}

/* Снимок таблицы (чтобы печатать спокойно, пока потоки меняются) */
UINTN sched_snapshot(KT_INFO *out, UINTN cap)
{
    UINTN n = 0;

    UINT64 fl = kx_irq_save();

    for (UINTN i = 0; i < KT_MAX && n < cap; i++) {

        KTHREAD *t = &g_kthreads[i];

        if (t->state == KT_UNUSED || t->state == KT_DEAD)
            continue;

        KT_INFO *o = &out[n++];

        o->tid = t->tid;
        for (UINTN k = 0; k < KT_NAME_LEN; k++)
            o->name[k] = t->name[k];
        o->state = t->state;
        o->current = (t == g_kcur);
        o->load_permille = t->load_permille;
        o->cpu_ms = (g_tsc_hz != 0) ? (t->cpu_tsc * 1000u) / g_tsc_hz : 0;
        o->switches = t->switches;
        o->stack_kib = (UINT32)(t->stack_pages * 4u);
        o->wait_what = t->wait_what;
        o->wake_in_ms = 0;
        if ((t->state == KT_SLEEPING || t->state == KT_BLOCKED) &&
            t->wake_tick > g_kticks)
            o->wake_in_ms = t->wake_tick - g_kticks;
        o->stack_ptr = t;
    }

    kx_irq_restore(fl);

    /* глубину стека считаем уже без запрета прерываний (это долго) */
    for (UINTN i = 0; i < n; i++)
        out[i].stack_used_kib =
            (UINT32)((kthread_stack_used((KTHREAD *)out[i].stack_ptr) + 1023u) / 1024u);

    return n;
}


/* ================================================================
 * Команда ps
 * ================================================================ */

void kernel_cmd_ps(SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    if (!g_sched_on) {
        print(out, "No threads: the timer is not running, everything runs in one flow.\n");
        return;
    }

    KT_INFO info[KT_MAX];
    UINTN n = sched_snapshot(info, KT_MAX);

    kprintf(out, "Threads: %u   scheduler: preemptive round-robin, quantum %u ms\n",
            (UINT32)n, (UINT32)KT_QUANTUM_MS);
    kprintf(out, "Context switches: %llu total, %llu forced by the timer (a thread's time ran out)\n\n",
            g_sched_switches, g_sched_preempts);

    print(out, "  TID  NAME            STATE      CPU%   CPU TIME    RUNS  STACK USED   WAITING FOR\n");

    for (UINTN i = 0; i < n; i++) {

        KT_INFO *o = &info[i];
        char wait[48];

        wait[0] = '\0';

        if (o->state == KT_SLEEPING)
            ksnprintf(wait, sizeof(wait), "timer (%llu ms left)", o->wake_in_ms);
        else if (o->state == KT_BLOCKED && o->wake_in_ms != 0)
            ksnprintf(wait, sizeof(wait), "%s (<= %llu ms)",
                      o->wait_what ? o->wait_what : "event", o->wake_in_ms);
        else if (o->state == KT_BLOCKED)
            ksnprintf(wait, sizeof(wait), "%s", o->wait_what ? o->wait_what : "event");
        else if (o->current)
            ksnprintf(wait, sizeof(wait), "- (this is the one printing)");

        kprintf(out, "  %3u  %-15s %-9s %3u.%u%%  %6llu ms  %6llu  %3u/%3u KiB  %s\n",
                o->tid, o->name, kthread_state_name(o->state),
                o->load_permille / 10u, o->load_permille % 10u,
                o->cpu_ms, o->switches,
                o->stack_used_kib, o->stack_kib, wait);
    }

    print(out, "\nCPU% - share of the last second. 'idle' is the free time: the CPU\n"
               "sleeps (hlt) there until the next interrupt.\n");
}


/* ================================================================
 * Команда threadtest: живая проверка потоков, вытеснения и мьютекса
 * ================================================================ */

static volatile UINT64 g_tt_count[3];
static volatile UINT64 g_tt_first[3];
static volatile UINT64 g_tt_last[3];
static volatile UINT64 g_tt_deadline;
static volatile UINT32 g_tt_finished;

/* Считает, пока не выйдет общее время. Процессор сам НЕ отдаёт -
   если остальные два потока тоже что-то насчитают, значит их
   переключал таймер (вытеснение работает). */
static void tt_counter(void *arg)
{
    UINTN i = (UINTN)arg;

    g_tt_first[i] = g_kticks;

    while (rdtsc() < g_tt_deadline) {
        g_tt_count[i]++;
        g_tt_last[i] = g_kticks;
    }

    __atomic_add_fetch(&g_tt_finished, 1u, __ATOMIC_SEQ_CST);
}

static KMUTEX g_tt_mutex = KMUTEX_INIT("threadtest lock");
static volatile UINT64 g_tt_shared;
static volatile UINT32 g_tt_adders_done;

#define TT_ADDS 300

/* "Прочитать - уступить очередь - записать +1": классическая гонка.
   Без замка второй поток успевает прочитать то же старое значение,
   и прибавления теряются. С замком читать-писать может только один. */
static void tt_adder(void *arg)
{
    BOOLEAN use_lock = (arg != NULL);

    for (UINTN k = 0; k < TT_ADDS; k++) {

        if (use_lock)
            kmutex_lock(&g_tt_mutex);

        UINT64 v = g_tt_shared;

        sched_yield();

        g_tt_shared = v + 1u;

        if (use_lock)
            kmutex_unlock(&g_tt_mutex);
    }

    __atomic_add_fetch(&g_tt_adders_done, 1u, __ATOMIC_SEQ_CST);
}

/* Подождать, пока счётчик дойдёт до want (не дольше limit_ms) */
static BOOLEAN tt_wait(volatile UINT32 *counter, UINT32 want, UINT64 limit_ms)
{
    UINT64 t0 = g_kticks;

    while (*counter < want) {

        if (g_kticks - t0 > limit_ms)
            return FALSE;

        sched_sleep_ms(10);
    }

    return TRUE;
}

static UINT64 tt_run_adders(BOOLEAN use_lock)
{
    g_tt_shared = 0;
    g_tt_adders_done = 0;

    void *arg = use_lock ? (void *)1 : NULL;

    if (!kthread_create("tt-add-1", tt_adder, arg, 4) ||
        !kthread_create("tt-add-2", tt_adder, arg, 4))
        return 0;

    if (!tt_wait(&g_tt_adders_done, 2, 5000))
        return 0;

    return g_tt_shared;
}

void kernel_cmd_threadtest(SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    if (!sched_can_block()) {
        print(out, "Threads are not running (no timer) - nothing to test.\n");
        return;
    }

    BOOLEAN ok = TRUE;

    /* --- 1. три потока-счётчика одновременно, 300 мс --- */
    print(out, "1) Three threads count as fast as they can for 300 ms, never\n"
               "   giving the CPU away by themselves...\n");
    kcon_flush();

    for (UINTN i = 0; i < 3; i++) {
        g_tt_count[i] = 0;
        g_tt_first[i] = 0;
        g_tt_last[i] = 0;
    }

    g_tt_finished = 0;
    g_tt_deadline = rdtsc() + (g_tsc_hz * 300u) / 1000u;

    UINT64 t0 = g_kticks;

    const char *names[3] = { "tt-count-A", "tt-count-B", "tt-count-C" };

    for (UINTN i = 0; i < 3; i++) {
        if (!kthread_create(names[i], tt_counter, (void *)i, 4)) {
            print(out, "   could not create a thread\n");
            return;
        }
    }

    if (!tt_wait(&g_tt_finished, 3, 5000)) {
        print(out, "   FAIL: the threads did not finish in 5 s\n");
        return;
    }

    UINT64 wall = g_kticks - t0;
    UINT64 mn = g_tt_count[0], mx = g_tt_count[0], sum = 0;

    for (UINTN i = 0; i < 3; i++) {
        if (g_tt_count[i] < mn) mn = g_tt_count[i];
        if (g_tt_count[i] > mx) mx = g_tt_count[i];
        sum += g_tt_count[i];
    }

    for (UINTN i = 0; i < 3; i++) {
        UINT64 share = sum ? (g_tt_count[i] * 1000u) / sum : 0;
        kprintf(out, "   %s: %llu loops (%llu.%llu%% of all), ran from %llu to %llu ms\n",
                names[i], g_tt_count[i], share / 10u, share % 10u,
                g_tt_first[i] - t0, g_tt_last[i] - t0);
    }

    /* все три работали в одном и том же отрезке времени? */
    BOOLEAN overlap = TRUE;

    for (UINTN i = 0; i < 3; i++)
        for (UINTN j = 0; j < 3; j++)
            if (g_tt_first[i] > g_tt_last[j])
                overlap = FALSE;

    BOOLEAN fair = (mn > 0) && (mn * 2u >= mx);

    kprintf(out, "   all three together took %llu ms (one after another it would be ~900)\n", wall);
    kprintf(out, "   -> %s, %s\n",
            overlap ? "they ran at the same time" : "NOT at the same time",
            fair ? "the timer shared the CPU fairly" : "the sharing is UNFAIR");

    if (!overlap || !fair)
        ok = FALSE;

    /* --- 2. гонка и мьютекс --- */
    kprintf(out, "\n2) Two threads each add 1 to a shared number %u times\n"
                 "   (read it, give the CPU away, write it + 1)...\n", (UINT32)TT_ADDS);
    kcon_flush();

    UINT64 no_lock = tt_run_adders(FALSE);
    UINT64 with_lock = tt_run_adders(TRUE);

    kprintf(out, "   without a lock: %llu (should be %u - additions got lost)\n",
            no_lock, (UINT32)(2u * TT_ADDS));
    kprintf(out, "   with a mutex:   %llu%s\n", with_lock,
            with_lock == 2u * TT_ADDS ? " - correct" : " - WRONG");

    if (with_lock != 2u * TT_ADDS)
        ok = FALSE;

    print(out, ok ? "\nthreadtest: OK\n" : "\nthreadtest: FAILED\n");
}
