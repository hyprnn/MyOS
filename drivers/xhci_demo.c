/*
 * drivers/xhci_demo.c - старое пошаговое демо xHCI одной мыши (команда ebsdemo).
 * Часть MyOS; общие объявления - в myos.h.
 */
#include "myos.h"

/*
 * ev_slot у нас - это "сквозной", вечно растущий номер по счёту
 * обработанного события (0, 1, 2, 3, ...), а не индекс внутри
 * кольца - так удобнее передавать между функциями (Address
 * Device, GET_DESCRIPTOR, Configure Endpoint, опрос отчётов
 * мыши - все они просто продолжают считать оттуда, где
 * остановился предыдущий шаг). Но физический адрес TRB в памяти
 * и ожидаемое значение его Cycle Bit нужно каждый раз вычислять
 * заново из этого сквозного номера - вот эти две функции.
 *
 * НАЙДЕННЫЙ БАГ (по скриншоту - опрос отчётов мыши "останавливался"
 * ровно около 249-256 отчёта, а перед остановкой печатались три
 * подозрительные строки "skipping unrelated event, TRB Type=9/11/12"
 * - а это же типы самих КОМАНД (Enable Slot/Address Device/
 * Configure Endpoint), а не событий (у событий Command Completion
 * Event всегда TRB Type=33!). Причина: раньше адрес TRB считался
 * просто как evring_phys + ev_slot*16, без остатка от деления на
 * размер кольца - то есть уже после первых 256 обработанных
 * событий код преспокойно "уезжал" за пределы выделенной под
 * Event Ring страницы и начинал читать СОСЕДНЮЮ страницу памяти
 * (которая оказалась Command Ring'ом - там как раз и лежат TRB
 * с типами 9/11/12, ещё с выставленным Cycle=1, поэтому код
 * принимал их за "новые события"). Дальше, за Command Ring'ом,
 * шла обнулённая память (Cycle=0) - вот и всё, опрос "замирал",
 * решив, что событий больше нет.
 *
 * Правильное поведение xHCI: Event Ring - кольцевой буфер.
 * Контроллер, записав TRB в последний слот кольца, на следующем
 * событии возвращается к слоту 0 - но при этом переворачивает
 * Cycle Bit (первый круг - пишет туда 1, второй круг - 0, третий
 * - снова 1, и так далее). Это единственный способ software
 * отличить "ещё не обработанное новое событие второго круга" от
 * "старого мусора первого круга, оставшегося по тому же адресу".
 * Поэтому здесь мы: (1) берём остаток от деления ev_slot на
 * размер кольца - получаем настоящий адрес внутри кольца;
 * (2) считаем, сколько полных кругов кольца уже пройдено
 * (ev_slot / размер кольца), и если это число нечётное - значит
 * сейчас ожидаем Cycle=0, если чётное (включая 0) - ожидаем
 * Cycle=1.
 */
UINT64 xhci_event_ring_trb_addr(
    UINT64 evring_phys,
    UINTN  ev_slot
)
{
    UINTN slot_in_ring =
        ev_slot % (UINTN)XHCI_EVENT_RING_TRBS;

    return evring_phys + (UINT64)slot_in_ring * 16u;
}

UINT32 xhci_event_ring_expected_cycle(
    UINTN ev_slot
)
{
    UINTN laps_done =
        ev_slot / (UINTN)XHCI_EVENT_RING_TRBS;

    if ((laps_done % 2u) == 0u)
        return 1u;

    return 0u;
}

UINT64 xhci_xfer_ring_trb_addr(
    UINT64 ring_phys,
    UINTN  seq
)
{
    UINTN slot_in_ring =
        seq % (UINTN)XHCI_XFER_RING_USABLE_TRBS;

    return ring_phys + (UINT64)slot_in_ring * 16u;
}

UINT32 xhci_xfer_ring_pcs(
    UINTN seq
)
{
    UINTN laps_done =
        seq / (UINTN)XHCI_XFER_RING_USABLE_TRBS;

    if ((laps_done % 2u) == 0u)
        return 1u;

    return 0u;
}

BOOLEAN xhci_wait_for_event(
    SIMPLE_TEXT_OUTPUT_INTERFACE *out,
    UINT64 evring_phys,
    UINT64 intr0,
    UINTN *io_slot,
    UINT8 want_trb_type,
    volatile UINT32 **out_trb
)
{
    for (;;) {

        UINT64 trb_addr =
            xhci_event_ring_trb_addr(evring_phys, *io_slot);

        UINT32 expected_cycle =
            xhci_event_ring_expected_cycle(*io_slot);

        volatile UINT32 *trb =
            (volatile UINT32 *)(UINTN)trb_addr;

        BOOLEAN got = FALSE;

        for (UINTN i = 0; i < 500; i++) {

            /* Раньше здесь стоял BootServices->Stall(1000) -
               эта функция теперь вызывается только после
               ExitBootServices (см. команду "ebs"), поэтому
               задержка firmware-независимая. */
            busy_wait_ms(1);

            if ((trb[3] & 0x1u) == expected_cycle) {
                got = TRUE;
                break;
            }
        }

        if (!got)
            return FALSE;

        UINT8 t = (UINT8)((trb[3] >> 10) & 0x3Fu);

        if (t == want_trb_type) {
            *out_trb = trb;
            return TRUE;
        }

        /* постороннее событие (например Port Status Change,
           TRB Type=34, которое генерит сам Port Reset) - не
           то, чего мы ждём именно в этой точке кода, но оно
           уже заняло место в Event Ring, и его нужно
           подтвердить (сдвинуть ERDP), иначе контроллер будет
           считать, что мы его ещё не обработали, и рано или
           поздно решит, что кольцо переполнено. Пропускаем и
           идём дальше. */

        print(out, "  (skipping unrelated event, TRB Type=");
        print_uint(out, t);
        print(out, ")\n");

        (*io_slot) = (*io_slot) + 1;

        UINT64 next_addr =
            xhci_event_ring_trb_addr(evring_phys, *io_slot);

        mmio_write32(
            intr0 + 0x18,
            (UINT32)(next_addr & 0xFFFFFFFFu)
        );
        mmio_write32(
            intr0 + 0x1C,
            (UINT32)(next_addr >> 32)
        );
    }
}



/*
 * Универсальный USB Control Transfer через Endpoint 0 (Setup +
 * опционально Data + Status), поверх уже настроенного Transfer
 * Ring конкретного устройства. Раньше (в самой первой версии
 * GET_DESCRIPTOR) все три TRB собирались вручную прямо в теле
 * xhci_address_device_and_get_descriptor - именно там и нашлась
 * ошибка с перепутанными битами IDT/IOC. Вынесено сюда одной
 * функцией: во-первых, чтобы больше не дублировать этот код
 * (следующие шаги - SET_CONFIGURATION, HID SET_PROTOCOL и
 * т.п. - тоже control transfers), во-вторых, чтобы саму логику
 * битов было где один раз перепроверить и больше не трогать.
 *
 * io_trb_slot - "текущий свободный слот" на Transfer Ring
 * Endpoint 0, в TRB (по 16 байт), сохраняется между вызовами -
 * кольцо общее на весь Endpoint 0, каждый новый control transfer
 * просто пишется дальше по кольцу.
 * io_ev_slot - аналогично, текущий слот Event Ring (общий на
 * весь контроллер, разделяется и с Command Ring).
 *
 * wLength==0 означает запрос без стадии данных (например
 * SET_CONFIGURATION) - тогда Data Stage TRB не создаётся вообще,
 * и Status Stage TRB идёт сразу после Setup, с направлением IN
 * (по спеке USB: если нет стадии данных, Status всегда IN).
 * Если стадия данных есть - направление Status противоположно
 * направлению Data (это тоже требование спеки).
 *
 * Возвращает FALSE только если событие вообще не появилось
 * (таймаут) - код завершения при этом не проверяется, это
 * решает вызывающий код через out_compl_code (1=Success,
 * 13=Short Packet - тоже нормальный исход для IN-запросов,
 * если устройство прислало меньше данных, чем мы запросили).
 */
