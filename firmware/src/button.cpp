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

void button_inc_bt_tx_message()
{

    // The message comprises a single byte.
    bt_tx_message_value++;
    if (bt_tx_message_value > 255)
    {
        bt_tx_message_value = 0;
    }
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
                button_last_down = 0;
                button_inc_bt_tx_message();
                ESP_LOGI(LOG_TAG, "setting transmission message to %d", bt_tx_message_value);
            }
        }
        else if (button_last_down == 0)
        {
            button_last_down = millis();
        }
        else if (millis() - button_last_down > BUTTON_TASK_LOOP_INTERVAL_MILLIS * 5)
        {
            // Hold the button down to rapidly increase the message byte value.
            button_inc_bt_tx_message();
        }
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(BUTTON_TASK_LOOP_INTERVAL_MILLIS));
    }
}