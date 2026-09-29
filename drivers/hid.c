/*
 * drivers/hid.c - разбор HID Report Descriptor.
 * Часть MyOS; общие объявления - в myos.h.
 */
#include "myos.h"


/*
 * Достаёт bit_size битов из буфера отчёта, начиная с абсолютного
 * bit_offset (считая от бита 0 байта 0), младшим битом вперёд
 * (см. объяснение bit-пакинга в комментарии выше). Возвращает
 * СЫРОЕ беззнаковое значение до 32 битов - знак (для
 * относительных dX/dY) навешивает отдельно hid_sign_extend ниже,
 * здесь мы его ещё не знаем (это решает Logical Minimum, а не
 * сам факт извлечения битов).
 */
UINT32 hid_extract_bits(
    volatile UINT8 *report,
    UINTN report_len_bytes,
    UINT32 bit_offset,
    UINT8 bit_size
)
{
    UINT32 value = 0;

    if (bit_size > 32)
        bit_size = 32; /* защита - в реальных мышиных дескрипторах
                           такого не бывает, но на всякий случай
                           не переполняем UINT32 сдвигом >=32 */

    for (UINT8 i = 0; i < bit_size; i++) {

        UINT32 abs_bit = bit_offset + i;
        UINT32 byte_index = abs_bit / 8u;
        UINT8  bit_in_byte = (UINT8)(abs_bit % 8u);

        if (byte_index >= report_len_bytes) {

            /* реальный отчёт оказался короче, чем обещал
               дескриптор (короткий пакет) - дальше считаем
               недостающие биты нулями, а не читаем за пределы
               буфера */
            break;
        }

        UINT8 byte_val = report[byte_index];
        UINT8 bit_val = (UINT8)((byte_val >> bit_in_byte) & 0x1u);

        if (bit_val)
            value = value | (1u << i);
    }

    return value;
}


/*
 * Знаковое расширение значения шириной bit_size битов до полных
 * 32 битов (дополнительный код) - нужно для dX/dY, которые
 * почти всегда описаны как Logical Minimum < 0 (типично -127..
 * 127 при Report Size=8), то есть являются знаковыми, а
 * hid_extract_bits выше отдаёт их просто как биты без знака.
 */
INT32 hid_sign_extend(UINT32 raw_value, UINT8 bit_size)
{
    if (bit_size == 0 || bit_size >= 32)
        return (INT32)raw_value;

    UINT32 sign_bit_mask = 1u << (bit_size - 1u);

    if (raw_value & sign_bit_mask) {

        UINT32 extend_mask = ~((1u << bit_size) - 1u);

        return (INT32)(raw_value | extend_mask);
    }

    return (INT32)raw_value;
}


/*
 * Интерпретирует raw как знаковое число шириной ровно
 * item_size_bytes байт (0,1,2 или 4 - см. bSize в комментарии
 * выше) - нужно для Global item'ов Logical Minimum/Maximum,
 * которые по спеке HID хранятся в ровно стольких байтах, сколько
 * реально нужно, и это ЗНАКОВЫЕ величины (Logical Minimum мыши
 * почти всегда отрицательный).
 */
INT32 hid_item_signed_value(
    UINT32 raw,
    UINT8 item_size_bytes
)
{
    if (item_size_bytes == 1) {

        return (INT32)(INT8)(raw & 0xFFu);

    } else if (item_size_bytes == 2) {

        /* efi.h не объявляет INT16 (только INT8/UINT8/UINT16),
           поэтому знаковое расширение 2-байтового значения
           делаем вручную через ту же битовую маску, что и
           hid_sign_extend ниже, а не приведением типа к
           несуществующему INT16 */
        UINT32 v = raw & 0xFFFFu;

        if (v & 0x8000u)
            return (INT32)(v | 0xFFFF0000u);

        return (INT32)v;
    }

    /* item_size_bytes == 4 (или 0, тогда raw и так 0) */
    return (INT32)raw;
}


