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
/* Circular graphics use the existing fixed-point sine table, without a framebuffer. */
static void orbit_ring(int32_t cx,int32_t cy,int32_t r,uint16_t color)
{
    uint32_t a;
    for(a=0;a<1024;a+=16) {
        uint32_t next=(a+16)&1023u;
        cv_line(cx+((SINE[(a+256)&1023u]*r)>>15),cy+((SINE[a]*r)>>15),
                cx+((SINE[(next+256)&1023u]*r)>>15),cy+((SINE[next]*r)>>15),color);
    }
}
static void orbit_reel(int32_t cx,int32_t cy,int32_t r,uint32_t phase,uint16_t color)
{
    uint32_t j;
    orbit_ring(cx,cy,r,color); orbit_ring(cx,cy,r-4,TE_G2); orbit_ring(cx,cy,8,color);
    for(j=0;j<3;j++) {
        uint32_t a=(phase+j*341u)&1023u;
        int32_t x=cx+((SINE[(a+256)&1023u]*(r-12))>>15), y=cy+((SINE[a]*(r-12))>>15);
        te_disc(x,y,6,TE_G2); cv_line(cx,cy,x,y,color);
    }
    te_disc(cx,cy,3,C_WHITE);
}
static void orbit_screen_draw(void)
{
    static uint32_t cache;
    uint32_t i,j,sig=orbit_tape.revision+song.sel*17u+song.rec*97u+song.playing;
    char b[16];
    const char *const labels[4]={"head","in","out","edit"};
    orbit_knob(4,0);
    for(i=0;i<NTRK;i++) {
        sig=sig*31u+trk[i].seq_idx+trk[i].p[P_MUTE]+trk[i].p[P_SLEN]*71u;
        for(j=0;j<NSTEP;j++) sig=sig*31u+trk_step_on(&trk[i],j);
    }
    sig=sig*31u+orbit_tape.cursor+orbit_tape.first*67u+orbit_tape.last*4099u;
    sig=sig*31u+song.g[G_BPM]+ui.msg_t+orbit_tape.lift;
    if(!ui.force && cache==sig) return;
    cache=sig;
    cv_begin(240,24,C_BLACK);
    cv_text(8,2,&FONT_S,"tape",C_WHITE); cv_text(64,2,&FONT_S,"events",TE_G3);
    fmt_int(b,song.g[G_BPM]); cv_text(168,2,&FONT_S,b,TE_G4);
    te_play_icon(218,2,song.playing); cv_blit(0,0);
    cv_begin(240,96,C_BLACK);
    {
        uint32_t pos=song.playing ? TSEL->seq_idx : orbit_tape.cursor, phase=(pos*89u)&1023u;
        cv_line(54,15,186,15,TE_G2); cv_line(54,81,186,81,TE_G2);
        orbit_reel(56,48,35,phase,TE_COL[0]); orbit_reel(184,48,35,phase,TE_COL[1]);
        cv_line(81,72,107,86,TE_G4); cv_line(107,86,133,86,TE_G4); cv_line(133,86,159,72,TE_G4);
        cv_rect(112,71,16,17,TE_G3); cv_rect(118,71,4,17,TE_COL[2]);
        if(song.rec) te_disc(120,38,7,TE_RED); else te_play_icon(114,30,song.playing);
    }
    cv_blit(0,24);
    cv_begin(240,76,C_BLACK);
    for(i=0;i<NTRK;i++) {
        int32_t y=(int32_t)i*19;
        if(i==song.sel) cv_rect(0,y,240,18,TE_G1);
        fmt_int(b,(int32_t)i+1); cv_text(6,y+1,&FONT_S,b,TE_COL[i]);
        for(j=0;j<NSTEP;j++) {
            int32_t x=30+(int32_t)j*3;
            if(i==song.sel && j>=orbit_tape.first && j<=orbit_tape.last) cv_rect(x,y+1,3,16,TE_DIM[i]);
            if(j<trk_len(&trk[i])) cv_rect(x,y+8,1,2,TE_G2);
            if(j<trk_len(&trk[i]) && trk_step_on(&trk[i],j)) cv_rect(x,y+4,3,10,trk[i].p[P_MUTE]?TE_DIM[i]:TE_COL[i]);
            if(song.playing && j==trk[i].seq_idx) cv_line(x,y+1,x,y+16,C_WHITE);
            if(i==song.sel && j==orbit_tape.cursor) cv_rect(x,y+15,3,2,TE_COL[2]);
        }
    }
    cv_blit(0,120);
    cv_begin(240,44,C_BLACK);
    if(ui.msg_t) cv_text(4,14,&FONT_S,ui.msg,C_WHITE);
    else for(i=0;i<4;i++) {
        int32_t x=6+(int32_t)i*60;
        cv_rect(x,2,48,2,TE_COL[i]); cv_text(x,6,&FONT_S,labels[i],TE_G3);
        if(i<3) fmt_int(b,1+(i==0?orbit_tape.cursor:i==1?orbit_tape.first:orbit_tape.last));
        else str_cpy(b,orbit_tape.lift?"lift":"copy",sizeof b);
        cv_text(x,25,&FONT_S,b,TE_COL[i]);
    }
    cv_blit(0,196);
}
