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
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, I2C_OLED_SCL_GPIO, I2C_OLED_SDA_GPIO);

bool oled_init()
{
    ESP_LOGI(LOG_TAG, "initialising oled");
    if (!u8g2.begin())
    {
        ESP_LOGI(LOG_TAG, "failed to initialise oled");
        return false;
    }
    u8g2.setContrast(255);
    u8g2.setFont(u8g_font_helvR08);
    u8g2.clearBuffer();
    u8g2.drawStr(xOffset, yOffset, "abc");
    u8g2.sendBuffer();
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
        u8g2.clearBuffer();
        u8g2.drawStr(xOffset, yOffset + 10, "def");
        u8g2.drawStr(xOffset, yOffset + 20, "ghi");
        u8g2.sendBuffer();
    next:
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(OLED_TASK_LOOP_INTERVAL_MILLIS));
    }
}

void oled_render_status(char lines[OLED_HEIGHT_LINES][OLED_WIDTH_CHARS])
{
    snprintf(lines[0], OLED_WIDTH_CHARS, "Heap usage: %d/%dKB", (ESP.getHeapSize() - ESP.getFreeHeap()) / 1024, ESP.getHeapSize() / 1024);
    snprintf(lines[1], OLED_WIDTH_CHARS, "%.2fC %.2f%%RH", bme280_latest.temp_celcius, bme280_latest.humidity_percent);
    snprintf(lines[2], OLED_WIDTH_CHARS, "%.2f Mamsl %d btdev", bme280_latest.altitude_masl, bt_nearby_device_count);
    snprintf(lines[3], OLED_WIDTH_CHARS, "%.2fHpa", bme280_latest.pressure_hpa);
    snprintf(lines[4], OLED_WIDTH_CHARS, "Beacon iter #%d (%ds)", bt_iter.num, (BT_TX_ITER_DURATION_MILLIS - (millis() % BT_TX_ITER_DURATION_MILLIS)) / 1000);
    switch (bt_get_tx_iter_num())
    {
    case BT_TX_ITER_TEMP:
        snprintf(lines[5], OLED_WIDTH_CHARS, "Beaconing %.2fC", bt_iter.bme280.temp_celcius);
        break;
    case BT_TX_ITER_HUMID:
        snprintf(lines[5], OLED_WIDTH_CHARS, "Beaconing %.2f%%RH", bt_iter.bme280.humidity_percent);
        break;
    case BT_TX_ITER_PRESS:
        snprintf(lines[5], OLED_WIDTH_CHARS, "Beaconing %.2fHpa", bt_iter.bme280.pressure_hpa);
        break;
    case BT_TX_ITER_LOCATION:
        snprintf(lines[5], OLED_WIDTH_CHARS, "Beaconing location...");
        break;
    case BT_TX_ITER_DEVICE_COUNT:
        snprintf(lines[5], OLED_WIDTH_CHARS, "Beaconing %d btdev", bt_iter.nearby_device_count);
        break;
    default:
        break;
    }
}
