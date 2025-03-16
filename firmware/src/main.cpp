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
#include <esp_task_wdt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "custom.h"
#include "bt.h"
#include "i2c.h"
#include "bme280.h"
#include "oled.h"
#include "crypt.h"
#include "supervisor.h"
#include "uECC.h"
#include "bt.h"
#include "button.h"

static const char LOG_TAG[] = __FILE__;

void setup(void)
{
    // Monitor the arduino SDK main task and reboot automatically when it is stuck.
    esp_task_wdt_deinit();
    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = SUPERVISOR_WATCHDOG_TIMEOUT_SEC * 1000,
        .idle_core_mask = (1 << portNUM_PROCESSORS) - 1, // all processors
        .trigger_panic = true};
    ESP_ERROR_CHECK(esp_task_wdt_init(&wdt_config));
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));

    esp_log_level_set("*", ESP_LOG_VERBOSE);
    // Bluetooth doesn't work below 80 MHz, despite claims of support down to 40 MHz.
    setCpuFrequencyMhz(80);
    Serial.begin(115200);
    ESP_LOGI(LOG_TAG, "hzgl-air-bridge is starting up, xtal %d Mhz, cpu %d Mhz, apb %d Mhz",
             getXtalFrequencyMhz(), getCpuFrequencyMhz(), getApbFrequency() / 1000000);

    // Initialise hardware and peripherals in the correct order.
    button_init();
    oled_init();
    i2c_init();
    bme280_init();
    bt_init();
    // Start background tasks.
    supervisor_init();
    ESP_LOGI(LOG_TAG, "successfully initialised main task");
}

void loop()
{
    // Bluetooth and peripheral functions are in separate tasks created by the supervisor.
    // The arduino SDK main loop only maintains the watchdog.
    if (millis() < SUPERVISOR_UNCONDITIONAL_RESTART_MILLIS)
    {
        esp_task_wdt_reset();
    }
    else
    {
        ESP_LOGW(LOG_TAG, "performing a routine restart");
        esp_restart();
    }
    // Use arduino's delay instead of vTaskDelay to avoid a deadlock in arduino's loop function.
    delay(SUPERVISOR_TASK_LOOP_INTERVAL_MILLIS);
}
