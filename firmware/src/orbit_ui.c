/* SPDX-License-Identifier: GPL-3.0-only */
/* ORBIT: original four-colour event-tape view for the FM-1. */
#include "orbit_tape.h"
static void orbit_knob(uint32_t k, int32_t delta)
{
    uint32_t len = trk_len(TSEL);
    if (orbit_tape.cursor >= len) orbit_tape.cursor = (uint8_t)(len-1);
    if (orbit_tape.last >= len) orbit_tape.last = (uint8_t)(len-1);
    if (orbit_tape.first > orbit_tape.last) orbit_tape.first = orbit_tape.last;
    if (k == 0) orbit_tape.cursor = (uint8_t)clamp(orbit_tape.cursor+delta, 0, len-1);
    if (k == 1) orbit_tape.first = (uint8_t)clamp(orbit_tape.first+delta, 0, orbit_tape.last);
    if (k == 2) orbit_tape.last = (uint8_t)clamp(orbit_tape.last+delta, orbit_tape.first, len-1);
    if (k == 3) orbit_tape.lift = (uint8_t)clamp(orbit_tape.lift+delta, 0, 1);
    if (k < 4) ui.force = 1;
}
static void orbit_edit(int drop)
{
    orbit_knob(4, 0); /* normalise after track or pattern length changes */
    if (drop) {
        if (orbit_drop(song.sel, orbit_tape.cursor)) ui_message("EVENTS DROPPED");
        else ui_message("EMPTY / WRONG TYPE");
    } else {
        if (orbit_tape.lift && !is_drum(TSEL)) undo_mark(TSEL, (undo_sess += 4u) | 3u);
        orbit_capture(song.sel, orbit_tape.first, orbit_tape.last, orbit_tape.lift);
        ui_message(orbit_tape.lift ? "LIFTED: OCT+ DROP" : "COPIED: OCT+ DROP");
    }
    sync_reload = 1;
    ui.force = 1;
}
static void orbit_screen_draw(void)
{
    static uint32_t cache, dial_cache;
    uint32_t i, j, sig = orbit_tape.revision + song.sel*17u + song.rec*97u + song.playing;
    char b[32], vals[4][12]; const char *v[4];
    const char *const labels[4] = {"head", "in", "out", "edit"};
    int32_t ratios[4];
    orbit_knob(4, 0);
    /* orbit_knob above normalises selection; redraw only if musical state changes. */
    for (i=0;i<NTRK;i++) {
        sig = sig*31u + trk[i].seq_idx + trk[i].p[P_MUTE] + trk[i].p[P_SLEN]*71u;
        for (j=0;j<NSTEP;j++) sig = sig*31u + trk_step_on(&trk[i],j);
    }
    sig = sig*31u + orbit_tape.cursor + orbit_tape.first*67u + orbit_tape.last*4099u;
    sig = sig*31u + song.g[G_BPM];
    if (ui.force || cache != sig) {
        cache = sig;
        cv_begin(240, 48, C_BLACK);
        cv_text(4, 0, &FONT_S, "ORBIT / EVENT TAPE", C_WHITE);
        fmt_int(b, song.g[G_BPM]); cv_text(196, 0, &FONT_S, b, TE_G4);
        cv_text(4, 24, &FONT_S, "OCT- edit   OCT+ drop", TE_G3);
        cv_blit(0, 0);
        cv_begin(240, 124, C_BLACK);
        for (i=0;i<NTRK;i++) {
            int32_t y = (int32_t)i*30;
            cv_rect(0,y,240,28, i==song.sel ? TE_G1 : C_BLACK);
            fmt_int(b, (int32_t)i+1); cv_text(4,y+5,&FONT_S,b,TE_COL[i]);
            if (song.rec & (1u<<i)) te_disc(20,y+12,3,TE_RED);
            for (j=0;j<NSTEP;j++) {
                int32_t x = 30+(int32_t)j*3;
                uint16_t color = trk[i].p[P_MUTE] ? TE_DIM[i] : TE_COL[i];
                if (i==song.sel && j>=orbit_tape.first && j<=orbit_tape.last) cv_rect(x,y+1,3,26,TE_DIM[i]);
                if (j<trk_len(&trk[i])) cv_rect(x,y+13,1,2,TE_G2);
                if (j%16==0) cv_line(x,y+2,x,y+4,TE_G3);
                if (j<trk_len(&trk[i]) && trk_step_on(&trk[i],j)) cv_rect(x,y+8,2,12,color);
                if (song.playing && j==trk[i].seq_idx) cv_line(x,y+2,x,y+25,C_WHITE);
                if (i==song.sel && j==orbit_tape.cursor) cv_rect(x,y+24,3,3,C_WHITE);
            }
        }
        cv_blit(0,52);
        lcd_fill(0,176,240,8,C_BLACK);
    }
    fmt_int(vals[0],orbit_tape.cursor+1); fmt_int(vals[1],orbit_tape.first+1);
    fmt_int(vals[2],orbit_tape.last+1); str_cpy(vals[3],orbit_tape.lift ? "lift" : "copy",12);
    for(i=0;i<4;i++) v[i]=vals[i];
    ratios[0]=orbit_tape.cursor*1000/63; ratios[1]=orbit_tape.first*1000/63;
    ratios[2]=orbit_tape.last*1000/63; ratios[3]=orbit_tape.lift*1000;
    te_dials(184,labels,v,ratios,sig,&dial_cache);
}
