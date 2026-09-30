/*
 * play - проиграть звуковой файл (этап 10, А).
 *
 *   play музыка.mp3
 *   play звук.wav
 *   play -t 440 [мс]    тон заданной частоты (проверка звука)
 *
 * Ctrl+C - остановить. Громкость - команда volume или клавиши
 * громкости (регулирует ядро).
 *
 * Ядро принимает звук в одном формате: 48000 Гц, 16 бит, стерео
 * (drivers/hda.c). Всё остальное - здесь:
 *   * WAV (PCM 8/16/24 бит, моно/стерео) - читаем как есть;
 *   * MP3 - декодер minimp3 (third_party/minimp3, общественное
 *     достояние, один заголовочный файл);
 *   * другая частота (44100 у CD и почти всех MP3, 22050, 11025) -
 *     пересчёт в 48000 линейной интерполяцией: между двумя соседними
 *     отсчётами берём точку на прямой. Для слуха этого хватает.
 */
#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "myos_sys.h"

#define MINIMP3_IMPLEMENTATION
#include "../../../third_party/minimp3/minimp3.h"

#define OUT_RATE  48000
#define OUT_CHUNK 4096            /* кадров за раз в ядро (~85 мс) */

static int16_t  g_out[OUT_CHUNK * 2];
static int      g_outn;
static uint64_t g_frames_out;
static int      g_total_sec = -1;
static int      g_last_sec = -1;

static void show_progress(void)
{
    int sec = (int)(g_frames_out / OUT_RATE);

    if (sec == g_last_sec)
        return;

    g_last_sec = sec;

    if (g_total_sec >= 0)
        printf("\r  %d:%02d / %d:%02d", sec / 60, sec % 60, g_total_sec / 60, g_total_sec % 60);
    else
        printf("\r  %d:%02d", sec / 60, sec % 60);

    fflush(stdout);
}

static int flush_out(void)
{
    if (g_outn == 0)
        return 0;

    long r = audio_write(g_out, (unsigned long)g_outn * 4u);

    if (r < 0) {
        printf("\nplay: sound: %ld\n", r);
        return -1;
    }

    g_frames_out += (uint64_t)g_outn;
    g_outn = 0;
    show_progress();
    return 0;
}

static int emit(int16_t l, int16_t r)
{
    g_out[g_outn * 2] = l;
    g_out[g_outn * 2 + 1] = r;

    if (++g_outn == OUT_CHUNK)
        return flush_out();

    return 0;
}

/* ---------------- пересчёт частоты ---------------- */

/*
 * Входной поток виден как [prev, in[0], in[1], ...]: prev - последний
 * кадр прошлого куска (чтобы интерполировать и через границу кусков).
 * pos - позиция в этом потоке, 32.32 с фиксированной точкой; шаг -
 * сколько входных кадров на один выходной (44100/48000 = 0.91875).
 */
typedef struct {
    uint64_t pos, step;
    int32_t  prev_l, prev_r;
} RESAMPLER;

static void rs_init(RESAMPLER *rs, int in_rate)
{
    rs->pos = 0;
    rs->step = ((uint64_t)in_rate << 32) / OUT_RATE;
    rs->prev_l = rs->prev_r = 0;
}

/* in - n кадров по ch каналов (16 бит) */
static int rs_push(RESAMPLER *rs, const int16_t *in, int n, int ch)
{
    for (;;) {

        uint64_t i = rs->pos >> 32;

        if (i + 1 > (uint64_t)n)
            break;

        int32_t frac = (int32_t)((rs->pos >> 16) & 0xFFFF);
        int32_t al = (i == 0) ? rs->prev_l : in[(i - 1) * ch];
        int32_t ar = (i == 0) ? rs->prev_r : in[(i - 1) * ch + (ch > 1)];
        int32_t bl = in[i * ch];
        int32_t br = in[i * ch + (ch > 1)];

        if (emit((int16_t)(al + (((bl - al) * frac) >> 16)),
                 (int16_t)(ar + (((br - ar) * frac) >> 16))) < 0)
            return -1;

        rs->pos += rs->step;
    }

    rs->pos -= (uint64_t)n << 32;
    rs->prev_l = in[(n - 1) * ch];
    rs->prev_r = in[(n - 1) * ch + (ch > 1)];
    return 0;
}

