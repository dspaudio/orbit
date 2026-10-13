/* SPDX-License-Identifier: GPL-3.0-only
 * SMK-37 port: Copyright (C) 2026 M. Geisler (genkigenki/makkuro) */
/* SMK-37 Pro pad sound picker (firmware/src/ui_pick.c) on the UI harness: the pages by kind, 16 a page, the
 * current sound's page, a cell loads its sound, < > wrap, the short names, the drum kits; renders
 * DIR/pick-synth.ppm, pick-drums.ppm. */
#define UI_PAGES_HARNESS_ONLY 1
#include "ui_pages_test.c"
#include "../firmware/src/ui_pick.c"

static void pick_shot(const char *name)
{
    ui.frame++;
    pick_draw();
    ppm(name);
}

int main(int argc, char **argv)
{
    uint32_t i, np, sum = 0, of = 0;
    pick_pg_t pg;
    char a[4], b[4];
    outdir = argc > 1 ? argv[1] : "build/host";
    panel = PANEL_DEFAULT;
    layers_init();
    settings.palette = 4;
    palette_set(4);
    host_tracks_init();
    for (i = 0; i < NPART; i++) { set_engine_of(&trk[i], TRK_DEF[i][0]); apply_preset_to(&trk[i], TRK_DEF[i][1]); trk[i].engine = trk[i].eng_req; }
    TDRUM->p[P_E0] = DRUM_DEFAULT_KIT;
    go_home(); ui.force = 1; frame();
    song.sel = 0;

    pick_abbr("PLUGG BASS", a, b);
    check(!strcmp(a, "PLU") && !strcmp(b, "BAS"), "short names: two words, 3 + 3");
    pick_abbr("SUPERSAW", a, b);
    check(!strcmp(a, "SUP") && !strcmp(b, "ERS"), "short names: one word, its first 6 letters");
    pick_abbr("PAD", a, b);
    check(!strcmp(a, "PAD") && !b[0], "short names: a short word, one line");

    np = pick_walk(0, &pg, 0xFFFFu, 0);
    check(pg.j0 == 0 && pg.kind == 0 && pick_kind_of(pick_cell(&pg, 0)) == 0 && pg.len == 16 && pg.nsub == 2,
          "page 1: the first 16 basses (from the whole list), BASS 1/2");
    {
        static uint8_t seen[NBANK + UP_SLOTS];
        uint32_t w, n, once = 1, kinds = 1;
        for (i = 0; i < np; i++) {
            pick_walk(i, &pg, 0xFFFFu, 0);
            if (pg.len == 0 || pg.len > 16) break;
            for (w = 0; w < pg.len; w++) {
                n = pick_cell(&pg, w);
                kinds &= pick_kind_of(n) == pg.kind;
                if (n < sizeof seen) seen[n]++;
                sum++;
            }
        }
        for (n = 0; n < NBANK + up_count(); n++)
            once &= seen[n] == 1;
        check(i == np && sum == NBANK + up_count() && once && kinds, "the pages hold every sound once, one kind a page, 16 at most");
    }
    pick_pin_toggle();
    pick_walk(0xFFFFu, 0, pick_cur(), &of);
    check(pick_shown() && pick_page == of, "KNOB BANK: open at the current sound's page");
    pick_turn(-1);
    pick_turn(1);
    check(pick_page == of, "< > step the pages and back");
    pick_page = 0; pick_turn(-1);
    check(pick_page == np - 1u, "< on the first page: the last");
    pick_page = 1;
    pick_select(3);
    pick_info(&pg);
    check(pick_cur() == pick_cell(&pg, 3), "a pad loads its sound (as PRESETS)");
    fm1_ms += 60000u;
    check(pick_shown(), "stays open until KNOB BANK / PATCH");
    ui.layer = LY_STEP;
    check(!pick_shown(), "hidden while a layer is held");
    ui.layer = LY_PLAY;
    ui.force = 1; pick_shot("pick-synth");
    pick_pin_toggle();
    check(!pick_shown(), "KNOB BANK again: closed");

    track_select(TRK_DRUM);
    pick_pin_toggle();
    np = pick_info(&pg);
    check(np == (DRUM_USR + 15u) / 16u + (DRUM_KITS - DRUM_USR + 15u) / 16u, "drum track: the kits, 16 a page");
    pick_select(2);
    check(pick_cur() == pick_cell(&pg, 2), "drum track: a pad picks its kit");
    ui.force = 1; pick_shot("pick-drums");
    pick_close();
    printf("pick: %s\n", fails ? "FAIL" : "ok");
    return fails != 0;
}
