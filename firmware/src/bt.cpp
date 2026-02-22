#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <esp_task_wdt.h>
#include <esp_log.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertising.h>
#include <string.h>
#include <nimble/ble.h>
#include <host/ble_gap.h>
#include <host/ble_hs.h>

#include "custom.h"
#include "crypt.h"
#include "bme280.h"
#include "bt.h"

#define IS_BIT_SET(var, pos) ((var) & (1 << (7 - pos)))

static const char LOG_TAG[] = __FILE__;

int bt_last_scan_millis = 0;
int bt_last_update_millis = 0;

bt_iter_snapshot bt_iter = {
    .data = {0},
    .bme280 = {
        .temp_celcius = 0,
        .humidity_percent = 0,
        .pressure_hpa = 0,
        .altitude_masl = 0,
    },
    .nearby_device_count = 0,
    .message_value = 0,
};
int bt_tx_iter = -1;
int bt_tx_message_value = -1;
int bt_nearby_device_count = 0;

BLEAdvertising *bt_advert = nullptr;
BLEScan *bt_scan = nullptr;
BLEServer *bt_server = nullptr;
// The bluetooth mac address contains the first part of the public key (beacon data payload).
esp_bd_addr_t bt_dev_addr = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
// The manufacture data contains the rest of the public key (beacon data payload).
uint8_t bt_advert_data[31] = {
    0x1e, /* Length (30) */
    0xff, /* Manufacturer Specific Data (type 0xff) */
    0x4c,
    0x00, /* Company ID (Apple) */
    0x12,
    0x19, /* Offline Finding type and length */
    0x00, /* State */
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00, /* First two bits */
    0x00, /* Hint (0x00) */
};

void bt_init()
{
    ESP_LOGI(LOG_TAG, "initialising bluetooth");
    BLEDevice::init("");
    if ((bt_server = BLEDevice::createServer()) == nullptr)
    {
        ESP_LOGE(LOG_TAG, "failed to create BLE server");
    }
    if ((bt_advert = bt_server->getAdvertising()) == nullptr)
    {
        ESP_LOGE(LOG_TAG, "failed to get advertising object");
    }
    if ((bt_scan = BLEDevice::getScan()) == nullptr)
    {
        ESP_LOGE(LOG_TAG, "failed to get scan object");
    }
    // 9dBm appears to max out the ESP32 chip: https://docs.espressif.com/projects/esp-techpedia/en/latest/esp-friends/advanced-development/performance/modify-tx-power.html
    ESP_ERROR_CHECK(esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_SCAN, ESP_PWR_LVL_P9));
    ESP_ERROR_CHECK(esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P9));
    ESP_ERROR_CHECK(esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P9));
    ESP_LOGI(LOG_TAG, "bluetooth initialised successfully");
}

void bt_task_work()
{
    // Scan for nearby bluetooth devices every BT_SCAN_INTERVAL_SEC for 5 seconds.
    int current_ms = (int)(esp_timer_get_time() / 1000ULL);
    if (bt_last_scan_millis <= 0 || current_ms - bt_last_scan_millis > BT_SCAN_INTERVAL_SEC * 1000)
    {
        bt_start_scan_nearby_devices();
        bt_last_scan_millis = current_ms;
        return;
    }
    // Update the data packet for the data (not location) beacon.
    bt_update_data_packet();
    bt_advance_tx_iter();
    ESP_LOGI(LOG_TAG, "this tx iteration: %d", bt_tx_iter);
    switch (bt_tx_iter)
    {
    case BT_TX_ITER_TEMP:
        bt_send_data_bme280_temp();
        break;
    case BT_TX_ITER_HUMID:
        bt_send_data_bme280_humid();
        break;
    case BT_TX_ITER_PRESS:
        bt_send_data_bme280_press();
        break;
    case BT_TX_ITER_LOCATION:
        bt_send_location_once();
        break;
    case BT_TX_ITER_DEVICE_COUNT:
        bt_send_nearby_dev_count();
        break;
    case BT_TX_ITER_MESSAGE:
        bt_send_message();
        break;
    }
}

