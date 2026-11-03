/* SPDX-License-Identifier: GPL-3.0-only */
/* Keep the pinned SDK configuration and its types; omit manufacturing modes
 * and voice links from this single-phone music receiver. */
#define CONFIG_TEST_DUT_CODE sdk_test_dut_code
#define CONFIG_TEST_FCC_CODE sdk_test_fcc_code
#define CONFIG_LMP_MASTER_ESCO_ENABLE sdk_master_esco_enable
#include "/sdk/apps/common/config/log_config/lib_btctrler_config.c"
#undef CONFIG_TEST_DUT_CODE
#undef CONFIG_TEST_FCC_CODE
#undef CONFIG_LMP_MASTER_ESCO_ENABLE
const int CONFIG_TEST_DUT_CODE = 0;
const int CONFIG_TEST_FCC_CODE = 0;
const int CONFIG_LMP_MASTER_ESCO_ENABLE = 0;
/* (the LE roles stay the SDK's: ORBIT's BLE host role, ble_central.c, scans, initiates and connects as master) */
