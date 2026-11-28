/*
 * Copyright (c) 2025
 * SPDX-License-Identifier: MIT
 *
 * Bluetooth profile / bond info dump for CLEAVE HHJP (central side).
 * Output format (one record per line):
 *   BTPROF|BEGIN|<device name>|<active profile>|<profile count>
 *   BTPROF|FW|<firmware commit (short)>
 *   BTPROF|ID|<own identity address>
 *   BTPROF|P|<index>|<active 0/1>|<bonded 0/1>|<connected 0/1>|<peer address>
 *   BTPROF|B|<bonded address>|<profile index or -1>
 *   BTPROF|LAYER|<highest active layer index>|<layer state bitmask (hex)>
 *   BTPROF|END
 *   BTPROF|ACK|<command>|<result>
 *
 * With CONFIG_CLEAVE_BT_PROFILE_CONTROL, these console commands are accepted:
 *   BTCMD dump | BTCMD clear <index> | BTCMD clearall | BTCMD disc <index>
 *   BTCMD boot（このキーボード（左手）をUF2書き込みモードで再起動）| BTCMD reset
 */

#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/sys/printk.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/addr.h>
#include <zephyr/drivers/uart.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/sys/reboot.h>
#include <dt-bindings/zmk/reset.h>

#ifndef CLEAVE_FW_SHA
#define CLEAVE_FW_SHA "unknown"
#endif

#include <zmk/ble.h>
#include <zmk/event_manager.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/keymap.h>
#include <zmk/events/layer_state_changed.h>

static void dump_bond(const struct bt_bond_info *info, void *user_data) {
    char a[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(&info->addr, a, sizeof(a));
    int idx = zmk_ble_profile_index(&info->addr);
    printk("BTPROF|B|%s|%d\n", a, idx >= 0 ? idx : -1);
}

static void print_layer(void) {
    printk("BTPROF|LAYER|%d|%x\n", zmk_keymap_highest_layer_active(),
           (unsigned int)zmk_keymap_layer_state());
}

static void dump(void) {
    char a[BT_ADDR_LE_STR_LEN];
    int active = zmk_ble_active_profile_index();

    printk("BTPROF|BEGIN|%s|%d|%d\n", CONFIG_BT_DEVICE_NAME, active, ZMK_BLE_PROFILE_COUNT);
    printk("BTPROF|FW|%s\n", CLEAVE_FW_SHA);

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
    print_layer();
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

/* レイヤー切替を設定ツールのレイヤ監視へ通知する */
static void layer_work_cb(struct k_work *work) { print_layer(); }
static K_WORK_DEFINE(layer_work, layer_work_cb);

static int layer_changed_cb(const zmk_event_t *eh) {
    k_work_submit(&layer_work);
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(cleave_layer_notify, layer_changed_cb);
ZMK_SUBSCRIPTION(cleave_layer_notify, zmk_layer_state_changed);

#if IS_ENABLED(CONFIG_CLEAVE_BT_PROFILE_CONTROL)
#define CMD_MAX 32
static const struct device *cmd_uart = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
static char rx_buf[CMD_MAX], cmd_buf[CMD_MAX];
static size_t rx_len;

static void clear_profile(uint8_t index) {
    int prev = zmk_ble_active_profile_index();
    if (prev != index) {
        zmk_ble_prof_select(index);
    }
    zmk_ble_clear_bonds();
    if (prev != index) {
        zmk_ble_prof_select(prev);
    }
}

static void cmd_work_cb(struct k_work *work) {
    char *c = cmd_buf;
    int rc = 0;
    if (strncmp(c, "BTCMD ", 6) != 0) {
        return;
    }
    c += 6;
    if (strcmp(c, "dump") == 0) {
        rc = 0;
    } else if (strncmp(c, "clear ", 6) == 0) {
        int i = atoi(c + 6);
        if (i < 0 || i >= ZMK_BLE_PROFILE_COUNT) {
            rc = -ERANGE;
        } else {
            clear_profile((uint8_t)i);
        }
    } else if (strcmp(c, "clearall") == 0) {
        zmk_ble_clear_all_bonds();
    } else if (strncmp(c, "disc ", 5) == 0) {
        int i = atoi(c + 5);
        rc = (i < 0 || i >= ZMK_BLE_PROFILE_COUNT) ? -ERANGE : zmk_ble_prof_disconnect((uint8_t)i);
    } else if (strcmp(c, "boot") == 0 || strcmp(c, "reset") == 0) {
        bool boot = c[0] == 'b';
        printk("BTPROF|ACK|%s|0\n", c);
        k_msleep(300); /* ACK を送り切ってから再起動する */
        sys_reboot(boot ? RST_UF2 : RST_COLD);
        return;
    } else {
        rc = -EINVAL;
    }
    printk("BTPROF|ACK|%s|%d\n", c, rc);
    k_work_reschedule(&dump_work, K_MSEC(300));
}
static K_WORK_DEFINE(cmd_work, cmd_work_cb);

static void uart_cb(const struct device *dev, void *user_data) {
    uint8_t ch;
    if (!uart_irq_update(dev)) {
        return;
    }
    while (uart_irq_rx_ready(dev) && uart_fifo_read(dev, &ch, 1) == 1) {
        if (ch == '\r' || ch == '\n') {
            if (rx_len > 0 && !k_work_is_pending(&cmd_work)) {
                rx_buf[rx_len] = '\0';
                memcpy(cmd_buf, rx_buf, rx_len + 1);
                k_work_submit(&cmd_work);
            }
            rx_len = 0;
        } else if (rx_len < CMD_MAX - 1) {
            rx_buf[rx_len++] = (char)ch;
        }
    }
}
#endif

static int bt_profile_dump_init(void) {
    k_work_schedule(&dump_work, K_SECONDS(5));
#if IS_ENABLED(CONFIG_CLEAVE_BT_PROFILE_CONTROL)
    if (device_is_ready(cmd_uart)) {
        uart_irq_callback_user_data_set(cmd_uart, uart_cb, NULL);
        uart_irq_rx_enable(cmd_uart);
    }
#endif
    return 0;
}

SYS_INIT(bt_profile_dump_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
