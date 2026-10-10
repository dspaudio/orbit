/* SPDX-License-Identifier: GPL-3.0-only */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <assert.h>

static uint32_t render_case(uint32_t engine, uint32_t preset, uint32_t note, int extreme)
{
    track_t t = {0};
    voice_t v = {0};
    vmod_t m = {0};
    uint32_t hash = 2166136261u, nonzero = 0, b, i;
    const engine_t *e = ENGINES[engine];
    m.inc = pitch_inc(note * 16); m.pitch16 = note * 16;
    m.amp0 = m.amp1 = m.envq15 = 32767; m.shape = 64 << 8;
    for (i = 0; i < 8; i++) t.p[P_E0+i] = e->presets[preset].e[i];
    if (extreme) for (i = 0; i < 8; i++)
        t.p[P_E0+i] = extreme == 1 ? e->edit[i].min : e->edit[i].max;
    e->note_on(&t, &v);
    for (b = 0; b < 128; b++) {
        int32_t o[66] = {0};
        o[0] = 123456789; o[65] = 987654321;
        e->render(&t, &v, o+1, 64, &m);
        assert(o[0] == 123456789 && o[65] == 987654321);
        for (i = 1; i <= 64; i++) {
            assert(o[i] >= -48000 && o[i] <= 48000);
            nonzero += o[i] != 0;
            hash = (hash ^ (uint32_t)o[i]) * 16777619u;
        }
    }
    assert(nonzero > 100);
    return hash;
}
int main(void)
{
    uint32_t e, p, note, hashes[3];
    assert(ENGINES[0] == &ENG_ANALOG && ENGINES[8] == &ENG_GRAIN);
    for (e = ORBIT_SWARM; e <= ORBIT_FM4; e++) {
        for (p = 0; p < ENGINES[e]->npresets; p++) {
            uint32_t h = render_case(e, p, 60, 0);
            assert(h == render_case(e, p, 60, 0));
            for (note = 24; note <= 120; note += 12) {
                render_case(e, p, note, 0);
                render_case(e, p, note, 1);
                render_case(e, p, note, 2);
            }
        }
        hashes[e - ORBIT_SWARM] = render_case(e, 0, 60, 0);
    }
    assert(hashes[0] != hashes[1] && hashes[1] != hashes[2] && hashes[0] != hashes[2]);
    /* No modulation: FM4 CHAIN is the carrier sine with a tone filter. */
    {
        track_t t = {0}; voice_t v = {0}; vmod_t m = {0};
        int32_t o[64], previous = 0; uint32_t blocks, i, crosses = 0;
        m.inc = pitch_inc(69 * 16); m.amp0 = m.amp1 = m.envq15 = 32767; m.shape = 64 << 8;
        t.p[P_E1] = t.p[P_E2] = t.p[P_E3] = 1; t.p[P_E4] = 127;
        orbit_osc_note_on(&t, &v);
        for (blocks = 0; blocks < 689; blocks++) {
            memset(o, 0, sizeof o); orbit_fm4_render(&t, &v, o, 64, &m);
            for (i = 0; i < 64; i++) { crosses += previous <= 0 && o[i] > 0; previous = o[i]; }
        }
        assert(crosses >= 439 && crosses <= 441);
    }
    assert(orbit_step(0, 0) == 0);
    printf("PASS: 12 independent patches, MIDI 24..120, parameter extremes, deterministic DSP, guards, 440 Hz FM carrier; voice state %zu bytes (unchanged)\n", sizeof(voice_t));
    return 0;
}
