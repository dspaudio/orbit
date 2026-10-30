/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* SAMPLE: IMA ADPCM sample sets. */
static const char *const N_ONOFF_E[] = {"OFF", "ON"};
/* IMA ADPCM sample sets (tools/gen_samples.py), native rates, zones across
 * the keyboard, loops with the ADPCM state stored at the loop start.
 * SET is P_E0 (EDIT 1, KNOB 1). */
typedef struct {
    uint32_t off, n, ls, le;     /* byte offset, samples, loop start / end (sample index) */
    uint32_t rate;               /* source rate / 44100, Q16 */
    int16_t root16;              /* root pitch in 1/16 semitone */
    int16_t pred;                /* ADPCM state at the loop start */
    uint8_t idx, lo, hi, looped;
} smp_zone_t;
typedef struct {
    const char *name;
    uint16_t z0, nz;
} smp_set_t;
#ifndef ORBIT_BLE
#define ORBIT_BLE 0
#endif
#include "felucca_samples.h"
#if ORBIT_BLE && !defined(SMP_SYNTH)
#error "ORBIT_BLE needs felucca_samples.h generated with ORBIT_BLE=1 (tools/gen_samples.py)"
#elif !ORBIT_BLE && defined(SMP_SYNTH)
#error "felucca_samples.h was generated with ORBIT_BLE=1: it lacks the melodic sets' data for this build"
#endif

static const int16_t IMA_STEP[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97,
    107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428,
    4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350,
    22385, 24623, 27086, 29794, 32767};
static const int8_t IMA_IDX[8] = {-1, -1, -1, -1, 2, 4, 6, 8};
/* 2^(i/192), Q16: pitch ratios in 1/16 semitones, d16 >= -3072 (16 octaves down) */
static uint32_t pow2_q16(int32_t d16)
{
    uint32_t u = (uint32_t)(d16 + 192 * 16), oct = u / 192u;     /* no loop for negative d16 */
    uint32_t r = pitch_inc(1600 + u % 192u) / (pitch_inc(1600) >> 16);   /* 2^(d/192) via the pitch table */
    return oct >= 16u ? r << (oct - 16u) : r >> (16u - oct);
}

/* ---- user sample slots (loaded from the web editor into flash, see web/EDITOR_PROTOCOL.md)
 * 4 slots of 80 KiB: 3 at flash 0xA0000.., USR4 at 0xE7000 (Felucca data regions), read through the plain XIP
 * window. Slot = header (magic, count, name, data length, CRC32) + up to 16 zones in the
 * smp_zone_t layout (off relative to the slot's data at +512) + IMA ADPCM data. */
#include "../hal/fm1_xip.h"   /* relative: hostsim includes this file too */
#define SMP_USER_SLOTS 4
#define SMP_USER_SIZE 0x14000u
#if BOARD_SMK37
/* SMK-37 Pro: only USR1 / USR2 have flash (USRFLASH 0xC2000..0xE9FFF, FL_SMP in fm1_flash.h); USR3 / USR4
 * stay empty: never scanned, their erase / write refused (SMP_USER_HAS). Their offset is a dummy that no
 * flash window accepts. */
#define SMP_USER_FLASH_SLOTS 2u
#define SMP_USER_BASE 0xC2000u                      /* USR1, USR2: 0xC2000..0xE9FFF */
#define SMP_USER_NONE 0xEA000u                      /* USR3 / USR4: no storage */
#define SMP_USER_HAS(k) ((uint32_t)(k) < SMP_USER_FLASH_SLOTS)
#define SMP_USER_OFF(k) (SMP_USER_HAS(k) ? SMP_USER_BASE + (uint32_t)(k) * SMP_USER_SIZE : SMP_USER_NONE)
_Static_assert(SMP_USER_BASE + SMP_USER_FLASH_SLOTS * SMP_USER_SIZE <= 0xEA000u, "SMK: user slots past FL_SMP");
#else
#define SMP_USER_BASE 0xA0000u                      /* USR1..USR3: 0xA0000..0xDBFFF */
#define SMP_USER4_BASE 0xE7000u                     /* USR4 (2.4): the free flash after the FM6 bank, 0xE7000..0xFAFFF */
#define SMP_USER_HAS(k) 1
#define SMP_USER_OFF(k) ((k) < 3u ? SMP_USER_BASE + (k) * SMP_USER_SIZE : SMP_USER4_BASE)
#endif
#define SMP_USER_DATA 512u
#define SMP_USER_MAGIC 0x504D5346u                  /* "FSMP" */
#define SMP_NALL (SMP_NSETS + SMP_USER_SLOTS)
typedef struct {
    uint32_t magic;
    uint16_t version;
    uint8_t nz, rsv;
    char name[8];
    uint32_t data_len, crc, rsv2[2];
    smp_zone_t zone[16];
} smp_user_hdr_t;                                   /* 32 + 16 x 28 = 480 B, data at +512 */
static smp_zone_t usr_zone[SMP_USER_SLOTS][16];     /* RAM copy, off rebased onto SMP_DATA */
static uint8_t usr_nz[SMP_USER_SLOTS];
static const char *const SMP_ALL_NAMES[SMP_NALL] = {SMP_SET_NAMES_INIT, "USR1", "USR2", "USR3", "USR4"};

