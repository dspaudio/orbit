/* SPDX-License-Identifier: GPL-3.0-only */
/* Fuzz of the web editor's SysEx protocol (firmware/src/editor.c ed_handle and what it reaches) and of the other
 * external inputs (USB-MIDI, TRS MIDI, the flash at power-on) on the host harness tests/editor_host.c, under
 * AddressSanitizer + UBSan. Ported from SLOOP 2.8.4, isod89/sloop-fm1 91f3018 (tests/editor_fuzz.c), adapted to
 * ORBIT: editor protocol 13 (ED_WIDE three-byte signed values, the macro commands), ORP8 / ORP7 projects and the
 * FUN1..FUN5 imports (FUN6 must be refused), OPB3 preset banks, the settings object's SYN kit and MIDI parts.
 *   editor_fuzz MODE ITER SEED
 *   MODE 0  single frames: every command byte 0..255 (legacy and ED_WIDE), lengths at the edges (4, 5, the
 *           expected, -1, +1, huge), arguments 7-bit (and 8-bit when injected straight into the frame buffer)
 *   MODE 1  sequences: sample uploads (begin / write / end, in and out of order), backup restore of every
 *           object (valid CRC, random or mutated content: checked "as a load"), user presets, macros, FM6 patches,
 *           SYN kits, steps, locks, the transport; interleaved with UI frames (random panel use) and the audio
 *   MODE 2  USB-MIDI in: random event packets (every CIN, cable, channel, notes vel 0, CC / NRPN / bend /
 *           pressure / program), TRS bytes, with the audio and UI running
 *   MODE 3  boots from a damaged / foreign flash (a child each)
 *   MODE 4  every command over every engine / track / id / step at the edges, deterministic
 *   MODE 5  MIDI in only, then every note let go: no voice may be left held
 *   MODE 6  backup / section / autosave round trips give the project (sound, macros, pattern) back
 * After every frame: every track parameter, macro and lock inside its range, steps sane, the audio bounded.
 * A failure prints the last operations (the reproducer) and aborts. */
#include <stddef.h>
#include "editor_host.c"
#include <sanitizer/common_interface_defs.h>

static uint32_t sd = 1;
static uint32_t rn(uint32_t n) { sd ^= sd << 13; sd ^= sd >> 17; sd ^= sd << 5; return n ? sd % n : 0; }

/* ---- the reproducer log: the last operations */
#define LOGN 48
typedef struct { uint8_t kind; uint16_t n; uint8_t b[700]; uint32_t ms; } op_t;
enum { OP_INJECT, OP_USB, OP_UI, OP_AUDIO, OP_MIDI, OP_UART, OP_BOOT, OP_TRANSPORT };
static op_t lg[LOGN];
static uint32_t lgn;
static void logop(uint32_t kind, const uint8_t *b, uint32_t n)
{
    op_t *o = &lg[lgn++ % LOGN];
    o->kind = (uint8_t)kind;
    o->n = (uint16_t)(n > sizeof o->b ? sizeof o->b : n);
    o->ms = fm1_ms;
    if (b) memcpy(o->b, b, o->n);
}
static void dump(void)
{
    uint32_t i, j, s = lgn > LOGN ? lgn - LOGN : 0;
    fprintf(stderr, "---- last %u operations (oldest first), seed state %u ----\n", lgn - s, sd);
    for (i = s; i < lgn; i++) {
        const op_t *o = &lg[i % LOGN];
        static const char *K[] = {"inject", "usb", "ui", "audio", "midi", "uart", "boot", "transport"};
        fprintf(stderr, "#%u %s ms=%u n=%u:", i, K[o->kind], o->ms, o->n);
        for (j = 0; j < o->n && j < 160u; j++)
            fprintf(stderr, " %02X", o->b[j]);
        fprintf(stderr, "%s\n", o->n > 160u ? " ..." : "");
    }
}
static void on_death(void) { dump(); }

static void fail(const char *what)
{
    dump();
    fprintf(stderr, "FAIL: %s\n", what);
    abort();
}
static void after(void)
{
    char why[256];
    if (!params_check(why, sizeof why))
        fail(why);
    if (audio_bad)
        fail("audio out of bounds");
}

static void frame(const uint8_t *f, uint32_t n, int via_usb)
{
    logop(via_usb ? OP_USB : OP_INJECT, f, n);
    if (via_usb)
        usb_sysex(f, n);
    else
        ed_inject(f, n);
    after();
}
static void cmdv(uint32_t cmd, const uint8_t *a, uint32_t na)
{
    uint8_t f[700];
    f[0] = ED_HDR0; f[1] = ED_HDR1; f[2] = ED_HDR2; f[3] = (uint8_t)cmd;
    if (na > sizeof f - 4) na = sizeof f - 4;
    memcpy(f + 4, a, na);
    frame(f, 4 + na, rn(4) == 0);
}
/* the same command in an ED_WIDE frame (its values three bytes, offset 32768) */
static void cmdw(uint32_t cmd, const uint8_t *a, uint32_t na)
{
    uint8_t f[700];
    f[0] = ED_HDR0; f[1] = ED_HDR1; f[2] = ED_HDR2; f[3] = ED_WIDE; f[4] = (uint8_t)cmd;
    if (na > sizeof f - 5) na = sizeof f - 5;
    memcpy(f + 5, a, na);
    frame(f, 5 + na, rn(4) == 0);
}
static void uif(void)
{
    logop(OP_UI, 0, 0);
    ui_frame();
    after();
}
static void aud(uint32_t nb)
{
    uint8_t b = (uint8_t)nb;
    logop(OP_AUDIO, &b, 1);
    audio_blocks(nb);
    after();
}
/* the transport (upstream: the LIVE command; ORBIT's editor has none): 1 start, 2 stop */
static void transport(uint32_t req)
{
    uint8_t b = (uint8_t)req;
    logop(OP_TRANSPORT, &b, 1);
    transport_req = (uint8_t)req;
    audio_blocks(1);
    after();
}

/* random panel use, as the UI fuzz of tests/ui_pages_test.c */
static uint32_t held;
static void ui_rand(void)
{
    if (rn(6) == 0) {
        uint32_t bt = rn(NB);
        edges_btn |= 1u << panel.btn[bt];
        if (bt != B_HOME && rn(3) == 0) held ^= 1u << panel.btn[bt];
    }
    if (rn(30) == 0) held = 0;
    fm1_in.buttons = held & ~(1u << panel.btn[B_HOME]);
    if (rn(4) == 0) encs[rn(7)] += (int32_t)rn(5) - 2;
    fm1_in.notes = rn(10) == 0 ? (1u << rn(27)) : (rn(3) ? fm1_in.notes : 0);
    uif();
    if (ui.menu && rn(4)) { ui.menu = 0; ui.force = 1; }
}

/* ---- builders */
static uint32_t pack7(uint8_t *o, const uint8_t *p, uint32_t n)   /* editor.c ed_pack7 */
{
    uint32_t w = 0;
    while (n) {
        uint32_t k = n > 7u ? 7u : n, m = 0, i;
        for (i = 0; i < k; i++) m |= (uint32_t)(p[i] >> 7) << i;
        o[w++] = (uint8_t)m;
        for (i = 0; i < k; i++) o[w++] = p[i] & 127u;
        p += k; n -= k;
    }
    return w;
}
static uint32_t put5(uint8_t *o, uint32_t v) { uint32_t i; for (i = 0; i < 5; i++) o[i] = (v >> (7 * i)) & 127u; return 5; }
static uint32_t v14(uint8_t *o, int32_t v) { uint32_t u = (uint32_t)(v + 8192); o[0] = u & 127u; o[1] = (u >> 7) & 127u; return 2; }
static uint32_t v21(uint8_t *o, int32_t v)            /* ED_WIDE: three bytes, offset 32768 (the top byte 0..3) */
{
    uint32_t u = (uint32_t)(v + 32768);
    o[0] = u & 127u; o[1] = (u >> 7) & 127u; o[2] = (u >> 14) & 3u;
    return 3;
}
static int32_t rval(void)
{
    switch (rn(6)) {
    case 0: return -8192;
    case 1: return 8191;
    case 2: return (int32_t)rn(16384) - 8192;
    case 3: return (int32_t)rn(300) - 150;
    default: return (int32_t)rn(128);
    }
}
static int32_t rvalw(void)                             /* a wide value: the signed16 range and its edges */
{
    switch (rn(6)) {
    case 0: return -32768;
    case 1: return 32767;
    case 2: return (int32_t)rn(65536) - 32768;
    default: return rval();
    }
}
static void rbytes(uint8_t *b, uint32_t n, int bit8) { uint32_t i; for (i = 0; i < n; i++) b[i] = (uint8_t)(bit8 ? rn(256) : rn(128)); }
static void mutate(uint8_t *b, uint32_t n)
{
    uint32_t k = rn(4), i;
    for (i = 0; i < k && n; i++) {
        uint32_t at = rn(n);
        switch (rn(4)) {
        case 0: b[at] = (uint8_t)rn(256); break;
        case 1: b[at] ^= (uint8_t)(1u << rn(8)); break;
        case 2: b[at] = 0xFF; break;
        default: b[at] = 0; break;
        }
    }
}

