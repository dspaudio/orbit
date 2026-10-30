/* SPDX-License-Identifier: GPL-3.0-only */
/* ORBIT BLE bridge integration; panel/audio/USB are real firmware.
 * The external BLE main-loop API is the only MIDI transport double. */
#define ORBIT_BLE 1
#include "../firmware/src/ble/ble_api.h"
static ble_status_t test_status = BLE_STATUS_OFF;
static uint32_t sent[512], nsent, isr_calls, test_isr;
static uint32_t received[128], rx_r, rx_w, rx_drops;
void ble_service(void) {}
ble_status_t ble_get_status(void) { return test_status; }
void ble_user_start(void) { test_status = BLE_STATUS_ADVERTISING; }
void ble_user_stop(void) { test_status = BLE_STATUS_OFF; }
const char *ble_last_error(uint32_t *code) { *code = 0; return "NO RF TRIM IN FLASH"; }
uint32_t ble_midi_rx_drops(void) { return rx_drops; }
void ble_midi_out_pkt(uint32_t pkt)
{
    if (test_isr) {
        isr_calls++;
        return;
    }
    if (nsent < 512u)
        sent[nsent++] = pkt;
}
int ble_midi_take(uint32_t *pkt)
{
    if (rx_r == rx_w)
        return 0;
    *pkt = received[rx_r++ % 128u];
    return 1;
}
#define UI_PAGES_HARNESS_ONLY
#include "ui_pages_test.c"

/* For the pre-integration snapshot only: these hooks did not exist. This
 * models their actual absence, not a replacement implementation of a queue. */
#ifdef BLE_BRIDGE_BASELINE
static uint32_t ble_out_drops;
static void orbit_ble_bridge_service(void) {}
#endif
#define NOTE_ON(n) (0x09u | 0x90u << 8 | (uint32_t)(n) << 16 | 100u << 24)
#define NOTE_OFF(n) (0x08u | 0x80u << 8 | (uint32_t)(n) << 16)
#define CC(c, v) (0x0Bu | 0xB0u << 8 | (uint32_t)(c) << 16 | (uint32_t)(v) << 24)