#ifndef SMP_USER_XIP                                /* host tests: a RAM image of the slots */
#define SMP_USER_XIP(k) fm1_xip_ptr(SMP_USER_OFF((uint32_t)(k)))
#endif
static const uint8_t *smp_user_xip(uint32_t k) { return SMP_USER_XIP(k); }
#if FELUCCA_SLICE
static void slc_user_scan(uint32_t k, int valid);   /* eng_slice.c: SLICE's slice table of the slot */
#else
#define slc_user_scan(k, valid) ((void)0)
#endif
static uint32_t smp_user_gen;                       /* + 1 per slot scan (GRAIN: its seek index follows uploads) */

/* (re)read slot k from flash: valid header -> zones usable; call after boot and after an upload
 * (main loop: SLICE scans the slot's audio here) */
/* a zone of a slot header usable (inside the slot's data, well formed) */
static int smp_zone_ok(const smp_zone_t *z, uint32_t data_len)
{
    return z->off <= data_len && z->n <= 2u * SMP_USER_SIZE && z->off + (z->n + 1u) / 2u <= data_len &&
           z->idx <= 88u && z->rate && z->rate <= 4u << 16 && z->ls <= z->le && z->le < z->n && z->lo <= z->hi;
}
static void smp_user_scan(uint32_t k)
{
    const smp_user_hdr_t *h = (const smp_user_hdr_t *)smp_user_xip(k);
    static smp_zone_t zs[16];
    uint32_t i, base;
    smp_user_gen++;
    usr_nz[k] = 0;                                  /* (from here no new voice takes the slot) */
    slc_user_scan(k, 0);
#if BOARD_SMK37
    if (!SMP_USER_HAS(k))
        return;                                     /* no flash behind this slot */
#endif
    if (h->magic != SMP_USER_MAGIC || h->version != 1 || !h->nz || h->nz > 16u ||
        h->data_len > SMP_USER_SIZE - SMP_USER_DATA)
        return;
    base = (uint32_t)(uintptr_t)(smp_user_xip(k) + SMP_USER_DATA) - (uint32_t)(uintptr_t)SMP_DATA;
    for (i = 0; i < h->nz; i++) {                   /* checked in a copy: a voice still playing the slot */
        zs[i] = h->zone[i];                         /* never reads a zone that is not valid */
        if (!smp_zone_ok(&zs[i], h->data_len))
            return;                                 /* zone outside the data or malformed: slot unusable */
        zs[i].off += base;
    }
    fm1_irq_off();
    for (i = 0; i < h->nz; i++)
        usr_zone[k][i] = zs[i];
    usr_nz[k] = h->nz;
    fm1_irq_on();
    slc_user_scan(k, 1);
}

/* zone index in a voice: < 0x8000 built-in, else 0x8000 | slot << 5 | zone */
static inline const smp_zone_t *smp_zone(uint32_t zi)
{
    return zi < 0x8000u ? &SMP_ZONES[zi] : &usr_zone[(zi >> 5) & 3u][zi & 31u];
}

/* voice: ph[0] position (samples), ph[1] fraction Q16, s[0] predictor, s[1] step
 * index, s[2] previous sample, s[3] current sample, s[4] zone, s[5] (UNISON) output samples before it starts */
static inline int32_t sample_next(const smp_zone_t *z, voice_t *v, int loop)
{
    uint32_t pos = v->ph[0], b = SMP_DATA[z->off + (pos >> 1)];
    uint32_t code = (pos & 1u) ? (b >> 4) : (b & 15u);
    int32_t step = IMA_STEP[(uint32_t)v->s[1] <= 88u ? v->s[1] : 88], vd = step >> 3;
    if (code & 4u)
        vd += step;
    if (code & 2u)
        vd += step >> 1;
    if (code & 1u)
        vd += step >> 2;
    v->s[0] = clamp(v->s[0] + ((code & 8u) ? -vd : vd), -32768, 32767);
    v->s[1] = clamp(v->s[1] + IMA_IDX[code & 7u], 0, 88);
    pos++;
    if (pos > z->le && z->looped && loop) {
        pos = z->ls;
        v->s[0] = z->pred;
        v->s[1] = z->idx;
    }
    v->ph[0] = pos;
    return v->s[0];
}

