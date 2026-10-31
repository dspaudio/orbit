/* SPDX-License-Identifier: GPL-3.0-only */
/* ORBIT MIDI CONTROLLER mode, the panel side: the FM-1 as a USB MIDI controller while the sequencer, the arp and
 * the drums keep running. OCT- + OCT+ tapped together (or MENU > AUDIO > CONTROLLER) turns it on and off; it is
 * always off at power-on. While on:
 *   keys        Note On / Off on the controller channel (seq.c ctrl_key, in the audio context); no voice, no recording
 *   KNOB 1..4   one CC each (absolute 0..127, kept per knob; the usual knob acceleration)
 *   FX SEL ENV LFO   momentary switch CCs: 127 pressed, 0 let go
 *   EDIT + KNOB n    the CC number of KNOB n (0..127); EDIT + SELECT the channel (1..16); SELECT alone the tempo
 *   PLAY REC OCT- OCT+   as ever; HOME held opens the menu
 * The CC numbers and the channel are settings of the FM-1 (seq.c ctrl_cfg, kept in project.c persist_t.ctl).
 * Included from ui_input.c, so every build of the UI (felucca.c and the host harnesses) has it. */
#define CTRL_TAP_GAP_MS 150u                     /* OCT- and OCT+ pressed at most this far apart */
#define CTRL_TAP_MS 600u                         /* both let go within this of the first press */
static const uint8_t CTRL_SW_BTN[4] = {B_FX, B_SCL, B_ENV, B_LFO};
static uint8_t ctrl_val[4];                      /* KNOB 1..4: the value last sent */
static uint8_t ctrl_sw;                          /* bit per switch: 127 sent, 0 not yet */
static uint8_t ctrl_sw_mute;                     /* bit per switch: held as the mode came on, ignored until let go */

static uint32_t ctrl_btn(uint32_t label) { return (fm1_in.buttons >> panel.btn[label]) & 1u; }
static void ctrl_cc(uint32_t cc, uint32_t v)
{
    ctrl_push(0x0Bu | (0xB0u | (ctrl_cfg.ch & 15u)) << 8 | (cc & 127u) << 16 | (v & 127u) << 24);
}

static void ctrl_set(uint32_t on)
{
    uint32_t i;
    on = on != 0u;
    if (on == ctrl_on)
        return;
    if (on) {
        ctrl_sw = 0;
        ctrl_sw_mute = 0;
        for (i = 0; i < 4u; i++)
            if (ctrl_btn(CTRL_SW_BTN[i]))
                ctrl_sw_mute |= (uint8_t)(1u << i);
        layer_unlock();                          /* no layer under the mode */
        ui.layer = LY_PLAY;
        ui.step_held = 0;
        punch.hold = 0;
        thr_req = 0;
        kb_grid = 0;
    } else {
        for (i = 0; i < 4u; i++)                 /* a switch still held: its 0 */
            if ((ctrl_sw >> i) & 1u)
                ctrl_cc(ctrl_cfg.sw[i], 0);
        ctrl_sw = 0;
        ui_message("CONTROLLER OFF");
    }
    ctrl_on = (uint8_t)on;                       /* (seq.c ctrl_block ends the held notes as it sees it go) */
    ui.force = 1;
}

/* OCT- + OCT+ tapped together: on / off as both are let go. A tap: the two pressed within CTRL_TAP_GAP_MS, both let
 * go within CTRL_TAP_MS of the first press, no other button and no key pressed meanwhile, on the play screen (not
 * the song page). Longer holds keep their other uses (main.c: the 5 s update mode; the drum track's ghost / hard).
 * The presses still do what they always do (both together: octave 0). */
