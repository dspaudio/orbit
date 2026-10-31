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

/* SYNC = OUT: run audio blocks for secs seconds; count START, clock and STOP packets sent to USB MIDI */
static uint32_t n_start, n_clock, n_stop, first_pkt;
static void out_drain(void)
{
    while (mo_r != mo_w) {
        uint32_t p = midi_out_q[mo_r % MQ] & 0xFFFFu;
        if (!first_pkt)
            first_pkt = p;
        n_start += p == 0xFA0Fu;
        n_clock += p == 0xF80Fu;
        n_stop += p == 0xFC0Fu;
        mo_r++;
    }
}
static void run_secs(uint32_t secs)
{
    uint32_t k;
    int32_t o[2 * CTL];
    for (k = 0; k < secs * FS / CTL; k++) {
        mix_block(o, CTL);
        out_drain();
    }
}
static void out_reset(void)
{
    out_drain();
    n_start = n_clock = n_stop = first_pkt = 0;
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

    host_tracks_init();
    memset(&mclk, 0, sizeof mclk);
    usb.config = 1;
    song.g[G_SYNC] = 3;                               /* OUT */
    song.g[G_BPM] = 120;
    out_reset();
    transport_req = 1;
    run_secs(4);
    CHECK(first_pkt == 0xFA0Fu && n_start == 1u && n_stop == 0u, "SYNC OUT: PLAY sends START before the first clock");
    CHECK(n_clock >= 191u && n_clock <= 193u, "SYNC OUT: 24 clocks a beat at 120 BPM (192 in 4 s)");
    out_reset();
    song.g[G_BPM] = 60;
    run_secs(4);
    CHECK(n_clock >= 95u && n_clock <= 97u, "SYNC OUT: a tempo change is followed at once (96 in 4 s at 60 BPM)");
    out_reset();
    transport_req = 2;
    run_secs(1);
    CHECK(n_stop == 1u && n_clock == 0u && !song.playing, "SYNC OUT: stop sends STOP, then no clock");
    out_reset();
    song.g[G_SYNC] = 0;                               /* INT */
    transport_req = 1;
    run_secs(2);
    transport_req = 2;
    run_secs(1);
    CHECK(n_start + n_clock + n_stop == 0u, "SYNC INT: no clock, START or STOP is sent");
    song.g[G_SYNC] = 3;
    mclk.alive = 1;
    mclk.last_ms = fm1_ms;
    CHECK(!mclk_on(), "SYNC OUT never follows an incoming clock");
    printf("midi_clock: %d failed\n", fails);
    return fails;
}