#if ORBIT_BLE
/* ---- ORBIT_BLE stand-ins: the melodic sets (PIANO..SCRCH) have no data; PERC keeps its samples. A stand-in
 * zone (SMP_ZONES[0 .. SMP_SYN_NZ - 1], tools/gen_samples.py ble_builder) is a virtual source at 22050 Hz
 * that smp_syn computes from the sample index alone: 2-operator phase modulation on the SINE table, its
 * modulation index and level enveloped over the note (stateless: SAMPLE and GRAIN's seeks and reverse grains
 * read it as they read ADPCM, with no decoder state). Zone off = SY_* << 8 | k: the waveform's period is 2^k
 * samples (root 22050 / 2^k Hz, a zone an octave), so pitch, TUNE, the zones, LOOP, BITS, CUT, DRV, the
 * ADSR and velocity work as on samples. Cost: two table sines per source sample (about 0.5 to 0.7 source
 * samples per output sample within a zone) plus the envelopes at control rate (smp_syn_cr), no division. */
/* One row per set (tools/gen_samples.py BLE_SYN: zone off >> 8): how its stand-in renders. A later step may
 * give a set another render kind (another engine) here without touching the zones or the callers. */
enum { SY_PIANO, SY_BASS, SY_VIBES, SY_HORNS, SY_STRGS, SY_FLUTE, SY_SCRCH, SY_N };
enum { SYK_PM, SYK_SWEEP, SYK_PM3 };                 /* 2-op phase modulation; SCRCH's triangle-and-noise sweep; */
                                                     /* SYK_PM plus a second modulator (r2, d2, i2) */
typedef struct {
    uint8_t kind;                /* SYK_* */
    uint8_t ratio;               /* modulator phase << ratio (0: 1 : 1, 2: 1 : 4) */
    uint8_t detune;              /* carrier above the modulator by 2^-detune (0: none; 7: ~13 cents) */
    uint8_t swell;               /* 1: the index follows the level envelope (brass), else its own decay */
    uint8_t ia, aa, ad;          /* SMP_SYN_EXP shifts: index decay, level attack, level decay (0: none) */
    int16_t i0, i1;              /* index: floor + decaying (or swelling) part, Q15 (32767 = pi rad) */
    int16_t a0, a1;              /* level: floor + enveloped part, Q15 */
    int16_t breath, chiff;       /* noise: steady level, + decaying at the attack */
    uint8_t r2, d2;              /* SYK_PM3: the second modulator's ratio to the carrier (integer) and its index
                                  * decay (SMP_SYN_EXP shift) */
    int16_t i2;                  /* SYK_PM3: its index at the note start, Q15 */
} smp_syn_def_t;
static const smp_syn_def_t SMP_SYN_DEF[SY_N] = {
    /*            kind      ratio det swell ia aa ad   i0     i1     a0     a1    breath chiff r2  d2  i2 */
    [SY_PIANO] = {SYK_PM3,   0,   0,  0,    6, 0, 8,  3000, 20000, 11000, 19000,    0,    0, 14, 4, 7000},   /* DX-style EP,
                                                                          * a 14 : 1 tine at the attack (tau 29 ms) */
    [SY_BASS]  = {SYK_PM,    0,   0,  0,    5, 0, 8,  2000, 22000,     0, 29000,    0,    0},   /* plucked FM bass */
    [SY_VIBES] = {SYK_PM,    2,   0,  0,    5, 0, 9,  1500, 14000,     0, 28000,    0,    0},   /* 1 : 4 bell */
    [SY_HORNS] = {SYK_PM,    0,   0,  1,    0, 4, 9,     0, 20000,     0, 28000,    0,    0},   /* brass swell */
    [SY_STRGS] = {SYK_PM,    0,   7,  0,    0, 6, 9,  9000,     0,     0, 29000,    0,    0},   /* detuned, slow */
    [SY_FLUTE] = {SYK_PM,    0,   0,  0,    0, 5, 0,  3000,     0,     0, 26000, 1300, 6000},   /* low index, breath */
    [SY_SCRCH] = {SYK_SWEEP, 0,   0,  0,    0, 0, 0,     0,     0,     0, 27000,    0,    0},   /* noise sweep */
};
static inline int smp_is_syn(const smp_zone_t *z)
{
    return (uintptr_t)z - (uintptr_t)SMP_ZONES < SMP_SYN_NZ * sizeof(smp_zone_t);
}
/* exp(-pos / (40 << s)), Q15, interpolated; 0 past the table */
static inline int32_t syn_exp(uint32_t pos, uint32_t s)
{
    uint32_t i = pos >> s;
    int32_t a;
    if (i >= 256u)
        return 0;
    a = SMP_SYN_EXP[i];
    return a + (((SMP_SYN_EXP[i + 1u] - a) * (int32_t)(pos & ((1u << s) - 1u))) >> s);
}
static inline int32_t syn_noise(uint32_t pos)        /* white noise of the index, +-16384 */
{
    uint32_t x = pos * 0x9E3779B1u;
    x ^= x >> 15;
    x *= 0x85EBCA77u;
    x ^= x >> 13;
    return (int32_t)x >> 17;
}
/* Control rate: a SYK_PM stand-in's level, index and noise envelopes are evaluated at every SYN_CR-th source
 * sample of the note (pe = 0, 16, 32 .., the last point clamped to the loop start) and ramped linearly in
 * between; the per-sample work is two sines and three multiply-adds. Each value is still a function of the
 * zone and the position alone (seeks and reverse grains read it as before). */
