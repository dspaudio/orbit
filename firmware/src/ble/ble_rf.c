/* SPDX-License-Identifier: GPL-3.0-only */
/* RF record loading from Hortator 4e40f27 by DEADACTIVE; ORBIT store callbacks.
 * Kept separate from the hardware port so the actual scan and SDK callback
 * can run against a host flash/store double. No flash writes here. */
static uint8_t ble_rf_trim[BLE_VM_RF_TRIM_LEN];
static uint32_t ble_rf_trim_at;                          /* flash offset, 1 kept copy, 2 fallback, 0 none */
#define BLE_VM_FROM 0xE5000u
#define BLE_VM_TO 0xFC000u
static __attribute__((unused)) int ble_rf_trim_load(void)
{
    static uint8_t buf[512];
    uint8_t kept[BLE_VM_RF_TRIM_LEN], found[BLE_VM_RF_TRIM_LEN];
    uint32_t off, from;
    int keep;
    ble_rf_trim_at = 0;
    for (off = BLE_VM_FROM; off < BLE_VM_TO; off += sizeof buf - (4u + BLE_VM_RF_TRIM_LEN)) {
        uint32_t n = BLE_VM_TO - off < sizeof buf ? BLE_VM_TO - off : sizeof buf;
        uint8_t rec[BLE_VM_RF_TRIM_LEN];
        core_wdt_feed();
        if (core_flash_read(off, buf, n)) {
            ble_rf_trim_at = 0;
            return 0;
        }
        if (ble_vm_rf_trim(buf, n, rec)) {
            memcpy(found, rec, sizeof rec);
            ble_rf_trim_at = off;
        }
        if (n < sizeof buf)
            break;
    }
    from = ble_vm_rf_pick(ble_rf_trim_at ? found : 0, core_rf_keep_get(kept) ? kept : 0, ble_rf_trim, &keep);
    if (!from) {
        ble_vm_rf_fallback(ble_rf_trim);
        ble_rf_trim_at = 2;
        keep = 1;
    }
    if (keep)
        core_rf_keep_put(ble_rf_trim);
    if (from == 2u)
        ble_rf_trim_at = 1;
    return 1;
}
int wifi_get_rf_trim_data(void *info, int size)
{
    if (!ble_rf_trim_at || size != (int)BLE_VM_RF_TRIM_LEN)
        return -1;
    memcpy(info, ble_rf_trim, BLE_VM_RF_TRIM_LEN);
    return 0;
}
int wifi_set_rf_trim_data(void *info, int size) { (void)info; (void)size; return -1; }