BOOLEAN xhci_control_transfer(
    SIMPLE_TEXT_OUTPUT_INTERFACE *out,
    UINT64 xmmio,
    XHCI_CAP_INFO *cap,
    UINT64 evring_phys,
    UINT64 intr0,
    UINT64 ep0_ring_phys,
    UINTN *io_trb_slot,
    UINT8 slot_id,
    UINT8 bm_request_type,
    UINT8 b_request,
    UINT16 w_value,
    UINT16 w_index,
    UINT16 w_length,
    UINT64 data_buf_phys,
    UINTN *io_ev_slot,
    UINT8 *out_compl_code
)
{
    /* Setup Stage TRB */

    volatile UINT32 *setup_trb =
        (volatile UINT32 *)
            (UINTN)(ep0_ring_phys + (UINT64)(*io_trb_slot) * 16);

    setup_trb[0] =
        (UINT32)bm_request_type |
        ((UINT32)b_request << 8) |
        ((UINT32)w_value << 16);
    setup_trb[1] =
        (UINT32)w_index | ((UINT32)w_length << 16);
    setup_trb[2] = 8u;

    UINT32 trt; /* Transfer Type: 0=нет данных,
                   2=Data Stage OUT, 3=Data Stage IN */

    if (w_length == 0)
        trt = 0u;
    else if (bm_request_type & 0x80u)
        trt = 3u;
    else
        trt = 2u;

    /* IDT - бит 6 (не 5! бит5 - это IOC, разные вещи -
       см. подробный комментарий на месте, где эта ошибка
       раньше пряталась) */
    setup_trb[3] =
        0x1u | (1u << 6) | (2u << 10) | (trt << 16);

    (*io_trb_slot) = (*io_trb_slot) + 1;

    BOOLEAN has_data = (w_length != 0);

    if (has_data) {

        volatile UINT32 *data_trb =
            (volatile UINT32 *)
                (UINTN)(ep0_ring_phys +
                        (UINT64)(*io_trb_slot) * 16);

        data_trb[0] =
            (UINT32)(data_buf_phys & 0xFFFFFFFFu);
        data_trb[1] =
            (UINT32)(data_buf_phys >> 32);
        data_trb[2] = w_length;

        UINT32 dir =
            (bm_request_type & 0x80u) ? 1u : 0u;

        data_trb[3] =
            0x1u | (3u << 10) | (dir << 16);

        (*io_trb_slot) = (*io_trb_slot) + 1;
    }

    volatile UINT32 *status_trb =
        (volatile UINT32 *)
            (UINTN)(ep0_ring_phys + (UINT64)(*io_trb_slot) * 16);

    /* если данных не было - Status всегда IN; если были -
       направление противоположно направлению Data Stage */
    UINT32 status_dir;

    if (!has_data)
        status_dir = 1u;
    else
        status_dir =
            (bm_request_type & 0x80u) ? 0u : 1u;

    status_trb[0] = 0;
    status_trb[1] = 0;
    status_trb[2] = 0;
    status_trb[3] =
        0x1u | (1u << 5) | (4u << 10) | (status_dir << 16);

    (*io_trb_slot) = (*io_trb_slot) + 1;

    /* Doorbell SlotID, Target=1 (Endpoint 0) */
    mmio_write32(
        xmmio + cap->DbOff + (UINT64)slot_id * 4, 1u
    );

    volatile UINT32 *ev = NULL;

    BOOLEAN got =
        xhci_wait_for_event(
            out, evring_phys, intr0,
            io_ev_slot, 32, &ev
        );

    if (!got)
        return FALSE;

    UINT8 cc = (UINT8)((ev[2] >> 24) & 0xFFu);

    if (out_compl_code)
        *out_compl_code = cc;

    (*io_ev_slot) = (*io_ev_slot) + 1;

    UINT64 next_ev_addr =
        xhci_event_ring_trb_addr(evring_phys, *io_ev_slot);

    mmio_write32(
        intr0 + 0x18,
        (UINT32)(next_ev_addr & 0xFFFFFFFFu)
    );
    mmio_write32(
        intr0 + 0x1C,
        (UINT32)(next_ev_addr >> 32)
    );

    return TRUE;
}



/*
 * Шаг 6 — Address Device + первый настоящий USB-запрос
 * (GET_DESCRIPTOR, Device Descriptor) через Default Control Pipe
 * (Endpoint 0).
 *
 * На входе уже готово (сделано раньше, в команде "xhci"):
 *   - контроллер сброшен, DCBAA и Command Ring настроены и
 *     работают (см. Enable Slot Command),
 *   - Event Ring настроен и мы уже один раз успешно прочитали
 *     из него Command Completion Event (для Enable Slot),
 *   - у устройства есть SlotID (получен Enable Slot Command),
 *   - порт прошёл Port Reset (PED=1).
 *
 * Что делает эта функция:
 *   1. Читает Port Speed из PORTSC уже сброшенного порта - нужно
 *      для двух вещей: поля Speed в Slot Context и стартового
 *      Max Packet Size для Endpoint 0 (у control endpoint это
 *      значение заранее неизвестно точно, пока не прочитан сам
 *      Device Descriptor - поэтому по спеке USB для старта
 *      берётся стандартное значение по скорости порта: 8 байт
 *      для Low/Full Speed, 64 для High Speed, 512 для SuperSpeed).
 *   2. Выделяет и заполняет Input Context (Input Control Context +
 *      Slot Context + Endpoint 0 Context) - "заявка" контроллеру
 *      на то, каким должен стать Device Context.
 *   3. Выделяет пустой Device Context (уйдёт в DCBAA[SlotID] -
 *      это то, что контроллер реально будет использовать и
 *      обновлять сам, в отличие от Input Context, который нужен
 *      только на момент самой команды).
 *   4. Выделяет Transfer Ring для Endpoint 0 - отдельное кольцо
 *      TRB (того же формата, что Command Ring, но для передачи
 *      данных конкретной конечной точке конкретного устройства,
 *      не команд самому контроллеру).
 *   5. Кладёт в Command Ring TRB Address Device (Type 11), звонит
 *      в Doorbell 0, ждёт Command Completion Event - после этого
 *      устройству реально назначен USB-адрес и Endpoint 0 готов
 *      к работе.
 *   6. Строит Control Transfer из трёх TRB на Transfer Ring
 *      Endpoint 0 (Setup Stage + Data Stage IN + Status Stage
 *      OUT) - классический 3-стадийный USB control-запрос,
 *      здесь конкретно GET_DESCRIPTOR(Device), запрашиваем все
 *      18 байт Device Descriptor. Звонит в Doorbell SlotID (не
 *      0! у Command Ring и у Transfer Ring конкретного
 *      устройства разные doorbell'ы), ждёт Transfer Event.
 *   7. Если всё получилось - печатает содержимое Device
 *      Descriptor человеко-читаемо, в первую очередь
 *      Vendor ID/Product ID - то, ради чего всё затевалось: это
 *      настоящие данные с настоящего USB-устройства, добытые
 *      без единого обращения к прошивке.
 *
 * Всё выделение памяти - через AllocatePages (не AllocatePool),
 * по той же причине, что и раньше: xHCI требует выравнивания
 * (Input/Device Context - 64 байта, Transfer Ring - 16 байт),
 * страница (4096, выровнена по странице) даёт выравнивание с
 * большим запасом, а размер тут всё равно намного меньше
 * страницы, так что переплата памятью не важна на этом этапе.
 *
 * ОГРАНИЧЕНИЕ этого шага (явно, чтобы не забыть): Context Size
 * (CSZ, бит 2 HCCPARAMS1) здесь читается и учитывается - если
 * контроллер требует 64-байтные контексты вместо 32-байтных,
 * это меняет только размер шага между Slot Context/EP Context
 * внутри Input Context и Device Context, сам код это учитывает
 * через переменную ctx_size. Дальше по коду отдельно НЕ
 * поддержаны: Multi-TT хабы, Streams, и любые EP кроме
 * Endpoint 0 - это всё будущие шаги (Endpoint 0 достаточно для
 * GET_DESCRIPTOR, но не для реального опроса координат мыши -
 * для этого понадобится ещё и Interrupt IN Endpoint, см.
 * SET_CONFIGURATION в планах дальше).
 */
