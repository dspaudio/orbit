/* SPDX-License-Identifier: GPL-3.0-only
 * Source: official #246 Cls 0x01960248..0x019608e0 and table initialization.
 * Addresses, hashes, verification scope and original rights: assets/op1-cluster/README.md.
 */
#include "op1_cluster_core.h"

/* Interpret signed bits and arithmetic shifts without implementation-defined behavior or overflow. */
static int32_t cls_s16(uint32_t x) {
    x &= 65535u;
    return x < 32768u ? (int32_t)x : (int32_t)x - 65536;
}
static int64_t cls_s32(uint32_t x) {
    return x < UINT32_C(0x80000000) ? (int64_t)x : (int64_t)x - INT64_C(4294967296);
}
static int64_t cls_asr(int64_t x, unsigned n) {
    int64_t d = INT64_C(1) << n;
    return x >= 0 ? x / d : -1 - (-(x + 1) / d);
}
static int16_t cls_sat16(int64_t x) {
    return (int16_t)(x > 32767 ? 32767 : x < -32768 ? -32768 : x);
}
static uint32_t cls_sat32(int64_t x) {
    return (uint32_t)(x > INT32_MAX ? INT32_MAX : x < INT32_MIN ? INT32_MIN : x);
}
static int16_t cls_ns16(int32_t x) { return (int16_t)cls_s16((uint32_t)x); }
/* Default fractional multiply with unbiased ties-to-even and halfword saturation. */
static int16_t cls_mul(int16_t a, int16_t b) {
    int64_t p = (int64_t)a * b * 2;
    if (p == INT64_C(2147483648)) p = INT32_MAX;
    int64_t h = cls_asr(p, 16);
    uint32_t l = (uint32_t)p & 65535u;
    if (l > 32768u || (l == 32768u && h % 2 != 0)) ++h;
    return cls_sat16(h);
}
static uint32_t cls_get32(const cls_voice *v, unsigned o) {
    const uint8_t *p = v->bytes + o;
    return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static int16_t cls_get16(const cls_voice *v, unsigned o) {
    return (int16_t)cls_s16(v->bytes[o] | (uint32_t)v->bytes[o+1] << 8);
}
static void cls_put16(cls_voice *v, unsigned o, int16_t x) {
    uint32_t u = (uint16_t)x;
    v->bytes[o] = (uint8_t)u; v->bytes[o+1] = (uint8_t)(u >> 8);
}
static void cls_put32(cls_voice *v, unsigned o, uint32_t x) {
    for (unsigned i = 0; i < 4; ++i) v->bytes[o+i] = (uint8_t)(x >> (8*i));
}
static uint32_t cls_next(uint32_t s) {
    return s * UINT32_C(0x0019660d) + UINT32_C(0x3c6ef35f);
}
static int32_t cls_q15(uint32_t b) {
    unsigned e = b >> 23 & 255u;
    if (e >= 127) return b >> 31 ? -32768 : 32767;
    if (e < 112) return 0;
    uint32_t m = ((b | UINT32_C(0x00800000)) << 8) >> (143-e);
    return b >> 31 ? -(int32_t)m : (int32_t)m;
}
static int32_t cls_q31(uint32_t b) {
    unsigned e = b >> 23 & 255u;
    if (e >= 127) return b >> 31 ? INT32_MIN : INT32_MAX;
    if (!e) return 0;
    uint32_t m = (b & UINT32_C(0x007fffff)) | UINT32_C(0x00800000);
    int n = (int)e - 119;
    m = n >= 0 ? m << n : n <= -32 ? 0 : m >> -n;
    return b >> 31 ? -(int32_t)m : (int32_t)m;
}
void cls_tables_init(cls_tables *t, const uint32_t w[8193],
                     const uint32_t c[8], const uint32_t f[24]) {
    for (unsigned i = 0; i < 8193; ++i) t->wave[i] = (int16_t)cls_s16(w[i]);
    for (unsigned i = 0; i < 8; ++i) t->count[i] = (int16_t)cls_q15(c[i]);
    for (unsigned i = 0; i < 24; ++i) t->curve[i] = cls_q31(f[i]);
}
void cls_global_init(cls_global *g) {
    g->master = 0x4000;
    for (unsigned i = 0; i < 8; ++i) g->gain[i] = g->control[i] = 0;
    /* Original 0x019608e0 does not initialize the shared PRNG. */
}
void cls_construct(cls_voice *v) {
    cls_put32(v, 0, UINT32_C(0x0190f67c));
    for (unsigned i = 0; i < 8; ++i) {
        cls_put32(v, 4+8*i, UINT32_C(0x80000000));
        cls_put16(v, 8+8*i, 0); cls_put16(v, 0x44+2*i, 0);
        cls_put16(v, 0x5a+2*i, 0);
    }
    cls_put32(v, 0x54, 0); cls_put16(v, 0x58, 0); v->bytes[0x6a] = 0;
}
void cls_note_init(cls_voice *v, cls_global *g, const cls_tables *t, const int16_t k[8]) {
    int count = (int)cls_asr(k[0], 11);
    for (unsigned i = 0; i < 8; ++i) {
        g->seed = cls_next(g->seed);
        int16_t r = cls_sat16(cls_s16(g->seed >> 16) - 16384);
        cls_put16(v, 0x44+2*i, cls_mul(r, 0x028f));
        cls_put32(v, 4+8*i, UINT32_C(0x80000000));
        cls_put16(v, 8+8*i, 0);
        cls_put16(v, 0x5a+2*i, (int)i < count ? 32767 : 0);
    }
    cls_put16(v, 0x54, 0); cls_put16(v, 0x56, 32767);
    cls_put16(v, 0x58, t->count[count-1]); v->bytes[0x6a] = 0;
}
void cls_release(cls_voice *v) {
    cls_put16(v, 0x56, 0x0100); v->bytes[0x6a] = 1;
}
void cls_decay(cls_global *g) {
    for (unsigned i = 0; i < 8; ++i) g->control[i] = cls_mul(g->control[i], 0x7f00);
}
/* 0x0196042a..52 / 0x019604f8..512.
 * Both paths shift the control word by 16, making its low half exactly zero.
 * FU low*low >>16 = 0; the first mixed cross term is zero.
 * A0=2*signed(high phase)*signed(control), A1=signed(control)*unsigned(low phase).
 * |A1| <= 2^31, |A0| <= 2^31: neither reaches 40-bit saturation.
 * Preserve signed32 saturating extraction after arithmetic A1 >>15 and A0+=A1.
 */
static uint32_t cls_detune(uint32_t phase, int16_t control) {
    int64_t a1 = (int64_t)control * (phase & 65535u);
    int64_t a0 = (int64_t)cls_s16(phase >> 16) * control * 2;
    a1 = cls_asr(a1, 15);
    return cls_sat32(a0 + a1);
}
static unsigned cls_index_for(uint32_t phase, int16_t previous, int16_t feedback) {
    int16_t sum = cls_ns16((int32_t)cls_mul(feedback, previous) + cls_s16(phase >> 16));
    return (unsigned)((int32_t)cls_asr(sum, 3) + 4096);
}
uint32_t cls_render(cls_voice *v, cls_global *g, const cls_tables *t, int16_t out[128],
                    const uint32_t phase[129], const int16_t k[8]) {
    /* 0x01960296..b8 and 0x01960614..62: raw piecewise coefficient. */
    int16_t u = cls_mul(k[1], 0x5000), hi, lo;
    if (u < 0x1000) { hi = -32768; lo = cls_sat16((int32_t)u * 8); }
    else if (u < 0x3000) { hi = cls_sat16(((int32_t)u - 0x2000)*8); lo = 32767; }
    else if (u < 0x4000) { hi = 32767; lo = cls_sat16(32767-cls_sat16(((int32_t)u-0x3000)*8)); }
    else { hi = cls_sat16(32767-cls_sat16(((int32_t)u-0x4000)*8)); lo = 0; }
    /* 0x019602c6..3c2: eight smoothing states and original gain scratch. */
    int count = (int)cls_asr(k[0], 11);
    int16_t norm = cls_sat16((int32_t)cls_mul(cls_get16(v,0x58),0x7f5c) + cls_mul(t->count[count-1],0x00a3));
    cls_put16(v,0x58,norm);
    hi = cls_mul(cls_mul(cls_mul(hi,hi),hi),0x01a3);
    for (unsigned j = 0; j < 8; ++j) {
        int16_t gain = cls_sat16((int32_t)cls_mul(cls_get16(v,0x5a+2*j),0x7f5c) +
                            cls_mul((int)j < count ? 32767 : 0,0x00a3));
        cls_put16(v,0x5a+2*j,gain);
        g->gain[j] = cls_mul(gain,norm);
    }
    /* 0x019603c4..418: NS halfword addition wraps before shifting. */
    int released = v->bytes[0x6a] != 0;
    int16_t strength = cls_mul(k[3],0x4000);
    if (released) strength = cls_mul(strength,0x1000);
    int16_t history = cls_sat16((int32_t)hi + cls_get16(v,0x54));
    cls_put16(v,0x54,history);
    int16_t feedback = cls_mul(cls_sat16((int32_t)lo + history), released ? 0x3000 : 0x4000);
    /* Preserve 0x01960466 LC0=127 plus the last sample, and LC1=7.
     * The first lookup uses previous-block history; subsequent lookups use this waveform.
     * The lookup after phase update and the output refer to different samples.
     */
    for (unsigned j = 0; j < 8; ++j) {
        uint32_t p = cls_get32(v,4+8*j);
        int16_t prev = cls_get16(v,8+8*j);
        int16_t wave = t->wave[cls_index_for(p,prev,feedback)];
        int16_t c = j ? cls_get16(v,0x44+2*j) : cls_get16(v,0x44);
        uint32_t delta = cls_detune(phase[j],c);
        p += cls_sat32(cls_s32(delta) + cls_s32(phase[0]));
        for (unsigned n = 0; n < 127; ++n) {
            unsigned ix = cls_index_for(p,wave,feedback);
            int16_t sample = cls_mul(wave,g->gain[j]);
            out[n] = j ? cls_sat16((int32_t)out[n] + sample) : sample;
            p += cls_sat32(cls_s32(delta) + cls_s32(phase[n+1]));
            wave = t->wave[ix];
        }
        int16_t sample = cls_mul(wave,g->gain[j]);
        out[127] = j ? cls_sat16((int32_t)out[127] + sample) : sample;
        cls_put32(v,4+8*j,p); cls_put16(v,8+8*j,wave);
    }
    /* 0x01960574..60c: eight control updates; packet store uses the previous R0.low. */
    int16_t curve = (int16_t)cls_s16((uint32_t)t->curve[(unsigned)cls_asr(k[2],10)] >> 16);
    cls_put16(v,0x56,cls_mul(cls_get16(v,0x56),curve));
    int16_t phase_strength = cls_sat16((int32_t)feedback + 0x2000);
    for (unsigned j = 0; j < 8; ++j) {
        g->seed = cls_next(g->seed);
        int16_t c = cls_sat16((int32_t)cls_mul(cls_get16(v,0x44+2*j),curve) +
                         cls_mul((int16_t)cls_s16(g->seed >> 16),strength));
        cls_put16(v,0x44+2*j,c);
        int16_t combined = cls_sat16((int32_t)c + cls_mul((int16_t)cls_s16(cls_get32(v,4+8*j) >> 16),phase_strength));
        g->control[j] = cls_mul(combined,cls_get16(v,0x5a+2*j));
    }
    /* 0x019605a2 reads the whole R0 as curve; subsequent instructions update only low16.
     * The final product at 0x01960606 is control[7]; high16 survives through RTS. */
    return ((uint32_t)(uint16_t)curve << 16) | (uint16_t)g->control[7];
}
