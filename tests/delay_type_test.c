/* SPDX-License-Identifier: GPL-3.0-only */
/* The delay TYPE (ORBIT 0.8.2, G_DTYPE, fx.c fx_buses): an impulse into the delay send, the wet return read.
 *   MONO   the same echo left and right, at the delay time
 *   WIDE   the left echo at the delay time, the right one DLY_WIDE samples later
 *   PING   the first echo left, the next right, the next left again, each FDBK lower; long times halved
 *   switch a TYPE change is silent until the line is clear: no stale echo on the other side */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int fails;
static void check(int ok, const char *what)
{
    printf("delay type: %-74s %s\n", what, ok ? "ok" : "FAIL");
    fails += !ok;
}

#define NOUT 120000u
static int32_t outl[NOUT], outr[NOUT];

/* n samples of the buses with an impulse into the delay send at sample 0 (amp 0: none) */
static void run(uint32_t n, int32_t amp)
{
    static int32_t zero[CTL], din[CTL], wl[CTL], wr[CTL];
    uint32_t b, i;
    for (b = 0; b * CTL < n; b++) {
        memset(din, 0, sizeof din);
        if (!b)
            din[0] = amp;
        fx_buses(zero, din, zero, wl, wr, CTL);
        for (i = 0; i < CTL && b * CTL + i < NOUT; i++)
            outl[b * CTL + i] = wl[i], outr[b * CTL + i] = wr[i];
    }
}

static uint32_t first_echo(const int32_t *o, uint32_t from, uint32_t to)
{
    uint32_t i;
    for (i = from; i < to && i < NOUT; i++)
        if (o[i] > 1000 || o[i] < -1000)
            return i;
    return ~0u;
}

static void settle(uint32_t type)
{
    song.g[G_DTYPE] = (int16_t)type;
    run(DLY_LEN + 4u * CTL, 0);                      /* (the line cleared after a change, then nothing left) */
}

int main(void)
{
    uint32_t i, dl, a, b, c;
    int32_t peak;
    for (i = 0; i < G_COUNT; i++)
        song.g[i] = GP[i].def;
    song.g[G_BPM] = 120, song.g[G_DTIME] = 1;           /* 1/8 at 120 BPM */
    song.g[G_DFDBK] = 60, song.g[G_DCOLOR] = 127, song.g[G_DMIX] = 127;
    dl = delay_samples();

    settle(0);
    run(4u * dl, 40000);
    a = first_echo(outl, 1, 4u * dl), b = first_echo(outr, 1, 4u * dl);
    check(a == dl && b == dl && !memcmp(outl, outr, 4u * dl * sizeof outl[0]), "MONO: one echo in the middle at the time");

    settle(1);
    run(4u * dl, 40000);
    a = first_echo(outl, 1, 4u * dl), b = first_echo(outr, 1, 4u * dl);
    check(a == dl && b == dl + DLY_WIDE, "WIDE: left at the time, right 13 ms later");
    check(first_echo(outl, dl + 1u, 2u * dl - 8u) == ~0u && first_echo(outl, 2u * dl - 8u, 3u * dl) == 2u * dl,
          "WIDE: the repeats keep the time (FDBK through the left)");

    settle(2);
    run(4u * dl, 40000);
    a = first_echo(outl, 1, 4u * dl), b = first_echo(outr, 1, 4u * dl), c = first_echo(outl, a + 1u, 4u * dl);
    check(a == dl && b == 2u * dl && c == 3u * dl, "PING: left, then right, then left again");
    check(outl[dl] > outr[2u * dl] && outr[2u * dl] > outl[3u * dl] && outl[3u * dl] > 0,
          "PING: each bounce lower by FDBK");
    song.g[G_DTIME] = 0, song.g[G_BPM] = 41;            /* 1/4 at 41 BPM: 1.46 s, longer than half the line */
    settle(0);
    settle(2);                                          /* (the change clears the line: no echo of the cases above) */
    run(3u * DLY_HALF, 40000);
    check(first_echo(outl, 1, 3u * DLY_HALF) == DLY_HALF - 1u && first_echo(outr, 1, 3u * DLY_HALF) == 2u * (DLY_HALF - 1u),
          "PING: a time past half the line is clamped to it");
    song.g[G_DTIME] = 1, song.g[G_BPM] = 120;

    settle(0);
    run(dl / 2u, 40000);                                /* an echo on its way in MONO .. */
    song.g[G_DTYPE] = 2;                                /* .. then PING */
    run(4u * dl, 0);
    for (peak = 0, i = 0; i < 4u * dl; i++) {
        int32_t v = outl[i] < 0 ? -outl[i] : outl[i], w = outr[i] < 0 ? -outr[i] : outr[i];
        if (v > peak) peak = v;
        if (w > peak) peak = w;
    }
    check(peak == 0, "a TYPE change: no stale echo while the line is cleared");
    run(DLY_LEN, 0);
    run(4u * dl, 40000);
    check(first_echo(outl, 1, 4u * dl) == dl && first_echo(outr, 1, 4u * dl) == 2u * dl,
          "after the change: PING echoes as it should");

    printf("delay type: %d failed\n", fails);
    return fails != 0;
}