void xhci_address_device_and_get_descriptor(
    SIMPLE_TEXT_OUTPUT_INTERFACE *out,
    UINT64 xmmio,
    XHCI_CAP_INFO *cap,
    UINT64 dcbaa_phys,
    UINT64 cmdring_phys,
    UINT64 evring_phys,
    UINT64 intr0,
    UINT64 port_base,
    UINTN  root_port,
    UINT8  slot_id,
    UINTN  start_ev_slot,
    /*
     * Раньше эти четыре страницы выделялись прямо здесь через
     * AllocatePages. Теперь эта функция вызывается уже ПОСЛЕ
     * ExitBootServices (см. команду "ebs"), где AllocatePages
     * недоступен - поэтому все страницы выделяются заранее, ещё
     * при живых Boot Services, и передаются сюда готовыми
     * физическими адресами. Функция только обнуляет и заполняет
     * их содержимое, память под них уже есть.
     */
    UINT64 input_ctx_phys,
    UINT64 dev_ctx_phys,
    UINT64 ep0_ring_phys,
    UINT64 desc_buf_phys,
    UINT64 int_ring_phys
)
{
    /* --- 1. скорость порта --- */

    UINT32 portsc_now = mmio_read32(port_base);

    /* Port Speed - биты [13:10] PORTSC. Значения по спеке xHCI
       (таблица Protocol Speed ID по умолчанию, USB2/3 root hub):
       1=Full Speed, 2=Low Speed, 3=High Speed, 4=SuperSpeed. */
    UINT32 port_speed = (portsc_now >> 10) & 0xFu;

    UINT16 ep0_max_packet;

    if (port_speed == 3) {
        ep0_max_packet = 64;   /* High Speed */
    } else if (port_speed == 4) {
        ep0_max_packet = 512;  /* SuperSpeed */
    } else {
        ep0_max_packet = 8;    /* Low/Full Speed - стандартный
                                   стартовый минимум по спеке USB,
                                   настоящее значение придёт в
                                   самом Device Descriptor
                                   (bMaxPacketSize0), для первого
                                   запроса используем его позже,
                                   если захотим перечитать точнее */
    }

    print(out, "Port speed code = ");
    print_uint(out, port_speed);
    print(out, " (1=FS 2=LS 3=HS 4=SS), EP0 MaxPacket = ");
    print_uint(out, ep0_max_packet);
    print(out, "\n");

    /* --- размер контекста (CSZ, HCCPARAMS1 бит 2) --- */

    UINT32 hccparams1 = mmio_read32(xmmio + 0x10);
    BOOLEAN csz64 = (hccparams1 & 0x4u) != 0;
    UINT32 ctx_size = csz64 ? 64u : 32u;

    print(out, "Context size = ");
    print_uint(out, ctx_size);
    print(out, " bytes per context (CSZ=");
    print_uint(out, csz64 ? 1 : 0);
    print(out, ")\n");

    /* --- 2/3/4. Input Context, Device Context и Transfer Ring
       под Endpoint 0 - страницы уже выделены вызывающим кодом
       (до ExitBootServices), тут только обнуляем их --- */

    raw_zero_mem((volatile UINT8 *)(UINTN)input_ctx_phys, 4096);
    raw_zero_mem((volatile UINT8 *)(UINTN)dev_ctx_phys, 4096);
    raw_zero_mem((volatile UINT8 *)(UINTN)ep0_ring_phys, 4096);
    raw_zero_mem((volatile UINT8 *)(UINTN)desc_buf_phys, 4096);

    /* --- заполняем Input Control Context (первый контекст в
       Input Context): Add Context Flags A0 (Slot) и A1 (EP0) --- */

    volatile UINT32 *input_ctrl =
        (volatile UINT32 *)(UINTN)input_ctx_phys;

    input_ctrl[0] = 0;      /* Drop Context flags - ничего не
                                убираем, устройство новое */
    input_ctrl[1] = 0x3u;   /* Add Context flags: бит0=Slot
                                Context, бит1=EP0 Context */

    /* --- Slot Context, второй контекст, смещение ctx_size --- */

    volatile UINT32 *slot_ctx =
        (volatile UINT32 *)
            (UINTN)(input_ctx_phys + ctx_size);

    /* DW0: Route String=0 (устройство напрямую в root hub,
       без промежуточных хабов), Speed (биты 20:23),
       Context Entries (биты 27:31) = 1 - валиден пока только
       EP0 */
    slot_ctx[0] =
        (port_speed << 20) | (1u << 27);

    /* DW1: Root Hub Port Number - биты [23:16] - номер
       физического порта root hub, куда воткнуто устройство
       (тот же номер, что печатала команда 'xhci' при дампе
       PORTSC и что использовался для Port Reset) */
    slot_ctx[1] = (UINT32)root_port << 16;

    /* DW2/DW3 пока 0 - Interrupter Target=0 (используем
       Interrupter 0, тот же, что уже настроен), USB Device
       Address=0 (ещё не назначен - назначит сам контроллер по
       Address Device Command), Slot State=0 (Disabled/Enabled,
       контроллер сам выставит после команды) */
    slot_ctx[2] = 0;
    slot_ctx[3] = 0;

    /* --- Endpoint 0 Context, третий контекст, смещение
       2*ctx_size --- */

    volatile UINT32 *ep0_ctx =
        (volatile UINT32 *)
            (UINTN)(input_ctx_phys + 2u * ctx_size);

    ep0_ctx[0] = 0; /* Mult/MaxPStreams/LSA/Interval - все 0
                        для control endpoint */

    /* DW1: CErr (Error Count, биты 1:2) = 3 - максимум,
       контроллер сам остановит endpoint после 3 подряд
       неудачных попыток; EP Type (биты 3:5) = 4 = Control;
       Max Packet Size (биты 16:31) */
    ep0_ctx[1] =
        (3u << 1) | (4u << 3) |
        ((UINT32)ep0_max_packet << 16);

    /* DW2/DW3: TR Dequeue Pointer (64-бит, 16-байтное
       выравнивание гарантировано страницей) + Dequeue Cycle
       State=1 (бит0 DW2) - начальное состояние кольца EP0,
       кольцо только что создано, Cycle Bit контроллера должен
       совпадать с тем, что мы пишем в наши TRB (тоже 1, см.
       ниже) */
    ep0_ctx[2] =
        (UINT32)(ep0_ring_phys & 0xFFFFFFFFu) | 0x1u;
    ep0_ctx[3] = (UINT32)(ep0_ring_phys >> 32);

    /* DW4: Average TRB Length (биты 0:15) - не критично для
       простого случая, пишем разумное ненулевое значение (8,
       размер Setup-пакета) - по спеке 0 недопустим */
    ep0_ctx[4] = 8;

    print(
        out,
        "Input Context / Device Context / EP0 Transfer "
        "Ring allocated and filled.\n"
    );

    /* --- DCBAA[SlotID] = Device Context (не Input Context!
       в DCBAA идёт "чистый" Device Context, который контроллер
       будет сам обновлять; Input Context используется только
       один раз, как параметр самой команды Address Device) --- */

    volatile UINT64 *dcbaa =
        (volatile UINT64 *)(UINTN)dcbaa_phys;

    dcbaa[slot_id] = dev_ctx_phys;

    /* --- 5. Address Device Command (TRB Type 11) во второй
       слот Command Ring - первый (offset 0) уже занят
       Enable Slot Command, отправленной раньше --- */

    volatile UINT32 *cmd_trb2 =
        (volatile UINT32 *)
            (UINTN)(cmdring_phys + 16);

    cmd_trb2[0] =
        (UINT32)(input_ctx_phys & 0xFFFFFFFFu);
    cmd_trb2[1] =
        (UINT32)(input_ctx_phys >> 32);
    cmd_trb2[2] = 0;
    /* DW3: SlotID (биты 24:31), TRB Type=11 (биты 10:15),
       BSR=0 (бит 9, значит контроллер сам пошлёт SET_ADDRESS
       устройству - не просим только "заблокировать" адрес),
       Cycle=1 (бит0) - тот же Producer Cycle State, что и у
       первой команды, кольцо ещё не переворачивалось */
    cmd_trb2[3] =
        ((UINT32)slot_id << 24) | (11u << 10) | 0x1u;

    /* Doorbell 0, Target=0 - опять Command Ring (это тот же
       "звонок", что и для Enable Slot - Command Ring общий на
       весь контроллер, не per-device) */
    mmio_write32(xmmio + cap->DbOff, 0);

    UINTN ev_slot = start_ev_slot;

    volatile UINT32 *ev_trb2 = NULL;

    BOOLEAN got_event2 =
        xhci_wait_for_event(
            out, evring_phys, intr0,
            &ev_slot, 33, &ev_trb2
        );

    if (!got_event2) {

        print(
            out,
            "No Command Completion Event for Address "
            "Device - stopping here.\n"
        );
        return;
    }

    UINT8 trb_type2 =
        (UINT8)((ev_trb2[3] >> 10) & 0x3Fu);
    UINT8 compl_code2 =
        (UINT8)((ev_trb2[2] >> 24) & 0xFFu);

    print(out, "Address Device event: TRB Type=");
    print_uint(out, trb_type2);
    print(out, " CompletionCode=");
    print_uint(out, compl_code2);
    print(out, " (1=Success)\n");

    /* подтверждаем событие - сдвигаем ERDP на следующий TRB
       и продвигаем ev_slot, чтобы GET_DESCRIPTOR ниже начал
       опрос с правильного места */
    ev_slot = ev_slot + 1;

    UINT64 addr_dev_next_ev =
        xhci_event_ring_trb_addr(evring_phys, ev_slot);

    mmio_write32(
        intr0 + 0x18,
        (UINT32)(addr_dev_next_ev & 0xFFFFFFFFu)
    );
    mmio_write32(
        intr0 + 0x1C,
        (UINT32)(addr_dev_next_ev >> 32)
    );

    if (!(trb_type2 == 33 && compl_code2 == 1)) {

        print(
            out,
            "Address Device did not succeed - not "
            "attempting GET_DESCRIPTOR.\n"
        );
        return;
    }

    print(
        out,
        "\nAddress Device succeeded - device now has a "
        "real USB address. Sending GET_DESCRIPTOR "
        "(Device) over the Default Control Pipe...\n"
    );

    /* --- 6. GET_DESCRIPTOR(Device) - первый control transfer,
       через универсальную xhci_control_transfer() --- */

    UINTN trb_slot = 0; /* текущий свободный TRB на EP0 Transfer
                            Ring - у этой функции кольцо только
                            что создано, начинаем с нуля */

    UINT8 compl_code3 = 0;

    BOOLEAN got_event3 =
        xhci_control_transfer(
            out, xmmio, cap,
            evring_phys, intr0,
            ep0_ring_phys, &trb_slot,
            slot_id,
            0x80u,   /* bmRequestType: Device-to-Host/
                        Standard/Device */
            0x06u,   /* bRequest: GET_DESCRIPTOR */
            0x0100u, /* wValue: тип=1 (Device), индекс=0 */
            0,       /* wIndex */
            18,      /* wLength - весь Device Descriptor */
            desc_buf_phys,
            &ev_slot,
            &compl_code3
        );

    if (!got_event3) {

        print(
            out,
            "No Transfer Event for GET_DESCRIPTOR - "
            "stopping here.\n"
        );
        return;
    }

    print(
        out,
        "GET_DESCRIPTOR event: CompletionCode="
    );
    print_uint(out, compl_code3);
    print(out, " (1=Success, 13=ShortPacket)\n");

    if (!(compl_code3 == 1 || compl_code3 == 13)) {

        print(
            out,
            "GET_DESCRIPTOR did not complete "
            "successfully.\n"
        );
        return;
    }

    /* --- 7. печатаем Device Descriptor --- */

    volatile UINT8 *d =
        (volatile UINT8 *)(UINTN)desc_buf_phys;

    UINT16 id_vendor =
        (UINT16)d[8] | ((UINT16)d[9] << 8);
    UINT16 id_product =
        (UINT16)d[10] | ((UINT16)d[11] << 8);

    print(
        out,
        "\n=== Device Descriptor (real data from the "
        "USB device, no firmware involved) ===\n"
    );

    print(out, "bLength            = ");
    print_uint(out, d[0]);
    print(out, "\nbDescriptorType    = ");
    print_uint(out, d[1]);
    print(out, "  (1=Device)\nbcdUSB             = 0x");
    print_hex(
        out,
        (UINT32)d[2] | ((UINT32)d[3] << 8),
        4
    );
    print(out, "\nbDeviceClass       = ");
    print_uint(out, d[4]);
    print(out, "\nbDeviceSubClass    = ");
    print_uint(out, d[5]);
    print(out, "\nbDeviceProtocol    = ");
    print_uint(out, d[6]);
    print(out, "\nbMaxPacketSize0    = ");
    print_uint(out, d[7]);
    print(out, "\nidVendor           = 0x");
    print_hex(out, id_vendor, 4);
    print(out, "\nidProduct          = 0x");
    print_hex(out, id_product, 4);
    print(out, "\nbcdDevice          = 0x");
    print_hex(
        out,
        (UINT32)d[12] | ((UINT32)d[13] << 8),
        4
    );
    print(out, "\nbNumConfigurations = ");
    print_uint(out, d[17]);
    print(
        out,
        "\n\nGot it - real Vendor ID / Product ID read "
        "straight from the USB device over our own "
        "xHCI driver.\n"
    );

    /*
     * --- Шаг 7 — Configuration Descriptor + поиск HID
     * Interrupt IN endpoint + SET_CONFIGURATION ---
     *
     * Configuration Descriptor устроен "матрёшкой": сам
     * GET_DESCRIPTOR(Configuration) возвращает не один
     * дескриптор, а сразу целую пачку подряд - сначала сам
     * Configuration Descriptor (9 байт, в т.ч. wTotalLength -
     * сколько байт всего вернулось и bNumInterfaces), потом
     * для каждого интерфейса - Interface Descriptor (9 байт,
     * в т.ч. bInterfaceClass - у мыши это 3 = HID) и следом
     * его Endpoint Descriptor'ы (7 байт каждый). Нас интересует
     * конкретно Interrupt IN endpoint внутри HID-интерфейса -
     * именно на него мышь будет сама, без опроса, присылать
     * пакеты с dx/dy/кнопками, когда мы его настроим (это уже
     * следующий шаг, Configure Endpoint Command - тут мы его
     * только находим и запоминаем адрес/MaxPacketSize/Interval).
     *
     * Буфер под ответ - та же страница, что и Device Descriptor
     * (desc_buf_phys), но со сдвигом +256 байт, чтобы не
     * затирать уже прочитанные 18 байт (страница у нас 4096
     * байт, с большим запасом на оба буфера).
     */

    UINT64 cfg_buf_phys = desc_buf_phys + 256;
    UINTN  cfg_buf_len  = 512; /* с запасом - у мыши обычно

                                   в разы меньше */

    UINT8 compl_code4 = 0;

    BOOLEAN got_event4 =
        xhci_control_transfer(
            out, xmmio, cap,
            evring_phys, intr0,
            ep0_ring_phys, &trb_slot,
            slot_id,
            0x80u,               /* Device-to-Host/Standard/
                                     Device */
            0x06u,               /* GET_DESCRIPTOR */
            0x0200u,             /* тип=2 (Configuration),
                                     индекс=0 */
            0,
            (UINT16)cfg_buf_len,
            cfg_buf_phys,
            &ev_slot,
            &compl_code4
        );

    if (!got_event4) {

        print(
            out,
            "\nNo Transfer Event for "
            "GET_DESCRIPTOR(Configuration) - stopping "
            "here.\n"
        );
        return;
    }

    print(
        out,
        "\nGET_DESCRIPTOR(Configuration) event: "
        "CompletionCode="
    );
    print_uint(out, compl_code4);
    print(out, " (1=Success, 13=ShortPacket)\n");

    if (!(compl_code4 == 1 || compl_code4 == 13)) {

        print(
            out,
            "GET_DESCRIPTOR(Configuration) did not "
            "complete successfully - stopping here.\n"
        );
        return;
    }

    volatile UINT8 *cfg =
        (volatile UINT8 *)(UINTN)cfg_buf_phys;

    UINT16 cfg_total_len =
        (UINT16)cfg[2] | ((UINT16)cfg[3] << 8);
    UINT8  cfg_num_interfaces = cfg[4];
    UINT8  cfg_value = cfg[5];

    print(out, "bNumInterfaces     = ");
    print_uint(out, cfg_num_interfaces);
    print(out, "\nbConfigurationValue= ");
    print_uint(out, cfg_value);
    print(out, "\nwTotalLength       = ");
    print_uint(out, cfg_total_len);
    print(out, "\n");

    /* устройство не обязано прислать больше, чем реально
       есть - на случай короткого пакета не читаем за пределы
       того, что реально уместилось в буфер */
    UINTN scan_len = cfg_total_len;

    if (scan_len > cfg_buf_len)
        scan_len = cfg_buf_len;

    UINTN off = 0;
    UINT8 cur_iface_class = 0xFFu;
    UINT8 cur_iface_num = 0xFFu;
    BOOLEAN found_ep = FALSE;
    UINT8  ep_addr = 0;
    UINT16 ep_maxpkt = 0;
    UINT8  ep_interval = 0;

    /* Найдено ли устройство HID Class Descriptor (тип 0x21) -
       он лежит внутри Configuration Descriptor сразу после
       Interface Descriptor HID-интерфейса и ДО его Endpoint
       Descriptor'ов, и это единственное место, откуда можно
       узнать реальную длину Report Descriptor'а (поле
       wDescriptorLength) и номер интерфейса, которому его
       адресовать (wIndex у GET_DESCRIPTOR(Report) - это номер
       интерфейса, не устройства) - см. следующий шаг ниже,
       отдельный control transfer за самим Report Descriptor'ом. */
    BOOLEAN found_hid_desc = FALSE;
    UINT8   hid_iface_num = 0;
    UINT16  hid_report_desc_len = 0;

    while (off + 2 <= scan_len) {

        UINT8 d_len = cfg[off];
        UINT8 d_type = cfg[off + 1];

        if (d_len == 0)
            break; /* защита от зацикливания на битом
                       дескрипторе */

        if (d_type == 4 && off + 9 <= scan_len) {

            /* Interface Descriptor: байт 2 - bInterfaceNumber,
               байт 5 - bInterfaceClass (3 = HID) */
            cur_iface_num = cfg[off + 2];
            cur_iface_class = cfg[off + 5];

        } else if (
            d_type == 0x21u && off + 9 <= scan_len &&
            cur_iface_class == 3 && !found_hid_desc
        ) {

            /* HID Class Descriptor (не путать с Report
               Descriptor - это отдельная маленькая "обёртка"
               внутри Configuration Descriptor, которая просто
               ОПИСЫВАЕТ Report Descriptor, но не содержит его):
               байты[2:3]=bcdHID, байт4=bCountryCode,
               байт5=bNumDescriptors (обычно 1 - один Report
               Descriptor на интерфейс), байт6=bDescriptorType
               следующего вложенного дескриптора (должно быть
               0x22 = Report), байты[7:8]=wDescriptorLength - его
               длина в байтах, ровно то число, которое нужно
               запросить через GET_DESCRIPTOR ниже. Берём только
               первый найденный (на случай нескольких HID-
               интерфейсов в составном устройстве, нас интересует
               именно тот, у которого чуть выше уже нашли
               Interrupt IN endpoint). */

            found_hid_desc = TRUE;
            hid_iface_num = cur_iface_num;
            hid_report_desc_len =
                (UINT16)cfg[off + 7] |
                ((UINT16)cfg[off + 8] << 8);

        } else if (
            d_type == 5 && off + 7 <= scan_len &&
            !found_ep
        ) {

            /* Endpoint Descriptor: байт2=bEndpointAddress
               (бит7=1 значит IN), байт3=bmAttributes (биты
               [1:0]=3 значит Interrupt), байты[4:5]=
               wMaxPacketSize, байт6=bInterval */
            UINT8 addr = cfg[off + 2];
            UINT8 attr = cfg[off + 3];

            if (
                cur_iface_class == 3 &&
                (addr & 0x80u) &&
                (attr & 0x3u) == 3u
            ) {

                found_ep = TRUE;
                ep_addr = addr;
                ep_maxpkt =
                    (UINT16)cfg[off + 4] |
                    ((UINT16)cfg[off + 5] << 8);
                ep_interval = cfg[off + 6];
            }
        }

        off = off + d_len;
    }

    if (!found_ep) {

        print(
            out,
            "\nNo HID Interrupt IN endpoint found in "
            "the Configuration Descriptor - this "
            "device may not be a plain HID mouse, or "
            "the descriptor set did not fully fit in "
            "the buffer.\n"
        );

    } else {

        print(out, "\nHID Interrupt IN endpoint found:\n");
        print(out, "  bEndpointAddress = 0x");
        print_hex(out, ep_addr, 2);
        print(out, "  (EP");
        print_uint(out, ep_addr & 0x0Fu);
        print(out, " IN)\n  wMaxPacketSize   = ");
        print_uint(out, ep_maxpkt);
        print(out, "\n  bInterval        = ");
        print_uint(out, ep_interval);
        print(
            out,
            " (polling interval, used below for the "
            "Configure Endpoint Command)\n"
        );
    }

    /* --- SET_CONFIGURATION - без стадии данных (wLength=0),
       переводит устройство из состояния Addressed в
       Configured. bmRequestType=0x00 (Host-to-Device/
       Standard/Device), bRequest=0x09 */

    print(out, "\nSending SET_CONFIGURATION(");
    print_uint(out, cfg_value);
    print(out, ")...\n");

    UINT8 compl_code5 = 0;

    BOOLEAN got_event5 =
        xhci_control_transfer(
            out, xmmio, cap,
            evring_phys, intr0,
            ep0_ring_phys, &trb_slot,
            slot_id,
            0x00u,
            0x09u,
            cfg_value,
            0,
            0,    /* wLength=0 - без Data Stage */
            0,    /* data_buf_phys не используется */
            &ev_slot,
            &compl_code5
        );

    if (!got_event5) {

        print(
            out,
            "No Transfer Event for SET_CONFIGURATION - "
            "stopping here.\n"
        );
        return;
    }

    print(out, "SET_CONFIGURATION event: CompletionCode=");
    print_uint(out, compl_code5);
    print(out, " (1=Success)\n");

    if (compl_code5 != 1) {

        return;
    }

    if (!found_ep) {

        print(
            out,
            "\nDevice is now Configured, but no HID "
            "Interrupt IN endpoint was found earlier - "
            "cannot poll for mouse reports.\n"
        );
        return;
    }

    /* --------------------------------------------------------
     * Шаг 7.5 — настоящий Report Descriptor (GET_DESCRIPTOR,
     * тип 0x22) вместо угаданного boot-protocol формата
     * --------------------------------------------------------
     *
     * bmRequestType=0x81: Device-to-Host (бит7=1) / Standard
     * (биты[6:5]=00 - это ещё обычный, не класс-специфичный
     * запрос, несмотря на то, что адресован он HID-дескриптору) /
     * Recipient=Interface (биты[4:0]=00001) - GET_DESCRIPTOR
     * Report ОБЯЗАН идти с Recipient=Interface, а не Device
     * (в отличие от Device/Configuration Descriptor выше), и
     * wIndex - это номер интерфейса (hid_iface_num, найденный
     * выше при разборе Configuration Descriptor), а НЕ 0.
     * wValue = (тип=0x22 << 8) | индекс=0 (первый, и почти
     * всегда единственный, Report Descriptor интерфейса).
     *
     * Длину берём из HID Class Descriptor'а (hid_report_desc_len,
     * см. выше) - если по какой-то причине он не нашёлся
     * (found_hid_desc == FALSE, дескриптор в кадр не влез или
     * был не там, где ожидали), подстраховываемся разумным
     * запасом в 256 байт - обычные мышиные Report Descriptor'ы
     * укладываются в 30-80 байт, этого с большим запасом хватит,
     * а лишние незаполненные байты GET_DESCRIPTOR просто не
     * пришлёт (Short Packet, CompletionCode=13 - тоже
     * обрабатываем как успех, как и везде выше).
     *
     * Буфер - та же страница, что Device/Configuration
     * Descriptor и сам буфер под отчёты (desc_buf_phys), сдвиг
     * +2048 - после Device Descriptor (0..18), Configuration
     * Descriptor (256..768) и буфера под сами отчёты
     * (1024..1088) остаётся больше 3000 байт свободного места в
     * странице, +2048 - с запасом от всех них.
     */

    UINT16 report_desc_len =
        found_hid_desc ? hid_report_desc_len : 256u;

    if (report_desc_len > 2048u)
        report_desc_len = 2048u; /* защита - не должно случаться
                                     у обычной мыши, но не
                                     позволяем запросу вылезти за
                                     пределы страницы */

    UINT64 report_desc_buf_phys = desc_buf_phys + 2048u;

    raw_zero_mem(
        (volatile UINT8 *)(UINTN)report_desc_buf_phys, 2048
    );

    print(out, "\nRequesting HID Report Descriptor (");
    print_uint(out, report_desc_len);
    print(out, " bytes, interface ");
    print_uint(out, hid_iface_num);
    print(out, ")...\n");

    UINT8 compl_code_rd = 0;

    BOOLEAN got_event_rd =
        xhci_control_transfer(
            out, xmmio, cap,
            evring_phys, intr0,
            ep0_ring_phys, &trb_slot,
            slot_id,
            0x81u,               /* Device-to-Host/Standard/
                                     Interface */
            0x06u,               /* GET_DESCRIPTOR */
            (UINT16)(0x2200u),   /* тип=0x22 (Report), индекс=0 */
            hid_iface_num,
            report_desc_len,
            report_desc_buf_phys,
            &ev_slot,
            &compl_code_rd
        );

    HID_MOUSE_REPORT_LAYOUT mouse_layout;

    /* если хоть что-то пошло не так - mouse_layout.valid
       останется FALSE (см. явную инициализацию ниже), и цикл
       опроса ниже сам заметит это и вернётся к старому
       угаданному byte0/1/2 формату, ничего специально
       обрабатывать тут не нужно */
    mouse_layout.valid = FALSE;
    mouse_layout.has_buttons = FALSE;
    mouse_layout.x_bit_size = 0;
    mouse_layout.y_bit_size = 0;
    mouse_layout.has_wheel = FALSE;
    mouse_layout.has_report_id = FALSE;

    if (!got_event_rd) {

        print(
            out,
            "No Transfer Event for GET_DESCRIPTOR(Report) - "
            "keeping the old guessed byte0/1/2 layout.\n"
        );

    } else {

        print(out, "GET_DESCRIPTOR(Report) event: "
                    "CompletionCode=");
        print_uint(out, compl_code_rd);
        print(out, " (1=Success, 13=ShortPacket)\n");

        if (compl_code_rd == 1u || compl_code_rd == 13u) {

            hid_parse_report_descriptor(
                out,
                (volatile UINT8 *)(UINTN)report_desc_buf_phys,
                report_desc_len,
                &mouse_layout
            );

        } else {

            print(
                out,
                "GET_DESCRIPTOR(Report) did not complete "
                "successfully - keeping the old guessed "
                "byte0/1/2 layout.\n"
            );
        }
    }

    /* --------------------------------------------------------
     * Шаг 8 — Configure Endpoint Command для Interrupt IN
     * endpoint + опрос реальных отчётов мыши (dX/dY/кнопки)
     * --------------------------------------------------------
     *
     * Устройство сейчас в состоянии Configured, но реально
     * работает пока только Endpoint 0 (Default Control Pipe) -
     * контроллер ничего не знает про Interrupt IN endpoint,
     * найденный выше в Configuration Descriptor, пока мы явно
     * не опишем его в Input Context и не пошлём Configure
     * Endpoint Command (TRB Type 12) - тот же механизм, что и
     * Address Device (Type 11) выше, только теперь добавляем
     * контекст ОДНОЙ конкретной конечной точки, а не Endpoint 0.
     *
     * Input Context здесь переиспользуется (та же страница
     * input_ctx_phys, что была под Address Device) - она нужна
     * только на момент самой команды, контроллер её не хранит
     * после обработки, поэтому спокойно перезаписываем.
     */

    print(
        out,
        "\nDevice is now Configured. Setting up the "
        "Interrupt IN endpoint (Configure Endpoint "
        "Command)...\n"
    );

    /* Device Context Index (DCI): по спеке xHCI конечные точки
       нумеруются в Device Context не по bEndpointAddress
       напрямую, а как DCI = 2*(номер endpoint) + направление
       (0=OUT, 1=IN). Endpoint 0 всегда DCI=1 (уже занят, EP0
       Context выше). Для нашего Interrupt IN endpoint номер N -
       DCI = 2*N + 1. */

    UINT8 ep_num = ep_addr & 0x0Fu;
    UINT8 dci = (UINT8)(ep_num * 2u + 1u);

    print(out, "Endpoint number = ");
    print_uint(out, ep_num);
    print(out, ", Device Context Index (DCI) = ");
    print_uint(out, dci);
    print(out, "\n");

    /* Interval: xHCI хранит период опроса как степень двойки в
       единицах 125мкс (реальный период = 2^Interval * 125мкс).
       USB-дескриптор хранит bInterval по-разному в зависимости
       от скорости порта:
       - High/SuperSpeed: bInterval УЖЕ показатель степени
         (1..16), период = 2^(bInterval-1) * 125мкс, поэтому
         Interval = bInterval - 1.
       - Low/Full Speed: bInterval - это число целых кадров по
         1мс (1..255) буквально. xHCI всё равно не умеет хранить
         произвольный период, только степень двойки от 125мкс,
         поэтому берём ближайшую снизу степень двойки от
         (bInterval мс, переведённых в те же единицы 125мкс,
         т.е. bInterval*8) через floor(log2(bInterval))+3 - это
         тот же приближённый пересчёт, что использует и Linux
         в своём xHCI-драйвере. */

    UINT8 interval_field;

    if (port_speed == 3 || port_speed == 4) {

        interval_field =
            (ep_interval >= 1) ? (UINT8)(ep_interval - 1) : 0;

    } else {

        UINT8 v = (ep_interval == 0) ? 1 : ep_interval;
        UINT8 log2_val = 0;

        while ((v >> 1) != 0) {
            v = (UINT8)(v >> 1);
            log2_val = (UINT8)(log2_val + 1);
        }

        interval_field = (UINT8)(log2_val + 3);
    }

    if (interval_field > 15)
        interval_field = 15;

    print(out, "xHCI Interval field = ");
    print_uint(out, interval_field);
    print(out, " (period = 2^");
    print_uint(out, interval_field);
    print(out, " * 125us)\n");

    /* --- Transfer Ring для этой конечной точки + буфер под
       отчёт (переиспользуем ту же страницу, что и под
       Device/Configuration Descriptor - там ещё много свободного
       места после байта 768). int_ring_phys теперь тоже приходит
       уже выделенным аргументом функции (см. комментарий выше про
       AllocatePages) - тут только обнуляем. --- */

    raw_zero_mem(
        (volatile UINT8 *)(UINTN)int_ring_phys, 4096
    );

    /*
     * Link TRB (TRB Type=6) в последнем слоте страницы (слот 255
     * из 256 - страница 4096 байт, TRB по 16 байт).
     *
     * ЭТО ТА ЖЕ САМАЯ ПРОБЛЕМА, что мы только что нашли и
     * починили на Event Ring (см. xhci_event_ring_trb_addr выше
     * по файлу) - только здесь наоборот: не мы читаем события
     * контроллера, а контроллер читает TRB, которые пишем МЫ
     * (Transfer Ring для Interrupt IN endpoint'а). Опрос отчётов
     * мыши раньше писал TRB по адресу int_ring_phys +
     * int_trb_slot*16, где int_trb_slot рос без остановки - то
     * есть ровно так же "уезжал" за пределы выделенной под кольцо
     * страницы после 256 отчётов, и мышь (точнее, xHC) переставала
     * присылать Transfer Event вообще - отчёты просто "кончались"
     * без единого сообщения об ошибке.
     *
     * Правильное решение по спеке xHCI - НЕ "остаток от деления"
     * (это работало для Event Ring, потому что там читаем МЫ),
     * а настоящий Link TRB: последний слот страницы навсегда
     * отдаётся под TRB Type=6, который указывает контроллеру
     * "дальше кольцо продолжается по адресу int_ring_phys" (то
     * есть с начала той же страницы) и содержит бит Toggle Cycle
     * (бит1) - это указание контроллеру, что при переходе по
     * этой ссылке нужно перевернуть его внутреннее понимание
     * Cycle Bit (ровно как и Event Ring, кольцо "разворачивается"
     * начиная со второго круга - только тут разворот явно
     * прописан TRB-указателем, а не подразумевается остатком от
     * деления). Cycle этого TRB тоже нужно обновлять на каждом
     * круге (см. xhci_xfer_ring_pcs ниже) - обновляем его сразу,
     * как только начинаем писать первый Normal TRB нового круга,
     * чтобы контроллер успел увидеть верное значение к тому
     * моменту, как дойдёт до этого слота.
     */
    volatile UINT32 *int_ring_link_trb =
        (volatile UINT32 *)
            (UINTN)(int_ring_phys +
                    (UINT64)(XHCI_XFER_RING_TRBS - 1u) * 16u);

    int_ring_link_trb[0] =
        (UINT32)(int_ring_phys & 0xFFFFFFFFu);
    int_ring_link_trb[1] =
        (UINT32)(int_ring_phys >> 32);
    int_ring_link_trb[2] = 0;
    /* DW3: Cycle=1 (для самого первого круга, совпадает с
       DCS=1, который мы пропишем в Endpoint Context ниже),
       Toggle Cycle (бит1), TRB Type=6 (Link, биты 10:15) */
    int_ring_link_trb[3] =
        0x1u | (1u << 1) | (6u << 10);

    UINT64 report_buf_phys = desc_buf_phys + 1024;

    raw_zero_mem(
        (volatile UINT8 *)(UINTN)report_buf_phys, 64
    );

    /* --- перезаписываем Input Context под Configure Endpoint
       Command: Add Context Flags A0 (Slot, обязательно всегда)
       и A(dci) (наша конечная точка). Slot Context копируем с
       теми же Route String/Speed/Root Port, что и раньше, но
       Context Entries теперь = dci (самый старший используемый
       индекс контекста, а не 1) - иначе контроллер не будет
       знать, что контекст endpoint'а вообще валиден. --- */

    raw_zero_mem(
        (volatile UINT8 *)(UINTN)input_ctx_phys, 4096
    );

    volatile UINT32 *input_ctrl2 =
        (volatile UINT32 *)(UINTN)input_ctx_phys;

    input_ctrl2[0] = 0;
    input_ctrl2[1] = 0x1u | (1u << dci);

    volatile UINT32 *slot_ctx2 =
        (volatile UINT32 *)
            (UINTN)(input_ctx_phys + ctx_size);

    slot_ctx2[0] =
        (port_speed << 20) | ((UINT32)dci << 27);
    slot_ctx2[1] = (UINT32)root_port << 16;
    slot_ctx2[2] = 0;
    slot_ctx2[3] = 0;

    volatile UINT32 *int_ep_ctx =
        (volatile UINT32 *)
            (UINTN)(input_ctx_phys +
                    (UINT64)(dci + 1u) * ctx_size);

    /* DW0: Interval - биты 16:23 (НЕ 8:15! Раньше тут была
       ошибка - Interval попадал в биты Mult/MaxPStreams/LSA,
       а для Interrupt-конечной точки это невалидные поля,
       контроллер мог принять команду (CompletionCode=1), но
       дальше просто не планировать реальный опрос устройства -
       см. историю отладки: с этим багом Configure Endpoint
       "успешно" завершался, но ни одного отчёта с мыши так и
       не приходило). Mult/MaxPStreams(биты 8:14)/LSA(бит15) = 0
       - обычная одиночная конечная точка без streams. */
    int_ep_ctx[0] = (UINT32)interval_field << 16;

    /* DW1: CErr=3, EP Type=7 (Interrupt IN), Max Burst Size=0,
       Max Packet Size (биты 16:31) */
    int_ep_ctx[1] =
        (3u << 1) | (7u << 3) |
        ((UINT32)ep_maxpkt << 16);

    /* DW2/DW3: TR Dequeue Pointer + DCS=1 - кольцо только что
       создано */
    int_ep_ctx[2] =
        (UINT32)(int_ring_phys & 0xFFFFFFFFu) | 0x1u;
    int_ep_ctx[3] = (UINT32)(int_ring_phys >> 32);

    /* DW4: Average TRB Length - размер отчёта (ep_maxpkt) - по
       спеке не должен быть 0; Max ESIT Payload Lo оставляем 0
       (не критично для простого прерывания без Streams/SS) */
    int_ep_ctx[4] = ep_maxpkt;

    /* --- Configure Endpoint Command (TRB Type 12) - третий
       слот Command Ring (offset 32, после Enable Slot и
       Address Device) --- */

    volatile UINT32 *cmd_trb3 =
        (volatile UINT32 *)
            (UINTN)(cmdring_phys + 32);

    cmd_trb3[0] =
        (UINT32)(input_ctx_phys & 0xFFFFFFFFu);
    cmd_trb3[1] =
        (UINT32)(input_ctx_phys >> 32);
    cmd_trb3[2] = 0;
    cmd_trb3[3] =
        ((UINT32)slot_id << 24) | (12u << 10) | 0x1u;

    mmio_write32(xmmio + cap->DbOff, 0);

    volatile UINT32 *ev_trb3 = NULL;

    BOOLEAN got_event6 =
        xhci_wait_for_event(
            out, evring_phys, intr0,
            &ev_slot, 33, &ev_trb3
        );

    if (!got_event6) {

        print(
            out,
            "No Command Completion Event for Configure "
            "Endpoint - stopping here.\n"
        );
        return;
    }

    UINT8 compl_code6 =
        (UINT8)((ev_trb3[2] >> 24) & 0xFFu);

    print(out, "Configure Endpoint event: CompletionCode=");
    print_uint(out, compl_code6);
    print(out, " (1=Success)\n");

    ev_slot = ev_slot + 1;

    UINT64 cfgep_next_ev =
        xhci_event_ring_trb_addr(evring_phys, ev_slot);

    mmio_write32(
        intr0 + 0x18,
        (UINT32)(cfgep_next_ev & 0xFFFFFFFFu)
    );
    mmio_write32(
        intr0 + 0x1C,
        (UINT32)(cfgep_next_ev >> 32)
    );

    if (compl_code6 != 1) {

        print(
            out,
            "Configure Endpoint did not succeed - not "
            "polling for reports.\n"
        );
        return;
    }

    print(
        out,
        "\nInterrupt IN endpoint configured. Polling "
        "for real mouse reports now - move the mouse.\n"
    );

    /* --------------------------------------------------------
     * Опрос отчётов: кладём один Normal TRB (буфер под отчёт)
     * на Transfer Ring endpoint'а, звоним в его Doorbell
     * (Target=dci, НЕ 1 - это отдельная конечная точка, не
     * Endpoint 0), и ждём Transfer Event.
     *
     * РАНЬШЕ здесь между короткими порциями ожидания опрашивалась
     * клавиатура (ConIn->ReadKeyStroke) неблокирующим образом,
     * чтобы можно было прервать опрос нажатием клавиши. Эта
     * функция теперь вызывается уже ПОСЛЕ ExitBootServices (см.
     * команду "ebs") - клавиатуры (как и всех остальных Boot
     * Services) больше гарантированно нет, поэтому "выход по
     * клавише" заменён на выход по ограничению числа отчётов
     * (XHCI_POLL_MAX_REPORTS) - опрос идёт заданное число раз и
     * останавливается сам, без участия пользователя.
     * -------------------------------------------------------- */

    UINTN int_trb_slot = 0;
    UINTN reports_seen = 0;

    for (;;) {

        if (reports_seen >= XHCI_POLL_MAX_REPORTS)
            break;

        /* Обновляем Cycle Bit в Link TRB на конец страницы -
           НО не в момент записи первого TRB нового круга (как
           было раньше - и это была ошибка), а на ВТОРОМ TRB
           нового круга.
           Почему: получив Transfer Event для первого TRB нового
           круга (int_trb_slot % USABLE == 0), мы точно знаем,
           что контроллер уже реально прошёл через Link TRB со
           СТАРЫМ значением Cycle Bit (иначе он не смог бы
           признать валидным TRB, который лежит сразу после
           Link TRB, и событие бы не пришло) - то есть именно
           сейчас, при записи второго TRB, обновлять Link TRB уже
           безопасно, старое значение ему больше не понадобится
           вплоть до конца ЭТОГО круга (ещё 253 TRB впереди).
           Если обновлять на первом TRB (как было) - есть гонка:
           контроллер может проверять Link TRB "лениво", только
           когда реально пытается дойти до следующего TRB после
           него, и наша слишком ранняя перезапись подменяет
           значение прямо в момент, когда он его ожидает старым -
           он видит "невалидный" Link TRB и зависает навсегда
           (ровно то, что и случилось на отчёте #256). */

        if (
            (int_trb_slot % (UINTN)XHCI_XFER_RING_USABLE_TRBS)
                == 1
        ) {

            volatile UINT32 *link_upd =
                (volatile UINT32 *)
                    (UINTN)(int_ring_phys +
                            (UINT64)(XHCI_XFER_RING_TRBS - 1u) *
                                16u);

            UINT32 link_cycle =
                xhci_xfer_ring_pcs(int_trb_slot);

            link_upd[3] =
                link_cycle | (1u << 1) | (6u << 10);
        }

        UINT32 xfer_cycle =
            xhci_xfer_ring_pcs(int_trb_slot);

        volatile UINT32 *xfer_trb =
            (volatile UINT32 *)(UINTN)
                xhci_xfer_ring_trb_addr(
                    int_ring_phys, int_trb_slot
                );

        xfer_trb[0] =
            (UINT32)(report_buf_phys & 0xFFFFFFFFu);
        xfer_trb[1] =
            (UINT32)(report_buf_phys >> 32);
        /* DW2: TRB Transfer Length (биты 0:16) - сколько байт
           готовы принять */
        xfer_trb[2] = (UINT32)ep_maxpkt;
        /* DW3: Cycle - см. xhci_xfer_ring_pcs (переворачивается
           каждый круг кольца, иначе контроллер после первого же
           оборота решит, что новых TRB больше нет), IOC (бит5),
           TRB Type=1 (Normal, биты 10:15) */
        xfer_trb[3] =
            xfer_cycle | (1u << 5) | (1u << 10);

        /* Doorbell SlotID, Target=dci - у каждой конечной точки
           свой Target в её собственном doorbell-регистре, не
           путать с Target=1 у Endpoint 0 (control transfer'ы
           выше) и Target=0 (Command Ring, отдельный doorbell
           0) */
        mmio_write32(
            xmmio + cap->DbOff + (UINT64)slot_id * 4, dci
        );

        volatile UINT32 *ev_trb4 =
            (volatile UINT32 *)(UINTN)
                xhci_event_ring_trb_addr(evring_phys, ev_slot);

        UINT32 expected_cycle4 =
            xhci_event_ring_expected_cycle(ev_slot);

        BOOLEAN got_report = FALSE;

        for (
            UINTN attempt = 0;
            attempt < XHCI_POLL_MAX_IDLE_ATTEMPTS &&
                !got_report;
            attempt++
        ) {

            for (UINTN i = 0; i < 50; i++) {

                /* Раньше - BootServices->Stall(1000).
                   Firmware-независимая задержка, см.
                   busy_wait_ms() выше по файлу. */
                busy_wait_ms(1);

                if ((ev_trb4[3] & 0x1u) == expected_cycle4) {
                    got_report = TRUE;
                    break;
                }
            }
        }

        if (!got_report) {

            /* мышь просто молчала это окно времени - не ошибка,
               тот же TRB ещё не потреблён контроллером (раз
               события не было), идём на следующую итерацию и
               пробуем ещё */
            continue;
        }

        UINT8 t4 = (UINT8)((ev_trb4[3] >> 10) & 0x3Fu);

        if (t4 != 32) {

            /* постороннее событие - подтверждаем (сдвигаем
               ERDP) и продолжаем опрос тем же TRB */
            print(
                out,
                "  (skipping unrelated event, TRB Type="
            );
            print_uint(out, t4);
            print(out, ")\n");

            ev_slot = ev_slot + 1;

            UINT64 skip_addr =
                xhci_event_ring_trb_addr(evring_phys, ev_slot);

            mmio_write32(
                intr0 + 0x18,
                (UINT32)(skip_addr & 0xFFFFFFFFu)
            );
            mmio_write32(
                intr0 + 0x1C,
                (UINT32)(skip_addr >> 32)
            );

            continue;
        }

        UINT8 compl_code7 =
            (UINT8)((ev_trb4[2] >> 24) & 0xFFu);

        ev_slot = ev_slot + 1;

        UINT64 rep_next_ev =
            xhci_event_ring_trb_addr(evring_phys, ev_slot);

        mmio_write32(
            intr0 + 0x18,
            (UINT32)(rep_next_ev & 0xFFFFFFFFu)
        );
        mmio_write32(
            intr0 + 0x1C,
            (UINT32)(rep_next_ev >> 32)
        );

        if (!(compl_code7 == 1 || compl_code7 == 13)) {

            print(
                out,
                "  (transfer event CompletionCode="
            );
            print_uint(out, compl_code7);
            print(out, " - skipping this report)\n");

            int_trb_slot = int_trb_slot + 1;
            continue;
        }

        reports_seen = reports_seen + 1;

        volatile UINT8 *rep =
            (volatile UINT8 *)(UINTN)report_buf_phys;

        print(out, "Report #");
        print_uint(out, reports_seen);
        print(out, ": ");

        if (mouse_layout.valid) {

            /* --- настоящий формат, разобранный из Report
               Descriptor этого конкретного устройства (см.
               hid_parse_report_descriptor выше) - никаких
               предположений про byte0/1/2 тут больше нет,
               каждое поле читается ровно с того бита и той
               ширины, которые прислало само устройство. --- */

            UINT32 buttons = 0;

            if (mouse_layout.has_buttons) {

                buttons =
                    hid_extract_bits(
                        rep, ep_maxpkt,
                        mouse_layout.button_bit_offset,
                        mouse_layout.button_count
                    );
            }

            UINT32 x_raw =
                hid_extract_bits(
                    rep, ep_maxpkt,
                    mouse_layout.x_bit_offset,
                    mouse_layout.x_bit_size
                );
            UINT32 y_raw =
                hid_extract_bits(
                    rep, ep_maxpkt,
                    mouse_layout.y_bit_offset,
                    mouse_layout.y_bit_size
                );

            /* знак имеет смысл только для относительных полей -
               абсолютные координаты (планшет/тачпад) печатаем
               как есть, без знакового расширения */
            INT32 dx =
                mouse_layout.x_is_relative ?
                    hid_sign_extend(
                        x_raw, mouse_layout.x_bit_size
                    ) :
                    (INT32)x_raw;
            INT32 dy =
                mouse_layout.y_is_relative ?
                    hid_sign_extend(
                        y_raw, mouse_layout.y_bit_size
                    ) :
                    (INT32)y_raw;

            print(out, "buttons=0x");
            print_hex(out, buttons, 2);
            print(out, "  dX=");
            print_int(out, dx);
            print(out, "  dY=");
            print_int(out, dy);

            if (mouse_layout.has_wheel) {

                UINT32 wheel_raw =
                    hid_extract_bits(
                        rep, ep_maxpkt,
                        mouse_layout.wheel_bit_offset,
                        mouse_layout.wheel_bit_size
                    );
                INT32 wheel =
                    mouse_layout.wheel_is_relative ?
                        hid_sign_extend(
                            wheel_raw,
                            mouse_layout.wheel_bit_size
                        ) :
                        (INT32)wheel_raw;

                print(out, "  wheel=");
                print_int(out, wheel);
            }

            print(out, "\n");

        } else {

            /* --- Report Descriptor не прочитался или не
               распознался (см. предупреждение чуть выше по
               логу) - тот же угаданный "boot protocol" формат,
               что использовался раньше: байт0 - битовая маска
               кнопок, байт1 - dX со знаком, байт2 - dY со
               знаком. Оставлен как подстраховка, а не основной
               путь. --- */

            UINT8 buttons = rep[0];
            INT8  dx = (INT8)rep[1];
            INT8  dy = (INT8)rep[2];

            print(out, "(fallback layout) buttons=0x");
            print_hex(out, buttons, 2);
            print(out, "  dX=");
            print_int(out, dx);
            print(out, "  dY=");
            print_int(out, dy);
            print(out, "\n");
        }

        int_trb_slot = int_trb_slot + 1;
    }

    print(out, "\nStopped polling after ");
    print_uint(out, reports_seen);
    print(out, " report(s).\n");
}


