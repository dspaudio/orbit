/* SPDX-License-Identifier: GPL-3.0-only */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#define A2DP_POOL
static uint32_t a2dp_generation;
static volatile uint8_t a2dp_active;
static void local_irq_disable(void) {}
static void local_irq_enable(void) {}
static void os_time_dly(int ticks);
#include "../firmware/src/ble/a2dp_pcm.c"
#include "../firmware/src/ble/a2dp_sbc.h"
#define BLE_POOL
static uint8_t fake_store[21 * 32], fake_valid;    /* felucca.c's BLM1 keys, as a power cycle leaves them */
static unsigned fake_puts;
void core_bt_keys_get(uint8_t *p, uint32_t n)
{
    if (fake_valid)
        memcpy(p, fake_store, n);
    else
        memset(p, 0xff, n);
}
void core_bt_keys_put(const uint8_t *p, uint32_t n)
{
    memcpy(fake_store, p, n);
    fake_valid = 1;
    fake_puts++;
}
#include "../firmware/src/ble/a2dp_keys.c"

static unsigned received, stop_on_wait;
static int32_t expected_l, expected_r;

static void consume(void)
{
    int32_t l[256], r[256];
    unsigned i;
    for (i = 0; i < 256; ++i)
        l[i] = 123, r[i] = -321;
    a2dp_mix(l, r, 256, 128);
    for (i = 0; i < 256; ++i) {
        if (l[i] != 123 || r[i] != -321) {
            assert(l[i] == 123 + expected_l && r[i] == -321 + expected_r);
            ++received;
        }
    }
}

static void os_time_dly(int ticks)
{
    assert(ticks == 1);
    if (stop_on_wait) {
        a2dp_active = 0;
        ++a2dp_generation;
    } else {
        consume();
    }
}

static void reset(void)
{
    a2dp_pcm_reset();
    a2dp_active = 1;
    received = stop_on_wait = 0;
}

static void ratio(unsigned rate, unsigned channels, unsigned input, unsigned output)
{
    int16_t *pcm = malloc(input * channels * sizeof *pcm);
    unsigned i;
    assert(pcm);
    reset();
    expected_l = 1000;
    expected_r = channels == 2 ? -2000 : 1000;
    for (i = 0; i < input; ++i) {
        pcm[i * channels] = (int16_t)expected_l;
        if (channels == 2)
            pcm[i * channels + 1] = (int16_t)expected_r;
    }
    /* One producer call keeps correction zero and spans a full ring wrap. */
    a2dp_pcm_write(pcm, input, channels, rate, a2dp_generation);
    while (a2dp_head != a2dp_tail)
        consume();
    assert(received == output);
    consume();
    assert(received == output); /* Underrun adds silence, not stale PCM. */
    free(pcm);
}

static void prebuffer_and_stop(void)
{
    int16_t pcm[8192];
    unsigned i;
    reset();
    expected_l = expected_r = 2222;
    for (i = 0; i < 8192; ++i)
        pcm[i] = 2222;
    a2dp_pcm_write(pcm, 1025, 1, 44100, a2dp_generation);
    consume();
    assert(received == 0);
    a2dp_pcm_write(pcm, 1024, 1, 44100, a2dp_generation);
    consume();
    assert(received == 256);
    stop_on_wait = 1;
    a2dp_pcm_write(pcm, 8192, 1, 44100, a2dp_generation);
    assert(!a2dp_active);
    consume();
    assert(received == 256);
    a2dp_active = 1;
    consume();
    assert(received == 256); /* A rapid restart cannot play the previous stream's ring. */
    reset();
    consume();
    assert(received == 0);
}

