/* SPDX-License-Identifier: GPL-3.0-only */
/* Original ORBIT boot screen. Entry-point name retained for the FM-1 boot flow. */
static void sloop_splash(void)
{
    uint32_t i;
    lcd_fill(0,0,240,240,C_BLACK);
    cv_begin(240,120,C_BLACK);
    cv_text(120-text_w(&FONT_L,"ORBIT")/2,16,&FONT_L,"ORBIT",C_WHITE);
    for(i=0;i<4;i++) {
        te_dial(30+(int32_t)i*60,82,18,250+(int32_t)i*200,TE_COL[i],TE_DIM[i]);
    }
    cv_blit(0,44);
    cv_begin(240,36,C_BLACK);
    cv_text(120-text_w(&FONT_S,FELUCCA_VERSION)/2,0,&FONT_S,FELUCCA_VERSION,TE_G4);
    cv_text(120-text_w(&FONT_S,"SLOOP / FELUCCA")/2,18,&FONT_S,"SLOOP / FELUCCA",TE_G3);
    cv_blit(0,202);
}
