#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <esp_task_wdt.h>
#include <esp_log.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertising.h>
#include <string.h>

#include "custom.h"
#include "crypt.h"
#include "bme280.h"
#include "bt.h"

#define IS_BIT_SET(var, pos) ((var) & (1 << (7 - pos)))

static const char LOG_TAG[] = __FILE__;

int bt_last_scan_millis = 0;
bool bt_scan_in_progress = false;
int bt_last_update_millis = 0;
int bt_ongoing_scan_count = 0;

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

// The beacon functions will reset the device address.
esp_bd_addr_t bt_dev_addr = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// Arduino BLE objects
static BLEAdvertising *pAdvertising = nullptr;
static BLEScan *pBLEScan = nullptr;

// Bluetooth scanning / advertising handled via Arduino BLE APIs.

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
    // Defer creation of scan/advertising objects until first use to avoid
    // triggering NimBLE host internals during early init.
    pBLEScan = nullptr;
    pAdvertising = nullptr;
    ESP_LOGI(LOG_TAG, "bluetooth initialised successfully");
}

void bt_task_work()
{
    // Scan for nearby bluetooth devices every BT_SCAN_INTERVAL_SEC for 5 seconds.
    int current_ms = (int)(esp_timer_get_time() / 1000ULL);
    if (bt_last_scan_millis <= 0 || current_ms - bt_last_scan_millis > BT_SCAN_INTERVAL_SEC * 1000)
    {
        // TODO FIXME: the first scan mistakenly counts reepated beacon transmissions without de-duplicating them.
        bt_start_scan_nearby_devices();
        bt_last_scan_millis = current_ms;
        return;
    }
    else if (current_ms - bt_last_scan_millis < BT_SCAN_DURATION_SEC * 1000)
    {
        // Wait for the scan to finish.
        return;
    }
    else if (current_ms - bt_last_scan_millis < BT_SCAN_DURATION_SEC * 1000 + (BT_TASK_LOOP_INTERVAL_MILLIS * 2))
    {
        // Stop scanning and give the callback a brief moment (100ms) to process the stop event.
        if (bt_scan_in_progress)
        {
            // Call the stop function explicitly to stop background scan task.
            if (pBLEScan)
            {
                pBLEScan->stop();
            }
            bt_scan_in_progress = false;
        }
        return;
    }
    // Beacon location and data.
    bt_update_beacon_iter();
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

void bt_set_phy_addr_and_advert_data()
{
    if (!pAdvertising)
    {
        pAdvertising = BLEDevice::getAdvertising();
    }
    if (pAdvertising)
    {
        pAdvertising->stop();
        BLEAdvertisementData adv;
        String manuf = String((const char *)bt_advert_data, sizeof(bt_advert_data));
        adv.setManufacturerData(manuf);
        pAdvertising->setAdvertisementData(adv);
        pAdvertising->start();
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
            bt_set_phy_addr_and_advert_data();
            vTaskDelay(pdMS_TO_TICKS(BT_BEACON_IX_MS));
        }
    }
    if (pAdvertising)
    {
        pAdvertising->stop();
    }
}

int bt_get_remaining_transmission_ms()
{
    if (bt_tx_message_value >= 0)
    {
        // The beacon is alternating between location and message beacons every iteration.
        int current_ms = (int)(esp_timer_get_time() / 1000ULL);
        return BT_TASK_LOOP_INTERVAL_MILLIS - (current_ms - bt_last_update_millis);
    }
    // The beacon stored a snapshot of telemetry data and will continuously beacon it for some minutes.
    {
        int current_ms = (int)(esp_timer_get_time() / 1000ULL);
        return BT_TX_ITER_DURATION_MILLIS - (current_ms - bt_last_update_millis);
    }
}

void bt_advance_tx_iter()
{
    // If there's a message to transmit, alternate between location and message beacons.
    if (bt_tx_message_value >= 0)
    {
        if (bt_tx_iter == BT_TX_ITER_MESSAGE)
        {
            bt_tx_iter = BT_TX_ITER_LOCATION;
        }
        else
        {
            bt_tx_iter = BT_TX_ITER_MESSAGE;
        }
    }
    else
    {
        // In the absence of a message, cycle through all location and telemetry data beacons.
        bt_tx_iter++;
        if (bt_tx_iter == BT_TX_ITER_MESSAGE)
        {
            // Skip the message beacon.
            bt_tx_iter++;
        }
        if (bt_tx_iter >= BT_TX_TOTAL_OPTIONS)
        {
            bt_tx_iter = 0;
        }
        // If the environment sensor is unavailable, skip their beacons.
        if (!bme280_avail && bt_tx_iter >= BT_TX_ITER_TEMP && bt_tx_iter <= BT_TX_ITER_PRESS)
        {
            while (bt_tx_iter >= BT_TX_ITER_TEMP && bt_tx_iter <= BT_TX_ITER_PRESS)
            {
                bt_tx_iter++;
            }
        }
    }
}

void bt_update_beacon_iter()
{
    // Update the beacon data and advance the iteration number at a regular interval.
    if (bt_last_update_millis <= 0 || bt_get_remaining_transmission_ms() <= 0)
    {
        bt_last_update_millis = (int)(esp_timer_get_time() / 1000ULL);
        memset(&bt_iter.data, 0, sizeof(bt_iter.data)); // to be populated by sensor data tx functions
        bt_iter.bme280.temp_celcius = bme280_latest.temp_celcius;
        bt_iter.bme280.humidity_percent = bme280_latest.humidity_percent;
        bt_iter.bme280.pressure_hpa = bme280_latest.pressure_hpa;
        bt_iter.bme280.altitude_masl = bme280_latest.altitude_masl;
        bt_iter.nearby_device_count = bt_nearby_device_count;
        bt_iter.message_value = bt_tx_message_value;
        bt_advance_tx_iter();
        ESP_LOGI(LOG_TAG, "update beacon iteration to %d", bt_tx_iter);
    }
}

void bt_send_location_once()
{
    ESP_LOGI(LOG_TAG, "beaconing location");
    bt_set_addr_from_key(bt_dev_addr, custom_public_key);
    bt_set_payload_from_key(bt_advert_data, custom_public_key);
    for (int i = 0; i < 2; i++)
    {
        bt_set_addr_from_key(bt_dev_addr, custom_public_key);
        bt_set_payload_from_key(bt_advert_data, custom_public_key);
        bt_set_phy_addr_and_advert_data();
        vTaskDelay(pdMS_TO_TICKS(BT_BEACON_IX_MS));
    }
    if (pAdvertising)
    {
        pAdvertising->stop();
    }
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
    ESP_LOGI(LOG_TAG, "scanning nearly devices");
    bt_scan_in_progress = true;
    bt_ongoing_scan_count = 0;
    if (!pBLEScan)
    {
        pBLEScan = BLEDevice::getScan();
    }
    pBLEScan->setActiveScan(true);
    BLEScanResults *results = pBLEScan->start(BT_SCAN_DURATION_SEC, false);
    bt_ongoing_scan_count = results->getCount();
    ESP_LOGI(LOG_TAG, "found %d nearly devices in total", bt_ongoing_scan_count);
    bt_nearby_device_count = bt_ongoing_scan_count;
    bt_ongoing_scan_count = 0;
    bt_scan_in_progress = false;
}

void bt_send_message()
{
    ESP_LOGI(LOG_TAG, "beacon round %d is sending message byte %d", bt_tx_iter, bt_iter.message_value);
    bt_iter.data[0] = bt_iter.message_value;
    bt_send_data_once_blocking(bt_iter.data, 1, (uint8_t)BT_TX_ITER_MESSAGE);
}