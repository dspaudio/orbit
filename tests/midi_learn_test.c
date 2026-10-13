/* SPDX-License-Identifier: GPL-3.0-only */
/* ORBIT 0.7.0: the MIDI settings of SLOOP 2.6 on the real firmware stack (UI, sequencer, storage.c on a simulated
 * NOR, project.c, upreset.c, editor.c; only the panel, the flash chip and USB are doubles):
 *   learn     a knob picks a parameter, the next CC is learned; a learned CC sets its track's parameter on any
 *             channel IN hears, not the standard map; a reserved CC is never learned; scaling at 0 and 127 for an
 *             ordinary, a bipolar and a signed16 CLUSTER parameter
 *   persist   the settings object holds the map and GLO > MIDI (always 40 bytes): written, RAM cleared, the real
 *             boot path (dsu_boot) brings them back; old objects (no part, 32, 36 bytes) read with defaults
 *   survive   factory sounds, a user sound, project loads A..D and a section recall leave the map and channels
 *   object 10 LIST / GET carry the actual map; PUT of a default part resets a customised FM-1; a bad length
 *             changes nothing; object 1 and 9 restores keep the MIDI part
 *   routing   channels, OTHER, IN = CH n, sustain, pitch bend, Program Change; ignored channels stay ignored
 * Exit status: the number of failed checks. */
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
    printf("midi-learn: %-92s %s\n", what, ok ? "ok" : "FAIL");
    fails += !ok;
}
static void frame(void)
{
    uint32_t q;
    static int32_t o[CTL * 2];
    for (q = 0; q < 22u; q++)
        mix_block(o, CTL);
    ui_input(); ui_leds(); ui_draw(); fm1_ms += 16;
}
static void frames(uint32_t n) { while (n--) frame(); }
static void midi_in(uint32_t st, uint32_t d1, uint32_t d2)
{
    static int32_t o[CTL * 2];
    midi_in_q[mi_w % MQ] = (st >> 4) | st << 8 | d1 << 16 | d2 << 24;
    mi_w++;
    mix_block(o, CTL);
}
static void ed(const uint8_t *a, uint32_t na, uint32_t cmd)
{
    uint8_t f[600];
    f[0] = ED_HDR0, f[1] = ED_HDR1, f[2] = ED_HDR2, f[3] = (uint8_t)cmd;
    memcpy(f + 4, a, na);
    nw = 0;
    ed_handle(f, 4u + na);
}
static int sounding(const track_t *t, uint32_t note)
{
    uint32_t k;
    for (k = 0; k < NVOICE; k++)
        if (t->v[k].active && t->v[k].note == note && t->v[k].gate)
            return 1;
    return 0;
}
static void reset_midi_ram(void)                  /* RAM as after a power cycle: nothing learned, channels garbage */
{
    uint32_t k;
    for (k = 0; k < ML_N; k++)
        ml_tab[k] = 0;
    memset(midi_set, 0xEE, sizeof midi_set);
    song.g[G_CH1] = song.g[G_CH2] = song.g[G_CH3] = song.g[G_DRCH] = song.g[G_CHOTH] = song.g[G_BEND] = -5;
}
static uint32_t settings_len(void)
{
    st_hdr_t h;
    return st_current(OBJ_SETTINGS, &h) >= 0 ? h.len : 0u;
}

