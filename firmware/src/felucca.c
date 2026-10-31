/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* SLOOP (based on FELUCCA): one compilation unit (the HAL is header-only). Order matters. */
#include <stdint.h>
#include "fm1_time.h"
#include "fm1_sys.h"
#include "fm1_irq.h"
#include "fm1_guard.h"
#if ORBIT_BLE
#include "fm1_ble_hal.h"
#include "ble/ble_api.h"
#endif
#if BOARD_SMK37
#include "smk_input.h"           /* keys, buttons, encoders, LEDs, pads, faders, wheels, battery */
#else
#include "fm1_input.h"
#endif
#include "fm1_timer.h"
#include "fm1_audio.h"
#if !BOARD_SMK37
#include "fm1_adc.h"
#endif
#include "fm1_lcd_hw.h"
#include "felucca_tables.h"

#include "libc.c"
#include "lcd.c"
#include "gfx.c"
#include "core.h"
#include "engines.c"
#include "drums.c"
#include "params.c"
#include "voice.c"
#include "slicer.c"          /* per-track SLICER insert, used by fx.c */
#include "fx.c"
#ifndef FELUCCA_OTA
#define FELUCCA_OTA 1            /* M-UPGRADE update entry; needs FELUCCA_FLASH */
#endif
#ifndef FELUCCA_OTA_DRYRUN
#define FELUCCA_OTA_DRYRUN 0     /* 1 = stage, ask "success", then undo: no record, no reset */
#endif
#ifndef FELUCCA_ID
#if BOARD_SMK37
#define FELUCCA_ID "SMK-37 Pro_900"   /* package identity (build.py: the .fwsc marker string) */
#else
#define FELUCCA_ID "FM-1_900"    /* package identity (build.py: the .fwsc marker string) */
#endif
#endif
#ifndef FELUCCA_CDC
#define FELUCCA_CDC 1            /* USB CDC-ACM serial console */
#endif
#ifndef FELUCCA_UAC                      /* (before usb.c: it defaults to 0) */
#define FELUCCA_UAC 1            /* USB audio input: the master output, 16-bit stereo 44.1 kHz (usb.c, after Felucca 1.0) */
#endif
#ifndef FELUCCA_UAC_TONE
#define FELUCCA_UAC_TONE 0       /* bench: the USB input sends test triangles instead of the music */
#endif
#include "usb.c"
#ifndef FELUCCA_UART
#if BOARD_SMK37
#define FELUCCA_UART 0           /* SMK-37 Pro: TRS MIDI OUT (UART1 TX, PC7), no MIDI IN; PH8 is a key-matrix
                                  * row there and must never become UART RX. TODO: a TX driver (see the fm1-to-smk37 guide) */
#else
#define FELUCCA_UART 1           /* TRS MIDI IN on UART1 (3.5 mm jack) */
#endif
#endif
#if BOARD_SMK37 && FELUCCA_UART
#error "BOARD_SMK37: midi_uart.c is the FM-1 MIDI IN driver (RX on PH8, a key-matrix row on the SMK)"
#endif
#if FELUCCA_UART
#include "midi_uart.c"
#endif
#define FELUCCA_ARRANGER 1
#include "arranger.c"
#include "seq.c"
#include "audio.c"
#include "panel.c"
#include "ui.c"
#include "ui_song.c"
#include "ui_studio.c"
#include "icons.c"           /* parameter icons (FELUCCA_ICONS), used by ui_draw.c */
#if BOARD_SMK37
#include "ui_pick.c"          /* SMK-37 Pro: the pad sound picker */
#endif
#include "ui_draw.c"
#include "ui_layers.c"       /* hold a function button: what the keys and knobs do (TE style) */
#include "ui_menu.c"
#if BOARD_SMK37
#include "ui_padgrid.c"       /* SMK-37 Pro: steps and tiles in reading order */
#endif
#include "ui_input.c"
#if BOARD_SMK37
#include "ui_fbank.c"         /* SMK-37 Pro: the FADER BANK screen */
#endif
#ifndef FELUCCA_FLASH
#define FELUCCA_FLASH 1          /* flash driver + storage.c */
#endif
#if FELUCCA_OTA && !FELUCCA_FLASH
#error "FELUCCA_OTA needs FELUCCA_FLASH"
#endif
#if FELUCCA_FLASH
#if ORBIT_BLE
#include "fm1_flash_map.h"
static uint32_t orbit_ble_rf_sectors;
static uint8_t orbit_ble_rf_scanned;
/* Inlined into the RAM flash driver: no XIP call or table while SFC is off.
 * Before discovery fail closed on the entire RF scan window; afterwards
 * preserve EVERY sector containing a valid stock trim, including a record
 * crossing a sector boundary. FM6/USR4 writes touching it are refused. */
