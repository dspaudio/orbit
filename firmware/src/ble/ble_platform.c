/* SPDX-License-Identifier: GPL-3.0-only */
/* ORBIT: ported from Hortator 4e40f27 by DEADACTIVE, felucca-ble ble-kit. */
/* Hortator core_ble_api.c's wrappers, bound instead of editing ORBIT's core TU. */
#include "ble_api.h"
#include "ble_core.h"
static ble_platform_t ble_platform;
static uint8_t ble_bound;
int ble_configure(const ble_platform_t *p)
{
    if (!p || ble_started() || !p->ms || !p->wdt_feed || !p->reboot ||
        !p->audio_stop || !p->irq_attach || !p->irq_mask || !p->irq_unmask ||
        !p->irq_is_masked || !p->stack_window || !p->flash_read || !p->mac ||
        !p->rf_get || !p->rf_keep)
        return 0;
    ble_platform = *p;
    ble_bound = 1;
    return 1;
}
uint32_t core_ms(void) { return ble_platform.ms(); }
void core_wdt_feed(void) { ble_platform.wdt_feed(); }
void core_reboot(void) { ble_platform.reboot(); }
void core_audio_stop(void) { ble_platform.audio_stop(); }
void core_irq_attach(uint32_t n, void (*h)(void), uint32_t p) { ble_platform.irq_attach(n, h, p); }
void core_irq_mask(uint32_t n) { ble_platform.irq_mask(n); }
void core_irq_unmask(uint32_t n) { ble_platform.irq_unmask(n); }
uint32_t core_irq_is_masked(uint32_t n) { return ble_platform.irq_is_masked(n); }
void core_ble_stack_window(void) { ble_platform.stack_window(); }
int core_flash_read(uint32_t off, void *buf, uint32_t n) { return ble_platform.flash_read(off, buf, n); }
void core_ble_mac(uint8_t mac[6]) { ble_platform.mac(mac); }
int core_rf_keep_get(uint8_t rec[68]) { return ble_platform.rf_get(rec); }
void core_rf_keep_put(const uint8_t rec[68]) { ble_platform.rf_keep(rec); }