/* a backup object restored: begin, data (pack7, 1..256 a frame), commit; the CRC right (the content is checked as a
 * load). A FUN6 project (SLOOP's engine namespace) must never commit */
static void bk_restore(uint32_t id, const uint8_t *raw, uint32_t len)
{
    uint8_t a[700];
    uint32_t n = 0, pos = 0, crc = st_crc32(raw, len), magic = 0, op;
    if (len >= 4) memcpy(&magic, raw, 4);
    a[n++] = 0; a[n++] = (uint8_t)id; n += put5(a + n, len); n += put5(a + n, crc);
    cmdv(ED_BK_PUT, a, n);
    while (pos < len) {
        uint32_t k = 1 + rn(256);
        if (k > len - pos) k = len - pos;
        n = 0;
        a[n++] = 1; a[n++] = (uint8_t)id; n += put5(a + n, pos);
        n += pack7(a + n, raw + pos, k);
        cmdv(ED_BK_PUT, a, n);
        pos += k;
        if (rn(200) == 0) break;                  /* (cut off) */
    }
    op = rn(30) ? 2 : 3;
    a[0] = (uint8_t)op; a[1] = (uint8_t)id;
    cmdv(ED_BK_PUT, a, 2);
    if (op == 2 && id <= 5 && magic == PROJ_MAGIC_FUN6 && rx_n >= 8 && rx_last[5] == 2 && rx_last[7] == 0)
        fail("a FUN6 project (another engine namespace) was restored");
}

/* random project content, the sum right: ORP8 (written), or ORP7 (read; its reserved FILTER byte 0) */
static void proj_random(project_t *p)
{
    uint32_t i, v7 = rn(4) == 0;
    if (rn(2)) {
        proj_capture(p);
        mutate((uint8_t *)p + 8, sizeof *p - 12);
        if (rn(2)) {
            uint32_t k = 4 + rn(60);
            for (i = 0; i < k; i++) ((uint8_t *)p)[8 + rn(sizeof *p - 12)] = (uint8_t)rn(256);
        }
    } else {
        rbytes((uint8_t *)p, sizeof *p, 1);
    }
    if (rn(3)) {                                  /* mostly engines and user tags it would take, to get past proj_ok */
        for (i = 0; i < NTRK; i++) {
            p->t[i].engine = (uint8_t)rn(i < NPART ? NENGINES : 2);
            p->t[i].user = (uint8_t)rn(UP_SLOTS + 1);
            if (v7) p->t[i].flt = 0;
        }
    }
    p->magic = v7 ? PROJ_MAGIC_V7 : PROJ_MAGIC;
    p->size = sizeof *p;
    p->sum = proj_sum(p);
}
/* an older format (FUN5..FUN1, imported), or a FUN6 one (refused), its hash right */
static void projv_random(uint8_t *b, uint32_t *len)
{
    static const uint32_t SZ[6] = {sizeof(project_v5_t), sizeof(project_v4_t), sizeof(project_v3_t), sizeof(project_v2_t),
                                   sizeof(project_v1_t), sizeof(project_t)};
    static const uint32_t MG[6] = {PROJ_MAGIC_V5, PROJ_MAGIC_V4, PROJ_MAGIC_V3, PROJ_MAGIC_V2, PROJ_MAGIC_V1, PROJ_MAGIC_FUN6};
    uint32_t v = rn(6), n = SZ[v];
    rbytes(b, n, 1);
    if (rn(2)) {                                  /* mostly plausible values */
        uint32_t i;
        for (i = 8; i < n - 4; i += 2) { b[i] = (uint8_t)rn(130); b[i + 1] = rn(8) ? 0 : (uint8_t)rn(256); }
    }
    ((uint32_t *)b)[0] = MG[v];
    ((uint32_t *)b)[1] = n;
    *(uint32_t *)(b + n - 4) = proj_hash(b, n - 4);
    *len = n;
}

static void smp_upload(uint32_t k)
{
    uint8_t a[700], hdr[512];
    smp_user_hdr_t *h = (smp_user_hdr_t *)hdr;
    uint32_t n, data_len = 256u * (1 + rn(40)), off, i;
    if (rn(8)) { a[0] = (uint8_t)k; cmdv(ED_SMP_BEGIN, a, 1); }
    for (off = 0; off < data_len; off += 256) {
        uint8_t d[256];
        uint32_t o = SMP_USER_DATA + off;
        if (rn(40) == 0) continue;               /* (a lost write) */
        rbytes(d, 256, 1);
        n = 0;
        a[n++] = (uint8_t)k; a[n++] = o & 127u; a[n++] = (o >> 7) & 127u; a[n++] = (o >> 14) & 127u;
        n += pack7(a + n, d, 256);
        cmdv(ED_SMP_WRITE, a, n);
        if (rn(30) == 0) aud(1 + rn(4));
    }
    memset(hdr, 0, sizeof hdr);
    h->magic = SMP_USER_MAGIC; h->version = 1; h->nz = (uint8_t)(1 + rn(16));
    memcpy(h->name, "FUZZ", 4);
    h->data_len = data_len;
    for (i = 0; i < h->nz; i++) {
        smp_zone_t *z = &h->zone[i];
        uint32_t zo = rn(data_len), maxn = 2u * (data_len - zo);
        z->off = zo; z->n = 1 + rn(maxn); z->le = rn(z->n); z->ls = rn(z->le + 1);
        z->rate = 1 + rn(4u << 16); z->root16 = (int16_t)(rn(128) * 16); z->pred = (int16_t)rval(); z->idx = (uint8_t)rn(89);
        z->lo = (uint8_t)rn(128); z->hi = (uint8_t)(z->lo + rn(128 - z->lo)); z->looped = (uint8_t)rn(3);
    }
    h->crc = st_crc32(nor + SMP_USER_OFF(k) + SMP_USER_DATA, data_len);
    if (rn(6) == 0) mutate(hdr, sizeof(smp_user_hdr_t));
    /* (a root outside the MIDI notes: drums.c drum_on, tests/editor_repro.c 1; kept to the notes here so that the
     * random fuzz goes on past it, as upstream) */
    for (i = 0; i < 16; i++) if (h->zone[i].root16 < 0 || h->zone[i].root16 > 2032) h->zone[i].root16 = 960;
    n = 0;
    a[n++] = (uint8_t)k;
    n += pack7(a + n, hdr, sizeof(smp_user_hdr_t));
    cmdv(ED_SMP_END, a, n);
}

/* UP_PUT (legacy, two-byte values), UP_PUT in ED_WIDE, or UP_PUT_MAP (ED_WIDE only, then four macro ids) */
static void up_put_rand(void)
{
    uint8_t a[700];
    uint32_t n = 0, i, nl = 1 + rn(12), kind = rn(3), wide = kind != 0;
    a[n++] = (uint8_t)rn(UP_SLOTS + 4); a[n++] = (uint8_t)rn(NENGINES + 2);
    for (i = 0; i < nl; i++) a[n++] = (uint8_t)(32 + rn(95));
    a[n++] = 0;
    for (i = 0; i < P_COUNT; i++) n += wide ? v21(a + n, rvalw()) : v14(a + n, rval());
    for (i = 0; i < 32; i++) a[n++] = (uint8_t)rn(128);
    if (kind == 2)
        for (i = 0; i < MACRO_COUNT; i++) a[n++] = (uint8_t)(rn(3) ? rn(P_COUNT) : rn(2) ? MACRO_DEFAULT : rn(128));
    if (rn(5) == 0) n = rn(n + 3);
    if (wide) cmdw(kind == 2 ? ED_UP_PUT_MAP : ED_UP_PUT, a, n);
    else cmdv(ED_UP_PUT, a, n);
}
static void macro_rand(void)
{
    uint8_t a[8];
    uint32_t i, n = rn(4) ? 1 + MACRO_COUNT : rn(7);
    a[0] = (uint8_t)rn(NTRK + 1);
    for (i = 1; i < sizeof a; i++) a[i] = (uint8_t)(rn(3) ? rn(P_COUNT) : rn(2) ? MACRO_DEFAULT : rn(128));
    cmdv(ED_MACRO, a, n);
}