static void ctrl_oct_tap(uint32_t pressed, uint32_t notes)
{
    static uint32_t t0;
    static uint8_t armed, both;
    uint32_t oct = (1u << panel.btn[B_OCTDN]) | (1u << panel.btn[B_OCTUP]), down = fm1_in.buttons & oct;
    if (!armed) {
        if (!(pressed & oct) || down != (pressed & oct) || (fm1_in.buttons & ~oct) || notes ||
            ui.layer != LY_PLAY || on_song_page())
            return;
        armed = 1;
        both = (uint8_t)(down == oct);
        t0 = fm1_ms;
        return;
    }
    if ((fm1_in.buttons & ~oct) || (pressed & ~oct) || notes || fm1_ms - t0 > CTRL_TAP_MS) {
        armed = 0;
        return;
    }
    if (!both) {
        if (down == oct && fm1_ms - t0 <= CTRL_TAP_GAP_MS)
            both = 1;
        else if (!down || fm1_ms - t0 > CTRL_TAP_GAP_MS)
            armed = 0;
        return;
    }
    if (!down) {
        armed = 0;
        ctrl_set(!ctrl_on);
    }
}

static void ctrl_input(uint32_t pressed)
{
    uint32_t i, edit = ctrl_btn(B_EDIT), oct = (1u << panel.btn[B_OCTDN]) | (1u << panel.btn[B_OCTUP]);
    int32_t s;
    for (i = 0; i < 4u; i++) {
        uint32_t b = 1u << i, d = ctrl_btn(CTRL_SW_BTN[i]);
        if (ctrl_sw_mute & b) {
            if (!d)
                ctrl_sw_mute &= (uint8_t)~b;
            continue;
        }
        if (d != ((ctrl_sw & b) != 0u)) {
            ctrl_sw ^= (uint8_t)b;
            ctrl_cc(ctrl_cfg.sw[i], d ? 127u : 0u);
        }
    }
    for (i = 0; i < 4u; i++) {
        if ((s = panel_enc(EN_K1 + i)) == 0)
            continue;
        s = accel(EN_K1 + i, s, 127);
        if (edit) {
            ctrl_cfg.cc[i] = (uint8_t)clamp((int32_t)ctrl_cfg.cc[i] + s, 0, 127);
            settings_later = 1;                  /* (saved when quiet: project.c) */
        } else {
            int32_t v = clamp((int32_t)ctrl_val[i] + s, 0, 127);
            if (v != (int32_t)ctrl_val[i]) {
                ctrl_val[i] = (uint8_t)v;
                ctrl_cc(ctrl_cfg.cc[i], (uint32_t)v);
            }
        }
    }
    if ((s = panel_enc(EN_SELECT)) != 0) {
        if (edit) {
            ctrl_cfg.ch = (uint8_t)clamp((int32_t)ctrl_cfg.ch + (s > 0 ? 1 : -1), 0, 15);
            settings_later = 1;
        } else {
            song.g[G_BPM] = (int16_t)clamp(song.g[G_BPM] + accel(EN_SELECT, s, 200), GP[G_BPM].min, GP[G_BPM].max);
            ui.bpm_t = 40;
        }
    }
    panel_enc(EN_ALGO);                          /* (no track or sound changes behind the screen) */
    panel_enc(EN_PRESET);
    holds_input(pressed, fm1_ms);                /* REC: a tap arms / records, a hold clears the track */
    if (pressed & (1u << panel.btn[B_PLAY])) {
#if FELUCCA_ARRANGER
        if (!song.playing && arrangement_enabled && !arr_valid(&arrangement, arrangement_ready()))
            ui_message("EMPTY SECTION: REC");
        else
#endif
        if (!ft_owns_press())
            transport_req = song.playing ? 2 : 1;
    }
    if (pressed & oct) {                         /* OCT- / OCT+: the keys' octave (both: 0), on any track */
        if ((fm1_in.buttons & oct) == oct)
            song.octave = 0;
        else if (pressed & (1u << panel.btn[B_OCTDN]))
            song.octave = (int8_t)(song.octave > -3 ? song.octave - 1 : -3);
        else
            song.octave = (int8_t)(song.octave < 3 ? song.octave + 1 : 3);
    }
}

/* the last message, as text: "CC 74 = 100", "ON 60 V100", "OFF 60" */
static void ctrl_msg(char *b, uint32_t n, uint32_t pkt)
{
    uint32_t st = (pkt >> 8) & 0xF0u, d1 = (pkt >> 16) & 127u, d2 = (pkt >> 24) & 127u;
    str_cpy(b, st == 0xB0u ? "CC " : st == 0x90u ? "ON " : "OFF ", n);
    fmt_int(b + str_len(b), (int32_t)d1);
    if (st == 0xB0u)
        str_cpy(b + str_len(b), " = ", n - str_len(b));
    else if (st == 0x90u)
        str_cpy(b + str_len(b), " V", n - str_len(b));
    if (st != 0x80u)
        fmt_int(b + str_len(b), (int32_t)d2);
}