#define SYN_CR_LOG2 4u
typedef struct {
    int32_t g, idx, nz, idx2;    /* level (Q15), index (Q15, pi rad), noise level, SYK_PM3's second index */
} syn_ctl_t;
static void syn_ctl(const smp_syn_def_t *d, uint32_t k, uint32_t pe, syn_ctl_t *c)
{
    int32_t env = d->ad ? syn_exp(pe, d->ad) : 32767, idx;
    if (d->aa)
        env = mulq15(env, 32767 - syn_exp(pe, d->aa));
    idx = d->swell ? mulq15(env, d->i1) : d->i0 + (d->ia ? mulq15(syn_exp(pe, d->ia), d->i1) : 0);
    if (k < 5u)
        idx >>= 5u - k;                              /* less index on the shortest periods: no aliasing */
    c->g = d->a0 + mulq15(env, d->a1);
    c->idx = idx;
    c->nz = d->breath ? d->breath + mulq15(syn_exp(pe, 6), d->chiff) : 0;
    c->idx2 = d->r2 && d->r2 < 1u << (k - 1u) ? mulq15(syn_exp(pe, d->d2), d->i2) : 0;   /* below the source's */
}                                                                                     /* Nyquist only */
/* The current control segment (one entry, a pure cache: any hit returns what a recompute would). smp_syn runs
 * in the audio render path only (sample_render, GRAIN's block and render), never nested, so no lock. */
