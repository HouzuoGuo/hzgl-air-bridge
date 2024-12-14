#pragma once

const int I2C_SDA_GPIO = 8; // TENSTAR ESP32 C3 SuperMini Development Board, GPIO 8 is SDA.
const int I2C_SCL_GPIO = 9; // TENSTAR ESP32 C3 SuperMini Development Board, GPIO 9 is SCL.
const int I2C_FREQUENCY_HZ = 100000;

extern bool i2c_avail;

bool i2c_init();