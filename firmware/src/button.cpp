#include "button.h"
#include <esp_timer.h>
#include <driver/gpio.h>
#include <esp_task_wdt.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include "bt.h"

static const char LOG_TAG[] = __FILE__;

int button_click_down = 0;
int button_last_press_millis = 0;

bool button_init()
{
    ESP_LOGI(LOG_TAG, "initialising button");
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
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
        int button = gpio_get_level(BUTTON_GPIO);
        if (button == 0)
        {

            int now_ms = (int)(esp_timer_get_time() / 1000ULL);
            if (button_click_down > 0 && now_ms - button_click_down > BUTTON_TASK_LOOP_INTERVAL_MILLIS + 2)
            {
                // Releasing a click.
                button_click_down = 0;
                button_inc_bt_tx_message();
                ESP_LOGI(LOG_TAG, "setting transmission message to %d", bt_tx_message_value);
            }
        }
        else if ((int)(esp_timer_get_time() / 1000ULL) - button_last_press_millis > BUTTON_NO_INPUT_SLEEP_MILLIS)
        {
            // Reset the timer and wake up the screen, don't considedr this a click.
            button_last_press_millis = (int)(esp_timer_get_time() / 1000ULL);
        }
        else if ((int)(esp_timer_get_time() / 1000ULL) - button_last_press_millis < BUTTON_TASK_LOOP_INTERVAL_MILLIS * 2)
        {
            // Ignore bounces for 200 milliseconds.
        }
        else if (button_click_down == 0)
        {
            // Register the down click.
            button_click_down = (int)(esp_timer_get_time() / 1000ULL);
            button_last_press_millis = (int)(esp_timer_get_time() / 1000ULL);
        }
        else if ((int)(esp_timer_get_time() / 1000ULL) - button_click_down > BUTTON_TASK_LOOP_INTERVAL_MILLIS * 5)
        {
            // Hold the click down to rapidly increase the message value.
            button_inc_bt_tx_message();
            button_last_press_millis = (int)(esp_timer_get_time() / 1000ULL);
        }
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(BUTTON_TASK_LOOP_INTERVAL_MILLIS));
    }
}