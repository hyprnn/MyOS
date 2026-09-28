/*
 * shell/calc.c - калькулятор calc.
 * Часть MyOS; общие объявления - в myos.h.
 */
#include "myos.h"


/* ============================================================
 * Calculator
 * ============================================================ */

void cmd_calc(
    EFI_SYSTEM_TABLE *st,
    CHAR16 *rest
)
{
    SIMPLE_TEXT_OUTPUT_INTERFACE *out =
        st->ConOut;

    rest =
        skip_ws16(rest);


    CHAR16 a[32];
    CHAR16 op[8];
    CHAR16 b[32];


    rest =
        take_word(
            rest,
            a,
            32
        );


    rest =
        skip_ws16(rest);


    rest =
        take_word(
            rest,
            op,
            8
        );


    rest =
        skip_ws16(rest);


    take_word(
        rest,
        b,
        32
    );


    if (char16_len(a) == 0 ||
        char16_len(op) == 0 ||
        char16_len(b) == 0) {

        print(
            out,
            "Usage: calc <a> <+|-|*|/> <b>\n"
        );

        return;
    }


    INTN x =
        (INTN)parse_uint(a);

    INTN y =
        (INTN)parse_uint(b);

    INTN r;


    if (char16_eq(op, L"+")) {

        r = x + y;

    } else if (char16_eq(op, L"-")) {

        r = x - y;

    } else if (char16_eq(op, L"*")) {

        r = x * y;

    } else if (char16_eq(op, L"/")) {

        if (y == 0) {

            print(
                out,
                "Error: division by zero.\n"
            );

            return;
        }

        r = x / y;

    } else {

        print(
            out,
            "Unknown operator. Use + - * /\n"
        );

        return;
    }


    if (r < 0) {

        print(out, "-");

        r = -r;
    }


    print_uint(
        out,
        (UINT64)r
    );

    print(
        out,
        "\n"
    );
}
