/* SPDX-License-Identifier: GPL-3.0-only */
/* Live ownership and lost MIDI note-offs, using the real input and sequencer. */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int gated(track_t *t, uint32_t note)
{
    uint32_t i;
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].active && t->v[i].note == note && t->v[i].gate)
            return 1;
    return 0;
}

static void init(void)
{
    uint32_t i;
    host_tracks_init();
    for (i = 0; i < NPART; i++) {
        host_preset(&trk[i], 0, 7);
        trk[i].p[P_VOICE] = V_POLY;
        trk[i].p[P_AMODE] = 0;
        trk[i].p[P_STRUM] = 0;
        trk[i].p[P_SLEN] = 4;
    }
    events_block(CTL);                           /* establish the MIDI routing snapshot */
}

static int record_loop(void)
{
    track_t *t = &trk[0];
    uint32_t i, age, ok = 1;
    put_step(t, 1, 1, (const uint8_t[]){67}, ST_NOTE, 0);
    song.rec = 1;
    seq_start();
    input_on(t, 48, 100);
    age = vage;
    for (i = 0; i < 2u * FS / CTL; i++) {
        events_block(CTL);                       /* two complete four-step loops */
        ok &= gated(t, 48);
    }
    /* The melody can retrigger; the held bass must retain its original voice age. */
    {
        uint32_t v, same = 0;
        for (v = 0; v < NVOICE; v++)
            same |= t->v[v].note == 48 && t->v[v].gate && t->v[v].age == age;
        ok &= same;
    }
    input_off(t, 48);
    return ok && !gated(t, 48);
}

static int gate_overlap(void)
{
    track_t *t = &trk[0];
    put_step(t, 0, 1, (const uint8_t[]){48}, ST_NOTE, 0);
    song.playing = 1;
    song.rec = 1;
    seq_step(t, &t->step[0], BEAT_U / 4u, 0);
    input_on(t, 48, 100);
    seq_release(t);
    {
        int held = gated(t, 48);
        input_off(t, 48);
        return held && !gated(t, 48);
    }
}

static int key_before_gate(void)
{
    track_t *t = &trk[0];
    int held;
    put_step(t, 0, 1, (const uint8_t[]){48}, ST_NOTE, 0);
    song.playing = 1;
    song.rec = 1;
    seq_step(t, &t->step[0], BEAT_U / 4u, 0);
    input_on(t, 48, 100);
    input_off(t, 48);
    held = gated(t, 48);
    seq_release(t);
    return held && !gated(t, 48);
}

static int slide_overlap(void)
{
    track_t *t = &trk[0];
    int held;
    put_step(t, 0, 1, (const uint8_t[]){48}, ST_NOTE, SF_SLIDE);
    put_step(t, 1, 1, (const uint8_t[]){67}, ST_NOTE, 0);
    song.playing = 1;
    song.rec = 1;
    seq_step(t, &t->step[0], BEAT_U / 4u, 0);
    input_on(t, 48, 100);
    t->seq_idx = 1;
    seq_step(t, &t->step[1], BEAT_U / 4u, 0);
    held = gated(t, 48) && gated(t, 67);
    input_off(t, 48);
    seq_release(t);
    return held && !gated(t, 48) && !gated(t, 67);
}

static int ratchet_overlap(void)
{
    track_t *t = &trk[0];
    uint32_t age;
    int held;
    song.playing = 1;
    song.rec = 1;
    input_on(t, 48, 100);
    t->step[0].rat = 3;
    age = vage;
    seq_step(t, &t->step[0], BEAT_U / 4u, 0);
    seq_ratchets(t, BEAT_U / 8u, BEAT_U / 4u);
    seq_release(t);
    held = gated(t, 48) && vage == age;
    input_off(t, 48);
    return held && !gated(t, 48);
}

static void message(uint32_t st, uint32_t note, uint32_t vel, int uart)
{
    if (uart) {
        um_byte(st);
        um_byte(note);
        um_byte(vel);
    } else {
        midi_in_event((st >> 4) | st << 8 | note << 16 | vel << 24);
    }
}

static int overflow(int uart, uint32_t status, int arp, int pedal)
{
    uint32_t i;
    track_t *t = &trk[0];
    int started, ended, local;
    input_on(&trk[1], 72, 100);                  /* local keys must not be panicked */
    t->p[P_AMODE] = arp;
    if (pedal)
        midi_in_event(0x7F40B00Bu);
    message(0x90u, 48, 100, uart);
    events_block(CTL);
    started = arp ? t->nheld != 0 : gated(t, 48);
    for (i = 0; i < MQ; i++)
        message(0xB0u, 0, 0, uart);             /* queue is full; note-off is lost */
    message(status, 48, 0, uart);
    events_block(CTL);
    if (pedal) {
        midi_in_event(0x0000B00Bu | 64u << 16);
        events_block(CTL);
    }
    ended = arp ? !t->nheld && !t->arp_phys : !gated(t, 48);
    local = gated(&trk[1], 72);
    /* Recovery must accept fresh MIDI notes and their ordinary release. */
    message(0x90u, 50, 100, uart);
    events_block(CTL);
    started &= arp ? t->nheld != 0 : gated(t, 50);
    message(0x80u, 50, 0, uart);
    events_block(CTL);
    ended &= arp ? !t->nheld : !gated(t, 50);
    return started && ended && local && !(mi_pen[0] | mi_pen[1] | mi_pen[2] | mi_pen[3]);
}

static int usb_off(void) { return overflow(0, 0x80u, 0, 0); }
static int usb_zero(void) { return overflow(0, 0x90u, 0, 0); }
static int uart_off(void) { return overflow(1, 0x80u, 0, 0); }
static int uart_zero(void) { return overflow(1, 0x90u, 0, 0); }
static int arp_off(void) { return overflow(1, 0x80u, 1, 0); }
static int pedal_off(void) { return overflow(0, 0x80u, 0, 1); }

int main(void)
{
    static const struct { const char *name; int (*run)(void); } cases[] = {
        {"POLY recording: bass held across melody and loop playback", record_loop},
        {"sequencer gate leaves physically held POLY note sounding", gate_overlap},
        {"key release leaves sequencer-owned note until its gate", key_before_gate},
        {"slide releases outgoing notes except live POLY hold", slide_overlap},
        {"held POLY note is not retriggered by step or ratchets", ratchet_overlap},
        {"USB overflow releases lost note-off, preserves local key", usb_off},
        {"USB overflow releases lost velocity-zero note-on", usb_zero},
        {"TRS overflow releases lost note-off, preserves local key", uart_off},
        {"TRS overflow releases lost velocity-zero note-on", uart_zero},
        {"TRS overflow removes MIDI notes from arpeggiator", arp_off},
        {"overflow note-off waits for sustain then releases", pedal_off},
    };
    uint32_t i;
    int fails = 0;
    fflush(stdout);
    for (i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        int status;
        pid_t pid = fork();
        if (!pid) {
            init();
            _exit(cases[i].run() ? 0 : 1);
        }
        if (pid < 0 || waitpid(pid, &status, 0) != pid)
            return 99;
        {
            int ok = WIFEXITED(status) && WEXITSTATUS(status) == 0;
            printf("hold_rec: %s: %s\n", cases[i].name, ok ? "ok" : "FAIL");
            fails += !ok;
        }
    }
    printf("hold_rec: %d failed\n", fails);
    return fails;
}
