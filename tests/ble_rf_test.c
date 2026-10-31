/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define ORBIT_BLE 1
#include "../firmware/src/ble/ble_vm.c"

static uint8_t flash[0x100000], kept[68];
static unsigned reads, feeds, keeps, erases, programs;
static int kept_valid, dirty, fail_read, fail_program;
static int st_read(uint32_t off, void *p, uint32_t n)
{
    assert(off <= sizeof flash && n <= sizeof flash - off);
    memcpy(p, flash + off, n);
    return 0;
}
static int st_erase(uint32_t off)
{
    assert(off == 0x93000u || off == 0x94000u);
    memset(flash + off, 0xFF, 4096);
    erases++;
    return 0;
}
static int st_prog(uint32_t off, const void *p, uint32_t n)
{
    const uint8_t *b = p;
    assert(off >= 0x93000u && off < 0x95000u && n <= 0x95000u - off);
    if (fail_program)
        return -1;
    for (uint32_t i = 0; i < n; i++) {
        assert((flash[off + i] & b[i]) == b[i]);
        flash[off + i] &= b[i];
    }
    programs++;
    return 0;
}
#include "../firmware/src/storage.c"
static void core_wdt_feed(void) { feeds++; }
static int core_flash_read(uint32_t off, void *p, uint32_t n)
{
    assert(off >= 0xE5000u && off < 0xFC000u && n <= 0xFC000u - off);
    reads++;
    return fail_read && reads >= (unsigned)fail_read ? -1 : st_read(off, p, n);
}
static int core_rf_keep_get(uint8_t *p)
{
    if (!kept_valid)
        return 0;
    memcpy(p, kept, sizeof kept);
    return 1;
}
static void core_rf_keep_put(const uint8_t *p)
{
    assert(ble_vm_rf_ok(p));
    memcpy(kept, p, sizeof kept);
    kept_valid = dirty = 1;
    keeps++;
}
#include "../firmware/src/ble/ble_rf.c"

static int fails;
#define CHECK(c, what) do { if (c) printf("ok    %s\n", what); \
    else { printf("FAIL  %s\n", what); fails++; } } while (0)
