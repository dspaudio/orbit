/* SPDX-License-Identifier: GPL-3.0-only */
/* SYN1..SYN4 runtime, validation, editor commands and bank serialization regressions. */
#define main hostsim_main
#include "hostsim.c"
#undef main
static struct { uint8_t force; } ui;

static uint8_t ed_out[600];
static uint32_t ed_n;
static void ed_b(uint32_t v) { if (ed_n < sizeof ed_out) ed_out[ed_n++] = (uint8_t)(v & 0x7Fu); }
static void ed_str(const char *s, uint32_t max)
{
    uint32_t i;
    for (i = 0; s && s[i] && i < max; i++)
        ed_b((uint8_t)s[i] & 0x7Fu);
    ed_b(0);
}
static uint32_t ed_unpack7(const uint8_t *a, uint32_t na, uint8_t *out, uint32_t max)
{
    uint32_t n = 0;
    while (na && n < max) {
        uint32_t m = *a++, j;
        na--;
        for (j = 0; j < 7u && na && n < max; j++, na--)
            out[n++] = (uint8_t)(*a++ | ((m >> j) & 1u) << 7);
    }
    return n;
}
static void ed_pack7(const uint8_t *p, uint32_t n)
{
    while (n) {
        uint32_t k = n > 7u ? 7u : n, m = 0, i;
        for (i = 0; i < k; i++)
            m |= (uint32_t)(p[i] >> 7) << i;
        ed_b(m);
        for (i = 0; i < k; i++)
            ed_b(p[i] & 127u);
        p += k;
        n -= k;
    }
}
#include "../firmware/src/editor_dsyn.c"

static int fails;
static void check(int ok, const char *what)
{
    printf("dsyn: %-78s %s\n", what, ok ? "ok" : "FAIL");
    fails += !ok;
}

static int32_t outbuf[2][120 * CTL];
static void voice_render(int32_t *o, const dsnd_t *snd, uint32_t crush, uint32_t lane)
{
    static dsv_t v;
    uint32_t blk;
    rng_state = 0x1234567u;
    ds_on(&v, snd, crush, DS_LANE_NOTE[lane], 110);
    for (blk = 0; blk < 120u; blk++)
        if (!ds_render(&v, o + blk * CTL, CTL))
            memset(o + blk * CTL, 0, CTL * sizeof(int32_t));
}

static int lanes_same(const dsnd_t *a, uint32_t ca, const dsnd_t *b, uint32_t cb)
{
    uint32_t lane, i;
    for (lane = 0; lane < DS_LANES; lane++) {
        voice_render(outbuf[0], a, ca, lane);
        voice_render(outbuf[1], b, cb, lane);
        for (i = 0; i < 120u * CTL; i++)
            if (outbuf[0][i] != outbuf[1][i])
                return 0;
    }
    return 1;
}

static uint32_t put(uint32_t k, uint32_t part, const uint8_t *d, uint32_t nd)
{
    uint8_t a[64];
    uint32_t na = 2, n = nd;
    const uint8_t *p = d;
    a[0] = (uint8_t)k;
    a[1] = (uint8_t)part;
    while (n) {
        uint32_t c = n > 7u ? 7u : n, i, m = 0;
        for (i = 0; i < c; i++)
            m |= (uint32_t)(p[i] >> 7) << i;
        a[na++] = (uint8_t)m;
        for (i = 0; i < c; i++)
            a[na++] = p[i] & 127u;
        p += c;
        n -= c;
    }
    ed_n = 0;
    ed_dsyn_handle(ED_DSYN_PUT, a, na);
    return ed_n == 3u ? ed_out[2] : 99u;
}