/*
 * Сам разбор Report Descriptor - проходит по всем item'ам подряд
 * (см. общее объяснение формата в большом комментарии перед этим
 * блоком) и ищет Input-поля с Usage Page/Usage, похожими на
 * обычную мышь: Generic Desktop (0x01) / X (0x30), Y (0x31),
 * Wheel (0x38), и Button Page (0x09) для кнопок. Найденные битовые
 * смещения/ширины складывает в *layout. Печатает в out
 * человеко-читаемый итог - чтобы пользователь мог убедиться
 * скриншотом, что разбор прошёл осмысленно (а не просто "молча
 * не нашёл ничего").
 *
 * ОГРАНИЧЕНИЯ (осознанные, явно, чтобы не забыть):
 *  - Не поддержаны Push/Pop (Global item'ы 0xA4/0xB4) - редкость
 *    у простых мышей, сохранение/восстановление стека Global-
 *    состояния не реализовано; если встретится - разбор просто
 *    пойдёт по item'ам дальше, результат для необычных
 *    дескрипторов может быть неверным.
 *  - Отдельные счётчики бит-смещения для Output/Feature НЕ
 *    ведутся вообще (нам не нужны их данные) - но и не мешают:
 *    их Report Size/Report Count просто не трогают тот
 *    единственный счётчик, что мы ведём (input_bit_offset), это
 *    и есть корректное поведение (у каждого типа отчёта, Input/
 *    Output/Feature, отдельная своя нумерация бит по спеке).
 *  - Составные устройства с НЕСКОЛЬКИМИ разными Report ID для
 *    разных наборов данных (например, отдельно мышь и отдельно
 *    медиа-клавиши в одном интерфейсе) - report_id запоминается
 *    просто "последний встреченный", а не привязывается к
 *    конкретному найденному полю. Для одиночного HID-интерфейса
 *    мыши (typичный случай, в т.ч. у большинства USB-донглов) это
 *    не проблема - там обычно вообще нет Report ID или он один.
 */
