/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 ORBIT contributors
 * Independently written synthesis, inspired by publicly documented synthesis
 * families. No OP-1 code, patches, samples or parameter mappings are used.
 * The host supplies sine lookup, amplitude ramps and the voice/FX framework.
 * All state fits existing voice_t; no allocation or additional RAM buffers.
 */
static void orbit_osc_note_on(track_t *t, voice_t *v)
{
    uint32_t k;
    (void)t;
    for (k = 0; k < 3; k++) v->ph[k] = k * 0x34567890u;
    for (k = 0; k < 8; k++) v->s[k] = 0;
}

/* A bounded one-pole tone control, Q15. No division in the sample loop. */
static int32_t orbit_tone(int32_t x, int32_t *z, int32_t a)
{
    *z += ((x - *z) * a) >> 15;
    return *z;
}
static int32_t orbit_tone_coef(const track_t *t, const vmod_t *m)
{
    int32_t c = clamp(t->p[P_E4] + (m->cutoff >> 8), 0, 127);
    return 128 + c * c * 2; /* 128..32386; stable even at the extremes */
}
static void orbit_emit(int32_t *out, uint32_t i, int32_t s, const vmod_t *m)
{
    out[i] += mulq15(mulq15(s, amp_at(m, i)), VOICE_FS) * 2;
}

/* SWARM: 1..6 independently detuned sine/harmonic oscillators. It is not a
 * renamed TRIO/ANALOG engine. Spread is a linear frequency offset, not cents.
 * Higher partials are omitted above Nyquist. Envelope can contract the spread.
 */
static void swarm_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    uint32_t i, k, count = (uint32_t)t->p[P_E0], ph[6], inc[6];
    int32_t spread = t->p[P_E1], harmonic = t->p[P_E2] * 128;
    int32_t a = orbit_tone_coef(t, m), z = v->s[6];
    spread = spread * (32767 - t->p[P_E3] * (32767 - m->envq15) / 127) / 32767;
    for (k = 0; k < count; k++) {
        int32_t offset = (int32_t)(2 * k) - (int32_t)count + 1;
        ph[k] = (uint32_t)v->s[k];
        inc[k] = m->inc + (uint32_t)((int32_t)(m->inc >> 16) * spread * offset);
    }
    for (i = 0; i < n; i++) {
        int32_t sum = 0;
        for (k = 0; k < count; k++) {
            int32_t s = sine_i(ph[k]);
            if (inc[k] < 0x40000000u)
                s = mulq15(s, 32767 - harmonic) + mulq15(sine_i(ph[k] * 2u), harmonic);
            sum += s;
            ph[k] += inc[k];
        }
        orbit_emit(out, i, orbit_tone(sum / (int32_t)count, &z, a), m);
    }
    for (k = 0; k < count; k++) v->s[k] = (int32_t)ph[k];
    v->s[6] = z;
}

/* Independent Q15 polynomial step correction for the dual pulse oscillator.
 * Normalised t uses a bounded phase interval; zero increments return zero.
 */
static int32_t orbit_step(uint32_t phase, uint32_t inc)
{
    int32_t x;
    uint32_t d = inc >> 16;
    if (!d) return 0;
    if (phase < inc) {
        x = (int32_t)((phase >> 16) * 32768u / d);
        x = clamp(x, 0, 32767);
        return x * 2 - ((x * x) >> 15) - 32768;
    }
    if (phase > 0xffffffffu - inc) {
        x = (int32_t)(((0xffffffffu - phase) >> 16) * 32768u / d);
        x = clamp(x, 0, 32767);
        return 32768 - x * 2 + ((x * x) >> 15);
    }
    return 0;
}
static int32_t orbit_pulse_wave(uint32_t ph, uint32_t inc, uint32_t width)
{
    /* Remove duty-cycle DC; step correction suppresses edge aliasing. */
    int32_t s = ph < width ? 32767 : -32767;
    s += orbit_step(ph, inc) - orbit_step(ph - width, inc);
    s -= (int32_t)(width >> 16) - 32768;
    return clamp(s, -32767, 32767);
}
static void orbit_pulse_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    uint32_t i, p0 = v->ph[0], p1 = v->ph[1], lp = v->ph[2];
    int32_t z = v->s[0], a = orbit_tone_coef(t, m);
    uint32_t inc = m->inc, inc2 = inc + (inc >> 16) * (uint32_t)t->p[P_E1];
    uint32_t rate = 150000u + (uint32_t)t->p[P_E3] * 18000u;
    int32_t mix = t->p[P_E5] * 258;
    for (i = 0; i < n; i++) {
        int32_t duty = clamp(t->p[P_E0] * 240 + 1024 + mulq15(sine_i(lp), t->p[P_E2] * 100), 2048, 30720);
        uint32_t width = (uint32_t)duty * 131072u;
        int32_t s = mulq15(orbit_pulse_wave(p0, inc, width), 32767 - mix)
                  + mulq15(orbit_pulse_wave(p1, inc2, 0x80000000u), mix);
        orbit_emit(out, i, orbit_tone(s, &z, a), m);
        p0 += inc; p1 += inc2; lp += rate;
    }
    v->ph[0] = p0; v->ph[1] = p1; v->ph[2] = lp; v->s[0] = z;
}

