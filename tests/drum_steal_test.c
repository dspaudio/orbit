/* SPDX-License-Identifier: GPL-3.0-only */
/* Drum voice allocation (SLOOP 2.8.3 behaviour): a hit takes a free voice; else the voice of its own sound if that
 * sound still rings (it starts over); else the oldest voice, a kick last. Checked on the GM sample kit, a synthesised
 * kit and a SYN user kit, by hit order alone (no rendering). Exit status: the number of failed checks. */
#define main hostsim_main
#include "hostsim.c"
#undef main
static uint32_t fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL kit %u: ", kit); printf(__VA_ARGS__); printf("\n"); } } while (0)
static int find(uint32_t note)
{
    uint32_t i;
    for (i = 0; i < NDRUM; i++)
        if (drums.v[i].active && drums.v[i].note == note)
            return (int)i;
    return -1;
}
static uint32_t nactive(void)
{
    uint32_t i, n = 0;
    for (i = 0; i < NDRUM; i++)
        n += drums.v[i].active;
    return n;
}
static void fill(uint32_t kit, const uint8_t *notes)
{
    uint32_t i;
    memset(&drums, 0, sizeof drums);
    drums.set = -2;
    TDRUM->p[P_E0] = (int16_t)kit;
    for (i = 0; i < NDRUM; i++)
        drum_on(notes[i], 100);
}
static void run_kit(uint32_t kit)
{
    static const uint8_t busy[NDRUM] = {36, 42, 38, 39, 49, 51};     /* the kick is the oldest */
    static const uint8_t kicks[NDRUM] = {35, 36, 35, 36, 35, 36};
    uint32_t i;
    int k, h;
    fill(kit, busy);
    CHECK(nactive() == NDRUM, "setup: %u voices busy", nactive());
    for (i = 0; i < NDRUM; i++)
        CHECK(drums.v[i].note == busy[i], "free voices taken in order: voice %u has note %u", i, drums.v[i].note);
    /* a sound that still rings starts over in its own voice, no other sound is cut */
    h = find(42);
    drum_on(42, 100);
    CHECK(find(42) == h && nactive() == NDRUM && find(36) >= 0 && find(51) >= 0, "hat re-hit does not use its own voice");
    /* a new sound steals the oldest, but not the kick */
    drum_on(37, 100);
    k = find(36);
    CHECK(k == 0, "kick lost its voice to a new sound");
    CHECK(find(37) == 2 && find(38) < 0 && find(42) == h, "oldest non-kick (the snare) not stolen: 37 in voice %d", find(37));
    /* a kick re-hit takes its own voice */
    fill(kit, busy);
    drum_on(36, 100);
    CHECK(find(36) == 0 && nactive() == NDRUM && find(42) == 1, "kick re-hit did not take its own voice");
    /* all kicks: the oldest of them goes */
    fill(kit, kicks);
    drum_on(38, 100);
    CHECK(find(38) == 0, "all kicks: oldest kick not stolen (voice %d)", find(38));
}
int main(void)
{
    uint32_t kit;
    host_tracks_init();
    for (kit = 0; kit < DRUM_KITS; kit++)
        if (kit == 0 || kit == DRUM_SAMPLED || kit == DRUM_SYN) {
            uint32_t before = fails;
            run_kit(kit);
            printf("drum_steal: kit %u %s\n", kit, fails == before ? "ok" : "FAIL");
        }
    printf("drum_steal: %u checks failed\n", fails);
    return (int)fails;
}