/* the commands that take ED_WIDE (editor.c ed_handle) */
static const uint8_t WIDE_OK[] = {ED_GET, ED_SET, ED_DUMP, ED_DESC, ED_UP_GET, ED_UP_PUT, ED_TRACK, ED_TRACK_MIX,
    ED_TRACK_DUMP, ED_TRACK_PARAM, ED_LOCK_GET, ED_LOCK_SET, ED_WATCH, ED_UP_GET_MAP, ED_UP_PUT_MAP, ED_MACRO_DESC};
static void cmd_rand(int bit8)
{
    static const uint8_t CMDS[] = {ED_INFO, ED_GET, ED_SET, ED_DUMP, ED_DESC, ED_STEP_GET, ED_STEP_SET, ED_PRESET, ED_PROJECT,
        ED_NAMES, ED_SMP_BEGIN, ED_SMP_WRITE, ED_SMP_END, ED_SMP_ERASE, ED_SMP_INFO, ED_UP_LIST, ED_UP_GET, ED_UP_PUT,
        ED_UP_STORE, ED_UP_LOAD, ED_UP_ERASE, ED_WATCH, ED_CHANGED, ED_RELOAD, ED_PING, ED_STEP_CHANGED, ED_TRACK,
        ED_TRACK_MIX, ED_TRACK_DUMP, ED_TRACK_STEP, ED_TRACK_PARAM, ED_TRACK_CHANGED, ED_DRUM_STEP, ED_BK_LIST, ED_BK_GET,
        ED_BK_PUT, ED_LOCK_GET, ED_LOCK_SET, ED_MICRO_GET, ED_MICRO_SET, ED_FILL_GET, ED_FILL_SET, ED_FM6_GET, ED_FM6_PUT,
        ED_FM6_LIST, ED_FM6_ERASE, ED_DSYN_LIST, ED_DSYN_GET, ED_DSYN_PUT, ED_DSYN_STORE, ED_DSYN_PLAY, ED_WIDE, ED_MACRO,
        ED_UP_GET_MAP, ED_UP_PUT_MAP, ED_MACRO_DESC};
    static const uint32_t EXP[128] = {[ED_GET] = 2, [ED_SET] = 4, [ED_DESC] = 2, [ED_STEP_GET] = 1, [ED_STEP_SET] = 12,
        [ED_PRESET] = 2, [ED_PROJECT] = 2, [ED_NAMES] = 1, [ED_SMP_BEGIN] = 1, [ED_SMP_WRITE] = 300, [ED_SMP_END] = 550,
        [ED_SMP_ERASE] = 1, [ED_UP_LIST] = 2, [ED_UP_GET] = 1, [ED_UP_PUT] = 3 + 13 + 2 * P_COUNT + 32, [ED_UP_STORE] = 6,
        [ED_UP_LOAD] = 1, [ED_UP_ERASE] = 1, [ED_WATCH] = 1, [ED_TRACK] = 1, [ED_TRACK_MIX] = 4, [ED_TRACK_DUMP] = 1,
        [ED_TRACK_STEP] = 13, [ED_TRACK_PARAM] = 4, [ED_DRUM_STEP] = 14, [ED_BK_GET] = 8, [ED_BK_PUT] = 12,
        [ED_LOCK_GET] = 1, [ED_LOCK_SET] = 5, [ED_MICRO_GET] = 1, [ED_MICRO_SET] = 3, [ED_FILL_GET] = 1, [ED_FILL_SET] = 3,
        [ED_FM6_GET] = 2, [ED_FM6_PUT] = 130, [ED_FM6_ERASE] = 1, [ED_DSYN_GET] = 1, [ED_DSYN_PUT] = 28,
        [ED_DSYN_PLAY] = 3, [ED_MACRO] = 1 + MACRO_COUNT, [ED_UP_GET_MAP] = 1,
        [ED_UP_PUT_MAP] = 3 + 13 + 3 * P_COUNT + 32 + MACRO_COUNT, [ED_MACRO_DESC] = 2};
    uint8_t f[700];
    uint32_t cmd = rn(10) == 0 ? rn(bit8 ? 256 : 128) : CMDS[rn(sizeof CMDS)], e = cmd < 128 ? EXP[cmd] : 0, na, i, h = 4;
    int wide = rn(3) == 0;
    if (wide) {                                   /* an ED_WIDE frame: mostly a command that takes it */
        cmd = rn(6) ? WIDE_OK[rn(sizeof WIDE_OK)] : rn(bit8 ? 256 : 128);
        e = cmd < 128 ? EXP[cmd] : 0;
        if (cmd == ED_SET || cmd == ED_TRACK_PARAM || cmd == ED_LOCK_SET || cmd == ED_TRACK_MIX) e++;
        if (cmd == ED_UP_PUT) e += P_COUNT;
    }
    switch (rn(8)) {
    case 0: na = 0; break;
    case 1: na = 1; break;
    case 2: na = e ? e - 1 : 2; break;
    case 3: na = e + 1; break;
    case 4: na = rn(640 - 5 + 1); break;
    case 5: na = rn(20); break;
    default: na = e; break;
    }
    f[0] = ED_HDR0; f[1] = ED_HDR1; f[2] = ED_HDR2;
    if (wide) { f[3] = ED_WIDE; f[4] = (uint8_t)cmd; h = 5; }
    else f[3] = (uint8_t)cmd;
    if (na > 640 - h) na = 640 - h;
    for (i = 0; i < na; i++) {
        uint32_t r = rn(10);
        f[h + i] = (uint8_t)(r < 3 ? rn(8) : r < 5 ? 127 - rn(4) : r < 6 && bit8 ? 128 + rn(128) : rn(bit8 && rn(8) == 0 ? 256 : 128));
    }
    if ((cmd == ED_SET || cmd == ED_GET || cmd == ED_DESC) && na >= 2 && rn(2)) { f[h] = (uint8_t)rn(2); f[h + 1] = (uint8_t)rn(f[h] ? G_COUNT + 2 : P_COUNT + 2); }
    if (cmd == ED_SET && na >= 2u + (wide ? 3u : 2u) && rn(2)) { if (wide) v21(f + h + 2, rvalw()); else v14(f + h + 2, rval()); }
    if ((cmd == ED_TRACK_PARAM || cmd == ED_LOCK_SET || cmd == ED_MACRO_DESC) && na >= 3 && rn(2)) { f[h] = (uint8_t)rn(NTRK); f[h + 1] = (uint8_t)rn(cmd == ED_LOCK_SET ? NSTEP : P_COUNT); }
    if (cmd == ED_LOCK_SET && na >= 3u + (wide ? 3u : 2u) && rn(2)) { f[h + 2] = (uint8_t)rn(P_COUNT); if (wide) v21(f + h + 3, rvalw()); else v14(f + h + 3, rval()); }
    if (cmd == ED_TRACK_PARAM && na >= 2u + (wide ? 3u : 2u) && rn(2)) { if (wide) v21(f + h + 2, rvalw()); else v14(f + h + 2, rval()); }
    if (bit8)
        frame(f, h + na, 0);
    else
        frame(f, h + na, rn(3) == 0);
}

