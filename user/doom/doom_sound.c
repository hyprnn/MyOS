/*
 * user/doom/doom_sound.c - звук DOOM на MyOS (этап 10, Б).
 *
 * Эффекты (выстрел, дверь, крик) лежат в WAD кусками "DSxxxx": 8 байт
 * заголовка (формат 3, частота - обычно 11025 Гц, число отсчётов), и
 * 8-битные отсчёты без знака (по 16 байт "подушки" по краям - их
 * пропускаем). Микшер: до 16 звуков разом, у каждого - громкость
 * слева/справа (DOOM сам считает её по расстоянию и направлению) и
 * шаг по отсчётам для пересчёта 11025 -> 48000 Гц.
 *
 * Музыка в DOOM - не запись, а ноты (формат MUS, похож на MIDI): на
 * звуковой карте 1993 года их играл синтезатор OPL. Здесь - свой
 * маленький синтезатор: у каждой ноты волна (прямоугольная, треугольная
 * или "пила" - по номеру инструмента), огибающая (быстрая атака, спад,
 * затухание после отпускания); барабаны (канал 15) - шум с быстрым
 * затуханием. Звучит проще OPL, но мелодии узнаются.
 *
 * Звук в ядро пишет Update (его зовут каждый такт игры, 35 раз в
 * секунду): столько, чтобы в ядре лежало ~100 мс вперёд - так звук не
 * прерывается, а задержка между выстрелом и звуком незаметна.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "myos_sys.h"

#include "doomtype.h"
#include "i_sound.h"
#include "w_wad.h"
#include "z_zone.h"

#define RATE        48000
#define AHEAD_BYTES (RATE * 4 / 10)       /* 100 мс вперёд */
#define NCH         16

typedef struct {
    const unsigned char *data;   /* 8-битные отсчёты */
    unsigned int len;
    unsigned int pos;            /* 16.16 */
    unsigned int step;
    int vl, vr;                  /* 0..254 */
    int active;
} SFX_CH;

static SFX_CH g_ch[NCH];
static int g_ok;
static boolean g_prefix;

static int16_t g_mix[4096 * 2];

/* настройки из default.cfg Chocolate Doom (пересчёт частоты через
   libsamplerate) - у нас свой пересчёт, переменные только для файла */
int use_libsamplerate = 0;
float libsamplerate_scale = 0.65f;

/* ================================================================
 * Музыка: MUS -> свой синтезатор
 * ================================================================ */

#define NVOICE 24

typedef struct {
    int on;             /* 1 - звучит, 2 - отпущена (затухает) */
    int chan, note;
    unsigned int phase, inc;     /* 32 бита на период */
    int vel;            /* 0..127 */
    int env;            /* 0..65535 */
    int wave;           /* 0 квадрат, 1 треугольник, 2 пила, 3 шум */
    unsigned int noise;
} VOICE;

static VOICE g_voice[NVOICE];

static const unsigned char *g_mus;      /* текущая песня */
static int g_mus_len, g_mus_pos, g_mus_start;
static int g_mus_playing, g_mus_loop;
static int g_mus_wait;                  /* сколько отсчётов до события */
static int g_mus_vol = 100;             /* 0..127 из настроек */
static int g_chan_vol[16], g_chan_instr[16], g_chan_pan[16];
static int g_chan_lastvel[16];

/* частота ноты: 440 * 2^((n-69)/12) -> приращение фазы за отсчёт */
static unsigned int note_inc(int n)
{
    static const double semi[12] = {
        1.0, 1.0594630943592953, 1.122462048309373, 1.189207115002721,
        1.2599210498948732, 1.3348398541700344, 1.4142135623730951, 1.4983070768766815,
        1.5874010519681994, 1.681792830507429, 1.7817974362806785, 1.8877486253633868
    };
    double f = 8.175798915643707 * semi[n % 12] * (double)(1 << (n / 12));
    return (unsigned int)(f * 4294967296.0 / RATE);
}