static inline __attribute__((always_inline)) int orbit_ble_write_ok(uint32_t off, uint32_t n)
{
    uint32_t first, last, mask;
    if (!n || off >= 0xFC000u || off + n <= 0xE5000u)
        return 1;
    if (!orbit_ble_rf_scanned)
        return 0;
    first = off <= 0xE5000u ? 0u : (off - 0xE5000u) >> 12;
    last = off + n >= 0xFC000u ? 22u : (off + n - 1u - 0xE5000u) >> 12;
    mask = ((1u << (last + 1u)) - 1u) & ~((1u << first) - 1u);
    return !(orbit_ble_rf_sectors & mask);
}
#undef FL_STORE_OK
#define FL_STORE_OK(off, n) (FL_IN(off, n, FL_DATA_LO, FL_DATA_HI) || FL_IN(off, n, FL_GLOB_LO, FL_GLOB_HI) || \
                            FL_IN(off, n, FL_FM6_LO, FL_FM6_HI) || FL_IN(off, n, FL_USR4_LO, FL_USR4_HI) || \
                            FL_IN(off, n, 0x93000u, 0x95000u))
#undef FL_GUARD
#define FL_GUARD(off, n) (FL_RANGE_OK(off, n) && orbit_ble_write_ok(off, n))
#endif
#include "fm1_flash.h"
static uint8_t flash_ok;                 /* JEDEC id matched at boot (persist_boot) */
static int st_read(uint32_t off, void *dst, uint32_t n)   /* 256-byte IRQ-off windows: audio keeps up */
{
    uint8_t *d = dst;
    while (n) {
        uint32_t k = n > 256u ? 256u : n, f = irq_save();
        int rc = FL_FAR(fl_read_ram)(off, d, k);
        irq_restore(f);
        if (rc)
            return rc;
        off += k;
        d += k;
        n -= k;
    }
    return 0;
}
static void audio_silence(void)                 /* IRQs off: the DMA would loop stale audio (buzz) */
{
    uint32_t i;
    for (i = 0; i < sizeof abuf / sizeof abuf[0]; i++)
        abuf[i] = 0;
}
/* a sector erase with the audio silenced inside the same interrupts-off window (silenced before it, the
 * audio interrupt could render a fresh half that the DMA then loops for the whole erase: a buzz) */
static int fl_erase4k_quiet(uint32_t off, uint32_t *took)
{
    uint32_t f = irq_save();
    int rc;
    audio_silence();
    rc = FL_FAR(fl_erase4k_ram)(off, took);
    irq_restore(f);
    return rc;
}
static int st_erase(uint32_t off)
{
    uint32_t took;
    if (!FL_STORE_OK(off, 0x1000u))
        return -8;
    return fl_erase4k_quiet(off, &took);
}
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    if (!FL_STORE_OK(off, n))
        return -8;
    return fl_write(off, src, n);
}
#define ST_XIP_PTR(off) fm1_xip_ptr(off)   /* storage.c st_xip: an object read where it is (fm1_xip.h) */
#include "storage.c"
#endif
#include "upreset.c"          /* user presets (read in the XIP window; a RAM mirror without the flash) */
#include "project.c"
#if ORBIT_BLE
#if !FELUCCA_FLASH || BOARD_SMK37
#error "ORBIT_BLE requires the FM-1 flash layout"
#endif
/* Platform glue adapted from Hortator 4e40f27, DEADACTIVE, core_ble_api.c
 * and bt_devs.c. BLM1 is a separate A/B object, not project/preset/settings
 * growth. RF callbacks cache only; flash writes happen after service returns. */
