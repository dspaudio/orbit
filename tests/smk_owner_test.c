/* SPDX-License-Identifier: GPL-3.0-only
 * SMK-37 port: Copyright (C) 2026 M. Geisler (genkigenki/makkuro) <mcgeisler@gmail.com> */
/* SMK-37 store owner (firmware/src/storage.c st_owner_check, build with -DBOARD_SMK37=1) on a simulated NOR:
 * the tag "SMK37DATA" + ORBIT is written once; ORBIT's own store loads as before; another port's store
 * (SLOOP's tag) and untagged data (the stock settings) are cleared; nothing outside the store is touched. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static uint8_t nor[0x100000];
static uint32_t erases_outside;

static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off)
{
    if (off < 0x9D000u || off >= 0xBB000u)
        erases_outside++;
    memset(nor + off, 0xFF, 4096);
    return 0;
}
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    uint32_t i;
    for (i = 0; i < n; i++)
        nor[off + i] &= s[i];
    return 0;
}
#include "../firmware/src/storage.c"

static int fails;
static void check(int ok, const char *what)
{
    printf("%-60s %s\n", what, ok ? "ok" : "FAIL");
    fails += !ok;
}

static void tag(const char *fam)
{
    char t[16] = "SMK37DATA";
    memset(nor + ST_OWNER_OFF, 0xFF, 4096);
    memcpy(t + 9, fam, strlen(fam) < 7 ? strlen(fam) : 7);
    if (strlen(fam) < 7)
        memset(t + 9 + strlen(fam), 0, 7 - strlen(fam));
    memcpy(nor + ST_OWNER_OFF, t, 16);
}

int main(void)
{
    char a[600], got[600];
    static const char WANT[16] = "SMK37DATAORBIT\0";
    memset(nor, 0xFF, sizeof nor);
    memset(a, 'A', sizeof a);
    check(!strcmp(FELUCCA_DATA_FAMILY, "ORBIT") && !FELUCCA_DATA_UNTAGGED_OK, "family ORBIT, untagged data not kept");
    check(ST_OWNER_OFF == 0xAF000u, "the tag sits after the objects (0xAF000)");

    st_owner_check();                                   /* empty flash (erased, as untagged): tagged */
    check(!memcmp(nor + ST_OWNER_OFF, WANT, 16), "empty flash: tagged SMK37DATA ORBIT");
    st_owner_cleared = 0;
    check(st_save(OBJ_PROJECT0, a, sizeof a) == 0, "save a project");
    st_owner_check();                                   /* ours: kept */
    check(st_load(OBJ_PROJECT0, got, sizeof got) == (int)sizeof a && !memcmp(got, a, sizeof a) && !st_owner_cleared,
          "ORBIT's own store: loads as before");

    tag("SLOOP");                                       /* another port's store */
    st_owner_check();
    check(st_load(OBJ_PROJECT0, got, sizeof got) < 0 && st_owner_cleared, "a SLOOP store: cleared");
    check(!memcmp(nor + ST_OWNER_OFF, WANT, 16), "... and tagged ORBIT");

    st_owner_cleared = 0;
    check(st_save(OBJ_PROJECT0, a, sizeof a) == 0, "save again");
    memset(nor + ST_OWNER_OFF, 0xFF, 4096);             /* untagged (the stock settings, an older build) */
    st_owner_check();
    check(st_load(OBJ_PROJECT0, got, sizeof got) < 0 && st_owner_cleared, "untagged data: cleared");
    check(!memcmp(nor + ST_OWNER_OFF, WANT, 16), "... and tagged ORBIT");
    check(erases_outside == 0, "no erase outside the store window 0x9D000..0xBAFFF");
    printf("smk owner: %s\n", fails ? "FAIL" : "ok");
    return fails != 0;
}
