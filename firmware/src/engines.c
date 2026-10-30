/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Engine table: order = PRESETS browsing order (and the engine numbers of the editor protocol). */
#include "dsp.c"
/* GRAIN, FM6 and PHYS share large per-part state after the engine-switch fade ends. */
static void *eng_arena_of(const track_t *t, uint32_t eng);
static inline int32_t voice_amp(int32_t s, const vmod_t *m, uint32_t i) { return mulq15(mulq15(s, amp_at(m, i)), VOICE_FS); }
static inline int32_t soft_knee(int32_t y, int32_t k)
{
    int32_t a = y < 0 ? -y : y;
    if (a <= k)
        return y;
    a = k + (softclip((a - k) * 2) >> 1);
    return y < 0 ? -a : a;
}
/* UNISON start-phase spread (voice.c unison_spread; ported from SLOOP 2.8.4, isod89/sloop-fm1 91f3018): r (Q32, a
 * turn of the note at pitch p16, 1/16 semitone) expressed in turns of an oscillator d16 (1/16 semitone) above it,
 * i.e. the same time shift at that oscillator's frequency. Called once per extra voice at note-on, never per sample */
static uint32_t uni_turns(uint32_t r, int32_t p16, int32_t d16)
{
    uint32_t b = pitch_inc((uint32_t)clamp(p16, 0, 2047));
    return b ? (uint32_t)(((uint64_t)r * pitch_inc((uint32_t)clamp(p16 + d16, 0, 2047))) / b) : r;
}
#include "eng_analog.c"
#include "eng_digital.c"
#include "eng_phase.c"
#include "eng_lofi.c"
#include "eng_sample.c"
#include "eng_formant.c"
#include "eng_trio.c"
#include "eng_drawbar.c"
#include "eng_grain.c"
#include "eng_cloud.c"
#include "eng_fm6.c"            /* FM6: 6-operator FM, msfa ported (fm6_core.c, Apache-2.0); SLOOP 2.4 */
#include "eng_phys.c"           /* PHYS: DaisySP/Rings physical models (MIT); SLOOP 2.5 */
#include "eng_noise.c"          /* NOISE: based on Felucca 1.0; SLOOP 2.5 */
#include "eng_op1_cluster.c"

typedef union {
    gr_part_t grain;
    drw_part_t drawbar;                  /* eng_drawbar.c: the part's wheels, percussion and rotary (SLOOP 2.7) */
    fm6_note_t fm6[FM6_POLY];
    uint32_t phys[PHYS_ARENA / 4u];
    cluster_part_t cluster;
    cloud_part_t cloud;
} eng_arena_t;
_Static_assert(sizeof(cluster_part_t) <= PHYS_ARENA, "cluster fits the existing engine arena");
_Static_assert(sizeof(cloud_part_t) <= sizeof(gr_part_t), "cloud fits the old grain arena");
_Static_assert(sizeof(eng_arena_t) == PHYS_ARENA, "cloud does not grow the existing arena");
static eng_arena_t eng_arena[NPART] __attribute__((section(".pool")));
static uint8_t eng_arena_own[NPART];
#ifdef ARENA_STATS
static uint32_t eng_arena_claims, eng_arena_bad;
#endif
static void *eng_arena_of(const track_t *t, uint32_t eng)
{
    uint32_t p = (uint32_t)(t - trk);
    if (p >= NPART)
        return 0;
#ifdef ARENA_STATS
    if (t->engine != eng)
        eng_arena_bad++;
#endif
    if (eng_arena_own[p] != eng + 1u) {
        memset(&eng_arena[p], 0, sizeof eng_arena[p]);
        eng_arena_own[p] = (uint8_t)(eng + 1u);
#ifdef ARENA_STATS
        eng_arena_claims++;
#endif
    }
    return &eng_arena[p];
}
#if FELUCCA_SLICE
#include "eng_slice.c"
#endif
#include "eng_orbit.c"

static const engine_t *const ENGINES[NENGINES] = {&ENG_ANALOG, &ENG_DIGITAL, &ENG_PHASE, &ENG_LOFI, &ENG_SAMPLE,
                                                    &ENG_FORMANT, &ENG_TRIO, &ENG_DRAWBAR, &ENG_GRAIN,
#if FELUCCA_SLICE
                                                    &ENG_SLICE,
#endif
    &ENG_SWARM, &ENG_ORBIT_PULSE, &ENG_FM4, &ENG_FM6, &ENG_PHYS, &ENG_NOISE, &ENG_CLUSTER, &ENG_CLOUD,
};
_Static_assert(ENGI_FM6 == ORBIT_FM4 + 1u, "ENGINES[ENGI_FM6] is FM6");
_Static_assert(ENGI_GRAIN == 8u && ENGI_PHYS == ENGI_FM6 + 1u && ENGI_NOISE == ENGI_PHYS + 1u,
               "append-only engine order");
_Static_assert(ENGI_CLUSTER == ENGI_NOISE + 1u && ENGI_CLOUD == ENGI_CLUSTER + 1u &&
               NENGINES == ENGI_CLOUD + 1u,
               "cluster is append-only");
#if FELUCCA_SLICE
_Static_assert(ORBIT_SWARM == 10u && ENGI_FM6 == 13u && ENGI_PHYS == 14u && ENGI_NOISE == 15u,
               "optional SLICE engine order");
#else
_Static_assert(ORBIT_SWARM == 9u && ENGI_FM6 == 12u && ENGI_PHYS == 13u && ENGI_NOISE == 14u,
               "default engine order");
#endif

/* every factory sound as loud as the others: a level trim per preset, 1/2 dB, measured on a phrase
 * that fits the sound (tools/level_presets.py writes preset_trim.h); a track keeps it in P_ED_FX */
#include "preset_trim.h"
static int16_t preset_trim(uint32_t e, uint32_t pi)
{
    uint32_t row;
    if (e < 9u)
        row = e;
    else if (e == ENGI_FM6)
        row = 9u;
    else if (e == ENGI_PHYS)
        row = 10u;
    else if (e == ENGI_NOISE)
        row = 11u;
#if FELUCCA_SLICE
    else if (e == 9u)
        row = 12u;
#endif
    else
        return 0;       /* preserve the existing ORBIT SWARM/PULSE/FM4 levels */
    return row < PT_ENGINES && pi < PT_MAX ? PRESET_TRIM[row][pi] : 0;
}

