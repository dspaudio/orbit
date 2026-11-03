/* SPDX-License-Identifier: GPL-3.0-only */
/* Built against the pinned SDK headers. Its SBC hardware driver supplies PCM
 * directly; no SDK audio server, DAC, resampler or source profile is linked. */
#include "system/includes.h"
#include "btstack/avctp_user.h"
#include "btstack/a2dp_media_codec.h"
#include "classic/hci_lmp.h"
#include "event/bt_event.h"
#include "btstack/bluetooth.h"
#include "asm/hwaccel.h"
#include "a2dp.h"
#include "a2dp_sbc.h"

#define A2DP_POOL __attribute__((section(".pool"), aligned(4)))
static uint32_t a2dp_generation, a2dp_decode_generation;
static volatile uint8_t a2dp_active;
static uint8_t a2dp_enabled, a2dp_ready, a2dp_connected, a2dp_mac[6];
#include "a2dp_pcm.c"

static int16_t a2dp_output[256] A2DP_POOL;
static uint8_t a2dp_frame[A2DP_SBC_BYTES] A2DP_POOL;
static uint8_t a2dp_packet[2048] A2DP_POOL;
static uint32_t a2dp_rate, a2dp_channels;
static const struct audio_hwaccel_ops *a2dp_hw;
const uint8_t log_tag_const_e_AUDIO_SBC = 0;
extern void __set_simple_pair_flag(bool flag);

static void *a2dp_output_space(void *priv, int *size)
{
    (void)priv;
    *size = sizeof a2dp_output;
    return a2dp_output;
}

static int a2dp_output_finish(void *priv, void *buf, int size)
{
    (void)priv;
    if (size > 0 && size <= sizeof a2dp_output && !(size % (a2dp_channels * 2u)))
        a2dp_pcm_write(buf, (uint32_t)size / (2u * a2dp_channels),
                       a2dp_channels, a2dp_rate, a2dp_decode_generation);
    return size;
}

static const struct audio_stream_ops a2dp_output_ops = {
    .alloc_free_space = a2dp_output_space, .finish = a2dp_output_finish
};

volatile uint32_t a2dp_dec_ticks;               /* time in the SBC decode calls (with their waits), ticks */
volatile uint32_t a2dp_stat[4];                 /* packets taken, too large (dropped), frames decoded, empty polls */
static void a2dp_decode_frame(void *handle, const uint8_t *data, unsigned len)
{
    static const unsigned rates[4] = {16000, 32000, 44100, 48000};
    unsigned rate = rates[data[1] >> 6];
    if (a2dp_rate != rate) {
        a2dp_pcm_reset();
        a2dp_rate = rate;
    }
    a2dp_channels = (data[1] & 12u) ? 2u : 1u;
    if (data != a2dp_frame)
        memcpy(a2dp_frame, data, len);
    a2dp_stat[2]++;
    {
        uint32_t t0 = *(volatile uint32_t *)0x10804u;  /* (TIMER4, 24 MHz: fm1_ble_hal.h fm1_ble_ticks) */
        a2dp_hw->decode(handle, a2dp_frame, (int)len);
        a2dp_dec_ticks += *(volatile uint32_t *)0x10804u - t0;
    }
}

static void a2dp_decoder(void *arg)
{
    unsigned seen = 0, frag = 0;
    void *handle = NULL;
    (void)arg;
    for (;;) {
        uint8_t *packet = NULL;
        int len;
        unsigned pos, end, payload, n;
        if (seen != a2dp_generation) {
            if (handle) {
                a2dp_hw->decode_stop(handle);
                a2dp_hw->close(handle);
                handle = NULL;
            }
            seen = a2dp_generation;
            a2dp_pcm_reset();
            frag = a2dp_rate = 0;
        }
        if (!a2dp_active) {
            os_time_dly(1);
            continue;
        }
        if (!handle) {
            struct audio_hw_codec_info info = {
                .format = "sbc", .mode = AUDIO_HW_DEC, .pcm_format = PCM_FMT_ORIG_CH,
                .ops = &a2dp_output_ops
            };
            handle = a2dp_hw->open(&info);
            if (!handle) {
                a2dp_active = 0;
                continue;
            }
        }
        len = a2dp_media_try_get_packet(&packet);
        a2dp_stat[len > 0 && packet ? 0 : 3]++;
        if (len <= 0 || !packet) {
            os_time_dly(1);
            continue;
        }
        /* The stack can close its channel while decode waits for the SBC IRQ.
         * Release its packet before yielding; only our private bytes survive. */
        if ((unsigned)len > sizeof a2dp_packet) {
            a2dp_stat[1]++;
            a2dp_media_free_packet(packet);
            continue;
        }
        memcpy(a2dp_packet, packet, (unsigned)len);
        a2dp_media_free_packet(packet);
        packet = a2dp_packet;
        a2dp_decode_generation = seen;
        end = (unsigned)len;
        if (end < 13u || (packet[0] >> 6) != 2u)
            goto release;
        pos = 12u + 4u * (packet[0] & 15u);
        if (packet[0] & 16u) {
            if (pos + 4u > end)
                goto release;
            pos += 4u + 4u * ((unsigned)packet[pos + 2u] * 256u + packet[pos + 3u]);
        }
        if (packet[0] & 32u) {
            n = packet[end - 1u];
            if (!n || n > end)
                goto release;
            end -= n;
        }
        if (pos >= end)
            goto release;
        payload = packet[pos++];
        if (payload & 128u) {
            if (payload & 64u)
                frag = 0;
            else if (!frag)
                goto release;
            n = end - pos;
            if (n > sizeof a2dp_frame - frag) {
                frag = 0;
                goto release;
            }
            memcpy(a2dp_frame + frag, packet + pos, n);
            frag += n;
            if (payload & 32u) {
                if (a2dp_frame_size(a2dp_frame, frag) == frag)
                    a2dp_decode_frame(handle, a2dp_frame, frag);
                frag = 0;
            }
        } else {
            frag = 0;
            for (n = payload & 15u; n && a2dp_active && seen == a2dp_generation; --n) {
                unsigned size = a2dp_frame_size(packet + pos, end - pos);
                if (!size)
                    break;
                a2dp_decode_frame(handle, packet + pos, size);
                pos += size;
            }
        }
release:
        os_time_dly(0); /* Give BLE-MIDI and the controller a turn after each packet. */
    }
}