static void play_slots(void);
static void fz_seq_step(void)
{
    static uint8_t raw[4096];
    uint8_t a[700];
    uint32_t n, len, r = rn(100);
    if (r < 15) { cmd_rand(0); return; }
    if (r < 30) { ui_rand(); return; }
    if (r < 35) { aud(1 + rn(30)); return; }
    if (r < 38) { smp_upload(rn(SMP_USER_SLOTS)); return; }
    if (r < 40) {                                 /* out of order sample commands */
        uint32_t k = rn(SMP_USER_SLOTS + 1), c = ED_SMP_BEGIN + rn(5);
        n = 0; a[n++] = (uint8_t)k;
        if (c == ED_SMP_WRITE) { uint32_t o = rn(2) ? SMP_USER_DATA + 256 * rn(300) : rn(1u << 21); uint8_t d[256]; a[n++] = o & 127; a[n++] = (o >> 7) & 127; a[n++] = (o >> 14) & 127; rbytes(d, 256, 1); n += pack7(a + n, d, 1 + rn(256)); }
        if (c == ED_SMP_END) { uint8_t d[480]; rbytes(d, 480, 1); n += pack7(a + n, d, rn(2) ? 480 : rn(480)); }
        cmdv(c, a, n);
        return;
    }
    if (r < 45) { cmdv(ED_BK_LIST, a, 0); return; }
    if (r < 48) {                                 /* GET of a random object */
        uint32_t id = rn(2) ? rn(12) : 32 + rn(5), off = rn(2) ? 0 : rn(4096), cnt = 1 + rn(256);
        n = 0; a[n++] = (uint8_t)id; n += put5(a + n, off); a[n++] = cnt & 127; a[n++] = (uint8_t)(cnt >> 7);
        cmdv(ED_BK_GET, a, n);
        return;
    }
    if (r < 58) {                                 /* restore an object */
        uint32_t id = rn(12);
        switch (id) {
        case 0: case 2: case 3: case 4: case 5:
            if (rn(3)) { proj_random((project_t *)raw); len = sizeof(project_t); }
            else projv_random(raw, &len);
            if (id && rn(10) == 0) len = 0;
            break;
        case 1: {                                 /* the settings: persist_t (this layout or the ones before) */
            static const uint32_t L[] = {sizeof(persist_t), PERSIST_SIZE_V23, PERSIST_SIZE_V22};
            persist_fill((persist_t *)raw);
            len = L[rn(NELEM(L))];
            if (rn(2)) mutate(raw + 4, len - 4);
            break;
        }
        case 6: case 7: {
            up_bank_t *b = (up_bank_t *)raw;
            uint32_t i;
            rbytes(raw, sizeof *b, 1);
            b->magic = UP_BANK_MAGIC; b->rsize = sizeof(up_rec_t); b->nslot = UP_PER_BANK;
            for (i = 0; i < UP_PER_BANK; i++)
                if (rn(2)) { b->r[i].used = UP_USED; b->r[i].ver = UP_VER; b->r[i].engine = (uint8_t)rn(NENGINES); b->r[i].np = (uint8_t)(8 + rn(UP_PMAX - 7)); b->r[i].name[0] = 'A'; }
            len = rn(10) ? sizeof *b : 0;
            break;
        }
        case 8: {
            fm6_bank_t *b = (fm6_bank_t *)raw;
            rbytes(raw, sizeof *b, 0);
            b->magic = FM6_BANK_MAGIC; b->ver = 1; b->nslot = FM6_BANK_N; b->used = rn(1u << FM6_BANK_N); b->rsv = 0;
            len = rn(10) ? sizeof *b : 0;
            break;
        }
        case 9: {
            dsu_bank_t *b = (dsu_bank_t *)raw;
            rbytes(raw, sizeof *b, 1);
            b->magic = DSU_MAGIC; b->ver = 1; b->n = DSU_N;
            len = sizeof *b;
            break;
        }
        case 10: rbytes(raw, MIDI_PART, 1); len = rn(2) ? MIDI_PART : rn(2) ? 2 * ML_N : 2 * ML_N + 4; break;
        default: len = 0;
        }
        bk_restore(id, raw, len);
        return;
    }
    if (r < 62) { up_put_rand(); return; }
    if (r < 64) { n = 0; a[n++] = (uint8_t)rn(UP_SLOTS + 2); a[n++] = 'X'; a[n++] = 0; cmdv(ED_UP_STORE, a, rn(3)); return; }
    if (r < 67) { a[0] = (uint8_t)rn(UP_SLOTS + 2); cmdv(rn(3) ? ED_UP_LOAD : ED_UP_ERASE, a, 1); return; }
    if (r < 71) {                                 /* an FM6 patch */
        n = 0; a[n++] = (uint8_t)rn(3); a[n++] = (uint8_t)rn(30);
        rbytes(a + n, 128, 0); n += 128;
        cmdv(ED_FM6_PUT, a, n);
        return;
    }
    if (r < 75) {                                 /* a SYN kit edit */
        uint8_t d[400];
        uint32_t part = rn(DS_LANES + 2), m = part < DS_LANES ? sizeof(dsnd_t) : part == DS_LANES ? 10 : 1;
        n = 0; a[n++] = (uint8_t)rn(DSU_N + 1); a[n++] = (uint8_t)part;
        rbytes(d, m, 1);
        if (part == DS_LANES + 1) d[0] = (uint8_t)rn(DS_NKITS + 1);
        n += pack7(a + n, d, m);
        cmdv(ED_DSYN_PUT, a, n);
        if (rn(3) == 0) { a[0] = (uint8_t)rn(DSU_N); a[1] = (uint8_t)rn(DS_LANES); a[2] = (uint8_t)rn(128); cmdv(ED_DSYN_PLAY, a, 3); }
        return;
    }
    if (r < 78) { macro_rand(); return; }
    if (r < 80) { a[0] = (uint8_t)rn(NTRK); a[1] = (uint8_t)rn(P_COUNT + 2); cmdv(ED_MACRO_DESC, a, 2); return; }
    if (r < 82) { transport(1 + rn(2)); return; }
    if (r < 85) { a[0] = (uint8_t)rn(NENGINES + 1); a[1] = (uint8_t)rn(64); cmdv(ED_PRESET, a, 2); return; }
    if (r < 87) { a[0] = (uint8_t)rn(3); a[1] = (uint8_t)rn(4); cmdv(ED_PROJECT, a, 2); return; }
    if (r < 89) { a[0] = (uint8_t)rn(NTRK); cmdv(ED_TRACK, a, 1); return; }
    if (r < 92) { a[0] = 1; a[1] = G_ENGSEL; v14(a + 2, (int32_t)rn(NENGINES + 2)); cmdv(ED_SET, a, 4); return; }
    if (r < 95) {
        a[0] = (uint8_t)rn(NTRK); a[1] = (uint8_t)rn(NSTEP); a[2] = (uint8_t)rn(P_COUNT);
        if (rn(2)) { v21(a + 3, rvalw()); cmdw(ED_LOCK_SET, a, 6); } else { v14(a + 3, rval()); cmdv(ED_LOCK_SET, a, 5); }
        return;
    }
    if (r < 97) { a[0] = (uint8_t)(rn(2)); cmdv(ED_WATCH, a, 1); a[0] = 3; cmdv(ED_WATCH, a, 1); return; }
    if (r < 99) { play_slots(); return; }
    { uint32_t i; for (i = 0; i < 60; i++) ui_rand(); }
}


static uint32_t eng_by_name(const char *n) { uint32_t e; for (e = 0; e < NENGINES; e++) if (!strcmp(ENGINES[e]->name, n)) return e; return 0; }
/* the user sample slots played: SAMPLE / GRAIN on a part, the drum kit USRn, the TEXTURE bed, notes and the transport */
static void play_slots(void)
{
    uint8_t a[16];
    uint32_t t = rn(NPART), e = rn(2) ? eng_by_name("SAMPLE") : eng_by_name("GRAIN"), i;
    a[0] = (uint8_t)t; cmdv(ED_TRACK, a, 1);
    a[0] = (uint8_t)e; a[1] = (uint8_t)rn(8); cmdv(ED_PRESET, a, 2);
    a[0] = (uint8_t)t; a[1] = P_E0; v14(a + 2, (int32_t)(SMP_NSETS + rn(SMP_USER_SLOTS))); cmdv(ED_TRACK_PARAM, a, 4);
    a[0] = TRK_DRUM; a[1] = P_E0; v14(a + 2, (int32_t)(DRUM_USR + rn(5))); cmdv(ED_TRACK_PARAM, a, 4);
    a[0] = 1; a[1] = G_TXSRC; v14(a + 2, (int32_t)(7 + rn(4))); cmdv(ED_SET, a, 4);
    a[0] = 1; a[1] = G_TXWHEN; v14(a + 2, 1); cmdv(ED_SET, a, 4);
    if (rn(2)) transport(1);
    for (i = 0; i < 6; i++) {
        uint8_t b[4] = {9, (uint8_t)(0x90 | (rn(2) ? t : 9)), (uint8_t)rn(128), (uint8_t)(1 + rn(127))};
        logop(OP_MIDI, b, 4);
        ep1_take(b, 4);
        aud(2 + rn(10));
        if (rn(3) == 0) { b[0] = 8; b[1] = (uint8_t)(0x80 | (b[1] & 15)); logop(OP_MIDI, b, 4); ep1_take(b, 4); }
    }
}
static void uart_rand(void)
{
    uint32_t k = 1 + rn(24), i;
    uint8_t b[24];
    for (i = 0; i < k; i++) {
        uint32_t r = rn(10);
        b[i] = (uint8_t)(r < 2 ? 0x80 + rn(0x80) : r < 3 ? 0xF8 + rn(8) : r < 4 ? 0xF0 + rn(8) : rn(128));
        um_byte(b[i]);
    }
    logop(OP_UART, b, k);
}


