#pragma once

static const int I2C_SDA_GPIO = 8; // TENSTAR ESP32 C3 SuperMini Development Board, GPIO 8 is SDA.
static const int I2C_SCL_GPIO = 9; // TENSTAR ESP32 C3 SuperMini Development Board, GPIO 9 is SCL.
static const int I2C_FREQUENCY_HZ = 100000;
bool i2c_init();

static const int BME280_I2C_ADDR = 0x76;
static const int BME280_TASK_LOOP_INTERVAL_MILLIS = 1000;
bool bme280_init();
void bme280_task_fun(void *_);

static const int OLED_I2C_ADDR = 0x3c;
static const int OLED_FONT_HEIGHT_PX = 10;
static const int OLED_TASK_LOOP_INTERVAL_MILLIS = 1000;
bool oled_init();
void oled_task_fun(void *_);