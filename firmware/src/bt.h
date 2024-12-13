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

// The approximate number of milliseconds bluetooth takes to transmit one beacon.
// According to some LLM, a beacon typicall only takes 1-2ms to transmit.
const static int BT_BEACON_IX_MS = 4;

// The bluetooth stack callback function for beaconing in generic access profile.
void bt_esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
void bt_set_addr_from_key(esp_bd_addr_t addr, const uint8_t *public_key);
void bt_set_payload_from_key(uint8_t *payload, const uint8_t *public_key);
void bt_set_phy_addr_and_advert_data();
void bt_set_addr_and_payload_for_bit(uint32_t index, uint32_t msg_id, uint8_t bit);
void bt_send_data_once_blocking(uint8_t *data_to_send, uint32_t len, uint32_t msg_id);
void bt_send_location_once_blocking();
// Initialise bluetooth stack.
void bt_init();
void bt_loop();