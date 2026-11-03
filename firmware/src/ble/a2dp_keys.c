/* SPDX-License-Identifier: GPL-3.0-only */
/* Pinned database_file.c syscfg_file ABI: 21 records, each 32 bytes, IDs 62..82.
 * Kept in ORBIT's BLM1 flash object (felucca.c core_bt_keys_*): read at the start,
 * handed back after each write and saved when the FM-1 is quiet, so a paired phone
 * reconnects after a power cycle. */
void core_bt_keys_get(uint8_t *p, uint32_t n);
void core_bt_keys_put(const uint8_t *p, uint32_t n);
static uint8_t a2dp_key_wrote;
static uint8_t a2dp_keys[21][32] BLE_POOL;
static uint32_t a2dp_key_index, a2dp_key_pos;
static int a2dp_key_open(uint32_t id, uint32_t size)
{
    if (id < 62u || id > 82u || size != 32u)
        return 0;
    a2dp_key_index = id - 62u;
    a2dp_key_pos = 0;
    return 1;
}
static int a2dp_key_read(void *buf, uint32_t len)
{
    if (len > 32u - a2dp_key_pos)
        return 0;
    memcpy(buf, a2dp_keys[a2dp_key_index] + a2dp_key_pos, len);
    a2dp_key_pos += len;
    return (int)len;
}
static int a2dp_key_write(const void *buf, uint32_t len)
{
    if (len > 32u - a2dp_key_pos)
        return 0;
    memcpy(a2dp_keys[a2dp_key_index] + a2dp_key_pos, buf, len);
    a2dp_key_pos += len;
    a2dp_key_wrote = 1;
    return (int)len;
}
static int a2dp_key_seek(uint32_t offset, uint32_t origin)
{
    if (origin || offset > 32u)
        return -1;
    a2dp_key_pos = offset;
    return 0;
}
static int a2dp_key_close(void)
{
    if (a2dp_key_wrote) {
        a2dp_key_wrote = 0;
        core_bt_keys_put(&a2dp_keys[0][0], sizeof a2dp_keys);
    }
    return 0;
}
void *bt_vm_interface(void)
{
    static const struct {
        int (*open)(uint32_t, uint32_t);
        int (*read)(void *, uint32_t);
        int (*write)(const void *, uint32_t);
        int (*seek)(uint32_t, uint32_t);
        int (*close)(void);
    } ops = {a2dp_key_open, a2dp_key_read, a2dp_key_write, a2dp_key_seek, a2dp_key_close};
    core_bt_keys_get(&a2dp_keys[0][0], sizeof a2dp_keys);
    return (void *)&ops;
}
