#pragma once

const int OLED_I2C_ADDR = 0x3c;
const int OLED_FONT_HEIGHT_PX = 10;
const int OLED_TASK_LOOP_INTERVAL_MILLIS = 1000;

const int OLED_WIDTH_CHARS = 24;
const int OLED_HEIGHT_LINES = 6;

const unsigned int BufferWidth = 132;
const unsigned int BufferHeight = 64;
const unsigned int ScreenWidth = 72;
const unsigned int ScreenHeight = 40;
const unsigned int xOffset = (BufferWidth - ScreenWidth) / 2;
const unsigned int yOffset = (BufferHeight - ScreenHeight) / 2 + 10;

extern bool oled_avail;

bool oled_init();
void oled_render_status(char lines[OLED_HEIGHT_LINES][OLED_WIDTH_CHARS]);
void oled_task_fun(void *_);