/*
 * Весь путь от уже включённого/отсоединённого от прошивки
 * контроллера (шаги 0-3, см. команду "xhci") до реального опроса
 * отчётов мыши: сброс, DCBAA + Command Ring + Event Ring, запуск,
 * сканирование портов, Port Reset, Enable Slot, Address Device +
 * GET_DESCRIPTOR + SET_CONFIGURATION + опрос.
 *
 * Эта функция вызывается командой "ebs" (см. ниже) ЦЕЛИКОМ ПОСЛЕ
 * ExitBootServices - "out" здесь уже не настоящий st->ConOut, а
 * наш собственный пиксельный "терминал" (см. ebs_console_start
 * выше), а все физические страницы под DCBAA/кольца/контексты
 * переданы уже готовыми - их выделяли заранее, пока ещё были живы
 * Boot Services (AllocatePages после ExitBootServices недоступен).
 * Единственное, чем эта функция вообще пользуется из "внешнего
 * мира" - чтение/запись MMIO-регистров контроллера через порты
 * PCI и обычную память - к прошивке никакого отношения не имеет.
 */
void xhci_run_post_ebs(
    SIMPLE_TEXT_OUTPUT_INTERFACE *out,
    UINT64 xmmio,
    XHCI_CAP_INFO *cap,
    UINT64 dcbaa_phys,
    UINT64 cmdring_phys,
    UINT64 evring_phys,
    UINT64 erst_phys,
    UINT64 input_ctx_phys,
    UINT64 dev_ctx_phys,
    UINT64 ep0_ring_phys,
    UINT64 desc_buf_phys,
    UINT64 int_ring_phys
)
{
    UINT64 op_base = xmmio + cap->CapLength;

    print(out, "\n--- reset + minimal init ---\n");

    if (!xhci_reset_controller(op_base)) {

        print(
            out,
            "Controller did not come out of reset within "
            "the timeout - stopping here.\n"
        );
        return;
    }

    print(out, "Host Controller Reset: done.\n");

    raw_zero_mem((volatile UINT8 *)(UINTN)dcbaa_phys, 4096);
    raw_zero_mem((volatile UINT8 *)(UINTN)cmdring_phys, 4096);

    print(out, "DCBAA   at 0x");
    print_hex(out, dcbaa_phys, 16);
    print(out, "\nCmdRing at 0x");
    print_hex(out, cmdring_phys, 16);
    print(out, "\n");

    /* DCBAAP - Device Context Base Address Array Pointer */
    mmio_write32(
        op_base + 0x30, (UINT32)(dcbaa_phys & 0xFFFFFFFFu)
    );
    mmio_write32(op_base + 0x34, (UINT32)(dcbaa_phys >> 32));

    /* CRCR - Command Ring Control Register: указатель + Ring
       Cycle State = 1 (стартовое значение по спеке) */
    UINT64 crcr = (cmdring_phys & ~0x3Full) | 0x1u;

    mmio_write32(op_base + 0x18, (UINT32)(crcr & 0xFFFFFFFFu));
    mmio_write32(op_base + 0x1C, (UINT32)(crcr >> 32));

    /* CONFIG - сколько Device Slot'ов реально включаем */
    mmio_write32(op_base + 0x38, cap->MaxSlots);

    /*
     * Event Ring (регистры интеррапера ERSTSZ/ERSTBA/ERDP)
     * обязаны быть настроены ДО того, как ставится RS=1
     * (Run/Stop) - иначе контроллер может записать событие
     * (например Port Status Change) по ещё не инициализированным
     * регистрам, то есть буквально куда попало в память.
     */
    UINT64 rt_base = xmmio + cap->RtsOff;
    UINT64 intr0 = rt_base + 0x20;

    raw_zero_mem((volatile UINT8 *)(UINTN)evring_phys, 4096);
    raw_zero_mem((volatile UINT8 *)(UINTN)erst_phys, 4096);

    /* ERST[0]: адрес сегмента + число TRB в нём (по 16 байт
       каждый) */
    volatile UINT32 *erst = (volatile UINT32 *)(UINTN)erst_phys;

    erst[0] = (UINT32)(evring_phys & 0xFFFFFFFFu);
    erst[1] = (UINT32)(evring_phys >> 32);
    erst[2] = 256;
    erst[3] = 0;

    /* ERSTSZ = 1 сегмент */
    mmio_write32(intr0 + 0x08, 1);

    /* ERSTBA */
    mmio_write32(
        intr0 + 0x10, (UINT32)(erst_phys & 0xFFFFFFFFu)
    );
    mmio_write32(intr0 + 0x14, (UINT32)(erst_phys >> 32));

    /* ERDP = начало кольца (мы ещё ничего не читали) */
    mmio_write32(
        intr0 + 0x18, (UINT32)(evring_phys & 0xFFFFFFFFu)
    );
    mmio_write32(intr0 + 0x1C, (UINT32)(evring_phys >> 32));

    /* Run/Stop = 1 - запускаем контроллер */
    UINT32 usbcmd = mmio_read32(op_base + 0x00);
    usbcmd |= 0x1u;
    mmio_write32(op_base + 0x00, usbcmd);

    BOOLEAN started = FALSE;

    for (UINTN i = 0; i < 200; i++) {

        /* Раньше - BootServices->Stall(1000), см. busy_wait_ms()
           выше по файлу. */
        busy_wait_ms(1);

        UINT32 sts = mmio_read32(op_base + 0x04);

        if ((sts & 0x1u) == 0) {
            started = TRUE;
            break;
        }
    }

    if (!started) {

        print(
            out,
            "Controller did not leave Halted state after "
            "Run/Stop=1 - stopping here.\n"
        );
        return;
    }

    print(
        out, "Controller running (HCHalted=0). Port status:\n\n"
    );

    UINTN reset_target_port = 0;

    for (UINTN p = 1; p <= cap->MaxPorts; p++) {

        UINT64 port_base =
            op_base + 0x400 + (UINT64)(p - 1) * 0x10;

        UINT32 portsc = mmio_read32(port_base);

        BOOLEAN ccs = (portsc & 0x1u) != 0;
        BOOLEAN ped = (portsc & 0x2u) != 0;

        print(out, "  Port ");
        print_uint(out, p);
        print(out, ": PORTSC=0x");
        print_hex(out, portsc, 8);
        print(out, "  ");

        if (ccs) {

            print(out, "CONNECTED");

            if (ped) {
                print(out, ", enabled");
            } else if (reset_target_port == 0) {
                reset_target_port = p;
            }

        } else {

            print(out, "empty");
        }

        print(out, "\n");
    }

    /*
     * Порт подключён, но не включён (обычное дело для
     * не-SuperSpeed портов - им нужен явный Port Reset, только
     * после него PED станет 1). Дальше - Enable Slot Command
     * через Command Ring, чтобы получить Slot ID для этого
     * устройства.
     */
    if (reset_target_port == 0) {

        print(
            out,
            "\nNo port needs a reset (nothing new connected) - "
            "nothing further to do.\n"
        );
        return;
    }

    print(out, "\n--- port reset + Enable Slot Command ---\n");

    UINT64 port_base =
        op_base + 0x400 + (UINT64)(reset_target_port - 1) * 0x10;

    UINT32 cur = mmio_read32(port_base);

    mmio_write32(
        port_base,
        portsc_base_for_write(cur) | PORTSC_BIT_PR
    );

    BOOLEAN reset_done = FALSE;

    for (UINTN i = 0; i < 500; i++) {

        busy_wait_ms(1);

        UINT32 s = mmio_read32(port_base);

        if (s & PORTSC_BIT_PRC) {
            reset_done = TRUE;
            cur = s;
            break;
        }
    }

    if (!reset_done) {

        print(
            out,
            "Port Reset did not complete within the timeout - "
            "stopping here.\n"
        );
        return;
    }

    /* подтвердить (очистить) PRC, не трогая PED и остальные
       RW1CS-биты */
    mmio_write32(
        port_base,
        portsc_base_for_write(cur) | PORTSC_BIT_PRC
    );

    print(out, "Port ");
    print_uint(out, reset_target_port);
    print(out, ": reset complete, now enabled (PED=1).\n");

    /* Command TRB: Enable Slot (TRB Type 9), Cycle=1 */
    volatile UINT32 *cmd_trb =
        (volatile UINT32 *)(UINTN)cmdring_phys;

    cmd_trb[0] = 0;
    cmd_trb[1] = 0;
    cmd_trb[2] = 0;
    cmd_trb[3] = (9u << 10) | 0x1u;

    /* Doorbell 0, Target=0 - звонок в Command Ring */
    mmio_write32(xmmio + cap->DbOff, 0);

    volatile UINT32 *ev_trb = NULL;
    UINTN ev_slot = 0;

    BOOLEAN got_event =
        xhci_wait_for_event(
            out, evring_phys, intr0,
            &ev_slot, 33, /* Command Completion Event */
            &ev_trb
        );

    if (!got_event) {

        print(
            out,
            "No Command Completion Event showed up on the "
            "Event Ring - stopping here.\n"
        );
        return;
    }

    UINT8 trb_type = (UINT8)((ev_trb[3] >> 10) & 0x3Fu);
    UINT8 compl_code = (UINT8)((ev_trb[2] >> 24) & 0xFFu);
    UINT8 slot_id = (UINT8)((ev_trb[3] >> 24) & 0xFFu);

    print(out, "Event: TRB Type=");
    print_uint(out, trb_type);
    print(
        out,
        " (33=CommandCompletionEvent), CompletionCode="
    );
    print_uint(out, compl_code);
    print(out, " (1=Success), SlotID=");
    print_uint(out, slot_id);
    print(out, "\n");

    /* подтвердить это событие - сдвинуть ERDP на следующий TRB,
       и запомнить индекс для дальнейшего опроса (Address
       Device) */
    ev_slot = ev_slot + 1;

    UINT64 next_ev_addr =
        xhci_event_ring_trb_addr(evring_phys, ev_slot);

    mmio_write32(
        intr0 + 0x18, (UINT32)(next_ev_addr & 0xFFFFFFFFu)
    );
    mmio_write32(intr0 + 0x1C, (UINT32)(next_ev_addr >> 32));

    if (!(trb_type == 33 && compl_code == 1)) {
        return;
    }

    print(
        out,
        "\nEnable Slot succeeded - device has a Slot ID now.\n"
    );

    /*
     * Address Device + GET_DESCRIPTOR + SET_CONFIGURATION +
     * опрос отчётов. port_base тут ещё указывает на PORTSC
     * reset_target_port (мы его не трогали с момента Port Reset
     * выше). ev_slot уже указывает на первый ещё не занятый слот
     * Event Ring - передаём его как стартовую точку опроса.
     */
    xhci_address_device_and_get_descriptor(
        out, xmmio, cap,
        dcbaa_phys, cmdring_phys, evring_phys, intr0,
        port_base, reset_target_port, slot_id, ev_slot,
        input_ctx_phys, dev_ctx_phys, ep0_ring_phys,
        desc_buf_phys, int_ring_phys
    );
}
