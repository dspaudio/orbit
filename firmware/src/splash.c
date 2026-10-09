/* SPDX-License-Identifier: GPL-3.0-only */
/* 작은 canvas로 그리는 Orbit 부트 워드마크. */
static void orbit_splash(void)
{
    uint32_t i;
    static const int32_t rings[2][3] = {{53, 32, 22}, {139, 39, 15}};
    lcd_fill(0,0,240,240,C_BLACK);
    cv_begin(240,64,C_BLACK);
    /* 1픽셀 스트로크이며 글꼴 비트맵이나 framebuffer를 추가하지 않는다. */
    for (i = 0; i < 2u; i++) {
        int32_t x = 0, y = rings[i][2], err = 1 - y;
        int32_t cx = rings[i][0], cy = rings[i][1];
        while (x <= y) {
            cv_pset(cx+x,cy+y,C_WHITE); cv_pset(cx-x,cy+y,C_WHITE);
            cv_pset(cx+x,cy-y,C_WHITE); cv_pset(cx-x,cy-y,C_WHITE);
            cv_pset(cx+y,cy+x,C_WHITE); cv_pset(cx-y,cy+x,C_WHITE);
            cv_pset(cx+y,cy-x,C_WHITE); cv_pset(cx-y,cy-x,C_WHITE);
            x++;
            if (err < 0) err += 2*x + 1;
            else { y--; err += 2*(x-y) + 1; }
        }
    }
    cv_line(89,25,89,54,C_WHITE);
    cv_line(89,31,95,26,C_WHITE); cv_line(95,26,102,24,C_WHITE);
    cv_line(102,24,111,25,C_WHITE);
    cv_line(124,10,124,54,C_WHITE);
    cv_line(168,25,168,54,C_WHITE); cv_pset(168,14,C_WHITE);
    cv_line(195,12,195,48,C_WHITE); cv_line(187,25,206,25,C_WHITE);
    cv_line(195,48,197,53,C_WHITE); cv_line(197,53,202,55,C_WHITE);
    cv_line(202,55,208,54,C_WHITE);
    cv_blit(0,88);
    cv_begin(240,36,C_BLACK);
    cv_text(120-text_w(&FONT_S,FELUCCA_VERSION)/2,0,&FONT_S,FELUCCA_VERSION,TE_G4);
    cv_text(120-text_w(&FONT_S,"FM-1")/2,18,&FONT_S,"FM-1",TE_G3);
    cv_blit(0,202);
    lcd_sync();
}
