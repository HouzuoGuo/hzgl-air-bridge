#include <esp_task_wdt.h>
#include <freertos/FreeRTOS.h>
#include <U8g2lib.h>
#include <string.h>
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
    next:
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(OLED_TASK_LOOP_INTERVAL_MILLIS));
    }
}

void oled_render_status(char lines[OLED_HEIGHT_LINES][OLED_WIDTH_CHARS])
{
    snprintf(lines[0], OLED_WIDTH_CHARS, "%.1f%% %.1fC", bme280_latest.humidity_percent, bme280_latest.temp_celcius);
    snprintf(lines[1], OLED_WIDTH_CHARS, "%.1fM %dBT", bme280_latest.altitude_masl, bt_nearby_device_count);
    snprintf(lines[2], OLED_WIDTH_CHARS, "%.2fHpa", bme280_latest.pressure_hpa);

    switch (oled_iter_counter++ % 3)
    {
    case 0:
        snprintf(lines[3], OLED_WIDTH_CHARS, "Mem %d/%dK", (ESP.getHeapSize() - ESP.getFreeHeap()) / 1024, ESP.getHeapSize() / 1024);
        break;
    case 1:
        snprintf(lines[3], OLED_WIDTH_CHARS, "BT TX %ds", bt_get_remaining_transmission_ms() / 1000);
        break;
    case 2:
        switch (bt_update_get_beacon_iter())
        {
        case BT_TX_ITER_TEMP:
            snprintf(lines[3], OLED_WIDTH_CHARS, "BT %.2fC", bt_iter.bme280.temp_celcius);
            break;
        case BT_TX_ITER_HUMID:
            snprintf(lines[3], OLED_WIDTH_CHARS, "BT %.2f%%H", bt_iter.bme280.humidity_percent);
            break;
        case BT_TX_ITER_PRESS:
            snprintf(lines[3], OLED_WIDTH_CHARS, "BT %.2fHpa", bt_iter.bme280.pressure_hpa);
            break;
        case BT_TX_ITER_LOCATION:
            snprintf(lines[3], OLED_WIDTH_CHARS, "BT location");
            break;
        case BT_TX_ITER_DEVICE_COUNT:
            snprintf(lines[3], OLED_WIDTH_CHARS, "BT %ddev", bt_iter.nearby_device_count);
            break;
        default:
            break;
        }
    }
}
