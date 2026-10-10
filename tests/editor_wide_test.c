/* SPDX-License-Identifier: GPL-3.0-only */
/* Use the actual editor, user presets, sequencer and engines; replace only USB/flash boundaries. */
#define FELUCCA_FLASH 0
#define FELUCCA_OTA 0
#define FELUCCA_VERSION "ORBIT TEST"
#define main hostsim_main
#include "hostsim.c"
#undef main

static struct { uint8_t force; } ui;
static uint8_t sync_reload, flash_ok;
static uint32_t up_gen;
static void ui_message(const char *s) { (void)s; }
static void ui_say(const char *a, const char *b) { (void)a; (void)b; }
static int param_kept(uint32_t i) { (void)i; return 0; }
static void set_engine(uint32_t e) { host_preset_req(TSEL, e, 0); }
static void apply_preset(uint32_t p) { host_preset_req(TSEL, TSEL->eng_req, p); }
static void track_select(uint32_t i) { song.sel = (uint8_t)i; }
static int project_used(uint32_t s) { (void)s; return 0; }
static void project_save(uint32_t s) { (void)s; }
static void project_load(uint32_t s) { (void)s; }
static void fm1_wdt_feed(void) {}
static void fl_inval(uint32_t a, uint32_t n) { (void)a; (void)n; }
static int fl_erase4k_quiet(uint32_t a, uint32_t *t) { (void)a; (void)t; abort(); }
static int fl_write(uint32_t a, const void *p, uint32_t n) { (void)a; (void)p; (void)n; abort(); }
static uint32_t st_crc32(const void *p, uint32_t n) { (void)p; (void)n; abort(); }
#include "../firmware/src/upreset.c"

static uint8_t wire[16][600];
static uint32_t wire_n[16], nw;
#define SXQ 64u
static uint32_t so_w, so_r;
static int ota_wire_send(const uint8_t *p, uint32_t n)
{
    if (nw >= 16u || n > sizeof wire[0])
        abort();
    memcpy(wire[nw], p, n);
    wire_n[nw++] = n;
    return 0;
}
static int ota_frame_get(const uint8_t **p, uint32_t *n) { (void)p; (void)n; return 0; }
static void ota_frame_done(void) {}
#include "../firmware/src/editor.c"

static int fails;
static void check(int ok, const char *what)
{
    printf("editor-wide: %-72s %s\n", what, ok ? "ok" : "FAIL");
    fails += !ok;
}
static uint32_t enc(uint8_t *p, int32_t v, uint32_t wide)
{
    uint32_t u = (uint32_t)(v + (wide ? 32768 : 8192));
    p[0] = u & 127u;
    p[1] = (u >> 7) & 127u;
    if (wide)
        p[2] = (u >> 14) & 127u;
    return wide ? 3u : 2u;
}
static int32_t dec(const uint8_t *p, uint32_t wide)
{
    return (int32_t)(p[0] | p[1] << 7 | (wide ? p[2] << 14 : 0)) - (wide ? 32768 : 8192);
}
static void request(uint32_t cmd, uint32_t wide, const uint8_t *a, uint32_t na)
{
    uint8_t f[600] = {0x7D, 0x46, 0x4C};
    uint32_t n = 3;
    f[n++] = (uint8_t)(wide ? 77u : cmd);
    if (wide)
        f[n++] = (uint8_t)cmd;
    if (na)
        memcpy(f + n, a, na);
    nw = 0;
    ed_handle(f, n + na);
}
static int frame(uint32_t cmd, uint32_t wide, uint32_t body)
{
    uint32_t n = 6u + wide + body, i;
    if (nw != 1u || wire_n[0] != n || wire[0][0] != 0xF0 || wire[0][1] != 0x7D ||
        wire[0][2] != 0x46 || wire[0][3] != 0x4C || wire[0][4] != (wide ? 77u : cmd) ||
        (wide && wire[0][5] != cmd) || wire[0][n - 1u] != 0xF7)
        return 0;
    for (i = 1; i < n - 1u; i++)
        if (wire[0][i] >= 128u)
            return 0;
    return 1;
}
static uint32_t preset_args(uint8_t *a, uint32_t wide)
{
    uint32_t n = 0, i;
    a[n++] = 5;
    a[n++] = ENGI_CLUSTER;
    a[n++] = 'R'; a[n++] = 'A'; a[n++] = 'W'; a[n++] = 0;
    for (i = 0; i < P_COUNT; i++)
        n += enc(a + n, TSEL->p[i], wide);
    for (i = 0; i < 16u; i++) {
        a[n++] = 60;
        a[n++] = SF_ACCENT;
    }
    return n;
}

