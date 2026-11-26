/*
 * Copyright (c) 2025
 * SPDX-License-Identifier: MIT
 *
 * Bluetooth profile / bond info dump for CLEAVE HHJP (central side).
 * Output format (one record per line):
 *   BTPROF|BEGIN|<device name>|<active profile>|<profile count>
 *   BTPROF|ID|<own identity address>
 *   BTPROF|P|<index>|<active 0/1>|<bonded 0/1>|<connected 0/1>|<peer address>
 *   BTPROF|B|<bonded address>|<profile index or -1>
 *   BTPROF|END
 */

#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/sys/printk.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/addr.h>

#include <zmk/ble.h>
#include <zmk/event_manager.h>
#include <zmk/events/ble_active_profile_changed.h>

static void dump_bond(const struct bt_bond_info *info, void *user_data) {
    char a[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(&info->addr, a, sizeof(a));
    int idx = zmk_ble_profile_index(&info->addr);
    printk("BTPROF|B|%s|%d\n", a, idx >= 0 ? idx : -1);
}

static void dump(void) {
    char a[BT_ADDR_LE_STR_LEN];
    int active = zmk_ble_active_profile_index();

    printk("BTPROF|BEGIN|%s|%d|%d\n", CONFIG_BT_DEVICE_NAME, active, ZMK_BLE_PROFILE_COUNT);

    bt_addr_le_t ids[CONFIG_BT_ID_MAX];
    size_t count = CONFIG_BT_ID_MAX;
    bt_id_get(ids, &count);
    for (size_t i = 0; i < count; i++) {
        bt_addr_le_to_str(&ids[i], a, sizeof(a));
        printk("BTPROF|ID|%s\n", a);
    }

    for (uint8_t i = 0; i < ZMK_BLE_PROFILE_COUNT; i++) {
        bool open = zmk_ble_profile_is_open(i);
        bt_addr_le_to_str(zmk_ble_profile_address(i), a, sizeof(a));
        printk("BTPROF|P|%d|%d|%d|%d|%s\n", i, i == active, !open,
               zmk_ble_profile_is_connected(i), open ? "-" : a);
    }

    bt_foreach_bond(BT_ID_DEFAULT, dump_bond, NULL);
    printk("BTPROF|END\n");
}

static void dump_work_cb(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(dump_work, dump_work_cb);

static void dump_work_cb(struct k_work *work) {
    dump();
    k_work_schedule(&dump_work, K_SECONDS(CONFIG_CLEAVE_BT_PROFILE_DUMP_INTERVAL_SEC));
}

static int profile_changed_cb(const zmk_event_t *eh) {
    k_work_reschedule(&dump_work, K_MSEC(500));
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(cleave_bt_profile_dump, profile_changed_cb);
ZMK_SUBSCRIPTION(cleave_bt_profile_dump, zmk_ble_active_profile_changed);

static int bt_profile_dump_init(void) {
    k_work_schedule(&dump_work, K_SECONDS(5));
    return 0;
}

SYS_INIT(bt_profile_dump_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
