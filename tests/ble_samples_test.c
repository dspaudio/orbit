/* SPDX-License-Identifier: GPL-3.0-only */
/* ORBIT_BLE: the melodic sets (PIANO..SCRCH) are synthesized FM stand-ins without data (eng_sample.c smp_syn,
 * tools/gen_samples.py ble_builder); PERC keeps its samples. Built with -DORBIT_BLE=1 against a
 * felucca_samples.h generated with ORBIT_BLE=1:
 *   SMP_DATA is PERC's 86144 B exactly (the sample build's bytes, offsets from 0); every set name, index and
 *   preset as before
 *   a melodic set: audible, unclipped, its pitch follows the note (autocorrelation, an octave apart), note-off
 *   decays to silence; SCRCH: its length follows the pitch
 *   PERC: GM 36 / 38 / 42 / 46 / 49 play their samples, distinct, on the drum track (ACOUSTIC..DUST) and in
 *   SAMPLE's GM KIT
 *   GRAIN over every built-in set (reverse grains too): no out-of-bounds read (build with ASan / UBSan)
 *   USR1..USR4 user slots: ADPCM from their flash image, as before (SAMPLE, GRAIN, a user drum kit) */
#include <stdint.h>
static uint32_t host_slots[4u * 0x14000u / 4u];          /* USR1..4: a RAM image of their flash */
#define SMP_USER_XIP(k) ((const uint8_t *)host_slots + (k) * SMP_USER_SIZE)
#define main hostsim_main
#include "hostsim.c"
#undef main

#if !ORBIT_BLE
#error "build with -DORBIT_BLE=1 and an ORBIT_BLE=1 felucca_samples.h"
#endif
void ble_midi_out_pkt(uint32_t pkt) { (void)pkt; }    /* (firmware/src/ble: not in the host build) */

static int fails;
static void check(int ok, const char *what)
{
    printf("ble_samples: %-84s %s\n", what, ok ? "ok" : "FAIL");
    fails += !ok;
}

#define NBUF 16384u
static int32_t buf[NBUF];

/* engine e (preset pi, SET / SRC src) plays note for nb blocks straight through its render: into buf (if
 * room), -> samples rendered; *ended: the voice ended (a one-shot) */
static uint32_t render_eng(uint32_t e, uint32_t pi, uint32_t src, uint32_t note, uint32_t nb, int *ended)
{
    track_t *t = &trk[0];
    voice_t *v = &t->v[0];
    const engine_t *eng = ENGINES[e];
    vmod_t m = {0};
    uint32_t b, i, n = 0;
    memset(t, 0, sizeof *t);
    host_preset(t, e, pi);
    t->p[P_E0] = (int16_t)src;
    v->note = (uint8_t)note;
    v->vel = 127;
    v->active = v->gate = 1;
    m.pitch16 = (int32_t)note * 16;
    m.inc = pitch_inc(m.pitch16);
    m.amp0 = m.amp1 = m.envq15 = 32767;
    eng->note_on(t, v);
    *ended = 0;
    for (b = 0; b < nb; b++) {
        int32_t out[CTL] = {0};
        if (eng->block)
            eng->block(t);
        if (!v->active) {
            *ended = 1;
            break;
        }
        eng->render(t, v, out, CTL, &m);
        for (i = 0; i < CTL; i++, n++)
            if (n < NBUF)
                buf[n] = out[i];
    }
    return n;
}

static int32_t peak_of(const int32_t *x, uint32_t n)
{
    int32_t pk = 0;
    uint32_t i;
    for (i = 0; i < n; i++)
        if ((x[i] < 0 ? -x[i] : x[i]) > pk)
            pk = x[i] < 0 ? -x[i] : x[i];
    return pk;
}

/* the period (samples) of x[0..n) near t0: the best normalised autocorrelation in 0.8 .. 1.25 t0, refined */
static double period_of(const int32_t *x, uint32_t n, double t0)
{
    uint32_t lo = (uint32_t)(t0 * 0.8), hi = (uint32_t)(t0 * 1.25) + 1u, l, i, best = lo;
    double r[2048], bv = -2;
    for (l = lo; l <= hi && l < 2048u; l++) {
        double s = 0, a = 0, c = 0;
        for (i = 0; i + l < n; i++) {
            s += (double)x[i] * x[i + l];
            a += (double)x[i] * x[i];
            c += (double)x[i + l] * x[i + l];
        }
        r[l] = s / (sqrt(a * c) + 1e-9);
        if (r[l] > bv) {
            bv = r[l];
            best = l;
        }
    }
    if (best > lo && best < hi) {                       /* parabolic refinement */
        double y0 = r[best - 1], y1 = r[best], y2 = r[best + 1], d = y0 - 2 * y1 + y2;
        if (d < 0)
            return best + 0.5 * (y0 - y2) / d;
    }
    return best;
}