static struct {
    const smp_zone_t *z;         /* 0: empty; a SYK_PM zone only */
    uint32_t seg;                /* pe >> SYN_CR_LOG2 */
    uint32_t ratio, detune, r2;  /* the row's modulator shift, carrier detune shift (32: none), second ratio */
    syn_ctl_t c0, dc;            /* the values at the segment start; end - start */
} smp_syn_cr;
static void smp_syn_seg(const smp_zone_t *z, const smp_syn_def_t *d, uint32_t k, uint32_t seg)
{
    uint32_t e = (seg + 1u) << SYN_CR_LOG2;
    syn_ctl_t c1;
    if (smp_syn_cr.z == z && smp_syn_cr.seg + 1u == seg) {   /* the next segment: its start is the old end */
        smp_syn_cr.c0.g += smp_syn_cr.dc.g;
        smp_syn_cr.c0.idx += smp_syn_cr.dc.idx;
        smp_syn_cr.c0.nz += smp_syn_cr.dc.nz;
        smp_syn_cr.c0.idx2 += smp_syn_cr.dc.idx2;
    } else {
        syn_ctl(d, k, seg << SYN_CR_LOG2, &smp_syn_cr.c0);
    }
    syn_ctl(d, k, e < z->ls ? e : z->ls, &c1);
    smp_syn_cr.dc.g = c1.g - smp_syn_cr.c0.g;
    smp_syn_cr.dc.idx = c1.idx - smp_syn_cr.c0.idx;
    smp_syn_cr.dc.nz = c1.nz - smp_syn_cr.c0.nz;
    smp_syn_cr.dc.idx2 = c1.idx2 - smp_syn_cr.c0.idx2;
    smp_syn_cr.r2 = d->r2;
    smp_syn_cr.ratio = k >= d->ratio + 2u ? d->ratio : 0u;
    smp_syn_cr.detune = d->detune ? 32u - k - d->detune : 32u;
    smp_syn_cr.z = z;
    smp_syn_cr.seg = seg;
}
/* the output of a stand-in: a one-shot's last 1024 samples fade out (fade), no wrap */
static inline int32_t smp_syn_out(const smp_zone_t *z, uint32_t pos, int fade, int32_t x)
{
    if (fade && pos + 1024u > z->n)
        x = (x * (int32_t)((z->n - pos) << 5)) >> 15;
    return clamp(x, -32767, 32767);
}
/* a SYK_PM (SYK_PM3) sample within the current control segment (smp_syn_cr is z's, at pe's segment): period 2^k samples */
static inline int32_t smp_syn_pm(const smp_zone_t *z, uint32_t pos, uint32_t pe, int fade)
{
    uint32_t ph = pos << (32u - (z->off & 15u)), f = pe & ((1u << SYN_CR_LOG2) - 1u);
    int32_t g = smp_syn_cr.c0.g + ((smp_syn_cr.dc.g * (int32_t)f) >> SYN_CR_LOG2);
    int32_t idx = smp_syn_cr.c0.idx + ((smp_syn_cr.dc.idx * (int32_t)f) >> SYN_CR_LOG2);
    int32_t mo = sine_i(ph << smp_syn_cr.ratio) * idx, x;
    if (smp_syn_cr.r2)                               /* SYK_PM3: the second modulator, a plain lookup */
        mo += SINE[(ph * smp_syn_cr.r2) >> 22] * (smp_syn_cr.c0.idx2 + ((smp_syn_cr.dc.idx2 * (int32_t)f) >> SYN_CR_LOG2));
    x = mulq15(sine_i((smp_syn_cr.detune < 32u ? ph + (pos << smp_syn_cr.detune) : ph) + ((uint32_t)mo << 1)), g);
    if (smp_syn_cr.c0.nz)
        x += mulq15(syn_noise(pos), smp_syn_cr.c0.nz + ((smp_syn_cr.dc.nz * (int32_t)f) >> SYN_CR_LOG2));
    return smp_syn_out(z, pos, fade, x);
}
/* SCRCH's sweep: 2048 samples, a triangle 2650 -> 250 -> 2650 Hz plus bright noise */
static inline int32_t smp_syn_sweep(const smp_zone_t *z, const smp_syn_def_t *d, uint32_t pos, int fade)
{
    uint32_t k = z->off & 15u, c = pos < 1024u ? pos : pos - 1024u, ph;
    int32_t x;
    ph = pos * 48695761u + (pos < 1024u ? pos * 467479300u - c * c * 228261u   /* 250 Hz + 2400 Hz |2t - 1| */
                                        : 512u * 467479300u + c * c * 228261u);
    x = (osc_tri(ph) * 21) >> 5;
    x += ((syn_noise(pos + k * 7919u) - syn_noise(pos + k * 7919u - 1u)) * 11) >> 5;   /* bright noise */
    x = mulq15(x, clamp((int32_t)(pos < 1024u ? pos : 2048u - pos) << 5, 0, 32767));
    x = mulq15(x, d->a1);
    return smp_syn_out(z, pos, fade, x);
}
/* the rare part of a SYK_PM (SYK_PM3) stand-in: a new control segment, then its sample */
static int32_t smp_syn_cold(const smp_zone_t *z, const smp_syn_def_t *d, uint32_t pos, uint32_t pe, int fade)
{
    smp_syn_seg(z, d, z->off & 15u, pe >> SYN_CR_LOG2);
    return smp_syn_pm(z, pos, pe, fade);
}
/* sample pos of stand-in zone z (pos < z->n); fade: a one-shot's last 1024 samples fade out */
static inline int32_t smp_syn(const smp_zone_t *z, uint32_t pos, int fade)
{
    uint32_t pe = pos < z->ls ? pos : z->ls;         /* pe: steady from the loop start */
    const smp_syn_def_t *d;
    if (pos >= z->n)
        return 0;
    if (smp_syn_cr.z == z && smp_syn_cr.seg == pe >> SYN_CR_LOG2)
        return smp_syn_pm(z, pos, pe, fade);
    d = &SMP_SYN_DEF[(z->off >> 8) % SY_N];
    if (d->kind == SYK_SWEEP)
        return smp_syn_sweep(z, d, pos, fade);
    return smp_syn_cold(z, d, pos, pe, fade);
}
/* sample_next for a stand-in zone: the same position and loop handling (SAMPLE renders SYK_PM and SYK_PM3
 * stand-ins in smp_syn_render: this is SCRCH's path) */
static inline int32_t smp_syn_next(const smp_zone_t *z, voice_t *v, int loop)
{
    const smp_syn_def_t *d = &SMP_SYN_DEF[(z->off >> 8) % SY_N];
    uint32_t pos = v->ph[0];
    int lp = z->looped && loop;
    v->s[0] = d->kind == SYK_SWEEP && pos < z->n ? smp_syn_sweep(z, d, pos, !lp) : smp_syn(z, pos, !lp);
    pos++;
    if (pos > z->le && lp)
        pos = z->ls;
    v->ph[0] = pos;
    return v->s[0];
}
#define SMP_NEXT(z, v, loop) (smp_is_syn(z) ? smp_syn_next(z, v, loop) : sample_next(z, v, loop))
#else
#define SMP_NEXT(z, v, loop) sample_next(z, v, loop)
#endif