static void voice_on(int chan, int note, int vel)
{
    int k = -1;

    for (int i = 0; i < NVOICE; i++)
        if (!g_voice[i].on) {
            k = i;
            break;
        }

    /* все заняты - отнять самую тихую отпущенную (или первую) */
    if (k < 0) {
        k = 0;
        for (int i = 0; i < NVOICE; i++)
            if (g_voice[i].on == 2 && g_voice[i].env < g_voice[k].env)
                k = i;
    }

    VOICE *v = &g_voice[k];
    v->on = 1;
    v->chan = chan;
    v->note = note;
    v->vel = vel;
    v->env = 0;
    v->phase = 0;
    v->noise = 0x12345u + (unsigned int)note * 7919u;

    if (chan == 15) {
        v->wave = 3;                       /* барабаны - шум */
        v->inc = 0;
    } else {
        int ins = g_chan_instr[chan];
        v->wave = (ins < 32) ? 1 : (ins < 80) ? 0 : 2;
        v->inc = note_inc(note);
    }
}

static void voice_off(int chan, int note)
{
    for (int i = 0; i < NVOICE; i++)
        if (g_voice[i].on == 1 && g_voice[i].chan == chan && g_voice[i].note == note)
            g_voice[i].on = 2;
}

/* задержка MUS: 7 бит на байт, старший бит - "ещё байт" */
static int mus_delay(void)
{
    int d = 0;

    while (g_mus_pos < g_mus_len) {
        int b = g_mus[g_mus_pos++];
        d = d * 128 + (b & 0x7F);
        if (!(b & 0x80))
            break;
    }

    return d;
}

/* Разобрать события до следующей паузы */
static void mus_step(void)
{
    for (;;) {

        if (g_mus_pos >= g_mus_len) {
            g_mus_playing = 0;
            return;
        }

        int ev = g_mus[g_mus_pos++];
        int type = (ev >> 4) & 7;
        int chan = ev & 15;
        int last = ev & 0x80;

        switch (type) {

        case 0:                                        /* отпустить ноту */
            voice_off(chan, g_mus[g_mus_pos++] & 0x7F);
            break;

        case 1: {                                      /* нота */
            int n = g_mus[g_mus_pos++];
            if (n & 0x80)
                g_chan_lastvel[chan] = g_mus[g_mus_pos++] & 0x7F;
            voice_on(chan, n & 0x7F, g_chan_lastvel[chan]);
            break;
        }

        case 2:                                        /* изгиб высоты - пропустить */
            g_mus_pos++;
            break;

        case 3:                                        /* системное (все ноты прочь) */
            g_mus_pos++;
            break;

        case 4: {                                      /* контроллер */
            int c = g_mus[g_mus_pos++];
            int val = g_mus[g_mus_pos++] & 0x7F;
            if (c == 0)
                g_chan_instr[chan] = val;
            else if (c == 3)
                g_chan_vol[chan] = val;
            else if (c == 4)
                g_chan_pan[chan] = val;
            break;
        }

        case 6:                                        /* конец песни */
            if (g_mus_loop) {
                g_mus_pos = g_mus_start;
            } else {
                g_mus_playing = 0;
                return;
            }
            break;

        default:
            break;
        }

        if (last) {
            /* такт MUS - 1/140 с */
            g_mus_wait += mus_delay() * RATE / 140;
            if (g_mus_wait > 0)
                return;
        }
    }
}

