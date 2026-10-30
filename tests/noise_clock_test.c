/* SPDX-License-Identifier: GPL-3.0-only */
/* NOISE in LFSR mode (ported from SLOOP 2.8.4, isod89/sloop-fm1 91f3018, tests/noise_clock_test.c; clipper-fm1 #10).
 *
 * 1. The register clock follows the keys past the sample rate. NZ ARCADE (CLK +84 semitones) played the same pitch
 *    on the whole upper half of the keyboard: the clock stopped at one register step a sample. Now a sample takes
 *    as many steps as clocks passed (at most 64): the register's period (127 steps, the 7-bit register of NZ ARCADE)
 *    gives a fundamental of clock / 127 that keeps rising. For each note the spectrum at that fundamental stands out
 *    of the rest, and no two notes give the same samples.
 * 2. The colour poles stay bounded (ORBIT addition for upstream's b0 / b1 / brown clamps): a slow LFSR held at one
 *    level for a long run fed the pink and brown leaky poles a constant input, whose steady state is far past
 *    2^31 / 32690, the range of their next 32-bit multiplication. The sanitizer runs exclude signed overflow, so
 *    the state itself is checked after every block.
 * Exit status: failed checks. */
#define main hostsim_main
#include "hostsim.c"
#undef main
#define NB 1200u                                      /* blocks a note (~0.9 s) */
static int32_t cap[24][NB * CTL];
static int fails;
static void ok(int c, const char *what)
{
    printf("noise: %-96s %s\n", what, c ? "ok" : "FAIL");
    fails += !c;
}
/* the magnitude of the DFT of x[0..n) at f Hz (Hann window) */
static double mag_at(const int32_t *x, uint32_t n, double f)
{
    double re = 0, im = 0;
    uint32_t i;
    for (i = 0; i < n; i++) {
        double w = 0.5 - 0.5 * cos(2 * M_PI * i / n), a = 2 * M_PI * f * i / FS;
        re += x[i] * w * cos(a);
        im -= x[i] * w * sin(a);
    }
    return sqrt(re * re + im * im);
}
static int32_t iabs(int32_t x)
{
    return x < 0 ? -x : x;
}
int main(void)
{
    uint32_t e, pi = 0, k, b, nn = 0, same = 0, weak = 0;
    int32_t peak = 0;
    static int32_t o[CTL * 2];
    const engine_t *en = ENGINES[ENGI_NOISE];
    host_tracks_init();
    for (k = 0; k < en->npresets; k++)
        if (!strcmp(en->presets[k].name, "NZ ARCADE"))
            pi = k;
    ok(!strcmp(en->presets[pi].name, "NZ ARCADE"), "NZ ARCADE found");
    e = ENGI_NOISE;
    host_preset(&trk[0], e, pi);
    trk[0].p[P_VOICE] = V_POLY;
    for (k = 36; k <= 102; k += 3, nn++) {
        trk_all_off(&trk[0]);
        for (b = 0; b < 400u; b++)
            mix_block(o, CTL);
        trk_note_on(&trk[0], k, 100);
        for (b = 0; b < NB; b++) {
            uint32_t i;
            mix_block(o, CTL);
            for (i = 0; i < CTL; i++)
                cap[nn][b * CTL + i] = o[2 * i];
        }
        trk_note_off(&trk[0], k);
    }
    for (k = 0; k < nn; k++) {
        uint32_t note = 36 + 3 * k, n = 16384, a = 4000, j;
        double clock = 440.0 * pow(2.0, (note + en->presets[pi].e[7] - 69) / 12.0), f0 = clock / 127.0, m, med;
        static double s[64];
        if (k + 1 < nn && !memcmp(&cap[k][a], &cap[k + 1][a], 4096u * 4u))
            same++;
        if (f0 > 10000)
            continue;
        m = 0;
        for (j = 0; j < 7; j++) {                       /* the strongest bin around f0 (+-3 bins) */
            double v = mag_at(&cap[k][a], n, f0 + (j - 3.0) * FS / n);
            if (v > m)
                m = v;
        }
        for (j = 0; j < 64; j++)                        /* a sample of the spectrum: 200 Hz .. 10 kHz */
            s[j] = mag_at(&cap[k][a], n, 200.0 + j * 153.1);
        for (j = 1; j < 64; j++) {                      /* (sorted: the median) */
            double v = s[j];
            uint32_t q = j;
            while (q && s[q - 1] > v) { s[q] = s[q - 1]; q--; }
            s[q] = v;
        }
        med = s[32];
        printf("noise: NZ ARCADE note %3u: clock %8.0f Hz, fundamental %6.0f Hz, %5.1f dB over the median\n", note, clock, f0,
               20 * log10(m / (med + 1e-9)));
        if (20 * log10(m / (med + 1e-9)) < 6)
            weak++;
    }
    ok(same == 0, "NZ ARCADE: no two notes (3 semitones apart, C2..F#7) give the same samples");
    ok(weak == 0, "NZ ARCADE: each note's fundamental (clock / 127) stands 6 dB over the spectrum's median");

    /* ORBIT adaptation of the check above: ORBIT seeds the register from the note and the FX tails of the previous
     * note reach the mix, so the mix differs even when the clock does not. Here every note starts from one seed and
     * the register itself is recorded after each block: only the clock can tell two notes apart. */
    same = 0;
    for (k = 36, nn = 0; k <= 102; k += 3, nn++) {
        uint32_t i;
        trk_all_off(&trk[0]);
        for (b = 0; b < 400u; b++)
            mix_block(o, CTL);
        trk_note_on(&trk[0], k, 100);
        for (i = 0; i < NVOICE; i++)
            if (trk[0].v[i].active)
                trk[0].v[i].s[2] = 1;
        for (b = 0; b < 256u; b++) {
            mix_block(o, CTL);
            cap[nn][b] = 0;
            for (i = 0; i < NVOICE; i++)
                if (trk[0].v[i].active)
                    cap[nn][b] = (int32_t)trk[0].v[i].ph[1];
        }
        trk_note_off(&trk[0], k);
        if (nn && !memcmp(cap[nn - 1], cap[nn], 256u * 4u))
            same++;
    }
    ok(same == 0, "NZ ARCADE, one register seed: no two notes (3 semitones apart) step the register alike");

    /* 2. a slow, brown-coloured LFSR (CLK 0, low key): long runs of one level into the colour poles */
    trk_all_off(&trk[0]);
    host_preset(&trk[0], e, pi);
    trk[0].p[P_VOICE] = V_POLY;
    trk[0].p[P_E1] = 127;                              /* COLR: brown (b0, b1 and the brown pole all run) */
    trk[0].p[P_E7] = 0;                                /* CLK: at the key */
    for (b = 0; b < 400u; b++)
        mix_block(o, CTL);
    trk_note_on(&trk[0], 24, 100);
    for (b = 0; b < 4000u; b++) {                      /* ~3 s */
        uint32_t i;
        mix_block(o, CTL);
        for (i = 0; i < NVOICE; i++) {
            const voice_t *v = &trk[0].v[i];
            if (!v->active)
                continue;
            if (iabs(v->s[4]) > peak) peak = iabs(v->s[4]);
            if (iabs(v->s[5]) > peak) peak = iabs(v->s[5]);
            if (iabs(v->s[7]) > peak) peak = iabs(v->s[7]);
        }
    }
    trk_note_off(&trk[0], 24);
    printf("noise: slow brown LFSR: largest colour-pole state %d\n", (int)peak);
    ok(peak > 0 && peak <= 65535, "NOISE colour poles (b0, b1, brown) stay within +-65535 on a held LFSR level");
    printf("noise: %d failed\n", fails);
    return fails;
}
