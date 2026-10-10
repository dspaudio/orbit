/* SPDX-License-Identifier: GPL-3.0-only
 * ORBIT adapter for the official OP-1 #246 oscillator translation.
 * Pitch/ADSR/velocity/FX are ORBIT's, not a complete original-patch audio port.
 */
#include "op1_cluster_core.c"
#include "felucca_op1_cluster.h"

typedef struct {
    cls_voice state;
    int16_t pcm[128];
    uint16_t cursor;
} cluster_note_t;
typedef struct {
    cluster_note_t note[NVOICE];
    cls_global global;
    uint8_t initialized;
} cluster_part_t;

static void cluster_preset_to(track_t *t, uint32_t pi)
{
    uint32_t k;
    for (k = 0; k < 8u; k++)
        t->p[P_E0 + k] = OP1_CLUSTER_KNOBS[pi][k];
}

static void cluster_note_on(track_t *t, voice_t *v)
{
    cluster_part_t *p = eng_arena_of(t, ENGI_CLUSTER);
    cluster_note_t *s = &p->note[v - t->v];
    if (!p->initialized) {
        /* Start a deterministic per-part PRNG when taking the arena from another engine. */
        p->global.seed = 1;
        cls_global_init(&p->global);
        p->initialized = 1;
    }
    cls_construct(&s->state);
    cls_note_init(&s->state, &p->global, &OP1_CLUSTER_TABLES, &t->p[P_E0]);
    s->cursor = 128;
}

static void cluster_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    cluster_part_t *p = eng_arena_of(t, ENGI_CLUSTER);
    cluster_note_t *s = &p->note[v - t->v];
    uint32_t i;
    if (!v->gate && !s->state.bytes[0x6a])
        cls_release(&s->state);
    for (i = 0; i < n; i++) {
        if (s->cursor == 128u) {
            uint32_t phase[129], k;
            /* Preserve the original 128-sample boundary; pitch/knob changes apply at the next one. */
            for (k = 0; k < 129u; k++)
                phase[k] = m->inc;
            cls_render(&s->state, &p->global, &OP1_CLUSTER_TABLES, s->pcm, phase, &t->p[P_E0]);
            s->cursor = 0;
        }
        out[i] += voice_amp(s->pcm[s->cursor++], m, i) * 2;
    }
}

/* The int8 factory format holds only the row index; values load from the signed16 table.
 * The common envelope is an explicit ORBIT default, not the original ADSR timing formula. */
#define CLUSTER_PRESET(i) {OP1_CLUSTER_NAMES[i], {(i)}, {0, 70, 127, 40}, 0, 0, FX(0, 0, 0, 0)}
static const preset_t CLUSTER_PRESETS[] = {
    CLUSTER_PRESET(0), CLUSTER_PRESET(1), CLUSTER_PRESET(2), CLUSTER_PRESET(3),
    CLUSTER_PRESET(4), CLUSTER_PRESET(5), CLUSTER_PRESET(6), CLUSTER_PRESET(7),
    CLUSTER_PRESET(8), CLUSTER_PRESET(9), CLUSTER_PRESET(10), CLUSTER_PRESET(11),
    CLUSTER_PRESET(12), CLUSTER_PRESET(13), CLUSTER_PRESET(14), CLUSTER_PRESET(15),
};
#undef CLUSTER_PRESET
static const engine_t ENG_CLUSTER = {
    .name = "CLUSTER",
    .page_title = {"OSC RAW", "RESERVED"},
    .edit = {
        {"COUNT", F_INT, 2048, 18431, 4000, 0, 0},
        {"WAVE", F_INT, -32768, 32767, 4000, 0, 0},
        {"CURVE", F_INT, 0, 24575, 8000, 0, 0},
        {"SPREAD", F_INT, -32768, 32767, 100, 0, 0},
        {"--", F_INT, 0, 0, 0, 0, 0}, {"--", F_INT, 0, 0, 0, 0, 0},
        {"--", F_INT, 0, 0, 0, 0, 0}, {"--", F_INT, 0, 0, 0, 0, 0},
    },
    .presets = CLUSTER_PRESETS,
    .npresets = NELEM(CLUSTER_PRESETS),
    .fil_page = -1,
    .note_on = cluster_note_on,
    .render = cluster_render,
    .color = 0x07ff,
    .macro = {P_E0, P_E1, P_E2, P_E3},
    .poly = 4,
};
