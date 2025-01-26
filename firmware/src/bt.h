#pragma once
#include <esp_bt.h>
#include <esp_bt_defs.h>
#include <esp_bt_main.h>
#include <esp_gap_ble_api.h>
#include <esp_gatt_defs.h>
#include <esp_gattc_api.h>
#include <esp_log.h>
#include <esp_random.h>
#include <esp_sleep.h>
#include "bme280.h"

typedef enum
{
    BT_TX_SCHED_REGULAR,
    BT_TX_SCHED_BUTTON_AND_LOCATION,
} bt_tx_sched_t;

typedef struct
{
    uint8_t data[2];
    bme280_sample bme280;
    int nearby_device_count;
    int message_value;
} bt_iter_snapshot;

// Approximate (worst) beacon transmission time, which is based on 17 millis per channel and 3 channels in total.
const int BT_BEACON_IX_MS = 20 * 3;
const int BT_TASK_LOOP_INTERVAL_MILLIS = 3000;
// Most data bits arrive within seconds of each other, but occasionally they can arrive a little over 5 minutes from each other.
const int BT_TX_ITER_DURATION_MILLIS = 6 * 60 * 1000;

const int BT_TX_ITER_TEMP = 0;
const int BT_TX_ITER_HUMID = 1;
const int BT_TX_ITER_PRESS = 2;
const int BT_TX_ITER_LOCATION = 3;
const int BT_TX_ITER_DEVICE_COUNT = 4;
const int BT_TX_ITER_MESSAGE = 5;
const int BT_TX_TOTAL_OPTIONS = 6;

const int BT_SCAN_DURATION_SEC = 5;
const int BT_SCAN_INTERVAL_SEC = 60;

extern bt_iter_snapshot bt_iter;
extern int bt_tx_iter;
extern int bt_nearby_device_count;
extern int bt_tx_message_value;

void bt_init();
void bt_task_work();
void bt_task_fun(void *_);

// The bluetooth stack callback function for beaconing in generic access profile.
void bt_esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
void bt_set_addr_from_key(esp_bd_addr_t addr, const uint8_t *public_key);
void bt_set_payload_from_key(uint8_t *payload, const uint8_t *public_key);
void bt_set_phy_addr_and_advert_data();
int bt_get_remaining_transmission_ms();
void bt_update_beacon_iter();
void bt_start_scan_nearby_devices();
void bt_set_addr_and_payload_for_bit(uint32_t index, uint32_t msg_id, uint8_t bit);
void bt_send_data_once_blocking(uint8_t *data_to_send, uint32_t len, uint32_t msg_id);
void bt_send_location_once();
void bt_send_data_bme280_temp();
void bt_send_data_bme280_humid();
void bt_send_data_bme280_press();
void bt_send_nearby_dev_count();
void bt_send_message();