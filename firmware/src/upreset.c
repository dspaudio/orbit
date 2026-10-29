/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* User presets (editor protocol v2, cmds 16-21; the SAVE > USER page): 32
 * slots in two storage.c objects (OBJ_UPRESET0/1, 0xDC000..0xDFFFF), 16
 * records each, mirrored in RAM so browsing never reads flash. A record:
 * engine, name, the instrument parameters, a 16-step pattern.
 *
 * Banks: "OPB3" (ORBIT 0.8, written): 198-byte records of version 3, all 73
 * sound values plus four HOME macro assignments. "OPB2" (0.7, frozen):
 * 196-byte version 2 records with 72 values. "UPB1" (before, frozen): 192-byte
 * records of version 1, no assignments; read and converted in RAM (macros: the
 * engine's own), written as OPB3 at the next save. A bank of another shape reads
 * as empty; so does a record of another version (never reinterpreted). A record
 * keeps np = the P_COUNT it was stored with; when that differs it is mapped by
 * count: its last 8 values are P_E0..P_E7, the first np - 8 are P_LEVEL.. in
 * order, missing ones take their defaults; a macro id the same way. So common
 * parameters may only be added just before P_E0.
 *
 * With -DUP_HOST (host test) only the part above #ifndef UP_HOST is built;
 * it needs nothing but core.h. */
#define UP_PER_BANK 16u
#define UP_PMAX 73u
#define UP_USED 0xA5u
#define UP_VER 3u
#define UP_BANK_MAGIC 0x3342504Fu                /* "OPB3" (bytes 4F 50 42 33) */
#define UP_RECORD 198u
#define UP_BANK_SIZE 3176u
#define UP_PMAX_V2 72u
#define UP_VER_V2 2u
#define UP_BANK_MAGIC_V2 0x3242504Fu             /* "OPB2": read only */
#define UP_RECORD_V2 196u
#define UP_BANK_SIZE_V2 3144u
#define UP_VER_V1 1u
#define UP_BANK_MAGIC_V1 0x31425055u             /* "UPB1": read only */
#define UP_RECORD_V1 192u
#define UP_BANK_SIZE_V1 3080u
typedef struct {
    uint8_t used, ver, engine, np;               /* UP_USED, UP_VER, engine, P_COUNT when stored */
    char name[12];                               /* ASCII 32..126, 0-padded (no 0 when 12 long) */
    int16_t p[UP_PMAX];
    uint8_t note[16], flags[16];                 /* note 0 = rest; flags 1 accent, 2 slide, 4 tie */
    uint8_t macro[MACRO_COUNT];                  /* HOME KNOB 1..4: a P_* id (np's numbering), MACRO_DEFAULT */
} up_rec_t;
typedef struct {
    uint32_t magic;
    uint16_t rsize, nslot;
    up_rec_t r[UP_PER_BANK];
} up_bank_t;
typedef struct {                                 /* version 1 (until ORBIT 0.6), frozen */
    uint8_t used, ver, engine, np;
    char name[12];
    int16_t p[UP_PMAX_V2];
    uint8_t note[16], flags[16];
} up_rec_v1_t;
typedef struct {                                 /* version 2 (ORBIT 0.7), frozen */
    uint8_t used, ver, engine, np;
    char name[12];
    int16_t p[UP_PMAX_V2];
    uint8_t note[16], flags[16], macro[MACRO_COUNT];
} up_rec_v2_t;
typedef struct {
    uint32_t magic;
    uint16_t rsize, nslot;
    up_rec_v2_t r[UP_PER_BANK];
} up_bank_v2_t;
typedef struct {
    uint32_t magic;
    uint16_t rsize, nslot;
    up_rec_v1_t r[UP_PER_BANK];
} up_bank_v1_t;
_Static_assert(sizeof(up_rec_t) == UP_RECORD && __builtin_offsetof(up_rec_t, p) == 16u &&
               __builtin_offsetof(up_rec_t, note) == 162u && __builtin_offsetof(up_rec_t, flags) == 178u &&
               __builtin_offsetof(up_rec_t, macro) == 194u && sizeof(up_bank_t) == UP_BANK_SIZE &&
               __builtin_offsetof(up_bank_t, r) == 8u, "OPB3 record and bank layout");
_Static_assert(sizeof(up_rec_v2_t) == UP_RECORD_V2 && sizeof(up_bank_v2_t) == UP_BANK_SIZE_V2 &&
               __builtin_offsetof(up_rec_v2_t, note) == 160u && __builtin_offsetof(up_rec_v2_t, macro) == 192u,
               "OPB2 as stored");
_Static_assert(sizeof(up_rec_v1_t) == UP_RECORD_V1 && sizeof(up_bank_v1_t) == UP_BANK_SIZE_V1, "UPB1 as stored");
_Static_assert(P_COUNT <= UP_PMAX && P_COUNT < 128, "user preset record: P_COUNT");
#if defined(UP_HOST) || !FELUCCA_FLASH || !defined(ST_XIP_PTR)
#define UP_RAM 1                                 /* host builds without the flash window: a RAM mirror */
static up_bank_t up_bank[UP_SLOTS / UP_PER_BANK];

static up_rec_t *up_rec(uint32_t k) { return &up_bank[k / UP_PER_BANK].r[k % UP_PER_BANK]; }
#else
/* SLOOP 2.7 (sloop-lab): no RAM mirror (6,352 B back); the banks are read where they are, in the flash's plain
 * XIP window (storage.c st_xip), as the samples and the FM6 bank already are. up_boot keeps every bank there in
 * the exact form the mirror held (OPB3, cleared records cleared); a write is staged in the project buffer */
#define UP_RAM 0
static const up_bank_t *up_bank_xip[UP_SLOTS / UP_PER_BANK];   /* each bank's current copy in the window, 0 = none */
static const up_rec_t up_rec_none;                              /* an empty slot (up_valid says no) */
static const up_rec_t *up_rec(uint32_t k)
{
    const up_bank_t *bk = up_bank_xip[(k / UP_PER_BANK) % NELEM(up_bank_xip)];
    return bk ? &bk->r[k % UP_PER_BANK] : &up_rec_none;
}
static void *up_stage(void);                                    /* project.c: the project buffer (main loop) */
#endif

static int up_valid(const up_rec_t *r)
{
    return r->used == UP_USED && r->ver == UP_VER && r->engine < NENGINES && r->np >= 8u && r->np <= UP_PMAX &&
           r->name[0];
}
static int up_empty(const void *r, uint32_t n)    /* a record of all zeros: an empty slot */
{
    const uint8_t *b = (const uint8_t *)r;
    uint32_t i;
    for (i = 0; i < n; i++)
        if (b[i])
            return 0;
    return 1;
}
/* the record's macro k in today's numbering, MACRO_DEFAULT when it has none this firmware knows */
static uint32_t up_macro(const up_rec_t *r, uint32_t k)
{
    uint32_t m = r->macro[k & 3u], id;
    if (m == MACRO_DEFAULT)
        return MACRO_DEFAULT;
    id = param_id_by_count(m, r->np);
    return id < P_COUNT ? id : MACRO_DEFAULT;
}

static int up_used(uint32_t k) { return k < UP_SLOTS && up_valid(up_rec(k)); }

/* the record's values in today's P_* order (mapped by count, see above); def = the defaults */
static void up_params(const up_rec_t *r, int16_t *out, const int16_t *def)
{
    uint32_t i, nc = r->np - 8u;
    for (i = 0; i < P_E0; i++)
        out[i] = i < nc ? r->p[i] : def[i];
    for (i = 0; i < 8u; i++)
        out[P_E0 + i] = r->p[nc + i];
}

/* a stored bank (OPB3, OPB2 or UPB1) -> dst as OPB3. strict (a backup restore): a nonempty
 * record that is not valid rejects the bank; else (boot) it reads as empty, its neighbours kept. 1 = done; 0 = not a
 * bank this firmware reads (dst untouched). src may be dst (a restore converted where it was staged): the records
 * grow (192 / 196 -> 198 B), so they are converted from the last one down, each read before its place is written */
static int up_bank_decode(up_bank_t *dst, const void *src, int n, int strict)
{
    uint32_t magic, i, k;
    if (n < 8)
        return 0;
    memcpy(&magic, src, 4);
    if (magic == UP_BANK_MAGIC) {
        const up_bank_t *b = (const up_bank_t *)src;
        if (n != (int)UP_BANK_SIZE || b->rsize != UP_RECORD || b->nslot != UP_PER_BANK)
            return 0;
        for (i = 0; strict && i < UP_PER_BANK; i++)
            if (!up_empty(&b->r[i], UP_RECORD) && !up_valid(&b->r[i]))
                return 0;
        if ((const void *)dst != src)
            memcpy(dst, b, sizeof *dst);
        for (i = 0; i < UP_PER_BANK; i++) {
            up_rec_t *r = &dst->r[i];
            if (!up_valid(r)) {
                memset(r, 0, sizeof *r);
                continue;
            }
            for (k = 0; k < MACRO_COUNT; k++)               /* (an id of no parameter: the engine's own) */
                if (r->macro[k] != MACRO_DEFAULT && param_id_by_count(r->macro[k], r->np) >= P_COUNT)
                    r->macro[k] = MACRO_DEFAULT;
        }
        return 1;
    }
    if (magic == UP_BANK_MAGIC_V1 || magic == UP_BANK_MAGIC_V2) {
        const uint8_t *b = (const uint8_t *)src;
        uint16_t rsize, nslot;
        uint32_t stride = magic == UP_BANK_MAGIC_V1 ? UP_RECORD_V1 : UP_RECORD_V2;
        uint32_t ver = magic == UP_BANK_MAGIC_V1 ? UP_VER_V1 : UP_VER_V2;
        memcpy(&rsize, b + 4, 2);
        memcpy(&nslot, b + 6, 2);
        if (n != (int)(8u + stride * UP_PER_BANK) || rsize != stride || nslot != UP_PER_BANK)
            return 0;
        for (i = 0; strict && i < UP_PER_BANK; i++) {
            const uint8_t *s = b + 8u + i * stride;
            if (!up_empty(s, stride) && !(s[0] == UP_USED && s[1] == ver && s[2] < NENGINES &&
                                          s[3] >= 8u && s[3] <= UP_PMAX_V2 && s[4]))
                return 0;
        }
        for (i = UP_PER_BANK; i-- > 0;) {
            up_rec_v2_t old = {0};
            const up_rec_v2_t *s = &old;
            up_rec_t *r = &dst->r[i];
            memcpy(&old, b + 8u + i * stride, stride);
            memset(r, 0, sizeof *r);
            if (!(s->used == UP_USED && s->ver == ver && s->engine < NENGINES && s->np >= 8u && s->np <= UP_PMAX_V2 &&
                  s->name[0]))
                continue;                                   /* (empty) */
            r->used = UP_USED;
            r->ver = UP_VER;
            r->engine = s->engine;
            r->np = s->np;                                  /* (the values as stored: mapped by count when read) */
            memcpy(r->name, s->name, sizeof r->name);
            memcpy(r->p, s->p, sizeof s->p);
            memcpy(r->note, s->note, sizeof r->note);
            memcpy(r->flags, s->flags, sizeof r->flags);
            for (k = 0; k < MACRO_COUNT; k++)
                r->macro[k] = ver == UP_VER_V2 && param_id_by_count(s->macro[k], s->np) < P_COUNT ?
                              s->macro[k] : MACRO_DEFAULT;
        }
        dst->magic = UP_BANK_MAGIC;
        dst->rsize = UP_RECORD;
        dst->nslot = UP_PER_BANK;
        return 1;
    }
    return 0;
}

static int up_name_ok(const uint8_t *s, uint32_t n)   /* 1..12 printable ASCII */
{
    uint32_t i;
    if (!n || n > 12u)
        return 0;
    for (i = 0; i < n; i++)
        if (s[i] < 32u || s[i] > 126u)
            return 0;
    return 1;
}

static void up_name(uint32_t k, char *b)       /* upper case, 0-terminated: b holds 13 */
{
    const up_rec_t *r = up_rec(k);
    uint32_t i;
    for (i = 0; i < 12u && r->name[i]; i++)
        b[i] = r->name[i] >= 'a' && r->name[i] <= 'z' ? (char)(r->name[i] - 32) : r->name[i];
    b[i] = 0;
}

static void up_pat_norm(uint8_t *note, uint8_t *flags)   /* tie: no note; rest: no flags */
{
    *note &= 127u;
    if (*flags & 4u) {
        *note = 0;
        *flags = 4;
    } else {
        *flags = *note ? (uint8_t)(*flags & (SF_ACCENT | SF_SLIDE)) : 0u;
    }
}

static void up_pat_from(up_rec_t *r, const step_t *st)   /* the first 16 steps -> the pattern */
{
    uint32_t i;
    for (i = 0; i < 16u; i++) {
        r->note[i] = st[i].time == ST_NOTE && st[i].n ? st[i].note[0] : 0u;
        r->flags[i] = st[i].time == ST_TIE ? 4u : st[i].flags;
        up_pat_norm(&r->note[i], &r->flags[i]);
    }
}

static int up_pat_empty(const up_rec_t *r)
{
    uint32_t i;
    for (i = 0; i < 16u; i++)
        if (r->note[i])
            return 0;
    return 1;
}

/* UP_PUT arguments: slot, engine, name, P_COUNT x v14, 16 x (note, flags) -> *r (values not yet
 * clamped); 0 ok, 1 bad arguments. *slot gets the slot byte when there is one. */
static int up_parse_width(const uint8_t *a, uint32_t na, up_rec_t *r, uint32_t *slot, uint32_t width)
{
    uint32_t i, n, k;
    if (na < 3u || (width != 2u && width != 3u))
        return 1;
    *slot = a[0];
    for (n = 0; 2u + n < na && a[2 + n]; n++)
        ;
    k = 3u + n;                                  /* after the name's 0 */
    if (a[0] >= UP_SLOTS || a[1] >= NENGINES || 2u + n >= na || !up_name_ok(a + 2, n) ||
        na < k + width * P_COUNT + 32u ||
        (width == 3u && na != k + width * P_COUNT + 32u))
        return 1;
    /* Validate every wide value's high byte before touching the record. */
    for (i = 0; width == 3u && i < P_COUNT; i++)
        if (a[k + i * width] >= 128u || a[k + i * width + 1u] >= 128u || a[k + i * width + 2u] > 3u)
            return 1;
    memset(r, 0, sizeof *r);
    r->used = UP_USED;
    r->ver = UP_VER;
    r->engine = a[1];
    r->np = P_COUNT;
    memset(r->macro, MACRO_DEFAULT, sizeof r->macro);   /* (UP_PUT carries no assignment: the engine's own) */
    for (i = 0; i < n; i++)
        r->name[i] = (char)a[2 + i];
    for (i = 0; i < P_COUNT; i++, k += width)
        r->p[i] = (int16_t)((int32_t)((a[k] & 127u) | (a[k + 1] & 127u) << 7 |
                                     (width == 3u ? a[k + 2] << 14 : 0)) - (width == 3u ? 32768 : 8192));
    for (i = 0; i < 16u; i++, k += 2u) {
        r->note[i] = a[k];
        r->flags[i] = a[k + 1];
        up_pat_norm(&r->note[i], &r->flags[i]);
    }
    return 0;
}
/* Existing host tests and v14 callers use the same parser with two-byte values. */
static int up_parse(const uint8_t *a, uint32_t na, up_rec_t *r, uint32_t *slot)
{
    return up_parse_width(a, na, r, slot, 2u);
}

#ifndef UP_HOST
static const param_desc_t *up_desc(uint32_t e, uint32_t i)
{
    return i >= P_E0 ? &ENGINES[e]->edit[i - P_E0] : &TP[i];
}

static void up_values(const up_rec_t *r, int16_t *v)   /* mapped and clamped for its engine */
{
    int16_t def[P_COUNT];
    uint32_t i;
    for (i = 0; i < P_COUNT; i++)
        def[i] = up_desc(r->engine, i)->def;
    up_params(r, v, def);
    for (i = 0; i < P_COUNT; i++)
        v[i] = (int16_t)clamp(v[i], up_desc(r->engine, i)->min, up_desc(r->engine, i)->max);
}

#if UP_RAM
/* persist_boot: the banks from flash, each read into the staging (cap bytes: an object fits) and decoded into its
 * RAM mirror; a bank of another shape reads as empty */
static void up_boot(void *stage, uint32_t cap)
{
#if FELUCCA_FLASH
    uint32_t b;
    for (b = 0; b < UP_SLOTS / UP_PER_BANK; b++) {
        int n = flash_ok ? st_load(OBJ_UPRESET0 + b, stage, cap) : -1;
        if (!up_bank_decode(&up_bank[b], stage, n, 0))
            memset(&up_bank[b], 0, sizeof up_bank[b]);
    }
#else
    (void)stage, (void)cap;
#endif
}
#else
static void up_bank_find(uint32_t b)            /* bank b's current copy in the window, or none (after a save too) */
{
    uint32_t len = 0;
    const up_bank_t *p = flash_ok ? (const up_bank_t *)st_xip(OBJ_UPRESET0 + b, &len) : 0;
    up_bank_xip[b % NELEM(up_bank_xip)] = p && len == sizeof *p && p->magic == UP_BANK_MAGIC &&
                                          p->rsize == UP_RECORD && p->nslot == UP_PER_BANK ? p : 0;
}

/* persist_boot: each bank read where it is. One that is not exactly what up_bank_decode makes of it (OPB2 or
 * UPB1, or OPB3 with a record to clear) is converted in the staging and written back once as OPB3 (two copies:
 * a power cut keeps the old one, converted again at the next start); a bank of another shape reads as empty */
static void up_boot(void *stage, uint32_t cap)
{
    uint32_t b;
    for (b = 0; b < UP_SLOTS / UP_PER_BANK; b++) {
        uint32_t len = 0;
        const void *p = flash_ok ? st_xip(OBJ_UPRESET0 + b, &len) : 0;
        up_bank_xip[b] = 0;
        if (!p || cap < sizeof(up_bank_t) || !up_bank_decode((up_bank_t *)stage, p, (int)len, 0))
            continue;
        if ((len != sizeof(up_bank_t) || memcmp(stage, p, sizeof(up_bank_t))) &&
            st_save(OBJ_UPRESET0 + b, stage, sizeof(up_bank_t)))
            continue;                                   /* (not written: the bank reads as empty, kept in flash) */
        up_bank_find(b);
    }
}
#endif

#if UP_RAM
/* record k = *r (0: erase), then the bank to flash: 0 ok, 2 flash error, 3 no flash (kept in RAM) */
static int up_put(uint32_t k, const up_rec_t *r)
{
    up_bank_t *bk = &up_bank[k / UP_PER_BANK];
    bk->magic = UP_BANK_MAGIC;
    bk->rsize = sizeof(up_rec_t);
    bk->nslot = UP_PER_BANK;
    if (r)
        *up_rec(k) = *r;
    else
        memset(up_rec(k), 0, sizeof(up_rec_t));
    if (!r) {
        uint32_t i;
        for (i = 0; i < NTRK; i++)
            if (trk[i].user == k + 1u)
                trk[i].user = 0;
    }
    up_gen++;
#if FELUCCA_FLASH
    if (flash_ok)
        return st_save(OBJ_UPRESET0 + k / UP_PER_BANK, bk, sizeof *bk) ? 2 : 0;
#endif
    return 3;
}
#else
/* record k = *r (0: erase), then the bank to flash: 0 ok, 2 flash error or no flash (no RAM mirror keeps it).
 * The bank is staged in the project buffer: its current copy, or an empty one, with the record put */
static int up_put(uint32_t k, const up_rec_t *r)
{
    uint32_t b = (k / UP_PER_BANK) % NELEM(up_bank_xip), i;
    up_bank_t *bk = (up_bank_t *)up_stage();
    int rc;
    if (!flash_ok)
        return 2;
    if (up_bank_xip[b])
        memcpy(bk, up_bank_xip[b], sizeof *bk);
    else
        memset(bk, 0, sizeof *bk);
    bk->magic = UP_BANK_MAGIC;
    bk->rsize = sizeof(up_rec_t);
    bk->nslot = UP_PER_BANK;
    if (r)
        bk->r[k % UP_PER_BANK] = *r;
    else
        memset(&bk->r[k % UP_PER_BANK], 0, sizeof(up_rec_t));
    rc = st_save(OBJ_UPRESET0 + b, bk, sizeof *bk) ? 2 : 0;
    up_bank_find(b);
    if (!r && !rc)
        for (i = 0; i < NTRK; i++)
            if (trk[i].user == k + 1u)
                trk[i].user = 0;
    up_gen++;
    return rc;
}
#endif

static void up_slot_label(char *b, uint32_t k)  /* "U07" */
{
    b[0] = 'U';
    b[1] = (char)('0' + (k + 1u) / 10u);
    b[2] = (char)('0' + (k + 1u) % 10u);
    b[3] = 0;
}

/* the selected part's sound -> slot k; name 0 or "": engine name + slot number ("ANALOG 07").
 * 1 = the drum track is selected (it has no sound to store) */
static int up_store(uint32_t k, const char *name)
{
    up_rec_t r;
    uint32_t i;
    if (is_drum(TSEL))
        return 1;
    memset(&r, 0, sizeof r);
    r.used = UP_USED;
    r.ver = UP_VER;
    r.engine = TSEL->eng_req;
    r.np = P_COUNT;
    for (i = 0; i < MACRO_COUNT; i++)
        r.macro[i] = (uint8_t)(TSEL->macro[i] < P_COUNT ? TSEL->macro[i] : MACRO_DEFAULT);
    if (name && name[0]) {
        for (i = 0; i < 12u && name[i]; i++)
            r.name[i] = name[i];
    } else {
        char b[16], l[4];
        str_cpy(b, ENGINES[TSEL->eng_req]->name, 9);
        up_slot_label(l, k);
        str_cpy(b + str_len(b), " ", 2);
        str_cpy(b + str_len(b), l + 1, 3);
        for (i = 0; i < 12u && b[i]; i++)
            r.name[i] = b[i];
    }
    for (i = 0; i < P_COUNT; i++)
        r.p[i] = trk_base_value(TSEL, i);              /* (a lock in force: the value under it) */
    up_pat_from(&r, TSEL->step);
    return up_put(k, &r);
}

/* slot k -> the selected part's sound: engine and every parameter except its mix (LEVEL,
 * PAN, MUTE: the TRACKS faders) and its pattern parameters (param_kept). LIVE: the pattern
 * stored in the record is not loaded: changing the sound never changes the sequence.
 * 0 ok, 1 empty (or the drum track is selected) */
static int up_load_to(track_t *t, uint32_t k)       /* user preset k into track t (2.6: any track, MIDI PC) */
{
    const up_rec_t *r;
    int16_t v[P_COUNT];
    uint32_t i;
    if (!up_used(k) || is_drum(t))
        return 1;
    r = up_rec(k);
    up_values(r, v);
    for (i = 0; i < P_COUNT; i++)                       /* (LEN etc. of a kept pattern changed too, and */
        if (param_kept(i))                              /* a preset pattern then counted as edited) */
            v[i] = t->p[i];
    sound_change(t);                                    /* (2.6: what sounds fades out with the old sound, no click) */
    panic_req |= (uint8_t)(1u << trk_index(t));
    fm1_irq_off();                                      /* the audio ISR must not see half a sound */
    t->eng_req = r->engine;
    for (i = 0; i < P_COUNT; i++)
        t->p[i] = v[i];
    for (i = 0; i < MACRO_COUNT; i++)
        t->macro[i] = (uint8_t)up_macro(r, i);
    t->preset = 0;
    fm1_irq_on();
    fm6_track_loaded(t);                                /* FM6: a user preset holds the PTCH and the macros */
    t->user = (uint8_t)(k + 1u);
    if (t == TSEL)
        sync_reload = 1;
    ui.force = 1;
    return 0;
}
static int up_load(uint32_t k) { return up_load_to(TSEL, k); }   /* into the selected track */

static uint32_t up_count(void)                 /* used slots */
{
    uint32_t k, n = 0;
    for (k = 0; k < UP_SLOTS; k++)
        n += (uint32_t)up_used(k);
    return n;
}

static uint32_t up_nth(uint32_t n)             /* slot of the n-th used one (n < up_count()) */
{
    uint32_t k;
    for (k = 0; k < UP_SLOTS; k++)
        if (up_used(k) && !n--)
            return k;
    return 0;
}

static uint32_t up_rank(uint32_t slot)         /* used slots before it */
{
    uint32_t k, n = 0;
    for (k = 0; k < slot && k < UP_SLOTS; k++)
        n += (uint32_t)up_used(k);
    return n;
}

/* SAVE > USER page actions, with the message in the top bar */
static void up_ui(uint32_t op, uint32_t k)     /* 0 load, 1 erase, 2 save */
{
    char l[4];
    int rc;
    up_slot_label(l, k);
    if (op != 1u && is_drum(TSEL)) {
        ui_message("DRUM TRACK: NO SOUND");
        return;
    }
    if (op < 2u && !up_used(k)) {
        ui_message("EMPTY SLOT");
        return;
    }
    if (op == 0u) {
        up_load(k);
        ui_say("LOADED ", l);
        return;
    }
    if (song.playing || transport_req) {               /* a flash erase stops the audio ~50 ms */
        ui_message("STOP BEFORE SAVE");
        return;
    }
    rc = op == 1u ? up_put(k, 0) : up_store(k, 0);
    if (rc == 3)
        ui_message(op == 1u ? "ERASED (RAM)" : "SAVED (RAM)");
    else if (rc)
        ui_message(op == 1u ? "ERASE ERROR" : "SAVE ERROR");
    else
        ui_say(op == 1u ? "ERASED " : "SAVED ", l);
    ui.force = 1;
}
#endif

