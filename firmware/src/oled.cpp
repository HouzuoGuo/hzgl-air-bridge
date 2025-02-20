#include <esp_task_wdt.h>
#include <freertos/FreeRTOS.h>
#include <U8g2lib.h>
#include <esp_log.h>
#include <string.h>
#include "button.h"
#include "i2c.h"
#include "bme280.h"
#include "bt.h"
#include "oled.h"

static const char LOG_TAG[] = __FILE__;

bool oled_avail = false;
int oled_iter_counter = 0;
U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE, I2C_OLED_SCL_GPIO, I2C_OLED_SDA_GPIO);

bool oled_init()
{
    ESP_LOGI(LOG_TAG, "initialising oled");
    if (!oled.begin())
    {
        ESP_LOGI(LOG_TAG, "failed to initialise oled");
        return false;
    }
    oled.setFlipMode(1);
    oled.setContrast(255);
    oled.setFont(u8g_font_helvR08);
    oled.clear();
    oled.clearDisplay();
    oled.clearBuffer();
    ESP_LOGI(LOG_TAG, "oled initialised successfully");
    oled_avail = true;
    return true;
}

void oled_task_fun(void *_)
{
    char lines[OLED_HEIGHT_LINES][OLED_WIDTH_CHARS] = {0};
    while (true)
    {
        if (!oled_avail)
        {
            goto next;
        }
        oled_render_status(lines);
        oled.clearBuffer();
        for (int line = 0; line < OLED_HEIGHT_LINES; line++)
        {
            oled.drawStr(xOffset, yOffset + line * OLED_FONT_HEIGHT_PX, lines[line]);
        }
        oled.sendBuffer();
        if (millis() - button_last_press_millis > BUTTON_NO_INPUT_SLEEP_MILLIS)
        {
            // Too long without user input, start blinking the screen.
            if (millis() % 4000 < 1000)
            {
                oled.setPowerSave(0);
            }
            else
            {
                oled.setPowerSave(1);
            }
            goto next;
        } else {
            oled.setPowerSave(0);
        }
    next:
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(OLED_TASK_LOOP_INTERVAL_MILLIS));
    }
}

void oled_render_status(char lines[OLED_HEIGHT_LINES][OLED_WIDTH_CHARS])
{
    if (bt_tx_message_value >= 0)
    {
        snprintf(lines[0], OLED_WIDTH_CHARS, "BT msg %d", bt_tx_message_value);
        snprintf(lines[1], OLED_WIDTH_CHARS, "BT dev %d", bt_nearby_device_count);
    }
    else if (bme280_avail)
    {
        snprintf(lines[0], OLED_WIDTH_CHARS, "%.1f%% %.1fC", bme280_latest.humidity_percent, bme280_latest.temp_celcius);
        snprintf(lines[1], OLED_WIDTH_CHARS, "%.1fM %dBT", bme280_latest.altitude_masl, bt_nearby_device_count);
        snprintf(lines[2], OLED_WIDTH_CHARS, "%.2fHpa", bme280_latest.pressure_hpa);
    }
    else
    {
        snprintf(lines[1], OLED_WIDTH_CHARS, "BT dev %d", bt_nearby_device_count);
    }

    // Refresh the screen content faster than the scrolling interval of the status line.
    int remaining_tx_sec = bt_get_remaining_transmission_ms() / 1000;
    switch ((oled_iter_counter++ / (OLED_SCROLL_INTERVAL_MILLIS / OLED_TASK_LOOP_INTERVAL_MILLIS)) % 3)
    {
    case 0:
        snprintf(lines[3], OLED_WIDTH_CHARS, "Mem %d/%dK", (ESP.getHeapSize() - ESP.getFreeHeap()) / 1024, ESP.getHeapSize() / 1024);
        break;
    case 1:
        // fallthrough
    case 2:
        switch (bt_tx_iter)
        {
        case BT_TX_ITER_TEMP:
            snprintf(lines[3], OLED_WIDTH_CHARS, "B %.0fC %ds", bt_iter.bme280.temp_celcius, remaining_tx_sec);
            break;
        case BT_TX_ITER_HUMID:
            snprintf(lines[3], OLED_WIDTH_CHARS, "B %.0f%% %ds", bt_iter.bme280.humidity_percent, remaining_tx_sec);
            break;
        case BT_TX_ITER_PRESS:
            snprintf(lines[3], OLED_WIDTH_CHARS, "B %.0f %ds", bt_iter.bme280.pressure_hpa, remaining_tx_sec);
            break;
        case BT_TX_ITER_LOCATION:
            snprintf(lines[3], OLED_WIDTH_CHARS, "B LOC %ds", remaining_tx_sec);
            break;
        case BT_TX_ITER_DEVICE_COUNT:
            snprintf(lines[3], OLED_WIDTH_CHARS, "B BT%d %ds", bt_iter.nearby_device_count, remaining_tx_sec);
            break;
        case BT_TX_ITER_MESSAGE:
            snprintf(lines[3], OLED_WIDTH_CHARS, "B MSG%d %ds", bt_iter.message_value, remaining_tx_sec);
            break;
        default:
            snprintf(lines[3], OLED_WIDTH_CHARS, "BEACON: INIT");
            break;
        }
    }
}