static void sample_note_on(track_t *t, voice_t *v)
{
    uint32_t si = (uint32_t)t->p[P_E0] % SMP_NALL, i, zi = 0xFFFFu;
    if (si < SMP_NSETS) {
        const smp_set_t *set = &SMP_SETS[si];
        for (i = 0; i < set->nz; i++)
            if (v->note >= SMP_ZONES[set->z0 + i].lo && v->note <= SMP_ZONES[set->z0 + i].hi)
                zi = set->z0 + i;
        v->s[4] = (int32_t)(zi == 0xFFFFu ? set->z0 : zi);
    } else {                                        /* user slot: silent if empty */
        uint32_t k = si - SMP_NSETS;
        for (i = 0; i < usr_nz[k]; i++)
            if (v->note >= usr_zone[k][i].lo && v->note <= usr_zone[k][i].hi)
                zi = 0x8000u | k << 5 | i;
        v->s[4] = (int32_t)(zi == 0xFFFFu ? 0 : zi);
    }
    v->ph[0] = 0;
    v->ph[1] = 0;
    v->s[0] = 0;
    v->s[1] = 0;
    v->s[2] = v->s[3] = 0;
    v->s[5] = 0;                 /* no start delay (UNISON: sample_spread) */
    v->s[6] = zi == 0xFFFFu;     /* 1 = sample ended (one-shot); a kit key with no sound stays silent */
    v->s[7] = 0;                 /* lo-pass state */
}

#ifndef SMP_STEP_MAX
#define SMP_STEP_MAX (8u << 16)   /* decodes an output sample at most (a 44.1 kHz zone 3 octaves up: 8, the cost bound as before) */
#endif
/* the playback rate, Q16 source samples an output sample, of pitch16 (+ TUNE) on zone z (ported from SLOOP 2.8.4,
 * isod89/sloop-fm1 91f3018): the pitch range (+-8 octaves) is apart from the decode bound, which is the old one
 * (3 octaves over the root) or SMP_STEP_MAX source samples an output sample if that is more: the 576 clamp on the
 * pitch alone made the keys past 3 octaves over a low-rate zone's root all play one pitch. Per block, not per sample */
static inline uint32_t smp_stepq(const smp_zone_t *z, int32_t d16)
{
    uint32_t stepq = (pow2_q16(clamp(d16, -1536, 1536)) >> 8) * (z->rate >> 8);
    uint32_t lim = (pow2_q16(576) >> 8) * (z->rate >> 8);
#if ORBIT_BLE
    if (!smp_is_syn(z))          /* a stand-in keeps the 3-octave bound: its zones go up an octave apart to E7, so */
#endif                           /* it is reached past ~22 kHz only, and a stand-in decode costs far more than ADPCM */
        lim = lim > SMP_STEP_MAX ? lim : SMP_STEP_MAX;
    return stepq > lim ? lim : stepq;
}

#if ORBIT_BLE
/* SAMPLE on a SYK_PM or SYK_PM3 stand-in: computed at the output rate from the Q16 position (pos, frac) itself, so no
 * source samples, no interpolator and no per-source-sample loop. The position, loop and one-shot end advance
 * as sample_render's do; the phases are accumulators equal to the position's (pos << 16 | frac) << (16 - k).
 * Control rate: the envelopes (and a one-shot's end fade) are evaluated at the block's first and last
 * position and ramped over CTL samples, as amp_at ramps the level. BITS, DRV, CUT and the level as in
 * sample_render (the voice state in locals: out[] may alias v->s[]) */
