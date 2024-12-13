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

#include "custom.h"
#include "bt.h"
#include "peripheral.h"
#include "crypt.h"
#include "uECC.h"
#include "bt.h"

static const char LOG_TAG[] = __FILE__;

void setup(void)
{
    esp_log_level_set("*", ESP_LOG_VERBOSE);
    Serial.begin(115200);
    ESP_LOGI(LOG_TAG, "hzgl-air-bridge is starting up");
    delay(2000);

    i2c_init();
    oled_init();
    bme280_init();
    bt_init();
}

void loop()
{
    oled_loop();
    bt_loop();
    bme280_loop();
    delay(10000);
}