/* ---- MODE 3: a boot from a damaged / foreign flash (and .noinit RAM): every object a valid storage record (its CRC
 * right) with random or mutated content of the lengths the loaders look for, damaged copies, random sample headers;
 * then the boot (fm1_main's persist_boot, settings_init, panel_init, felucca_init) and use */
static void st_put3(uint32_t obj, const void *p, uint32_t n, const void *p2, uint32_t n2, const void *p3, uint32_t n3)
{
    if (rn(12) == 0) {                            /* a torn write: the header or the payload damaged */
        fl_fail_at = (int)(1 + rn(8));
        (void)st_save3(obj, p, n, p2, n2, p3, n3);
        fl_fail_at = -1;
        return;
    }
    (void)st_save3(obj, p, n, p2, n2, p3, n3);
    if (rn(3) == 0) (void)st_save3(obj, p, n, p2, n2, p3, n3);   /* both copies */
}
static void st_put(uint32_t obj, const void *p, uint32_t n) { st_put3(obj, p, n, 0, 0, 0, 0); }
static void slot_random(uint32_t k)
{
    uint8_t *img = nor + SMP_USER_OFF(k);
    smp_user_hdr_t *h = (smp_user_hdr_t *)img;
    uint32_t i, data_len = rn(4) ? rn(SMP_USER_SIZE - SMP_USER_DATA + 1) : rn(4096);
    if (rn(3) == 0) return;
    memset(img, 0xFF, SMP_USER_SIZE);
    for (i = 0; i < data_len; i += 64) img[SMP_USER_DATA + i] = (uint8_t)rn(256);
    rbytes(img, sizeof *h, 1);
    h->magic = SMP_USER_MAGIC; h->version = 1; h->nz = (uint8_t)(1 + rn(16)); h->data_len = data_len;
    for (i = 0; i < h->nz; i++) {
        smp_zone_t *z = &h->zone[i];
        if (rn(4) == 0) continue;                 /* (random: refused) */
        z->off = data_len ? rn(data_len) : 0; z->n = data_len ? 1 + rn(2u * (data_len - z->off)) : 0;
        z->le = z->n ? rn(z->n) : 0; z->ls = rn(z->le + 1); z->rate = 1 + rn(4u << 16); z->idx = (uint8_t)rn(89);
        z->lo = (uint8_t)rn(128); z->hi = (uint8_t)(z->lo + rn(128 - z->lo));
        z->root16 = (int16_t)(rn(128) * 16);
    }
    if (rn(5) == 0) mutate(img, sizeof *h);
    for (i = 0; i < 16; i++) if (h->zone[i].root16 < 0 || h->zone[i].root16 > 2032) h->zone[i].root16 = 960;   /* (see smp_upload) */
}
static void flash_random(void)
{
    static uint8_t raw[4096];
    uint32_t i, len;
    memset(nor, 0xFF, sizeof nor);
    flash_ok = 1;
    if (rn(2)) {                                   /* the settings object: persist_t, the SYN kits, the MIDI part */
        static const uint32_t PL[] = {sizeof(persist_t), PERSIST_SIZE_V23, PERSIST_SIZE_V22, 16u + sizeof(panel_t), 8u + sizeof(panel_t)};
        static const uint32_t ML[] = {0, 2 * ML_N, 2 * ML_N + 4, MIDI_PART};
        persist_t *ps = (persist_t *)raw;
        dsu_bank_t db;
        uint8_t mp[MIDI_PART];
        uint32_t pl = PL[rn(NELEM(PL))], ml = ML[rn(NELEM(ML))], with_dsu = rn(4) != 0;
        persist_fill(ps);
        if (rn(3) == 0) mutate(raw, sizeof *ps);
        if (rn(4) == 0) rbytes(raw + 4, sizeof *ps - 4, 1);
        if (rn(2)) ps->arrangement.count = (uint8_t)rn(20);
        if (pl == 16u + sizeof(panel_t)) ps->magic = 0x50455232u;
        if (pl == 8u + sizeof(panel_t)) ps->magic = 0x50455231u;
        rbytes((uint8_t *)&db, sizeof db, 1);
        db.magic = DSU_MAGIC; db.ver = 1; db.n = DSU_N;
        rbytes(mp, sizeof mp, 1);
        if (!with_dsu) ml = 0;
        st_put3(OBJ_SETTINGS, raw, pl, with_dsu ? &db : 0, with_dsu ? sizeof db : 0, ml ? mp : 0, ml);
    }
    for (i = 0; i < 5; i++) {                      /* the projects and the autosave */
        uint32_t obj = i < 4 ? OBJ_PROJECT0 + i : OBJ_AUTOSAVE;
        if (rn(3) == 0) continue;
        if (rn(3)) { proj_random((project_t *)raw); len = sizeof(project_t); }
        else projv_random(raw, &len);
        if (rn(10) == 0) len = rn(len + 1);
        st_put(obj, raw, len);
    }
    for (i = 0; i < 2; i++) {                      /* the user preset banks */
        up_bank_t *b = (up_bank_t *)raw;
        uint32_t j;
        if (rn(3) == 0) continue;
        rbytes(raw, sizeof *b, 1);
        b->magic = UP_BANK_MAGIC; b->rsize = sizeof(up_rec_t); b->nslot = UP_PER_BANK;
        for (j = 0; j < UP_PER_BANK; j++)
            if (rn(2)) { b->r[j].used = UP_USED; b->r[j].ver = UP_VER; b->r[j].engine = (uint8_t)rn(NENGINES); b->r[j].np = (uint8_t)(8 + rn(UP_PMAX - 7)); b->r[j].name[0] = (char)(32 + rn(95)); }
        st_put(OBJ_UPRESET0 + i, raw, rn(10) ? sizeof *b : rn(sizeof *b));
    }
    if (rn(2)) {                                   /* the FM6 bank */
        fm6_bank_t *b = (fm6_bank_t *)raw;
        rbytes(raw, sizeof *b, 0);
        b->magic = FM6_BANK_MAGIC; b->ver = 1; b->nslot = FM6_BANK_N; b->used = rn(1u << FM6_BANK_N); b->rsv = 0;
        st_put(OBJ_FM6BANK, raw, sizeof *b);
    }
    for (i = 0; i < SMP_USER_SLOTS; i++) slot_random(i);
    for (i = 0; i < 4; i++) {                      /* .noinit: garbage, or a valid project (a warm reset) */
        if (rn(2)) rbytes((uint8_t *)&proj_slot[i], sizeof proj_slot[i], 1);
        else proj_random(&proj_slot[i]);
    }
    if (rn(30) == 0) {                             /* random bytes over a few sectors */
        uint32_t k = rn(8);
        for (i = 0; i < k; i++) nor[FL_DATA_LO + rn(0x100000 - FL_DATA_LO)] = (uint8_t)rn(256);
    }
}
static void boot(void)
{
    logop(OP_BOOT, 0, 0);
    flash_random();
    memset(&settings, 0, sizeof settings);
    if (rn(2)) rbytes((uint8_t *)&settings, sizeof settings, 1);
    boot_main();
    after();
}


/* ---- MODE 4: every command over every engine / track / id / step at the edges, deterministic; a reply that fills
 * ed_out (cut short) is reported */
