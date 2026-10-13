/* SPDX-License-Identifier: GPL-3.0-only */
/* Real panel/UI, sounds, NOR storage, projects, user presets and editor macros. */
#define FELUCCA_ARRANGER 1
#define FELUCCA_FLASH 1
#define FELUCCA_OTA 0
#define FELUCCA_VERSION "ORBIT TEST"
#include <stdint.h>
static uint32_t host_slots[4u * 0x14000u / 4u];
#define SMP_USER_XIP(k) ((const uint8_t *)host_slots + (k) * SMP_USER_SIZE)
#define FM6_BANK_XIP(copy) (nor + st_sector(OBJ_FM6BANK, copy) + ST_PAYLOAD_OFF)
static uint8_t nor[0x100000];
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <assert.h>
static uint16_t screen[240 * 240];
static void lcd_sync(void) {}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p)
{
    uint32_t i, j;
    assert(x + w <= 240 && y + h <= 240);
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            screen[(y + j) * 240 + x + i] = p[j * w + i];
}
#include "../firmware/src/gfx.c"
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    uint32_t i, j;
    assert(x + w <= 240 && y + h <= 240);
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            screen[(y + j) * 240 + x + i] = swap16(c);
}
static int32_t encs[7];
static uint32_t fm1_ticks(void) { return fm1_ms * 1000u * 24u; }
#define FM1_TICKS_PER_US 24u
static int32_t fm1_enc_take(uint32_t e) { int32_t s = encs[e]; encs[e] = 0; return s; }
static uint8_t fm1_led[16], fm1_led_dim[16], fm1_led_bg[16];
static volatile uint16_t fm1_led_bg_ns;
#define FM1_NCOL 16u
#define FM1_NBTN 14u
#define FM1_NENC 7u
static const int8_t FM1_KEYMAP[5][16];
static void fm1_led_key(uint32_t id, int on) { (void)id; (void)on; }
static uint32_t edges_btn, notes_seen;
static uint32_t fm1_input_edges(int x) { uint32_t e = edges_btn; (void)x; edges_btn = 0; return e; }
static uint32_t fm1_input_note_edges(void) { uint32_t e = fm1_in.notes & ~notes_seen; notes_seen = fm1_in.notes; return e; }
static void fm1_wdt_feed(void) {}
static int32_t fm1_adc_read(int c) { (void)c; return -1; }
static struct { uint32_t magic, stage, page, home, ui_frames; } felucca_dbg;
#define FELUCCA_ICONS 1
#define SCOPE_N 512u
static int16_t scope_buf[SCOPE_N], scope_bufr[SCOPE_N];
static uint32_t scope_w;
#include "../firmware/src/panel.c"
#include "../firmware/src/ui.c"
#include "../firmware/src/ui_song.c"
#include "../firmware/src/ui_studio.c"
#include "../firmware/src/icons.c"
#include "../firmware/src/ui_draw.c"
#include "../firmware/src/ui_vis.c"
#include "../firmware/src/ui_layers.c"
#include "../firmware/src/ui_menu.c"
#include "../firmware/src/ui_input.c"

/* ---- the flash chip: a NOR image (erase -> 0xFF, a program clears bits), a write failure on request */
static int fail_prog;
static uint8_t flash_ok;
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off) { memset(nor + off, 0xFF, 4096); return 0; }
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    uint32_t i;
    if (fail_prog)
        return -9;
    for (i = 0; i < n; i++)
        nor[off + i] &= s[i];
    return 0;
}
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static uint32_t fl_jedec_host(void) { return 0x856014u; }
#define fl_jedec_ram fl_jedec_host
#define FL_FAR(f) f
#define FL_JEDEC_OK(id) ((id) == 0x856014u)   /* as firmware/hal/fm1_flash.h */
static void fl_plain_window_init(void) {}
static int fl_write(uint32_t off, const void *p, uint32_t n) { return st_prog(off, p, n); }
static int fl_erase4k_quiet(uint32_t off, uint32_t *t) { (void)t; return st_erase(off); }
static void fl_inval(uint32_t a, uint32_t n) { (void)a; (void)n; }
#include "../firmware/src/storage.c"
#include "../firmware/src/upreset.c"
#include "../firmware/src/project.c"
#define SXQ 64u
static uint32_t so_w, so_r;
static uint8_t wire[4][600];
static uint32_t wire_n[4], nw;
static int ota_wire_send(const uint8_t *p, uint32_t n)
{
    assert(n <= sizeof wire[0]);
    memcpy(wire[nw & 3u], p, n);
    wire_n[nw++ & 3u] = n;
    return 0;
}
static int ota_frame_get(const uint8_t **p, uint32_t *n) { (void)p; (void)n; return 0; }
static void ota_frame_done(void) {}
#include "../firmware/src/editor.c"


