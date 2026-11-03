/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ORBIT_A2DP_H
#define ORBIT_A2DP_H
#include <stdint.h>
void orbit_a2dp_init(const uint8_t mac[6]);
void a2dp_event(int from, unsigned event, unsigned value);
void a2dp_enable(int enabled);
int a2dp_streaming(void);
int a2dp_link(void);                            /* a phone connected (the mixer's BT AUDIO page) */                       /* a phone's stream plays: the BT tasks get a longer turn */
/* Audio ISR: consumes already resampled PCM, never calls the SDK. */
void a2dp_mix(int32_t *left, int32_t *right, uint32_t frames, int32_t gain_q7);   /* gain: 128 = as sent */
#endif
