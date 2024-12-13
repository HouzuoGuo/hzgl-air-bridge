#include <Arduino.h>
#include <esp_log.h>
#include <string.h>

#include "custom.h"
#include "crypt.h"
#include "bt.h"

#define IS_BIT_SET(var, pos) ((var) & (1 << (7 - pos)))

static const char LOG_TAG[] = __FILE__;
static int loop_round = 0;

// The beacon functions will reset the device address.
esp_bd_addr_t bt_dev_addr = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

esp_ble_adv_params_t bt_advert_params = {
    // The advertisement intervals do not matter much, leave them at a second or two.
    // The beacon functions control the attempts and duty cycle.
    .adv_int_min = 0x0640, // 1600 * 0.625 = 1000 milliseconds
    .adv_int_max = 0x0C80, // 3200 * 0.625 = 2000 milliseconds
    .adv_type = ADV_TYPE_NONCONN_IND,
    .own_addr_type = BLE_ADDR_TYPE_RANDOM,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

uint8_t bt_advert_data[31] = {
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

void bt_esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    esp_err_t err;
    switch (event)
    {
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
        esp_ble_gap_start_advertising(&bt_advert_params);
        break;

    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        if ((err = param->adv_start_cmpl.status) != ESP_BT_STATUS_SUCCESS)
        {
            ESP_LOGE(LOG_TAG, "failed to start bt advertisement: %s", esp_err_to_name(err));
        }
        else
        {
            ESP_LOGI(LOG_TAG, "bt advertisement started");
        }
        break;

    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        if ((err = param->adv_stop_cmpl.status) != ESP_BT_STATUS_SUCCESS)
        {
            ESP_LOGE(LOG_TAG, "failed to stop bt advertisement: %s", esp_err_to_name(err));
        }
        else
        {
            ESP_LOGI(LOG_TAG, "bt advertisement stopped");
        }
        break;
    default:
        break;
    }
}

void bt_set_addr_from_key(esp_bd_addr_t addr, const uint8_t *public_key)
{
    addr[0] = public_key[0] | 0b11000000;
    addr[1] = public_key[1];
    addr[2] = public_key[2];
    addr[3] = public_key[3];
    addr[4] = public_key[4];
    addr[5] = public_key[5];
}

void bt_set_payload_from_key(uint8_t *payload, const uint8_t *public_key)
{
    /* copy last 22 bytes */
    memcpy(&payload[7], &public_key[6], 22);
    /* append two bits of public key */
    payload[29] = public_key[0] >> 6;
}

void bt_set_phy_addr_and_advert_data()
{
    esp_err_t status;
    esp_ble_gap_stop_advertising();
    if ((status = esp_ble_gap_set_rand_addr(bt_dev_addr)) != ESP_OK)
    {
        ESP_LOGE(LOG_TAG, "failed to set bt physical address: %s", esp_err_to_name(status));
        return;
    }
    if ((esp_ble_gap_config_adv_data_raw((uint8_t *)&bt_advert_data, sizeof(bt_advert_data))) != ESP_OK)
    {
        ESP_LOGE(LOG_TAG, "failed to set bt advertisement payload: %s", esp_err_to_name(status));
        return;
    }
    ESP_LOGI(LOG_TAG, "bt physical address: %02x %02x %02x %02x %02x %02x", bt_dev_addr[0], bt_dev_addr[1], bt_dev_addr[2], bt_dev_addr[3], bt_dev_addr[4], bt_dev_addr[5]);
}

void bt_set_addr_and_payload_for_bit(uint32_t index, uint32_t msg_id, uint8_t bit)
{
    // Packet format: [2byte magic] [4byte index] [4byte msg_id] [4byte modem_id] [zeros] [1byte bit value]
    uint32_t pubkey_gen_attempt = 0;
    static uint8_t public_key[28] = {0};
    public_key[0] = custom_pubkey_magic1;
    public_key[1] = custom_pubkey_magic2;
    crypt_copy_4b_big_endian(&public_key[2], &index);
    crypt_copy_4b_big_endian(&public_key[6], &msg_id);
    crypt_copy_4b_big_endian(&public_key[10], &custom_modem_id);
    public_key[27] = bit;
    do
    {
        crypt_copy_4b_big_endian(&public_key[14], &pubkey_gen_attempt);
        pubkey_gen_attempt++;
    } while (!crypt_is_valid_pubkey(public_key));
    ESP_LOGI(LOG_TAG, "data report beacon payload (pubkey gen attempt %d): %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x ... %02x", pubkey_gen_attempt, public_key[0], public_key[1], public_key[2], public_key[3], public_key[4], public_key[5], public_key[6], public_key[7], public_key[8], public_key[9], public_key[10], public_key[11], public_key[12], public_key[13], public_key[14], public_key[15], public_key[16], public_key[17], public_key[19], public_key[19], public_key[20], public_key[21], public_key[22], public_key[23], public_key[24], public_key[25], public_key[26], public_key[27]);
    bt_set_addr_from_key(bt_dev_addr, public_key);
    bt_set_payload_from_key(bt_advert_data, public_key);
}

void bt_send_data_once_blocking(uint8_t *data_to_send, uint32_t len, uint32_t msg_id)
{
    uint8_t current_bit = 0;
    for (int byte_index = 0; byte_index < len; byte_index++)
    {
        for (int bit_index = 0; bit_index < 8; bit_index++)
        {
            if (IS_BIT_SET(data_to_send[byte_index], bit_index))
            {
                current_bit = 1;
            }
            else
            {
                current_bit = 0;
            }
            ESP_LOGI(LOG_TAG, "beaconing data message id %d byte %d bit %d: %d", msg_id, byte_index, bit_index);
            bt_set_addr_and_payload_for_bit(byte_index * 8 + bit_index, msg_id, current_bit);
            bt_set_phy_addr_and_advert_data();
            delay(BT_BEACON_IX_MS);
        }
    }
    esp_ble_gap_stop_advertising();
}

void bt_send_location_once_blocking()
{
    ESP_LOGI(LOG_TAG, "beaconing location");
    bt_set_addr_from_key(bt_dev_addr, custom_public_key);
    bt_set_payload_from_key(bt_advert_data, custom_public_key);
    for (int i = 0; i < 2; i++)
    {
        bt_set_addr_from_key(bt_dev_addr, custom_public_key);
        bt_set_payload_from_key(bt_advert_data, custom_public_key);
        bt_set_phy_addr_and_advert_data();
        delay(BT_BEACON_IX_MS);
    }
    esp_ble_gap_stop_advertising();
}

void bt_init()
{
    ESP_LOGI(LOG_TAG, "initialising bluetooth");
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());
    ESP_ERROR_CHECK(esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P9));
    ESP_ERROR_CHECK(esp_ble_gap_register_callback(bt_esp_gap_cb));
    ESP_LOGI(LOG_TAG, "bluetooth initialised successfully");
}

void bt_loop()
{
    if (loop_round++ % 2 == 0)
    {
        ESP_LOGI(LOG_TAG, "beaconing data in round %d", loop_round);
        static uint8_t data_to_send[] = "hzgl";
        uint32_t current_message_id = 0;
        bt_send_data_once_blocking(data_to_send, sizeof(data_to_send), current_message_id);
    }
    else
    {
        ESP_LOGI(LOG_TAG, "beaconing location in round %d", loop_round);
        bt_send_location_once_blocking();
    }
}