static uint32_t cut_replies;
static void xcmd2(uint32_t wide, uint32_t cmd, const uint8_t *a, uint32_t na)
{
    uint8_t f[700];
    uint32_t h = wide ? 5u : 4u;
    f[0] = ED_HDR0; f[1] = ED_HDR1; f[2] = ED_HDR2; f[3] = (uint8_t)(wide ? ED_WIDE : cmd); f[4] = (uint8_t)cmd;
    memcpy(f + h, a, na);
    logop(OP_INJECT, f, h + na);
    ed_inject(f, h + na);
    if (ed_n >= sizeof ed_out - 1u) {
        cut_replies++;
        printf("reply cut short: %scmd %u args", wide ? "WIDE " : "", cmd);
        { uint32_t i; for (i = 0; i < na && i < 8; i++) printf(" %u", a[i]); }
        printf(" (%u bytes)\n", ed_n);
    }
    after();
}
static void xcmd(uint32_t cmd, const uint8_t *a, uint32_t na) { xcmd2(0, cmd, a, na); }
static void exhaustive(void)
{
    static const int32_t V[] = {-8192, -1, 0, 1, 63, 64, 127, 128, 8191};
    static const int32_t VW[] = {-32768, -8193, -1, 0, 1, 127, 128, 8192, 32767};
    uint8_t a[700];
    uint32_t e, p, id, t, st, v, sc;
    for (e = 0; e <= NENGINES; e++) {
        a[0] = (uint8_t)e; xcmd(ED_NAMES, a, 1);
        for (p = 0; e < NENGINES && p < ENGINES[e]->npresets + 2u; p++) {
            a[0] = 0; xcmd(ED_TRACK, a, 1);
            a[0] = (uint8_t)e; a[1] = (uint8_t)p; xcmd(ED_PRESET, a, 2);
            aud(4);
        }
        for (id = 0; id < P_COUNT + 2u; id++) {
            a[0] = 0; a[1] = (uint8_t)id; xcmd(ED_DESC, a, 2); xcmd(ED_GET, a, 2); xcmd2(1, ED_GET, a, 2);
            for (v = 0; v < NELEM(V); v++) { a[0] = 0; a[1] = (uint8_t)id; v14(a + 2, V[v]); xcmd(ED_SET, a, 4); }
            for (v = 0; v < NELEM(VW); v++) { a[0] = 0; a[1] = (uint8_t)id; v21(a + 2, VW[v]); xcmd2(1, ED_SET, a, 5); }
            for (t = 0; t < NTRK; t++) {
                for (v = 0; v < NELEM(V); v++) { a[0] = (uint8_t)t; a[1] = (uint8_t)id; v14(a + 2, V[v]); xcmd(ED_TRACK_PARAM, a, 4); }
                for (v = 0; v < NELEM(VW); v++) { a[0] = (uint8_t)t; a[1] = (uint8_t)id; v21(a + 2, VW[v]); xcmd2(1, ED_TRACK_PARAM, a, 5); }
                if (t < NPART) { a[0] = (uint8_t)t; a[1] = (uint8_t)id; xcmd(ED_MACRO_DESC, a, 2); }
            }
            aud(2);
            { uint32_t f2; for (f2 = 0; f2 < 3; f2++) ui_frame(); }
        }
        if (e < NENGINES) { a[0] = 0; a[1] = 0; xcmd(ED_PRESET, a, 0); }
    }
    for (sc = 0; sc < 2; sc++)
        for (id = 0; id < G_COUNT + 2u; id++) {
            a[0] = (uint8_t)sc; a[1] = (uint8_t)id; xcmd(ED_DESC, a, 2); xcmd2(1, ED_DESC, a, 2);
            if (sc == 1 && (id == G_ENGSEL)) continue;
            for (v = 0; v < NELEM(V); v++) { a[0] = (uint8_t)sc; a[1] = (uint8_t)id; v14(a + 2, V[v]); xcmd(ED_SET, a, 4); aud(1); }
        }
    a[0] = TRK_DRUM; xcmd(ED_TRACK, a, 1);
    for (id = 0; id < P_COUNT; id++) { a[0] = 0; a[1] = (uint8_t)id; xcmd(ED_DESC, a, 2); }
    for (t = 0; t <= NTRK; t++) {
        a[0] = (uint8_t)(t % NTRK); xcmd(ED_TRACK, a, 1);
        for (st = 0; st < NSTEP + 1; st++) {
            uint8_t b[16];
            uint32_t k;
            for (k = 0; k < 13; k++) b[k] = (uint8_t)(k & 1 ? 127 : st);
            b[0] = (uint8_t)t; b[1] = (uint8_t)st; xcmd(ED_TRACK_STEP, b, 13);
            b[0] = (uint8_t)st; xcmd(ED_STEP_SET, b, 12);
            b[0] = (uint8_t)st; for (k = 1; k < 14; k++) b[k] = 127; xcmd(ED_DRUM_STEP, b, 14);
            b[0] = (uint8_t)t; b[1] = (uint8_t)st; b[2] = 127; xcmd(ED_MICRO_SET, b, 3); b[2] = 0; xcmd(ED_MICRO_SET, b, 3);
            b[2] = (uint8_t)st; xcmd(ED_FILL_SET, b, 3);
            for (id = 0; id < P_COUNT + 1; id += 7) {
                b[0] = (uint8_t)t; b[1] = (uint8_t)st; b[2] = (uint8_t)id;
                v14(b + 3, st & 1 ? 8191 : -8192); xcmd(ED_LOCK_SET, b, 5);
                v21(b + 3, st & 1 ? 32767 : -32768); xcmd2(1, ED_LOCK_SET, b, 6);
            }
        }
        a[0] = (uint8_t)t; xcmd(ED_LOCK_GET, a, 1); xcmd2(1, ED_LOCK_GET, a, 1); xcmd(ED_MICRO_GET, a, 1);
        xcmd(ED_FILL_GET, a, 1); xcmd(ED_TRACK_DUMP, a, 1); xcmd2(1, ED_TRACK_DUMP, a, 1);
        a[0] = (uint8_t)t; a[1] = 0; a[2] = 0; a[3] = 1; a[4] = 2; xcmd(ED_MACRO, a, 5);
        a[1] = a[2] = a[3] = a[4] = MACRO_DEFAULT; xcmd(ED_MACRO, a, 5); xcmd(ED_MACRO, a, 1);
        transport(1); aud(200); transport(2); aud(20);
    }
    for (p = 0; p < 3; p++)
        for (id = 0; id < 40; id++) { a[0] = (uint8_t)p; a[1] = (uint8_t)id; xcmd(ED_FM6_GET, a, 2); }
    xcmd(ED_FM6_LIST, a, 0);
    for (id = 0; id < 128; id++) { a[0] = (uint8_t)id; xcmd(ED_DSYN_GET, a, 1); }
    xcmd(ED_DSYN_LIST, a, 0);
    for (id = 0; id < 128; id++) {
        a[0] = (uint8_t)id; a[1] = 2; xcmd(ED_UP_LIST, a, 2);
        a[0] = (uint8_t)id; xcmd(ED_UP_GET, a, 1); xcmd2(1, ED_UP_GET_MAP, a, 1);
    }
    xcmd(ED_BK_LIST, a, 0);
    for (id = 0; id < 128; id++) {
        uint32_t off;
        for (off = 0; off < 0x14200; off += 256) {
            uint32_t n = 0;
            a[n++] = (uint8_t)id; n += put5(a + n, off); a[n++] = 0; a[n++] = 2;
            xcmd(ED_BK_GET, a, n);
            if (a[0] && id != 0 && rx_n > 0 && rx_last[6] != 0) break;   /* (past its end) */
        }
    }
    xcmd(ED_SMP_INFO, a, 0); xcmd(ED_INFO, a, 0); xcmd(ED_DUMP, a, 0); xcmd2(1, ED_DUMP, a, 0); a[0] = 0; xcmd(ED_TRACK, a, 0);
    for (id = 0; id < 128; id++) { a[0] = (uint8_t)id; xcmd2(1, id, a, 1); }   /* ED_WIDE over every inner command */
    printf("exhaustive: %u replies cut short\n", cut_replies);
}


/* ---- MODE 5: MIDI in only (USB packets and TRS bytes), then every note let go on every channel (and the pedal up):
 * after 4 s no voice may still be held */
static void midi_let_go(void)
{
    uint32_t ch, n;
    for (ch = 0; ch < 16; ch++) {
        uint8_t b[4] = {0x0B, (uint8_t)(0xB0 | ch), 64, 0};
        while (!ep1_take(b, 4)) audio_blocks(1);
        for (n = 0; n < 128; n++) {
            uint8_t o[4] = {0x08, (uint8_t)(0x80 | ch), (uint8_t)n, 0};
            while (!ep1_take(o, 4)) audio_blocks(1);
        }
        audio_blocks(2);
    }
    audio_blocks(4u * FS / CTL);
}
static int midi_hang_check(void)
{
    uint32_t k, v, bad = 0;
    for (k = 0; k < NPART; k++)
        for (v = 0; v < NVOICE; v++)
            if (trk[k].v[v].active && trk[k].v[v].gate) {
                fprintf(stderr, "held: track %u voice %u note %u (arp mode %d hold %d, voice mode %d, nheld %u)\n", k, v,
                        trk[k].v[v].note, trk[k].p[P_AMODE], trk[k].p[P_AHOLD], trk[k].p[P_VOICE], trk[k].nheld);
                bad++;
            }
    return bad == 0;
}


