#pragma once

/*
// TENSTAR ROBOT ESP32-C3 Development Board ESP32 C3 SuperMini WiFi Bluetooth (by TENSTAR ROBOT Store 1103035012, https://www.aliexpress.com/item/1005005967641936.html)
const int I2C_SDA_GPIO = 8;
const int I2C_SCL_GPIO = 9;
*/

// ESP32-C3 OLED development board ceramic antenna ESP32 supermini development board wifi Bluetooth 0.42-inch screen (by Wanzai Store 1103140424, https://www.aliexpress.com/item/1005007118546314.html)
const int I2C_OLED_SDA_GPIO = 5;
const int I2C_OLED_SCL_GPIO = 6;

const int I2C_HOLE_SDA_GPIO = 8;
const int I2C_HOLE_SCL_GPIO = 9;

const int I2C_FREQUENCY_HZ = 400000;

extern bool i2c_avail;

bool i2c_init();