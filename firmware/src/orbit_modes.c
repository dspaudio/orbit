/* SPDX-License-Identifier: GPL-3.0-only */
/* Graphic-first sound pages. Included after ui_draw.c graph helpers. */
static int orbit_sound_page(void)
{
    const page_t *pg=cur_page();
    return !is_drum(TSEL) && (pg->scope==SC_ENGINE || pg->fam==FAM_ENV || pg->fam==FAM_LFO);
}
static void orbit_sound_draw(void)
{
    static uint32_t cache;
    const page_t *pg=cur_page();
    const engine_t *engine=ENGINES[TSEL->eng_req%NENGINES];
    int lfo_off = !TSEL->p[P_LD_PIT] && !TSEL->p[P_LD_FLT] && !TSEL->p[P_LD_SHP] && !TSEL->p[P_LD_AMP];
    const param_desc_t *desc[4];
    char value[4][16], label[4][8], small[8], title[24];
    int32_t ratio[4]; uint32_t k,x,sig=ui.page+TSEL->eng_req*71u+song.sel*65537u;
    for(k=0;k<4;k++) {
        int16_t *vp; const char *unit;
        desc[k]=page_desc(pg,k,&vp); ratio[k]=0; value[k][0]=label[k][0]=0;
        if(desc[k] && desc[k]->label && desc[k]->label[0]!='-') {
            str_cpy(label[k],desc[k]->label,sizeof label[k]);
            param_format(desc[k],*vp,value[k],&unit);
            ratio[k]=clamp(RATIO(desc[k],*vp),0,1000); sig=sig*31u+(uint32_t)*vp;
        }
    }
    /* The sample/grain page shows the live master output, labelled explicitly. */
    if(pg->scope==SC_ENGINE && (TSEL->eng_req==4 || TSEL->eng_req==8)) sig=sig*31u+scope_w;
    sig=sig*31u+ui.msg_t+ui.hot_t+ui.hot_col;
    if(pg->fam==FAM_LFO) sig=sig*31u+(uint32_t)lfo_off;
    if(!ui.force && sig==cache) return;
    cache=sig;
    cv_begin(240,36,C_BLACK);
    str_cpy(title,pg->scope==SC_ENGINE ? engine->name : pg->fam==FAM_LFO ?
            (pg->graph==GR_LFO ? "LFO SOURCE 1/2" : "LFO DEST 2/2") : pg->title,sizeof title);
    cv_text(8,2,&FONT_S,title,C_WHITE);
    fmt_int(small,song.sel+1); cv_text(218,2,&FONT_S,small,TE_COL[song.sel]);
    if(pg->fam==FAM_LFO && pg->graph==GR_LFO && lfo_off)
        cv_text(8,20,&FONT_S,"NO DEPTH: PRESS LFO",TE_G3);
    else if(ui.hot_t && label[ui.hot_col][0]) {
        cv_text(8,20,&FONT_S,label[ui.hot_col],TE_COL[ui.hot_col]);
        cv_text(72,20,&FONT_S,value[ui.hot_col],C_WHITE);
    } else cv_text(8,20,&FONT_S,"sound / four controls",TE_G3);
    cv_blit(0,0);
    lcd_fill(0,36,240,4,C_BLACK);
    cv_begin(240,124,C_BLACK);
    if(pg->graph==GR_ADSR) {
        int32_t a=16+ratio[0]*45/1000,d=a+16+ratio[1]*45/1000,y=108-ratio[2]*86/1000;
        cv_line(12,110,a,12,TE_COL[0]); cv_line(a,12,d,y,TE_COL[1]);
        cv_line(d,y,180,y,TE_COL[2]); cv_line(180,y,200+ratio[3]*28/1000,110,TE_COL[3]);
        te_disc(a,12,4,TE_COL[0]); te_disc(d,y,4,TE_COL[1]); te_disc(180,y,4,TE_COL[2]);
    } else if(pg->fam==FAM_LFO) {
        graph_lfo(TSEL,TE_COL[0]);
        orbit_ring(192,34,17+ratio[2]*12/1000,TE_COL[2]);
        orbit_ring(192,34,8+ratio[3]*8/1000,TE_COL[3]);
    } else if(pg->scope==SC_ENGINE && (TSEL->eng_req==4 || TSEL->eng_req==8)) {
        cv_text(8,0,&FONT_S,"output",TE_G3);
        for(k=0;k<4;k++) for(x=0;x<54;x++) {
            uint32_t p=(scope_w+x*2u+k*108u)&(SCOPE_N-1u),next=(p+2)&(SCOPE_N-1u);
            cv_line(12+k*54+x,62+scope_buf[p]*45/32768,
                    13+k*54+x,62+scope_buf[next]*45/32768,TE_COL[k]);
        }
        cv_line(14,114,226,114,TE_G2);
    } else if(pg->scope==SC_ENGINE && (TSEL->eng_req==1 || TSEL->eng_req==2 || TSEL->eng_req==6)) {
        /* Parameter-driven resonator/orbit diagram; not an audio measurement. */
        for(k=0;k<4;k++) {
            int32_t cx=68+(int32_t)k*34,cy=60+(k%2 ? 10 : -10),r=18+ratio[k]*22/1000;
            uint32_t angle=(uint32_t)ratio[k]*1023/1000;
            orbit_ring(cx,cy,r,TE_COL[k]);
            cv_line(cx,cy,cx+((SINE[(angle+256)&1023]*r)>>15),cy+((SINE[angle]*r)>>15),TE_COL[k]);
            te_disc(cx,cy,3,C_WHITE);
        }
    } else {
        /* Four coloured oscillator curves, driven by the active page values. */
        for(k=0;k<4;k++) {
            int32_t py=0;
            for(x=0;x<216;x++) {
                uint32_t phase=(x*(5u+(uint32_t)ratio[k]/100u)+k*128u)&1023u;
                int32_t y=62+((SINE[phase]*(18+ratio[k]*30/1000))>>15);
                if(x) cv_line(11+x,py,12+x,y,TE_COL[k]); py=y;
            }
        }
    }
    cv_blit(0,40);
    cv_begin(240,32,C_BLACK);
    for(k=0;k<4;k++) { te_dial(30+k*60,16,13,ratio[k],TE_COL[k],TE_DIM[k]); }
    cv_blit(0,164);
    cv_begin(240,44,C_BLACK);
    if(ui.msg_t) cv_text(4,14,&FONT_S,ui.msg,C_WHITE);
    else for(k=0;k<4;k++) {
        str_cpy(small,label[k],7); cv_text(6+k*60,2,&FONT_S,small,TE_G3);
        str_cpy(small,value[k],7); cv_text(6+k*60,23,&FONT_S,small,TE_COL[k]);
    }
    cv_blit(0,196);
}

