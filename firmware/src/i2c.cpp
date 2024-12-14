#include <SSD1306Wire.h>
#include <esp_log.h>
#include "i2c.h"

static const char LOG_TAG[] = __FILE__;

bool i2c_avail = false;

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