/* ---------------- WAV ---------------- */

static uint32_t le32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }

static int play_wav(FILE *f, const char *name)
{
    uint8_t h[12];

    if (fread(h, 1, 12, f) != 12 || memcmp(h, "RIFF", 4) || memcmp(h + 8, "WAVE", 4)) {
        printf("play: %s: not a WAV file\n", name);
        return 1;
    }

    int fmt = 0, ch = 0, rate = 0, bits = 0;
    uint32_t data_len = 0;

    /* куски RIFF: "fmt " - формат, "data" - сами отсчёты */
    for (;;) {

        uint8_t c[8];

        if (fread(c, 1, 8, f) != 8) {
            printf("play: %s: no sound data in the file\n", name);
            return 1;
        }

        uint32_t len = le32(c + 4);

        if (!memcmp(c, "fmt ", 4)) {

            uint8_t b[40];
            uint32_t k = len < sizeof(b) ? len : sizeof(b);

            if (fread(b, 1, k, f) != k)
                return 1;

            fmt = le16(b);
            ch = le16(b + 2);
            rate = (int)le32(b + 4);
            bits = le16(b + 14);

            /* WAVE_FORMAT_EXTENSIBLE: настоящий формат - в подформате */
            if (fmt == 0xFFFE && k >= 26)
                fmt = le16(b + 24);

            fseek(f, (long)(len - k + (len & 1)), SEEK_CUR);

        } else if (!memcmp(c, "data", 4)) {

            data_len = len;
            break;

        } else {
            fseek(f, (long)(len + (len & 1)), SEEK_CUR);
        }
    }

    if (fmt != 1 || (bits != 8 && bits != 16 && bits != 24) || ch < 1 || rate < 4000) {
        printf("play: %s: unsupported WAV (format %d, %d bit, %d channels) - "
               "PCM 8/16/24 bit only\n", name, fmt, bits, ch);
        return 1;
    }

    int bps = bits / 8 * ch;              /* байт на кадр */
    g_total_sec = (int)(data_len / (uint32_t)bps / (uint32_t)rate);

    printf("Playing %s: WAV, %d Hz, %d bit, %s\n", name, rate, bits,
           ch == 1 ? "mono" : ch == 2 ? "stereo" : "multichannel");

    RESAMPLER rs;
    rs_init(&rs, rate);

    static uint8_t raw[8192 * 4];
    static int16_t pcm[8192 * 2];
    uint32_t left = data_len;

    while (left > 0) {

        uint32_t want = (uint32_t)(sizeof(raw) / (size_t)bps) * (uint32_t)bps;
        if (want > left)
            want = left;

        size_t got = fread(raw, 1, want, f);
        int n = (int)(got / (size_t)bps);

        if (n == 0)
            break;

        left -= (uint32_t)got;

        /* в 16 бит, два канала (лишние каналы - прочь) */
        for (int i = 0; i < n; i++)
            for (int c = 0; c < 2; c++) {
                int src = c < ch ? c : 0;
                const uint8_t *s = raw + (size_t)i * bps + (size_t)src * (bits / 8);
                int16_t v;
                if (bits == 8)
                    v = (int16_t)((s[0] - 128) << 8);
                else if (bits == 16)
                    v = (int16_t)le16(s);
                else
                    v = (int16_t)le16(s + 1);    /* 24 бит: старшие 16 */
                pcm[i * 2 + c] = v;
            }

        if (rs_push(&rs, pcm, n, 2) < 0)
            return 1;
    }

    return 0;
}

