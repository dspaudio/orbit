/* SPDX-License-Identifier: GPL-3.0-only */
#define ARENA_STATS 1
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <assert.h>

static void adapter_case(uint32_t pi, int release)
{
    track_t *t = &trk[0];
    cls_voice reference = {{0}};
    cls_global global = {0};
    uint32_t phase[129], b, i, audible = 0;
    int16_t pcm[128];
    vmod_t m = {0};
    cluster_part_t *part;
    /* Given: independent kernel state with the real part/arena's seed and raw patch. */
    memset(t, 0, sizeof *t);
    eng_arena_own[0] = 0;
    host_preset(t, ENGI_CLUSTER, pi);
    assert(!memcmp(&t->p[P_E0], OP1_CLUSTER_KNOBS[pi], sizeof OP1_CLUSTER_KNOBS[pi]));
    t->v[0].gate = 1;
    m.inc = 0x00800000u;
    m.amp0 = m.amp1 = 32768;
    global.seed = 1;
    cls_global_init(&global);
    cls_construct(&reference);
    cls_note_init(&reference, &global, &OP1_CLUSTER_TABLES, OP1_CLUSTER_KNOBS[pi]);
    for (i = 0; i < 129u; i++)
        phase[i] = m.inc;
    cluster_note_on(t, &t->v[0]);
    part = eng_arena_of(t, ENGI_CLUSTER);
    for (b = 0; b < 3u; b++) {
        uint32_t q;
        if (release && b == 1u) {
            t->v[0].gate = 0;
            cls_release(&reference);
        }
        cls_render(&reference, &global, &OP1_CLUSTER_TABLES, pcm, phase, OP1_CLUSTER_KNOBS[pi]);
        /* When: consume 128 samples through four actual 32-sample engine calls. */
        for (q = 0; q < 4u; q++) {
            int32_t out[CTL + 2] = {0};
            out[0] = 123456789;
            out[CTL + 1] = 987654321;
            cluster_render(t, &t->v[0], out + 1, CTL, &m);
            /* Then: cache boundaries, amplitude application and kernel update counts are preserved. */
            assert(out[0] == 123456789 && out[CTL + 1] == 987654321);
            for (i = 0; i < CTL; i++) {
                int32_t expected = ((int32_t)pcm[q * CTL + i] * VOICE_FS >> 15) * 2;
                assert(out[i + 1] == expected);
                audible += out[i + 1] != 0;
            }
            assert(!memcmp(&part->note[0].state, &reference, sizeof reference));
            assert(part->global.seed == global.seed);
            assert(!memcmp(part->global.control, global.control, sizeof global.control));
        }
    }
    assert(audible);
    assert(!eng_arena_bad);
}

int main(void)
{
    uint32_t pi, b, p, count = 0;
    int32_t out[CTL];
    _Static_assert(ENGI_CLUSTER == 15u + FELUCCA_SLICE, "append-only cluster ID");
    assert(ENGINES[ENGI_CLUSTER] == &ENG_CLUSTER);
    assert(ENGINES[ENGI_NOISE] == &ENG_NOISE && ENGINES[ENGI_PHYS] == &ENG_PHYS);
    for (pi = 0; pi < 16u; pi++) {
        adapter_case(pi, 0);
        adapter_case(pi, 1);
    }
    /* Given: multiple voices started on three actual parts. */
    host_tracks_init();
    for (p = 0; p < NPART; p++) {
        host_preset(&trk[p], ENGI_CLUSTER, p);
        trk_note_on(&trk[p], 48u + p * 5u, 100);
        trk_note_on(&trk[p], 55u + p * 5u, 100);
    }
    for (p = 0; p < NPART; p++)
        for (pi = 0; pi < NVOICE; pi++)
            count += trk[p].v[pi].active;
    assert(count == 6u && !eng_arena_bad);
    for (p = 0; p < NPART; p++) {
        uint32_t audible = 0, i;
        for (b = 0; b < 16u; b++) {
            track_render(&trk[p], out, CTL);
            for (i = 0; i < CTL; i++)
                audible += out[i] != 0;
        }
        assert(audible);
    }
    /* When: release the actual ORBIT envelopes and advance a fixed number of audio blocks. */
    for (p = 0; p < NPART; p++)
        trk_all_off(&trk[p]);
    for (b = 0; b < 12000u; b++)
        for (p = 0; p < NPART; p++)
            track_render(&trk[p], out, CTL);
    for (p = 0; p < NPART; p++)
        for (pi = 0; pi < NVOICE; pi++)
            assert(!trk[p].v[pi].active);
    assert(!eng_arena_bad);
    puts("PASS: 16 raw patches, 128/32 cadence, PCM/state/PRNG, release, guards, multi-part voices");
    return 0;
}