void bt_task_fun(void *_)
{
    while (true)
    {
        bt_task_work();
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(BT_TASK_LOOP_INTERVAL_MILLIS));
    }
}

void bt_set_addr_from_key(esp_bd_addr_t addr, const uint8_t *public_key)
{
    addr[0] = public_key[0] | 0b11000000; // static random address
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

void bt_transmit_beacon_data()
{
    ESP_LOGI(LOG_TAG, "beaconing addr: %02x %02x %02x %02x %02x %02x, payload: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
             bt_dev_addr[0], bt_dev_addr[1], bt_dev_addr[2], bt_dev_addr[3], bt_dev_addr[4], bt_dev_addr[5],
             bt_advert_data[0], bt_advert_data[1], bt_advert_data[2], bt_advert_data[3], bt_advert_data[4], bt_advert_data[5], bt_advert_data[6], bt_advert_data[7],
             bt_advert_data[8], bt_advert_data[9], bt_advert_data[10], bt_advert_data[11], bt_advert_data[12], bt_advert_data[13], bt_advert_data[14], bt_advert_data[15],
             bt_advert_data[16], bt_advert_data[17], bt_advert_data[18], bt_advert_data[19], bt_advert_data[20], bt_advert_data[21], bt_advert_data[22], bt_advert_data[23],
             bt_advert_data[24], bt_advert_data[25], bt_advert_data[26], bt_advert_data[27], bt_advert_data[28], bt_advert_data[29], bt_advert_data[30]);
    int rc = 0;
    // The device address contains a part of the public key (beacon payload).
    uint8_t rnd_addr[6];
    // NimBLE uses little endian for the device mac address.
    for (int i = 0; i < 6; ++i)
    {
        rnd_addr[i] = bt_dev_addr[5 - i];
    }
    if ((rc = ble_hs_id_set_rnd(rnd_addr)) != 0)
    {
        ESP_LOGE(LOG_TAG, "ble_hs_id_set_rnd failed: %d", rc);
        return;
    }
    // The manufacture data contains the rest of the public key (beacon payload).
    struct ble_hs_adv_fields fields;
    memset(&fields, 0, sizeof(fields));
    fields.mfg_data = (uint8_t *)&bt_advert_data[2];
    fields.mfg_data_len = sizeof(bt_advert_data) - 2;
    if ((rc = ble_gap_adv_set_fields(&fields)) != 0)
    {
        ESP_LOGE(LOG_TAG, "ble_gap_adv_set_fields failed: %d", rc);
        return;
    }
    // https://adamcatley.com/AirTag.html#software - "Advertising PDU type: Connectable undirected (ADV_IND)"
    struct ble_gap_adv_params adv_params;
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_NON;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    adv_params.itvl_min = 0x20;
    adv_params.itvl_max = 0x20;
    if ((rc = ble_gap_adv_start(BLE_OWN_ADDR_RANDOM, NULL, BT_BEACON_IX_MS, &adv_params, BLEAdvertising::handleGAPEvent, (void *)bt_advert)) != 0)
    {
        ESP_LOGE(LOG_TAG, "ble_gap_adv_start failed: %d", rc);
        return;
    }
    vTaskDelay(pdMS_TO_TICKS(BT_BEACON_IX_MS));
    if (((rc = ble_gap_adv_stop())) != 0)
    {
        ESP_LOGE(LOG_TAG, "ble_gap_adv_stop failed: %d", rc);
    }
}

void bt_set_addr_and_payload_for_bit(uint8_t index, uint8_t msg_id, uint8_t bit)
{
    // Packet format: [2byte magic] [4byte index] [4byte msg_id] [4byte modem_id] [zeros] [1byte bit value]
    uint8_t pubkey_gen_attempt = 0;
    static uint8_t data[28] = {0};
    // [0 - 24] = custom magic key
    memcpy(&data, &custom_magic_key, 24);
    // [24] - id of the message type
    data[24] = msg_id;
    // [25] - index of the bit
    data[25] = index;
    // [26] - bit value
    data[26] = bit;
    do
    {
        // [27] = public key generation attempt
        data[27] = pubkey_gen_attempt & 0xFF;
        pubkey_gen_attempt++;
    } while (!crypt_is_valid_pubkey(data));
    // ESP_LOGI(LOG_TAG, "beacon data (pubkey gen attempt %d): %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
    //  pubkey_gen_attempt, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7], data[8], data[9], data[10], data[11], data[12], data[13], data[14], data[15], data[16], data[17], data[19], data[19], data[20], data[21], data[22], data[23], data[24], data[25], data[26], data[27]);
    bt_set_addr_from_key(bt_dev_addr, data);
    bt_set_payload_from_key(bt_advert_data, data);
}

void bt_send_data_once_blocking(uint8_t *data_to_send, uint32_t len, uint8_t msg_id)
{
    for (uint8_t byte_index = 0; byte_index < len; byte_index++)
    {
        for (uint8_t bit_index = 0; bit_index < 8; bit_index++)
        {
            uint8_t bit_value = 0;
            if (IS_BIT_SET(data_to_send[byte_index], bit_index))
            {
                bit_value = 1;
            }
            ESP_LOGI(LOG_TAG, "beaconing data message id %d byte %d bit %d: %d", msg_id, byte_index, bit_index, bit_value);
            bt_set_addr_and_payload_for_bit(byte_index * 8 + bit_index, msg_id, bit_value);
            bt_transmit_beacon_data();
        }
    }
}

void bt_advance_tx_iter()
{
    switch (bt_tx_iter)
    {
    case BT_TX_ITER_TEMP:
        bt_tx_iter = BT_TX_ITER_HUMID;
        if (!bme280_avail)
        {
            bt_advance_tx_iter();
        }
        break;
    case BT_TX_ITER_HUMID:
        bt_tx_iter = BT_TX_ITER_PRESS;
        if (!bme280_avail)
        {
            bt_advance_tx_iter();
        }
        break;
    case BT_TX_ITER_PRESS:
        bt_tx_iter = BT_TX_ITER_LOCATION;
        break;
    case BT_TX_ITER_LOCATION:
        bt_tx_iter = BT_TX_ITER_DEVICE_COUNT;
        if (bt_nearby_device_count <= 0)
        {
            bt_advance_tx_iter();
        }
        break;
    case BT_TX_ITER_DEVICE_COUNT:
        bt_tx_iter = BT_TX_ITER_MESSAGE;
        if (bt_tx_message_value < 0)
        {
            bt_advance_tx_iter();
        }
        break;
    case BT_TX_ITER_MESSAGE:
        bt_tx_iter = BT_TX_ITER_TEMP;
        if (!bme280_avail)
        {
            bt_advance_tx_iter();
        }
        break;
    default:
        bt_tx_iter = BT_TX_ITER_TEMP;
        if (!bme280_avail)
        {
            bt_advance_tx_iter();
        }
        break;
    }
}

int bt_get_ms_till_data_refresh()
{
    int ret = BT_DATA_UPDATE_INTERVAL_MS - (millis() - bt_last_update_millis);
    if (ret < 0 || ret >= BT_DATA_UPDATE_INTERVAL_MS)
    {
        ret = 0;
    }
    return ret;
}

void bt_update_data_packet()
{
    // Freeze the sensor & message data snapshot for a couple of minutes, ensure their bits are picked up by Find My network.
    // Update the snapshot at regular interval.
    if (bt_get_ms_till_data_refresh() <= 0 ||
        (bt_iter.nearby_device_count == 0 && bt_nearby_device_count > 0) || // first BT scan completed
        bt_iter.message_value != bt_tx_message_value)                       // new message to send
    {
        bt_last_update_millis = millis();
        memset(&bt_iter.data, 0, sizeof(bt_iter.data)); // to be populated by sensor data tx functions
        bt_iter.bme280.temp_celcius = bme280_latest.temp_celcius;
        bt_iter.bme280.humidity_percent = bme280_latest.humidity_percent;
        bt_iter.bme280.pressure_hpa = bme280_latest.pressure_hpa;
        bt_iter.bme280.altitude_masl = bme280_latest.altitude_masl;
        bt_iter.nearby_device_count = bt_nearby_device_count;
        bt_iter.message_value = bt_tx_message_value;
        ESP_LOGI(LOG_TAG, "updated data packet: temp %.2f, humidity %.2f, pressure %.2f, nearby devices %d, message value %d",
                 bt_iter.bme280.temp_celcius, bt_iter.bme280.humidity_percent, bt_iter.bme280.pressure_hpa, bt_iter.nearby_device_count, bt_iter.message_value);
    }
}

void bt_send_location_once()
{
    ESP_LOGI(LOG_TAG, "beaconing location");
    bt_set_addr_from_key(bt_dev_addr, custom_public_key);
    bt_set_payload_from_key(bt_advert_data, custom_public_key);
    bt_transmit_beacon_data();
}

void bt_send_data_bme280_temp()
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
    ESP_LOGI(LOG_TAG, "beacon round %d is sending temperature byte %02x", bt_tx_iter, val);
    bt_iter.data[0] = val;
    bt_send_data_once_blocking(bt_iter.data, 1, (uint8_t)BT_TX_ITER_TEMP);
}

