#include <supervisor.h>
#include <esp_sleep.h>
#include <esp_log.h>
#include <esp_task_wdt.h>
#include "peripheral.h"

static const char LOG_TAG[] = __FILE__;

static TaskHandle_t oled_task, bme280_task, supervisor_task;

void supervisor_setup()
{
    ESP_LOGI(LOG_TAG, "initialising supervisor");
    esp_sleep_wakeup_cause_t wakeup_reason;
    wakeup_reason = esp_sleep_get_wakeup_cause();
    switch (wakeup_reason)
    {
    case ESP_SLEEP_WAKEUP_UNDEFINED:
        ESP_LOGI(LOG_TAG, "wake-up reason is undefined");
        break;
    case ESP_SLEEP_WAKEUP_EXT0:
        ESP_LOGI(LOG_TAG, "wake-up from RTC_IO (EXT0)");
        break;
    case ESP_SLEEP_WAKEUP_EXT1:
        ESP_LOGI(LOG_TAG, "wake-up from RTC_CNT (EXT1)");
        break;
    case ESP_SLEEP_WAKEUP_TIMER:
        ESP_LOGI(LOG_TAG, "wake-up from timer");
        break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD:
        ESP_LOGI(LOG_TAG, "wake-up from touch pad");
        break;
    case ESP_SLEEP_WAKEUP_ULP:
        ESP_LOGI(LOG_TAG, "wake-up from ULP");
        break;
    case ESP_SLEEP_WAKEUP_GPIO:
        ESP_LOGI(LOG_TAG, "wake-up from GPIO");
        break;
    case ESP_SLEEP_WAKEUP_UART:
        ESP_LOGI(LOG_TAG, "wake-up from UART");
        break;
    default:
        ESP_LOGI(LOG_TAG, "wake-up reason is %d", wakeup_reason);
        break;
    }

    // A higher priority number means higher scheduling priority.
    unsigned long priority = tskIDLE_PRIORITY;
    xTaskCreatePinnedToCore(oled_task_loop, "oled_task_loop", 16 * 1024, NULL, priority++, &oled_task, 1);
    ESP_ERROR_CHECK(esp_task_wdt_add(oled_task));
    xTaskCreatePinnedToCore(bme280_task_loop, "bme280_task_loop", 16 * 1024, NULL, priority++, &bme280_task, 1);
    ESP_ERROR_CHECK(esp_task_wdt_add(bme280_task));
    xTaskCreatePinnedToCore(supervisor_task_loop, "supervisor_task_loop", 16 * 1024, NULL, priority++, &supervisor_task, 1);
    ESP_ERROR_CHECK(esp_task_wdt_add(supervisor_task));
    ESP_LOGI(LOG_TAG, "supervisor initialised successfully");
}

void supervisor_task_loop(void *_)
{
    while (true)
    {
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(SUPERVISOR_TASK_LOOP_DELAY_MILLIS));
    }
}