/* ---- MODE 6: round trips. After random use (stopped): the working project captured (project.c proj_capture), then
 * (a) the editor's backup of object 0 read (LIST, GET) and restored (PUT, commit), (b) saved to a section and loaded,
 * (c) the autosave written and resumed: each must give back the same project (sound, macros, pattern), byte for byte */
static project_t rt_a, rt_b;
static const char *proj_field(uint32_t off)
{
    static char b[96];
    uint32_t t0 = (uint32_t)offsetof(project_t, t), ts = sizeof(proj_trk_t);
    if (off < t0) { snprintf(b, sizeof b, "header/globals byte %u (g[%u])", off, off >= 8 ? (off - 8) / 2 : 0); return b; }
    if (off >= t0 + NTRK * ts) { snprintf(b, sizeof b, "sum"); return b; }
    {
        uint32_t k = (off - t0) / ts, o = (off - t0) % ts;
        if (o < offsetof(proj_trk_t, e)) snprintf(b, sizeof b, "track %u p[%u]", k, o / 2);
        else if (o < offsetof(proj_trk_t, x)) snprintf(b, sizeof b, "track %u e[%u]", k, (o - (uint32_t)offsetof(proj_trk_t, e)) / 2);
        else if (o < offsetof(proj_trk_t, engine)) snprintf(b, sizeof b, "track %u x[%u] (p[%u])", k, o - (uint32_t)offsetof(proj_trk_t, x), PROJ_NC7 + o - (uint32_t)offsetof(proj_trk_t, x));
        else if (o < offsetof(proj_trk_t, step)) snprintf(b, sizeof b, "track %u engine/preset", k);
        else if (o < offsetof(proj_trk_t, mf)) snprintf(b, sizeof b, "track %u step %u byte %u", k, (o - (uint32_t)offsetof(proj_trk_t, step)) / (uint32_t)sizeof(step_t), (o - (uint32_t)offsetof(proj_trk_t, step)) % (uint32_t)sizeof(step_t));
        else if (o < offsetof(proj_trk_t, lock)) snprintf(b, sizeof b, "track %u mf[%u]", k, o - (uint32_t)offsetof(proj_trk_t, mf));
        else if (o < offsetof(proj_trk_t, macro)) snprintf(b, sizeof b, "track %u lock %u", k, (o - (uint32_t)offsetof(proj_trk_t, lock)) / 4);
        else if (o < offsetof(proj_trk_t, user)) snprintf(b, sizeof b, "track %u macro[%u]", k, o - (uint32_t)offsetof(proj_trk_t, macro));
        else if (o < offsetof(proj_trk_t, flt)) snprintf(b, sizeof b, "track %u user", k);
        else snprintf(b, sizeof b, "track %u flt (RES / TYPE)", k);
    }
    return b;
}
static int rt_cmp(const char *what)
{
    uint32_t i, nd = 0;
    const uint8_t *x = (const uint8_t *)&rt_a, *y = (const uint8_t *)&rt_b;
    static const uint8_t SETTING[] = {G_SLOT, G_LOAD, G_SYNC, G_MIDI, G_ROUTE};   /* (in a project, not loaded: the FM-1's) */
    for (i = 0; i < sizeof SETTING; i++) rt_b.g[SETTING[i]] = rt_a.g[SETTING[i]];
    for (i = 0; i < sizeof rt_a - 4; i++)
        if (x[i] != y[i]) {
            if (nd < 6) fprintf(stderr, "%s: %s: %02X -> %02X\n", what, proj_field(i), x[i], y[i]);
            nd++;
        }
    if (nd) fprintf(stderr, "%s: %u bytes differ\n", what, nd);
    return nd == 0;
}
static void stop_all(void)
{
    transport(2);
    audio_blocks(20);
    arrangement_enabled = 0;
}
static void midi_rand(void);
static void scramble(void)                         /* other use in between (stopped after) */
{
    uint32_t j, k = 5 + rn(40);
    for (j = 0; j < k; j++) {
        uint32_t r = rn(10);
        if (r < 5) fz_seq_step(); else if (r < 8) ui_rand(); else midi_rand();
    }
    stop_all();
}
/* other edits that never write a section, the autosave or a backup object (so what was saved is still there) */
static void scramble_light(void)
{
    uint8_t a[700];
    uint32_t j, k = 5 + rn(40);
    for (j = 0; j < k; j++) {
        uint32_t r = rn(13), t = rn(NTRK);
        switch (r) {
        case 0: a[0] = 0; a[1] = (uint8_t)rn(P_COUNT); v14(a + 2, rval()); cmdv(ED_SET, a, 4); break;
        case 1: a[0] = (uint8_t)t; a[1] = (uint8_t)rn(P_COUNT); v14(a + 2, rval()); cmdv(ED_TRACK_PARAM, a, 4); break;
        case 2: a[0] = (uint8_t)t; a[1] = (uint8_t)rn(NSTEP); rbytes(a + 2, 11, 0); cmdv(ED_TRACK_STEP, a, 13); break;
        case 3: a[0] = (uint8_t)rn(NSTEP); rbytes(a + 1, 13, 0); cmdv(ED_DRUM_STEP, a, 14); break;
        case 4: a[0] = (uint8_t)t; a[1] = (uint8_t)rn(NSTEP); a[2] = (uint8_t)rn(P_COUNT); v14(a + 3, rval()); cmdv(ED_LOCK_SET, a, rn(4) ? 5 : 3); break;
        case 5: a[0] = (uint8_t)t; a[1] = (uint8_t)rn(NSTEP); a[2] = (uint8_t)rn(128); cmdv(ED_MICRO_SET, a, 3); break;
        case 6: a[0] = (uint8_t)t; a[1] = (uint8_t)rn(NSTEP); a[2] = (uint8_t)rn(4); cmdv(ED_FILL_SET, a, 3); break;
        case 7: a[0] = (uint8_t)rn(NENGINES); a[1] = (uint8_t)rn(32); cmdv(ED_PRESET, a, 2); break;
        case 8: a[0] = (uint8_t)t; cmdv(ED_TRACK, a, 1); break;
        case 9: a[0] = 1; a[1] = (uint8_t)rn(G_COUNT); if (a[1] == G_SAVE || a[1] == G_LOAD || a[1] == G_NEWPRJ || a[1] == G_CLRSEQ || a[1] == G_INITSND) a[1] = G_BPM; v14(a + 2, rval()); cmdv(ED_SET, a, 4); break;
        case 10: a[0] = (uint8_t)t; a[1] = (uint8_t)rn(P_COUNT); v21(a + 2, rvalw()); cmdw(ED_TRACK_PARAM, a, 5); break;
        case 11: macro_rand(); break;
        default: midi_rand(); aud(1); break;
        }
    }
    stop_all();
}
static void bk_read0(uint8_t *raw)
{
    uint8_t a[16];
    uint32_t off, n;
    cmdv(ED_BK_LIST, a, 0);
    for (off = 0; off < sizeof(project_t); off += 256) {
        uint32_t c = sizeof(project_t) - off > 256 ? 256 : (uint32_t)sizeof(project_t) - off, k, w;
        n = 0; a[n++] = 0; n += put5(a + n, off); a[n++] = c & 127; a[n++] = (uint8_t)(c >> 7);
        cmdv(ED_BK_GET, a, n);
        if (rx_n < 13 || rx_last[6] != 0) { fprintf(stderr, "BK_GET rc %u\n", rx_n > 6 ? rx_last[6] : 99); fail("backup GET"); }
        for (k = 14, w = 0; w < c && k < rx_n - 1; ) {   /* F0 7D 46 4C 23 id rc off(5) n(2) pack7.. F7 */
            uint32_t m = rx_last[k++], j;
            for (j = 0; j < 7 && w < c && k < rx_n - 1; j++) raw[off + w++] = (uint8_t)(rx_last[k++] | ((m >> j) & 1u) << 7);
        }
    }
}
static int bk_restore_ok(const uint8_t *raw)
{
    uint32_t tries;
    for (tries = 0; tries < 20; tries++) {             /* (bk_restore cuts some off at random: again) */
        bk_restore(0, raw, sizeof(project_t));
        if (rx_n >= 8 && rx_last[5] == 2 && rx_last[7] == 0) return 1;   /* (the commit; not an abort) */
        stop_all();
    }
    return 0;
}
static void round_trip(void)
{
    static uint8_t raw[sizeof(project_t)];
    uint32_t i;
    stop_all();
    if (song.playing || transport_req) return;
    /* the backup of the working project reads as what it is */
    proj_capture(&rt_a);
    bk_read0(raw);
    if (memcmp(raw, &rt_a, sizeof raw)) {
        for (i = 0; i < sizeof raw && raw[i] == ((uint8_t *)&rt_a)[i]; i++) ;
        fprintf(stderr, "the backup read differs from the capture at %s\n", proj_field(i));
        fail("backup GET data");
    }
    /* restored once (its values put in range), captured: the project as it loads. The working project's macro
     * assignments are valid ids already: that restore keeps every one */
    if (!bk_restore_ok(raw)) fail("restore of an unchanged backup refused");
    audio_blocks(2);
    rt_b = rt_a;
    proj_capture(&rt_a);
    for (i = 0; i < NTRK; i++)
        if (memcmp(rt_a.t[i].macro, rt_b.t[i].macro, MACRO_COUNT)) {
            fprintf(stderr, "track %u macros %u %u %u %u -> %u %u %u %u\n", i, rt_b.t[i].macro[0], rt_b.t[i].macro[1],
                    rt_b.t[i].macro[2], rt_b.t[i].macro[3], rt_a.t[i].macro[0], rt_a.t[i].macro[1], rt_a.t[i].macro[2],
                    rt_a.t[i].macro[3]);
            fail("a backup restored loses the macro assignments");
        }
    memcpy(raw, &rt_a, sizeof raw);
    /* (a) other use, then that backup restored: the same project */
    scramble_light();
    if (!bk_restore_ok(raw)) fail("restore refused");
    audio_blocks(2);
    proj_capture(&rt_b);
    if (!rt_cmp("backup -> other use -> restore")) fail("a backup restored does not give the project back");
    /* (b) a section saved, other use, loaded (from RAM, or from flash) */
    {
        uint32_t sl = rn(4);
        int lost;
        project_save(sl);
        scramble_light();
        lost = rn(2);
        if (lost) proj_slot[sl].magic ^= 0xFFu;     /* (the RAM copy lost: from flash) */
        if (song.playing || transport_req) return;
        project_load(sl);
        proj_capture(&rt_b);
        if (!rt_cmp("section save -> other use -> load (before the audio)")) { fprintf(stderr, "slot %u, %s\n", sl, lost ? "from flash" : "from RAM"); fail("a section saved and loaded is not the same"); }
        audio_blocks(2);
        proj_capture(&rt_b);
        if (!rt_cmp("section save -> other use -> load")) { fprintf(stderr, "slot %u, %s\n", sl, lost ? "from flash" : "from RAM"); fail("a section saved and loaded is not the same"); }
    }
    /* (c) the autosave, other use, the power-on resume */
    (void)st_save(OBJ_AUTOSAVE, &rt_a, sizeof rt_a);
    scramble_light();
    {   /* (at power-on nothing sounds: autosave_resume switches the engines at once) */
        uint32_t k, v;
        for (k = 0; k < NTRK; k++) for (v = 0; v < NVOICE; v++) trk[k].v[v].active = trk[k].v[v].gate = 0;
        for (v = 0; v < NDRUM; v++) drums.v[v].active = 0;
    }
    autosave_resume();
    audio_blocks(2);
    proj_capture(&rt_b);
    if (!rt_cmp("autosave -> other use -> resume")) fail("the autosave resumed is not the project saved");
}

