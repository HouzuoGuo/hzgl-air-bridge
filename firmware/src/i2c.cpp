#include <esp_log.h>
#include "i2c.h"
#include <Wire.h>

static const char LOG_TAG[] = __FILE__;

bool i2c_avail = false;

bool i2c_init()
{
    ESP_LOGI(LOG_TAG, "initialising i2c");
    // The OLED library insists on initialising the i2c bus for oled (Wire0 sda5 scl6) itself.
    /*
    if (!Wire1.begin(I2C_HOLE_SDA_GPIO, I2C_HOLE_SCL_GPIO, I2C_FREQUENCY_HZ))
    {
        // TODO FIXME: wire1 doesn't initialise successfully.
        ESP_LOGI(LOG_TAG, "failed to initialise i2c wire1");
    }
    */

    for (int dev_addr = 1; dev_addr < 127; dev_addr++)
    {
        Wire.beginTransmission(dev_addr);
        if (Wire.endTransmission() == 0)
        {
            ESP_LOGI(LOG_TAG, "found i2c device at address %02x on wire0", dev_addr);
        }
    }
    ESP_LOGI(LOG_TAG, "successfully initialised i2c");
    i2c_avail = true;
    return true;
}
