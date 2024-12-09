// hzgl-air-bridge beacon firmware.

#include <Arduino.h>
#include <esp_bt.h>
#include <esp_bt_defs.h>
#include <esp_bt_main.h>
#include <esp_gap_ble_api.h>
#include <esp_gatt_defs.h>
#include <esp_gattc_api.h>
#include <esp_log.h>
#include <esp_random.h>
#include <esp_sleep.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "pubkey.h"
#include "uECC.h"

static const char *LOG_TAG = "hzgl-air-bridge";

#define CHECK_BIT(var, pos) ((var) & (1 << (7 - pos)))

/** Callback function for BT events */
static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);

/** Random device address */
static esp_bd_addr_t rnd_addr = {0xFF, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};

/** Advertisement payload */
static uint8_t adv_data[31] = {
    0x1e,       /* Length (30) */
    0xff,       /* Manufacturer Specific Data (type 0xff) */
    0x4c, 0x00, /* Company ID (Apple) */
    0x12, 0x19, /* Offline Finding type and length */
    0x00,       /* State */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, /* First two bits */
    0x00, /* Hint (0x00) */
};

/* https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/bluetooth/esp_gap_ble.html#_CPPv420esp_ble_adv_params_t */
static esp_ble_adv_params_t ble_adv_params = {
    // Advertising min interval:
    // Minimum advertising interval for undirected and low duty cycle
    // directed advertising. Range: 0x0020 to 0x4000 Default: N = 0x0800
    // (1.28 second) Time = N * 0.625 msec Time Range: 20 ms to 10.24 sec
    .adv_int_min = 0x0640, // 1s
    // Advertising max interval:
    // Maximum advertising interval for undirected and low duty cycle
    // directed advertising. Range: 0x0020 to 0x4000 Default: N = 0x0800
    // (1.28 second) Time = N * 0.625 msec Time Range: 20 ms to 10.24 sec
    .adv_int_max = 0x0C80, // 2s
    // Advertisement type
    .adv_type = ADV_TYPE_NONCONN_IND,
    // Use the random address
    .own_addr_type = BLE_ADDR_TYPE_RANDOM,
    // All channels
    .channel_map = ADV_CHNL_ALL,
    // Allow both scan and connection requests from anyone.
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    esp_err_t err;

    switch (event)
    {
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
        esp_ble_gap_start_advertising(&ble_adv_params);
        break;

    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        // adv start complete event to indicate adv start successfully or failed
        if ((err = param->adv_start_cmpl.status) != ESP_BT_STATUS_SUCCESS)
        {
            ESP_LOGE(LOG_TAG, "advertising start failed: %s", esp_err_to_name(err));
        }
        else
        {
            ESP_LOGI(LOG_TAG, "advertising has started.");
        }
        break;

    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        if ((err = param->adv_stop_cmpl.status) != ESP_BT_STATUS_SUCCESS)
        {
            ESP_LOGE(LOG_TAG, "adv stop failed: %s", esp_err_to_name(err));
        }
        else
        {
            ESP_LOGI(LOG_TAG, "stop adv successfully");
        }
        break;
    default:
        break;
    }
}

void set_addr_from_key(esp_bd_addr_t addr, uint8_t *public_key)
{
    addr[0] = public_key[0] | 0b11000000;
    addr[1] = public_key[1];
    addr[2] = public_key[2];
    addr[3] = public_key[3];
    addr[4] = public_key[4];
    addr[5] = public_key[5];
}

void set_payload_from_key(uint8_t *payload, uint8_t *public_key)
{
    /* copy last 22 bytes */
    memcpy(&payload[7], &public_key[6], 22);
    /* append two bits of public key */
    payload[29] = public_key[0] >> 6;
}

void reset_advertising()
{
    esp_err_t status;
    if ((status = esp_ble_gap_set_rand_addr(rnd_addr)) != ESP_OK)
    {
        ESP_LOGE(LOG_TAG, "couldn't set random address: %s", esp_err_to_name(status));
        return;
    }
    if ((esp_ble_gap_config_adv_data_raw((uint8_t *)&adv_data, sizeof(adv_data))) != ESP_OK)
    {
        ESP_LOGE(LOG_TAG, "couldn't configure BLE adv: %s", esp_err_to_name(status));
        return;
    }
}

int is_valid_pubkey(uint8_t *pub_key_compressed)
{
    uint8_t with_sign_byte[29];
    uint8_t pub_key_uncompressed[128];
    const struct uECC_Curve_t *curve = uECC_secp224r1();
    with_sign_byte[0] = 0x02;
    memcpy(&with_sign_byte[1], pub_key_compressed, 28);
    uECC_decompress(with_sign_byte, pub_key_uncompressed, curve);
    if (!uECC_valid_public_key(pub_key_uncompressed, curve))
    {
        ESP_LOGW(LOG_TAG, "Generated public key tested as invalid");
        return 0;
    }
    return 1;
}

