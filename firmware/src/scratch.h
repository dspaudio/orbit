/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Andre Rezemini (the SLOOP ram branch, github.com/andrerezemini/sloop-lab) */
/* One scratch for the main loop's short-lived buffers that are never alive at the same time (SLOOP 2.7):
 *   st     the storage payload (storage.c st_buf): filled and consumed inside one st_load / st_save /
 *          st_current call and its caller's next lines (dsu_boot, fm6_bank_scan read it right after);
 *   snap   the HOME oscilloscope's (ui_draw.c graph_scope): refilled by every call.
 * Neither is touched by the audio interrupt or by TIMER5's poll; neither is read after a return to the main
 * loop; a flash access never happens inside a draw, nor a draw inside a flash access (main.c: ed_service,
 * autosave_tick and ui_draw run one after another). ORBIT: the visualiser's snapshot (SLOOP's third member)
 * went with the visualiser. Each user asserts its size against its own constant. */
#pragma once
#include <stdint.h>
static union {
    uint8_t st[4096 - 256];                  /* ST_PAYLOAD_MAX (storage.c) */
    int16_t snap[512];                       /* SCOPE_N (ui_draw.c) */
} main_scratch __attribute__((aligned(4)));
