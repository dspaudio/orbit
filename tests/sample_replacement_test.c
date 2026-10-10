/* SPDX-License-Identifier: GPL-3.0-only */
/* Verify synthesized replacements, existing IDs/retained ADPCM and actual SAMPLE/GRAIN output. */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <assert.h>

static uint32_t render_source(uint32_t engine, uint32_t src, uint32_t note)
{
    track_t *t = &trk[0];
    voice_t *v = &t->v[0];
    const engine_t *e = ENGINES[engine];
    vmod_t m = {0};
    uint32_t b, i, nonzero = 0;

    /* Given: actual track voices and an engine preset with nonzero amplitude. */
    memset(t, 0, sizeof *t);
    host_preset(t, engine, 0);
    t->p[P_E0] = (int16_t)src;
    v->note = (uint8_t)note;
    v->vel = 127;
    v->active = v->gate = 1;
    m.pitch16 = (int32_t)note * 16;
    m.inc = pitch_inc(m.pitch16);
    m.amp0 = m.amp1 = m.envq15 = 32767;

    /* When: call SAMPLE note_on/render and GRAIN's actual block hooks. */
    e->note_on(t, v);
    for (b = 0; b < 128u; b++) {
        int32_t out[CTL + 2] = {0};
        out[0] = 123456789;
        out[CTL + 1] = 987654321;
        if (e->block)
            e->block(t);
        e->render(t, v, out + 1, CTL, &m);
        /* Then: inspect output boundaries and every audio sample. */
        assert(out[0] == 123456789 && out[CTL + 1] == 987654321);
        for (i = 1; i <= CTL; i++)
            nonzero += out[i] != 0;
    }
    return nonzero;
}

int main(void)
{
    static const char *const names[] = {
        "PIANO", "BASS", "VIBES", "HORNS", "STRGS", "FLUTE", "SCRCH", "PERC",
        "USR1", "USR2", "USR3", "USR4"
    };
    static const uint16_t z0[] = {0, 4, 8, 11, 15, 18, 21, 24};
    static const uint16_t nz[] = {4, 4, 3, 4, 3, 3, 3, 27};
    static const uint32_t bytes[] = {44104, 39138, 33078, 26462, 19844, 6172, 3072, 86144};
    /* Pre-change FNV-1a hashes of retained banks' ADPCM, including padding. */
    static const uint32_t hashes[] = {
        0x37bf6d96u, 0xc28631c7u, 0x0252c520u, 0x36b59adeu,
        0x069b3793u, 0, 0, 0xdd089458u
    };
    static const uint8_t preset_sets[] = {0, 1, 2, 3, 4, 5, 6, 7, 0, 0, 1};
    uint32_t i, j, offset = 0;

    /* Given/When: read the engine registry with the actual generated header. */
    assert(SMP_NSETS == 8 && SMP_NALL == 12);
    assert(ENGINES[4] == &ENG_SAMPLE && ENGINES[ENGI_GRAIN] == &ENG_GRAIN);
    assert(sizeof SMP_ZONES / sizeof SMP_ZONES[0] == 51u);
    assert(sizeof SMP_DATA == 303054u - 54284u + 9244u);
    assert(ENG_SAMPLE.npresets == sizeof preset_sets);
    assert(ENG_SAMPLE.edit[0].max == 11 && ENG_GRAIN.edit[0].max == 11);

    /* Then: existing set, zone, preset and USR1..4 IDs are unchanged. */
    for (i = 0; i < SMP_NALL; i++)
        assert(!strcmp(SMP_ALL_NAMES[i], names[i]));
    for (i = 0; i < SMP_NSETS; i++) {
        const smp_set_t *s = &SMP_SETS[i];
        assert(!strcmp(s->name, names[i]) && !strcmp(SMP_SET_NAMES[i], names[i]));
        assert(s->z0 == z0[i] && s->nz == nz[i]);
        if (bytes[i]) {
            uint32_t hash = 2166136261u;
            assert(SMP_ZONES[s->z0].off == offset);
            assert(offset + bytes[i] <= sizeof SMP_DATA);
            for (j = 0; j < bytes[i]; j++)
                hash = (hash ^ SMP_DATA[offset + j]) * 16777619u;
            if (i != 5u && i != 6u)
                assert(hash == hashes[i]);
            offset += bytes[i];
        }
    }
    assert(offset == sizeof SMP_DATA);
    for (i = 0; i < sizeof preset_sets; i++)
        assert(SMP_PRESET_TABLE[i].e[0] == preset_sets[i]);
    for (i = 0; i < SMP_USER_SLOTS; i++)
        assert(SMP_USER_OFF(i) == (i < 3u ? 0xA0000u + i * 0x14000u : 0xE7000u));

    /* Positive controls: retained PIANO and PERC are audible through the same render path. */
    assert(render_source(4, 0, 60) > 0);
    assert(render_source(ENGI_GRAIN, 0, 60) > 0);
    assert(render_source(4, 7, 36) > 0);
    assert(render_source(ENGI_GRAIN, 7, 36) > 0);
    /* Given/Then: original roots/key zones, FLUTE loops and SCRCH one-shots are preserved. */
    static const int16_t roots[] = {1152, 1344, 1536, 912, 1040, 1152};
    static const uint8_t lo[] = {0, 79, 91, 0, 62, 69};
    static const uint8_t hi[] = {78, 90, 127, 61, 68, 127};
    for (i = 18; i < 24u; i++) {
        const smp_zone_t *z = &SMP_ZONES[i];
        assert(z->root16 == roots[i - 18] && z->lo == lo[i - 18] && z->hi == hi[i - 18]);
        assert(z->n >= 2048u && z->off + (z->n + 1u) / 2u <= sizeof SMP_DATA);
        assert(z->looped == (i < 21u));
        if (z->looped)
            assert(z->ls < z->le && z->le < z->n && z->n >= 4096u);
    }
    /* When/Then: every MIDI note in both replacement banks produces actual decoder output. */
    for (i = 5; i <= 6; i++)
        for (j = 0; j < 128u; j++) {
            assert(render_source(4, i, j) > 0);
            assert(render_source(ENGI_GRAIN, i, j) > 0);
        }
    puts("PASS: 9244 B synthesized replacements, retained ADPCM/IDs/roots/key zones; "
         "FLUTE/SCRCH audible through SAMPLE/GRAIN on all 128 MIDI notes");
    return 0;
}