static inline uint32_t smp_syn_fade(const smp_zone_t *z, uint32_t lpz, uint32_t pos)   /* Q15, 32768 = 1 */
{
    return lpz || pos + 1024u <= z->n ? 32768u : pos >= z->n ? 0u : (z->n - pos) << 5;
}
static void smp_syn_render(const int16_t *p, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m,
                           const smp_zone_t *z, uint32_t stepq)
{
    const smp_syn_def_t *d = &SMP_SYN_DEF[(z->off >> 8) % SY_N];
    uint32_t k = z->off & 15u, pos = v->ph[0], frac = v->ph[1], i;
    uint32_t rs = k >= d->ratio + 2u ? d->ratio : 0u, dt = d->detune ? k + d->detune : 0u;   /* dt <= 16 */
    uint32_t lpz = z->looped && p[P_E3], span = z->le + 1u - z->ls, pe1 = pos + ((frac + CTL * stepq) >> 16);
    uint32_t le = lpz ? z->le : 0xFFFFFFFFu, zn = z->n;
    uint32_t ph = (pos << (32u - k)) + ((frac << 16) >> k), dph = stepq << (16u - k), hspan = span << (32u - k);
    uint32_t pd = 0, dpd = 0, dspan = 0;             /* the detuned carrier's extra phase */
    int32_t bits = p[P_E2], lp = 4000 + ((clamp((p[P_E4] << 8) + m->cutoff, 0, 127 << 8) * 28767) >> 15);
    int32_t drv = p[P_E6], sh = bits / 10, g, x, nz, dg, dx, dnz, y = v->s[7];
    int32_t a0 = m->amp0, da = m->amp1 - m->amp0, ai = 0, nb[CTL], tm[CTL];
    syn_ctl_t c0, c1;
    if (n > CTL)                                      /* (a render is a block of CTL at most, as GRAIN's) */
        n = CTL;
    if (dt) {
        pd = (pos << (32u - dt)) + ((frac << 16) >> dt);
        dpd = stepq << (16u - dt);
        dspan = span << (32u - dt);
    }
    syn_ctl(d, k, pos < z->ls ? pos : z->ls, &c0);
    syn_ctl(d, k, pe1 < z->ls ? pe1 : z->ls, &c1);
    c0.g = (int32_t)(((uint32_t)c0.g * smp_syn_fade(z, lpz, pos)) >> 15);
    c1.g = (int32_t)(((uint32_t)c1.g * smp_syn_fade(z, lpz, pe1)) >> 15);
    g = c0.g << CTL_LOG2;
    x = c0.idx << CTL_LOG2;
    nz = c0.nz << CTL_LOG2;
    dg = c1.g - c0.g;
    dx = c1.idx - c0.idx;
    dnz = c1.nz - c0.nz;
    /* FLUTE's breath: white noise at the output rate seeded by the position, two taps averaged (the soft top the
     * interpolated source-rate noise had), in a pass of its own: inside the loop below, compilers if-convert it
     * and every set pays for it */
    if (d->breath) {
        uint32_t r = (uint32_t)syn_noise(pos) ^ frac;
        int32_t pr = 0;
        for (i = 0; i < n; i++) {
            int32_t a;
            r = r * 1664525u + 1013904223u;
            a = (int32_t)r >> 17;
            nb[i] = mulq15((a + pr) >> 1, nz >> CTL_LOG2);
            pr = a;
            nz += dnz;
        }
    }
    /* SYK_PM3's second modulator (PIANO's tine), its phase r2 times the carrier's, in a pass of its own as the
     * breath: off near the output's Nyquist, and on a loop that is not whole periods (none is: ble_samples_test) */
    if (d->r2) {
        uint32_t ph2 = ph * d->r2, dph2 = dph * d->r2;
        int32_t t2 = c0.idx2 << CTL_LOG2, dt2 = c1.idx2 - c0.idx2;
        if ((lpz && hspan * d->r2) || d->r2 * (dph >> 4) > 0x60000000u >> 4)
            t2 = dt2 = 0;
        for (i = 0; i < n; i++) {
            tm[i] = SINE[ph2 >> 22] * (t2 >> CTL_LOG2);
            ph2 += dph2;
            t2 += dt2;
        }
    }
    for (i = 0; i < n; i++) {
        /* the modulator a plain table lookup (its -55 dB error only moves the index); |s| <= a0 + a1 (+ (breath +
         * chiff) / 2) < 32767 for every row: no clamp (ble_samples_test) */
        int32_t mo = SINE[(ph << rs) >> 22] * (x >> CTL_LOG2), s;
        if (d->r2)
            mo += tm[i];
        s = mulq15(sine_i(ph + pd + ((uint32_t)mo << 1)), g >> CTL_LOG2);
        if (d->breath)
            s += nb[i];
        if (sh)
            s = (s >> sh) << sh;
        if (drv)
            s = softclip(s + (((s >> 2) * (drv * 150)) >> 11));
        y += mulq15(s - y, lp);
        out[i] += mulq15(mulq15(y, a0 + ((ai + ((ai >> 31) & (CTL - 1))) >> CTL_LOG2)), VOICE_FS) << 1;   /* amp_at */
        ai += da;
        g += dg;
        x += dx;
        ph += dph;
        pd += dpd;
        frac += stepq;
        pos += frac >> 16;
        frac &= 0xFFFFu;
        while (pos > le) {
            pos -= span;
            ph -= hspan;
            pd -= dspan;
        }
        if (pos >= zn) {                              /* one-shot reached its end */
            v->s[6] = 1;
            break;
        }
    }
    v->s[7] = y;
    v->ph[0] = pos;
    v->ph[1] = frac;
}
#endif

/* Keep the block-start dispatch separate so its cold exits do not spill the sample loop's state. */
static __attribute__((noinline)) void sample_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    const smp_zone_t *z = smp_zone((uint32_t)v->s[4]);
    uint32_t i, frac = v->ph[1];
    uint32_t stepq = smp_stepq(z, m->pitch16 + p[P_E1] * 16 - z->root16);   /* Q16 samples per output */
    int32_t bits = p[P_E2], lp = 4000 + ((clamp((p[P_E4] << 8) + m->cutoff, 0, 127 << 8) * 28767) >> 15);
    int32_t drv = p[P_E6], sh = bits / 10;
#if ORBIT_BLE
    if (smp_is_syn(z) && SMP_SYN_DEF[(z->off >> 8) % SY_N].kind != SYK_SWEEP) {
        smp_syn_render(p, v, out, n, m, z, stepq);
        return;
    }