static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static void crc_record(uint8_t *p)
{
    uint32_t c = ble_vm_crc16(p, 64);
    p[64] = (uint8_t)c;
    p[65] = (uint8_t)(c >> 8);
    p[66] = p[67] = 0;
}
static void stock_record(uint32_t off, const uint8_t *p)
{
    /* Fixture injection only, not a firmware write callback. */
    flash[off] = (uint8_t)ble_vm_crc16(p, 68);
    flash[off + 1] = 187;
    flash[off + 2] = 0x40;
    flash[off + 3] = 0x04;
    memcpy(flash + off + 4, p, 68);
}
int main(void)
{
    uint8_t first[68] = {0}, other[68] = {0}, out[70];
    memset(flash, 0xFF, sizeof flash);
    CHECK(ble_rf_trim_load(), "erased stock VM and no kept copy still provide an RF record");
    CHECK(keeps == 1 && dirty && ble_vm_rf_ok(kept), "first fallback is queued for ORBIT persistence");
    CHECK(!erases && !programs, "RF loading never writes flash or stock sectors");
    CHECK(reads && reads == feeds, "stock scan feeds watchdog for every bounded read");
    memset(out, 0xA5, sizeof out);
    CHECK(wifi_get_rf_trim_data(out + 1, 68) == 0 && out[0] == 0xA5 && out[69] == 0xA5,
          "SDK receives exactly 68 bytes with intact guards");
    if (kept_valid) {
        memcpy(first, kept, 68);
        CHECK(le32(first) == 1023 && le32(first + 8) == 1023, "neutral TX IQ uses SDK gains, not zero");
        CHECK(!le32(first + 4) && !le32(first + 12) && !le32(first + 16) && !le32(first + 20),
              "neutral TX phase and DC corrections are zero");
        CHECK(first[24] == 64 && first[25] == 64 && first[26] == 255 && first[27] == 255,
              "SDK TX LO midpoint and RX IQ gains");
        CHECK(!le32(first + 28) && !le32(first + 32) && !le32(first + 36), "neutral RX phase and analog DC");
        CHECK(le32(first + 40) == 0x3B89FFFFu && le32(first + 44) == 0x3B89FFFFu,
              "FM-1 reference bandwidth words are retained");
        for (unsigned i = 48; i < 64; i += 2)
            CHECK(first[i] == 0 && first[i + 1] == 2, "RX DC table uses SDK positive-zero encoding");
    }
    CHECK(st_save(OBJ_BLE, kept, sizeof kept) == 0, "fallback commits to the existing OBJ_BLE A/B store");
    memset(kept, 0, sizeof kept);
    kept_valid = st_load(OBJ_BLE, kept, sizeof kept) == sizeof kept;
    dirty = 0;
    keeps = 0;
    CHECK(ble_rf_trim_load() && !memcmp(ble_rf_trim, first, 68) && !keeps && !dirty,
          "power cycle loads the saved fallback without another save");
    memset(out, 0xA5, sizeof out);
    CHECK(wifi_get_rf_trim_data(out, 67) == -1 && out[0] == 0xA5, "SDK short record request is refused");
    CHECK(wifi_get_rf_trim_data(out, 69) == -1 && out[0] == 0xA5, "SDK oversized record request is refused");
    CHECK(wifi_set_rf_trim_data(out, 68) == -1, "SDK cannot write its VM");

    /* Distinct valid records to prove priority, including an overlap-boundary record. */
    other[0] = 0xE8;
    other[1] = 3;
    other[8] = 0xFF;
    other[9] = 3;
    crc_record(other);
    stock_record(0xE5000u + 510u, other);
    flash[0xE5000u + 514u] ^= 1;
    CHECK(ble_rf_trim_load() && !memcmp(ble_rf_trim, first, 68) && !keeps,
          "damaged stock record cannot displace the valid kept fallback");
    stock_record(0xE5000u + 510u, other);
    CHECK(ble_rf_trim_load() && !memcmp(ble_rf_trim, other, 68) && keeps == 1,
          "stock record crossing a scan boundary replaces a saved fallback");
    CHECK(ble_rf_trim_load() && keeps == 1, "unchanged stock record does not queue repeated saves");
    fail_program = 1;
    CHECK(st_save(OBJ_BLE, kept, sizeof kept) != 0, "failed new save leaves previous A/B copy readable");
    fail_program = 0;
    CHECK(st_load(OBJ_BLE, out, 68) == 68 && !memcmp(out, first, 68), "old fallback survives interrupted save");
    CHECK(st_save(OBJ_BLE, kept, sizeof kept) == 0, "stock replacement can be committed after failure");
    memset(flash + 0xE5000u, 0xFF, 0x17000u);
    memset(kept, 0, 68);
    kept_valid = st_load(OBJ_BLE, kept, 68) == 68;
    keeps = 0;
    CHECK(ble_rf_trim_load() && !memcmp(ble_rf_trim, other, 68) && !keeps,
          "saved device-specific record still wins after stock VM is erased");
    kept[3] ^= 1;
    CHECK(ble_rf_trim_load() && !memcmp(ble_rf_trim, first, 68) && keeps == 1,
          "damaged kept record is repaired with fallback, never sent to SDK");
    fail_read = 1;
    CHECK(!ble_rf_trim_load() && wifi_get_rf_trim_data(out, 68) == -1 && keeps == 1,
          "flash read failure cannot expose stale or partially scanned trim");
    fail_read = 0;
    CHECK(ble_rf_trim_load(), "complete scan works again after read failure");
    stock_record(0xE5000u + 4u, other);
    fail_read = (int)reads + 2;
    CHECK(!ble_rf_trim_load() && wifi_get_rf_trim_data(out, 68) == -1 && keeps == 1,
          "read failure after finding stock trim cannot publish a partial scan");
    memset(flash + 0xE5000u, 0xFF, 0x17000u);
    for (unsigned i = 0xE5000u; i < sizeof flash; i++)
        assert(flash[i] == 0xFF);
    printf(fails ? "ble_rf: %d FAILED\n" : "ble_rf: all checks passed\n", fails);
    return fails != 0;
}