/* ---------------- MP3 ---------------- */

static int play_mp3(FILE *f, const char *name)
{
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0) {
        printf("play: %s: empty file\n", name);
        return 1;
    }

    uint8_t *buf = malloc((size_t)size);

    if (buf == NULL) {
        printf("play: %s: too big (%ld bytes) - not enough memory\n", name, size);
        return 1;
    }

    if (fread(buf, 1, (size_t)size, f) != (size_t)size) {
        printf("play: %s: read error\n", name);
        free(buf);
        return 1;
    }

    static mp3dec_t dec;
    static int16_t pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
    mp3dec_init(&dec);

    RESAMPLER rs;
    int started = 0;
    long off = 0;

    while (off < size) {

        mp3dec_frame_info_t info;
        int n = mp3dec_decode_frame(&dec, buf + off, (int)(size - off), pcm, &info);

        if (info.frame_bytes == 0)
            break;                     /* дальше не MP3 */

        off += info.frame_bytes;

        if (n == 0)
            continue;                  /* заголовок ID3 или мусор */

        if (!started) {
            started = 1;
            rs_init(&rs, info.hz);
            /* длина - по битрейту первого кадра (для VBR - примерно) */
            if (info.bitrate_kbps > 0)
                g_total_sec = (int)(size * 8 / 1000 / info.bitrate_kbps);
            printf("Playing %s: MP3, %d Hz, %d kbit/s, %s\n", name, info.hz,
                   info.bitrate_kbps, info.channels == 1 ? "mono" : "stereo");
        }

        if (rs_push(&rs, pcm, n, info.channels) < 0) {
            free(buf);
            return 1;
        }
    }

    free(buf);

    if (!started) {
        printf("play: %s: no MP3 frames found\n", name);
        return 1;
    }

    return 0;
}

/* ---------------- тон ---------------- */

static int play_tone(int hz, int ms)
{
    printf("Playing a %d Hz tone for %d ms\n", hz, ms);

    int frames = (int)((int64_t)ms * OUT_RATE / 1000);

    for (int i = 0; i < frames; i++) {
        int16_t v = (int16_t)(12000.0 * sin(2.0 * M_PI * hz * i / OUT_RATE));
        if (emit(v, v) < 0)
            return 1;
    }

    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: play <file.wav|file.mp3>   or   play -t <Hz> [ms]\n");
        return 1;
    }

    int r = audio_open();

    if (r < 0) {
        printf("play: %s\n", r == MYOS_EBUSY ? "sound is busy - another program is playing"
                             : r == MYOS_ENODEV ? "no sound card (Intel HD Audio) found"
                             : "cannot open sound");
        return 1;
    }

    int rc;

    if (strcmp(argv[1], "-t") == 0) {

        int hz = argc > 2 ? atoi(argv[2]) : 440;
        int ms = argc > 3 ? atoi(argv[3]) : 1000;
        rc = play_tone(hz > 0 ? hz : 440, ms > 0 ? ms : 1000);

    } else {

        FILE *f = fopen(argv[1], "rb");

        if (f == NULL) {
            printf("play: %s: %s\n", argv[1], strerror(errno));
            audio_close();
            return 1;
        }

        /* WAV начинается с "RIFF"; остальное пробуем как MP3 */
        char magic[4] = { 0 };
        fread(magic, 1, 4, f);
        fseek(f, 0, SEEK_SET);

        rc = (memcmp(magic, "RIFF", 4) == 0) ? play_wav(f, argv[1]) : play_mp3(f, argv[1]);
        fclose(f);
    }

    if (rc == 0) {
        flush_out();
        audio_drain();
        printf("\nDone: %d:%02d played.\n", (int)(g_frames_out / OUT_RATE / 60),
               (int)(g_frames_out / OUT_RATE % 60));
    }

    audio_close();
    return rc;
}
