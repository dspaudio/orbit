/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Host test of the user preset record (firmware/src/upreset.c, -DUP_HOST part):
 * UP_PUT parsing, a bank round trip through storage.c on a simulated NOR,
 * OPB3 (198 bytes), frozen OPB2 (196 bytes) and UPB1 (192 bytes) decoded,
 * bank / record version checks, map-by-count of values and macro ids,
 * pattern <-> steps. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define __attribute__(x)
#define UP_HOST 1
#include "../firmware/src/core.h"

static uint8_t nor[0x100000];
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off) { memset(nor + off, 0xFF, 4096); return 0; }
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    uint32_t i;
    for (i = 0; i < n; i++)
        nor[off + i] &= s[i];
    return 0;
}
#include "../firmware/src/storage.c"
#include "../firmware/src/upreset.c"
static uint32_t up_count_host(void)              /* (up_count is outside UP_HOST) */
{
    uint32_t k, n = 0;
    for (k = 0; k < UP_SLOTS; k++)
        n += (uint32_t)up_used(k);
    return n;
}

static int check(const char *what, int ok)
{
    printf("%-46s %s\n", what, ok ? "ok" : "FAIL");
    return ok ? 0 : 1;
}

static uint32_t put_frame(uint8_t *a, uint32_t slot, uint32_t eng, const char *name, int32_t base)
{
    uint32_t n = 0, i;
    a[n++] = (uint8_t)slot;
    a[n++] = (uint8_t)eng;
    for (i = 0; name[i]; i++)
        a[n++] = (uint8_t)name[i];
    a[n++] = 0;
    for (i = 0; i < P_COUNT; i++) {
        uint32_t u = (uint32_t)(base + (int32_t)i + 8192);
        a[n++] = u & 127u;
        a[n++] = (u >> 7) & 127u;
    }
    for (i = 0; i < 16u; i++) {
        a[n++] = (uint8_t)(i % 3u ? 40u + i : 0u);  /* rests on 0, 3, 6 .. */
        a[n++] = (uint8_t)(i == 3u ? 4u : i % 3u ? 1u : 2u);   /* step 3: tie; slide on a rest drops */
    }
    return n;
}

/* a UPB1 bank as ORBIT 0.6 stored it, through its frozen layout: every slot used, np 61 */
static void fill_v1_bank(up_bank_v1_t *b, uint32_t bank)
{
    uint32_t i, k;
    memset(b, 0, sizeof *b);
    b->magic = UP_BANK_MAGIC_V1;
    b->rsize = UP_RECORD_V1;
    b->nslot = UP_PER_BANK;
    for (i = 0; i < UP_PER_BANK; i++) {
        up_rec_v1_t *r = &b->r[i];
        r->used = UP_USED;
        r->ver = UP_VER_V1;
        r->engine = (uint8_t)((bank * 16u + i) % NENGINES);
        r->np = 61;
        memcpy(r->name, "SLOT NAME 12", 12);
        r->name[11] = (char)('A' + i);
        for (k = 0; k < 61u; k++)
            r->p[k] = (int16_t)(-(int32_t)(bank * 1000u + i * 61u + k));
        r->note[i] = (uint8_t)(40u + i);
        r->flags[(i + 1u) % 16u] = 4u;               /* a tie */
    }
}