static void audio_pass(void)
{
    static int32_t out[CTL * 2];
    test_isr = 1;
    mix_block(out, CTL);
    test_isr = 0;
}
static void send_isr(uint32_t pkt)
{
    test_isr = 1;
    midi_out_event(pkt);
    test_isr = 0;
}
static void clear_output(void)
{
    orbit_ble_bridge_service();
    mo_r = mo_w;
    nsent = isr_calls = 0;
}
static int sounding(uint32_t n)
{
    uint32_t k;
    for (k = 0; k < NVOICE; k++)
        if (trk[0].v[k].active && trk[0].v[k].gate && trk[0].v[k].note == n)
            return 1;
    return 0;
}
static int same_output(uint32_t n)
{
    uint32_t k;
    if (nsent != n || mo_w - mo_r != n || isr_calls)
        return 0;
    for (k = 0; k < n; k++)
        if (sent[k] != midi_out_q[(mo_r + k) % MQ])
            return 0;
    return 1;
}
int main(void)
{
    uint32_t k, drop0;
    host_tracks_init();
    host_preset(&trk[0], 0, 0);
    trk[0].p[P_VOICE] = V_POLY;                    /* two simultaneous disconnect fixtures, no mono stealing */
    trk[0].p[P_AMODE] = 0;
    panel_init();
    settings_init();
    song.g[G_ROUTE] = 0;
    song.g[G_CH1] = 1;
    usb.config = 1;
    test_status = BLE_STATUS_CONNECTED;
    orbit_ble_bridge_service();
    ctrl_set(1);
#ifndef BLE_BRIDGE_BASELINE
    outdir = getenv("BLE_SCREEN_DIR");
    if (outdir) {
        static const char *const names[] = {"off", "starting", "advertising", "connected", "stopping", "failed"};
        for (k = 0; k < 6u; k++) {
            char name[48];
            test_status = (ble_status_t)k;
            ui.menu = 0;
            ui.force = 1;
            ui_draw();
            snprintf(name, sizeof name, "ble-controller-%s", names[k]);
            ppm(name);
            ui.menu = 1;
            ui.menu_sel = MI_BLE;
            ui.force = 1;
            ui_draw();
            snprintf(name, sizeof name, "ble-menu-%s", names[k]);
            ppm(name);
        }
        ui.menu = 0;
        test_status = BLE_STATUS_CONNECTED;
    }
#endif
    clear_output();

    test_isr = 1;
    ctrl_key(7, 1);
    ctrl_key(7, 0);
    test_isr = 0;
    orbit_ble_bridge_service();
    check(same_output(2) && sent[0] == NOTE_ON(60) && sent[1] == NOTE_OFF(60),
          "BLE controller keys: deferred, USB bytes and order");

    clear_output();
    encs[panel.enc[EN_K2]] = panel.dir[EN_K2];
    ctrl_input(0);
    audio_pass();
    orbit_ble_bridge_service();
    check(same_output(1) && sent[0] == CC(74, ctrl_val[1]) && ctrl_val[1],
          "BLE controller knob: deferred, same CC bytes as USB");

    clear_output();
    fm1_in.buttons = BT(B_FX);
    ctrl_input(0);
    audio_pass();
    fm1_in.buttons = 0;
    ctrl_input(0);
    audio_pass();
    orbit_ble_bridge_service();
    check(same_output(2) && sent[0] == CC(80, 127) && sent[1] == CC(80, 0),
          "BLE controller switch: press/release order and USB bytes");

    clear_output();
    for (k = 0; k < 20u; k++)
        send_isr(NOTE_ON(40u + k));
    k = nsent;
    orbit_ble_bridge_service();
    check(!k && same_output(20) && sent[0] == NOTE_ON(40) && sent[19] == NOTE_ON(59),
          "BLE audio ISR: no main-loop calls while idle, later ordered drain");

    clear_output();
    drop0 = ble_out_drops;
    for (k = 0; k < 70u; k++)
        send_isr(CC(7, k));
    orbit_ble_bridge_service();
    check(nsent == 64u && ble_out_drops - drop0 == 6u && sent[0] == CC(7, 0) && sent[63] == CC(7, 63),
          "BLE output overflow: drop newest, retain oldest 64 in order");

    clear_output();
    for (k = 0; k < 20u; k++)
        send_isr(0xF80Fu);
    orbit_ble_bridge_service();
    check(nsent == 8u && ble_out_drops - drop0 == 18u,
          "BLE clock backlog: eight clocks maximum");

    clear_output();
    send_isr(NOTE_ON(60));
    test_status = BLE_STATUS_ADVERTISING;
    orbit_ble_bridge_service();
    test_status = BLE_STATUS_CONNECTED;
    orbit_ble_bridge_service();
    send_isr(NOTE_ON(61));
    orbit_ble_bridge_service();
    check(nsent == 1u && sent[0] == NOTE_ON(61),
          "BLE disconnect: stale output flushed before reconnect");

    ctrl_set(0);
    audio_pass();
    clear_output();
    midi_in_event(NOTE_ON(65));
    audio_pass();
    k = sounding(65);
    midi_in_event(NOTE_OFF(65));
    audio_pass();
    received[rx_w++ % 128u] = NOTE_ON(65);
    orbit_ble_bridge_service();
    audio_pass();
    check(k && sounding(65) && (mi_trk_held[0][2] & 2u),
          "BLE input: Note On reaches the USB voice/held-note path");

    received[rx_w++ % 128u] = CC(74, 100);
    orbit_ble_bridge_service();
    audio_pass();
    k = (uint32_t)trk[0].p[P_TFLT];
    midi_in_event(CC(74, 100));
    audio_pass();
    check(rx_r == rx_w && k == (uint32_t)trk[0].p[P_TFLT] && k != 0u,
          "BLE input: CC uses the same parameter mapping as USB");

    received[rx_w++ % 128u] = CC(64, 127);
    received[rx_w++ % 128u] = NOTE_OFF(65);
    received[rx_w++ % 128u] = NOTE_ON(67);
    orbit_ble_bridge_service();
    audio_pass();
    k = sounding(65) && sounding(67) && sus_on[0];
    test_status = BLE_STATUS_ADVERTISING;
    orbit_ble_bridge_service();
    audio_pass();
    check(k && !sounding(65) && !sounding(67) && !sus_on[0] && !ch_sus[0],
          "BLE disconnect: pedal-held and key-held notes both released");

    test_status = BLE_STATUS_CONNECTED;
    orbit_ble_bridge_service();
    received[rx_w++ % 128u] = NOTE_ON(69);
    orbit_ble_bridge_service();
    audio_pass();
    k = sounding(69);
    rx_drops++;
    orbit_ble_bridge_service();
    audio_pass();
    check(k && !sounding(69), "BLE RX overflow: held notes recovered");

    received[rx_w++ % 128u] = CC(64, 127);
    received[rx_w++ % 128u] = NOTE_ON(70);
    received[rx_w++ % 128u] = NOTE_OFF(70);
    orbit_ble_bridge_service();
    audio_pass();
    k = sounding(70) && sus_on[0];
    while (mi_w - mi_r < MQ - 8u)
        midi_in_event(CC(10, 64));
    received[rx_w++ % 128u] = CC(64, 0);
    orbit_ble_bridge_service();                     /* core ring has no BLE room: release remains owed */
    audio_pass();
    orbit_ble_bridge_service();                     /* room now exists for the synthetic pedal-up */
    audio_pass();
    check(k && !sounding(70) && !sus_on[0] && !ch_sus[0],
          "BLE core RX ring full: dropped pedal-up is retried safely");
    printf("ble_bridge: %d failed\n", fails);
    return fails != 0;
}
