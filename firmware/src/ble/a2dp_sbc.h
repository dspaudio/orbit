/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ORBIT_A2DP_SBC_H
#define ORBIT_A2DP_SBC_H
#include <stdint.h>
#define A2DP_SBC_BYTES 540u
/* Validate the phone-controlled frame length before starting hardware DMA. */
static unsigned a2dp_frame_size(const uint8_t *p, unsigned len)
{
    unsigned bands, blocks, mode, channels, bits, size;
    if (len < 4 || p[0] != 0x9c)
        return 0;
    bands = p[1] & 1u ? 8u : 4u;
    blocks = 4u * (1u + ((p[1] >> 4) & 3u));
    mode = (p[1] >> 2) & 3u;
    channels = mode ? 2u : 1u;
    if (p[2] < 2u || p[2] > 250u || p[2] > (mode < 2u ? 16u : 32u) * bands)
        return 0;
    bits = blocks * p[2] * (mode == 1u ? 2u : 1u);
    if (mode == 3u)
        bits += bands;
    size = 4u + bands * channels / 2u + (bits + 7u) / 8u;
    return size <= len && size <= A2DP_SBC_BYTES ? size : 0;
}
#endif
