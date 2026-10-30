/* SPDX-License-Identifier: GPL-3.0-only */
/* ORBIT: ported from Hortator 4e40f27 by DEADACTIVE, felucca-ble ble-kit. */
#ifndef ORBIT_BLE_API_H
#define ORBIT_BLE_API_H
#include <stdint.h>

/* Main-loop API only. Bind once before starting; the core retains ownership of
 * its private HAL state. Binding does not start the radio or access flash.
 * Every callback is required and must remain valid for the life of the stack.
 * irq_attach must unlock/relock the protected vector table; stack_window must
 * include the BLE task stacks in .pool. RF callbacks only read/cache 68 bytes,
 * never write flash during BLE service. mac returns a persistent static address. */
typedef struct {
    uint32_t (*ms)(void);
    void (*wdt_feed)(void);
    void (*reboot)(void);
    void (*audio_stop)(void);
    void (*irq_attach)(uint32_t n, void (*handler)(void), uint32_t priority);
    void (*irq_mask)(uint32_t n);
    void (*irq_unmask)(uint32_t n);
    uint32_t (*irq_is_masked)(uint32_t n);
    void (*stack_window)(void);
    int (*flash_read)(uint32_t off, void *buf, uint32_t len);
    void (*mac)(uint8_t addr[6]);
    int (*rf_get)(uint8_t record[68]);
    void (*rf_keep)(const uint8_t record[68]);
} ble_platform_t;

/* Returns 0 for incomplete callbacks or a stack already started, 1 on binding. */
int ble_configure(const ble_platform_t *platform);
void ble_user_start(void);
/* Stop advertising/disconnect asynchronously; continue calling ble_service.
 * The initialized vendor stack remains serviced; this is not RF power-down. */
void ble_user_stop(void);
void ble_service(void);
typedef enum {
    BLE_STATUS_OFF, BLE_STATUS_STARTING, BLE_STATUS_ADVERTISING,
    BLE_STATUS_CONNECTED, BLE_STATUS_STOPPING, BLE_STATUS_FAILED
} ble_status_t;
ble_status_t ble_get_status(void);
const char *ble_last_error(uint32_t *code);

/* USB-MIDI event, cable ignored. Whole 1..3-byte messages only; bounded output
 * queue, dropped if disconnected/full. SysEx output is not supported.
 * The integration lane must queue ISR output and hand it here in the main loop. */
void ble_midi_out_pkt(uint32_t usb_midi_pkt);
/* Drain cable-0 USB-MIDI events after ble_service; 1 = packet, 0 = empty.
 * Disconnect discards stale input. Supports incoming SysEx as USB-MIDI chunks. */
int ble_midi_take(uint32_t *usb_midi_pkt);
uint32_t ble_midi_rx_drops(void);
uint32_t ble_midi_tx_drops(void);
#endif