static void drift_and_extremes(void)
{
    int16_t pcm[4096];
    unsigned i, before;
    reset();
    expected_l = expected_r = 32767;
    for (i = 0; i < 4096; ++i)
        pcm[i] = 32767;
    a2dp_pcm_write(pcm, 3501, 1, 44100, a2dp_generation);
    consume();
    /* High fill must consume source faster and produce fewer PCM samples. */
    a2dp_correction = 99;
    before = a2dp_head;
    a2dp_pcm_write(pcm, 512, 1, 44100, a2dp_generation);
    assert(a2dp_head - before < 512 && a2dp_correction == 100);
    reset();
    for (i = 0; i < 4096; ++i)
        pcm[i] = i & 1u ? 32767 : -32768;
    a2dp_pcm_write(pcm, 2048, 1, 48000, a2dp_generation);
    for (i = 0; i < a2dp_head; ++i)
        assert(a2dp_pcm[i][0] == a2dp_pcm[i][1]);
    /* Ring counters may wrap after a long playing session. */
    reset();
    a2dp_head = a2dp_tail = UINT32_MAX - 1023u;
    expected_l = expected_r = -3000;
    for (i = 0; i < 4096; ++i)
        pcm[i] = -3000;
    a2dp_pcm_write(pcm, 4096, 1, 44100, a2dp_generation);
    while (a2dp_head != a2dp_tail)
        consume();
    assert(received == 4095);
}

int main(void)
{
    uint8_t frame[540] = {0x9c, 0xfd, 53, 0};
    uint8_t key[32], copy[32];
    unsigned i;
    assert(a2dp_frame_size(frame, 119) == 119); /* 48 kHz joint stereo, 16 x 8, bitpool 53. */
    assert(!a2dp_frame_size(frame, 118));
    frame[1] = 0xf5; frame[2] = 128;
    assert(a2dp_frame_size(frame, 524) == 524); /* Largest dual-channel frame. */
    frame[2] = 129;
    assert(!a2dp_frame_size(frame, sizeof frame));
    frame[1] = 0xf8; frame[2] = 129; /* Four subbands: stereo bitpool at most 128. */
    assert(!a2dp_frame_size(frame, sizeof frame));
    frame[0] = 0;
    assert(!a2dp_frame_size(frame, sizeof frame));
    assert(!a2dp_frame_size(frame, 3));
    assert(bt_vm_interface());
    assert(!a2dp_key_open(61, 32) && !a2dp_key_open(83, 32));
    assert(!a2dp_key_open(62, 33));
    for (i = 0; i < sizeof key; ++i)
        key[i] = (uint8_t)(i * 7u);
    assert(a2dp_key_open(62, 32));
    assert(a2dp_key_write(key, sizeof key) == 32);
    assert(!a2dp_key_write(key, 1));
    assert(fake_puts == 0 && !a2dp_key_close() && fake_puts == 1);   /* a write: handed to the flash store */
    assert(a2dp_key_open(62, 32) && a2dp_key_read(copy, 32) == 32 && !a2dp_key_close() && fake_puts == 1);
    assert(a2dp_key_open(62, 32));
    assert(a2dp_key_read(copy, sizeof copy) == 32 && !memcmp(key, copy, 32));
    assert(a2dp_key_open(82, 32));
    assert(a2dp_key_read(copy, sizeof copy) == 32);
    for (i = 0; i < sizeof copy; ++i)
        assert(copy[i] == 0xff);
    assert(a2dp_key_seek(33, 0) == -1);
    assert(bt_vm_interface()); /* A power cycle: the paired phone's key comes back from flash. */
    assert(a2dp_key_open(62, 32) && a2dp_key_read(copy, sizeof copy) == 32 && !memcmp(key, copy, 32));
    assert(a2dp_key_open(63, 32) && a2dp_key_read(copy, sizeof copy) == 32);
    for (i = 0; i < sizeof copy; ++i)
        assert(copy[i] == 0xff);
    ratio(48000, 2, 4801, 4410);
    ratio(44100, 2, 4411, 4410);
    ratio(32000, 1, 3201, 4410);
    ratio(16000, 1, 1601, 4410);
    prebuffer_and_stop();
    drift_and_extremes();
    puts("a2dp_pcm_test: PASS (rates, stereo, wrap, underrun, stop, drift, SBC bounds, session keys)");
    return 0;
}
