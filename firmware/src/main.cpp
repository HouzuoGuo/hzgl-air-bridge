// hzgl-air-bridge beacon firmware.

#include <Arduino.h>
#include <esp_bt.h>
#include <esp_bt_defs.h>
#include <esp_bt_main.h>
#include <esp_gap_ble_api.h>
#include <esp_gatt_defs.h>
#include <esp_task_wdt.h>
#include <esp_gattc_api.h>
#include <esp_log.h>
#include <esp_random.h>
#include <esp_sleep.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "supervisor.h"
#include "custom.h"
#include "bt.h"
#include "peripheral.h"
#include "crypt.h"
#include "uECC.h"
#include "bt.h"

// Unconditionally reboot the microcontroller at this interval.
const int UNCONDITIONAL_RESTART_MILLIS = 10 * 60 * 1000;
// Reset the main task watchdog at this interval.
const int WATCHDOG_RESET_INTERVAL_MILLIS = 5 * 1000;

static const char LOG_TAG[] = __FILE__;

void setup(void)
{
    // Monitor the arduino SDK main task (setup & loop) and reboot if it becomes stuck for 10 seconds.
    ESP_ERROR_CHECK(esp_task_wdt_init(10, true));
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));

    esp_log_level_set("*", ESP_LOG_VERBOSE);
    Serial.begin(115200);

    // Initialise hardware first and then start watchdog-protected concurrent tasks.
    i2c_init();
    oled_init();
    bme280_init();
    bt_init();
    supervisor_setup();
}

void loop()
{
    // The arduino SDK loop function only maintains the main task watchdog.
    // Bluetooth and peripheral functions are in separate tasks created by the supervisor.
    delay(WATCHDOG_RESET_INTERVAL_MILLIS);
    if (millis() < UNCONDITIONAL_RESTART_MILLIS)
    {
        // Reset watchdog timer prior to reaching the soft reset interval.
        esp_task_wdt_reset();
    }
    else
    {
        ESP_LOGW(LOG_TAG, "performing a routine restart to clear unpredictable faults");
        esp_restart();
    }
}
