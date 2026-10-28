/* SPDX-License-Identifier: GPL-3.0-only */
/* Clock timestamps are synthetic milliseconds, not wall-clock sleeps. */
#define main hostsim_main
#include "hostsim.c"
#undef main

static void beat(uint32_t dt, uint32_t src)
{
    uint32_t i, start = fm1_ms;
    for (i = 1; i <= 24u; i++) {
        fm1_ms = start + (dt * i + 12u) / 24u;
        mclk_event(0xF8u, src);
    }
}

static void init(uint32_t src)
{
    host_tracks_init();
    memset(&mclk, 0, sizeof mclk);
    song.g[G_SYNC] = (int16_t)src;
    song.g[G_BPM] = 140;
    fm1_ms = 1000;
    mclk_event(0xF8u, src);
}

static int steady(uint32_t src)
{
    uint32_t i;
    int ok = 1;
    init(src);
    for (i = 0; i < 8u; i++)
        beat(i % 2u ? 428u : 429u, src);
    for (i = 0; i < 64u; i++) {
        beat(i % 2u ? 432u : 425u, src);
        ok &= song.g[G_BPM] == 140;
    }
    return ok;
}

int main(void)
{
    int fails = 0, ok;
    uint32_t i;
#define CHECK(cond, msg) do { int pass = (cond); printf("midi_clock: %s: %s\n", msg, pass ? "ok" : "FAIL"); fails += !pass; } while (0)
    CHECK(steady(1), "USB steady 140 BPM stays 140 with millisecond jitter");
    CHECK(steady(2), "TRS steady 140 BPM stays 140 with millisecond jitter");
    ok = steady(1);
    beat(333, 1);
    ok &= song.g[G_BPM] == 180;
    beat(600, 1);
    ok &= song.g[G_BPM] == 100;
    CHECK(ok, "averaged clock takes both upward and downward jumps on first beat");
    ok = steady(1);
    fm1_ms += 600;
    mclk_event(0xF8u, 1);
    beat(500, 1);
    ok &= song.g[G_BPM] == 120;
    for (i = 0; i < 8u; i++)
        beat(500, 1);
    for (i = 0; i < 8u; i++)
        beat(510 + i * 2u, 1);
    ok &= song.g[G_BPM] < 120;
    CHECK(ok, "clock timeout discards old beats and a small tempo ramp still follows");
    printf("midi_clock: %d failed\n", fails);
    return fails;
}