void hid_parse_report_descriptor(
    SIMPLE_TEXT_OUTPUT_INTERFACE *out,
    volatile UINT8 *desc,
    UINT16 desc_len,
    HID_MOUSE_REPORT_LAYOUT *layout
)
{
    /* обнуляем результат вручную - libc/memset нет */
    layout->valid = FALSE;
    layout->has_report_id = FALSE;
    layout->report_id = 0;
    layout->has_buttons = FALSE;
    layout->button_bit_offset = 0;
    layout->button_count = 0;
    layout->x_bit_offset = 0;
    layout->x_bit_size = 0;
    layout->x_is_relative = FALSE;
    layout->y_bit_offset = 0;
    layout->y_bit_size = 0;
    layout->y_is_relative = FALSE;
    layout->has_wheel = FALSE;
    layout->wheel_bit_offset = 0;
    layout->wheel_bit_size = 0;
    layout->wheel_is_relative = FALSE;
    layout->x_logical_max = 0;
    layout->y_logical_max = 0;

    /* "текущие" Global-настройки */
    UINT16 usage_page = 0;
    UINT32 report_size = 0;
    UINT32 report_count = 0;
    INT32  logical_min = 0;
    INT32  logical_max = 0;

    /*
     * Несколько Report ID в одном дескрипторе (частый случай у
     * беспроводных донглов: в одном интерфейсе и мышь, и
     * мультимедиа-клавиши). По спеке HID у КАЖДОГО Report ID
     * своя нумерация бит, начиная с нуля (сразу после байта ID).
     * Раньше счётчик бит был один на весь дескриптор - поля
     * второго отчёта получали неверные смещения. Теперь для
     * каждого встреченного ID хранится свой счётчик, а у
     * найденных полей запоминается, в каком отчёте они живут -
     * в итоге layout->report_id = ID того отчёта, где X.
     */
    UINT8  rid_ids[8];
    UINT32 rid_offs[8];
    UINT8  rid_count = 0;
    UINT8  cur_report_id = 0;
    UINT8  buttons_report_id = 0;
    UINT8  x_report_id = 0;
    UINT8  wheel_report_id = 0;

    /* "текущие" Local-настройки - сбрасываются после КАЖДОГО
       Main item'а (см. конец цикла) */
    UINT16  usage_stack[16];
    UINT8   usage_stack_count = 0;
    UINT16  usage_min = 0;
    UINT16  usage_max = 0;
    BOOLEAN seen_usage_min = FALSE;
    BOOLEAN seen_usage_max = FALSE;

    /* Счётчик бит только для Input-отчётов (см. ограничение про
       Output/Feature в комментарии выше) */
    UINT32 input_bit_offset = 0;

    UINT16 pos = 0;

    while (pos < desc_len) {

        UINT8 prefix = desc[pos];

        if (prefix == 0xFEu) {

            /* Long item - у простых HID-мышей практически не
               встречается, но формат другой (следующий байт -
               размер данных, затем байт tag'а, затем сами
               данные) - корректно пропускаем целиком, не пытаясь
               разобрать как обычный item. */

            if ((UINT32)pos + 2u > (UINT32)desc_len)
                break;

            UINT8 long_data_size = desc[pos + 1];

            pos = (UINT16)(pos + 3 + long_data_size);
            continue;
        }

        UINT8 b_size_code = (UINT8)(prefix & 0x3u);
        UINT8 b_type = (UINT8)((prefix >> 2) & 0x3u);
        UINT8 b_tag = (UINT8)((prefix >> 4) & 0xFu);

        /* bSize=3 - особый случай, означает 4 байта данных, а
           не 3 (см. большой комментарий выше) */
        UINT8 item_size =
            (b_size_code == 3u) ? 4u : b_size_code;

        pos = (UINT16)(pos + 1);

        if ((UINT32)pos + (UINT32)item_size > (UINT32)desc_len)
            break; /* битый/обрезанный дескриптор (короткий
                       пакет) - останавливаемся, не читаем за
                       пределы буфера */

        UINT32 raw = 0;

        for (UINT8 i = 0; i < item_size; i++) {
            raw = raw | ((UINT32)desc[pos + i] << (8u * i));
        }

        pos = (UINT16)(pos + item_size);

        if (b_type == 1u) {

            /* --- Global item --- */

            if (b_tag == 0u) {

                /* Usage Page */
                usage_page = (UINT16)(raw & 0xFFFFu);

            } else if (b_tag == 1u) {

                /* Logical Minimum - знаковое, ширина=item_size */
                logical_min =
                    hid_item_signed_value(raw, item_size);

            } else if (b_tag == 2u) {

                /* Logical Maximum - знаковое, как и Minimum */
                logical_max =
                    hid_item_signed_value(raw, item_size);

            } else if (b_tag == 7u) {

                /* Report Size - ширина ОДНОГО элемента поля,
                   в битах */
                report_size = raw;

            } else if (b_tag == 8u) {

                /* Report ID - раз он вообще встретился в
                   дескрипторе, значит каждый реальный отчёт
                   с устройства начинается с байта Report ID
                   (см. has_report_id ниже, в конце функции) */
                layout->has_report_id = TRUE;

                UINT8 new_id = (UINT8)(raw & 0xFFu);

                /* сохранить счётчик бит текущего отчёта и
                   переключиться на счётчик нового (0, если этот
                   ID встретился впервые) */
                BOOLEAN saved = FALSE;

                for (UINT8 k = 0; k < rid_count; k++) {
                    if (rid_ids[k] == cur_report_id) {
                        rid_offs[k] = input_bit_offset;
                        saved = TRUE;
                    }
                }

                if (!saved && rid_count < 8u) {
                    rid_ids[rid_count] = cur_report_id;
                    rid_offs[rid_count] = input_bit_offset;
                    rid_count++;
                }

                input_bit_offset = 0;

                for (UINT8 k = 0; k < rid_count; k++) {
                    if (rid_ids[k] == new_id)
                        input_bit_offset = rid_offs[k];
                }

                cur_report_id = new_id;

            } else if (b_tag == 9u) {

                /* Report Count - сколько ОДИНАКОВЫХ по ширине
                   элементов подряд в следующем Main item'е */
                report_count = raw;
            }

            /* остальные Global item'ы (Logical Maximum,
               Physical Minimum/Maximum, Unit Exponent, Unit,
               Push, Pop) нам не нужны - намеренно игнорируем */

        } else if (b_type == 2u) {

            /* --- Local item --- */

            if (b_tag == 0u) {

                /* Usage - "назначение" одного конкретного поля
                   (например, X, Y или Wheel по отдельности) */
                if (usage_stack_count < 16u) {
                    usage_stack[usage_stack_count] =
                        (UINT16)(raw & 0xFFFFu);
                    usage_stack_count =
                        (UINT8)(usage_stack_count + 1u);
                }

            } else if (b_tag == 1u) {

                /* Usage Minimum - начало ДИАПАЗОНА назначений
                   (так почти всегда описывают кнопки: "кнопки
                   с Minimum по Maximum") */
                usage_min = (UINT16)(raw & 0xFFFFu);
                seen_usage_min = TRUE;

            } else if (b_tag == 2u) {

                /* Usage Maximum - конец того же диапазона */
                usage_max = (UINT16)(raw & 0xFFFFu);
                seen_usage_max = TRUE;
            }

        } else if (b_type == 0u) {

            /* --- Main item --- */

            if (b_tag == 8u) {

                /* Input - собственно поле данных, которое
                   устройство нам ПРИСЫЛАЕТ (то, что мы опрашиваем
                   через Interrupt IN endpoint) */

                UINT32 flags = raw;
                BOOLEAN is_const = (flags & 0x1u) != 0;
                BOOLEAN is_relative = (flags & 0x4u) != 0;

                if (
                    !is_const &&
                    usage_page == 0x09u &&
                    !layout->has_buttons
                ) {

                    /* Button Page - по спеке USB HID кнопки
                       мыши описываются ОДНИМ полем-диапазоном
                       (Usage Minimum=1..Usage Maximum=N кнопок,
                       Report Size=1 бит на кнопку, Report
                       Count=N) - значит всё это Input item'а
                       целиком и есть блок кнопок, bit0 этого
                       блока = кнопка с номером Usage Minimum
                       (почти всегда 1 = левая). Берём только
                       первое такое поле, если их несколько -
                       см. ограничение про несколько Report ID
                       в комментарии перед функцией. */

                    layout->has_buttons = TRUE;
                    layout->button_bit_offset = input_bit_offset;
                    layout->button_count = (UINT8)report_count;
                    buttons_report_id = cur_report_id;
                }

                /* X/Y/Wheel описываются ОТДЕЛЬНЫМИ Usage-тегами
                   (не диапазоном) - i-й Usage из usage_stack
                   соответствует i-му полю по порядку внутри
                   этого Input item'а (см. общий комментарий про
                   Main item выше). */

                /* Некоторые (особенно дешёвые) устройства описывают
                   X/Y не отдельными Usage, а диапазоном "Usage
                   Minimum=X(0x30) .. Usage Maximum=Y(0x31)" - тогда
                   i-е поле получает Usage = Minimum + i. */
                BOOLEAN xy_by_range =
                    (usage_stack_count == 0u) &&
                    seen_usage_min && seen_usage_max &&
                    usage_max >= usage_min;

                UINT32 xy_fields =
                    xy_by_range ?
                        (UINT32)(usage_max - usage_min) + 1u :
                        (UINT32)usage_stack_count;

                if (
                    !is_const &&
                    usage_page == 0x01u &&
                    xy_fields > 0u
                ) {

                    for (
                        UINT8 i = 0;
                        i < report_count && i < xy_fields;
                        i++
                    ) {

                        UINT16 field_usage =
                            xy_by_range ?
                                (UINT16)(usage_min + i) :
                                usage_stack[i];

                        UINT32 field_bit_offset =
                            input_bit_offset +
                            (UINT32)i * report_size;

                        if (
                            field_usage == 0x30u &&
                            layout->x_bit_size == 0
                        ) {

                            /* Generic Desktop / X */
                            layout->x_bit_offset =
                                field_bit_offset;
                            layout->x_bit_size =
                                (UINT8)report_size;
                            layout->x_is_relative =
                                is_relative;
                            layout->x_logical_max =
                                logical_max;
                            x_report_id = cur_report_id;

                        } else if (
                            field_usage == 0x31u &&
                            layout->y_bit_size == 0
                        ) {

                            /* Generic Desktop / Y */
                            layout->y_bit_offset =
                                field_bit_offset;
                            layout->y_bit_size =
                                (UINT8)report_size;
                            layout->y_is_relative =
                                is_relative;
                            layout->y_logical_max =
                                logical_max;

                        } else if (
                            field_usage == 0x38u &&
                            !layout->has_wheel
                        ) {

                            /* Generic Desktop / Wheel */
                            layout->has_wheel = TRUE;
                            layout->wheel_bit_offset =
                                field_bit_offset;
                            layout->wheel_bit_size =
                                (UINT8)report_size;
                            layout->wheel_is_relative =
                                is_relative;
                            wheel_report_id = cur_report_id;
                        }
                    }
                }

                /* только Input-поля двигают наш счётчик - см.
                   ограничение про Output/Feature в комментарии
                   перед функцией */
                input_bit_offset =
                    input_bit_offset +
                    report_count * report_size;
            }

            /* Local-состояние (Usage/Usage Minimum/Usage
               Maximum) по спеке HID живёт ровно до следующего
               Main item'а (Input/Output/Feature/Collection/End
               Collection - вообще любого) - сбрасываем после
               ЛЮБОГО Main item'а, не только Input. */
            usage_stack_count = 0;
            usage_min = 0;
            usage_max = 0;
            seen_usage_min = FALSE;
            seen_usage_max = FALSE;

            /* usage_min/usage_max выше пока нигде не
               используются для полей мыши (кнопки определяются
               по Usage Page целиком, см. Input выше) - они
               оставлены разобранными на будущее (например, если
               понадобится точно знать номер первой кнопки,
               logical_min тоже уже разобран и лежит рядом) */
            (void)usage_min;
            (void)usage_max;
            (void)seen_usage_min;
            (void)seen_usage_max;
            (void)logical_min;
        }
    }

    /* Если в дескрипторе был Report ID - реальный байт 0
       каждого отчёта занят им, а не данными; все найденные
       смещения выше считались от начала "полезной" части
       отчёта (как будто Report ID не было), нужно сдвинуть их
       на 8 бит вперёд ровно один раз, здесь. */

    if (layout->has_report_id) {

        /* отчёт мыши - тот, где нашёлся X; кнопки/колесо из
           ДРУГОГО отчёта (например, "кнопки" мультимедиа-блока)
           к мыши отношения не имеют - лучше без них, чем
           с чужими битами */
        layout->report_id = x_report_id;

        if (layout->has_buttons && buttons_report_id != x_report_id)
            layout->has_buttons = FALSE;

        if (layout->has_wheel && wheel_report_id != x_report_id)
            layout->has_wheel = FALSE;
    }

    UINT32 id_shift = layout->has_report_id ? 8u : 0u;

    layout->button_bit_offset =
        layout->button_bit_offset + id_shift;
    layout->x_bit_offset = layout->x_bit_offset + id_shift;
    layout->y_bit_offset = layout->y_bit_offset + id_shift;
    layout->wheel_bit_offset =
        layout->wheel_bit_offset + id_shift;

    layout->valid =
        (layout->x_bit_size != 0) &&
        (layout->y_bit_size != 0);

    /* --- печатаем человеко-читаемый итог разбора --- */

    /* фоновое подключение (горячее, в GUI) - печатать некуда */
    if (out == NULL)
        return;

    print(out, "\nReport Descriptor parsed (");
    print_uint(out, desc_len);
    print(out, " bytes):\n");

    if (layout->has_report_id) {
        print(out, "  Report ID = ");
        print_uint(out, layout->report_id);
        print(out, " (present in every report as byte 0)\n");
    } else {
        print(out, "  no Report ID (reports start with data "
                    "directly)\n");
    }

    if (layout->has_buttons) {
        print(out, "  Buttons: ");
        print_uint(out, layout->button_count);
        print(out, " bit(s) at bit offset ");
        print_uint(out, layout->button_bit_offset);
        print(out, "\n");
    } else {
        print(out, "  Buttons: not found\n");
    }

    if (layout->x_bit_size != 0) {
        print(out, "  X: ");
        print_uint(out, layout->x_bit_size);
        print(out, "-bit, offset ");
        print_uint(out, layout->x_bit_offset);
        print(out, ", ");
        print(out, layout->x_is_relative ? "relative" :
                                            "ABSOLUTE");
        print(out, "\n");
    } else {
        print(out, "  X: not found\n");
    }

    if (layout->y_bit_size != 0) {
        print(out, "  Y: ");
        print_uint(out, layout->y_bit_size);
        print(out, "-bit, offset ");
        print_uint(out, layout->y_bit_offset);
        print(out, ", ");
        print(out, layout->y_is_relative ? "relative" :
                                            "ABSOLUTE");
        print(out, "\n");
    } else {
        print(out, "  Y: not found\n");
    }

    if (layout->has_wheel) {
        print(out, "  Wheel: ");
        print_uint(out, layout->wheel_bit_size);
        print(out, "-bit, offset ");
        print_uint(out, layout->wheel_bit_offset);
        print(out, "\n");
    } else {
        print(out, "  Wheel: not present\n");
    }

    if (!layout->valid) {

        print(
            out,
            "  WARNING: no X and/or Y field found - this "
            "does not look like a pointing device, falling "
            "back to the old guessed byte0/1/2 layout below.\n"
        );
    }
}
