/* SPDX-License-Identifier: GPL-3.0-only */
/* Event-tape editing shares the existing sequencer/project format. No audio buffer. */
#ifndef ORBIT_TAPE_H
#define ORBIT_TAPE_H
static struct {
    step_t steps[NSTEP];
    uint8_t count, drum, cursor, first, last, lift;
    uint32_t revision;
} orbit_tape = {.last = 15};

static int orbit_capture(uint32_t track, uint32_t first, uint32_t last, int lift)
{
    uint32_t i;
    if (track >= NTRK || first > last || last >= NSTEP || last >= trk_len(&trk[track])) return 0;
    fm1_irq_off();
    for (i = first; i <= last; ++i) {
        orbit_tape.steps[i-first] = trk[track].step[i];
        if (lift) {
            memset(&trk[track].step[i], 0, sizeof(step_t));
            if (!is_drum(&trk[track])) trk[track].step[i].time = ST_REST;
        }
    }
    orbit_tape.count = (uint8_t)(last-first+1);
    orbit_tape.drum = (uint8_t)is_drum(&trk[track]);
    ++orbit_tape.revision;
    fm1_irq_on();
    return 1;
}

/* Drop overwrites events, truncates at the pattern end, and never reinterprets drum bits as notes. */
static int orbit_drop(uint32_t track, uint32_t at)
{
    uint32_t i, count, len;
    if (track >= NTRK || !orbit_tape.count || orbit_tape.drum != is_drum(&trk[track])) return 0;
    len = trk_len(&trk[track]);
    if (at >= len) return 0;
    count = orbit_tape.count;
    if (count > len-at) count = len-at;
    fm1_irq_off();
    for (i = 0; i < count; ++i) trk[track].step[at+i] = orbit_tape.steps[i];
    ++orbit_tape.revision;
    fm1_irq_on();
    return (int)count;
}
#endif