static void orbit_mixer_draw(void)
{
    static uint32_t cache,footer;
    uint32_t k,sig=song.sel+song.solo*31u+song.g[G_BPM]*71u;
    char b[16];
    for(k=0;k<NTRK;k++) sig=sig*31u+trk[k].p[P_LEVEL]+trk[k].p[P_MUTE]*521u+trk[k].p[P_PAN]*17u;
    sig=sig*31u+song.g[G_DRLVL];
    if(ui.force || sig!=cache) {
        cache=sig;
        cv_begin(240,36,C_BLACK);
        cv_text(8,2,&FONT_S,"mixer",C_WHITE); fmt_int(b,song.g[G_BPM]); cv_text(192,2,&FONT_S,b,TE_G3);
        cv_text(8,20,&FONT_S,"levels / selected track",TE_G3); cv_blit(0,0);
        cv_begin(240,124,C_BLACK);
        for(k=0;k<NTRK;k++) {
            int32_t cx=30+(int32_t)k*60,level=is_drum(&trk[k])?song.g[G_DRLVL]:trk[k].p[P_LEVEL];
            int32_t y=108-level*82/127,j;
            uint16_t col=trk_silent(&trk[k])?TE_DIM[k]:TE_COL[k];
            for(j=0;j<6;j++) cv_line(cx-18,26+j*16,cx+18,26+j*16,TE_G1);
            cv_rect(cx-2,25,4,86,TE_G2); cv_rect(cx-2,y,4,109-y,col);
            cv_rect(cx-17,y-3,34,8,col); cv_line(cx-12,y,cx+12,y,C_BLACK);
            fmt_int(b,k+1); cv_text(cx-4,2,&FONT_S,b,col);
            if(k==song.sel) orbit_ring(cx,11,10,C_WHITE);
            if(trk_silent(&trk[k])) cv_line(cx-8,113,cx+8,113,TE_G3);
        }
        cv_blit(0,36);
        cv_begin(240,24,C_BLACK);
        for(k=0;k<NTRK;k++) {
            uint32_t level=is_drum(&trk[k])?song.g[G_DRLVL]:trk[k].p[P_LEVEL];
            fmt_int(b,trk_silent(&trk[k])?0:level*100/127);
            te_text_c(30+(int32_t)k*60,2,b,TE_COL[k]);
        }
        cv_blit(0,160);
    }
    {
        track_t *t=TSEL;
        char v[4][8]; const char *val[4]={v[0],v[1],v[2],v[3]};
        const char *const lab[4]={"swing","level","steps","pan"};
        int32_t ratio[4],level=is_drum(t)?song.g[G_DRLVL]:t->p[P_LEVEL];
        swing_str(v[0],song.g[G_SWING]); fmt_int(v[1],t->p[P_MUTE]?0:level*100/127);
        fmt_int(v[2],t->p[P_SLEN]); fmt_int(v[3],t->p[P_PAN]);
        ratio[0]=song.g[G_SWING]*10; ratio[1]=t->p[P_MUTE]?0:level*1000/127;
        ratio[2]=(t->p[P_SLEN]-1)*1000/63; ratio[3]=(t->p[P_PAN]+64)*1000/127;
        te_dials(184,lab,val,ratio,sig,&footer);
    }
}
