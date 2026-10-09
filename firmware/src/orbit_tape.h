/* SPDX-License-Identifier: GPL-3.0-only */
/* Event-tape editing shares the existing sequencer/project format. No audio buffer. */
#ifndef ORBIT_TAPE_H
#define ORBIT_TAPE_H
static struct {
    step_t steps[NSTEP];
    int8_t micro[NSTEP];
    uint8_t fill[NSTEP], nlock;
    plock_t lock[NLOCK];
    uint8_t count, drum, cursor, first, last, lift;
    uint32_t revision;
} orbit_tape = {.last = 15};

static int orbit_capture(uint32_t track, uint32_t first, uint32_t last, int lift)
{
    uint32_t i;
    if (track >= NTRK || first > last || last >= NSTEP || last >= trk_len(&trk[track])) return 0;
    fm1_irq_off();
    orbit_tape.nlock = 0;
    for (i = 0; i < NLOCK; i++) {
        plock_t l = trk[track].lock[i];
        if (l.step >= first && l.step <= last) {
            l.step = (uint8_t)(l.step - first);
            orbit_tape.lock[orbit_tape.nlock++] = l;
        }
    }
    if (lift) locks_restore(&trk[track]);
    for (i = first; i <= last; ++i) {
        orbit_tape.steps[i-first] = trk[track].step[i];
        orbit_tape.micro[i-first] = trk[track].micro[i];
        orbit_tape.fill[i-first] = (uint8_t)step_fill(&trk[track], i);
        if (lift) {
            trk[track].micro[i] = 0;
            step_fill_set(&trk[track], i, FC_NORM);
            lock_del(&trk[track], i, P_COUNT);
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
    uint32_t i, k, count, len, keep = 0, need = 0;
    if (track >= NTRK || !orbit_tape.count || orbit_tape.drum != is_drum(&trk[track])) return 0;
    len = trk_len(&trk[track]);
    if (at >= len) return 0;
    count = orbit_tape.count;
    if (count > len-at) count = len-at;
    fm1_irq_off();
    /* Preflight the finite lock table: fail without editing if it would overflow. */
    for (i = 0; i < NLOCK; i++)
        if (trk[track].lock[i].step != LOCK_FREE &&
            (trk[track].lock[i].step < at || trk[track].lock[i].step >= at + count)) keep++;
    for (i = 0; i < orbit_tape.nlock; i++) if (orbit_tape.lock[i].step < count) need++;
    if (keep + need > NLOCK) { fm1_irq_on(); return 0; }
    locks_restore(&trk[track]);
    for (i = 0; i < count; ++i) {
        trk[track].step[at+i] = orbit_tape.steps[i];
        trk[track].micro[at+i] = orbit_tape.micro[i];
        step_fill_set(&trk[track], at+i, orbit_tape.fill[i]);
        lock_del(&trk[track], at+i, P_COUNT);
    }
    for (i = 0; i < orbit_tape.nlock; i++) {
        plock_t l = orbit_tape.lock[i];
        if (l.step >= count) continue;
        for (k = 0; k < NLOCK && trk[track].lock[k].step != LOCK_FREE; k++) ;
        l.step = (uint8_t)(at + l.step);
        trk[track].lock[k] = l;
    }
    ++orbit_tape.revision;
    fm1_irq_on();
    return (int)count;
}
#endif
