// hzgl-air-bridge beacon firmware.

#include <esp_bt.h>
#include <esp_system.h>
#include <esp_timer.h>
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
#ifdef LEGACY_WATCHDOG_API
    ESP_ERROR_CHECK(esp_task_wdt_init(SUPERVISOR_WATCHDOG_TIMEOUT_SEC * 1000, true));
#else
    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = SUPERVISOR_WATCHDOG_TIMEOUT_SEC * 1000,
        .idle_core_mask = (1 << portNUM_PROCESSORS) - 1, // all processors
        .trigger_panic = true};
    ESP_ERROR_CHECK(esp_task_wdt_init(&wdt_config));
#endif
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));

    esp_log_level_set("*", ESP_LOG_VERBOSE);
    // Initialise logging and environment.
    ESP_LOGI(LOG_TAG, "hzgl-air-bridge is starting up");

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
    if ((esp_timer_get_time() / 1000ULL) < SUPERVISOR_UNCONDITIONAL_RESTART_MILLIS)
    {
        esp_task_wdt_reset();
    }
    else
    {
        ESP_LOGW(LOG_TAG, "performing a routine restart");
        esp_restart();
    }
    vTaskDelay(pdMS_TO_TICKS(SUPERVISOR_TASK_LOOP_INTERVAL_MILLIS));
}