int main(void)
{
    uint32_t i, k, ok;
    uint64_t rng_s = 12345;
    host_tracks_init();
    song.g[G_DRREV] = 0;
    song.g[G_DRDLY] = 0;
    song.g[G_BPM] = 120;

    check(DRUM_SYN == DRUM_PAIR + 1u && DRUM_KITS == DRUM_SYN + DSU_N &&
          !strcmp(DRUM_KIT_NAMES[DRUM_SYN], "SYN1") &&
          !strcmp(DRUM_KIT_NAMES[DRUM_KITS - 1u], "SYN4"),
          "kit IDs append SYN1..SYN4 after USR3+4");
    (void)dsu_kit(0);
    ok = 1;
    for (k = 0; k < DSU_N; k++)
        ok &= !memcmp(dsu.k[k].s, DS_KITS[DSU_DEF[k]].s, sizeof dsu.k[k].s) &&
              dsu.k[k].crush == DS_KITS[DSU_DEF[k]].crush &&
              dsu.k[k].src == DSU_DEF[k];
    check(ok, "defaults copy 808, 909, TRAP, TECHNO");
    check(lanes_same(DS_KITS[0].s, DS_KITS[0].crush, dsu.k[0].s, dsu.k[0].crush),
          "SYN1 default audio is bit-identical to 808");

    ed_n = 0;
    ed_dsyn_handle(ED_DSYN_LIST, 0, 0);
    check(ed_out[0] == DS_NKITS && ed_out[1] == DSU_N && ed_out[2] == 1,
          "DSYN_LIST reports factory/user/stored state");
    {
        uint8_t a = (uint8_t)(ED_DSYN_USER + 1u);
        uint8_t got[2u + DS_LANES * sizeof(dsnd_t)];
        uint32_t p;
        ed_n = 0;
        ed_dsyn_handle(ED_DSYN_GET, &a, 1);
        p = 2u + (uint32_t)strlen((char *)ed_out + 2) + 1u;
        k = ed_unpack7(ed_out + p, ed_n - p, got, sizeof got);
        check(ed_out[0] == a && ed_out[1] == 0 && k == sizeof got &&
              got[0] == DS_KITS[1].crush && got[1] == 1 &&
              !memcmp(got + 2, DS_KITS[1].s, sizeof DS_KITS[1].s),
              "DSYN_GET returns complete SYN2 kit");
    }
    {
        uint8_t ff[22], hdr[10] = {'M', 'Y', ' ', 'K', 'I', 'T', 0, 0, 0x21, 3}, src = 5;
        const dsnd_t *d = &dsu.k[2].s[4];
        memset(ff, 0xFF, sizeof ff);
        check(put(2, 4, ff, sizeof ff) == 0 && dsu_dirty &&
              d->wave <= DW_BELL && (d->src & 15u) <= DN_CHIP && d->pitch <= 127u &&
              d->fine <= 15u && d->bend <= 96u && d->drive <= 127u,
              "DSYN_PUT sanitizes an all-0xFF sound");
        check(put(2, DS_LANES, hdr, sizeof hdr) == 0 &&
              !memcmp(dsu.k[2].name, "MY KIT\0\0", 8) &&
              dsu.k[2].crush == 0x21 && dsu.k[2].src == 3,
              "DSYN_PUT stores name/crush/source");
        check(put(3, DS_LANES + 1u, &src, 1) == 0 &&
              !memcmp(dsu.k[3].s, DS_KITS[5].s, sizeof dsu.k[3].s),
              "DSYN_PUT copies a full factory kit");
        check(put(4, 0, ff, sizeof ff) == 1 && put(0, 0, ff, sizeof ff - 1u) == 1,
              "DSYN_PUT rejects invalid kit and short sound");
    }
    {
        dsu_bank_t backup, reloaded;
        memcpy(&backup, &dsu, sizeof backup);
        memset(&dsu, 0, sizeof dsu);
        check(!dsu_valid(&dsu), "cleared RAM bank is invalid");
        memcpy(&reloaded, &backup, sizeof reloaded);
        check(dsu_valid(&reloaded), "backup object validates DSU1/version/count");
        for (k = 0; k < DSU_N; k++)
            dsu_fix_kit(&reloaded.k[k]);
        memcpy(&dsu, &reloaded, sizeof dsu);
        dsu_dirty = 0;
        check(!memcmp(&dsu, &backup, sizeof dsu) &&
              !memcmp(dsu.k[2].name, "MY KIT\0\0", 8),
              "serialized bank reload preserves stored SYN kits");
        backup.magic ^= 1u;
        check(!dsu_valid(&backup), "backup object 9 rejects bad magic");
    }
    {
        uint8_t a[3] = {1, 3, 100};
        uint32_t active = 0;
        for (i = 0; i < NDRUM; i++)
            drums.v[i].active = 0;
        ed_n = 0;
        ed_dsyn_handle(ED_DSYN_PLAY, a, 3);
        drum_audition_poll();
        for (i = 0; i < NDRUM; i++)
            active += drums.v[i].active && drums.synth[i] &&
                      drums.kit[i] == DRUM_SYN + 1u && drums.v[i].note == 42u;
        check(ed_out[2] == 0 && active == 1u && !dsu_aud_vel,
              "DSYN_PLAY queues and consumes one audition");
    }
    {
        static int32_t b[CTL * 2];
        int32_t peak = 0;
        uint32_t t, lane, blk;
        for (t = 0; t < 2000u; t++) {
            uint8_t raw[22];
            for (lane = 0; lane < DS_LANES; lane++) {
                for (i = 0; i < sizeof raw; i++) {
                    rng_s = rng_s * 6364136223846793005ull + 1442695040888963407ull;
                    raw[i] = (uint8_t)(rng_s >> 56);
                }
                put(t & 3u, lane, raw, sizeof raw);
            }
            dsu.k[t & 3u].crush = (uint8_t)(rng_s >> 40);
            TDRUM->p[P_E0] = (int16_t)(DRUM_SYN + (t & 3u));
            drum_on(DS_LANE_NOTE[t % DS_LANES], 30u + t % 98u);
            for (blk = 0; blk < 6u; blk++) {
                mix_block(b, CTL);
                for (i = 0; i < CTL * 2u; i++) {
                    int32_t a = b[i] < 0 ? -b[i] : b[i];
                    if (a > peak) peak = a;
                }
            }
        }
        check(peak > 0 && peak < (1 << 24), "2000 randomized validated kits remain bounded");
    }
    printf(fails ? "dsyn: FAILED\n" : "dsyn: all checks ok\n");
    return fails;
}