void bt_send_data_bme280_humid()
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
    ESP_LOGI(LOG_TAG, "beacon round %d is sending humidity byte %02x", bt_tx_iter, val);
    bt_iter.data[0] = val;
    bt_send_data_once_blocking(bt_iter.data, 1, (uint8_t)BT_TX_ITER_HUMID);
}

void bt_send_nearby_dev_count()
{
    int count = bt_iter.nearby_device_count;
    if (count > 255 * 2)
    {
        count = 255 * 2;
    }
    // In a busy office there can be upwards of 500 bluetooth devices nearby.
    ESP_LOGI(LOG_TAG, "beacon round %d is sending nearby devicce count byte %d", bt_tx_iter, count / 2);
    bt_iter.data[0] = count / 2;
    bt_send_data_once_blocking(bt_iter.data, 1, (uint8_t)BT_TX_ITER_DEVICE_COUNT);
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

void bt_send_data_bme280_press()
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
    ESP_LOGI(LOG_TAG, "beacon round %d is sending pressure bytes %02x %02x", bt_tx_iter, bt_iter.data[0], bt_iter.data[1]);
    bt_send_data_once_blocking(bt_iter.data, 2, (uint8_t)BT_TX_ITER_PRESS);
}

void bt_start_scan_nearby_devices()
{
    ESP_LOGI(LOG_TAG, "scanning neerby bluetooth devices");
    bt_scan->setActiveScan(true);
    // The scan operation blocks.
    BLEScanResults *results = bt_scan->start(BT_SCAN_DURATION_SEC, false);
    bt_nearby_device_count = results->getCount();
    bt_scan->stop();
    bt_scan->clearResults();
    ESP_LOGI(LOG_TAG, "found %d bluetooth devices nearby", bt_nearby_device_count);
}

void bt_send_message()
{
    ESP_LOGI(LOG_TAG, "beacon round %d is sending message byte %d", bt_tx_iter, bt_iter.message_value);
    bt_iter.data[0] = bt_iter.message_value;
    bt_send_data_once_blocking(bt_iter.data, 1, (uint8_t)BT_TX_ITER_MESSAGE);
}