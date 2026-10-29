/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Projects: four slots in .noinit RAM (the song sections A..D), so they survive resets and UBOOT
 * entry. With FELUCCA_FLASH every save also goes to flash through storage.c, and an empty RAM slot
 * is filled from flash on load. The working project is also kept in flash by itself (autosave, when
 * the transport is stopped and nothing sounds) and comes back at power-on: SLOOP starts where you
 * left it.
 *
 * Formats: 8 ("ORP8", written, issue #5): per track the common parameters P_LEVEL..P_VLEAD (signed 16 bits),
 * the eight engine controls P_E0..P_E7 (signed 16 bits: CLUSTER keeps its raw values), the ten SLOOP 2.6 controls
 * P_ITYPE..P_LPOL in a byte each, engine, preset, the steps, each step's nudge (6 bits) and fill condition (2 bits)
 * in one byte, NLOCK parameter locks, the four HOME macro assignments, the user preset slot the sound came from
 * and the track FILTER's RES | TYPE << 7 in the last byte. 3840 bytes: one flash object. 7 ("ORP7", ORBIT 0.7.0)
 * is the same layout with that byte 0 and P_E0 at 63: read, its engine controls' ids (locks, macros) moved up by
 * two, RES and TYPE their defaults. SLOOP 2.6's "FUN6" has another engine registry and is rejected, never loaded.
 * Read and converted (frozen layouts): 5 ("FUN5", ORBIT 0.4 .. 0.6 and SLOOP 2.4 .. 2.5: 61 parameters, the nudges,
 * locks and conditions; by count, a lock on an engine control follows it to its new id), 4 ("FUN4", SLOOP 2.0 .. 2.3:
 * PROJ_NP_V4 parameters, 10-byte steps with levels and ratchets, the drum track's 16 lanes; no nudge, no lock), 3
 * ("FUN3", SLOOP 1.x: 8-byte steps, the drum track's notes become its lanes, the swings x 0.8 for the MPC scale),
 * 2 ("FUN2") and 1 ("FUN1"), which held PROJ_NP_V2 parameters per track, mapped by count as user presets are (the
 * first np - 8 are P_LEVEL.. in order, the last 8 P_E0..P_E7; the parameters added since take their defaults).
 * Their engine bytes are kept: ORBIT engine ids are append-only; the drum track's byte (it has no engine) becomes 0.
 * Legacy magics are shared with upstream SLOOP: a legacy project is always read as ORBIT's (no provenance exists).
 *
 * Built on the host too (tests/project_test.c, -DPROJ_HOST): the part above the #ifndef
 * PROJ_HOST needs core.h, params.c (TP), drums.c (the lanes), the engines and trk_def_engine (ui.c). */
#define PROJ_MAGIC 0x3850524Fu                 /* "ORP8" (bytes 4F 52 50 38): written */
#define PROJ_MAGIC_V7 0x3750524Fu              /* "ORP7": ORBIT 0.7.0; read */
#define PROJ_E0_V7 63u                         /* P_E0 of format 7 */
#define PROJ_MAGIC_FUN6 0x46554E36u            /* "FUN6": SLOOP 2.6, another engine namespace: rejected */
#define PROJ_MAGIC_V5 0x46554E35u              /* "FUN5": format 4 + per-step nudge, locks, fill conditions; read only */
#define PROJ_MAGIC_V4 0x46554E34u              /* "FUN4": four tracks, P_COUNT parameters each, 10-byte steps; read only */
#define PROJ_MAGIC_V3 0x46554E33u              /* "FUN3": SLOOP 1.x; read only */
#define PROJ_MAGIC_V2 0x46554E32u              /* "FUN2": four tracks, PROJ_NP_V2 parameters; read only */
#define PROJ_MAGIC_V1 0x46554E31u              /* "FUN1": one instrument; loads into track 1 */
#define PROJ_NP_V5 61u                         /* P_COUNT of format 5 (P_E0 was 53) */
#define PROJ_NP_V4 58u                         /* P_COUNT of format 4 (P_E0 was 50) */
#define PROJ_NP_V3 57u                         /* P_COUNT of format 3 (P_E0 was 49) */
#define PROJ_NG_V3 27u                         /* G_COUNT of formats 1..3 */
#define PROJ_NP_V2 53u                         /* P_COUNT of formats 1 and 2 (P_E0 was 45) */
#define PROJ_NG_V2 27u                         /* G_COUNT of formats 1 and 2 */
#define PROJ_NG 32u                            /* the globals a project holds (G_DRDLY rides in its own byte) */
#define PROJ_NC7 53u                           /* format 7: common parameters kept in 16 bits, P_LEVEL .. P_VLEAD */
#define PROJ_NX7 10u                           /* .. the 2.6 ones in a byte each, P_ITYPE .. P_LPOL (then format 8's flt) */
_Static_assert(G_DRDLY == PROJ_NG && G_TXSRC == PROJ_NG + 5u && G_DTYPE == G_BEND + 1u && G_COUNT == G_DTYPE + 1u && G_COUNT == 46u,
               "the globals a project holds, then G_DRDLY, then the 2.6 device settings and TEXTURE");
_Static_assert(PROJ_NC7 == (uint32_t)P_ITYPE && PROJ_NX7 == (uint32_t)(P_TRES - P_ITYPE) && P_TFTYPE == P_TRES + 1 &&
               P_E0 == P_TFTYPE + 1 && P_E0 == PROJ_E0_V7 + 2u && P_COUNT == 73 && PROJ_NC7 == PROJ_NP_V5 - 8u,
               "format 8: format 5's common parameters, ten bytes, eight engine controls, the FILTER's byte");
typedef struct {                               /* one track of formats 7 and 8 */
    int16_t p[PROJ_NC7];                       /* P_LEVEL .. P_VLEAD (pt_get, pt_set) */
    int16_t e[8];                              /* P_E0 .. P_E7, raw signed 16 bits */
    uint8_t x[PROJ_NX7];                       /* P_ITYPE .. P_LPOL (2.6) */
    uint8_t engine, preset;                    /* (the drum track: 0) */
    union {
        step_t step[NSTEP];
        dstep_t dstep[NSTEP];                  /* (the drum track: 16 lanes, the same size) */
    };
    uint8_t mf[NSTEP];                         /* each step's nudge (6 bits, signed) | its fill condition << 6 */
    plock_t lock[NLOCK];                       /* parameter locks (step LOCK_FREE = none) */
    uint8_t macro[MACRO_COUNT];                /* HOME KNOB 1..4: a P_* id, MACRO_DEFAULT = the engine's own */
    uint8_t user;                              /* the user preset slot + 1 the sound came from, 0 = none (a tag) */
    uint8_t flt;                               /* P_TRES | P_TFTYPE << 7 (format 7: 0, reserved) */
} proj_trk_t;
typedef struct {
    uint32_t magic, size;
    int16_t g[PROJ_NG];
    uint8_t sel, drdly, rsv[2];                /* selected track, drum delay send, TEXTURE (proj_tx_put) */
    proj_trk_t t[NTRK];
    uint32_t sum;
} project_t;
_Static_assert(sizeof(plock_t) == 4u && NSTEP == 64 && NLOCK == 24 && MACRO_COUNT == 4u, "format 7 sizes");
_Static_assert(__builtin_offsetof(proj_trk_t, p) == 0u && __builtin_offsetof(proj_trk_t, e) == 106u &&
               __builtin_offsetof(proj_trk_t, x) == 122u && __builtin_offsetof(proj_trk_t, engine) == 132u &&
               __builtin_offsetof(proj_trk_t, preset) == 133u && __builtin_offsetof(proj_trk_t, step) == 134u &&
               __builtin_offsetof(proj_trk_t, mf) == 774u && __builtin_offsetof(proj_trk_t, lock) == 838u &&
               __builtin_offsetof(proj_trk_t, macro) == 934u && __builtin_offsetof(proj_trk_t, user) == 938u &&
               __builtin_offsetof(proj_trk_t, flt) == 939u && sizeof(proj_trk_t) == 940u,
               "format 7 / 8 track layout");
_Static_assert(__builtin_offsetof(project_t, g) == 8u && __builtin_offsetof(project_t, sel) == 72u &&
               __builtin_offsetof(project_t, drdly) == 73u && __builtin_offsetof(project_t, rsv) == 74u &&
               __builtin_offsetof(project_t, t) == 76u && __builtin_offsetof(project_t, sum) == 3836u &&
               sizeof(project_t) == 3840u, "format 7 is exactly one 3840-byte flash object");
static int16_t pt_get(const proj_trk_t *t, uint32_t id)   /* p[id] of a stored track */
{
    return id < PROJ_NC7 ? t->p[id] : id < (uint32_t)P_TRES ? (int16_t)t->x[id - PROJ_NC7]
           : id == (uint32_t)P_TRES ? (int16_t)(t->flt & 127u) : id == (uint32_t)P_TFTYPE ? (int16_t)(t->flt >> 7)
           : t->e[(id - P_E0) & 7u];
}
static void pt_set(proj_trk_t *t, uint32_t id, int32_t v)
{
    if (id < PROJ_NC7)
        t->p[id] = (int16_t)v;
    else if (id < (uint32_t)P_TRES)
        t->x[id - PROJ_NC7] = (uint8_t)clamp(v, TP[id].min, TP[id].max);   /* (their descriptors: all within 0..127) */
    else if (id == (uint32_t)P_TRES)
        t->flt = (uint8_t)((t->flt & 128u) | (uint32_t)clamp(v, 0, 127));
    else if (id == (uint32_t)P_TFTYPE)
        t->flt = (uint8_t)((t->flt & 127u) | (v > 0 ? 128u : 0u));
    else
        t->e[(id - P_E0) & 7u] = (int16_t)v;
}
static int32_t pt_micro(const proj_trk_t *t, uint32_t i) { int32_t m = t->mf[i] & 63; return m >= 32 ? m - 64 : m; }
static uint32_t pt_fill(const proj_trk_t *t, uint32_t i) { return (uint32_t)t->mf[i] >> 6; }
static void pt_set_mf(proj_trk_t *t, uint32_t i, int32_t micro, uint32_t fill)
{
    t->mf[i] = (uint8_t)(((uint32_t)micro & 63u) | (fill & 3u) << 6);
}
typedef struct {                               /* a track of format 5 (ORBIT 0.4 .. 0.6, SLOOP 2.4 .. 2.5), frozen */
    int16_t p[PROJ_NP_V5];
    uint8_t engine, preset;
    union {
        step_t step[NSTEP];
        dstep_t dstep[NSTEP];
    };
    int8_t micro[NSTEP];
    plock_t lock[NLOCK];
    uint8_t fill[NSTEP / 4];
} proj_trk_v5_t;
typedef struct {                               /* format 5, read only */
    uint32_t magic, size;
    int16_t g[PROJ_NG];
    uint8_t sel, drdly, rsv[2];
    proj_trk_v5_t t[NTRK];
    uint32_t sum;
} project_v5_t;
typedef struct {                               /* a track of format 4 (SLOOP 2.0 .. 2.3), read only */
    int16_t p[PROJ_NP_V4];
    uint8_t engine, preset;
    union {
        step_t step[NSTEP];
        dstep_t dstep[NSTEP];
    };
} proj_trk_v4_t;
typedef struct {                               /* format 4, read only */
    uint32_t magic, size;
    int16_t g[PROJ_NG];
    uint8_t sel, rsv[3];
    proj_trk_v4_t t[NTRK];
    uint32_t sum;
} project_v4_t;
typedef struct { uint8_t note[4], n, time, flags, vel; } step8_t;   /* the steps of formats 1..3 */
typedef struct {                               /* a track of format 3, read only */
    int16_t p[PROJ_NP_V3];
    uint8_t engine, preset;
    step8_t step[NSTEP];
} proj_trk_v3_t;
typedef struct {                               /* format 3 (SLOOP 1.x), read only */
    uint32_t magic, size;
    int16_t g[PROJ_NG_V3];
    uint8_t sel, rsv[3];
    proj_trk_v3_t t[NTRK];
    uint32_t sum;
} project_v3_t;
typedef struct {                               /* a track of formats 1 and 2, read only */
    int16_t p[PROJ_NP_V2];
    uint8_t engine, preset;
    step8_t step[NSTEP];
} proj_trk_v2_t;
typedef struct {                               /* format 2 (until 0.9), read only */
    uint32_t magic, size;
    int16_t g[PROJ_NG_V2];
    uint8_t sel, rsv[3];
    proj_trk_v2_t t[NTRK];
    uint32_t sum;
} project_v2_t;
typedef struct {                               /* format 1 (until 0.5 beta), read only */
    uint32_t magic, size;
    int16_t g[PROJ_NG_V2];
    proj_trk_v2_t t;
    uint32_t sum;
} project_v1_t;
_Static_assert(sizeof(project_v2_t) == 2552u && sizeof(project_v1_t) == 688u && sizeof(project_v3_t) == 2584u &&
               sizeof(project_v4_t) == 3112u, "formats 1 / 2 / 3 / 4 as they were stored");
_Static_assert(sizeof(project_v5_t) == 3840u && __builtin_offsetof(project_v5_t, t) == 76u &&
               __builtin_offsetof(proj_trk_v5_t, micro) == 764u && __builtin_offsetof(proj_trk_v5_t, fill) == 924u &&
               sizeof(proj_trk_v5_t) == 940u,
               "format 5 as it was stored");
project_t proj_slot[4] __attribute__((section(".noinit")));

static uint32_t proj_hash(const void *p, uint32_t n)   /* FNV-1a over n bytes */
{
    const uint8_t *b = (const uint8_t *)p;
    uint32_t i, s = 0x811C9DC5u;
    for (i = 0; i < n; i++)
        s = (s ^ b[i]) * 16777619u;
    return s;
}
static uint32_t proj_sum(const project_t *p) { return proj_hash(p, sizeof *p - 4u); }
/* a format 8 (or, magic PROJ_MAGIC_V7, 7) project as stored: its header and checksum, then what no writer of it leaves
 * (format 7: a reserved byte not 0; an engine this firmware does not have, a user slot out of range) */
static int proj_ok_as(const project_t *q, uint32_t magic)
{
    uint32_t k;
    if (q->magic != magic || q->size != sizeof *q || q->sum != proj_sum(q))
        return 0;
    for (k = 0; k < NTRK; k++)
        if ((magic == PROJ_MAGIC_V7 && q->t[k].flt) || q->t[k].user > UP_SLOTS || (k < NPART && q->t[k].engine >= NENGINES))
            return 0;
    return 1;
}
static int proj_ok(const project_t *q) { return proj_ok_as(q, PROJ_MAGIC); }

/* ---- old formats -> format 7 */
/* an old step into a synth step (no level, no ratchet) */
static void step_from8(step_t *d, const step8_t *s)
{
    memcpy(d->note, s->note, 4);
    d->n = s->n;
    d->time = s->time;
    d->flags = s->flags;
    d->vel = s->vel;
    d->lvl = d->rat = 0;
}
/* an old drum step (GM notes) into the drum track's lanes; its velocity / accent -> their level */
static void dstep_from8(dstep_t *d, const step8_t *s)
{
    uint32_t i, lvl = (s->flags & SF_ACCENT) || s->vel > 115u ? LV_HARD : !s->vel ? LV_NORM : vel_lvl(s->vel);
    memset(d, 0, sizeof *d);
    if (s->time != ST_NOTE)
        return;
    for (i = 0; i < s->n && i < 4u; i++)
        dstep_set(d, lane_of_note(s->note[i] & 127u), lvl, 0);
}
static int16_t swing_from_v3(int32_t v) { return (int16_t)clamp((v * 4 + 2) / 5, 0, 100); }   /* /250 -> /200 */

/* the globals of formats 1..3 (G_* unchanged since; any added later: their defaults) */
static void proj_g_from_old(int16_t *g, const int16_t *g2)
{
    uint32_t i;
    for (i = 0; i < PROJ_NG; i++)
        g[i] = i < PROJ_NG_V3 ? g2[i] : GP[i].def;
    g[G_SWING] = swing_from_v3(g[G_SWING]);
}

/* no nudge, no lock, no condition (formats 1..4); the 2.6 parameters at their defaults, the engine's own HOME
 * macros, no user preset tag (formats 1..5) */
static void proj_trk_plain(proj_trk_t *d)
{
    uint32_t k;
    memset(d->mf, 0, sizeof d->mf);
    for (k = PROJ_NC7; k < (uint32_t)P_E0; k++)
        pt_set(d, k, TP[k].def);
    memset(d->macro, MACRO_DEFAULT, sizeof d->macro);
    d->user = 0;
    for (k = 0; k < NLOCK; k++) {
        d->lock[k].step = LOCK_FREE;
        d->lock[k].param = 0;
        d->lock[k].val = 0;
    }
}

/* a track of format 3 -> today's (by id up to P_SLDEPTH; P_E0.. moved) */
static void proj_trk_from_v3(proj_trk_t *d, const proj_trk_v3_t *s, int drum)
{
    uint32_t k, nc = PROJ_NP_V3 - 8u;
    proj_trk_plain(d);
    for (k = 0; k < PROJ_NC7; k++)
        d->p[k] = k < nc ? s->p[k] : TP[k].def;
    for (k = 0; k < 8u; k++)
        d->e[k] = s->p[nc + k];
    d->p[P_SSWING] = swing_from_v3(d->p[P_SSWING]);
    d->p[P_ASWING] = swing_from_v3(d->p[P_ASWING]);
    d->engine = drum ? 0u : s->engine;
    d->preset = drum ? 0u : s->preset;
    for (k = 0; k < NSTEP; k++) {
        if (drum)
            dstep_from8(&d->dstep[k], &s->step[k]);
        else
            step_from8(&d->step[k], &s->step[k]);
    }
}

/* a track of formats 1 and 2 -> format 3 (mapped by count, see the top) */
static void proj_trk_v2_to_v3(proj_trk_v3_t *d, const proj_trk_v2_t *s, int drum)
{
    uint32_t k, nc = PROJ_NP_V2 - 8u;
    for (k = 0; k < PROJ_NP_V3 - 8u; k++)
        d->p[k] = k < nc ? s->p[k] : TP[k].def;
    for (k = 0; k < 8u; k++)
        d->p[PROJ_NP_V3 - 8u + k] = s->p[nc + k];
    d->engine = drum ? 0u : s->engine;          /* (indices 0..7 as they were) */
    d->preset = drum ? 0u : s->preset;
    memcpy(d->step, s->step, sizeof d->step);
}

/* a format 4 project (n bytes in *v4) -> slot q as format 7: the same, no nudge, no lock, no condition, no FILTER */
static int proj_from_v4(project_t *q, const project_v4_t *v4, int n)
{
    uint32_t i;
    if (n != (int)sizeof *v4 || v4->magic != PROJ_MAGIC_V4 || v4->size != sizeof *v4 ||
        v4->sum != proj_hash(v4, sizeof *v4 - 4u))
        return 0;
    memset(q, 0, sizeof *q);
    q->magic = PROJ_MAGIC;
    q->size = sizeof *q;
    memcpy(q->g, v4->g, sizeof q->g);
    q->sel = v4->sel;
    for (i = 0; i < NTRK; i++) {
        uint32_t k;
        proj_trk_plain(&q->t[i]);
        for (k = 0; k < PROJ_NC7; k++)                 /* by id up to P_CHORD, the parameters added since: */
            q->t[i].p[k] = k < PROJ_NP_V4 - 8u ? v4->t[i].p[k] : TP[k].def;   /* their defaults; P_E0.. moved */
        for (k = 0; k < 8u; k++)
            q->t[i].e[k] = v4->t[i].p[PROJ_NP_V4 - 8u + k];
        q->t[i].engine = i == TRK_DRUM ? 0u : v4->t[i].engine;
        q->t[i].preset = i == TRK_DRUM ? 0u : v4->t[i].preset;
        memcpy(q->t[i].step, v4->t[i].step, sizeof q->t[i].step);
    }
    q->sum = proj_sum(q);
    return 1;
}

/* a format 5 project (n bytes in *v5) -> slot q as format 7: by count (the 2.6 parameters at their defaults, an
 * engine control's lock follows it to its new id, a lock on no parameter is freed), each step's nudge and condition
 * into its byte. q must not be v5 (the layouts differ at the same size) */
static int proj_from_v5(project_t *q, const project_v5_t *v5, int n)
{
    uint32_t i, k;
    if (n != (int)sizeof *v5 || v5->magic != PROJ_MAGIC_V5 || v5->size != sizeof *v5 ||
        v5->sum != proj_hash(v5, sizeof *v5 - 4u))
        return 0;
    memset(q, 0, sizeof *q);
    q->magic = PROJ_MAGIC;
    q->size = sizeof *q;
    memcpy(q->g, v5->g, sizeof q->g);
    q->sel = v5->sel;
    q->drdly = v5->drdly;
    for (i = 0; i < NTRK; i++) {
        const proj_trk_v5_t *s = &v5->t[i];
        proj_trk_t *d = &q->t[i];
        proj_trk_plain(d);
        for (k = 0; k < PROJ_NC7; k++)
            d->p[k] = s->p[k];
        for (k = 0; k < 8u; k++)
            d->e[k] = s->p[PROJ_NP_V5 - 8u + k];
        d->engine = i == TRK_DRUM ? 0u : s->engine;
        d->preset = i == TRK_DRUM ? 0u : s->preset;
        memcpy(d->step, s->step, sizeof d->step);
        for (k = 0; k < NSTEP; k++)
            pt_set_mf(d, k, clamp(s->micro[k], MICRO_MIN, MICRO_MAX), (uint32_t)s->fill[k / 4u] >> (2u * (k % 4u)));
        for (k = 0; k < NLOCK; k++) {
            uint32_t id = param_id_by_count(s->lock[k].param, PROJ_NP_V5);
            if (s->lock[k].step == LOCK_FREE || id >= P_COUNT)
                continue;                              /* (proj_trk_plain: free) */
            d->lock[k].step = s->lock[k].step;
            d->lock[k].param = (uint8_t)id;
            d->lock[k].val = s->lock[k].val;
        }
    }
    q->sum = proj_sum(q);
    return 1;
}

/* an id of format 7 (P_E0 at 63) -> today's: the engine controls two up (RES, TYPE before them) */
static uint32_t proj_id_from_v7(uint32_t id)
{
    return id < PROJ_E0_V7 ? id : id < PROJ_E0_V7 + 8u ? id + 2u : P_COUNT;
}
/* a format 7 project (n bytes in *v7) -> slot q as format 8: the same bytes, the engine controls' locks and HOME
 * macros at their new ids, RES and TYPE their defaults */
static int proj_from_v7(project_t *q, const project_t *v7, int n)
{
    uint32_t i, k;
    if (n != (int)sizeof *v7 || !proj_ok_as(v7, PROJ_MAGIC_V7))
        return 0;
    memcpy(q, v7, sizeof *q);
    q->magic = PROJ_MAGIC;
    for (i = 0; i < NTRK; i++) {
        proj_trk_t *d = &q->t[i];
        pt_set(d, P_TRES, TP[P_TRES].def);
        pt_set(d, P_TFTYPE, TP[P_TFTYPE].def);
        for (k = 0; k < NLOCK; k++)
            if (d->lock[k].step != LOCK_FREE)
                d->lock[k].param = (uint8_t)proj_id_from_v7(d->lock[k].param);
        for (k = 0; k < MACRO_COUNT; k++)
            if (d->macro[k] != MACRO_DEFAULT)
                d->macro[k] = (uint8_t)proj_id_from_v7(d->macro[k]);
    }
    q->sum = proj_sum(q);
    return 1;
}

/* a format 3 project -> slot q as format 7 */
static void proj_from_v3_ok(project_t *q, const project_v3_t *v3)
{
    uint32_t i;
    memset(q, 0, sizeof *q);
    q->magic = PROJ_MAGIC;
    q->size = sizeof *q;
    proj_g_from_old(q->g, v3->g);
    q->sel = v3->sel;
    for (i = 0; i < NTRK; i++)
        proj_trk_from_v3(&q->t[i], &v3->t[i], i == TRK_DRUM);
    q->sum = proj_sum(q);
}
static int proj_from_v3(project_t *q, const project_v3_t *v3, int n)
{
    if (n != (int)sizeof *v3 || v3->magic != PROJ_MAGIC_V3 || v3->size != sizeof *v3 ||
        v3->sum != proj_hash(v3, sizeof *v3 - 4u))
        return 0;
    proj_from_v3_ok(q, v3);
    return 1;
}

static project_v3_t proj_v3_tmp;               /* (formats 1, 2: through format 3) */
/* a format 2 project (n bytes in *v2) -> slot q as format 7 */
static int proj_from_v2(project_t *q, const project_v2_t *v2, int n)
{
    project_v3_t *v3 = &proj_v3_tmp;
    uint32_t i;
    if (n != (int)sizeof *v2 || v2->magic != PROJ_MAGIC_V2 || v2->size != sizeof *v2 ||
        v2->sum != proj_hash(v2, sizeof *v2 - 4u))
        return 0;
    memset(v3, 0, sizeof *v3);
    memcpy(v3->g, v2->g, sizeof v3->g);
    v3->sel = v2->sel;
    for (i = 0; i < NTRK; i++)
        proj_trk_v2_to_v3(&v3->t[i], &v2->t[i], i == TRK_DRUM);
    proj_from_v3_ok(q, v3);
    return 1;
}

/* a format 1 project (n bytes in *v1) -> slot q as format 7: the instrument becomes track 1,
 * tracks 2..4 start empty (their sounds as at power-on) */
static int proj_from_v1(project_t *q, const project_v1_t *v1, int n)
{
    project_v3_t *v3 = &proj_v3_tmp;
    uint32_t i;
    if (n != (int)sizeof *v1 || v1->magic != PROJ_MAGIC_V1 || v1->size != sizeof *v1 ||
        v1->sum != proj_hash(v1, sizeof *v1 - 4u))
        return 0;
    memset(v3, 0, sizeof *v3);
    memcpy(v3->g, v1->g, sizeof v3->g);
    proj_trk_v2_to_v3(&v3->t[0], &v1->t, 0);
    proj_from_v3_ok(q, v3);
    for (i = 1; i < NTRK; i++) {               /* the other tracks: their defaults, no steps */
        uint32_t k;
        for (k = 0; k < P_COUNT; k++)
            pt_set(&q->t[i], k, k >= P_E0 ? ENGINES[trk_def_engine(i)]->edit[k - P_E0].def : TP[k].def);
        q->t[i].engine = (uint8_t)trk_def_engine(i);
        q->t[i].preset = 0xFF;                 /* 0xFF: its default preset (project_load) */
        memset(q->t[i].step, 0, sizeof q->t[i].step);
        if (i != TRK_DRUM)
            for (k = 0; k < NSTEP; k++)
                q->t[i].step[k].time = ST_REST;
        proj_trk_plain(&q->t[i]);
    }
    q->sum = proj_sum(q);
    return 1;
}

/* n bytes of a stored project (any ORBIT format) -> slot q as format 7; 0 = not a project, q untouched. b and q are
 * distinct buffers (a legacy layout is read while q is written). SLOOP 2.6's FUN6 is never an ORBIT project */
static int proj_import(project_t *q, const void *b, int n)
{
    uint32_t magic;
    if (n < 4 || (const void *)q == b)
        return 0;
    memcpy(&magic, b, 4);
    if (magic == PROJ_MAGIC_FUN6)
        return 0;
    if (magic == PROJ_MAGIC) {
        if (n != (int)sizeof *q || !proj_ok((const project_t *)b))
            return 0;
        memcpy(q, b, sizeof *q);
        return 1;
    }
    switch (magic) {
    case PROJ_MAGIC_V7:
        return proj_from_v7(q, (const project_t *)b, n);
    case PROJ_MAGIC_V5:
        return proj_from_v5(q, (const project_v5_t *)b, n);
    case PROJ_MAGIC_V4:
        return proj_from_v4(q, (const project_v4_t *)b, n);
    case PROJ_MAGIC_V3:
        return proj_from_v3(q, (const project_v3_t *)b, n);
    case PROJ_MAGIC_V2:
        return proj_from_v2(q, (const project_v2_t *)b, n);
    case PROJ_MAGIC_V1:
        return proj_from_v1(q, (const project_v1_t *)b, n);
    default:
        return 0;
    }
}

/* ---- the working project <-> a project_t */
/* 2.6: GLO > TEXTURE in a project (a song section has its own): in the slots of two globals a project does not apply
 * (G_DRCH: a setting of the FM-1; G_INFO: the CPU readout) and the spare bytes. rsv[0]: 0x80 (there) | SRC
 * | WHEN << 4 | DUCK << 5, rsv[1]: RATE, g[G_DRCH]: LEVEL | (TONE + 64) << 7 | the delay TYPE << 14 (ORBIT 0.8.2:
 * bits a project before it left 0, MONO; older firmware masks them off), g[G_INFO]: MOVE. Without the 0x80 (a
 * project before it, converted): the defaults, OFF */
static void proj_tx_put(project_t *p)
{
    p->rsv[0] = (uint8_t)(0x80u | ((uint32_t)song.g[G_TXSRC] & 15u) | ((uint32_t)song.g[G_TXWHEN] & 1u) << 4 |
                          ((uint32_t)song.g[G_TXDUCK] & 1u) << 5);
    p->rsv[1] = (uint8_t)song.g[G_TXRATE];
    p->g[G_DRCH] = (int16_t)(uint16_t)((song.g[G_TXLVL] & 127) | ((song.g[G_TXTONE] + 64) & 127) << 7 |
                                       (song.g[G_DTYPE] & 3) << 14);
    p->g[G_INFO] = song.g[G_TXMOVE];
}
static void proj_tx_get(const project_t *p)
{
    static const uint8_t ID[8] = {G_TXSRC, G_TXLVL, G_TXTONE, G_TXMOVE, G_TXRATE, G_TXWHEN, G_TXDUCK, G_DTYPE};
    int32_t v[8];
    uint32_t i;
    if (!(p->rsv[0] & 0x80u)) {
        for (i = 0; i < 8u; i++)
            v[i] = GP[ID[i]].def;
    } else {
        v[0] = p->rsv[0] & 15u, v[5] = (p->rsv[0] >> 4) & 1u, v[6] = (p->rsv[0] >> 5) & 1u, v[4] = p->rsv[1];
        v[1] = p->g[G_DRCH] & 127, v[2] = ((p->g[G_DRCH] >> 7) & 127) - 64, v[3] = p->g[G_INFO];
        v[7] = ((uint16_t)p->g[G_DRCH] >> 14) & 3u;
    }
    for (i = 0; i < 8u; i++)
        song.g[ID[i]] = (int16_t)clamp(v[i], GP[ID[i]].min, GP[ID[i]].max);
}

static void proj_capture(project_t *p)        /* what is playing now, as a project */
{
    uint32_t i;
    memset(p, 0, sizeof *p);
    p->magic = PROJ_MAGIC;
    p->size = sizeof *p;
    for (i = 0; i < PROJ_NG; i++)
        p->g[i] = song.g[i];
    p->sel = song.sel;
    p->drdly = (uint8_t)song.g[G_DRDLY];
    proj_tx_put(p);
    for (i = 0; i < NTRK; i++) {
        const track_t *t = &trk[i];
        proj_trk_t *d = &p->t[i];
        uint32_t k;
        for (k = 0; k < P_COUNT; k++)
            pt_set(d, k, trk_base_value(t, k));
        if (i == TRK_DRUM) {
            memset(d->macro, MACRO_DEFAULT, sizeof d->macro);   /* (engine, preset, user: 0) */
        } else {
            d->engine = t->eng_req;
            d->preset = t->preset;
            memcpy(d->macro, t->macro, sizeof d->macro);
            d->user = (uint8_t)(t->user <= UP_SLOTS ? t->user : 0u);
        }
        memcpy(d->step, t->step, sizeof t->step);
        for (k = 0; k < NSTEP; k++)
            pt_set_mf(d, k, t->micro[k], (uint32_t)t->fill[k / 4u] >> (2u * (k % 4u)));
        for (k = 0; k < NLOCK; k++)
            if (t->lock[k].step < NSTEP)
                d->lock[k] = t->lock[k];
            else
                d->lock[k].step = LOCK_FREE;           /* (a free slot: {255, 0, 0}) */
    }
    p->sum = proj_sum(p);
}

/* a project's tracks (and its globals, all: a load; or only the drum level / reverb / delay and TEXTURE: a song
 * section) into the working one, every value back inside its range. The audio ISR must not run meanwhile (the
 * song sections: called from it; a load: IRQ off). The device settings (SYNC, MIDI, IN, the channels) stay */
static void proj_apply(const project_t *p, int all)
{
    uint32_t i, k;
    for (i = 0; i < PROJ_NG; i++)
        if (all ? i != G_SLOT && i != G_LOAD && i != G_SAVE && i != G_SYNC && i != G_MIDI && i != G_ROUTE && i != G_DRCH &&
                      i != G_INFO
                : i == G_DRLVL || i == G_DRREV)
            song.g[i] = (int16_t)clamp(p->g[i], GP[i].min, GP[i].max);
    song.g[G_DRDLY] = (int16_t)clamp(p->drdly, GP[G_DRDLY].min, GP[G_DRDLY].max);
    proj_tx_get(p);                                     /* GLO > TEXTURE (2.6): a load and a section */
    for (k = 0; k < NTRK; k++) {
        track_t *t = &trk[k];
        const proj_trk_t *s = &p->t[k];
        uint32_t e = k < NPART && s->engine < NENGINES ? s->engine : 0u;
        t->eng_req = (uint8_t)e;
        t->user = (uint8_t)(k < NPART && s->user <= UP_SLOTS ? s->user : 0u);   /* (a tag only: no up_load) */
        for (i = 0; i < MACRO_COUNT; i++)               /* an id this firmware has, else the engine's own */
            t->macro[i] = (uint8_t)(k < NPART && s->macro[i] < P_COUNT ? s->macro[i] : MACRO_DEFAULT);
        t->lk_n = 0;                                    /* (the locks in force: the values come from the project) */
        for (i = 0; i < P_COUNT; i++) {                 /* every value back inside its range */
            const param_desc_t *d = k == TRK_DRUM && i == P_E0 ? &DRUM_KIT_DESC :   /* the drum kit */
                                    i >= P_E0 && i <= P_E7 ? &ENGINES[e]->edit[i - P_E0] : &TP[i];
            t->p[i] = (int16_t)clamp(pt_get(s, i), d->min, d->max);
        }
        t->preset = (uint8_t)(ENGINES[e]->npresets ? (s->preset == 0xFFu ? 0u : s->preset) % ENGINES[e]->npresets : 0u);
        memcpy(t->step, s->step, sizeof t->step);
        memset(t->fill, 0, sizeof t->fill);
        for (i = 0; i < NSTEP; i++) {                   /* the nudges and conditions (3 means normal: 0) */
            uint32_t c = pt_fill(s, i);
            t->micro[i] = (int8_t)clamp(pt_micro(s, i), MICRO_MIN, MICRO_MAX);
            if (c != 3u)
                t->fill[i / 4u] |= (uint8_t)(c << (2u * (i % 4u)));
        }
        for (i = 0; i < NLOCK; i++) {
            const plock_t *l = &s->lock[i];
            uint32_t id = l->param;
            if (l->step < NSTEP && id < P_COUNT && p_lockable(id)) {
                const param_desc_t *d = k == TRK_DRUM && id == P_E0 ? &DRUM_KIT_DESC :
                                        id >= P_E0 && id <= P_E7 ? &ENGINES[e]->edit[id - P_E0] : &TP[id];
                t->lock[i].step = l->step;
                t->lock[i].param = (uint8_t)id;
                t->lock[i].val = (int16_t)clamp(l->val, d->min, d->max);
            } else {
                t->lock[i].step = LOCK_FREE;            /* no such step or parameter: the slot is free */
                t->lock[i].param = 0;
                t->lock[i].val = 0;
            }
        }
        if (k != TRK_DRUM)
            for (i = 0; i < NSTEP; i++) {
                step_t *st = &t->step[i];
                uint32_t j;
                if (st->n > 4u)
                    st->n = 4;
                if (st->time > ST_REST)
                    st->time = ST_REST;
                for (j = 0; j < 4u; j++)
                    st->note[j] &= 127u;
            }
        fm6_track_loaded(t);                            /* FM6: the project keeps PTCH, not the patch: its slot's */
    }
}

#ifndef PROJ_HOST
#if FELUCCA_ARRANGER
#include "arranger_scene.c"
#endif
static uint8_t sec_dirty, song_dirty;           /* live sections / the song: in RAM, not yet in flash */
#if FELUCCA_FLASH
/* slot from flash into RAM (format 7, or an old one converted): the staging of every layout */
static union {
    project_t v7;
    project_v5_t v5;
    project_v4_t v4;
    project_v3_t v3;
    project_v2_t v2;
    project_v1_t v1;
} proj_tmp;
#if !UP_RAM
_Static_assert(sizeof proj_tmp >= sizeof(up_bank_t), "a user preset bank is staged in the project buffer");
static void *up_stage(void) { return &proj_tmp; }   /* upreset.c: a user preset bank staged for a write */
#endif
static void proj_fetch(uint32_t slot)
{
    project_t *q = &proj_slot[slot & 3u];
    int n = st_load(OBJ_PROJECT0 + (slot & 3u), &proj_tmp, sizeof proj_tmp);
    if (!proj_import(q, &proj_tmp, n))
        q->magic = 0;
}
#include "fm6_bank.c"                           /* the FM6 patch bank (eng_fm6.c PTCH B1..B27): staged in proj_tmp */
#endif

static void project_save(uint32_t slot)
{
    project_t *p = &proj_slot[slot & 3u];
#if FELUCCA_ARRANGER
    if (song.playing || transport_req) { ui_message("STOP BEFORE SAVE"); return; }
#endif
    proj_capture(p);
#if FELUCCA_FLASH
    if (flash_ok) {
        ui_message(st_save(OBJ_PROJECT0 + (slot & 3u), p, sizeof *p) ? "SAVE ERROR" : "SAVED");
        return;
    }
#endif
    ui_message("SAVED (RAM)");
}

/* SAVE > PROJECT > ERASE (SLOOP 2.6): the slot (section A..D) empty again. In flash as the editor's restore leaves an
 * empty slot (project_restore): an object of no bytes, which proj_fetch reads as no project. The song's steps that
 * played that section are taken out (the default song if none is left), so song mode still plays. Stopped only: a
 * flash erase stops the audio ~50 ms */
static void project_erase(uint32_t slot)
{
    slot &= 3u;
    if (song.playing || transport_req) { ui_message("STOP BEFORE ERASE"); return; }
    if (!proj_ok(&proj_slot[slot])) {
        ui_message("EMPTY SLOT");
        return;
    }
#if FELUCCA_FLASH
    if (flash_ok && st_save(OBJ_PROJECT0 + slot, &proj_slot[slot], 0)) {
        ui_message("ERASE ERROR");
        return;
    }
#endif
    fm1_irq_off();
    memset(&proj_slot[slot], 0, sizeof proj_slot[slot]);
    sec_dirty &= (uint8_t)~(1u << slot);
#if FELUCCA_ARRANGER
    if (live_sec == (int8_t)slot)
        live_sec = -1;
    if (arr_drop_scene(&arrangement, slot))
        song_dirty = 1;                                     /* (saved with the settings, when quiet) */
#endif
    fm1_irq_on();
    ui_message("ERASED");
}

/* a project into the working one: the transport stops, everything sounding is released */
static void project_apply(const project_t *p)
{
    uint32_t k;
    transport_req = 2;
    panic_req = (1u << NTRK) - 1u;
    drum_mute = 0;                                      /* (the live mutes: a performance's, not the project's) */
    fm1_irq_off();                                      /* the audio ISR must not see half a project */
    proj_apply(p, 1);
    song.sel = (uint8_t)(p->sel < NTRK ? p->sel : 0u);
    fm1_irq_on();
    for (k = 0; k < NPART; k++)                         /* a format 1 project: the default sounds of tracks 2, 3 */
        if (p->t[k].preset == 0xFFu) {
            apply_preset_to(&trk[k], TRK_DEF[k][1]);
            steps_clear(&trk[k]);
        }
    sync_reload = 1;
    ui.force = 1;
}

static void project_load(uint32_t slot)
{
    project_t *p = &proj_slot[slot & 3u];
#if FELUCCA_ARRANGER
    if (song.playing || transport_req) { ui_message("STOP BEFORE LOAD"); return; }
#endif
#if FELUCCA_FLASH
    if (flash_ok && !proj_ok(p))
        proj_fetch(slot);
#endif
    if (!proj_ok(p)) {
        ui_message("EMPTY SLOT");
        return;
    }
    project_apply(p);
    ui_message("LOADED");
}

/* ---- the working project, kept in flash by itself: saved when it changed, the transport is stopped,
 * nothing sounds and the panel was not touched for AUTOSAVE_IDLE (a flash erase stops the audio for
 * ~50 ms: never while something plays); loaded at power-on (autosave_resume) */
#define AUTOSAVE_IDLE 2500u                    /* ms without input */
#define AUTOSAVE_GAP 20000u                    /* ms between two saves at least */
static project_t autosave_buf __attribute__((section(".pool")));
static uint32_t autosave_hash, autosave_ms, autosave_checked;

static int audio_quiet(void)
{
    uint32_t p, i;
    for (p = 0; p < NPART; p++)
        for (i = 0; i < NVOICE; i++)
            if (trk[p].v[i].active)
                return 0;
    for (i = 0; i < NDRUM; i++)
        if (drums.v[i].active)
            return 0;
    return 1;
}

static void autosave_tick(void)                /* main loop */
{
#if FELUCCA_FLASH
    uint32_t h, now = fm1_ms;
    if (!flash_ok || song.playing || transport_req || rec_wait || ft_on || ui.menu ||
        now - ui_input_ms < AUTOSAVE_IDLE || now - autosave_ms < AUTOSAVE_GAP || now - autosave_checked < 1000u)
        return;
    autosave_checked = now;
    proj_capture(&autosave_buf);
    h = autosave_buf.sum;
    if (h == autosave_hash || !audio_quiet())
        return;
    if (st_save(OBJ_AUTOSAVE, &autosave_buf, sizeof autosave_buf) == 0)
        autosave_hash = h;
    autosave_ms = fm1_ms;
#endif
}

static uint8_t drch_seed;                           /* the settings object has no MIDI part (older firmware): the drum
                                                     * channel was the project's then; autosave_resume takes it once */
static void autosave_resume(void)              /* power-on: the project as it was left (felucca_init) */
{
#if FELUCCA_FLASH
    project_t *q = &autosave_buf;
    int n;
    if (!flash_ok)
        return;
    n = st_load(OBJ_AUTOSAVE, &proj_tmp, sizeof proj_tmp);
    if (!proj_import(q, &proj_tmp, n)) {
        drch_seed = 0;
        return;
    }
    autosave_hash = q->sum;
    if (drch_seed && proj_tmp.v7.magic != PROJ_MAGIC && proj_tmp.v7.magic != PROJ_MAGIC_V7 && q->g[G_DRCH] >= 0 &&
        q->g[G_DRCH] <= 16) {
        midi_set[3] = (uint8_t)q->g[G_DRCH];          /* a legacy (FUN1..5) autosave: its GLO > DRUMS > CH, now a */
        song.g[G_DRCH] = q->g[G_DRCH];                /* setting of the FM-1, kept in the settings once */
        settings_later = 1;
    }
    drch_seed = 0;
    proj_apply(q, 1);
    song.sel = (uint8_t)(q->sel < NTRK ? q->sel : 0u);
    for (n = 0; n < NPART; n++)
        trk[n].engine = trk[n].eng_req;        /* (nothing sounds yet: no fade) */
#endif
}

/* settings + learned panel table: one flash object. The flash copy wins at
 * boot (the .noinit copies are garbage after a power-off). */
typedef struct {
    uint32_t magic, palette, lowcut, zoom;
    panel_t panel;
#if FELUCCA_ARRANGER
    arr_config_t arrangement;
#endif
    uint32_t lights;                               /* SLOOP 2.3: the backlight (panel.c lights_word); appended,
                                                    * so 2.2 still reads its part (st_load cuts at its size) */
} persist_t;
#define PERSIST_SIZE_V22 __builtin_offsetof(persist_t, lights)   /* the settings as 2.2 wrote them (no lights) */
_Static_assert(sizeof(persist_t) == PERSIST_SIZE_V22 + 4u, "lights: the last word, no padding before it");
#if FELUCCA_ARRANGER
_Static_assert(__builtin_offsetof(persist_t, arrangement) == 48u && sizeof(arr_config_t) == 36u,
               "Song editor contract: arrangement bytes 48..83");
#endif
#if FELUCCA_ARRANGER
#define PERSIST_MAGIC 0x50455233u                  /* "PER3": includes the song order */
#else
#define PERSIST_MAGIC 0x50455232u
#endif
#if FELUCCA_FLASH
static persist_t persist_saved;
static void dsu_boot(void);
#endif

static void persist_boot(void)                    /* before settings_init / panel_init */
{
#if FELUCCA_ARRANGER
    arr_defaults(&arrangement);
#endif
#if FELUCCA_FLASH
    persist_t p;
    uint32_t f = irq_save();
    flash_ok = FL_JEDEC_OK(FL_FAR(fl_jedec_ram)());      /* the expected 1 MiB part, else stay RAM-only */
    irq_restore(f);
    if (!flash_ok)
        return;
    fl_plain_window_init();                        /* flash above 0x93000 (SMK 0x9D000) reads as plaintext through XIP
                                                    * (user sample sets are played from there) */
    st_owner_check();                              /* SMK: never parse another ported firmware's store (storage.c) */
    {
        uint32_t k;
        for (k = 0; k < SMP_USER_SLOTS; k++)
            smp_user_scan(k);
    }
    {
        int n = st_load(OBJ_SETTINGS, &p, sizeof p);
        if (n == (int)PERSIST_SIZE_V22 && p.magic == PERSIST_MAGIC)
            p.lights = 0;                          /* from 2.2: backlight off */
        if (((n == (int)sizeof p || n == (int)PERSIST_SIZE_V22) && p.magic == PERSIST_MAGIC)
#if FELUCCA_ARRANGER
            || (n == (int)(16u + sizeof(panel_t)) && p.magic == 0x50455232u)
#endif
            ) {
            settings.magic = SETTINGS_MAGIC;
            settings.palette = p.palette;
            settings.lowcut = p.lowcut;
            settings.zoom = p.zoom;
            if (p.panel.magic == PANEL_MAGIC)
                panel = p.panel;
            if (p.magic == PERSIST_MAGIC)
                lights_from_word(p.lights);
            else
                p.lights = 0;
#if FELUCCA_ARRANGER
            if (p.magic == PERSIST_MAGIC && arr_valid(&p.arrangement, 15u))
                arrangement = p.arrangement;
            else
                p.arrangement = arrangement;
#endif
            persist_saved = p;
        } else if (n == (int)(8u + sizeof(panel_t)) && p.magic == 0x50455231u) {   /* "PER1": palette, panel */
            const uint32_t *w = (const uint32_t *)&p;
            panel_t old;
            memcpy(&old, w + 2, sizeof old);
            settings.magic = SETTINGS_MAGIC;
            settings.palette = w[1];
            settings.lowcut = 0;
            settings.zoom = 0;
            if (old.magic == PANEL_MAGIC)
                panel = old;
        }
    }
    {   /* projects: fill empty RAM slots from flash, so the slot list is right after power-on. A slot
         * still valid in RAM (a warm reset: an update, UPDATE MODE, a crash) may never have reached
         * flash (a live section stored while playing): marked to be written when quiet */
        uint32_t i;
        for (i = 0; i < 4u; i++) {
            if (!proj_ok(&proj_slot[i])) {             /* a format 5 slot of the firmware before: converted through */
                memcpy(&proj_tmp, &proj_slot[i], sizeof proj_slot[i]);   /* the staging, never read in place */
                if ((proj_tmp.v5.magic != PROJ_MAGIC_V5 && proj_tmp.v5.magic != PROJ_MAGIC_V7) ||
                    !proj_import(&proj_slot[i], &proj_tmp, sizeof proj_tmp.v5)) {
                    proj_fetch(i);
                    continue;
                }
            }
            {
                int n = st_load(OBJ_PROJECT0 + i, &proj_tmp, sizeof proj_tmp);
                if (n != (int)sizeof proj_slot[i] || memcmp(&proj_tmp.v7, &proj_slot[i], sizeof proj_slot[i]))
                    sec_dirty |= (uint8_t)(1u << i);
            }
        }
    }
    dsu_boot();
    up_boot(&proj_tmp, sizeof proj_tmp);           /* user presets (read in place; OPB2 / UPB1 converted once) */
    fm6_bank_boot();                               /* the FM6 patch bank (fm6_bank.c) */
#endif
}

static int project_used(uint32_t slot) { return proj_ok(&proj_slot[slot & 3u]); }

/* ---- the MIDI settings (2.6): the settings object's third part, after the SYN kits, and backup object 10. 40 bytes:
 * the MIDI LEARN map (seq.c ml_tab, ML_N entries, 2 bytes each, low first), GLO > MIDI's CH1 CH2 CH3 (0 OFF, 1..16),
 * the drum channel | OTHER << 7 (bits 5, 6: 0), GLO > MIDI 2's BEND (1..12), 3 zero bytes. ORBIT always writes all of
 * it (the defaults too: a backup of them resets a customised FM-1). Read: 32 (the map), 36 (and the channels) or 40
 * bytes; what is missing takes its default, an entry this firmware cannot learn is dropped, a channel or range out
 * of bounds its default */
#define MIDI_PART (2u * ML_N + 8u)
_Static_assert(MIDI_PART == 40u && sizeof(midi_set) == 6u, "the MIDI part's layout");
typedef struct {
    uint16_t ml[ML_N];
    uint8_t set[6];                                 /* panel.c midi_set: CH1 CH2 CH3, drums, OTHER, BEND */
} midi_cfg_t;
static void midi_cfg_now(midi_cfg_t *c)
{
    uint32_t k;
    for (k = 0; k < ML_N; k++)
        c->ml[k] = ml_tab[k];
    memcpy(c->set, midi_set, sizeof c->set);
}
static void midi_part_encode(uint8_t *b, const midi_cfg_t *c)
{
    uint32_t k;
    for (k = 0; k < ML_N; k++)
        b[2u * k] = (uint8_t)c->ml[k], b[2u * k + 1u] = (uint8_t)(c->ml[k] >> 8);
    b[2u * ML_N] = c->set[0], b[2u * ML_N + 1u] = c->set[1], b[2u * ML_N + 2u] = c->set[2];
    b[2u * ML_N + 3u] = (uint8_t)((c->set[3] & 31u) | (c->set[4] & 1u) << 7);
    b[2u * ML_N + 4u] = c->set[5];
    b[2u * ML_N + 5u] = b[2u * ML_N + 6u] = b[2u * ML_N + 7u] = 0;
}
static void midi_part_fill(uint8_t *b)              /* the part as it is now */
{
    midi_cfg_t c;
    midi_cfg_now(&c);
    midi_part_encode(b, &c);
}
/* n bytes (0, 32, 36 or 40) of a part -> c; missing fields: the defaults */
static void midi_part_decode(midi_cfg_t *c, const uint8_t *b, uint32_t n)
{
    static const uint8_t def[6] = MIDI_SET_DEF;
    uint32_t k, e;
    memcpy(c->set, def, sizeof c->set);
    for (k = 0; k < ML_N; k++) {
        e = n >= 2u * ML_N ? (uint32_t)b[2u * k] | (uint32_t)b[2u * k + 1u] << 8 : 0u;
        c->ml[k] = (uint16_t)((e >> 9) && ml_free_cc(e & 127u) && ml_id(e >> 9) < P_COUNT ? e : 0u);
    }
    if (n >= 2u * ML_N + 4u) {
        for (k = 0; k < 3u; k++)
            c->set[k] = (uint8_t)(b[2u * ML_N + k] <= 16u ? b[2u * ML_N + k] : def[k]);
        c->set[3] = (uint8_t)((b[2u * ML_N + 3u] & 31u) <= 16u ? b[2u * ML_N + 3u] & 31u : def[3]);
        c->set[4] = (uint8_t)(b[2u * ML_N + 3u] >> 7);
    }
    if (n >= MIDI_PART)
        c->set[5] = (uint8_t)(b[2u * ML_N + 4u] >= 1u && b[2u * ML_N + 4u] <= 12u ? b[2u * ML_N + 4u] : def[5]);
}
static void midi_cfg_install(const midi_cfg_t *c)
{
    uint32_t k;
    fm1_irq_off();                                  /* (the ISR reads the map and the channels: never half of them) */
    for (k = 0; k < ML_N; k++)
        ml_tab[k] = c->ml[k];
    memcpy(midi_set, c->set, sizeof midi_set);
    song.g[G_CH1] = midi_set[0], song.g[G_CH2] = midi_set[1], song.g[G_CH3] = midi_set[2];
    song.g[G_DRCH] = midi_set[3], song.g[G_CHOTH] = midi_set[4], song.g[G_BEND] = midi_set[5];
    fm1_irq_on();
}
static int midi_part_valid_len(uint32_t n) { return n == 2u * ML_N || n == 2u * ML_N + 4u || n == MIDI_PART; }

#if FELUCCA_FLASH
static uint8_t midi_saved[MIDI_PART];               /* the part as flash has it (settings_save: written when it differs) */
/* the settings object: persist_t, the SYN kits, the MIDI part mp (always all of it) */
static int settings_write_midi(const persist_t *p, const uint8_t *mp)
{
    if (!dsu_valid(&dsu))
        dsu_defaults();
    if (st_save3(OBJ_SETTINGS, p, sizeof *p, &dsu, sizeof dsu, mp, MIDI_PART))
        return -1;
    dsu_dirty = 0;
    memcpy(midi_saved, mp, sizeof midi_saved);
    return 0;
}
static int settings_write(const persist_t *p)
{
    uint8_t mp[MIDI_PART];
    midi_part_fill(mp);
    return settings_write_midi(p, mp);
}

/* the SYN kits and the MIDI part from the settings object (its persist_t and SYN kits checked first), else the
 * defaults. Lengths: base (older firmware: no MIDI part), base + 32, + 36 (SLOOP 2.6 test builds), + 40 */
static void dsu_boot(void)
{
    st_hdr_t h;
    midi_cfg_t c;
    uint32_t len = sizeof(persist_t) + sizeof(dsu_bank_t), k;
    dsu_defaults();
    midi_part_decode(&c, st_buf, 0);
    drch_seed = 0;
    if (st_current(OBJ_SETTINGS, &h) >= 0 &&
        (h.len == len || (h.len > len && midi_part_valid_len(h.len - len))) &&
        ((const persist_t *)st_buf)->magic == PERSIST_MAGIC &&
        dsu_valid((const dsu_bank_t *)(st_buf + sizeof(persist_t)))) {
        memcpy(&dsu, st_buf + sizeof(persist_t), sizeof dsu);
        for (k = 0; k < DSU_N; k++)
            dsu_fix_kit(&dsu.k[k]);
        if (h.len > len)
            midi_part_decode(&c, st_buf + len, h.len - len);
        drch_seed = h.len == len;                   /* (autosave_resume: the drum channel of an old project) */
    }
    midi_cfg_install(&c);
    midi_part_fill(midi_saved);
    dsu_dirty = 0;
}
#endif

static void persist_fill(persist_t *p)              /* the settings as they are now */
{
    memset(p, 0, sizeof *p);
    p->magic = PERSIST_MAGIC;
    p->palette = settings.palette;
    p->lowcut = settings.lowcut;
    p->zoom = settings.zoom;
    p->panel = panel;
    p->lights = lights_word();
#if FELUCCA_ARRANGER
    p->arrangement = arrangement;
#endif
}

static void settings_save(void)
{
#if FELUCCA_FLASH
    persist_t p;
    if (!flash_ok)
        return;
    persist_fill(&p);
    {
        uint8_t mp[MIDI_PART];
        midi_part_fill(mp);
        if (!memcmp(&p, &persist_saved, sizeof p) && !memcmp(mp, midi_saved, sizeof mp))
            return;                                /* unchanged: no erase cycle */
    }
    if (settings_write(&p) == 0)
        persist_saved = p;
#endif
}

#if FELUCCA_FLASH
_Static_assert(sizeof(project_t) <= ST_PAYLOAD_MAX, "project does not fit one flash sector");

/* ---- backup restore (editor.c BK_PUT): each object checked as a load checks it, then written through the
 * same A/B commit as a save. rc: 0 ok, 2 not a valid object, 3 stop the song first, 4 flash */
static int panel_valid(const panel_t *q)           /* a permutation of the buttons and of the knobs */
{
    uint32_t i, b = 0, e = 0;
    if (q->magic != PANEL_MAGIC)
        return 0;
    for (i = 0; i < NB; i++) {
        if (q->btn[i] >= 14u || (b >> q->btn[i]) & 1u)
            return 0;
        b |= 1u << q->btn[i];
    }
    for (i = 0; i < NE; i++) {
        if (q->enc[i] >= 7u || (e >> q->enc[i]) & 1u || (q->dir[i] != 1 && q->dir[i] != -1))
            return 0;
        e |= 1u << q->enc[i];
    }
    return 1;
}

static uint32_t settings_restore(const void *raw, uint32_t n)
{
    persist_t p;
    if (n != sizeof p && n != PERSIST_SIZE_V22)
        return 2;
    memset(&p, 0, sizeof p);
    memcpy(&p, raw, n);
    if (p.magic != PERSIST_MAGIC || p.palette >= NPALETTES || p.lowcut > 1u || p.zoom > 1u || !panel_valid(&p.panel))
        return 2;
#if FELUCCA_ARRANGER
    if (!arr_valid(&p.arrangement, 15u))
        return 2;
    if ((song.playing || transport_req) && memcmp(&p.arrangement, &arrangement, sizeof arrangement))
        return 3;
#endif
    if (!flash_ok || settings_write(&p))
        return 4;
    persist_saved = p;
    settings.palette = p.palette;
    settings.lowcut = p.lowcut;
    settings.zoom = p.zoom;
    panel = p.panel;
#if FELUCCA_ARRANGER
    arrangement = p.arrangement;
#endif
    lights_from_word(p.lights);
    song.g[G_SYNC] = (int16_t)lights_sync;
    song.g[G_MIDI] = (int16_t)lights_mout;
    song.g[G_ROUTE] = (int16_t)lights_min;
    palette_set(settings.palette);
    fx_lowcut = (uint8_t)(settings.lowcut != 0);
    ui.force = 1;
    return 0;
}

/* backup object 10: the MIDI settings (32, 36 or 40 bytes; what is missing: its default). The whole settings object is
 * written with the candidate first; only then does the FM-1 use it (a failed write changes nothing) */
static uint32_t midi_restore(const void *raw, uint32_t n)
{
    midi_cfg_t c;
    persist_t p;
    uint8_t mp[MIDI_PART];
    if (!midi_part_valid_len(n))
        return 2;
    midi_part_decode(&c, (const uint8_t *)raw, n);
    midi_part_encode(mp, &c);
    persist_fill(&p);
    if (!flash_ok || settings_write_midi(&p, mp))
        return 4;
    persist_saved = p;
    midi_cfg_install(&c);
    lights_min = (uint8_t)song.g[G_ROUTE];
    ui.force = 1;
    return 0;
}

/* slot 0..3 (n 0: empty), or 4: the working project (loaded now) */
static uint32_t project_restore(uint32_t slot, const void *raw, uint32_t n)
{
    if (song.playing || transport_req)
        return 3;
    if (slot < 4u && !n) {
        if (!flash_ok || st_save(OBJ_PROJECT0 + slot, raw, 0))
            return 4;
        memset(&proj_slot[slot], 0, sizeof proj_slot[slot]);
        sec_dirty &= (uint8_t)~(1u << slot);
        return 0;
    }
    if (!proj_import(&autosave_buf, raw, (int)n))
        return 2;
    if (slot == 4u) {
        project_apply(&autosave_buf);
        return 0;
    }
    if (!flash_ok || st_save(OBJ_PROJECT0 + slot, &autosave_buf, sizeof autosave_buf))
        return 4;
    memcpy(&proj_slot[slot], &autosave_buf, sizeof proj_slot[slot]);
    sec_dirty &= (uint8_t)~(1u << slot);
    return 0;
}
#endif
#if FELUCCA_ARRANGER
static void arrangement_save(void)
{
    if (song.playing || transport_req) { ui_message("STOP BEFORE SAVE"); return; }
    settings_save();
#if FELUCCA_FLASH
    if (flash_ok) {
        ui_message(memcmp(&persist_saved.arrangement, &arrangement, sizeof arrangement) ? "SAVE ERROR" : "SONG SAVED");
        return;
    }
#endif
    ui_message("SONG IN RAM ONLY");
}

/* ---- live sections (SAVE + key, ui_layers.c). A section is a project slot (A..D = 1..4): stored into RAM
 * at once (playing too), written to flash once the transport is stopped and nothing sounds (an erase
 * stops the audio for ~50 ms); a song recorded with SONG REC is saved the same way. */
static void section_store(uint32_t s)
{
    s &= 3u;
    fm1_irq_off();                                      /* (the audio ISR may be applying a section) */
    proj_capture(&proj_slot[s]);
    live_sec = (int8_t)s;
    fm1_irq_on();
    sec_dirty |= (uint8_t)(1u << s);
}
static void section_load(uint32_t s)                    /* stopped: the section is the loop now */
{
    s &= 3u;
    project_apply(&proj_slot[s]);
    live_sec = (int8_t)s;
}
static void sections_write(void)                        /* the dirty sections and song into flash */
{
    uint32_t i;
#if FELUCCA_FLASH
    if (flash_ok)
        for (i = 0; i < 4u; i++)
            if (((sec_dirty >> i) & 1u) && st_save(OBJ_PROJECT0 + i, &proj_slot[i], sizeof proj_slot[i]) == 0)
                sec_dirty &= (uint8_t)~(1u << i);       /* (a failed write stays dirty: tried again later) */
    if (!flash_ok)
#endif
        sec_dirty = 0;
    (void)i;
    if (song_dirty) {
        song_dirty = 0;
        settings_save();
    }
}
/* before an intentional reset (an update, UPDATE MODE, UBOOT from the host): the audio is stopped, so
 * whatever is only in RAM goes to flash now: the live sections, the song, the working project */
static void persist_flush_now(void)
{
    sections_write();
#if FELUCCA_FLASH
    if (flash_ok && !arrangement_clock.running) {     /* (a song playing: the tracks hold a section) */
        proj_capture(&autosave_buf);
        if (autosave_buf.sum != autosave_hash && st_save(OBJ_AUTOSAVE, &autosave_buf, sizeof autosave_buf) == 0)
            autosave_hash = autosave_buf.sum;
    }
#endif
}
static void sections_flush(void)                        /* main loop */
{
    static uint32_t tried;
    if (srec_done) {
        song_dirty = srec_done != 0xFFu;
        if (song_dirty) {
            char b[8];
            fmt_int(b, srec_done);
            ui_say("SONG PARTS ", b);
        } else {
            ui_message("NO SONG");
        }
        srec_done = 0;
    }
    if ((uint32_t)song.g[G_SYNC] != lights_sync) {      /* GLO > SYSTEM > SYNC: kept with the settings */
        lights_sync = (uint8_t)song.g[G_SYNC];
        settings_later = 1;
    }
    if ((uint32_t)(song.g[G_MIDI] != 0) != lights_mout) {   /* GLO > SYSTEM > MIDI: the same */
        lights_mout = (uint8_t)(song.g[G_MIDI] != 0);
        settings_later = 1;
    }
    if ((uint32_t)song.g[G_ROUTE] != lights_min) {        /* GLO > SYSTEM > IN: the same (2.6: CH n too) */
        lights_min = (uint8_t)song.g[G_ROUTE];
        settings_later = 1;
    }
    {   /* 2.6: GLO > MIDI (CH1 CH2 CH3 OTHER BEND) and GLO > DRUMS > CH: the same */
        static const uint8_t gid[6] = {G_CH1, G_CH2, G_CH3, G_DRCH, G_CHOTH, G_BEND};
        uint32_t k;
        for (k = 0; k < 6u; k++)
            if ((uint32_t)song.g[gid[k]] != midi_set[k]) {
                fm1_irq_off();
                midi_set[k] = (uint8_t)song.g[gid[k]];
                fm1_irq_on();
                settings_later = 1;
            }
    }
    if (settings_later) {                               /* the menu closed while playing */
        settings_later = 0;
        song_dirty = 1;                                 /* (settings_save when quiet, with the song) */
    }
    if ((!sec_dirty && !song_dirty) || song.playing || transport_req || !audio_quiet() || fm1_ms - ui_input_ms < 1500u ||
        fm1_ms - tried < 5000u)
        return;
    tried = fm1_ms;                                     /* (a failed write: again in 5 s, not every frame) */
    sections_write();
    if (sec_dirty)
        ui_message("SAVE ERROR: RETRYING");
}
#endif
#endif /* PROJ_HOST */