int main(void)
{
    static uint8_t stage[3840];
    static up_bank_v1_t b1;
    static up_bank_v2_t b2;
    static up_bank_t nb;
    uint8_t a[640];
    up_rec_t r, got;
    uint32_t n, slot = 99, i;
    int bad = 0, len, ok;
    int16_t v[P_COUNT], def[P_COUNT];
    memset(nor, 0xFF, sizeof nor);

    n = put_frame(a, 5, 2, "Bass One", -40);
    bad += check("UP_PUT frame < 640 bytes", 5u + n + 1u < 640u);
    bad += check("UP_PUT parses", up_parse(a, n, &r, &slot) == 0 && slot == 5u && r.engine == 2u &&
                                      up_valid(&r) && !memcmp(r.name, "Bass One", 8) && !r.name[8] &&
                                      r.ver == 3u && r.np == P_COUNT && r.macro[0] == MACRO_DEFAULT &&
                                      r.macro[3] == MACRO_DEFAULT);
    ok = 1;
    for (i = 0; i < P_COUNT; i++)
        ok &= r.p[i] == (int16_t)(-40 + (int32_t)i);
    bad += check("UP_PUT all 73 values (negative v14 too)", ok);
    bad += check("pattern: rest drops flags, tie has no note",
                 r.note[0] == 0 && r.flags[0] == 0 && r.note[1] == 41 && r.flags[1] == 1 && r.note[3] == 0 &&
                     r.flags[3] == 4);
    bad += check("UP_PUT short frame -> args", up_parse(a, n - 1u, &r, &slot) == 1);
    n = put_frame(a, 32, 0, "X", 0);
    bad += check("UP_PUT slot 32 -> args", up_parse(a, n, &r, &slot) == 1);
    n = put_frame(a, 0, NENGINES, "X", 0);
    bad += check("UP_PUT bad engine -> args", up_parse(a, n, &r, &slot) == 1);
    n = put_frame(a, 0, 0, "", 0);
    bad += check("UP_PUT empty name -> args", up_parse(a, n, &r, &slot) == 1);
    n = put_frame(a, 0, 0, "THIRTEEN CHRS", 0);
    bad += check("UP_PUT 13-char name -> args", up_parse(a, n, &r, &slot) == 1);
    n = put_frame(a, 0, 0, "TWELVE CHARS", 0);
    bad += check("UP_PUT 12-char name ok", up_parse(a, n, &r, &slot) == 0 && !memcmp(r.name, "TWELVE CHARS", 12));
    {
        char nm[13];
        *up_rec(31) = r;
        memcpy(up_rec(31)->name, "Low case", 9);
        up_name(31, nm);
        bad += check("name shown upper case", !strcmp(nm, "LOW CASE"));
    }

    /* bank round trip through storage.c */
    n = put_frame(a, 17, 3, "Keys", 7);
    up_parse(a, n, &r, &slot);
    up_bank[1].magic = UP_BANK_MAGIC;
    up_bank[1].rsize = sizeof(up_rec_t);
    up_bank[1].nslot = UP_PER_BANK;
    *up_rec(17) = r;
    bad += check("bank fits one object", sizeof(up_bank_t) <= ST_PAYLOAD_MAX);
    bad += check("bank save", st_save(OBJ_UPRESET0 + 1, &up_bank[1], sizeof up_bank[1]) == 0);
    memset(up_bank, 0, sizeof up_bank);
    len = st_load(OBJ_UPRESET0 + 1, stage, sizeof stage);
    bad += check("OPB3 bank object is 3176 bytes", len == (int)UP_BANK_SIZE && UP_RECORD == 198u);
    bad += check("bank decode", up_bank_decode(&up_bank[1], stage, len, 0) == 1);
    got = *up_rec(17);
    bad += check("bank load: the record is back", up_used(17) && !memcmp(&got, &r, sizeof r));
    bad += check("other slots empty", !up_used(16) && !up_used(18) && !up_used(0));
    len = st_load(OBJ_UPRESET0, stage, sizeof stage);
    bad += check("bank 0 never written -> not decoded", len < 0 && !up_bank_decode(&up_bank[0], stage, len, 0) &&
                                                         !up_used(0) && up_bank[0].magic == 0);
    bad += check("banks in 0xDC000..0xDFFFF", st_sector(OBJ_UPRESET0, 0) == 0xDC000u &&
                                                   st_sector(OBJ_UPRESET0 + 1, 1) == 0xDF000u &&
                                                   st_sector(OBJ_PROJECT0 + 3, 1) + 4096u <= 0xA0000u);
    memcpy(&nb, &up_bank[1], sizeof nb);
    nb.rsize = 192;                                         /* another record layout */
    bad += check("OPB2 header with record size 192 -> not decoded", !up_bank_decode(&up_bank[0], &nb, (int)sizeof nb, 0));
    nb.rsize = UP_RECORD;
    bad += check("OPB2 of the wrong length -> not decoded", !up_bank_decode(&up_bank[0], &nb, (int)sizeof nb - 4, 0));
    nb.r[1].ver = UP_VER + 1u;                              /* an unknown record version, nonempty */
    nb.r[1].used = UP_USED;
    bad += check("strict (restore): unknown record version rejects the bank", !up_bank_decode(&up_bank[0], &nb, (int)sizeof nb, 1));
    bad += check("boot: unknown record version reads empty, neighbour kept",
                 up_bank_decode(&up_bank[0], &nb, (int)sizeof nb, 0) && !up_used(1) && up_used(1u) == 0 &&
                 !memcmp(up_rec(1u), &(up_rec_t){0}, sizeof(up_rec_t)) && up_used(UP_PER_BANK + 1u) &&
                 !memcmp(&up_bank[0].r[1u + 0u], &(up_rec_t){0}, sizeof(up_rec_t)));
    memset(up_bank, 0, sizeof up_bank);
    up_bank[1].magic = UP_BANK_MAGIC;
    up_bank[1].rsize = sizeof(up_rec_t);
    up_bank[1].nslot = UP_PER_BANK;
    *up_rec(17) = r;
    up_rec(17)->ver = UP_VER + 1u;
    bad += check("record with another version -> empty", !up_used(17));
    up_rec(17)->ver = UP_VER_V1;
    bad += check("a version 1 record in an OPB2 bank -> empty (no other stride)", !up_used(17));

    /* UPB1 (ORBIT 0.6, 192-byte records): both banks, all 32 slots, into OPB3 without a loss */
    ok = 1;
    for (i = 0; i < 2u; i++) {
        uint32_t k, j;
        fill_v1_bank(&b1, i);
        ok &= up_bank_decode(&up_bank[i], &b1, (int)sizeof b1, 1) && up_bank[i].magic == UP_BANK_MAGIC &&
              up_bank[i].rsize == UP_RECORD;
        for (k = 0; k < UP_PER_BANK; k++) {
            const up_rec_t *q = &up_bank[i].r[k];
            ok &= up_used(i * 16u + k) && q->ver == UP_VER && q->np == 61u && q->engine == b1.r[k].engine &&
                  !memcmp(q->name, b1.r[k].name, 12) && !memcmp(q->note, b1.r[k].note, 16) &&
                  !memcmp(q->flags, b1.r[k].flags, 16) && !memcmp(q->p, b1.r[k].p, sizeof b1.r[k].p) &&
                  q->p[72] == 0;
            for (j = 0; j < MACRO_COUNT; j++)
                ok &= q->macro[j] == MACRO_DEFAULT && up_macro(q, j) == MACRO_DEFAULT;
        }
    }
    bad += check("UPB1 x2 -> OPB3: 32 slots, last slot, names, ties, negatives, macros DEFAULT", ok && up_used(31) && up_count_host() == 32u);
    for (i = 0; i < P_COUNT; i++)
        def[i] = (int16_t)(1000 + i);
    up_params(up_rec(31), v, def);
    ok = 1;
    for (i = 0; i < 53u; i++)
        ok &= v[i] == (int16_t)-(1000 + 15 * 61 + (int32_t)i);
    for (i = P_ITYPE; i < P_E0; i++)
        ok &= v[i] == def[i];
    for (i = 0; i < 8u; i++)
        ok &= v[P_E0 + i] == (int16_t)-(1000 + 15 * 61 + 53 + (int32_t)i);
    bad += check("UPB1 np 61 slot 32: common by id, 2.6 defaults, engine 53..60 -> 63..70", ok);
    fill_v1_bank(&b1, 0);
    b1.r[4].ver = 2;                                        /* a version 2 record inside UPB1: invalid */
    bad += check("strict: a version 2 record in UPB1 rejects the bank", !up_bank_decode(&nb, &b1, (int)sizeof b1, 1));
    bad += check("boot: that record reads empty, the others converted",
                 up_bank_decode(&nb, &b1, (int)sizeof b1, 0) && !nb.r[4].used && nb.r[5].used == UP_USED);
    fill_v1_bank(&b1, 0);
    bad += check("UPB1 of the OPB2 length / OPB2 magic on a UPB1 body: refused",
                 !up_bank_decode(&nb, &b1, (int)UP_BANK_SIZE, 0) &&
                 !up_bank_decode(&nb, &(up_bank_v1_t){.magic = UP_BANK_MAGIC, .rsize = UP_RECORD_V1, .nslot = 16},
                                 (int)sizeof b1, 0));
    bad += check("an empty / short / unknown bank: refused", !up_bank_decode(&nb, stage, 0, 1) &&
                                                             !up_bank_decode(&nb, stage, 4, 0));

    /* Frozen OPB2: both banks, every slot, np 71 and the formerly future np 72. */
    ok = 1;
    for (i = 0; i < 2u; i++) {
        uint32_t k, j;
        memset(&b2, 0, sizeof b2);
        b2.magic = UP_BANK_MAGIC_V2, b2.rsize = UP_RECORD_V2, b2.nslot = UP_PER_BANK;
        for (k = 0; k < UP_PER_BANK; k++) {
            up_rec_v2_t *q = &b2.r[k];
            q->used = UP_USED, q->ver = UP_VER_V2, q->engine = (uint8_t)((i * 16u + k) % NENGINES);
            q->np = (uint8_t)(71u + k % 2u);
            memcpy(q->name, "OLD BANK", 9);
            for (j = 0; j < UP_PMAX_V2; j++)
                q->p[j] = (int16_t)(-32000 + (int32_t)(i * 1000u + k * 72u + j));
            q->note[k] = (uint8_t)(40u + k), q->flags[k] = 3;
            q->macro[0] = 62, q->macro[1] = (uint8_t)(q->np - 8u);
            q->macro[2] = (uint8_t)(q->np - 1u), q->macro[3] = MACRO_DEFAULT;
        }
        ok &= st_save(OBJ_UPRESET0 + i, &b2, sizeof b2) == 0 &&
              (len = st_load(OBJ_UPRESET0 + i, stage, sizeof stage)) == UP_BANK_SIZE_V2 &&
              up_bank_decode(&up_bank[i], stage, len, 1);
        for (k = 0; k < UP_PER_BANK; k++) {
            const up_rec_t *q = &up_bank[i].r[k];
            ok &= up_used(i * 16u + k) && q->ver == UP_VER && q->np == b2.r[k].np &&
                  q->engine == b2.r[k].engine && !memcmp(q->name, b2.r[k].name, 12) &&
                  !memcmp(q->p, b2.r[k].p, sizeof b2.r[k].p) && q->p[72] == 0 &&
                  !memcmp(q->note, b2.r[k].note, 16) && !memcmp(q->flags, b2.r[k].flags, 16) &&
                  !memcmp(q->macro, b2.r[k].macro, MACRO_COUNT) &&
                  up_macro(q, 0) == P_LPOL && up_macro(q, 1) == P_E0 && up_macro(q, 2) == P_E7;
        }
        ok &= st_save(OBJ_UPRESET0 + i, &up_bank[i], sizeof up_bank[i]) == 0 &&
              (len = st_load(OBJ_UPRESET0 + i, stage, sizeof stage)) == UP_BANK_SIZE &&
              up_bank_decode(&nb, stage, len, 1) && !memcmp(&nb, &up_bank[i], sizeof nb);
    }
    bad += check("OPB2 x2 -> OPB3 -> flash: all 32 sounds, patterns and macros intact", ok);
    b2.r[4].np = 73;
    bad += check("OPB2 np73 cannot exceed frozen p72, strict rejects",
                 !up_bank_decode(&nb, &b2, sizeof b2, 1));
    bad += check("OPB2 np73 boot empties only bad slot",
                 up_bank_decode(&nb, &b2, sizeof b2, 0) && !nb.r[4].used && up_valid(&nb.r[5]));
    b2.r[4].np = 72, b2.r[4].ver = UP_VER;
    bad += check("v3 in OPB2 is not reinterpreted", !up_bank_decode(&nb, &b2, sizeof b2, 1));
    b2.r[4].ver = UP_VER_V2;
    bad += check("OPB2 truncated or new stride refused", !up_bank_decode(&nb, &b2, sizeof b2 - 1, 1) &&
                 !up_bank_decode(&nb, &b2, UP_BANK_SIZE, 1));
    /* Macros by count, including the prior np72 future mapping test. */
    memset(&r, 0, sizeof r);
    r.used = UP_USED, r.ver = UP_VER, r.engine = 0, r.np = 61, r.name[0] = 'M';
    r.macro[0] = 53, r.macro[1] = 60, r.macro[2] = 52, r.macro[3] = 61;
    bad += check("macro ids by count (np 61)", up_macro(&r, 0) == P_E0 && up_macro(&r, 1) == P_E7 &&
                                                  up_macro(&r, 2) == P_VLEAD && up_macro(&r, 3) == MACRO_DEFAULT);
    r.np = 71;
    r.macro[0] = 62, r.macro[1] = 63, r.macro[2] = 70, r.macro[3] = 71;
    bad += check("macro ids by count (np 71): 62 -> P_LPOL, 63 -> P_E0, 70 -> P_E7, 71 unknown",
                 up_macro(&r, 0) == P_LPOL && up_macro(&r, 1) == P_E0 && up_macro(&r, 2) == P_E7 &&
                 up_macro(&r, 3) == MACRO_DEFAULT);
    r.np = 72;
    r.macro[0] = 63, r.macro[1] = 64, r.macro[2] = 71, r.macro[3] = MACRO_DEFAULT;
    bad += check("np72: common63 now RES, engine64..71 -> E0..E7",
                 up_macro(&r, 0) == P_TRES && up_macro(&r, 1) == P_E0 && up_macro(&r, 2) == P_E7 &&
                 up_macro(&r, 3) == MACRO_DEFAULT);
    r.np = P_COUNT;
    r.macro[0] = P_TRES, r.macro[1] = P_TFTYPE, r.macro[2] = P_LPOL, r.macro[3] = P_E3;
    r.p[P_TRES] = 117, r.p[P_TFTYPE] = 1;
    memset(&nb, 0, sizeof nb);
    nb.magic = UP_BANK_MAGIC, nb.rsize = UP_RECORD, nb.nslot = UP_PER_BANK;
    nb.r[15] = r;
    nb.r[14] = r;
    nb.r[14].macro[1] = 99;                                 /* (an id of no parameter: DEFAULT) */
    bad += check("OPB3 RES/BP values and macros kept through flash (last slot)",
                 st_save(OBJ_UPRESET0, &nb, sizeof nb) == 0 && (len = st_load(OBJ_UPRESET0, stage, sizeof stage)) > 0 &&
                 up_bank_decode(&up_bank[0], stage, len, 0) && !memcmp(&up_bank[0].r[15], &r, sizeof r) &&
                 up_macro(&up_bank[0].r[15], 0) == P_TRES && up_macro(&up_bank[0].r[15], 1) == P_TFTYPE &&
                 up_bank[0].r[14].macro[1] == MACRO_DEFAULT && up_bank[0].r[14].macro[2] == P_LPOL);

    /* map by count: a record from a build with 2 parameters fewer */
    for (i = 0; i < P_COUNT; i++)
        def[i] = (int16_t)(1000 + i);
    r.np = P_COUNT - 2u;
    for (i = 0; i < r.np; i++)
        r.p[i] = (int16_t)i;
    up_params(&r, v, def);
    ok = 1;
    for (i = 0; i < P_E0; i++)
        ok &= v[i] == (i < P_E0 - 2u ? (int16_t)i : def[i]);
    for (i = 0; i < 8u; i++)
        ok &= v[P_E0 + i] == (int16_t)(P_E0 - 2u + i);
    bad += check("np < P_COUNT: mapped by count", ok);
    /* a record saved before the SLICER (P_COUNT 53, P_E0 45): the four SLICER parameters (just
     * before P_E0) take their defaults, everything else keeps its id */
    r.np = 53;
    for (i = 0; i < 53u; i++)
        r.p[i] = (int16_t)(2000 + i);
    up_params(&r, v, def);
    ok = P_SLCR == 45 && P_SLDEPTH + 1 == P_CHORD && P_CHORD == 49 && P_TFLT == 50 && P_VLEAD == 52 && P_E0 == 65;
    for (i = 0; i < 45u; i++)
        ok &= v[i] == (int16_t)(2000 + i);
    for (i = P_SLCR; i < P_E0; i++)
        ok &= v[i] == def[i];
    for (i = 0; i < 8u; i++)
        ok &= v[P_E0 + i] == (int16_t)(2000 + 45 + i);
    bad += check("old record (np 53): SLICER, CHORD and FILTER defaults, E0..E7 kept", ok);
    /* a record of SLOOP 2.0 .. 2.3 (P_COUNT 58, P_E0 50): only the FILTER (2.4) takes its default */
    r.np = 58;
    for (i = 0; i < 58u; i++)
        r.p[i] = (int16_t)(3000 + i);
    up_params(&r, v, def);
    ok = 1;
    for (i = 0; i < P_TFLT; i++)
        ok &= v[i] == (int16_t)(3000 + i);
    ok &= v[P_TFLT] == def[P_TFLT] && v[P_STRUM] == def[P_STRUM] && v[P_VLEAD] == def[P_VLEAD];
    for (i = 0; i < 8u; i++)
        ok &= v[P_E0 + i] == (int16_t)(3000 + 50 + i);
    bad += check("2.3 record (np 58): FILTER, STRUM, VLEAD defaults, CHORD and E0..E7 kept", ok);
    /* a record of SLOOP 1.0 (P_COUNT 57, P_E0 49): CHORD (SLOOP 2.0) takes its default */
    r.np = 57;
    for (i = 0; i < 57u; i++)
        r.p[i] = (int16_t)(3000 + i);
    up_params(&r, v, def);
    ok = 1;
    for (i = 0; i < P_CHORD; i++)
        ok &= v[i] == (int16_t)(3000 + i);
    ok &= v[P_CHORD] == def[P_CHORD];
    for (i = 0; i < 8u; i++)
        ok &= v[P_E0 + i] == (int16_t)(3000 + 49 + i);
    bad += check("SLOOP 1.0 record (np 57): CHORD default, the rest kept", ok);
    r.np = P_COUNT;
    for (i = 0; i < r.np; i++)
        r.p[i] = (int16_t)i;
    up_params(&r, v, def);
    ok = 1;
    for (i = 0; i < P_COUNT; i++)
        ok &= v[i] == (int16_t)i;
    bad += check("np == P_COUNT: every sound value as stored", ok);

    {   /* steps -> pattern (UP_STORE) */
        step_t st[NSTEP];
        memset(st, 0, sizeof st);
        for (i = 0; i < 16u; i++)
            st[i].time = ST_REST;
        st[0] = (step_t){{60, 64, 67, 0}, 3, ST_NOTE, SF_ACCENT | SF_SLIDE, 100};
        st[1] = (step_t){{0}, 0, ST_TIE, 0, 0};
        st[2] = (step_t){{50}, 0, ST_NOTE, SF_ACCENT, 0};   /* n = 0: empty */
        st[20] = (step_t){{70}, 1, ST_NOTE, 0, 90};         /* beyond 16: not stored */
        up_pat_from(&r, st);
        bad += check("steps -> pattern", r.note[0] == 60 && r.flags[0] == 3 && r.note[1] == 0 && r.flags[1] == 4 &&
                                             r.note[2] == 0 && r.flags[2] == 0 && !up_pat_empty(&r));
        memset(st, 0, sizeof st);
        up_pat_from(&r, st);
        bad += check("empty sequencer -> empty pattern", up_pat_empty(&r));
    }
    printf("%s\n", bad ? "USER PRESET TEST FAILED" : "user preset test passed");
    return bad != 0;
}

