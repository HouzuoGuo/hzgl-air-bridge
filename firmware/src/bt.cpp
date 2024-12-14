#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <esp_task_wdt.h>
#include <esp_log.h>
#include <string.h>

#include "custom.h"
#include "crypt.h"
#include "bme280.h"
#include "bt.h"

#define IS_BIT_SET(var, pos) ((var) & (1 << (7 - pos)))

static const char LOG_TAG[] = __FILE__;

uint8_t bt_iter_data[2] = {0};
bt_iter_snapshot bt_iter = {
    .num = -1,
    .data = {0},
    .bme280 = {
        .temp_celcius = 0,
        .humidity_percent = 0,
        .pressure_hpa = 0,
        .altitude_masl = 0,
    },
};

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

void bt_task_fun(void *_)
{
    while (true)
    {
        bt_update_iter_snapshot();
        // Alternative between location and data beacons.
        switch (bt_get_tx_iter_num())
        {
        case BT_TX_ITER_TEMP:
            bt_send_data_bme280_temp_blocking();
            break;
        case BT_TX_ITER_HUMID:
            bt_send_data_bme280_humid_blocking();
            break;
        case BT_TX_ITER_PRESS:
            bt_send_data_bme280_press_blocking();
            break;
        case BT_TX_ITER_LOCATION:
            bt_send_location_once_blocking();
            break;
        default:
            goto next;
        }
    next:
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(BT_TASK_LOOP_INTERVAL_MILLIS));
    }
}

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
    ESP_LOGI(LOG_TAG, "data report beacon payload (pubkey gen attempt %d): %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x ... %02x",
             pubkey_gen_attempt, public_key[0], public_key[1], public_key[2], public_key[3], public_key[4], public_key[5], public_key[6], public_key[7], public_key[8], public_key[9], public_key[10], public_key[11], public_key[12], public_key[13], public_key[14], public_key[15], public_key[16], public_key[17], public_key[19], public_key[19], public_key[20], public_key[21], public_key[22], public_key[23], public_key[24], public_key[25], public_key[26], public_key[27]);
    bt_set_addr_from_key(bt_dev_addr, public_key);
    bt_set_payload_from_key(bt_advert_data, public_key);
}

void bt_send_data_once_blocking(uint8_t *data_to_send, uint32_t len, uint32_t msg_id)
{
    for (int byte_index = 0; byte_index < len; byte_index++)
    {
        for (int bit_index = 0; bit_index < 8; bit_index++)
        {
            uint8_t bit_value = 0;
            if (IS_BIT_SET(data_to_send[byte_index], bit_index))
            {
                bit_value = 1;
            }
            ESP_LOGI(LOG_TAG, "beaconing data message id %d byte %d bit %d: %d", msg_id, byte_index, bit_index, bit_value);
            bt_set_addr_and_payload_for_bit(byte_index * 8 + bit_index, msg_id, bit_value);
            bt_set_phy_addr_and_advert_data();
            delay(BT_BEACON_IX_MS);
        }
    }
    esp_ble_gap_stop_advertising();
}

int bt_get_tx_iter_num()
{
    return (millis() / BT_TX_ITER_DURATION_MILLIS) % BT_TX_TOTAL_ITERS;
}

void bt_update_iter_snapshot()
{
    if (bt_get_tx_iter_num() != bt_iter.num)
    {
        bt_iter = {
            .num = bt_get_tx_iter_num(),
            .data = {0}, // to be populated by sensor data tx functions
            .bme280 = {
                .temp_celcius = bme280_latest.temp_celcius,
                .humidity_percent = bme280_latest.humidity_percent,
                .pressure_hpa = bme280_latest.pressure_hpa,
                .altitude_masl = bme280_latest.altitude_masl,
            },
        };
        memset(bt_iter_data, 0, sizeof(bt_iter_data));
    }
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

void bt_send_data_bme280_temp_blocking()
{
    // Encode the ambient temperature between [-40, +45] celcius, step 1/3rd of a celcius.
    float temp = bt_iter.bme280.temp_celcius;
    if (temp > 45)
    {
        temp = 45;
    }
    else if (temp < -40)
    {
        temp = -40;
    }
    // Offset by the lower boundary and then zoom in to fill up the entire byte.
    temp += 40;
    temp *= 3;
    uint8_t val = uint8_t(temp);
    ESP_LOGI(LOG_TAG, "beacon round %d is sending temperature byte %02x", bt_iter.num, val);
    bt_iter.data[0] = val;
    bt_send_data_once_blocking(bt_iter.data, 1, BT_TX_ITER_TEMP);
}

void bt_send_data_bme280_humid_blocking()
{
    // Encode the ambient relative humidity percent [0, 100], step 0.4%.
    float humid = bt_iter.bme280.humidity_percent;
    if (humid > 100)
    {
        humid = 100;
    }
    else if (humid < 0)
    {
        humid = 0;
    }
    // Zoom in to fill up the entire byte.
    humid *= 2.55;
    uint8_t val = uint8_t(humid);
    ESP_LOGI(LOG_TAG, "beacon round %d is sending humidity byte %02x", bt_iter.num, val);
    bt_iter.data[0] = val;
    bt_send_data_once_blocking(bt_iter.data, 1, BT_TX_ITER_HUMID);
}

typedef struct
{
    uint8_t bytes[2];
} two_be_bytes;

two_be_bytes data_be2(int val)
{
    two_be_bytes ret;
    // Big endian style.
    ret.bytes[0] = (val >> 8) & 0xFF;
    ret.bytes[1] = val & 0xFF;
    return ret;
}

void bt_send_data_bme280_press_blocking()
{
    // Encode the ambient pressure between 100 hpa (>15000m) and 1200 hpa (<-1000m)
    float press = bt_iter.bme280.pressure_hpa;
    if (press > 1200)
    {
        press = 1200;
    }
    else if (press < 100)
    {
        press = 100;
    }
    // Offset by the lower boundary and then zoom in to fill up the entire byte.
    press -= 100;
    press *= 65535.0 / (1200.0 - 100.0);
    two_be_bytes press_data = data_be2(int(press));
    bt_iter.data[0] = press_data.bytes[0];
    bt_iter.data[1] = press_data.bytes[1];
    ESP_LOGI(LOG_TAG, "beacon round %d is sending pressure bytes %02x %02x", bt_iter.num, bt_iter.data[0], bt_iter.data[1]);
    bt_send_data_once_blocking(bt_iter.data, 2, BT_TX_ITER_PRESS);
}
