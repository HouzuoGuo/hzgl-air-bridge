#include "button.h"
#include <Arduino.h>
#include <esp_task_wdt.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include "bt.h"

static const char LOG_TAG[] = __FILE__;

int button_last_down = 0;

bool button_init()
{
    ESP_LOGI(LOG_TAG, "initialising button");
    pinMode(BUTTON_GPIO, INPUT_PULLDOWN);
    ESP_LOGI(LOG_TAG, "button initialised successfully");
    return true;
}

void button_task_fun(void *_)
{
    while (true)
    {
        int button = digitalRead(BUTTON_GPIO);
        if (button == 0)
        {
            if (button_last_down > 0 && millis() - button_last_down > BUTTON_TASK_LOOP_INTERVAL_MILLIS + 2)
            {
                bt_tx_sched = BT_TX_SCHED_BUTTON_AND_LOCATION;
                button_last_down = 0;
                // The message comprises a single byte.
                bt_tx_message_value++;
                if (bt_tx_message_value > 255)
                {
                    bt_tx_message_value = 0;
                }
                ESP_LOGI(LOG_TAG, "setting transmission message to %d", bt_tx_message_value);
            }
        }
        else if (button_last_down == 0)
        {
            button_last_down = millis();
        }
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(BUTTON_TASK_LOOP_INTERVAL_MILLIS));
    }
}