static void a2dp_scan(void)
{
    int scan = a2dp_enabled && !a2dp_connected;
    user_send_cmd_prepare(scan ? USER_CTRL_WRITE_SCAN_ENABLE : USER_CTRL_WRITE_SCAN_DISABLE, 0, NULL);
    user_send_cmd_prepare(scan ? USER_CTRL_WRITE_CONN_ENABLE : USER_CTRL_WRITE_CONN_DISABLE, 0, NULL);
}
/* the controller's classic scans end on their own (seen on the FM-1: page_scan and inquiry_scan off a while after
 * BT_STATUS_INIT_OK); asked again every 2 s while no phone is connected, in the stack's own task (app_core), as
 * the SDK's demos send their commands (link_*_scan_enable returns at once while a scan runs) */
static void a2dp_scan_timer(void *p)
{
    (void)p;
    if (a2dp_ready && a2dp_enabled && !a2dp_connected)
        a2dp_scan();
}

void orbit_a2dp_init(const uint8_t mac[6])
{
    memcpy(a2dp_mac, mac, sizeof a2dp_mac);
    a2dp_mac[0] ^= 0x01u;                       /* its own address: one shared with BLE-MIDI listed it twice (iOS) */
    __set_user_ctrl_conn_num(1);
    __set_auto_conn_device_num(0);              /* (no reconnect paging: it stops the scans, and no keys survive a reboot) */
    __set_disable_sco_flag(1);
    __set_support_aac_flag(0);
    __set_simple_pair_flag(1);
    __set_simple_pair_param(3, 0, 2);
    a2dp_hw = audio_hwaccel_begin;
    a2dp_enabled = 1;
    if (a2dp_hw < audio_hwaccel_end && !strcmp(a2dp_hw->name, "sbc"))
        task_create(a2dp_decoder, NULL, "decoder");
}

int a2dp_streaming(void) { return a2dp_active; }
int a2dp_link(void) { return a2dp_connected != 0; }

void a2dp_enable(int enabled)
{
    if (a2dp_enabled == !!enabled)
        return;
    a2dp_enabled = !!enabled;
    if (!enabled) {
        a2dp_active = 0;
        ++a2dp_generation;
        if (a2dp_connected)
            user_send_cmd_prepare(USER_CTRL_DISCONNECTION_HCI, 0, NULL);
    }
    if (a2dp_ready)
        a2dp_scan();
}

void a2dp_event(int from, unsigned event, unsigned value)
{
    if (from == BT_EVENT_FROM_HCI) {             /* pairing: accept, as the SDK's demo_edr does (no screen to confirm on) */
        if (event == HCI_EVENT_USER_CONFIRMATION_REQUEST || event == HCI_EVENT_PIN_CODE_REQUEST) {
            u8 yes = 1;
            user_send_cmd_prepare(USER_CTRL_PAIR, 1, &yes);
        }
        return;
    }
    if (from != BT_EVENT_FROM_CON)
        return;
    switch (event) {
    case BT_STATUS_INIT_OK:
        lmp_hci_write_local_address(a2dp_mac);
        lmp_hci_write_local_name("ORBIT FM-1 Audio");
        lmp_hci_write_class_of_device(0x240414); /* Audio/rendering, loudspeaker. */
        a2dp_ready = 1;
        a2dp_scan();
        sys_timer_add_to_task("app_core", NULL, a2dp_scan_timer, 2000);   /* (each start: the BT OS clears its timers) */
        break;
    case BT_STATUS_FIRST_CONNECTED:
        a2dp_connected = 1;
        a2dp_scan();
        break;
    case BT_STATUS_FIRST_DISCONNECT:
        a2dp_connected = a2dp_active = 0;
        ++a2dp_generation;
        a2dp_scan();
        break;
    case BT_STATUS_A2DP_MEDIA_START:
        ++a2dp_generation;
        a2dp_active = a2dp_enabled && value == 0; /* SBC only. */
        break;
    case BT_STATUS_A2DP_MEDIA_STOP:
        a2dp_active = 0;
        ++a2dp_generation;
        break;
    default:
        break;
    }
}
