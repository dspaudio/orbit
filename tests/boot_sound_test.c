/* SPDX-License-Identifier: GPL-3.0-only */
/* Power-on sounds on the real storage, project and UI code (tests/editor_host.c). Ported from SLOOP 2.8.4,
 * isod89/sloop-fm1 91f3018 (tests/boot_sound_test.c), adapted to ORBIT:
 *   1. a first install (nothing in the flash): every part sounds as its default sound (TRK_DEF): the values the
 *      boot leaves equal the ones the same sound gets when it is loaded again, and key C4 plays the same note
 *   2. a power-on after an update (the autosave and a stored project in the flash): the same, for the sounds the
 *      working project had
 *   3. a working project ORBIT cannot read, left in the autosave: SLOOP's FUN6 (another engine namespace), and an
 *      ORP8 project with its checksum right but an engine this firmware does not have. Upstream maps a foreign
 *      engine to the default sound after accepting the project; ORBIT refuses the whole project instead
 *      (project.c proj_import / proj_ok_as). Either way: every part boots with its default sound and an empty
 *      pattern, and the valid project stored in section slot 1 is kept, byte for byte, and still loads.
 * Exit status: failed checks. */
#include "editor_host.c"
static int fails;
static void check(int c, const char *what) { printf("boot_sound: %-96s %s\n", what, c ? "ok" : "FAIL"); fails += !c; }
static void boot_as_main(void)
{
    boot_main();
}
static void compare(const char *when)
{
    uint32_t k, i;
    char b[160];
    for (k = 0; k < NPART; k++) {
        int16_t now[P_COUNT];
        uint32_t e = trk[k].eng_req, pi = trk[k].preset, bad = 0;
        memcpy(now, trk[k].p, sizeof now);
        apply_preset_to(&trk[k], pi);                 /* the same sound loaded again */
        for (i = 0; i < P_COUNT; i++)
            if (now[i] != trk[k].p[i]) {
                if (bad++ < 6)
                    printf("boot_sound: %s track %u (%s %s): p[%u] %s %d, loaded again %d\n", when, k + 1, ENGINES[e]->name,
                           ENGINES[e]->presets[pi % ENGINES[e]->npresets].name, i, i < P_E0 ? TP[i].label : "E", now[i], trk[k].p[i]);
            }
        snprintf(b, sizeof b, "%s: track %u plays its sound as loaded (every value)", when, k + 1);
        check(!bad, b);
        memcpy(trk[k].p, now, sizeof now);
    }
}
/* the FM-1's key C4 (key 7) on track k: the note its voice plays */
static int key_note(uint32_t k)
{
    uint32_t i, f;
    int n = -1;
    song.sel = (uint8_t)k;
    for (f = 0; f < 4; f++) ui_frame();
    fm1_in.notes = 1u << 7;
    for (f = 0; f < 6; f++) ui_frame();
    for (i = 0; i < NVOICE; i++) if (trk[k].v[i].active && trk[k].v[i].gate) n = trk[k].v[i].note;
    fm1_in.notes = 0;
    for (f = 0; f < 30; f++) ui_frame();
    return n;
}
static void key_check(const char *when)
{
    uint32_t k;
    char b[160];
    for (k = 0; k < NPART; k++) {
        int a = key_note(k), c;
        printf("boot_sound: %s track %u: octave %d, TRN %d, key C4 plays note %d\n", when, k + 1, song.octave, trk[k].p[P_TRANS], a);
        apply_preset_to(&trk[k], trk[k].preset);
        c = key_note(k);
        printf("boot_sound: %s track %u: the sound loaded again, key C4 plays note %d\n", when, k + 1, c);
        snprintf(b, sizeof b, "%s: track %u, key C4 plays the same note as with the sound loaded again", when, k + 1);
        check(a >= 0 && a == c, b);
    }
}
/* every part on its default sound (TRK_DEF), its pattern empty */
static void default_check(const char *when)
{
    uint32_t k, i, notes = 0;
    char b[160];
    for (k = 0; k < NPART; k++) {
        printf("boot_sound: %s track %u: %s %s, TRN %d\n", when, k + 1, ENGINES[trk[k].eng_req]->name,
               ENGINES[trk[k].eng_req]->presets[trk[k].preset % ENGINES[trk[k].eng_req]->npresets].name, trk[k].p[P_TRANS]);
        snprintf(b, sizeof b, "%s: track %u boots with its default sound (TRK_DEF)", when, k + 1);
        check(trk[k].eng_req == TRK_DEF[k][0] && trk[k].engine == TRK_DEF[k][0] && trk[k].preset == TRK_DEF[k][1], b);
        for (i = 0; i < NSTEP; i++) notes += trk[k].step[i].time == ST_NOTE;
    }
    snprintf(b, sizeof b, "%s: nothing of the refused project is played (empty patterns)", when);
    check(notes == 0, b);
}
/* the project stored in section slot 1 before: still there (RAM and flash), byte for byte, and it loads */
static project_t kept;
static void kept_check(const char *when)
{
    static project_t fl;
    char b[160];
    int n = st_load(OBJ_PROJECT0, &fl, sizeof fl);
    snprintf(b, sizeof b, "%s: the valid ORP8 project in slot 1 is kept (RAM and flash, byte for byte)", when);
    check(project_used(0) && !memcmp(&proj_slot[0], &kept, sizeof kept) && n == (int)sizeof kept && !memcmp(&fl, &kept, sizeof kept), b);
    project_load(0);
    audio_blocks(2);
    snprintf(b, sizeof b, "%s: ... and loads: track 1 on its engine and preset, its pattern", when);
    check(trk[0].eng_req == kept.t[0].engine && trk[0].preset == kept.t[0].preset && trk[0].step[0].time == ST_NOTE &&
          trk[0].step[0].note[0] == 60, b);
}
/* a power-off: .noinit (the RAM project slots) does not survive it */
static void power_off(void)
{
    memset(proj_slot, 0, sizeof proj_slot);
}
int main(void)
{
    uint32_t i;
    harness_init();
    memset(nor, 0xFF, sizeof nor);               /* 1. a first install: nothing in the flash */
    power_off();
    boot_as_main();
    default_check("first power-on");
    compare("first power-on");
    boot_as_main();
    boot_as_main();
    key_check("first power-on");
    {   /* a project stored in section slot 1: track 1 on another sound, a pattern */
        uint32_t e = (TRK_DEF[0][0] + 1u) % NENGINES;
        uint8_t f[6] = {ED_HDR0, ED_HDR1, ED_HDR2, ED_PRESET, (uint8_t)e, 1};
        song.sel = 0;
        ed_inject(f, 6);
        audio_blocks(40);
        trk[0].step[0].time = ST_NOTE; trk[0].step[0].n = 1; trk[0].step[0].note[0] = 60; trk[0].step[0].vel = 100;
        project_save(0);
        persist_flush_now();
        kept = proj_slot[0];
        check(project_used(0) && kept.t[0].engine == e, "a valid ORP8 project stored in slot 1 (track 1 on another engine)");
    }
    for (i = 0; i < 400; i++) ui_frame();         /* (an autosave) */
    persist_flush_now();
    power_off();
    boot_as_main();                               /* 2. an update: the flash as it was */
    compare("power-on after an update");
    boot_as_main();
    boot_as_main();
    key_check("power-on after an update");
    {   /* 3a. SLOOP's working project (FUN6) left in the autosave: track 1 on engine 12 (Clipper's HYBRID), a pattern */
        static project_t q;
        proj_capture(&q);
        q.magic = PROJ_MAGIC_FUN6;
        q.t[0].engine = 12;
        q.t[0].preset = 0;
        pt_set(&q.t[0], P_TRANS, 0);
        q.t[0].step[0].time = ST_NOTE; q.t[0].step[0].n = 1; q.t[0].step[0].note[0] = 48;
        q.sum = proj_sum(&q);
        st_save(OBJ_AUTOSAVE, &q, sizeof q);
        power_off();
        boot_as_main();
        default_check("over SLOOP's FUN6 working project");
        compare("over SLOOP's FUN6 working project");
        key_check("over SLOOP's FUN6 working project");
        kept_check("over SLOOP's FUN6 working project");
    }
    {   /* 3b. an ORP8 working project, its checksum right, track 1 on an engine this firmware does not have */
        static project_t q;
        proj_capture(&q);
        q.magic = PROJ_MAGIC;
        q.t[0].engine = NENGINES;
        q.t[0].preset = 0;
        q.t[0].step[0].time = ST_NOTE; q.t[0].step[0].n = 1; q.t[0].step[0].note[0] = 48;
        q.sum = proj_sum(&q);
        st_save(OBJ_AUTOSAVE, &q, sizeof q);
        power_off();
        boot_as_main();
        default_check("over an ORP8 project with engine NENGINES");
        compare("over an ORP8 project with engine NENGINES");
        key_check("over an ORP8 project with engine NENGINES");
        kept_check("over an ORP8 project with engine NENGINES");
    }
    printf("boot_sound: %d failed\n", fails);
    return fails != 0;
}
