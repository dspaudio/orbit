/* SPDX-License-Identifier: GPL-3.0-only
 * Verify generated constants' static addresses, signed16 serialization and target sections.
 */
#include "felucca_op1_cluster.h"

const cls_tables *const cls_test_tables = &OP1_CLUSTER_TABLES;
const int16_t (*const cls_test_knobs)[8] = OP1_CLUSTER_KNOBS;
const char *const cls_test_names[16] = {
    OP1_CLUSTER_NAMES[0], OP1_CLUSTER_NAMES[1],
    OP1_CLUSTER_NAMES[2], OP1_CLUSTER_NAMES[3],
    OP1_CLUSTER_NAMES[4], OP1_CLUSTER_NAMES[5],
    OP1_CLUSTER_NAMES[6], OP1_CLUSTER_NAMES[7],
    OP1_CLUSTER_NAMES[8], OP1_CLUSTER_NAMES[9],
    OP1_CLUSTER_NAMES[10], OP1_CLUSTER_NAMES[11],
    OP1_CLUSTER_NAMES[12], OP1_CLUSTER_NAMES[13],
    OP1_CLUSTER_NAMES[14], OP1_CLUSTER_NAMES[15],
};

#ifndef CLS_DATA_ONLY
#include <stdio.h>
int main(void) {
    for (unsigned i = 0; i < 16; ++i) {
        for (unsigned j = 0; j < 8; ++j) {
            uint16_t x = (uint16_t)cls_test_knobs[i][j];
            putchar(x & 255); putchar(x >> 8);
        }
    }
    for (unsigned i = 0; i < 16; ++i)
        for (unsigned j = 0; j < 13; ++j)
            putchar((unsigned char)cls_test_names[i][j]);
    fprintf(stderr, "tables=%zu knobs=%zu names=%zu voice=%zu global=%zu\n",
            sizeof *cls_test_tables, sizeof OP1_CLUSTER_KNOBS,
            sizeof OP1_CLUSTER_NAMES, sizeof(cls_voice), sizeof(cls_global));
    return ferror(stdout) ? 2 : 0;
}
#endif
