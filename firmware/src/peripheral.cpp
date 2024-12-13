#include <SSD1306Wire.h>
#include <Adafruit_BME280.h>
#include <esp_task_wdt.h>
#include <freertos/FreeRTOS.h>
#include "peripheral.h"

static const char LOG_TAG[] = __FILE__;
static bool i2c_avail = false, oled_avail = false, bme280_avail = false;

static SSD1306Wire oled(OLED_I2C_ADDR, -1, -1, GEOMETRY_128_64, I2C_ONE, I2C_FREQUENCY_HZ);
static Adafruit_BME280 bme280;

bool i2c_init()
{
    ESP_LOGI(LOG_TAG, "initialising i2c");
    if (!Wire.begin(I2C_SDA_GPIO, I2C_SCL_GPIO, I2C_FREQUENCY_HZ))
    {
        ESP_LOGI(LOG_TAG, "failed to initialise i2c");
        return false;
    }
    for (int dev_addr = 1; dev_addr < 127; dev_addr++)
    {
        Wire.beginTransmission(dev_addr);
        if (Wire.endTransmission() == 0)
        {
            ESP_LOGI(LOG_TAG, "found i2c device at address %02x", dev_addr);
        }
    }
    ESP_LOGI(LOG_TAG, "successfully initialised i2c");
    i2c_avail = true;
    return true;
}

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
        oled.display();
    next:
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(OLED_TASK_LOOP_INTERVAL_MILLIS));
    }
}

bool bme280_init()
{
    ESP_LOGI(LOG_TAG, "initialising bme280");
    if (!i2c_avail || !bme280.begin(BME280_I2C_ADDR))
    {
        ESP_LOGI(LOG_TAG, "failed to initialise bme280");
        return false;
    }

    ESP_LOGI(LOG_TAG, "successfully initialised bme280, initial temperature %.2fc, humidity %.2f%%, pressure %.2f hpa", bme280.readTemperature(), bme280.readHumidity(), bme280.readPressure() / 100);
    return true;
}

void bme280_task_fun(void *_)
{
    while (true)
    {
        if (!bme280_avail)
        {
            goto next;
        }
    next:
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(BME280_TASK_LOOP_INTERVAL_MILLIS));
    }
}