static int fails;
static void check(int ok, const char *what)
{
    printf("macro: %-78s %s\n", what, ok ? "ok" : "FAIL");
    fails += !ok;
}
static void frame(void)
{
    ui_input(); ui_leds(); ui_draw(); fm1_ms += 16;
}
static void turn(uint32_t role, int32_t steps)
{
    encs[panel.enc[role]] = steps; frame();
}
static void tap(uint32_t b)
{
    edges_btn = 1u << panel.btn[b]; fm1_in.buttons = edges_btn; frame();
    fm1_in.buttons = 0; frame();
}
static void ed(uint32_t cmd, const uint8_t *a, uint32_t n)
{
    uint8_t f[600] = {ED_HDR0, ED_HDR1, ED_HDR2};
    f[3] = (uint8_t)cmd; memcpy(f + 4, a, n); nw = 0; ed_handle(f, n + 4);
}
static void backup_cycle(uint32_t id)
{
    uint8_t raw[3840], a[320];
    uint32_t n, off, i, count, len;
    ed(ED_BK_LIST, a, 0); (void)ed_bk_obj(id, &len);
    for (off = 0; off < len; off += count) {
        count = len - off < 256u ? len - off : 256u; a[0] = (uint8_t)id;
        for (i = 0; i < 5u; i++) a[i + 1] = (uint8_t)((off >> (7u * i)) & 127u);
        a[6] = count & 127u; a[7] = count >> 7; ed(ED_BK_GET, a, 8);
        check(wire[0][6] == 0 && ed_unpack7(wire[0] + 14, wire_n[0] - 15u, raw + off, count) == count,
              "backup GET reads real map-containing bytes in bounded chunks");
    }
    a[0] = 0; a[1] = (uint8_t)id;
    for (i = 0; i < 5u; i++) {
        a[i + 2] = (uint8_t)((len >> (7u * i)) & 127u);
        a[i + 7] = (uint8_t)((st_crc32(raw, len) >> (7u * i)) & 127u);
    }
    ed(ED_BK_PUT, a, 12); check(wire[0][7] == 0, "backup BEGIN accepts the captured object");
    for (off = 0; off < len; off += count) {
        count = len - off < 256u ? len - off : 256u;
        ed_begin(ED_BK_PUT); ed_pack7(raw + off, count); n = ed_n - 5u;
        a[0] = 1; a[1] = (uint8_t)id;
        for (i = 0; i < 5u; i++) a[i + 2] = (uint8_t)((off >> (7u * i)) & 127u);
        memcpy(a + 7, ed_out + 5, n); ed(ED_BK_PUT, a, n + 7u);
        check(wire[0][7] == 0, "backup DATA retains packed macro and signed16 bytes");
    }
    memset(TSEL->macro, MACRO_DEFAULT, 4);
    a[0] = 2; a[1] = (uint8_t)id; ed(ED_BK_PUT, a, 2);
    check(wire[0][7] == 0, "backup COMMIT accepts the same codec and CRC");
}
int main(void)
{
    uint32_t i, k;
    project_t saved;
    uint8_t maps[4] = {P_ATK, P_ITYPE, P_E1, P_E3};
    memset(nor, 255, sizeof nor); panel = PANEL_DEFAULT; layers_init();
    host_tracks_init(); settings.palette = 5; palette_set(5); flash_ok = 1;
    for (i = 0; i < NTRK; i++) track_defaults(&trk[i]);
    song.sel = 0; apply_preset_to(TSEL, 0); go_home();
    /* Given a factory sound, When EDIT / SELECT navigates, Then MACROS is editable. */
    tap(B_EDIT); turn(EN_SELECT, 99);
    check(!ui.home && cur_page()->graph == GR_MACROS, "EDIT / SELECT reaches MACROS through real panel input");
    { int16_t value = TSEL->p[P_LEVEL]; turn(EN_K1, 1);
      check(TSEL->macro[0] == P_LEVEL && TSEL->p[P_LEVEL] == value && sync_reload,
            "one assignment detent chooses LEVEL without changing the sound"); }
    turn(EN_K1, -1);
    check(TSEL->macro[0] == MACRO_DEFAULT, "left from first candidate selects engine DEFAULT");
    turn(EN_K1, 2); turn(EN_K1 + 1, 2);
    check(TSEL->macro[0] == P_ATK && TSEL->macro[1] == P_ATK, "duplicate assignments are legal, one item per detent");
    turn(EN_SELECT, -1);
    check(!ui.home && cur_page()->graph == GR_PERFORM, "SELECT from assignment page opens PERFORM");
    { int16_t value = TSEL->p[P_ATK]; mlu.on = 1; turn(EN_K1, 1);
      check(TSEL->p[P_ATK] > value, "real PERFORM knob changes the assigned envelope parameter");
      check(ui.lock_par == P_ATK && mlu.pick && mlu.id == P_ATK,
            "PERFORM selects its assigned parameter for step locks and MIDI learn");
      mlu.on = 0; }
    turn(EN_K1, 99999); check(TSEL->p[P_ATK] == 127, "PERFORM clamps high input to descriptor range");
    turn(EN_K1, -99999); check(TSEL->p[P_ATK] == 0, "PERFORM clamps low input to descriptor range");
    tap(B_HOME); check(ui.home && !vis_shown(), "PERFORM HOME returns directly to unchanged Event Tape");
    tap(B_HOME); check(vis_shown(), "Tape HOME still opens Visualizer"); tap(B_HOME);
    /* Given every factory engine, When a sound loads, Then DEFAULT resolves its own descriptors. */
    for (i = 0; i < NENGINES; i++) {
        set_engine_of(TSEL, i); apply_preset_to(TSEL, 0);
        for (k = 0; k < 4u; k++)
            check(TSEL->macro[k] == MACRO_DEFAULT && macro_id(TSEL, k) == ENGINES[i]->macro[k],
                  "factory load inherits the engine's four macro destinations");
    }
    set_engine_of(TSEL, ENGI_CLUSTER); apply_preset_to(TSEL, 0);
    memcpy(TSEL->macro, maps, 4); TSEL->p[P_E1] = -32768; TSEL->p[P_E3] = 32767;
    /* Given a customised factory sound, When USER stores it, Then another track receives sound and map. */
    tap(B_EDIT); tap(B_SAVE); turn(EN_SELECT, -99); turn(EN_SELECT, 1);
    check(cur_page()->graph == GR_USER, "SAVE / SELECT reaches the actual USER page");
    turn(EN_K1, 31); turn(EN_K1 + 3, 1);
    check(!up_used(31), "first USER SAVE detent arms without writing");
    turn(EN_K1 + 3, 1);
    check(up_used(31) && !memcmp(up_rec(31)->macro, maps, 4),
          "actual USER STORE knob writes OPB2 to simulated NOR storage");
    song.sel = 1; apply_preset_to(TSEL, 0);
    turn(EN_K1 + 1, 1); turn(EN_K1 + 1, 1);
    check(TSEL->eng_req == ENGI_CLUSTER, "actual USER LOAD knob loads sound onto another track");
    check(!memcmp(TSEL->macro, maps, 4) && TSEL->p[P_E1] == -32768 && TSEL->p[P_E3] == 32767,
          "another track receives signed16 CLUSTER values and assignments together");
    memset(up_bank, 0, sizeof up_bank); up_boot(&proj_tmp, sizeof proj_tmp);
    check(up_load(31) == 0 && !memcmp(TSEL->macro, maps, 4), "cold bank load retains slot32 assignments");
    transport_req = 0; backup_cycle(7);
    check(up_load(31) == 0 && !memcmp(TSEL->macro, maps, 4) && TSEL->p[P_E1] == -32768,
          "actual bank LIST/GET/PUT round trip retains map and signed16 sound");
    /* Given distinct maps in A..D, When cold loading or recalling a section, Then each sound owns its map. */
    for (i = 0; i < 4u; i++) {
        TSEL->macro[0] = (uint8_t)(P_ATK + i); transport_req = 0; project_save(i);
    }
    memset(proj_slot, 0, sizeof proj_slot); persist_boot();
    for (i = 0; i < 4u; i++) {
        transport_req = 0; project_load(i);
        check(TSEL->macro[0] == P_ATK + i && TSEL->p[P_E1] == -32768,
              "A/B/C/D cold flash project load restores mappings with raw sound");
        song.playing = 1; arrangement_apply(i); song.playing = 0;
        check(TSEL->macro[0] == P_ATK + i, "playing section recall installs that section's mappings");
    }
    transport_req = 0; proj_capture(&saved);
    backup_cycle(0);
    check(TSEL->macro[0] == P_REL && TSEL->p[P_E1] == -32768,
          "actual working project LIST/GET/PUT restores sound and macros");
    transport_req = 0; persist_flush_now(); memset(trk, 0, sizeof trk); autosave_resume();
    check(TSEL->macro[0] == P_REL && TSEL->p[P_E1] == -32768, "cold working autosave resumes sound and macros");
    check(up_put(31, 0) == 0, "erasing provenance slot succeeds");
    project_apply(&saved);
    check(TSEL->macro[0] == P_REL && TSEL->p[P_E1] == -32768,
          "project sound and maps stay authoritative after its user slot is erased");
    transport_req = 0;
    check(project_restore(4, &saved, sizeof saved) == 0 && TSEL->macro[0] == P_REL,
          "working backup restore uses the same ORP7 macro codec");
    { uint8_t a[5] = {1, P_ATK, P_ITYPE, P_E1, P_E3}; ed(ED_MACRO, a, 5);
      check(wire[0][6] == 0 && !memcmp(trk[1].macro, maps, 4), "protocol78 assigns the actual track atomically");
      a[2] = P_COUNT; ed(ED_MACRO, a, 5);
      check(wire[0][6] == 1 && !memcmp(trk[1].macro, maps, 4), "invalid protocol macro ID changes nothing"); }
    TSEL->macro[0] = P_E1; TSEL->p[P_E1] = -32768;
    tap(B_EDIT); turn(EN_SELECT, 99); turn(EN_SELECT, -1);
    turn(EN_K1, -99999); check(TSEL->p[P_E1] == -32768, "PERFORM retains signed16 CLUSTER negative endpoint");
    turn(EN_K1, 99999); check(TSEL->p[P_E1] == 32767, "PERFORM reaches signed16 CLUSTER positive endpoint");
    /* Given a reserved CLUSTER control, When PERFORM edits it, Then the unavailable slot is a no-op. */
    TSEL->macro[0] = P_E7;
    { int16_t value = TSEL->p[P_E7], *vp; turn(EN_K1, 5);
      check(!page_desc(cur_page(), 0, &vp) && !vp && TSEL->p[P_E7] == value,
            "unavailable descriptors show -- and PERFORM cannot edit them"); }
    TSEL->macro[0] = P_IA; TSEL->p[P_ITYPE] = 5;
    check(str_eq(track_desc(TSEL, macro_id(TSEL, 0))->label, "BITS"), "macro descriptor follows INSERT type");
    TSEL->macro[0] = P_ATK; TSEL->p[P_ESYNC] = 1;
    check(track_desc(TSEL, macro_id(TSEL, 0))->names == ESYNC_NAMES, "macro descriptor follows ENV SYNC");
    { uint8_t a[2] = {1, P_ATK};
      ed(ED_MACRO_DESC, a, 2);
      check(wire_n[0] > 100u && wire[0][8] == 1, "protocol81 exposes bounded F_INT ENV SYNC names");
      a[0] = 0; ed(ED_DESC, a, 2);
      check(wire[0][7] == F_TIME, "legacy DESC preserves its static descriptor contract"); }
    { int16_t bpm = song.g[G_BPM]; song.g[G_BPM] = 0;
      check(bpm_div() == 20 && div_samples(0) > 0 && dly_samples(0) > 0,
            "zero tempo cannot produce a zero denominator in native clock helpers");
      song.g[G_BPM] = bpm; }
    { uint8_t a[4] = {1, G_ENGSEL, ENGI_CLOUD, 64};
      ed(ED_SET, a, 4);
      check(TSEL->eng_req == ENGI_CLOUD,
            "real editor engine SET reaches CLOUD without clamping to CLUSTER"); }
    song.sel = TRK_DRUM; go_home(); tap(B_EDIT); turn(EN_SELECT, 99);
    check(cur_page()->graph != GR_MACROS && cur_page()->graph != GR_PERFORM && !macro_eligible(TDRUM, P_LEVEL),
          "drum track cannot enter PERFORM or assign MACROS");
    printf("macro: %d failure(s)\n", fails); return fails;
}
