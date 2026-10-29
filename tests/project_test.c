/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Host test of the project formats (firmware/src/project.c, -DPROJ_HOST part). Format 7 ("ORP7", ORBIT 0.7.0:
 * compact 2.6 parameters, nudge and condition in one byte, HOME macros, user tag) is written; format 5 ("FUN5",
 * ORBIT 0.4 .. 0.6: 61 parameters, nudges, locks, fill conditions) converts by count (an engine control's lock moves
 * with it); SLOOP 2.6's FUN6 is refused; format 4 ("FUN4", SLOOP 2.0:
 * 10-byte steps with levels and ratchets, the drum track's 16 lanes, P_CHORD), format 3 ("FUN3", SLOOP
 * 1.x), format 2 ("FUN2", 53 parameters per track) and format 1 ("FUN1"), built byte for byte as the
 * firmware stored them, convert: every old value at its parameter, the parameters added since at their
 * defaults, the swings onto the MPC scale (x 0.8), synth steps as they were, the drum track's notes onto
 * its lanes (accent: hard), globals, selection, the engine bytes (kept; the drum track's 0), no nudge, no
 * lock, no fill condition; damaged ones are refused. Run by tests/run_tests.sh (needs build/gen). */
#define main hostsim_main
#include <stddef.h>
#include "hostsim.c"
#undef main
#define PROJ_HOST 1
static uint32_t trk_def_engine(uint32_t i)       /* ui.c TRK_DEF: ANALOG, DIGITAL, LOFI */
{
    static const uint8_t E[NPART] = {0, 1, 3};
    return i < NPART ? E[i] : 0u;
}
#include "../firmware/src/project.c"

static int check(const char *what, int ok)
{
    printf("%-66s %s\n", what, ok ? "ok" : "FAIL");
    return ok ? 0 : 1;
}

/* the value parameter k (old id) of track t had in the old project */
static int16_t oldv(uint32_t t, uint32_t k) { return (int16_t)(t * 100u + k * 3u + 1u); }

static const uint8_t OLD_ENG[NTRK] = {7, 0, 6, 8};   /* WHEEL, ANALOG, TRIO; the drum track: 8 (none) */
static void fill_old_steps(step8_t *st, uint32_t t)
{
    uint32_t k;
    for (k = 0; k < NSTEP; k++) {
        step8_t *s = &st[k];
        s->note[0] = (uint8_t)(36u + (k + t) % 40u);
        s->note[1] = (uint8_t)(38u + k % 5u);
        s->n = (uint8_t)(k % 3u);
        s->time = (uint8_t)(k % 3u);
        s->flags = (uint8_t)(k & 3u);
        s->vel = (uint8_t)(64u + t);
    }
}
static void fill_v2_track(proj_trk_v2_t *d, uint32_t t)
{
    uint32_t k;
    for (k = 0; k < PROJ_NP_V2; k++)
        d->p[k] = oldv(t, k);
    d->engine = OLD_ENG[t];
    d->preset = (uint8_t)(t + 5u);
    fill_old_steps(d->step, t);
}
static void fill_v3_track(proj_trk_v3_t *d, uint32_t t)
{
    uint32_t k;
    for (k = 0; k < PROJ_NP_V3; k++)
        d->p[k] = oldv(t, k);
    d->p[P_SSWING] = 50;                           /* (swings: within 0..100) */
    d->p[P_ASWING] = 100;
    d->engine = OLD_ENG[t];
    d->preset = (uint8_t)(t + 5u);
    fill_old_steps(d->step, t);
}

/* the steps of a converted track against the old ones: synth as they were, drums onto lanes */
static int steps_ok(const proj_trk_t *n, const step8_t *o, int drum)
{
    uint32_t k, i;
    for (k = 0; k < NSTEP; k++) {
        if (drum) {
            const dstep_t *d = &n->dstep[k];
            uint32_t want = 0;
            if (o[k].time == ST_NOTE)
                for (i = 0; i < o[k].n; i++)
                    want |= 1u << lane_of_note(o[k].note[i]);
            if (dstep_mask(d) != want)
                return 0;
            for (i = 0; i < DRUM_LANES; i++)
                if ((want >> i) & 1u && dstep_lvl(d, i) != ((o[k].flags & SF_ACCENT) ? LV_HARD : vel_lvl(o[k].vel)))
                    return 0;
        } else {
            const step_t *s = &n->step[k];
            if (memcmp(s->note, o[k].note, 4) || s->n != o[k].n || s->time != o[k].time || s->flags != o[k].flags ||
                s->vel != o[k].vel || s->lvl || s->rat)
                return 0;
        }
    }
    return 1;
}

/* track t converted from format 2 / 1 has the old values where they belong */
static int track_ok_v2(const proj_trk_t *n, const proj_trk_v2_t *o, uint32_t t)
{
    uint32_t k;
    int ok = (t == TRK_DRUM ? n->engine == 0 && n->preset == 0 : n->engine == o->engine && n->preset == o->preset) &&
             steps_ok(n, o->step, t == TRK_DRUM);
    for (k = 0; k <= P_DETUNE; k++)
        if (k != P_SSWING && k != P_ASWING)
            ok &= n->p[k] == oldv(t, k);
    ok &= n->p[P_SLCR] == 0 && n->p[P_SLPAT] == TP[P_SLPAT].def && n->p[P_SLRATE] == TP[P_SLRATE].def &&
          n->p[P_SLDEPTH] == TP[P_SLDEPTH].def && n->p[P_CHORD] == 0;
    for (k = 0; k < 8u; k++)
        ok &= n->e[k] == oldv(t, 45u + k);
    return ok;
}

/* a converted track has no nudge, condition or lock, the 2.6 parameters at their defaults, default macros, no tag */
static int track_plain(const proj_trk_t *n)
{
    uint32_t k;
    int ok = n->user == 0 && pt_get(n, P_TRES) == 40 && pt_get(n, P_TFTYPE) == 0;
    for (k = 0; k < NLOCK; k++)
        ok &= n->lock[k].step == LOCK_FREE;
    for (k = 0; k < NSTEP; k++)
        ok &= n->mf[k] == 0;
    for (k = P_ITYPE; k < P_E0; k++)
        ok &= pt_get(n, k) == TP[k].def;
    for (k = 0; k < MACRO_COUNT; k++)
        ok &= n->macro[k] == MACRO_DEFAULT;
    return ok;
}

int main(void)
{
    static project_v3_t v3;
    static project_v2_t v2;
    static project_v1_t v1;
    static project_v4_t v4;
    static project_t q, q2;
    static project_v5_t v5;
    static union {
        project_t v7;
        project_v5_t v5;
        project_v4_t v4;
        project_v3_t v3;
        project_v2_t v2;
        project_v1_t v1;
    } buf;
    uint32_t i, t;
    int bad = 0, ok;

    bad += check("layout: common ids 0..52 kept, P_ITYPE 53 .. P_LPOL 62, P_E0 63, P_COUNT 71, G_COUNT 46 (G_DTYPE 45)",
                 P_CHORD + 1 == P_TFLT && P_TFLT + 1 == P_STRUM && P_STRUM + 1 == P_VLEAD && P_VLEAD == 52 &&
                 P_ITYPE == 53 && P_LPOL == 62 && P_TRES == 63 && P_TFTYPE == 64 && P_E0 == 65 && P_COUNT == 73 && G_COUNT == 46 && G_DTYPE == 45 && G_CH1 == 33 &&
                 G_BEND == 44 && PROJ_NP_V5 == 61u && P_SLDEPTH + 1 == P_CHORD);
    bad += check("ORP7 is 3840 bytes (track 940, offsets asserted); FUN5 frozen at 3840; 4 slots fit .noinit",
                 sizeof(project_t) == 3840u && sizeof(proj_trk_t) == 940u && sizeof(project_v5_t) == 3840u &&
                 offsetof(proj_trk_t, macro) == 934u && offsetof(project_t, sum) == 3836u &&
                 4u * sizeof(project_t) <= 0x3D50u - 256u && PROJ_MAGIC == 0x3850524Fu);
    bad += check("ORP7 magic bytes are 4F 52 50 37 and differ from FUN1..FUN6",
                 ((const uint8_t *)&(uint32_t){PROJ_MAGIC})[0] == 0x4Fu && ((const uint8_t *)&(uint32_t){PROJ_MAGIC})[3] == 0x38u &&
                 PROJ_MAGIC != PROJ_MAGIC_FUN6 && PROJ_MAGIC != PROJ_MAGIC_V5 && PROJ_MAGIC != PROJ_MAGIC_V1);

    /* format 3 (SLOOP 1.x) */
    memset(&v3, 0, sizeof v3);
    v3.magic = PROJ_MAGIC_V3;
    v3.size = sizeof v3;
    for (i = 0; i < PROJ_NG_V3; i++)
        v3.g[i] = (int16_t)(300 + i);
    v3.g[G_SWING] = 50;
    v3.sel = 3;
    for (t = 0; t < NTRK; t++)
        fill_v3_track(&v3.t[t], t);
    v3.sum = proj_hash(&v3, sizeof v3 - 4u);
    memcpy(&buf, &v3, sizeof v3);
    ok = proj_import(&q, &buf, (int)sizeof v3);
    bad += check("FUN3 -> ORP7: converted, valid format 7 slot", ok && proj_ok(&q) && q.magic == PROJ_MAGIC);
    ok = q.sel == 3 && q.g[G_SWING] == 40;
    for (i = 0; i < PROJ_NG_V3; i++)
        ok &= i == G_SWING || q.g[i] == (int16_t)(300 + i);
    for (i = PROJ_NG_V3; i < PROJ_NG; i++)
        ok &= q.g[i] == GP[i].def;
    ok &= q.drdly == 0;
    bad += check("FUN3 -> ORP7: globals (swing 50 -> 40: the MPC scale), the new ones default", ok);
    ok = 1;
    for (t = 0; t < NTRK; t++) {
        const proj_trk_t *n = &q.t[t];
        uint32_t k;
        ok &= (t == TRK_DRUM ? n->engine == 0 : n->engine == OLD_ENG[t]) && steps_ok(n, v3.t[t].step, t == TRK_DRUM);
        for (k = 0; k < PROJ_NP_V3 - 8u; k++)
            if (k != P_SSWING && k != P_ASWING)
                ok &= n->p[k] == oldv(t, k);
        ok &= n->p[P_SSWING] == 40 && n->p[P_ASWING] == 80 && n->p[P_CHORD] == 0;
        for (k = 0; k < 8u; k++)
            ok &= n->e[k] == oldv(t, PROJ_NP_V3 - 8u + k);
        ok &= track_plain(n);
    }
    bad += check("FUN3 -> ORP7: parameters (P_E0.. moved), steps, drum notes -> lanes, no lock, no fill", ok);

    /* format 2, as written before the SLICER */
    memset(&v2, 0, sizeof v2);
    v2.magic = PROJ_MAGIC_V2;
    v2.size = sizeof v2;
    for (i = 0; i < PROJ_NG_V2; i++)
        v2.g[i] = (int16_t)(500 + i);
    v2.sel = 2;
    for (t = 0; t < NTRK; t++)
        fill_v2_track(&v2.t[t], t);
    v2.sum = proj_hash(&v2, sizeof v2 - 4u);
    bad += check("FUN2 image is 2552 bytes (as stored)", sizeof v2 == 2552u);
    memcpy(&buf, &v2, sizeof v2);
    ok = proj_import(&q, &buf, (int)sizeof v2);
    bad += check("FUN2 -> ORP7: converted, valid format 7 slot", ok && proj_ok(&q) && q.magic == PROJ_MAGIC);
    ok = q.sel == 2;
    for (i = 0; i < PROJ_NG_V2; i++)
        ok &= i == G_SWING || q.g[i] == (int16_t)(500 + i);
    bad += check("FUN2 -> ORP7: globals and selected track", ok);
    ok = 1;
    for (t = 0; t < NTRK; t++)
        ok &= track_ok_v2(&q.t[t], &v2.t[t], t) && track_plain(&q.t[t]);
    bad += check("FUN2 -> ORP7: every parameter mapped, SLICER OFF, CHORD OFF (4 tracks)", ok);
    bad += check("FUN2 -> ORP7: engine bytes kept (WHEEL 7, ANALOG 0, TRIO 6), drum 0",
                 q.t[0].engine == 7 && q.t[1].engine == 0 && q.t[2].engine == 6 && q.t[3].engine == 0 &&
                 str_eq(ENGINES[7]->name, "WHEEL") && str_eq(ENGINES[6]->name, "TRIO") && NENGINES > 8);

    /* a FUN4 image (SLOOP 2.0 .. 2.3, as stored) -> FUN5: the same, zero nudges, every lock free */
    memset(&v4, 0, sizeof v4);
    v4.magic = PROJ_MAGIC_V4;
    v4.size = sizeof v4;
    memcpy(v4.g, q.g, sizeof v4.g);
    v4.sel = 1;
    for (t = 0; t < NTRK; t++) {
        for (i = 0; i < PROJ_NP_V4; i++)               /* (format 4's ids: P_E0 was P_TFLT) */
            v4.t[t].p[i] = i < P_TFLT ? q.t[t].p[i] : q.t[t].e[i - P_TFLT];
        v4.t[t].engine = q.t[t].engine;
        v4.t[t].preset = q.t[t].preset;
        memcpy(v4.t[t].step, q.t[t].step, sizeof v4.t[t].step);
    }
    v4.t[1].engine = 8;
    v4.t[0].engine = ORBIT_SWARM;
    v4.t[2].engine = ORBIT_FM4;
    v4.t[0].step[3].lvl = 0x9C;
    v4.t[0].step[3].rat = 0x27;
    dstep_set(&v4.t[TRK_DRUM].dstep[5], 13, LV_GHOST, 2);
    v4.sum = proj_hash(&v4, sizeof v4 - 4u);
    bad += check("FUN4 image is 3112 bytes (as stored)", sizeof v4 == 3112u);
    memcpy(&buf, &v4, sizeof v4);
    ok = proj_import(&q2, &buf, (int)sizeof v4) && proj_ok(&q2) && q2.magic == PROJ_MAGIC && q2.sel == 1 && q2.t[1].engine == 8 && q2.t[0].engine == ORBIT_SWARM && q2.t[2].engine == ORBIT_FM4 &&
         q2.t[0].step[3].lvl == 0x9C && q2.t[0].step[3].rat == 0x27 && dstep_lvl(&q2.t[TRK_DRUM].dstep[5], 13) == LV_GHOST &&
         dstep_rat(&q2.t[TRK_DRUM].dstep[5], 13) == 2u && !memcmp(q2.g, v4.g, sizeof q2.g);
    for (t = 0; t < NTRK; t++) {
        ok &= !memcmp(q2.t[t].p, v4.t[t].p, P_TFLT * 2u) && !memcmp(q2.t[t].e, v4.t[t].p + P_TFLT, 16u) &&
              q2.t[t].p[P_TFLT] == 0 && q2.t[t].p[P_STRUM] == 0 && q2.t[t].p[P_VLEAD] == 0 &&
              !memcmp(q2.t[t].step, v4.t[t].step, sizeof q2.t[t].step) && track_plain(&q2.t[t]);
    }
    bad += check("FUN4 -> ORP7: as stored (levels, ratchets, lanes, engine 8; P_E0.. moved, FILTER off), zero nudges, locks free, no fill condition", ok);
    v4.t[2].step[1].vel ^= 1u;
    memcpy(&buf, &v4, sizeof v4);
    bad += check("FUN4 with a bad checksum: refused", !proj_import(&q2, &buf, (int)sizeof v4));
    v4.t[2].step[1].vel ^= 1u;

    /* a FUN5 image as ORBIT 0.6 stored it (built through the frozen layout): by count into ORP7 */
    memset(&v5, 0, sizeof v5);
    v5.magic = PROJ_MAGIC_V5;
    v5.size = sizeof v5;
    for (i = 0; i < PROJ_NG; i++)
        v5.g[i] = (int16_t)(40 + i);
    v5.sel = 2;
    v5.drdly = 91;
    v5.rsv[0] = 0xFF;                              /* (spare in FUN5: never TEXTURE) */
    for (t = 0; t < NTRK; t++) {
        for (i = 0; i < PROJ_NP_V5; i++)
            v5.t[t].p[i] = (int16_t)(t * 1000u + i * 7u);
        v5.t[t].engine = (uint8_t)(t == 0 ? ENGI_CLUSTER : t == 1 ? ENGI_CLOUD : t == 2 ? ORBIT_FM4 : 0u);
        v5.t[t].preset = (uint8_t)(3u + t);
        memcpy(v5.t[t].step, q.t[t].step, sizeof v5.t[t].step);
        for (i = 0; i < NLOCK; i++)
            v5.t[t].lock[i].step = LOCK_FREE;
    }
    v5.t[0].p[53] = 18431, v5.t[0].p[54] = -32768, v5.t[0].p[55] = 24575, v5.t[0].p[56] = 32767;   /* CLUSTER COUNT, */
    v5.t[0].p[60] = 32767;                         /* WAVE, CURVE, SPREAD at their ends; a reserved control raw */
    v5.t[0].micro[0] = MICRO_MIN, v5.t[0].micro[1] = MICRO_MAX, v5.t[0].micro[2] = -1, v5.t[0].micro[63] = 5;
    v5.t[0].fill[0] = 0x39;                        /* steps 1..3: FILL, NO FILL, 3 (normal); step 4: 0 */
    v5.t[TRK_DRUM].fill[15] = 0x40;                /* step 64: FILL */
    v5.t[0].lock[0].step = 3, v5.t[0].lock[0].param = 54, v5.t[0].lock[0].val = -32768;   /* old P_E1 (WAVE) */
    v5.t[0].lock[1].step = 4, v5.t[0].lock[1].param = 56, v5.t[0].lock[1].val = 32767;    /* old P_E3 (SPREAD) */
    v5.t[0].lock[4].step = 7, v5.t[0].lock[4].param = 60, v5.t[0].lock[4].val = 9;        /* old P_E7 */
    v5.t[0].lock[2].step = 5, v5.t[0].lock[2].param = P_DIST, v5.t[0].lock[2].val = 100;  /* common: kept */
    v5.t[0].lock[3].step = 6, v5.t[0].lock[3].param = 61, v5.t[0].lock[3].val = 1;        /* no parameter: freed */
    v5.t[2].lock[23].step = 63, v5.t[2].lock[23].param = P_LEVEL, v5.t[2].lock[23].val = 50;
    v5.sum = proj_hash(&v5, sizeof v5 - 4u);
    memcpy(&buf, &v5, sizeof v5);
    ok = proj_import(&q2, &buf, (int)sizeof v5) && proj_ok(&q2) && q2.magic == PROJ_MAGIC && q2.sel == 2 &&
         q2.drdly == 91 && q2.rsv[0] == 0 && q2.rsv[1] == 0 && !memcmp(q2.g, v5.g, sizeof q2.g);
    bad += check("FUN5 -> ORP7: converted, valid, globals / sel / drum DLY kept, no TEXTURE marker", ok);
    ok = 1;
    for (t = 0; t < NTRK; t++) {
        for (i = 0; i < PROJ_NC7; i++)
            ok &= q2.t[t].p[i] == v5.t[t].p[i];
        for (i = 0; i < 8u; i++)
            ok &= pt_get(&q2.t[t], P_E0 + i) == v5.t[t].p[53u + i];
        for (i = P_ITYPE; i < P_E0; i++)
            ok &= pt_get(&q2.t[t], i) == TP[i].def;
        for (i = 0; i < MACRO_COUNT; i++)
            ok &= q2.t[t].macro[i] == MACRO_DEFAULT;
        ok &= q2.t[t].user == 0 && q2.t[t].flt == 40 && !memcmp(q2.t[t].step, v5.t[t].step, sizeof v5.t[t].step);
    }
    ok &= q2.t[0].engine == ENGI_CLUSTER && q2.t[1].engine == ENGI_CLOUD && q2.t[2].engine == ORBIT_FM4 &&
          q2.t[TRK_DRUM].engine == 0 && q2.t[0].preset == 3;
    bad += check("FUN5 -> ORP7: common 0..52 by id, engine 53..60 -> 63..70 (signed16), 2.6 defaults, macros DEFAULT", ok);
    bad += check("FUN5 -> ORP7: CLUSTER 18431 / -32768 / 24575 / 32767 and reserved 32767 raw in e[]",
                 q2.t[0].e[0] == 18431 && q2.t[0].e[1] == -32768 && q2.t[0].e[2] == 24575 && q2.t[0].e[3] == 32767 &&
                 q2.t[0].e[7] == 32767);
    bad += check("FUN5 -> ORP7: nudges -32 / 31 / -1 / 5 and conditions packed (3 kept raw, normal on apply)",
                 pt_micro(&q2.t[0], 0) == MICRO_MIN && pt_micro(&q2.t[0], 1) == MICRO_MAX && pt_micro(&q2.t[0], 2) == -1 &&
                 pt_micro(&q2.t[0], 63) == 5 && pt_fill(&q2.t[0], 0) == FC_FILL && pt_fill(&q2.t[0], 1) == FC_NOFILL &&
                 pt_fill(&q2.t[0], 2) == 3u && pt_fill(&q2.t[0], 3) == 0u && pt_fill(&q2.t[TRK_DRUM], 63) == FC_FILL);
    bad += check("FUN5 -> ORP7: locks 54 -> 64, 56 -> 66, 60 -> 70 (values raw), common kept, id 61 freed, slot 23 kept",
                 q2.t[0].lock[0].param == P_E1 && q2.t[0].lock[0].val == -32768 && q2.t[0].lock[1].param == P_E3 &&
                 q2.t[0].lock[1].val == 32767 && q2.t[0].lock[4].param == P_E7 && q2.t[0].lock[4].val == 9 && q2.t[0].lock[2].param == P_DIST && q2.t[0].lock[2].val == 100 &&
                 q2.t[0].lock[3].step == LOCK_FREE && q2.t[2].lock[23].step == 63 && q2.t[2].lock[23].param == P_LEVEL);
    host_tracks_init();
    proj_apply(&q2, 1);
    bad += check("FUN5 -> ORP7 -> apply: CLUSTER extremes, reserved clamps to 0, moved locks, condition 3 normal",
                 trk[0].eng_req == ENGI_CLUSTER && trk[0].p[P_E0] == 18431 && trk[0].p[P_E1] == -32768 &&
                 trk[0].p[P_E2] == 24575 && trk[0].p[P_E3] == 32767 && trk[0].p[P_E7] == 0 &&
                 trk[0].lock[lock_find(&trk[0], 3, P_E1, 0)].val == -32768 &&
                 trk[0].lock[lock_find(&trk[0], 4, P_E3, 0)].val == 32767 && lock_find(&trk[0], 7, P_E7, 0) >= 0 &&
                 lock_find(&trk[0], 5, P_DIST, 0) >= 0 && step_fill(&trk[0], 2) == FC_NORM &&
                 step_fill(&trk[0], 0) == FC_FILL && trk[0].micro[0] == MICRO_MIN && trk[1].eng_req == ENGI_CLOUD &&
                 song.g[G_TXSRC] == GP[G_TXSRC].def && song.g[G_TXLVL] == GP[G_TXLVL].def);

    /* ORP7 round trip: stored as is */
    q2.t[0].macro[0] = P_ITYPE, q2.t[0].macro[1] = P_E7, q2.t[0].macro[2] = 0, q2.t[0].macro[3] = MACRO_DEFAULT;
    q2.t[1].macro[2] = P_LPOL;
    q2.t[0].user = 32;
    pt_set(&q2.t[1], P_ITYPE, 8);
    pt_set(&q2.t[1], P_IMIX, 0);
    pt_set(&q2.t[2], P_LSYNC, 10);
    q2.sum = proj_sum(&q2);
    memcpy(&buf, &q2, sizeof q2);
    bad += check("ORP7 -> ORP7: byte for byte (macros, user tag 32, x bytes)",
                 proj_import(&q, &buf, (int)sizeof q2) && !memcmp(&q, &q2, sizeof q) && pt_get(&q.t[1], P_ITYPE) == 8 &&
                 pt_get(&q.t[2], P_LSYNC) == 10 && q.t[0].user == 32);

    /* ORP7 uses the same byte layout, but engine parameter IDs precede RES/TYPE. */
    memcpy(&buf, &q2, sizeof q2);
    buf.v7.magic = PROJ_MAGIC_V7;
    for (t = 0; t < NTRK; t++) {
        buf.v7.t[t].flt = 0;
        for (i = 0; i < NLOCK; i++)
            if (buf.v7.t[t].lock[i].param >= P_E0 && buf.v7.t[t].lock[i].param <= P_E7)
                buf.v7.t[t].lock[i].param -= 2;
        for (i = 0; i < MACRO_COUNT; i++)
            if (buf.v7.t[t].macro[i] >= P_E0 && buf.v7.t[t].macro[i] <= P_E7)
                buf.v7.t[t].macro[i] -= 2;
    }
    buf.v7.sum = proj_sum(&buf.v7);
    bad += check("ORP7 migration preserves sounds, locks and macros and defaults RES/TYPE",
                 proj_import(&q, &buf, sizeof buf.v7) && proj_ok(&q) && !memcmp(&q, &q2, sizeof q));
    buf.v7.t[0].macro[0] = 71;
    buf.v7.t[0].lock[0].step = 0;
    buf.v7.t[0].lock[0].param = 72;
    buf.v7.sum = proj_sum(&buf.v7);
    ok = proj_import(&q, &buf, sizeof buf.v7);
    proj_apply(&q, 1);
    bad += check("invalid ORP7 IDs do not become newly valid engine controls",
                 ok && trk[0].macro[0] == MACRO_DEFAULT && trk[0].lock[0].step == LOCK_FREE);
    pt_set(&q.t[0], P_TRES, 127);
    pt_set(&q.t[0], P_TFTYPE, 1);
    q.sum = proj_sum(&q);
    memcpy(&buf, &q, sizeof q);
    bad += check("ORP8 packed RES/BP survives import and apply",
                 proj_import(&q, &buf, sizeof buf.v7) && q.t[0].flt == 255 &&
                 pt_get(&q.t[0], P_TRES) == 127 && pt_get(&q.t[0], P_TFTYPE) == 1);
    proj_apply(&q, 1);
    bad += check("ORP8 applies both track filter controls",
                 trk[0].p[P_TRES] == 127 && trk[0].p[P_TFTYPE] == 1);

    /* refused before anything changes: FUN6, a damaged ORP7 */
    memset(&q, 0x5A, sizeof q);
    memcpy(&buf, &q2, sizeof q2);
    buf.v7.magic = PROJ_MAGIC_FUN6;
    buf.v7.sum = proj_hash(&buf, sizeof q2 - 4u);
    ok = !proj_import(&q, &buf, (int)sizeof q2);
    memcpy(&buf, &q2, sizeof q2);
    buf.v7.magic = PROJ_MAGIC_V7;
    buf.v7.t[1].flt = 1;
    buf.v7.sum = proj_hash(&buf, sizeof q2 - 4u);
    ok &= !proj_import(&q, &buf, (int)sizeof q2);
    memcpy(&buf, &q2, sizeof q2);
    buf.v7.t[2].engine = NENGINES;
    buf.v7.sum = proj_hash(&buf, sizeof q2 - 4u);
    ok &= !proj_import(&q, &buf, (int)sizeof q2);
    memcpy(&buf, &q2, sizeof q2);
    buf.v7.t[0].user = UP_SLOTS + 1u;
    buf.v7.sum = proj_hash(&buf, sizeof q2 - 4u);
    ok &= !proj_import(&q, &buf, (int)sizeof q2);
    memcpy(&buf, &q2, sizeof q2);
    buf.v7.t[0].step[0].vel ^= 1u;
    ok &= !proj_import(&q, &buf, (int)sizeof q2) && !proj_import(&q, &q2, (int)sizeof q2 - 4);
    memcpy(&buf, &q2, sizeof q2);
    buf.v7.magic = 0x12345678u;
    buf.v7.sum = proj_hash(&buf, sizeof q2 - 4u);
    ok &= !proj_import(&q, &buf, (int)sizeof q2) && !proj_import(&q, &q, (int)sizeof q);
    for (i = 0; ok && i < sizeof q; i++)
        ok &= ((const uint8_t *)&q)[i] == 0x5Au;
    bad += check("refused, destination untouched: checksummed FUN6, reserved byte, engine 17, user 33, bad sum/length, unknown magic, aliasing", ok);

    /* apply: macros (an unknown id -> DEFAULT), the user tag kept, the drum track's none */
    memcpy(&q, &q2, sizeof q);
    q.t[1].macro[0] = P_COUNT;                     /* (a later firmware's id) */
    q.t[1].macro[1] = 126;
    q.t[1].user = 7;
    q.t[TRK_DRUM].macro[0] = 3;
    q.sum = proj_sum(&q);
    host_tracks_init();
    proj_apply(&q, 1);
    bad += check("apply: macros installed, ids past P_COUNT -> DEFAULT, user tag kept, drum macros DEFAULT",
                 trk[0].macro[0] == P_ITYPE && trk[0].macro[1] == P_E7 && trk[0].macro[2] == 0 &&
                 trk[0].macro[3] == MACRO_DEFAULT && trk[1].macro[0] == MACRO_DEFAULT && trk[1].macro[1] == MACRO_DEFAULT &&
                 trk[1].macro[2] == P_LPOL && trk[0].user == 32 && trk[1].user == 7 &&
                 TDRUM->macro[0] == MACRO_DEFAULT && macro_id(&trk[0], 3) == ENGINES[trk[0].eng_req]->macro[3] &&
                 macro_id(&trk[0], 0) == P_ITYPE);
    proj_capture(&q);
    bad += check("capture: macros, user, drum engine / preset / user 0, drum macros 127, x bytes",
                 q.t[0].macro[0] == P_ITYPE && q.t[1].macro[2] == P_LPOL && q.t[1].macro[0] == MACRO_DEFAULT &&
                 q.t[0].user == 32 && q.t[TRK_DRUM].engine == 0 && q.t[TRK_DRUM].preset == 0 && q.t[TRK_DRUM].user == 0 &&
                 q.t[TRK_DRUM].macro[0] == MACRO_DEFAULT && pt_get(&q.t[1], P_ITYPE) == 8 && proj_ok(&q));

    /* damaged / wrong size */
    v2.t[1].p[3]++;
    memcpy(&buf, &v2, sizeof v2);
    bad += check("FUN2 with a bad checksum: refused", !proj_import(&q2, &buf, (int)sizeof v2));
    v2.t[1].p[3]--;
    memcpy(&buf, &v2, sizeof v2);
    bad += check("FUN2 with a wrong length: refused", !proj_import(&q2, &buf, (int)sizeof v2 - 2));
    memcpy(&buf, &v5, sizeof v5);
    buf.v5.magic = PROJ_MAGIC_V3;
    bad += check("FUN5 size with a FUN3 magic: refused", !proj_import(&q2, &buf, (int)sizeof v5));
    memcpy(&buf, &v5, sizeof v5);
    buf.v5.magic = PROJ_MAGIC_V4;
    bad += check("FUN5 size with a FUN4 magic: refused", !proj_import(&q2, &buf, (int)sizeof v5));
    memcpy(&buf, &v5, sizeof v5);
    buf.v5.magic = PROJ_MAGIC;                     /* (an ORP7 magic on a FUN5 body: its checksum fails) */
    bad += check("FUN5 body with the ORP7 magic: refused (never read as ORP7)", !proj_import(&q2, &buf, (int)sizeof v5));
    memcpy(&buf, &v5, sizeof v5);
    buf.v5.t[1].p[60] ^= 1;
    bad += check("FUN5 with a bad checksum: refused", !proj_import(&q2, &buf, (int)sizeof v5));
    memcpy(&buf, &v3, sizeof v3);
    buf.v3.t[2].step[7].vel ^= 1u;
    bad += check("FUN3 with a bad checksum: refused", !proj_import(&q2, &buf, (int)sizeof v3));

    /* format 1: one instrument -> track 1, the others their defaults */
    memset(&v1, 0, sizeof v1);
    v1.magic = PROJ_MAGIC_V1;
    v1.size = sizeof v1;
    for (i = 0; i < PROJ_NG_V2; i++)
        v1.g[i] = (int16_t)(700 + i);
    fill_v2_track(&v1.t, 0);
    v1.sum = proj_hash(&v1, sizeof v1 - 4u);
    memcpy(&buf, &v1, sizeof v1);
    ok = proj_import(&q, &buf, (int)sizeof v1) && proj_ok(&q) && track_ok_v2(&q.t[0], &v1.t, 0) && q.g[5] == 705;
    for (t = 1; t < NTRK; t++)
        ok &= q.t[t].preset == 0xFF && q.t[t].p[P_SLCR] == 0 && q.t[t].p[P_LEVEL] == TP[P_LEVEL].def &&
              q.t[t].e[0] == ENGINES[trk_def_engine(t)]->edit[0].def && q.t[t].lock[0].step == LOCK_FREE &&
              q.t[t].macro[0] == MACRO_DEFAULT && pt_get(&q.t[t], P_IMIX) == TP[P_IMIX].def &&
              (t == TRK_DRUM ? dstep_mask(&q.t[t].dstep[0]) == 0u : q.t[t].step[0].time == ST_REST);
    bad += check("FUN1 -> ORP7: track 1 mapped, tracks 2..4 defaults", ok);

    /* capture / apply: the working project round trip (nudges and locks too) */
    host_tracks_init();
    for (t = 0; t < NTRK; t++)
        trk[t].p[P_SLEN] = (int16_t)(5 + t);
    trk[1].step[2].n = 2, trk[1].step[2].note[0] = 60, trk[1].step[2].note[1] = 64, trk[1].step[2].time = ST_NOTE;
    trk[1].step[2].lvl = 0x0D;
    dstep_set(&TDRUM->dstep[9], 4, LV_SOFT, 1);
    song.g[G_DUST] = 33;
    song.g[G_DRDLY] = 77;
    trk[1].micro[2] = -20;
    TDRUM->micro[9] = 12;
    lock_set(&trk[1], 2, P_ED_FLT, -30);
    lock_set(&trk[1], 2, P_E1, 5);
    lock_set(TDRUM, 9, P_DIST, 64);
    step_fill_set(&trk[1], 2, FC_FILL);
    step_fill_set(TDRUM, 9, FC_NOFILL);
    step_fill_set(TDRUM, 63, FC_FILL);
    proj_capture(&q);
    host_tracks_init();
    song.g[G_DRDLY] = 0;
    proj_apply(&q, 1);
    ok = q.drdly == 77 && song.g[G_DRDLY] == 77 &&
         trk[2].p[P_SLEN] == 7 && trk[1].step[2].n == 2 && trk[1].step[2].lvl == 0x0D && song.g[G_DUST] == 33 &&
         dstep_has(&TDRUM->dstep[9], 4) && dstep_lvl(&TDRUM->dstep[9], 4) == LV_SOFT && dstep_rat(&TDRUM->dstep[9], 4) == 1u &&
         trk[1].micro[2] == -20 && TDRUM->micro[9] == 12 && trk[1].micro[3] == 0 &&
         lock_find(&trk[1], 2, P_ED_FLT, 0) >= 0 && trk[1].lock[lock_find(&trk[1], 2, P_ED_FLT, 0)].val == -30 &&
         lock_find(&trk[1], 2, P_E1, 0) >= 0 && lock_find(TDRUM, 9, P_DIST, 0) >= 0 && lock_find(TDRUM, 9, P_E0, 0) < 0 &&
         step_fill(&trk[1], 2) == FC_FILL && step_fill(&trk[1], 3) == FC_NORM && step_fill(TDRUM, 9) == FC_NOFILL &&
         step_fill(TDRUM, 63) == FC_FILL && step_fill(TDRUM, 8) == FC_NORM;
    bad += check("the working project: capture -> apply round trip (levels, lanes, DUST, drum DLY, nudges, locks, fill conditions)", ok);
    q2 = q;
    q2.drdly = 0;
    proj_apply(&q2, 0);
    ok = song.g[G_DRDLY] == 0;
    q2.drdly = 200;
    proj_apply(&q2, 1);
    ok &= song.g[G_DRDLY] == 127;
    bad += check("drum DLY reuses the byte after sel; section/load clamp",
                 ok && G_DRDLY == PROJ_NG &&
                 offsetof(project_t, drdly) == 8u + 2u * PROJ_NG + 1u);
    /* TEXTURE: in the project's spare bytes; a section applies it too; the device MIDI settings never */
    song.g[G_TXSRC] = 10, song.g[G_TXLVL] = 127, song.g[G_TXTONE] = -64, song.g[G_TXMOVE] = 0;
    song.g[G_TXRATE] = 4, song.g[G_TXWHEN] = 1, song.g[G_TXDUCK] = 1;
    song.g[G_DRCH] = 16, song.g[G_CH1] = 5, song.g[G_BEND] = 12, song.g[G_INFO] = 1;
    proj_capture(&q2);
    song.g[G_TXSRC] = 0, song.g[G_TXLVL] = 0, song.g[G_TXTONE] = 0, song.g[G_TXRATE] = 0, song.g[G_TXDUCK] = 0;
    song.g[G_DRCH] = 9, song.g[G_CH1] = 2, song.g[G_BEND] = 3;
    proj_apply(&q2, 0);
    ok = song.g[G_TXSRC] == 10 && song.g[G_TXLVL] == 127 && song.g[G_TXTONE] == -64 && song.g[G_TXMOVE] == 0 &&
         song.g[G_TXRATE] == 4 && song.g[G_TXWHEN] == 1 && song.g[G_TXDUCK] == 1;
    proj_apply(&q2, 1);
    ok &= song.g[G_DRCH] == 9 && song.g[G_CH1] == 2 && song.g[G_BEND] == 3 && (q2.rsv[0] & 0x80u);
    bad += check("TEXTURE round trip (section and load); drum channel, CH1, BEND untouched by a project", ok);
    /* ORBIT 0.8.2: the delay TYPE in g[G_DRCH] bits 14..15, beside TEXTURE's LEVEL and TONE; a 0.8.1 project: MONO */
    ok = 1;
    for (t = 0; t < 3u; t++) {
        song.g[G_DTYPE] = (int16_t)t, song.g[G_TXLVL] = 127, song.g[G_TXTONE] = 63;
        proj_capture(&q2);
        song.g[G_DTYPE] = 0, song.g[G_TXLVL] = 0, song.g[G_TXTONE] = 0;
        proj_apply(&q2, 0);
        ok &= song.g[G_DTYPE] == (int16_t)t && song.g[G_TXLVL] == 127 && song.g[G_TXTONE] == 63;
    }
    q2.g[G_DRCH] = (int16_t)(q2.g[G_DRCH] & 0x3FFF);   /* (the bits as 0.8.1 wrote them) */
    proj_apply(&q2, 0);
    ok &= song.g[G_DTYPE] == 0 && song.g[G_TXLVL] == 127 && song.g[G_TXTONE] == 63;
    bad += check("delay TYPE round trip in the spare bits; a 0.8.1 project reads MONO, TEXTURE intact", ok);
    /* a damaged image: a lock on a parameter that cannot lock, on a step past the end, with a value past the
     * range: freed, freed, clamped */
    q.t[1].lock[5].step = 4, q.t[1].lock[5].param = P_SLEN, q.t[1].lock[5].val = 8;
    q.t[1].lock[6].step = 64, q.t[1].lock[6].param = P_E0, q.t[1].lock[6].val = 1;
    q.t[1].lock[7].step = 4, q.t[1].lock[7].param = P_LEVEL, q.t[1].lock[7].val = 999;
    q.t[1].lock[8].step = 4, q.t[1].lock[8].param = 200, q.t[1].lock[8].val = 1;
    pt_set_mf(&q.t[1], 4, 0, FC_FILL), pt_set_mf(&q.t[1], 5, 0, FC_NOFILL);
    pt_set_mf(&q.t[1], 6, 0, 3u), pt_set_mf(&q.t[1], 7, 0, 3u);   /* steps 7, 8: 3 (-> normal) */
    proj_apply(&q, 1);
    ok = trk[1].lock[5].step == LOCK_FREE &&
         trk[1].lock[6].step == LOCK_FREE && trk[1].lock[8].step == LOCK_FREE && trk[1].lock[7].step == 4 &&
         trk[1].lock[7].val == 127 && lock_find(&trk[1], 2, P_ED_FLT, 0) >= 0 &&
         step_fill(&trk[1], 4) == FC_FILL && step_fill(&trk[1], 5) == FC_NOFILL && trk[1].fill[1] == 0x09;
    bad += check("apply: a lock on LEN / step 64 / param 200 is freed, LEVEL 999 -> 127, condition 3 -> normal", ok);

    /* The new engine's raw values retain their width in existing FUN5 int16 fields. */
    host_tracks_init();
    host_preset(&trk[0], ENGI_CLUSTER, 0);
    trk[0].p[P_E1] = -32768;
    trk[0].p[P_E2] = 24575;
    trk[0].p[P_E3] = 32767;
    trk[0].p[P_E3] = -8193;
    lock_set(&trk[0], 1, P_E1, -32768);
    lock_set(&trk[0], 2, P_E3, 32767);
    proj_capture(&q);
    host_tracks_init();
    proj_apply(&q, 1);
    bad += check("ORP7: CLUSTER signed16 knobs and locks -32768 / 32767 survive capture/apply",
                 trk[0].p[P_E3] == -8193 && trk[0].lock[lock_find(&trk[0], 1, P_E1, 0)].val == -32768 &&
                 trk[0].lock[lock_find(&trk[0], 2, P_E3, 0)].val == 32767);
    bad += check("ORP7: CLUSTER ID and full signed16 raw knobs survive capture/apply",
                 trk[0].eng_req == ENGI_CLUSTER && trk[0].p[P_E0] == 17408 &&
                 trk[0].p[P_E1] == -32768 && trk[0].p[P_E2] == 24575);

    /* a lock in force while the snapshot is taken: the value under it is kept, not the step's */
    host_tracks_init();
    trk[1].p[P_DIST] = 10;
    trk[1].lk_n = 1, trk[1].lk_param[0] = P_DIST, trk[1].lk_base[0] = 10, trk[1].lk_set[0] = 99;
    trk[1].p[P_DIST] = 99;                          /* (lock_step wrote the lock's value) */
    trk[1].p[P_ITYPE] = 5, trk[1].lk_n = 2, trk[1].lk_param[1] = P_ITYPE, trk[1].lk_base[1] = 2, trk[1].lk_set[1] = 4;
    proj_capture(&q);                               /* P_ITYPE edited by hand since (5, not the lock's 4): kept */
    bad += check("capture: an active lock's base value, a manual edit since then wins; live state untouched",
                 q.t[1].p[P_DIST] == 10 && pt_get(&q.t[1], P_ITYPE) == 5 && trk[1].p[P_DIST] == 99 && trk[1].lk_n == 2);

    printf("%s\n", bad ? "PROJECT FORMAT TEST FAILED" : "project format test passed");
    return bad != 0;
}