int main(void)
{
    static uint8_t mp[MIDI_PART], raw[MIDI_PART];
    uint32_t i;
    memset(nor, 0xFF, sizeof nor);
    host_tracks_init();
    arr_defaults(&arrangement);
    flash_ok = 1;                                 /* (persist_boot's flash part: XIP sample and FM6 scans are the chip's) */
    dsu_boot();
    up_boot(&proj_tmp, sizeof proj_tmp);
    panel_init();
    song.g[G_CH1] = midi_set[0], song.g[G_CH2] = midi_set[1], song.g[G_CH3] = midi_set[2];
    song.g[G_DRCH] = midi_set[3], song.g[G_CHOTH] = midi_set[4], song.g[G_BEND] = midi_set[5];
    for (i = 0; i < NTRK; i++)
        track_defaults(&trk[i]);
    check(flash_ok && midi_set[0] == 1 && midi_set[1] == 2 && midi_set[2] == 3 && midi_set[3] == 10 && !midi_set[4] &&
          midi_set[5] == 2 && ml_count() == 0u, "empty flash: channels 1 2 3 10, OTHER SEL, BEND 2, nothing learned");

    /* ---- learn through the UI: GLO + F#3, a knob, a CC */
    ml_toggle();
    ml_knob(&trk[0], P_DIST);
    check(mlu.on && mlu.pick && ml_arm, "LEARN on, DIST of track 1 picked: waiting for a CC");
    midi_in(0xB5, 74, 10);                        /* (channel 6: OTHER plays the selected track) */
    frame();
    check(ml_count() == 1u && (ml_tab[0] & 127u) == 74u && ((ml_tab[0] >> 7) & 3u) == 0u &&
          (ml_tab[0] >> 9) == ml_code(P_DIST), "CC 74 learned for track 1 DIST (entry: cc | track << 7 | code << 9)");
    check(ml_learn(64, 1, P_LEVEL) == 2u && ml_learn(0, 1, P_LEVEL) == 2u && ml_learn(1, 1, P_LEVEL) == 2u &&
          ml_learn(120, 1, P_LEVEL) == 2u && ml_count() == 1u, "sustain 64, bank 0, mod wheel 1, mode 120: never learned");
    check(ml_learn(21, 2, P_E1) == 1u && ml_learn(22, 1, P_PAN) == 1u && ml_learn(23, 0, P_ITYPE) == 1u,
          "CC 21 -> track 3 engine control 2, CC 22 -> track 2 PAN, CC 23 -> track 1 INSERT TYPE");
    ml_toggle();
    trk[0].p[P_DIST] = 0;
    midi_in(0xB3, 74, 127);                       /* (channel 4: OTHER = SEL, the selected track is 1) */
    check(trk[0].p[P_DIST] == TP[P_DIST].max, "learned CC 74 at 127 (any channel IN hears): DIST at its top");
    midi_in(0xB0, 74, 0);
    check(trk[0].p[P_DIST] == TP[P_DIST].min && trk[0].p[P_TFLT] == 0, "at 0: its bottom; not the standard map's FILTER");
    midi_in(0xB0, 22, 0);
    i = trk[1].p[P_PAN];
    midi_in(0xB0, 22, 127);
    check(i == (uint32_t)TP[P_PAN].min && trk[1].p[P_PAN] == TP[P_PAN].max, "bipolar PAN of track 2: 0 -> min, 127 -> max");
    host_preset(&trk[2], ENGI_CLUSTER, 0);
    midi_in(0xB0, 21, 0);
    check(trk[2].p[P_E1] == -32768, "signed16 CLUSTER WAVE of track 3 at 0: -32768");
    midi_in(0xB0, 21, 127);
    check(trk[2].p[P_E1] == 32767, "... at 127: 32767 (no zero denominator, no wrap)");
    midi_in(0xB0, 23, 127);
    check(trk[0].p[P_ITYPE] == TP[P_ITYPE].max, "a 2.6 parameter learned (INSERT TYPE): its top");

    /* ---- channels and routing */
    song.g[G_CH1] = 5, song.g[G_CH2] = 0, song.g[G_CHOTH] = 1;
    sections_flush();                              /* (the main loop: GLO > MIDI into the settings) */
    check(midi_set[0] == 5 && midi_set[1] == 0 && midi_set[4] == 1 && settings_later == 0 && song_dirty,
          "GLO > MIDI: CH1 5, CH2 OFF, OTHER OFF taken into the settings (saved when quiet)");
    midi_in(0x94, 60, 100);
    check(sounding(&trk[0], 60) && !sounding(&trk[1], 60), "a note on channel 5 plays track 1");
    midi_in(0x84, 60, 0);
    midi_in(0x91, 62, 100);
    check(!trk[1].nheld && !trk[0].nheld, "channel 2 (CH2 OFF, OTHER OFF): ignored");
    midi_in(0xB1, 22, 0);
    check(trk[1].p[P_PAN] == TP[P_PAN].min, "... but a learned CC still works on it (the controller is the user's)");
    song.g[G_CHOTH] = 0, song.g[G_CH2] = 2;
    midi_in(0xB4, 64, 127);
    midi_in(0x94, 64, 100);
    midi_in(0x84, 64, 0);
    check(trk[0].nheld || trk[0].v[0].active, "sustain held on channel 5: the note keeps sounding after its note-off");
    midi_in(0xB4, 64, 0);
    midi_in(0xE4, 0, 127);
    check(trk[0].bend == 8191 - 0 - 0 || trk[0].bend == (int16_t)((0 | 127 << 7) - 8192), "pitch bend on channel 5: track 1");
    midi_in(0xE4, 0, 64);
    check(trk[0].bend == 0, "... the wheel back: no bend");
    midi_in(0xC4, 3, 0);
    check(pc_req[0] == 4u, "Program Change 3 on channel 5: a sound request for track 1 (list position + 1)");
    pc_req[0] = 0;
    song.g[G_ROUTE] = 2 + 11;                      /* IN = CH 12 */
    song.sel = 2;
    midi_in(0x9B, 67, 90);
    midi_in(0x94, 69, 90);
    check(sounding(&trk[2], 67) && !sounding(&trk[0], 69), "IN = CH 12: channel 12 plays the selected track, channel 5 ignored");
    midi_in(0x8B, 67, 0);
    sections_flush();
    check(lights_min == 13u && ((lights_word() >> 21) & 31u) == 12u, "IN = CH 12 kept in the settings word (bits 21..25)");
    song.g[G_ROUTE] = 0, song.sel = 0;
    sections_flush();

    /* ---- the settings object: always the 40-byte MIDI part, and back through the real boot path */
    song.playing = 0;
    settings_save();
    check(settings_len() == sizeof(persist_t) + sizeof(dsu_bank_t) + MIDI_PART, "settings object = persist + SYN kits + 40-byte MIDI part");
    midi_part_fill(mp);
    reset_midi_ram();
    dsu_boot();
    midi_part_fill(raw);
    check(!memcmp(mp, raw, sizeof mp) && ml_count() == 4u && midi_set[0] == 5 && midi_set[5] == 2 &&
          song.g[G_CH1] == 5 && song.g[G_BEND] == 2, "cold RAM -> dsu_boot: the map and GLO > MIDI back, globals set");
    trk[0].p[P_DIST] = 0;
    midi_in(0xB0, 74, 127);
    check(trk[0].p[P_DIST] == TP[P_DIST].max, "after the reload the learned CC 74 sets track 1 DIST again");

    /* ---- survive sound and project changes */
    apply_preset_to(&trk[0], 1);
    set_engine_of(&trk[1], 1);
    project_save(0);
    project_load(0);
    section_store(1);
    section_load(1);
    project_new();
    check(ml_count() == 4u && midi_set[0] == 5 && song.g[G_CH1] == 5 && song.g[G_DRCH] == midi_set[3],
          "factory sound, engine change, save / load A, section B store / recall, NEW: map and channels unchanged");
    trk[0].p[P_DIST] = 0;
    midi_in(0xB0, 74, 127);
    check(trk[0].p[P_DIST] == TP[P_DIST].max, "... and the learned CC still acts on track 1 DIST");

    /* ---- backup object 10 */
    ed((const uint8_t[]){0}, 0, ED_BK_LIST);
    {
        uint32_t n = wire_n[0], k, found = 0, len = 0;
        const uint8_t *w = wire[0];
        for (k = 7; k + 11u <= n; k += 11u)
            if (w[k] == 10u) {
                found = 1;
                len = (uint32_t)w[k + 1] | (uint32_t)w[k + 2] << 7;
            }
        check(found && len == MIDI_PART, "LIST: object 10 listed at 40 bytes");
    }
    {
        const uint8_t *o;
        uint32_t len;
        o = ed_bk_obj(10, &len);
        check(o && len == MIDI_PART && !memcmp(o, mp, MIDI_PART), "object 10 holds the actual map and channels");
    }
    memcpy(raw, mp, sizeof raw);
    fail_prog = 1;
    check(midi_restore(raw, 36) == 4u && ml_count() == 4u && midi_set[0] == 5, "PUT 10, flash fails: rc 4, nothing changed");
    fail_prog = 0;
    check(midi_restore(raw, 39) == 2u && midi_restore(raw, 0) == 2u && ml_count() == 4u, "PUT 10 of 39 / 0 bytes: rc 2, nothing changed");
    memset(raw, 0, sizeof raw);
    raw[32] = 1, raw[33] = 2, raw[34] = 3, raw[35] = 10, raw[36] = 2;   /* the defaults */
    check(midi_restore(raw, MIDI_PART) == 0u && ml_count() == 0u && midi_set[0] == 1 && song.g[G_CH1] == 1 &&
          settings_len() == sizeof(persist_t) + sizeof(dsu_bank_t) + MIDI_PART, "PUT 10 of the defaults: a customised FM-1 reset, written");
    raw[0] = 74, raw[1] = (uint8_t)(ml_code(P_DIST) << 1);          /* CC 74 -> track 1 DIST */
    raw[2] = 64, raw[3] = (uint8_t)(ml_code(P_LEVEL) << 1);         /* sustain: never learnable */
    raw[4] = 30, raw[5] = 100u << 1;                               /* code 100: no such parameter */
    raw[32] = 17, raw[33] = 7, raw[34] = 16, raw[35] = 0x7F | 0x80, raw[36] = 13;
    check(midi_restore(raw, MIDI_PART) == 0u && ml_count() == 1u && midi_set[0] == 1 && midi_set[1] == 7 &&
          midi_set[2] == 16 && midi_set[3] == 10 && midi_set[4] == 1 && midi_set[5] == 2,
          "PUT 10 sanitised: bad entries dropped, channel 17 / drums 31 -> defaults, OTHER bit 7, BEND 13 -> 2");
    check(midi_restore(raw, 32) == 0u && ml_count() == 1u && midi_set[1] == 2 && midi_set[4] == 0 && midi_set[5] == 2,
          "PUT 10 of 32 bytes: the map, every channel and BEND at its default");
    {
        persist_t p;
        uint32_t rc;
        persist_fill(&p);
        midi_part_fill(mp);
        rc = settings_restore(&p, sizeof p);
        check(rc == 0u && (midi_part_fill(raw), !memcmp(mp, raw, sizeof mp)) &&
                  settings_len() == sizeof(persist_t) + sizeof(dsu_bank_t) + MIDI_PART, "object 1 restore keeps the MIDI part");
    }

    /* ---- old settings objects: no MIDI part, 32 and 36 bytes */
    {
        persist_t p;
        uint8_t part[MIDI_PART];
        persist_fill(&p);
        midi_part_fill(part);
        st_save2(OBJ_SETTINGS, &p, sizeof p, &dsu, sizeof dsu);
        reset_midi_ram();
        dsu_boot();
        check(ml_count() == 0u && midi_set[0] == 1 && midi_set[3] == 10 && midi_set[5] == 2 && drch_seed == 1u,
              "an object without the part (0.6): defaults, the drum channel to seed once from an old autosave");
        st_save3(OBJ_SETTINGS, &p, sizeof p, &dsu, sizeof dsu, part, 32);
        reset_midi_ram();
        dsu_boot();
        check(ml_count() == 1u && midi_set[0] == 1 && midi_set[5] == 2 && !drch_seed, "a 32-byte part: the map, channels default");
        part[32] = 9;
        st_save3(OBJ_SETTINGS, &p, sizeof p, &dsu, sizeof dsu, part, 36);
        reset_midi_ram();
        dsu_boot();
        check(ml_count() == 1u && midi_set[0] == 9 && midi_set[5] == 2, "a 36-byte part: the map and channels, BEND default");
        st_save3(OBJ_SETTINGS, &p, sizeof p, &dsu, sizeof dsu, part, 37);
        reset_midi_ram();
        dsu_boot();
        check(ml_count() == 0u && midi_set[0] == 1, "a part of another length: not read (defaults)");
    }
    printf("%s\n", fails ? "MIDI LEARN TEST FAILED" : "midi learn test passed");
    return fails;
}