/* the screen: the header of the live screens, CONTROLLER and the channel, the four knobs (CC, value), the last
 * message; three strips, each redrawn when its signature changes */
static void ctrl_draw(void)
{
    static uint32_t head, sig_top, sig_k, sig_foot;
    uint32_t i, edit = ctrl_btn(B_EDIT), cnt = ctrl_count, last = ctrl_last, sig;
    char b[24];
    if (!ctrl_shown) {
        lcd_fill(0, 0, 240, 240, C_BLACK);
        ui.force = 1;
    }
    ctrl_shown = 1;
    te_header("midi", TE_G3, &head);
    sig = 7u + ctrl_cfg.ch * 31u + edit * 977u;
#if ORBIT_BLE
    sig = sig * 131u + (uint32_t)ble_get_status();
#endif
    if (ui.force || sig != sig_top) {
        sig_top = sig;
        cv_begin(240, 40, C_BLACK);
#if ORBIT_BLE
        cv_text(4, 0, &FONT_L, "CONTROLLER", C_WHITE);
#else
        cv_text(4, 6, &FONT_L, "CONTROLLER", C_WHITE);
#endif
        str_cpy(b, "CH ", sizeof b);
        fmt_int(b + 3, (int32_t)ctrl_cfg.ch + 1);
#if ORBIT_BLE
        cv_text(236 - text_w(&FONT_L, b), 0, &FONT_L, b, edit ? C_WHITE : TE_COL[0]);
        cv_text(4, 26, &FONT_S, orbit_ble_status(), TE_G3);
#else
        cv_text(236 - text_w(&FONT_L, b), 6, &FONT_L, b, edit ? C_WHITE : TE_COL[0]);
#endif
        cv_rect(0, 39, 240, 1, TE_G1);
        cv_blit(0, 40);
    }
    sig = 11u + edit * 7u;
    for (i = 0; i < 4u; i++)
        sig = sig * 131u + ctrl_cfg.cc[i] * 257u + ctrl_val[i];
    if (ui.force || sig != sig_k) {
        sig_k = sig;
        cv_begin(240, 120, C_BLACK);
        for (i = 0; i < 4u; i++) {
            int32_t y = (int32_t)i * 30, w = (int32_t)ctrl_val[i] * 96 / 127;
            char k[3] = {'K', (char)('1' + i), 0};
            cv_rect(4, y + 6, 4, 20, TE_COL[i]);
            cv_text(14, y + 8, &FONT_S, k, TE_COL[i]);
            str_cpy(b, "CC ", sizeof b);
            fmt_int(b + 3, ctrl_cfg.cc[i]);
            cv_text(44, y + 8, &FONT_S, b, edit ? C_WHITE : TE_G4);
            cv_rect(112, y + 13, 96, 6, TE_G2);
            cv_rect(112, y + 13, w, 6, TE_COL[i]);
            fmt_int(b, ctrl_val[i]);
            cv_text(236 - text_w(&FONT_S, b), y + 8, &FONT_S, b, C_WHITE);
        }
        cv_blit(0, 80);
    }
    sig = cnt * 2654435761u + last;
    if (ui.force || sig != sig_foot) {
        sig_foot = sig;
        cv_begin(240, 40, C_BLACK);
        cv_rect(0, 0, 240, 1, TE_G1);
        cv_text(4, 4, &FONT_S, "LAST", TE_G3);
        if (cnt) {
            ctrl_msg(b, sizeof b, last);
            cv_text(52, 4, &FONT_S, b, C_WHITE);
        } else {
            cv_text(52, 4, &FONT_S, "-", TE_G3);
        }
        cv_text(4, 22, &FONT_S, "EDIT+KNOB: CC  EDIT+SEL: CH", TE_G3);
        cv_blit(0, 200);
    }
}
