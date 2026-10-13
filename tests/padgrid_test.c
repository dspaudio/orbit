/* SPDX-License-Identifier: GPL-3.0-only
 * SMK-37 port: Copyright (C) 2026 M. Geisler (genkigenki/makkuro) */
/* SMK-37 Pro pads (firmware/hal/smk_input.h): the step grid in reading order (top-left = step 1),
 * notes low at the bottom (bottom-left = the lowest). Built -O2 (the IRQ lock's asm is left out). */
#define BOARD_SMK37 1
#include "smk_input.h"
#include <stdio.h>
static int fails;
static void check(int ok, const char *what) { printf("padgrid: %-60s %s\n", what, ok ? "ok" : "FAIL"); fails += !ok; }
static uint32_t hit(uint32_t pad)               /* the note key a pad presses (bit) */
{
    uint32_t n;
    smk__padq(pad, 100);
    n = fm1_in.pnotes;
    smk__padq(pad, 0);
    return n;
}

int main(void)
{
    smk_pad_grid = 0;
    check(hit(8) == 1u << 0, "notes: bottom-left pad = the lowest note (F3)");
    check(hit(0) == 1u << SMK_WHITE_NOTE[8], "notes: top-left pad = white key 9");
    check(SMK_PAD_OF_KEY(0) == 8u, "notes: white key 0 lit on the bottom-left pad");
    smk_pad_grid = 1;
    check(hit(0) == 1u << 0, "grid: top-left pad = step 1");
    check(hit(7) == 1u << SMK_WHITE_NOTE[7], "grid: top-right pad = step 8");
    check(hit(8) == 1u << SMK_WHITE_NOTE[8], "grid: bottom-left pad = step 9");
    check(SMK_PAD_OF_KEY(0) == 0u && SMK_PAD_OF_KEY(15) == 15u, "grid: step w lit on pad w");
    smk__padq(8, 100);                          /* a pad down as the mode changes: its release ends its own key */
    smk_pad_grid = 0;
    smk__padq(8, 0);
    check(fm1_in.pnotes == 0u, "a mode change while held: no stuck key");
    printf("padgrid: %s\n", fails ? "FAIL" : "ok");
    return fails != 0;
}