void copy_4b_big_endian(uint8_t *dst, const uint32_t *src)
{
    uint32_t value = *src;
    dst[0] = (value >> 24) & 0xFF;
    dst[1] = (value >> 16) & 0xFF;
    dst[2] = (value >> 8) & 0xFF;
    dst[3] = value & 0xFF;
}

// index as first part of payload to have an often changing MAC address
// [2b magic] [4byte index] [4byte msg_id] [4byte modem_id] [000.000] [1bit]
// There is a rade-off between sending and receiving throughput (e.g. we could also use a 1-byte lookup table)
void set_addr_and_payload_for_bit(uint32_t index, uint32_t msg_id, uint8_t bit)
{
    uint32_t valid_key_counter = 0;
    static uint8_t public_key[28] = {0};
    public_key[0] = pubkey_magic1;
    public_key[1] = pubkey_magic2;
    copy_4b_big_endian(&public_key[2], &index);
    copy_4b_big_endian(&public_key[6], &msg_id);
    copy_4b_big_endian(&public_key[10], &modem_id);
    public_key[27] = bit;
    do
    {
        copy_4b_big_endian(&public_key[14], &valid_key_counter);
        // here, you could call `pub_from_priv(public_key, private_key)` to instead treat the payload as private key
        valid_key_counter++; // for next round
    } while (!is_valid_pubkey(public_key));
    ESP_LOGI(LOG_TAG, "  pub key to use (%d. try): %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x ... %02x", valid_key_counter, public_key[0], public_key[1], public_key[2], public_key[3], public_key[4], public_key[5], public_key[6], public_key[7], public_key[8], public_key[9], public_key[10], public_key[11], public_key[12], public_key[13], public_key[14], public_key[15], public_key[16], public_key[17], public_key[19], public_key[19], public_key[20], public_key[21], public_key[22], public_key[23], public_key[24], public_key[25], public_key[26], public_key[27]);
    set_addr_from_key(rnd_addr, public_key);
    set_payload_from_key(adv_data, public_key);
}

void send_data_once_blocking(uint8_t *data_to_send, uint32_t len, uint32_t msg_id)
{
    ESP_LOGI(LOG_TAG, "Data to send (msg_id: %d): %s", msg_id, data_to_send);
    uint8_t current_bit = 0;
    // iterate byte-by-byte
    for (int by_i = 0; by_i < len; by_i++)
    {
        ESP_LOGI(LOG_TAG, "  Sending byte %d/%d (0x%02x)", by_i, len - 1, data_to_send[by_i]);
        // iterate bit-by-bit
        for (int bi_i = 0; bi_i < 8; bi_i++)
        {
            if (CHECK_BIT(data_to_send[by_i], bi_i))
            {
                current_bit = 1;
            }
            else
            {
                current_bit = 0;
            }
            ESP_LOGD(LOG_TAG, "  Sending byte %d, bit %d: %d", by_i, bi_i, current_bit);
            set_addr_and_payload_for_bit(by_i * 8 + bi_i, msg_id, current_bit);
            ESP_LOGD(LOG_TAG, "    resetting. Will now use device address: %02x %02x %02x %02x %02x %02x", rnd_addr[0], rnd_addr[1], rnd_addr[2], rnd_addr[3], rnd_addr[4], rnd_addr[5]);
            delay(100);
            reset_advertising();
        }
    }
    esp_ble_gap_stop_advertising();
}

void setup(void)
{
    esp_log_level_set("*", ESP_LOG_VERBOSE);
    Serial.begin(115200);
    ESP_LOGI(LOG_TAG, "starting up");
    delay(2000);
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));
    ESP_LOGI(LOG_TAG, "bluetooth initialised");

    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());
    ESP_ERROR_CHECK(esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P9));
    ESP_LOGI(LOG_TAG, "bluedroid initialised");

    set_addr_from_key(rnd_addr, public_key);
    set_payload_from_key(adv_data, public_key);

    ESP_LOGI(LOG_TAG, "using device address: %02x %02x %02x %02x %02x %02x", rnd_addr[0], rnd_addr[1], rnd_addr[2], rnd_addr[3], rnd_addr[4], rnd_addr[5]);

    esp_err_t status;
    if ((status = esp_ble_gap_register_callback(esp_gap_cb)) != ESP_OK)
    {
        ESP_LOGE(LOG_TAG, "gap register error: %s", esp_err_to_name(status));
        return;
    }

    static uint8_t data_to_send[] = "hzgl";
    uint32_t current_message_id = 0;
    ESP_LOGI(LOG_TAG, "Sending initial default message: %s", data_to_send);
    send_data_once_blocking(data_to_send, sizeof(data_to_send), current_message_id);

    reset_advertising();
    ESP_LOGI(LOG_TAG, "application initialized");
}

void loop()
{
    ESP_LOGI(LOG_TAG, "still alive");

    static uint8_t data_to_send[] = "hzgl";
    uint32_t current_message_id = 0;
    send_data_once_blocking(data_to_send, sizeof(data_to_send), current_message_id);
    esp_ble_gap_stop_advertising();

    delay(5000);
}