/* FM4: four sine operators, three original routing choices: chain,
 * two parallel pairs, or three modulators feeding one carrier. */
static const char *const ORBIT_FM_ROUTING[] = {"CHAIN", "PAIRS", "FAN"};
static int32_t orbit_fm_op(uint32_t ph, int32_t mod, int32_t depth)
{
    return sine_i(ph + (uint32_t)(mulq15(mod, depth) * 16384));
}
static void orbit_fm4_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    uint32_t i, ph[4] = {v->ph[0], v->ph[1], v->ph[2], (uint32_t)v->s[0]};
    uint32_t inc[4] = {m->inc, 0, 0, 0}, k;
    int32_t fb = v->s[1], z = v->s[2], a = orbit_tone_coef(t, m);
    int32_t depth = t->p[P_E0] * 258;
    depth = mulq15(depth, 32767 - t->p[P_E6] * (32767 - m->envq15) / 127);
    inc[1] = m->inc * (uint32_t)t->p[P_E1];
    inc[2] = m->inc * (uint32_t)t->p[P_E2];
    inc[3] = m->inc * (uint32_t)t->p[P_E3];
    for (i = 0; i < n; i++) {
        int32_t d = orbit_fm_op(ph[3], fb, t->p[P_E7] * 128), b, c, s;
        if (t->p[P_E5] == 0) {
            c = orbit_fm_op(ph[2], d, depth);
            b = orbit_fm_op(ph[1], c, depth);
            s = orbit_fm_op(ph[0], b, depth);
        } else if (t->p[P_E5] == 1) {
            b = orbit_fm_op(ph[1], d, depth);
            c = orbit_fm_op(ph[0], sine_i(ph[2]), depth);
            s = (b + c) / 2;
        } else {
            b = (sine_i(ph[1]) + sine_i(ph[2]) + d) / 3;
            s = orbit_fm_op(ph[0], b, depth);
        }
        fb = d;
        orbit_emit(out, i, orbit_tone(s, &z, a), m);
        for (k = 0; k < 4; k++) ph[k] += inc[k];
    }
    for (k = 0; k < 3; k++) v->ph[k] = ph[k];
    v->s[0] = (int32_t)ph[3]; v->s[1] = fb; v->s[2] = z;
}

