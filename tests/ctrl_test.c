/* SPDX-License-Identifier: GPL-3.0-only */
/* ORBIT MIDI CONTROLLER mode on the real firmware stack (UI, sequencer, storage.c on a simulated NOR, project.c;
 * only the panel, the flash chip and USB are doubles):
 *   combo     ENV + LFO held 2 s turns it on (not before), held again 2 s turns it off; the menu entry toggles it
 *   keys      Note On / Off on the controller channel (key velocity, OCT), no internal voice, nothing recorded
 *   knobs     KNOB 1..4 send their CC with a changing value; EDIT + knob sets the CC number, EDIT + SELECT the channel
 *   switches  FX / SEL / ENV / LFO send 127 on press, 0 on release
 *   sequencer keeps playing while the mode is on
 *   exit      held notes and held switches are released; the keys play the synth again
 *   persist   the CC numbers and the channel survive the settings save / boot; older settings objects give defaults
 * Exit status: the number of failed checks. */
#define FELUCCA_ARRANGER 1
#define FELUCCA_FLASH 1
#define FELUCCA_OTA 0
#define FELUCCA_VERSION "ORBIT TEST"
#include <stdint.h>
static uint32_t host_slots[4u * 0x14000u / 4u];
#define SMP_USER_XIP(k) ((const uint8_t *)host_slots + (k) * SMP_USER_SIZE)
#define FM6_BANK_XIP(copy) (nor + st_sector(OBJ_FM6BANK, copy) + ST_PAYLOAD_OFF)
#define ST_XIP_PTR(off) (nor + (off))
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
static int16_t scope_buf[SCOPE_N];
static uint32_t scope_w;
#include "../firmware/src/panel.c"
#include "../firmware/src/ui.c"
#include "../firmware/src/ui_song.c"
#include "../firmware/src/ui_studio.c"
#include "../firmware/src/icons.c"
#include "../firmware/src/ui_draw.c"
#include "../firmware/src/ui_layers.c"
#include "../firmware/src/ui_menu.c"
#include "../firmware/src/ui_input.c"

/* ---- the flash chip: a NOR image (erase -> 0xFF, a program clears bits) */
static uint8_t flash_ok;
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off) { memset(nor + off, 0xFF, 4096); return 0; }
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    uint32_t i;
    for (i = 0; i < n; i++)
        nor[off + i] &= s[i];
    return 0;
}
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static uint32_t fl_jedec_host(void) { return 0x856014u; }
#define fl_jedec_ram fl_jedec_host
#define FL_FAR(f) f
#define FL_JEDEC_OK(id) ((id) == 0x856014u)
static void fl_plain_window_init(void) {}
static int fl_write(uint32_t off, const void *p, uint32_t n) { return st_prog(off, p, n); }
static int fl_erase4k_quiet(uint32_t off, uint32_t *t) { (void)t; return st_erase(off); }
static void fl_inval(uint32_t a, uint32_t n) { (void)a; (void)n; }
#include "../firmware/src/storage.c"
#include "../firmware/src/upreset.c"
#include "../firmware/src/project.c"

static int fails;
static void check(int ok, const char *what)
{
    printf("ctrl: %-96s %s\n", what, ok ? "ok" : "FAIL");
    fails += !ok;
}

/* ---- USB MIDI OUT: every packet the firmware queued */
static uint32_t outp[512], nout;
static void drain(void)
{
    while (mo_r != mo_w) {
        if (nout < 512u)
            outp[nout++] = midi_out_q[mo_r % MQ];
        mo_r++;
    }
}
static void clear_out(void) { drain(); nout = 0; }
static uint32_t count_pkt(uint32_t pkt)
{
    uint32_t i, n = 0;
    for (i = 0; i < nout; i++)
        n += outp[i] == pkt;
    return n;
}
static uint32_t count_cc_num(uint32_t cc)             /* CC packets (any channel, any value) for one number */
{
    uint32_t i, n = 0;
    for (i = 0; i < nout; i++)
        n += (outp[i] & 0xFFu) == 0x0Bu && ((outp[i] >> 12) & 15u) == 0xBu && ((outp[i] >> 16) & 127u) == cc;
    return n;
}
static int32_t last_cc(uint32_t ch, uint32_t cc)      /* the last value sent for a CC on channel ch, -1 none */
{
    int32_t v = -1;
    uint32_t i;
    for (i = 0; i < nout; i++)
        if ((outp[i] & 0xFFFFu) == (0x0Bu | (0xB0u | ch) << 8) && ((outp[i] >> 16) & 127u) == cc)
            v = (int32_t)(outp[i] >> 24);
    return v;
}
#define NOTE_ON(ch, n, v) (0x09u | (0x90u | (ch)) << 8 | (uint32_t)(n) << 16 | (uint32_t)(v) << 24)
#define NOTE_OFF(ch, n) (0x08u | (0x80u | (ch)) << 8 | (uint32_t)(n) << 16)
#define CC(ch, c, v) (0x0Bu | (0xB0u | (ch)) << 8 | (uint32_t)(c) << 16 | (uint32_t)(v) << 24)