/* ---- MODE 2: USB-MIDI in */
static void midi_rand(void)
{
    uint32_t cin = rn(16), cable = rn(4) ? 0 : rn(16), ch = rn(16), st, d1, d2, pkt;
    switch (rn(10)) {
    case 0: st = 0x90 | ch; d1 = rn(128); d2 = rn(3) ? rn(128) : 0; cin = 9; break;
    case 1: st = 0x80 | ch; d1 = rn(128); d2 = rn(128); cin = 8; break;
    case 2: st = 0xB0 | ch; d1 = rn(4) ? rn(128) : (uint32_t)(98 + rn(4)); d2 = rn(128); cin = 11; break;
    case 3: st = 0xE0 | ch; d1 = rn(128); d2 = rn(128); cin = 14; break;
    case 4: st = 0xC0 | ch; d1 = rn(128); d2 = 0; cin = 12; break;
    case 5: st = (rn(2) ? 0xD0 : 0xA0) | ch; d1 = rn(128); d2 = rn(128); cin = st >> 4; break;
    case 6: st = 0xF8 + rn(8); d1 = d2 = 0; cin = 15; break;
    default: st = rn(256); d1 = rn(256); d2 = rn(256); break;
    }
    pkt = cin | cable << 4 | st << 8 | d1 << 16 | d2 << 24;
    {
        uint8_t b[4] = {(uint8_t)pkt, (uint8_t)(pkt >> 8), (uint8_t)(pkt >> 16), (uint8_t)(pkt >> 24)};
        logop(OP_MIDI, b, 4);
        if (!ep1_take(b, 4))
            audio_blocks(1);                      /* (the ring full: the audio drains it) */
    }
}

int main(int argc, char **argv)
{
    uint32_t mode = argc > 1 ? (uint32_t)atoi(argv[1]) : 0, iter = argc > 2 ? (uint32_t)atoi(argv[2]) : 10000, i, seed;
    sd = argc > 3 ? (uint32_t)atoi(argv[3]) : 1;
    if (!sd) sd = 1;
    seed = sd;
    __sanitizer_set_death_callback(on_death);
    harness_init();
    after();
    for (i = 0; i < iter; i++) {
        if (i && i % 5000 == 0) { printf("mode %u: %u iterations ok\n", mode, i); fflush(stdout); }
        if (mode == 0) {
            cmd_rand(rn(3) == 0);
            if (rn(20) == 0) aud(1 + rn(8));
            if (rn(200) == 0) ui_rand();
        } else if (mode == 1) {
            fz_seq_step();
        } else if (mode == 6) {
            scramble();
            round_trip();
        } else if (mode == 5) {
            uint32_t k = 1 + rn(16), j;
            for (j = 0; j < k; j++) midi_rand();
            if (rn(3) == 0) uart_rand();
            if (rn(2) == 0) aud(1 + rn(4));
            after();
            if (i % 2000 == 1999 || i + 1 == iter) {
                midi_let_go();
                if (!midi_hang_check()) fail("a note still held after every note-off");
            }
        } else if (mode == 4) {
            exhaustive();
            break;
        } else if (mode == 3) {                   /* each boot in a child: its own state */
            pid_t pid;
            int st = 0;
            fflush(stdout); fflush(stderr);
            pid = fork();
            if (!pid) {
                uint32_t j, n = 40 + rn(200);
                boot();
                for (j = 0; j < n; j++) {
                    uint32_t r = rn(10);
                    if (r < 4) fz_seq_step();
                    else if (r < 6) play_slots();
                    else if (r < 8) ui_rand();
                    else { uint32_t k = 1 + rn(8), q; for (q = 0; q < k; q++) midi_rand(); aud(2); }
                }
                _exit(0);
            }
            waitpid(pid, &st, 0);
            if (!WIFEXITED(st) || WEXITSTATUS(st)) { printf("boot %u (seed %u): child failed\n", i, seed); return 1; }
            rn(1);
        } else {
            uint32_t k = 1 + rn(16), j;
            for (j = 0; j < k; j++) midi_rand();
            if (rn(3) == 0) uart_rand();
            if (rn(3) == 0) aud(1 + rn(4));
            if (rn(20) == 0) ui_rand();
            if (rn(50) == 0) fz_seq_step();
            after();
        }
    }
    printf("editor_fuzz mode %u seed %u: %u iterations ok (flash: %u erases, %u programs, %u refused; audio peak %d)\n",
           mode, seed, iter, fl_erases, fl_progs, fl_bad, audio_peak);
    return 0;
}