/* Добавить музыку в микс (n кадров) */
static void mus_render(int16_t *out, int n)
{
    for (int f = 0; f < n; f++) {

        if (g_mus_playing) {
            while (g_mus_wait <= 0 && g_mus_playing)
                mus_step();
            g_mus_wait--;
        }

        int l = 0, r = 0;

        for (int i = 0; i < NVOICE; i++) {

            VOICE *v = &g_voice[i];

            if (!v->on)
                continue;

            /* огибающая: атака ~3 мс, дальше держим 70%; отпущена - ~150 мс */
            if (v->on == 1) {
                if (v->env < 65535)
                    v->env += 400;
                if (v->env > 65535)
                    v->env = 65535;
                if (v->wave == 3)
                    v->env -= 40;               /* барабан сам затихает */
            } else {
                v->env -= 12;
            }

            if (v->env <= 0) {
                v->env = 0;
                if (v->on == 2 || v->wave == 3)
                    v->on = 0;
                continue;
            }

            int s;
            v->phase += v->inc;

            switch (v->wave) {
            case 0:  s = (v->phase & 0x80000000u) ? 8000 : -8000; break;
            case 1:  s = (int)((v->phase >> 16) & 0xFFFF);
                     s = (s < 32768 ? s : 65535 - s) * 16000 / 32768 - 8000;
                     break;
            case 2:  s = (int)(v->phase >> 16) * 16000 / 65536 - 8000; break;
            default: v->noise = v->noise * 1103515245u + 12345u;
                     s = (int)((v->noise >> 16) & 0x3FFF) - 8192;
                     break;
            }

            int vol = v->env / 256 * v->vel / 127 * g_chan_vol[v->chan] / 127;   /* 0..255 */
            s = s * vol / 256;

            int pan = g_chan_pan[v->chan];                                      /* 0..127 */
            l += s * (127 - pan / 2) / 127;
            r += s * (64 + pan / 2) / 127;
        }

        l = l * g_mus_vol / 127 / 4;
        r = r * g_mus_vol / 127 / 4;

        int a = out[f * 2] + l, b = out[f * 2 + 1] + r;
        out[f * 2] = (int16_t)(a > 32767 ? 32767 : a < -32768 ? -32768 : a);
        out[f * 2 + 1] = (int16_t)(b > 32767 ? 32767 : b < -32768 ? -32768 : b);
    }
}

/* ================================================================
 * Эффекты
 * ================================================================ */

static void mix_and_write(void)
{
    if (!g_ok)
        return;

    struct myos_audio_info inf;

    if (audio_info(&inf) != 0)
        return;

    if (inf.queued >= AHEAD_BYTES)
        return;

    int frames = (int)(AHEAD_BYTES - inf.queued) / 4;

    if (frames > 4096)
        frames = 4096;

    for (int f = 0; f < frames; f++) {

        int l = 0, r = 0;

        for (int c = 0; c < NCH; c++) {

            SFX_CH *ch = &g_ch[c];

            if (!ch->active)
                continue;

            unsigned int i = ch->pos >> 16;

            if (i >= ch->len) {
                ch->active = 0;
                continue;
            }

            int s = ((int)ch->data[i] - 128) << 8;
            l += s * ch->vl / 254;
            r += s * ch->vr / 254;
            ch->pos += ch->step;
        }

        g_mix[f * 2] = (int16_t)(l > 32767 ? 32767 : l < -32768 ? -32768 : l);
        g_mix[f * 2 + 1] = (int16_t)(r > 32767 ? 32767 : r < -32768 ? -32768 : r);
    }

    mus_render(g_mix, frames);
    audio_write(g_mix, (unsigned long)frames * 4u);
}

static boolean snd_init(boolean use_sfx_prefix)
{
    g_prefix = use_sfx_prefix;

    int r = audio_open();

    if (r != 0) {
        printf("doom: no sound (%s)\n", r == MYOS_EBUSY ? "another program is playing"
                                       : r == MYOS_ENODEV ? "no sound card" : "error");
        return false;
    }

    g_ok = 1;
    for (int i = 0; i < 16; i++) {
        g_chan_vol[i] = 100;
        g_chan_pan[i] = 64;
        g_chan_lastvel[i] = 100;
    }

    printf("doom: sound on (48000 Hz; effects + music synth)\n");
    return true;
}

static void snd_shutdown(void)
{
    if (g_ok)
        audio_close();
    g_ok = 0;
}

static int snd_lump(sfxinfo_t *sfx)
{
    char name[16];

    if (sfx->link != NULL)
        sfx = sfx->link;

    snprintf(name, sizeof(name), g_prefix ? "ds%s" : "%s", sfx->name);
    return W_CheckNumForName(name);
}

static void snd_update(void)
{
    mix_and_write();
}

static void snd_params(int channel, int vol, int sep)
{
    if (channel < 0 || channel >= NCH)
        return;

    /* как в Chocolate Doom: sep 0 - слева, 128 - посередине, 254 - справа */
    g_ch[channel].vl = (254 - sep) * vol / 127;
    g_ch[channel].vr = sep * vol / 127;
    if (g_ch[channel].vl > 254) g_ch[channel].vl = 254;
    if (g_ch[channel].vr > 254) g_ch[channel].vr = 254;
}