static uint32_t starts;                               /* internal voices started (a voice's note-on: active + gate) */
static uint8_t was_on[NTRK][NVOICE];
static void count_starts(void)
{
    uint32_t t, k;
    for (t = 0; t < NTRK; t++)
        for (k = 0; k < NVOICE; k++) {
            uint8_t on = (uint8_t)(trk[t].v[k].active && trk[t].v[k].gate);
            starts += on && !was_on[t][k];
            was_on[t][k] = on;
        }
}
static void frame(void)
{
    uint32_t q;
    static int32_t o[CTL * 2];
    ui_input(); ui_leds(); ui_draw();
    for (q = 0; q < 22u; q++) {                       /* (the audio after the UI pass: its queued CCs go out) */
        mix_block(o, CTL);
        count_starts();
    }
    fm1_ms += 16;
    drain();
}
static void frames(uint32_t n) { while (n--) frame(); }
static uint32_t bit(uint32_t label) { return 1u << panel.btn[label]; }
static void press(uint32_t m) { fm1_in.buttons |= m; edges_btn |= m; }
static void release(uint32_t m) { fm1_in.buttons &= ~m; }
static void turn(uint32_t role, int32_t s) { encs[panel.enc[role]] += s * panel.dir[role]; }
static int sounding(const track_t *t, uint32_t note)
{
    uint32_t k;
    for (k = 0; k < NVOICE; k++)
        if (t->v[k].active && t->v[k].note == note && t->v[k].gate)
            return 1;
    return 0;
}
static uint32_t settings_len(void)
{
    st_hdr_t h;
    return st_current(OBJ_SETTINGS, &h) >= 0 ? h.len : 0u;
}
static void boot(void)                                /* the boot path of felucca.c: persist_boot, then the settings */
{
    persist_boot();
    settings_init();
    panel_init();
    song.g[G_CH1] = midi_set[0], song.g[G_CH2] = midi_set[1], song.g[G_CH3] = midi_set[2];
    song.g[G_DRCH] = midi_set[3], song.g[G_CHOTH] = midi_set[4], song.g[G_BEND] = midi_set[5];
}
/* hold ENV + LFO; returns the ms held when the mode changed (0: it did not within limit_ms) */
static uint32_t combo_hold(uint32_t limit_ms)
{
    uint32_t was = ctrl_on, t0 = fm1_ms;
    press(bit(B_ENV) | bit(B_LFO));
    while (fm1_ms - t0 <= limit_ms) {
        uint32_t at = fm1_ms - t0;                    /* (ui_input in this frame sees fm1_ms = t0 + at) */
        frame();
        if (ctrl_on != was)
            return at;
    }
    return 0;
}