int main(void)
{
    uint8_t a[600];
    uint32_t w, i, j, n, b, width, slot;
    static const int16_t raw[] = {-32768, -8193, -1, 0, 8191, 8192, 18431, 24575, 32767};
    static const uint8_t ids[] = {P_E0, P_E0 + 1, P_E0 + 2, P_E0 + 3};
    static const int16_t bounds[][2] = {{2048, 18431}, {-32768, 32767}, {0, 24575}, {-32768, 32767}};
    host_tracks_init();
    host_preset(&trk[0], ENGI_CLUSTER, 0);
    host_preset(&trk[1], ENGI_CLUSTER, 0);
    song.sel = 0;
    request(ED_INFO, 0, 0, 0);
    check(nw == 1u && wire[0][wire_n[0] - 2u] == 11u && ED_WIDE == 77u,
          "INFO protocol11 and envelope77");

    for (w = 0; w < 2u; w++) {
        width = w ? 3u : 2u;
        b = 5u + w;
        for (i = 0; i < sizeof raw / sizeof raw[0]; i++) {
            int32_t v = w ? raw[i] : clamp(raw[i], -8192, 8191);
            a[0] = 0; a[1] = P_E0 + 1;
            n = 2u + enc(a + 2, v, w);
            request(ED_SET, w, a, n);
            check(frame(ED_SET, w, 2u + width) && TSEL->p[P_E0 + 1] == v &&
                  !memcmp(wire[0] + b, a, n), "SET exact bytes and signed boundary state");
            request(ED_GET, w, a, 2);
            check(frame(ED_GET, w, 2u + width) && dec(wire[0] + b + 2, w) == v,
                  "GET preserves request width");
            a[0] = 1;
            request(ED_TRACK_PARAM, w, a, n);
            check(frame(ED_TRACK_PARAM, w, 2u + width) && trk[1].p[P_E0 + 1] == v &&
                  dec(wire[0] + b + 2, w) == v, "TRACK_PARAM exact signed write/read");
            a[0] = 0; a[1] = 2; a[2] = P_E0 + 1;
            n = 3u + enc(a + 3, v, w);
            request(ED_LOCK_SET, w, a, n);
            j = (uint32_t)lock_find(TSEL, 2, P_E0 + 1, 0);
            check(frame(ED_LOCK_SET, w, 5u + width) && j < NLOCK && TSEL->lock[j].val == v &&
                  wire[0][b + 3] == 0 && wire[0][b + 4] == 1 &&
                  dec(wire[0] + b + 5, w) == v, "LOCK_SET signed value and ordinary rc/has fields");
            request(ED_LOCK_GET, w, a, 1);
            check(frame(ED_LOCK_GET, w, 4u + width) && wire[0][b + 1] == 1 &&
                  dec(wire[0] + b + 4, w) == v, "LOCK_GET count and width stride");
        }
        a[0] = 0; a[1] = P_E0 + 1;
        request(ED_DESC, w, a, 2);
        check(frame(ED_DESC, w, 3u + 3u * width + 6u) &&
              dec(wire[0] + b + 3, w) == (w ? -32768 : -8192) &&
              dec(wire[0] + b + 3 + width, w) == (w ? 32767 : 8191) &&
              dec(wire[0] + b + 3 + 2u * width, w) == 4000 &&
              !strcmp((char *)wire[0] + b + 3 + 3u * width, "WAVE"),
              "DESC signed min/max/default; strings retain layout");
        request(ED_DUMP, w, 0, 0);
        n = 1;
        for (i = 0; i < P_COUNT; i++)
            n &= dec(wire[0] + b + 2 + width * i, w) ==
                 (w ? TSEL->p[i] : clamp(TSEL->p[i], -8192, 8191));
        for (i = 0; i < G_COUNT; i++)
            n &= dec(wire[0] + b + 2 + width * (P_COUNT + i), w) == song.g[i];
        check(frame(ED_DUMP, w, 2u + ED_NV * width) && n, "DUMP complete parameter/global widths");
        a[0] = 1;
        request(ED_TRACK_DUMP, w, a, 1);
        check(frame(ED_TRACK_DUMP, w, 3u + P_COUNT * width) &&
              dec(wire[0] + b + 3 + width * (P_E0 + 1), w) == (w ? 32767 : 8191),
              "TRACK_DUMP value count and stride");
        a[0] = 1;
        n = 1u + enc(a + 1, 93, w);
        a[n++] = 1;
        request(ED_TRACK_MIX, w, a, n);
        check(frame(ED_TRACK_MIX, w, 2u + width) && trk[1].p[P_LEVEL] == 93 &&
              trk[1].p[P_MUTE] == 1 && wire[0][b + 1 + width] == 1,
              "TRACK_MIX mute follows width-dependent signed level");
        request(ED_TRACK, w, 0, 0);
        check(frame(ED_TRACK, w, 3u + NTRK * (4u + width)) &&
              dec(wire[0] + b + 2 + 4u + width + 2, w) == 93,
              "TRACK signed levels also support envelope");
        a[0] = 0; a[1] = 2; a[2] = P_E0 + 1;
        request(ED_LOCK_SET, w, a, 3);
        check(frame(ED_LOCK_SET, w, 5u + width) && lock_find(TSEL, 2, P_E0 + 1, 0) < 0,
              "LOCK_SET exact three-byte delete remains supported");
    }
    for (i = 0; i < 4u; i++)
        for (j = 0; j < 2u; j++) {
            a[0] = 0; a[1] = ids[i];
            n = 2u + enc(a + 2, bounds[i][j], 1);
            request(ED_SET, 1, a, n);
            check(TSEL->p[ids[i]] == bounds[i][j] && dec(wire[0] + 8, 1) == bounds[i][j],
                  "CLUSTER COUNT/WAVE/CURVE/SPREAD raw limits remain exact");
        }
    a[0] = 0; a[1] = P_E0 + 1;
    request(ED_GET, 0, a, 2);
    check(frame(ED_GET, 0, 4) && wire[0][7] == 127 && wire[0][8] == 127 &&
          TSEL->p[P_E0 + 1] == 32767,
          "legacy GET keeps literal v14 saturation bytes without changing raw state");
    request(ED_GET, 1, a, 2);
    check(frame(ED_GET, 1, 5) && wire[0][8] == 127 && wire[0][9] == 127 && wire[0][10] == 3,
          "wide32767 is literal 7f7f03");
    a[0] = 1;
    enc(a + 1, 0, 1);
    for (n = 2; n < 5u; n++) {
        request(ED_TRACK_MIX, 1, a, n);
        check(nw == 0 && trk[1].p[P_LEVEL] == 93 && trk[1].p[P_MUTE] == 1,
              "partial wide TRACK_MIX cannot change level or mute");
    }
    /* Short or out-of-range wide input must not change values, locks or presets. */
    a[0] = 0; a[1] = P_E0 + 1;
    enc(a + 2, -32768, 1);
    for (n = 2; n < 5u; n++) {
        request(ED_SET, 1, a, n);
        check(nw == 0 && TSEL->p[P_E0 + 1] == 32767, "short wide SET cannot mutate");
        request(ED_TRACK_PARAM, 1, a, n);
        check(TSEL->p[P_E0 + 1] == 32767, "short TRACK_PARAM cannot mutate");
    }
    a[4] = 4;
    request(ED_SET, 1, a, 5);
    check(nw == 0 && TSEL->p[P_E0 + 1] == 32767, "wide SET rejects high byte above3");
    lock_set(TSEL, 2, P_E0 + 1, 24575);
    a[0] = 0; a[1] = 2; a[2] = P_E0 + 1;
    enc(a + 3, 0, 1);
    for (n = 4; n < 6u; n++) {
        request(ED_LOCK_SET, 1, a, n);
        j = (uint32_t)lock_find(TSEL, 2, P_E0 + 1, 0);
        check(nw == 0 && j < NLOCK && TSEL->lock[j].val == 24575,
              "short wide LOCK_SET cannot delete or overwrite");
    }
    a[5] = 4;
    request(ED_LOCK_SET, 1, a, 6);
    check(nw == 0 && TSEL->lock[lock_find(TSEL, 2, P_E0 + 1, 0)].val == 24575,
          "invalid wide LOCK_SET cannot mutate");
    for (w = 0; w < 2u; w++) {
        up_rec_t parsed;
        host_preset_req(TSEL, ENGI_CLUSTER, 0);
        TSEL->p[P_E0] = w ? 18431 : 4000;
        TSEL->p[P_E0 + 1] = w ? -32768 : -8192;
        TSEL->p[P_E0 + 2] = w ? 24575 : 8000;
        TSEL->p[P_E0 + 3] = w ? 32767 : 8191;
        n = preset_args(a, w);
        check(!(w ? up_parse_width(a, n, &parsed, &slot, 3) : up_parse(a, n, &parsed, &slot)) &&
              slot == 5 && !memcmp(parsed.p, TSEL->p, sizeof TSEL->p),
              "shared UP parser preserves all values in both widths");
        request(ED_UP_PUT, w, a, n);
        check(frame(ED_UP_PUT, w, 2) && up_used(5) &&
              !memcmp(up_rec(5)->p, TSEL->p, sizeof TSEL->p),
              "real UP_PUT retains full raw values in unchanged RAM record");
        a[0] = 5;
        request(ED_UP_GET, w, a, 1);
        b = 5u + w;
        width = w ? 3u : 2u;
        check(frame(ED_UP_GET, w, 7u + P_COUNT * width + 32u) &&
              !strcmp((char *)wire[0] + b + 3, "RAW") &&
              dec(wire[0] + b + 7 + (P_E0 + 1) * width, w) == TSEL->p[P_E0 + 1] &&
              wire[0][b + 7 + P_COUNT * width] == 60 &&
              wire[0][b + 8 + P_COUNT * width] == SF_ACCENT,
              "UP_GET strings/count/pattern layout and signed stride");
    }
    {
        up_rec_t before = *up_rec(5), parsed = before;
        n = preset_args(a, 1);
        for (i = 1; i < n; i++) {
            request(ED_UP_PUT, 1, a, i);
            if (memcmp(&before, up_rec(5), sizeof before))
                break;
        }
        check(i == n, "every truncated wide UP_PUT leaves bank record unchanged");
        a[n] = 0;
        request(ED_UP_PUT, 1, a, n + 1u);
        check(wire[0][7] == 1 && !memcmp(&before, up_rec(5), sizeof before),
              "wide preset rejects trailing fields");
        a[6 + 2] = 4;
        request(ED_UP_PUT, 1, a, n);
        check(wire[0][7] == 1 && !memcmp(&before, up_rec(5), sizeof before) &&
              up_parse_width(a, n, &parsed, &slot, 3) == 1 && !memcmp(&parsed, &before, sizeof before),
              "invalid preset high byte rejected before record mutation");
    }
    request(ED_WIDE, 1, 0, 0);
    check(nw == 0, "recursive envelope rejected");
    request(ED_WIDE, 0, 0, 0);
    check(nw == 0, "empty envelope rejected");
    request(ED_DSYN_GET, 1, a, 1);
    check(nw == 0, "wide DSYN request rejected without altering allocated commands");
    request(ED_INFO, 1, 0, 0);
    check(nw == 0, "unsupported wide INFO rejected");
    request(ED_PING, 0, 0, 0);
    check(frame(ED_PING, 0, 1), "plain request resets temporary wide codec");
    for (w = 1; ; w--) {
        usb.config = 1;
        fm1_ms += 100;
        a[0] = 3;
        request(ED_WATCH, w, a, 1);
        check(frame(ED_WATCH, w, 1) && wire[0][5u + w] == 3, "WATCH reply retains ordinary mask byte");
        request(ED_GET, !w, (uint8_t[]){0, P_LEVEL}, 2);
        nw = 0;
        TSEL->p[P_E0 + 1] = -24576;
        trk[1].p[P_PAN] = -43;
        fm1_ms += 100;
        ed_sync();
        n = 0;
        for (i = 0; i < nw; i++) {
            b = 5u + w;
            if (wire[i][4] != (w ? 77u : ED_CHANGED) && wire[i][4] != ED_TRACK_CHANGED)
                continue;
            j = w ? wire[i][5] : wire[i][4];
            if (j == ED_CHANGED && wire_n[i] == 8u + w + (w ? 3u : 2u) &&
                wire[i][b + 1] == P_E0 + 1 && dec(wire[i] + b + 2, w) == (w ? -24576 : -8192))
                n |= 1;
            if (j == ED_TRACK_CHANGED && wire[i][b] == 1 && wire[i][b + 1] == P_PAN &&
                dec(wire[i] + b + 2, w) == -43)
                n |= 2;
        }
        check(n == 3, "WATCH width persists across opposite-width requests for both value pushes");
        nw = 0;
        sync_reload = 1;
        fm1_ms += 100;
        ed_sync();
        check(frame(ED_RELOAD, 0, 3), "non-value RELOAD push keeps original layout");
        if (!w)
            break;
        /* Ensure the next plain WATCH also sees a value change. */
        TSEL->p[P_E0 + 1] = 0;
        trk[1].p[P_PAN] = 0;
    }
    printf("editor-wide: %d failures\n", fails);
    return fails ? 1 : 0;
}
