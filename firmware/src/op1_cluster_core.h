/* SPDX-License-Identifier: GPL-3.0-only
 * Standalone product C core of official OP-1 #246 machine code; original asset rights are separate.
 */
#ifndef OP1_CLUSTER_CORE_H
#define OP1_CLUSTER_CORE_H
#include <stdint.h>

typedef struct { uint8_t bytes[108]; } cls_voice;
typedef struct {
    int16_t wave[8193];
    int16_t count[8];
    int32_t curve[24];
} cls_tables;
typedef struct {
    uint32_t seed;
    int16_t gain[8], control[8];
    int16_t master;
} cls_global;

/* Accept original u32 waveform and binary32 bits, without sine generation or host floats. */
void cls_tables_init(cls_tables *, const uint32_t wave[8193],
                     const uint32_t count[8], const uint32_t curve[24]);
void cls_global_init(cls_global *);
/* Constructor and note-init preserve padding untouched by the original. */
void cls_construct(cls_voice *);
void cls_note_init(cls_voice *, cls_global *, const cls_tables *, const int16_t knobs[8]);
void cls_release(cls_voice *);
void cls_decay(cls_global *);
/* Contract: 1 <= knobs[0] >>> 11 <= 8, 0 <= knobs[2] >>> 10 < 24.
 * Pitch/ADSR/FX belong to the external layer; phase contains 129 original input words.
 */
/* Return all R0 bits left by the original final halfword operation, without bool normalization. */
uint32_t cls_render(cls_voice *, cls_global *, const cls_tables *, int16_t output[128],
                    const uint32_t phase[129], const int16_t knobs[8]);
#endif