int main(void)
{
    static const uint8_t SEQN = 48;
    uint32_t i, held, n60;
    memset(nor, 0xFF, sizeof nor);
    host_tracks_init();
    arr_defaults(&arrangement);
    boot();
    for (i = 0; i < NTRK; i++)
        track_defaults(&trk[i]);
    layers_init();
    go_home();
    ui.force = 1;
    usb.config = 1;                                   /* (USB configured: midi_out_event queues) */
    frames(4);
    clear_out();
    check(!ctrl_on && ctrl_cfg.ch == 0u && ctrl_cfg.cc[0] == 7u && ctrl_cfg.cc[1] == 74u && ctrl_cfg.cc[2] == 71u &&
          ctrl_cfg.cc[3] == 64u && ctrl_cfg.sw[0] == 80u && ctrl_cfg.sw[3] == 83u,
          "boot: off; CH 1, KNOB CCs 7 74 71 64, switch CCs 80..83");

    /* ---- normal keys play the synth */
    fm1_in.notes = 1u << 7;
    frames(2);
    check(sounding(&trk[0], kb_map(&trk[0], 7)), "mode off: a key plays track 1");
    fm1_in.notes = 0;
    frames(30);

    /* ---- the combo: ENV + LFO held 2 s */
    held = combo_hold(2400u);
    check(ctrl_on && held >= 2000u && held < 2032u, "ENV + LFO held: on at 2.0 s, not before");
    release(bit(B_ENV) | bit(B_LFO));
    frames(2);
    check(count_cc_num(82u) == 0u && count_cc_num(83u) == 0u, "the combo's own ENV / LFO hold sends no switch CC");
    clear_out();

    /* ---- keys: Note On / Off on CH 1, no voice */
    starts = 0;
    fm1_in.notes = 1u << 7;                           /* (key 7: C4 = 60 at OCT 0) */
    frames(2);
    check(count_pkt(NOTE_ON(0u, 60u, 100u)) == 1u, "key down: USB 09 90 3C 64 (note 60, velocity 100, CH 1)");
    check(starts == 0u && !sounding(&trk[0], kb_map(&trk[0], 7)), "key down: no internal voice started");
    fm1_in.notes = 0;
    frames(2);
    check(count_pkt(NOTE_OFF(0u, 60u)) == 1u, "key up: USB 08 80 3C 00");
    check(!(trk[0].step[0].n) && !(trk[0].step[1].n), "nothing recorded into track 1");
    press(bit(B_OCTUP));
    frames(1);
    release(bit(B_OCTUP));
    fm1_in.notes = 1u << 7;
    frames(2);
    fm1_in.notes = 0;
    frames(1);
    check(song.octave == 1 && count_pkt(NOTE_ON(0u, 72u, 100u)) == 1u && count_pkt(NOTE_OFF(0u, 72u)) == 1u,
          "OCT+ transposes the controller keys (note 72)");
    press(bit(B_OCTDN));
    frames(1);
    release(bit(B_OCTDN));
    frames(1);
    check(song.octave == 0, "OCT- back to 0");
    clear_out();

    /* ---- knobs */
    turn(EN_K2, 1);
    frames(1);
    {
        int32_t v1 = last_cc(0u, 74u), v2;
        frames(8);                                    /* (slow turns: no acceleration) */
        turn(EN_K2, 2);
        frames(1);
        v2 = last_cc(0u, 74u);
        check(v1 == 1 && v2 == 3 && ctrl_val[1] == 3u, "KNOB 2: CC 74 sent, value 1 then 3 (absolute, per knob)");
    }
    frames(8);
    clear_out();
    press(bit(B_EDIT));
    frames(1);
    turn(EN_K1, 3);
    frames(1);
    release(bit(B_EDIT));
    frames(8);
    check(ctrl_cfg.cc[0] == 10u && nout == 0u, "EDIT + KNOB 1: CC number 7 -> 10, nothing sent");
    turn(EN_K1, 1);
    frames(1);
    check(last_cc(0u, 10u) == 1, "KNOB 1 now sends CC 10");
    clear_out();

    /* ---- switches */
    press(bit(B_FX));
    frames(1);
    check(count_pkt(CC(0u, 80u, 127u)) == 1u && punch.req < 0, "FX press: CC 80 = 127 (no punch-in)");
    release(bit(B_FX));
    frames(1);
    check(count_pkt(CC(0u, 80u, 0u)) == 1u, "FX release: CC 80 = 0");
    clear_out();

    /* ---- channel: EDIT + SELECT */
    {
        int16_t bpm = song.g[G_BPM];
        press(bit(B_EDIT));
        frames(1);
        turn(EN_SELECT, 1);
        frames(1);
        release(bit(B_EDIT));
        frames(8);
        check(ctrl_cfg.ch == 1u && song.g[G_BPM] == bpm, "EDIT + SELECT: channel 2, the tempo unchanged");
    }
    fm1_in.notes = 1u << 7;
    frames(1);
    fm1_in.notes = 0;
    frames(1);
    check(count_pkt(NOTE_ON(1u, 60u, 100u)) == 1u && count_pkt(NOTE_OFF(1u, 60u)) == 1u, "the keys now on CH 2 (0x91 / 0x81)");
    clear_out();

    /* ---- the sequencer keeps playing */
    for (i = 0; i < 16u; i++)
        put_step(&trk[0], i, 1u, &SEQN, ST_NOTE, 0);
    starts = 0;
    {
        uint32_t beat0 = clk_beat, idx_seen = 0, seq_sounded = 0, last_idx = trk[0].seq_idx;
        press(bit(B_PLAY));
        for (i = 0; i < 90u; i++) {
            frame();
            if (i == 0u)
                release(bit(B_PLAY));
            seq_sounded |= (uint32_t)sounding(&trk[0], SEQN);
            if (trk[0].seq_idx != last_idx)
                idx_seen++, last_idx = trk[0].seq_idx;
        }
        check(ctrl_on && song.playing && clk_beat > beat0 + 1u && idx_seen >= 4u,
              "PLAY in the mode: the transport runs, the step position advances");
        check(seq_sounded && starts > 0u, "the sequencer's notes still sound on track 1");
    }
    press(bit(B_PLAY));
    frames(1);
    release(bit(B_PLAY));
    frames(20);
    check(!song.playing, "PLAY again: stopped");
    clear_out();

    /* ---- the menu entry toggles it */
    mi_set(MI_CTRL, 0);
    frames(2);
    check(!ctrl_on, "MENU > SYSTEM > CONTROLLER: off");
    mi_set(MI_CTRL, 0);
    frames(2);
    check(ctrl_on && !ui.menu, "MENU > SYSTEM > CONTROLLER: on again (the menu closes)");
    clear_out();

    /* ---- exit with a key held */
    fm1_in.notes = 1u << 3;                           /* (key 3: note 56) */
    frames(2);
    check(count_pkt(NOTE_ON(1u, 56u, 100u)) == 1u, "key 3 held: note 56 on CH 2");
    held = combo_hold(2400u);
    check(!ctrl_on && held >= 2000u && held < 2032u, "ENV + LFO held 2 s again: off");
    check(count_pkt(NOTE_OFF(1u, 56u)) == 1u, "exit: the held note gets its note-off");
    check(count_pkt(CC(1u, 82u, 127u)) == 1u && count_pkt(CC(1u, 82u, 0u)) == 1u &&
          count_pkt(CC(1u, 83u, 127u)) == 1u && count_pkt(CC(1u, 83u, 0u)) == 1u,
          "exit: ENV / LFO (held as switches) sent 127, then 0 on exit");
    release(bit(B_ENV) | bit(B_LFO));
    fm1_in.notes = 0;
    frames(4);
    check(count_pkt(NOTE_OFF(1u, 56u)) == 1u, "key 3 let go after exit: no second note-off");
    n60 = count_pkt(NOTE_ON(1u, 60u, 100u));
    fm1_in.notes = 1u << 7;
    frames(2);
    check(sounding(&trk[0], kb_map(&trk[0], 7)) && count_pkt(NOTE_ON(1u, 60u, 100u)) == n60,
          "after exit: a key plays track 1 again, nothing on the controller channel");
    fm1_in.notes = 0;
    frames(30);

    /* ---- persistence: the CC numbers and the channel */
    settings_save();
    check(settings_len() == sizeof(persist_t) + sizeof(dsu_bank_t) + MIDI_PART,
          "settings object = persist (with the controller) + SYN kits + MIDI part");
    memset(&ctrl_cfg, 0xEE, sizeof ctrl_cfg);
    boot();
    check(ctrl_cfg.ch == 1u && ctrl_cfg.cc[0] == 10u && ctrl_cfg.cc[1] == 74u && ctrl_cfg.cc[2] == 71u &&
          ctrl_cfg.cc[3] == 64u && ctrl_cfg.sw[0] == 80u && ctrl_cfg.sw[1] == 81u && ctrl_cfg.sw[2] == 82u &&
          ctrl_cfg.sw[3] == 83u && !ctrl_on, "boot: CC 10 / CH 2 back from flash; the mode itself off");
    {   /* an older object: persist up to lights (88 bytes), SYN kits, MIDI part */
        persist_t p;
        uint8_t mp[MIDI_PART];
        uint32_t pal = settings.palette;
        persist_fill(&p);
        midi_set[0] = 5;
        midi_part_fill(mp);
        check(st_save3(OBJ_SETTINGS, &p, PERSIST_SIZE_V23, &dsu, sizeof dsu, mp, MIDI_PART) == 0 &&
              settings_len() == PERSIST_SIZE_V23 + sizeof(dsu_bank_t) + MIDI_PART, "an older settings object written (88 + kits + 40)");
        midi_set[0] = 9;
        boot();
        check(ctrl_cfg.ch == 0u && ctrl_cfg.cc[0] == 7u && ctrl_cfg.cc[1] == 74u && ctrl_cfg.sw[0] == 80u,
              "older object: the controller gets its defaults");
        check(midi_set[0] == 5u && settings.palette == pal, "older object: the MIDI part and the settings still read");
        ctrl_cfg.cc[2] = 99;
        check(settings_restore(&p, PERSIST_SIZE_V23) == 0u && ctrl_cfg.cc[2] == 99u,
              "backup object 1 of the older size restores; the controller settings stay");
        check(st_save3(OBJ_SETTINGS, &p, PERSIST_SIZE_V22, 0, 0, 0, 0) == 0, "a SLOOP 2.2 object written (no lights)");
        ctrl_cfg.cc[2] = 99;
        boot();
        check(ctrl_cfg.cc[2] == 71u && ctrl_cfg.ch == 0u, "SLOOP 2.2 object: defaults");
    }
    printf("ctrl: %s (%d failed)\n", fails ? "FAIL" : "all ok", fails);
    return fails;
}
