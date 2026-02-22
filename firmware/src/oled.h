#pragma once

const int OLED_I2C_ADDR = 0x3c;
const int OLED_FONT_HEIGHT_PX = 10;
const int OLED_TASK_LOOP_INTERVAL_MILLIS = 200;
const int OLED_SCROLL_INTERVAL_MILLIS = 1000;

// The OLED dimensions are tuned for "ESP32-C3 OLED development board ceramic antenna ESP32 supermini development board wifi Bluetooth 0.42-inch screen" (by Wanzai Store 1103140424, https://www.aliexpress.com/item/1005007118546314.html)
const int OLED_WIDTH_CHARS = 14;
const int OLED_HEIGHT_LINES = 4;
const int OLED_POWER_SAVE_BLINK_INTERVAL_MILLIS = 1000;

extern bool oled_avail;

bool oled_init();
void oled_render_status(char lines[OLED_HEIGHT_LINES][OLED_WIDTH_CHARS]);
void oled_task_fun(void *_);
