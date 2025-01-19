#include <Adafruit_BME280.h>
#include <esp_task_wdt.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include "i2c.h"
#include "bme280.h"

static const char LOG_TAG[] = __FILE__;
static Adafruit_BME280 bme280;

bool bme280_avail = false;
bme280_sample bme280_latest;

bool bme280_init()
{
    ESP_LOGI(LOG_TAG, "initialising bme280");
    if (!i2c_avail || !bme280.begin(BME280_I2C_ADDR))
    {
        ESP_LOGI(LOG_TAG, "failed to initialise bme280");
        return false;
    }
    bme280_avail = true;
    bme280_update();
    ESP_LOGI(LOG_TAG, "successfully initialised bme280");
    return true;
}

void bme280_update()
{
    if (!bme280_avail)
    {
        return;
    }
    bme280_latest.temp_celcius = bme280.readTemperature();
    bme280_latest.humidity_percent = bme280.readHumidity();
    bme280_latest.pressure_hpa = bme280.readPressure() / 100;
    bme280_latest.altitude_masl = bme280.readAltitude(1013.25);
}

void bme280_task_fun(void *_)
{
    while (true)
    {
        if (!bme280_avail)
        {
            goto next;
        }
        bme280_update();
        ESP_LOGI(LOG_TAG, "bme280 sample: temperature %.2fc, humidity %.2f%%, pressure %.2f hpa, alt %.2f mamsl", bme280_latest.temp_celcius, bme280_latest.humidity_percent, bme280_latest.pressure_hpa, bme280_latest.altitude_masl);
    next:
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(BME280_TASK_LOOP_INTERVAL_MILLIS));
    }
}