/* the amplitude of x[0..n) at f cycles a sample (Goertzel, Hann window) */
static double tone_of(const int32_t *x, uint32_t n, double f)
{
    double c = 2 * cos(2 * M_PI * f), s1 = 0, s2 = 0;
    uint32_t i;
    for (i = 0; i < n; i++) {
        double s0 = x[i] * (0.5 - 0.5 * cos(2 * M_PI * i / n)) + c * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    return sqrt(s1 * s1 + s2 * s2 - c * s1 * s2) / n;
}

static int32_t mix_peak(uint32_t blocks)
{
    static int32_t o[CTL * 2];
    int32_t pk = 0;
    uint32_t b;
    for (b = 0; b < blocks; b++) {
        mix_block(o, CTL);
        if (peak_of(o, CTL * 2u) > pk)
            pk = peak_of(o, CTL * 2u);
    }
    return pk;
}

static void quiet(void)
{
    uint32_t i, k;
    for (k = 0; k < NTRK; k++)
        trk_all_off(&trk[k]);
    for (i = 0; i < NDRUM; i++)
        drums.v[i].active = 0;
    drums.tail = 0;
    mix_peak(3u * FS / CTL);
}

/* the drum track (kit) hits GM note: its mono output into x[0..n); -> sampled GM-kit voices it started */
static uint32_t drum_hit(uint32_t kit, uint32_t note, int32_t *x, uint32_t n)
{
    static int32_t o[CTL * 2];
    uint32_t i, j = 0, syn = 0;
    TDRUM->p[P_E0] = (int16_t)kit;
    quiet();
    drum_on(note, 110);
    for (i = 0; i < NDRUM; i++)
        syn += drums.v[i].active && !drums.synth[i] && drums.v[i].s[4] < DZ_USR;
    while (j < n) {
        mix_block(o, CTL);
        for (i = 0; i < CTL && j < n; i++)
            x[j++] = o[2 * i] + o[2 * i + 1];
    }
    return syn;
}

static double corr(const int32_t *a, const int32_t *b, uint32_t n)
{
    double s = 0, x = 0, y = 0;
    uint32_t i;
    for (i = 0; i < n; i++) {
        s += (double)a[i] * b[i];
        x += (double)a[i] * a[i];
        y += (double)b[i] * b[i];
    }
    return s / (sqrt(x * y) + 1e-9);
}
static uint32_t zcr(const int32_t *x, uint32_t n)
{
    uint32_t i, z = 0;
    for (i = 1; i < n; i++)
        z += (x[i - 1] < 0) != (x[i] < 0);
    return z;
}

/* user slot k: one zone over every key, ADPCM nibbles alternating up / down (as userkit_test) */
static void slot_build(uint32_t k, uint32_t lo, uint32_t hi, uint32_t len)
{
    uint8_t *img = (uint8_t *)host_slots + k * SMP_USER_SIZE, *d = img + SMP_USER_DATA;
    smp_user_hdr_t *h = (smp_user_hdr_t *)img;
    smp_zone_t *z = &h->zone[0];
    uint32_t j, bytes = (len + 1u) / 2u;
    memset(img, 0xFF, SMP_USER_SIZE);
    memset(h, 0, sizeof *h);
    h->magic = SMP_USER_MAGIC;
    h->version = 1;
    h->nz = 1;
    memcpy(h->name, "TEST", 5);
    for (j = 0; j < bytes; j++)
        d[j] = (j / 8u) & 1u ? 0xFFu : 0x77u;
    z->off = 0;
    z->n = len;
    z->ls = 0;
    z->le = len - 1u;
    z->rate = 0x8000u;
    z->root16 = (int16_t)(lo * 16u);
    z->lo = (uint8_t)lo;
    z->hi = (uint8_t)hi;
    h->data_len = bytes;
    smp_user_scan(k);
}

int main(void)
{
    static const char *const names[] = {"PIANO", "BASS", "VIBES", "HORNS", "STRGS", "FLUTE", "SCRCH", "PERC",
                                        "USR1", "USR2", "USR3", "USR4"};
    static const uint8_t gm[5] = {36, 38, 42, 46, 49};
    static int32_t hit[5][8192];
    uint32_t i, j, k, ok;
    int ended;
    char msg[128];

    if ((uintptr_t)host_slots < (uintptr_t)SMP_DATA || (uintptr_t)host_slots - (uintptr_t)SMP_DATA > 0xF0000000u) {
        printf("ble_samples: the slot image must lie above SMP_DATA within 4 GiB (32-bit offsets)\n");
        return 1;
    }
    host_tracks_init();

    /* the sets: names, indices, presets; no sample data */
    ok = SMP_NSETS == 8 && SMP_NALL == 12 && sizeof SMP_DATA == 86144u && SMP_SYN_NZ == 45 &&
         SMP_SETS[7].z0 == SMP_SYN_NZ && SMP_SETS[7].nz == 27u && ENG_SAMPLE.edit[0].max == 11 &&
         ENG_GRAIN.edit[0].max == 11 && ENG_SAMPLE.npresets == 11u && drum_set() == 7;
    for (i = 0; i < SMP_NALL; i++)
        ok &= !strcmp(SMP_ALL_NAMES[i], names[i]);
    for (i = 0; i < SMP_NSETS; i++)
        ok &= !strcmp(SMP_SETS[i].name, names[i]) && !strcmp(SMP_SET_NAMES[i], names[i]) && SMP_SETS[i].nz > 0;
    for (i = 0; i < SMP_NSETS; i++)
        ok &= SMP_PRESET_TABLE[i].e[0] == (int8_t)i;
    check(ok, "8 built-in sets, names / indices / presets / USR1..4 as before; 45 stand-in zones, PERC 27 zones");
    {   /* PERC's bytes: the sample build's (sample_replacement_test: FNV-1a 0xdd089458 over its 86144 B) */
        uint32_t hash = 2166136261u, lo = 0xFFFFFFFFu, hi = 0;
        for (i = 0; i < sizeof SMP_DATA; i++)
            hash = (hash ^ SMP_DATA[i]) * 16777619u;
        for (i = SMP_SYN_NZ; i < sizeof SMP_ZONES / sizeof SMP_ZONES[0]; i++) {
            const smp_zone_t *z = &SMP_ZONES[i];
            lo = z->off < lo ? z->off : lo;
            hi = z->off + (z->n + 1u) / 2u > hi ? z->off + (z->n + 1u) / 2u : hi;
        }
        snprintf(msg, sizeof msg, "SMP_DATA: %u B, PERC only (FNV-1a %08x), its zones at offsets %u .. %u",
                 (unsigned)sizeof SMP_DATA, hash, lo, hi);
        check(hash == 0xdd089458u && lo == 0u && hi <= sizeof SMP_DATA, msg);
    }

    /* every stand-in zone: in range at every sample, audible, never at the clamp */
    ok = 1;
    for (i = 0; i < sizeof SMP_ZONES / sizeof SMP_ZONES[0]; i++) {
        const smp_zone_t *z = &SMP_ZONES[i];
        int32_t pk = 0;
        if (i >= SMP_SYN_NZ) {
            ok &= !smp_is_syn(z);                       /* PERC: its samples */
            continue;
        }
        ok &= smp_is_syn(z) && z->n > 0 && z->ls <= z->le && z->le <= z->n && z->lo <= z->hi;
        for (j = 0; j < z->n; j++) {
            int32_t a = smp_syn(z, j, 1), b = smp_syn(z, j, 0);
            a = a < 0 ? -a : a;
            b = b < 0 ? -b : b;
            pk = a > pk ? a : pk;
            pk = b > pk ? b : pk;
        }
        if (pk >= 32767 || pk < 3000) {
            printf("  zone %u (off %u): peak %d\n", i, z->off, pk);
            ok = 0;
        }
    }
    check(ok, "every stand-in zone: peak 3000 .. 32766 (no clamp), well formed");

    /* melodic sets: the pitch follows the note */
    for (i = 0; i < 6u; i++) {
        double err = 0, per[3];
        int32_t pk = 0;
        static const uint8_t notes[3] = {48, 60, 72};
        for (j = 0; j < 3u; j++) {
            uint32_t n = render_eng(4, i, i, notes[j], 12000u / CTL, &ended);
            double t0 = (double)FS / (440.0 * pow(2.0, (notes[j] - 69) / 12.0));
            int32_t p = peak_of(buf, n);
            pk = p > pk ? p : pk;
            per[j] = period_of(buf + 2048, 8192, t0);
            if (fabs(per[j] / t0 - 1) > err)
                err = fabs(per[j] / t0 - 1);
            if (p < 2000)
                err = 1;
        }
        snprintf(msg, sizeof msg, "%s: notes 48/60/72 at their pitch (worst %.2f %%), octaves %.3f %.3f, peak %d",
                 names[i], err * 100, per[0] / per[1], per[1] / per[2], pk);
        check(err < 0.03 && fabs(per[0] / per[1] - 2) < 0.06 && fabs(per[1] / per[2] - 2) < 0.06 &&
              pk < 2 * VOICE_FS, msg);
    }
    /* the computed (SYK_PM / SYK_PM3) rows: sample_render's stand-in path adds no clamp (level + noise below full
     * scale), shifts the detuned carrier by 16 - k - detune >= 0, and keeps its phases through a loop (whole periods) */
    ok = 1;
    for (i = 0; i < SMP_SYN_NZ; i++) {
        const smp_zone_t *z = &SMP_ZONES[i];
        const smp_syn_def_t *d = &SMP_SYN_DEF[(z->off >> 8) % SY_N];
        uint32_t kz = z->off & 15u;
        if (d->kind == SYK_SWEEP)
            continue;
        ok &= d->a0 + d->a1 + (d->breath + d->chiff) / 2 < 32767 && (!d->detune || kz + d->detune <= 16u) &&
              (!z->looped || (z->le + 1u - z->ls) % (1u << kz) == 0u) && (d->kind == SYK_PM3) == (d->r2 != 0);
    }
    check(ok, "computed stand-in rows: no clamp needed, detune shift in range, loops whole periods");

    {   /* PIANO (SYK_PM3): a 14 : 1 tine at the attack, its sidebands at 13 and 15 f0, gone a quarter second later;
         * in SAMPLE's render and in the stateless read (GRAIN) */
        const smp_zone_t *z = &SMP_ZONES[SMP_SETS[0].z0];
        double f0 = 440.0 * pow(2.0, (60 - 69) / 12.0) / FS, a1, a15, l15, s1, s15, fz;
        static int32_t raw[2048];
        uint32_t n;
        for (j = 0; j < SMP_SETS[0].nz; j++)
            if (SMP_ZONES[SMP_SETS[0].z0 + j].lo <= 60 && 60 <= SMP_ZONES[SMP_SETS[0].z0 + j].hi)
                z = &SMP_ZONES[SMP_SETS[0].z0 + j];
        n = render_eng(4, 0, 0, 60, 14400u / CTL, &ended);
        a1 = tone_of(buf, 2048, f0);
        a15 = tone_of(buf, 2048, 15 * f0) + tone_of(buf, 2048, 13 * f0);
        l15 = tone_of(buf + 12288, 2048, 15 * f0) + tone_of(buf + 12288, 2048, 13 * f0);
        fz = 1.0 / (1u << (z->off & 15u));              /* the zone's own pitch, cycles a source sample */
        for (j = 0; j < 2048u; j++)
            raw[j] = smp_syn(z, j, 1);
        s1 = tone_of(raw, 2048, fz);
        s15 = tone_of(raw, 2048, 15 * fz) + tone_of(raw, 2048, 13 * fz);
        snprintf(msg, sizeof msg, "PIANO: tine at the attack %.1f / %.1f dB (SAMPLE / read), %.1f dB later",
                 20 * log10(a15 / a1 + 1e-12), 20 * log10(s15 / s1 + 1e-12), 20 * log10(l15 / a1 + 1e-12));
        check(n >= 14336u && a15 > 0.03 * a1 && s15 > 0.03 * s1 && l15 < 0.1 * a15, msg);
    }
    {   /* SCRCH: a one-shot whose length halves an octave up (zone 0: keys 0..61, root 57) */
        uint32_t n1 = render_eng(4, 6, 6, 45, 400, &ended), n2;
        int32_t p1 = peak_of(buf, n1 < NBUF ? n1 : NBUF), p2;
        int e1 = ended;
        n2 = render_eng(4, 6, 6, 57, 400, &ended);
        p2 = peak_of(buf, n2 < NBUF ? n2 : NBUF);
        snprintf(msg, sizeof msg, "SCRCH: audible (%d, %d), a one-shot, %u -> %u samples an octave up", p1, p2, n1, n2);
        check(e1 && ended && p1 > 2000 && p2 > 2000 && fabs((double)n1 / n2 - 2) < 0.1, msg);
    }

    /* note-off: every melodic preset (and SCRCH) decays to silence through the real voice path */
    ok = 1;
    for (i = 0; i < ENG_SAMPLE.npresets; i++) {
        track_t *t = &trk[0];
        int32_t on, off;
        uint32_t s;
        if (SMP_PRESET_TABLE[i].e[0] == 7)
            continue;                                   /* (GM KIT: the drums below) */
        host_tracks_init();
        host_preset(t, 4, i);
        for (s = 0; s < 4u; s++)
            t->p[P_DIST + s] = 0;                       /* no sends: no reverb / delay tail */
        trk_note_on(t, 60, 100);
        on = mix_peak(FS / 2u / CTL);
        trk_note_off(t, 60);
        mix_peak(4u * FS / CTL);
        off = mix_peak(FS / 10u / CTL);
        if (on < 2000 || off > 16) {
            printf("  preset %s: held %d, 4 s after the note-off %d\n", SMP_PRESET_TABLE[i].name, on, off);
            ok = 0;
        }
    }
    check(ok, "every SAMPLE preset on a stand-in: audible, silent 4 s after its note-off");

    /* PERC on the drum track: ACOUSTIC..DUST play the GM samples; 36/38/42/46/49 distinct */
    host_tracks_init();
    song.g[G_DRREV] = song.g[G_DRDLY] = 0;
    for (k = 0; k < DRUM_SAMPLED; k++) {
        double cmax = 0;
        int32_t pmin = 1 << 30;
        uint32_t zs[5], same = 0;
        for (j = 0; j < 5u; j++) {
            uint32_t syn = drum_hit(k, gm[j], hit[j], 8192);
            if (peak_of(hit[j], 8192) < pmin)
                pmin = peak_of(hit[j], 8192);
            zs[j] = zcr(hit[j], 8192);
            if (syn != 1u)
                pmin = 0;                               /* (not one sampled GM-kit voice) */
        }
        for (j = 0; j < 5u; j++)
            for (i = j + 1; i < 5u; i++) {
                double c = fabs(corr(hit[j], hit[i], 8192));
                cmax = c > cmax ? c : cmax;
                same += zs[i] == zs[j];
            }
        snprintf(msg, sizeof msg, "drum kit %s: GM 36/38/42/46/49 sampled, audible (min %d), distinct (|r| <= %.2f)",
                 DRUM_KIT_NAMES[k], pmin, cmax);
        check(pmin > 2000 && cmax < 0.9 && !same, msg);
    }
    {   /* SAMPLE's GM KIT (the PERC zones) */
        double cmax = 0;
        int32_t pmin = 1 << 30;
        for (j = 0; j < 5u; j++) {
            uint32_t n = render_eng(4, 7, 7, gm[j], 8192u / CTL, &ended);
            memcpy(hit[j], buf, sizeof hit[j]);
            if (n < 8192u)
                memset(hit[j] + n, 0, (8192u - n) * sizeof hit[j][0]);
            if (peak_of(hit[j], 8192) < pmin)
                pmin = peak_of(hit[j], 8192);
        }
        for (j = 0; j < 5u; j++)
            for (i = j + 1; i < 5u; i++) {
                double c = fabs(corr(hit[j], hit[i], 8192));
                cmax = c > cmax ? c : cmax;
            }
        snprintf(msg, sizeof msg, "SAMPLE GM KIT: GM 36/38/42/46/49 audible (min %d), distinct (|r| <= %.2f)", pmin, cmax);
        check(pmin > 2000 && cmax < 0.9, msg);
    }

    /* GRAIN over every built-in set (stand-ins and PERC's samples): wide spread, every position, reverse grains (RAND), short and long grains */
    ok = 1;
    for (i = 0; i < SMP_NSETS; i++) {
        static const uint8_t mel[4] = {20, 45, 60, 100}, perc[4] = {36, 38, 42, 49};
        uint32_t q;
        for (q = 0; q < 4u; q++) {
            uint32_t note = i == 7u ? perc[q] : mel[q], b, n = 0;
            int32_t pk = 0;
            track_t *t = &trk[0];
            voice_t *v = &t->v[0];
            vmod_t m = {0};
            memset(t, 0, sizeof *t);
            host_preset(t, ENGI_GRAIN, q);
            t->p[P_E0] = (int16_t)i;
            t->p[P_E1] = (int16_t)(q * 40);             /* POS */
            t->p[P_E2] = (int16_t)(q & 1u ? 127 : 10);  /* SIZE */
            t->p[P_E3] = 127;                           /* DENS */
            t->p[P_E5] = 127;                           /* SPRD */
            t->p[P_E6] = 127;                           /* RAND: half the grains reversed */
            v->note = (uint8_t)note;
            v->vel = 127;
            v->active = v->gate = 1;
            m.pitch16 = (int32_t)note * 16;
            m.inc = pitch_inc(m.pitch16);
            m.amp0 = m.amp1 = m.envq15 = 32767;
            ENG_GRAIN.note_on(t, v);
            for (b = 0; b < 600u; b++) {
                int32_t out[CTL] = {0};
                m.shape = (int32_t)((b * 97u) % (128u << 8));   /* SHP modulation sweeps POS */
                ENG_GRAIN.block(t);
                ENG_GRAIN.render(t, v, out, CTL, &m);
                if (peak_of(out, CTL) > pk)
                    pk = peak_of(out, CTL);
                n += CTL;
            }
            if (pk < 500) {
                printf("  GRAIN %s note %u: peak %d\n", names[i], note, pk);
                ok = 0;
            }
        }
    }
    check(ok, "GRAIN over every built-in set (reverse grains, SPRD 127): audible, in bounds");

    /* the user slots: ADPCM from their flash, as before. Their zones' offsets are rebased onto SMP_DATA
     * (eng_sample.c smp_user_scan): reading them indexes past SMP_DATA by design, which UBSan's bounds check
     * reports in the sample build too (userkit_test); the sanitizer build stops here, the plain build runs them */
#if defined(__has_feature)
#if __has_feature(address_sanitizer) || __has_feature(undefined_behavior_sanitizer)
#define BLE_TEST_SANITIZED 1
#endif
#endif
#ifdef BLE_TEST_SANITIZED
    printf("ble_samples: (sanitizer build: the user-slot checks run in the plain build)\n");
    printf("ble_samples: %s\n", fails ? "FAILED" : "PASS");
    return fails != 0;
#endif
    slot_build(0, 0, 127, 20000);
    slot_build(1, 36, 36, 6000);
    check(usr_nz[0] == 1u && usr_nz[1] == 1u && !smp_is_syn(&usr_zone[0][0]) && !usr_nz[2],
          "USR1 / USR2 read from their flash image (ADPCM zones, not stand-ins)");
    {
        uint32_t n = render_eng(4, 0, SMP_NSETS, 60, 200, &ended);
        int32_t p = peak_of(buf, n < NBUF ? n : NBUF), g;
        render_eng(4, 0, SMP_NSETS + 2u, 60, 20, &ended);
        check(p > 2000 && ended, "SAMPLE USR1 plays its ADPCM; USR3 (empty) is silent");
        n = render_eng(ENGI_GRAIN, 0, SMP_NSETS, 60, 400, &ended);
        g = peak_of(buf, n < NBUF ? n : NBUF);
        check(g > 500, "GRAIN USR1 plays its ADPCM");
    }
    host_tracks_init();
    song.g[G_DRREV] = song.g[G_DRDLY] = 0;
    drum_hit(DRUM_USR + 1u, 36, hit[0], 4096);
    {
        uint32_t s, usr = 0;
        for (s = 0; s < NDRUM; s++)
            usr += drums.v[s].active && !drums.synth[s] && drums.v[s].s[4] >= DZ_USR;
        check(usr == 1u && peak_of(hit[0], 4096) > 2000, "drum kit USR2: its kick from the slot (the sample path)");
    }
    printf("ble_samples: %s\n", fails ? "FAILED" : "PASS");
    return fails != 0;
}