static int snd_start(sfxinfo_t *sfx, int channel, int vol, int sep)
{
    if (!g_ok || channel < 0 || channel >= NCH)
        return -1;

    int lump = sfx->lumpnum;

    if (lump < 0)
        lump = snd_lump(sfx);
    if (lump < 0)
        return -1;

    const unsigned char *d = W_CacheLumpNum(lump, PU_STATIC);
    int len = W_LumpLength(lump);

    /* заголовок: формат 3, частота, число отсчётов */
    if (len < 8 || d[0] != 3 || d[1] != 0)
        return -1;

    unsigned int rate = d[2] | d[3] << 8;
    unsigned int n = d[4] | d[5] << 8 | d[6] << 16 | (unsigned int)d[7] << 24;

    if (n > (unsigned int)len - 8)
        n = (unsigned int)len - 8;

    SFX_CH *ch = &g_ch[channel];

    /* 16 байт "подушки" DMX с каждого края - не звук */
    if (n > 32) {
        ch->data = d + 8 + 16;
        ch->len = n - 32;
    } else {
        ch->data = d + 8;
        ch->len = n;
    }

    ch->pos = 0;
    ch->step = (rate ? rate : 11025) * 65536u / RATE;
    ch->active = 1;
    snd_params(channel, vol, sep);

    return channel;
}

static void snd_stop(int channel)
{
    if (channel >= 0 && channel < NCH)
        g_ch[channel].active = 0;
}

static boolean snd_playing(int channel)
{
    return channel >= 0 && channel < NCH && g_ch[channel].active;
}

static void snd_cache(sfxinfo_t *sounds, int num)
{
    (void)sounds;
    (void)num;
}

static snddevice_t g_devs[] = {
    SNDDEVICE_SB, SNDDEVICE_PAS, SNDDEVICE_GUS, SNDDEVICE_WAVEBLASTER,
    SNDDEVICE_SOUNDCANVAS, SNDDEVICE_AWE32,
};

sound_module_t DG_sound_module = {
    g_devs, sizeof(g_devs) / sizeof(g_devs[0]),
    snd_init, snd_shutdown, snd_lump, snd_update, snd_params,
    snd_start, snd_stop, snd_playing, snd_cache,
};

/* ---------------- музыка: модуль ---------------- */

static boolean mus_init(void) { return true; }
static void mus_shutdown(void) { g_mus_playing = 0; }
static void mus_volume(int v) { g_mus_vol = v; }
static void mus_pause(void) { g_mus_playing = 0; }
static void mus_resume(void) { if (g_mus) g_mus_playing = 1; }

static void *mus_register(void *data, int len)
{
    const unsigned char *d = data;

    if (len < 16 || memcmp(d, "MUS\x1a", 4) != 0)
        return NULL;                  /* не MUS (MIDI в doom2 PWAD) - тишина */

    return data;
}

static void mus_unregister(void *h)
{
    if (h == g_mus) {
        g_mus = NULL;
        g_mus_playing = 0;
    }
}

static void mus_play(void *h, boolean looping)
{
    if (h == NULL)
        return;

    const unsigned char *d = h;

    g_mus = d;
    g_mus_len = (d[4] | d[5] << 8) + (d[6] | d[7] << 8);
    g_mus_start = d[6] | d[7] << 8;
    g_mus_pos = g_mus_start;
    g_mus_loop = looping;
    g_mus_wait = 0;
    memset(g_voice, 0, sizeof(g_voice));

    for (int i = 0; i < 16; i++) {
        g_chan_vol[i] = 100;
        g_chan_instr[i] = 0;
        g_chan_pan[i] = 64;
        g_chan_lastvel[i] = 100;
    }

    g_mus_playing = 1;
}

static void mus_stop(void)
{
    g_mus_playing = 0;
    for (int i = 0; i < NVOICE; i++)
        if (g_voice[i].on)
            g_voice[i].on = 2;
}

static boolean mus_is_playing(void) { return g_mus_playing; }

music_module_t DG_music_module = {
    g_devs, sizeof(g_devs) / sizeof(g_devs[0]),
    mus_init, mus_shutdown, mus_volume, mus_pause, mus_resume,
    mus_register, mus_unregister, mus_play, mus_stop, mus_is_playing, NULL,
};
