/* SPDX-License-Identifier: GPL-3.0-only
 * Original composition: FIRST LIGHT, ORBIT contributors, 2026.
 * Four bars, 108 BPM, Am7 / Fmaj7 / Cmaj7 / G7. Editable note events,
 * not prerecorded audio. Loading requires two confirmations in the HOME menu.
 */
static void orbit_demo_note(track_t *t, uint32_t pos, uint32_t note, uint32_t vel)
{
    step_t *s = &t->step[pos];
    s->note[0] = (uint8_t)note; s->n = 1; s->time = ST_NOTE;
    s->vel = (uint8_t)vel;
}
static void orbit_demo_load(void)
{
    static const uint8_t root[4] = {45, 41, 48, 43};
    static const uint8_t chords[4][3] = {{60, 64, 67}, {57, 60, 64}, {59, 64, 67}, {59, 62, 65}};
    static const uint8_t lead[4][5] = {{76, 72, 71, 67, 69}, {72, 69, 67, 64, 67},
                                     {76, 79, 76, 74, 71}, {74, 71, 69, 67, 71}};
    static const uint8_t onset[5] = {2, 5, 8, 11, 14};
    uint32_t bar, j, k;
    project_new();
    fm1_irq_off();
#if FELUCCA_ARRANGER
    arrangement_enabled = 0; /* demo is an editable four-bar loop, not saved section playback */
#endif
    song.rec = 0; rec_wait = 0;
    song.g[G_BPM] = 108; song.g[G_SWING] = 12; song.g[G_DUCK] = 18;
    song.solo = 0; song.sel = 0;
    set_engine_of(&trk[2], ORBIT_PULSE); apply_preset_to(&trk[2], 2); /* PIN */
    for (k = 0; k < NTRK; k++) {
        trk[k].p[P_SLEN] = 64;
        trk[k].p[P_SDIV] = 2; /* 1/16: N_DIV[2] */
        trk[k].p[P_MUTE] = 0;
    }
    trk[0].p[P_LEVEL] = 96; trk[1].p[P_LEVEL] = 86; trk[2].p[P_LEVEL] = 92;
    song.g[G_DRLVL] = 92;
    trk[1].p[P_PAN] = -12; trk[2].p[P_PAN] = 14;
    for (bar = 0; bar < 4; bar++) {
        uint32_t base = bar * 16;
        orbit_demo_note(&trk[0], base, root[bar], 112);
        orbit_demo_note(&trk[0], base+6, root[bar], 90);
        orbit_demo_note(&trk[0], base+10, root[bar]+12, 98);
        orbit_demo_note(&trk[0], base+14, root[bar]+7, 85);
        for (j = 1; j < 5; j++) trk[0].step[base+j].time = ST_TIE;
        orbit_demo_note(&trk[1], base, chords[bar][0], 80);
        trk[1].step[base].n = 3;
        for (j = 0; j < 3; j++) trk[1].step[base].note[j] = chords[bar][j];
        for (j = 1; j < 15; j++) trk[1].step[base+j].time = ST_TIE;
        for (j = 0; j < 5; j++) orbit_demo_note(&trk[2], base+onset[j], lead[bar][j], 80+j*5);
        for (j = 0; j < 16; j++) {
            dstep_t *s = &TDRUM->dstep[base+j];
            if (j == 0 || j == 6 || j == 10) dstep_set(s, 0, LV_HARD, 0);
            if (j == 4 || j == 12) dstep_set(s, 2, LV_NORM, 0);
            if (!(j & 1u)) dstep_set(s, 4, j % 4 ? LV_SOFT : LV_NORM, 0);
        }
    }
    fm1_irq_on();
    sync_reload = 1; ui.force = 1;
}
