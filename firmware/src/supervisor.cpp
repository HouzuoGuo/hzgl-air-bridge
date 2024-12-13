#include <esp_task_wdt.h>
#include <esp_sleep.h>
#include <esp_log.h>
#include <Esp.h>
#include "peripheral.h"
#include "supervisor.h"
#include "bt.h"

static const char LOG_TAG[] = __FILE__;
static TaskHandle_t bt_task, bme280_task, oled_task, supervisor_task;

void supervisor_init()
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

    // A numerically higher number enjoys higher runtime priority.
    unsigned long priority = tskIDLE_PRIORITY;
    xTaskCreate(bt_task_fun, "bt_task_loop", 16 * 1024, NULL, priority++, &bt_task);
    ESP_ERROR_CHECK(esp_task_wdt_add(bt_task));
    xTaskCreate(bme280_task_fun, "bme280_task_loop", 16 * 1024, NULL, priority++, &bme280_task);
    ESP_ERROR_CHECK(esp_task_wdt_add(bme280_task));
    xTaskCreate(oled_task_fun, "oled_task_loop", 16 * 1024, NULL, priority++, &oled_task);
    ESP_ERROR_CHECK(esp_task_wdt_add(oled_task));
    xTaskCreate(supervisor_task_fun, "supervisor_task_loop", 16 * 1024, NULL, priority++, &supervisor_task);
    ESP_ERROR_CHECK(esp_task_wdt_add(supervisor_task));
    ESP_LOGI(LOG_TAG, "successfully initialised supervisor");
}

void supervisor_task_fun(void *_)
{
    while (true)
    {
        supervisor_health_check();
    next:
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(SUPERVISOR_TASK_LOOP_INTERVAL_MILLIS));
    }
}

void supervisor_reboot()
{
    ESP_LOGE(LOG_TAG, "rebooting due to a critical malfunction");
    esp_restart();
}

void supervisor_health_check()
{
    uint32_t heap_min_free = ESP.getMinFreeHeap();
    ESP_LOGI(LOG_TAG, "heap usage: free - %dKB, min.free - %dKB, capacity - %dKB, maxalloc: %dKB, min.free stack: %dKB",
             ESP.getFreeHeap() / 1024, heap_min_free / 1024, ESP.getHeapSize() / 1024,
             ESP.getMaxAllocHeap() / 1024, uxTaskGetStackHighWaterMark(NULL) / 1024);
    UBaseType_t bluetooth_stack_free = uxTaskGetStackHighWaterMark(bt_task),
                bme280_stack_free = uxTaskGetStackHighWaterMark(bme280_task),
                oled_stack_free = uxTaskGetStackHighWaterMark(oled_task),
                supervisor_stack_free = uxTaskGetStackHighWaterMark(supervisor_task);
    if (heap_min_free < SUPERVISOR_FREE_MEM_REBOOT_THRESHOLD ||
        bluetooth_stack_free < SUPERVISOR_FREE_MEM_REBOOT_THRESHOLD ||
        bme280_stack_free < SUPERVISOR_FREE_MEM_REBOOT_THRESHOLD ||
        oled_stack_free < SUPERVISOR_FREE_MEM_REBOOT_THRESHOLD ||
        supervisor_stack_free < SUPERVISOR_FREE_MEM_REBOOT_THRESHOLD)
    {
        ESP_LOGE(LOG_TAG, "bluetooth task state: %d, min.free stack: %dKB", eTaskGetState(bt_task), bluetooth_stack_free);
        ESP_LOGE(LOG_TAG, "bme280 task state: %d, min.free stack: %dKB", eTaskGetState(bme280_task), bme280_stack_free);
        ESP_LOGE(LOG_TAG, "oled task state: %d, min.free stack: %dKB", eTaskGetState(oled_task), oled_stack_free);
        ESP_LOGE(LOG_TAG, "supervisor task state: %d, min.free stack: %dKB", eTaskGetState(supervisor_task), supervisor_stack_free);
        supervisor_reboot();
    }
}
