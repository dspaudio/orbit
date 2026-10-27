/* SPDX-License-Identifier: GPL-3.0-only */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int fails;
static void check(int ok, const char *what)
{
    printf("track filter: %s %s\n", what, ok ? "ok" : "FAIL");
    fails += !ok;
}

static double gain(int v, int res, int bp, double hz)
{
    djf_t f = {0};
    tsvf_t c;
    double in = 0, out = 0;
    uint32_t b, i;
    for (b = 0; b < 1024; b++) {
        int32_t x[CTL];
        int active = djf_block(&f, v, res, bp, &c);
        for (i = 0; i < CTL; i++)
            x[i] = (int32_t)(4000 * sin(2 * M_PI * hz * (b * CTL + i) / FS)) * 4;
        if (b >= 512)
            for (i = 0; i < CTL; i++) in += (double)x[i] * x[i];
        if (active) tflt_run(&f, &c, x, CTL, 0);
        if (b >= 512)
            for (i = 0; i < CTL; i++) out += (double)x[i] * x[i];
    }
    return 10 * log10(out / in);
}

static void transitions(void)
{
    djf_t f = {0};
    tsvf_t c;
    int32_t x[CTL], last[4] = {40000, 40000, 40000, 40000}, jump = 0;
    uint32_t b, i, ch;
    for (b = 0; b < 40; b++) {
        int active = djf_block(&f, b < 20 ? -1 : 0, 40, 1, &c);
        for (ch = 0; ch < 4; ch++) {
            for (i = 0; i < CTL; i++) x[i] = 40000;
            if (active) tflt_run(&f, &c, x, CTL, ch);
            for (i = 0; i < CTL; i++) {
                int32_t d = abs(x[i] - last[ch]);
                if (d > jump) jump = d;
                last[ch] = x[i];
            }
        }
    }
    printf("  BP bypass transition maximum DC jump %d\n", jump);
    check(jump < 1000 && last[0] == 40000 && last[1] == 40000 &&
          last[2] == 40000 && last[3] == 40000 && f.mode == 0,
          "BP entry/zero exit ramp on dry stereo and both drum sends");
}

static void arithmetic(void)
{
    int exact = 1;
    uint32_t cut, res, i;
    for (cut = 0; cut < 128; cut++)
        for (res = 0; res < 128; res += 127) {
            djf_t f = {0};
            tsvf_t c;
            int32_t a = 150000, b = -150000;
            tsvf_coef(&c, cut << 8, res);
            f.mode = 2;
            /* Settle the bypass ramp before comparing the saturated-state oracle. */
            for (i = 0; i < 16; i++) {
                int32_t x[CTL] = {0};
                djf_block(&f, cut == 64 ? 1 : (int)cut - 64, res, 1, &c);
                tflt_run(&f, &c, x, CTL, 0);
            }
            tsvf_coef(&c, cut << 8, res);
            f.z[0][0] = a; f.z[0][1] = b;
            for (i = 0; i < 32; i++) {
                int32_t x = i & 1 ? -140000 : 140000, got = x;
                int32_t v3 = x - b;
                int32_t v1 = (int32_t)(((int64_t)c.a1 * a + (int64_t)c.a2 * v3) >> 13);
                int32_t v2 = b + (int32_t)(((int64_t)c.a2 * a + (int64_t)c.a3 * v3) >> 13);
                a = clamp(2 * v1 - a, -150000, 150000);
                b = clamp(2 * v2 - b, -150000, 150000);
                djf_run_bp(&f, &c, &got, 1, 0);
                exact &= got == v1 && f.z[0][0] == a && f.z[0][1] == b;
            }
        }
    check(exact, "BP bounded states agree with wide arithmetic at every cutoff");
}

static void legacy(void)
{
    djf_t f = {0};
    tsvf_t c, old;
    uint32_t i, ch, hp;
    int exact = 1;
    for (hp = 0; hp < 2; hp++) {
      int32_t cut = hp ? 0 : 127 << 8, z[4][2] = {{0}};
      memset(&f, 0, sizeof f);
      for (i = 0; i < 128; i++) {
        int32_t to = hp ? 32 * 360 : (127 << 8) - 32 * 360;
        cut += clamp(to - cut, -384, 384);
        djf_block(&f, hp ? 32 : -32, 40, 0, &c);
        tsvf_coef(&old, cut, 40);
        exact &= !memcmp(&c, &old, sizeof c);
        for (ch = 0; ch < 4; ch++) {
            int32_t x[CTL], want[CTL];
            uint32_t k;
            for (k = 0; k < CTL; k++) x[k] = want[k] = ((i * CTL + k) * 7919 % 20000) - 10000;
            if (!i && !hp) z[ch][1] = want[0];
            for (k = 0; k < CTL; k++) {
                int32_t y = tsvf_lp(&old, want[k], &z[ch][0], &z[ch][1]);
                want[k] = hp ? want[k] - y : y;
            }
            djf_run(&f, &c, x, CTL, ch);
            exact &= !memcmp(x, want, sizeof x);
        }
      }
    }
    check(exact, "default RES=40 LP/HP coefficients, seeding and all four channels remain exact");
}

int main(void)
{
    double hz = atan(SVF_G[63] / 4096.0) * FS / M_PI;
    double lo = gain(-1, 40, 1, hz / 8), mid = gain(-1, 40, 1, hz), hi = gain(-1, 40, 1, hz * 8);
    double r0 = gain(-1, 0, 1, hz), r127 = gain(-1, 127, 1, hz);
    printf("  BP %.1f Hz: low %.2f, centre %.2f, high %.2f dB; RES 0 %.2f, 127 %.2f dB\n",
           hz, lo, mid, hi, r0, r127);
    check(mid > lo + 12 && mid > hi + 12 && lo < -12 && hi < -12,
          "BP attenuates both ends of the spectrum");
    check(r127 > r0 + 15, "RES audibly raises the BP centre peak");
    {
        int32_t cut = (127 << 8) - 32 * 360;
        double g = SVF_G[cut >> 8] +
                   ((SVF_G[(cut >> 8) + 1] - SVF_G[cut >> 8]) * (cut & 255) >> 8);
        double centre = atan(g / 4096.0) * FS / M_PI;
        double lp0 = gain(-32, 0, 0, centre), lp127 = gain(-32, 127, 0, centre);
        double hpcentre = atan(SVF_G[(32 * 360) >> 8] / 4096.0) * FS / M_PI;
        double hp0 = gain(32, 0, 0, hpcentre), hp127 = gain(32, 127, 0, hpcentre);
        printf("  LP RES peak %.2f -> %.2f dB; HP at %.1f Hz %.2f -> %.2f dB\n",
               lp0, lp127, hpcentre, hp0, hp127);
        check(lp127 > lp0 + 15 && hp127 > hp0 + 5, "RES changes LP and HP spectra too");
    }
    check(gain(0, 127, 1, hz) == 0 && gain(0, 127, 0, hz) == 0,
          "both types bypass exactly at rest with FILT zero");
    check(TP[P_TRES].def == 40 && TP[P_TFTYPE].def == 0 &&
          P_TRES == 63 && P_TFTYPE == 64 && P_E0 == 65 && F_BPF == 16,
          "parameter defaults and numeric contract");
    legacy();
    transitions();
    arithmetic();
    return fails ? 1 : 0;
}
