#pragma once

const int BME280_I2C_ADDR = 0x76;
const int BME280_TASK_LOOP_INTERVAL_MILLIS = 1000;

typedef struct
{
    float temp_celcius;
    float humidity_percent;
    float pressure_hpa;
    float altitude_masl;
} bme280_sample;

extern bool bme280_avail;
extern bme280_sample bme280_latest;

bool bme280_init();
void bme280_update();
void bme280_task_fun(void *_);