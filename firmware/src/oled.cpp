#include <SSD1306Wire.h>
#include <esp_task_wdt.h>
#include <freertos/FreeRTOS.h>
#include <string.h>
#include "i2c.h"
#include "bme280.h"
#include "bt.h"
#include "oled.h"

static const char LOG_TAG[] = __FILE__;
static SSD1306Wire oled(OLED_I2C_ADDR, -1, -1, GEOMETRY_128_64, I2C_ONE, I2C_FREQUENCY_HZ);

bool oled_avail = false;

bool oled_init()
{
    ESP_LOGI(LOG_TAG, "initialising oled");
    if (!i2c_avail || !oled.init())
    {
        ESP_LOGI(LOG_TAG, "failed to initialise oled");
        return false;
    }
    oled.clear();
    oled.setBrightness(64);
    oled.setContrast(0xF1, 128, 0x40);
    oled.resetOrientation();
    oled.flipScreenVertically();
    oled.setTextAlignment(TEXT_ALIGN_LEFT);
    oled.setFont(ArialMT_Plain_10);
    oled.displayOn();
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
        oled.drawStringMaxWidth(0, 0 * OLED_FONT_HEIGHT_PX, 200, "hi line 0");
        oled.drawStringMaxWidth(0, 1 * OLED_FONT_HEIGHT_PX, 200, "hi line 1");
        oled.drawStringMaxWidth(0, 2 * OLED_FONT_HEIGHT_PX, 200, "hi line 2");
        oled.drawStringMaxWidth(0, 3 * OLED_FONT_HEIGHT_PX, 200, "hi line 3");
        memset(lines, 0, sizeof(lines));
        oled_render_status(lines);
        oled.clear();
        for (int i = 0; i < OLED_HEIGHT_LINES; i++)
        {
            oled.drawStringMaxWidth(0, i * OLED_FONT_HEIGHT_PX, 200, lines[i]);
        }
        oled.display();
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
