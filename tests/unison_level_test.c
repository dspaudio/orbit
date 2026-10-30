/* SPDX-License-Identifier: GPL-3.0-only */
/* Ported from SLOOP 2.8.4, isod89/sloop-fm1 91f3018 (tests/unison_level_test.c), adapted to ORBIT.
 *
 * VOICE = UNISON about as loud as POLY, on every engine. UNISON starts its extra voices at random phases
 * (voice.c unison_spread) and at a lower level (the velocity: about the same sum). In phase they stack: 8 voices
 * at 0.36 are +9 dB over one. Spreading ph[0] alone left every other oscillator (ph[1], ph[2], operators, a
 * sample's read position, the engines' own phase arrays) in phase, so those voices stacked.
 *
 * For every preset of every engine (ORBIT's NENGINES, ORBIT-only engines included), notes 36 and 60 (velocity
 * 100; arp, chorus, delay and reverb off): the rms of the mix over 0.05 .. 0.55 s in UNISON against POLY.
 * Passes:
 *   A  the preset's DETUNE;
 *   B  DETUNE 0 (no detune decorrelates the voices: only the start spread does);
 *   C  SAMPLE only: a note released and pressed again while its voices still sound, so every voice restarts
 *      after its fade (voice.c track_render's pending restart, VP_UNI) instead of at note-on.
 * Fails when an engine's mean (per pass) is over +5 dB, or (passes A and C) a render is more than +8 dB over POLY.
 * B has no per-render bound: at one frequency the voices sum to one sine whose level, for any random start phases,
 * is Rayleigh-distributed (LOFI GB BASS note 60 measured -9.7 .. +3.9 dB over 24 seeds), so a single render's
 * level is chance; coherent stacking still shows in the engine's mean (+5.5 .. +8.4 dB before the fix).
 * Taps: ORBIT's mix_l / mix_r (only part 0 plays). Exit status: the number of failed checks. */
#define main hostsim_main
#include "hostsim.c"
#undef main

enum { PASS_PRESET, PASS_DET0, PASS_RESTART, NPASS };
static const char *const PASSN[NPASS] = {"DETUNE", "DET0", "RESTART"};
static int32_t o[CTL * 2];

static void settle(void)
{
    uint32_t i, b, k, any = 1;
    for (k = 0; k < NPART; k++)
        trk_all_off(&trk[k]);
    for (b = 0; b < 6000u && any; b++) {             /* the last note rung out */
        mix_block(o, CTL);
        for (any = 0, i = 0; i < NVOICE; i++)
            any |= trk[0].v[i].active;
    }
}

static double play(uint32_t e, uint32_t p, uint32_t mode, uint32_t note, uint32_t pass)
{
    uint32_t i, b;
    double s2 = 0;
    settle();
    for (i = 0; i < P_E0; i++)                       /* the preset as ui.c apply_preset_to loads it */
        trk[0].p[i] = TP[i].def;
    host_preset(&trk[0], e, p);
    trk[0].p[P_AMODE] = 0, trk[0].p[P_DLY] = trk[0].p[P_REV] = 0, trk[0].p[P_CHOR] = 0;
    trk[0].p[P_VOICE] = (int16_t)mode;
    if (pass == PASS_DET0)
        trk[0].p[P_DETUNE] = 0;
    for (b = 0; b < 16; b++)
        mix_block(o, CTL);
    trk_note_on(&trk[0], note, 100);
    if (pass == PASS_RESTART) {                      /* held, let go, pressed again within the release */
        for (b = 0; b < 200; b++)
            mix_block(o, CTL);
        trk_note_off(&trk[0], note);
        for (b = 0; b < 4; b++)
            mix_block(o, CTL);
        trk_note_on(&trk[0], note, 100);
    }
    for (b = 0; b < 760; b++) {
        mix_block(o, CTL);
        if (b >= 69)
            for (i = 0; i < CTL; i++) {
                double x = (mix_l[i] + mix_r[i]) / 2.0;
                s2 += x * x;
            }
    }
    trk_note_off(&trk[0], note);
    return sqrt(s2 / ((760 - 69) * CTL));
}

/* engine e, one pass: the mean unison / POLY ratio (dB) of its presets at notes 36 and 60 */
static void engine(uint32_t e, uint32_t pass, int *fails)
{
    const engine_t *en = ENGINES[e];
    uint32_t p, k, n = 0;
    double sum[2] = {0, 0}, worst = -99, mean;
    const char *wn = "";
    for (p = 0; p < en->npresets; p++, n++)
        for (k = 0; k < 2u; k++) {
            uint32_t note = k ? 60u : 36u;
            double r0 = play(e, p, V_POLY, note, pass);
            double d = 20 * log10((play(e, p, V_UNISON, note, pass) + 1) / (r0 + 1));
            if (d > 8.0 && pass != PASS_DET0) {
                printf("unison: %-7s %-7s %-14s note %u: %+.1f dB over POLY  FAIL\n", en->name, PASSN[pass],
                       en->presets[p].name, note, d);
                ++*fails;
            }
            if (d > worst)
                worst = d, wn = en->presets[p].name;
            sum[k] += d;
        }
    if (!n)
        return;
    mean = (sum[0] + sum[1]) / (2 * n);
    printf("unison: %-7s %-7s mean %+5.1f dB over POLY (note 36 %+5.1f, 60 %+5.1f; %3u presets; the most %+5.1f, "
           "%s)%s\n", en->name, PASSN[pass], mean, sum[0] / n, sum[1] / n, n, worst, wn, mean > 5.0 ? "  FAIL" : "");
    if (mean > 5.0)
        ++*fails;
}

int main(void)
{
    int fails = 0;
    uint32_t e;
    host_tracks_init();
    for (e = 0; e < NENGINES; e++) {
        engine(e, PASS_PRESET, &fails);
        engine(e, PASS_DET0, &fails);
        if (ENGINES[e] == &ENG_SAMPLE)
            engine(e, PASS_RESTART, &fails);
    }
    printf("unison: %d failed\n", fails);
    return fails;
}