#endif
    if (v->ph[0] == 0 && v->ph[1] == 0 && v->s[3] == 0)
        v->s[3] = SMP_NEXT(z, v, p[P_E3]);            /* prime the interpolator */
    for (i = 0; i < n; i++) {
        int32_t s;
        frac += stepq;
        while (frac >= 65536u) {
            frac -= 65536u;
            v->s[2] = v->s[3];
            if (v->ph[0] >= z->n) {                   /* one-shot reached its end */
                v->s[6] = 1;
                v->s[3] = 0;
                break;
            }
            v->s[3] = SMP_NEXT(z, v, p[P_E3]);
        }
        s = v->s[2] + (((v->s[3] - v->s[2]) * (int32_t)(frac >> 1)) >> 15);
        if (sh)                                       /* BITS: 0 = clean, up to 12 bits removed */
            s = (s >> sh) << sh;
        if (drv)
            s = softclip(s + (((s >> 2) * (drv * 150)) >> 11));   /* = s * drv * 600 / 32768, no overflow */
        v->s[7] += mulq15(s - v->s[7], lp);           /* gentle tone control (CUT) */
        out[i] += mulq15(mulq15(v->s[7], amp_at(m, i)), VOICE_FS) << 1;
        if (v->s[6])
            break;
    }
    v->ph[1] = frac;
}

static void sample_voice_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const smp_zone_t *z = smp_zone((uint32_t)v->s[4]);
    vmod_t m0;
    if (v->s[6] || !z->n) {
        v->active = 0;
        return;
    }
    if (v->s[5]) {                                    /* UNISON: silence first, no decoding or filter advance */
        uint32_t i;
        if ((uint32_t)v->s[5] >= n) {
            v->s[5] -= (int32_t)n;
            return;
        }
        i = (uint32_t)v->s[5];
        v->s[5] = 0;
        m0 = *m;
        m0.amp0 = amp_at(m, i);
        m0.amp1 = m0.amp0 + (m->amp1 - m->amp0);
        m = &m0;
        out += i;
        n -= i;
    }
    sample_render(t, v, out, n, m);
}


/* UNISON (voice.c unison_spread; ported from SLOOP 2.8.4, isod89/sloop-fm1 91f3018): the voice starts r (Q32) of
 * four periods of the pitch it sounds later (s[5]: output samples of silence first, nothing decoded): every voice
 * otherwise read the same samples from the same start, in phase (+8 dB over POLY). Four: a sample may sound lower
 * than its key says. At most the time of 256 source samples and of a quarter of the zone. Two 32-bit divides per
 * extra voice at its start, none per sample. The pitch range is the one sample_render reads at */
static void sample_spread(track_t *t, voice_t *v, uint32_t r)
{
    const int16_t *p = t->p;
    const smp_zone_t *z = smp_zone((uint32_t)v->s[4]);
    uint32_t stepq = smp_stepq(z, v->pitch16 + p[P_E1] * 16 - z->root16);   /* Q16 source samples an output sample */
    uint32_t b = pitch_inc((uint32_t)clamp(v->pitch16 + p[P_E1] * 16, 0, 2047));   /* the pitch it sounds (TUNE) */
    uint32_t d, q;
    if (v->s[6] || !z->n || !b || !stepq || v->ph[0] || v->ph[1])
        return;
    d = (0xFFFFFFFFu / b) << 2;                       /* four periods, in output samples */
    q = ((z->n >> 2 < 256u ? z->n >> 2 : 256u) << 16) / stepq;   /* the time of 256 source samples */
    v->s[5] = (int32_t)(((d < q ? d : q) * (r >> 16)) >> 16);   /* scaled by r, not clamped: a clamp gave many */
}                                                                /* voices the same start */

static const engine_t ENG_SAMPLE = {
    "SAMPLE", {"SET", "TONE"},
    {
        {"SET", F_ENUM, 0, SMP_NALL - 1, 0, SMP_ALL_NAMES, 0},
        {"TUNE", F_SEMI, -24, 24, 0, 0, 0},
        {"BITS", F_INT, 0, 127, 0, 0, 0},
        {"LOOP", F_ENUM, 0, 1, 1, N_ONOFF_E, 0},
        {"CUT", F_INT, 0, 127, 127, 0, 0},
        {"-", F_INT, 0, 0, 0, 0, 0},
        {"DRV", F_PCT, 0, 127, 0, 0, 0},
        {"-", F_INT, 0, 0, 0, 0, 0},
    },
    SMP_PRESET_TABLE, sizeof(SMP_PRESET_TABLE) / sizeof(SMP_PRESET_TABLE[0]), -1, sample_note_on, sample_voice_render,
    0xFFFF, {P_E0, P_E4, P_ATK, P_REL},
};