static const preset_t SWARM_PRESETS[] = {
    {"ORBIT HAZE", {6, 32, 12, 30, 100, 0, 0, 0}, {35, 85, 110, 75}, 0, 0, FX(0, 20, 15, 35)},
    {"ORBIT GLASS", {3, 9, 75, 90, 127, 0, 0, 0}, {0, 75, 20, 65}, 0, 0, FX(0, 0, 30, 25)},
    {"ORBIT REED", {2, 6, 65, 10, 105, 0, 0, 0}, {6, 70, 95, 28}, 0, 1, FX(0, 8, 12, 10)},
    {"ORBIT CHOIR", {6, 50, 50, 20, 72, 0, 0, 0}, {55, 90, 110, 90}, 0, 0, FX(0, 25, 10, 40)},
};
static const preset_t ORBIT_PULSE_PRESETS[] = {
    {"ORBIT SQUARE", {64, 0, 0, 20, 100, 0, 0, 0}, {0, 75, 100, 25}, 0, 1, FX(0, 0, 0, 0)},
    {"ORBIT PWM", {55, 22, 65, 35, 105, 45, 0, 0}, {12, 80, 100, 55}, 0, 0, FX(0, 15, 20, 25)},
    {"ORBIT PIN", {25, 8, 20, 70, 127, 20, 0, 0}, {0, 55, 0, 35}, 0, 0, FX(0, 0, 22, 15)},
    {"ORBIT HOLLOW", {32, 12, 45, 10, 60, 60, 0, 0}, {8, 80, 95, 40}, 0, 1, FX(0, 8, 10, 10)},
};
static const preset_t ORBIT_FM4_PRESETS[] = {
    {"ORBIT TINES", {45, 1, 3, 7, 127, 1, 100, 0}, {0, 90, 15, 65}, 0, 0, FX(0, 8, 15, 25)},
    {"ORBIT METAL", {100, 2, 5, 7, 127, 0, 90, 20}, {0, 85, 0, 70}, 0, 0, FX(0, 0, 25, 35)},
    {"ORBIT ROUND", {35, 1, 2, 1, 85, 2, 65, 0}, {0, 75, 80, 25}, 0, 1, FX(0, 0, 0, 0)},
    {"ORBIT AURORA", {65, 1, 2, 4, 100, 1, 25, 12}, {45, 90, 100, 80}, 0, 0, FX(0, 20, 18, 40)},
};
#define ORBIT_PARAM(label, lo, hi, def) {label, F_INT, lo, hi, def, 0, 0}
#define ORBIT_UNUSED ORBIT_PARAM("--", 0, 0, 0)
static const engine_t ENG_SWARM = {
    "SWARM", {"OSC", "TONE"}, {
        ORBIT_PARAM("COUNT", 1, 6, 6), ORBIT_PARAM("SPREAD", 0, 100, 32),
        ORBIT_PARAM("PARTIAL", 0, 127, 12), ORBIT_PARAM("ENV", 0, 127, 30),
        {"TONE", F_PCT, 0, 127, 100, 0, 0}, ORBIT_UNUSED, ORBIT_UNUSED, ORBIT_UNUSED},
    SWARM_PRESETS, 4, 1, orbit_osc_note_on, swarm_render, 0x07ff,
    {P_E0, P_E1, P_E2, P_E3}, 4
};
static const engine_t ENG_ORBIT_PULSE = {
    "PULSE", {"OSC", "TONE"}, {
        ORBIT_PARAM("WIDTH", 0, 127, 64), ORBIT_PARAM("DETUNE", 0, 100, 0),
        ORBIT_PARAM("PWM", 0, 127, 0), ORBIT_PARAM("RATE", 0, 127, 20),
        {"TONE", F_PCT, 0, 127, 100, 0, 0}, ORBIT_PARAM("MIX", 0, 127, 0), ORBIT_UNUSED, ORBIT_UNUSED},
    ORBIT_PULSE_PRESETS, 4, 1, orbit_osc_note_on, orbit_pulse_render, 0x07e0,
    {P_E0, P_E1, P_E2, P_E3}, 4
};
static const engine_t ENG_FM4 = {
    "FM4", {"OPS", "ROUTE"}, {
        ORBIT_PARAM("DEPTH", 0, 127, 45), ORBIT_PARAM("RATIO2", 1, 8, 1),
        ORBIT_PARAM("RATIO3", 1, 8, 3), ORBIT_PARAM("RATIO4", 1, 8, 7),
        {"TONE", F_PCT, 0, 127, 127, 0, 0}, {"ROUTE", F_ENUM, 0, 2, 1, ORBIT_FM_ROUTING, 0},
        ORBIT_PARAM("ENV", 0, 127, 100), ORBIT_PARAM("FEEDBACK", 0, 127, 0)},
    ORBIT_FM4_PRESETS, 4, 1, orbit_osc_note_on, orbit_fm4_render, 0xfd20,
    {P_E0, P_E1, P_E2, P_E3}, 4
};
#undef ORBIT_PARAM
#undef ORBIT_UNUSED