#include "ble/ble_vm.c"
static struct {
    uint32_t magic;
    uint8_t mac[6], mac_valid, rf_valid, rf[68];
} orbit_ble_store;
static uint8_t orbit_ble_dirty;
#define ORBIT_BLE_STORE_MAGIC 0x314D4C42u             /* BLM1 */
static uint32_t orbit_ble_ms(void) { return fm1_ms; }
static void orbit_ble_wdt(void) { fm1_wdt_feed(); }
static void orbit_ble_reboot(void) { fm1_reboot(); for (;;) ; }
static void orbit_ble_audio_stop(void) { fm1_audio_stop(); }
static void orbit_ble_irq_attach(uint32_t n, void (*h)(void), uint32_t prio)
{
    fm1_guard_unlock_top();
    fm1_irq_attach(n, h, prio);
    fm1_guard_lock_top();
}
static void orbit_ble_irq_mask(uint32_t n) { fm1_irq_mask(n); }
static void orbit_ble_irq_unmask(uint32_t n) { fm1_irq_unmask(n); }
static uint32_t orbit_ble_irq_is_masked(uint32_t n) { return fm1_irq_is_masked(n); }
static void orbit_ble_stack_window(void)
{
    extern uint32_t _pool_start[];
    fm1_guard_stack_window((uintptr_t)_pool_start, (uintptr_t)_sstack_top);
}
static int orbit_ble_flash_read(uint32_t off, void *buf, uint32_t n)
{
    if (!flash_ok || !buf || !n || off < 0xE5000u || off >= 0xFC000u || n > 0xFC000u - off)
        return -1;
    return st_read(off, buf, n);
}
static int orbit_ble_rf_get(uint8_t rec[68])
{
    if (!orbit_ble_store.rf_valid || !ble_vm_rf_ok(orbit_ble_store.rf))
        return 0;
    memcpy(rec, orbit_ble_store.rf, 68);
    return 1;
}
static void orbit_ble_rf_keep(const uint8_t rec[68])
{
    if (ble_vm_rf_ok(rec) && (!orbit_ble_store.rf_valid || memcmp(orbit_ble_store.rf, rec, 68))) {
        memcpy(orbit_ble_store.rf, rec, 68);
        orbit_ble_store.rf_valid = 1;
        orbit_ble_dirty = 1;
    }
}
static void orbit_ble_mac(uint8_t addr[6])
{
    if (orbit_ble_store.mac_valid != 0xA5u) {
        uint32_t i, x = fm1_ticks() ^ fm1_ms * 2654435761u ^ st_crc32(orbit_ble_store.rf, 68);
        for (i = 0; i < 6u; i++) {
            x = x * 1103515245u + 12345u;
            orbit_ble_store.mac[i] = (uint8_t)(x >> 16);
        }
        orbit_ble_store.mac[0] |= 0xC0u;
        orbit_ble_store.mac[5] |= 0xC0u;
        orbit_ble_store.mac_valid = 0xA5u;
        orbit_ble_dirty = 1;
    }
    memcpy(addr, orbit_ble_store.mac, 6);
}
static void orbit_ble_bind(void)
{
    static const ble_platform_t platform = {
        orbit_ble_ms, orbit_ble_wdt, orbit_ble_reboot, orbit_ble_audio_stop,
        orbit_ble_irq_attach, orbit_ble_irq_mask, orbit_ble_irq_unmask, orbit_ble_irq_is_masked,
        orbit_ble_stack_window, orbit_ble_flash_read, orbit_ble_mac, orbit_ble_rf_get, orbit_ble_rf_keep
    };
    uint8_t buf[512], rec[68];
    uint32_t off, i;
    int scanned = 1;
    if (flash_ok && (st_load(OBJ_BLE, &orbit_ble_store, sizeof orbit_ble_store) != sizeof orbit_ble_store ||
                     orbit_ble_store.magic != ORBIT_BLE_STORE_MAGIC))
        memset(&orbit_ble_store, 0, sizeof orbit_ble_store);
    orbit_ble_store.magic = ORBIT_BLE_STORE_MAGIC;
    for (off = 0xE5000u; off < 0xFC000u; off += sizeof buf - 71u) {
        uint32_t n = 0xFC000u - off < sizeof buf ? 0xFC000u - off : sizeof buf;
        fm1_wdt_feed();
        if (orbit_ble_flash_read(off, buf, n)) {
            scanned = 0;
            break;
        }
        for (i = 0; i + 72u <= n; i++)
            if (ble_vm_rf_trim(buf + i, 72, rec)) {
                uint32_t a = off + i - 0xE5000u;
                orbit_ble_rf_sectors |= (1u << (a >> 12)) | (1u << ((a + 71u) >> 12));
                orbit_ble_rf_keep(rec);
            }
        if (n < sizeof buf)
            break;
    }
    orbit_ble_rf_scanned = (uint8_t)scanned;
    if (scanned && !orbit_ble_rf_get(rec)) {
        ble_vm_rf_fallback(rec);
        orbit_ble_rf_keep(rec);                        /* persist before audio starts, even with BT still off */
    }
    {
        uint8_t addr[6];
        orbit_ble_mac(addr);                            /* keep identity before audio/transport can defer saves */
    }
    if (orbit_ble_dirty && flash_ok && st_save(OBJ_BLE, &orbit_ble_store, sizeof orbit_ble_store) == 0)
        orbit_ble_dirty = 0;
    (void)ble_configure(&platform);                    /* once; never starts the radio */
}
static void orbit_ble_core_service(void)
{
    orbit_ble_bridge_service();
    if (orbit_ble_dirty && flash_ok && !song.playing && !transport_req &&
        st_save(OBJ_BLE, &orbit_ble_store, sizeof orbit_ble_store) == 0)
        orbit_ble_dirty = 0;
}
#endif
#if FELUCCA_OTA
static uint8_t recovery_active;
#if BOARD_SMK37
#define OTA_IDENTITY (recovery_active ? SMK_MODEL "_000" : FELUCCA_ID)
#else
#define OTA_IDENTITY (recovery_active ? "FM-1_000" : FELUCCA_ID)
#endif
#include "ota.c"
static void recovery_poll(void);
static uint32_t ota_now_ms(void) { return fm1_ms; }
static void ota_idle(void)
{
    fm1_wdt_feed();
    if (recovery_active) recovery_poll();
}
static int ota_in_area(uint32_t off, uint32_t n) { return FL_IN(off, n, OTA_AREA, OTA_AREA + OTA_AREA_LEN); }
static int ota_erase(uint32_t off)
{
    uint32_t took;
    if (!ota_in_area(off, 0x1000u) || (off & 0xFFFu))
        return -8;
    return fl_erase4k_quiet(off, &took);
}
static int ota_prog(uint32_t off, const void *p, uint32_t n)
{
    if (!ota_in_area(off, n))
        return -8;
    return fl_write(off, p, n);
}
static int ota_fread(uint32_t off, void *p, uint32_t n) { return st_read(off, p, n); }
static void ota_show(uint32_t step, int32_t code)
{
#if BOARD_SMK37   /* step 5 stays on screen while the loader writes the flash: say so */
    static const char *const STEP[] = {"", "PACKAGE", "CHECK HEAD", "LOADER", "CONFIRM", "UPDATING..."};
#else
    static const char *const STEP[] = {"", "PACKAGE", "CHECK HEAD", "LOADER", "CONFIRM", "RESTART"};
#endif
    char b[24];
    if (recovery_active) return;             /* keep polled USB alive during OTA */
    lcd_fill(0, 0, 240, 240, C_BLACK);
    draw_text_box(0, 92, 240, &FONT_S, "UPDATE", C_WHITE, 1);
    if (step < 9u) {
        draw_text_box(0, 124, 240, &FONT_S, STEP[step < 6u ? step : 0], C_HI, 1);
        return;
    }
    if (code == 1)
        str_cpy(b, "DRY RUN OK", sizeof b);
    else {
        uint32_t k = 0, v = (uint32_t)-code;
        str_cpy(b, "FAILED  -", sizeof b);
        while (b[k])
            k++;
        if (v >= 10u)
            b[k++] = (char)('0' + v / 10u);
        b[k++] = (char)('0' + v % 10u);
        b[k] = 0;
    }
    draw_text_box(0, 124, 240, &FONT_S, b, C_HI, 1);
    fm1_delay_ms(1500);
}
static void ota_commit(const uint8_t *parm)
{
    bootguard_clear(&bootguard);                        /* intentional update reset */
    usb_detach();
    fm1_delay_ms(30);
    fm1_enter_update(parm);                             /* record into RAM, core reset (fm1_sys.h) */
}
#endif
#if FELUCCA_OTA
#include "editor.c"          /* web editor SysEx (needs the OTA SysEx plumbing) */
#endif
#if FELUCCA_CDC
#include "console.c"
#endif
#if FELUCCA_OTA
#include "recovery.c"        /* early, polled USB updater; no synth or settings */
#endif
#include "splash.c"          /* ORBIT 부트 화면 */
#include "main.c"

