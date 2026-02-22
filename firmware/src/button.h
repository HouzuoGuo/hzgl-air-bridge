#pragma once

const int BUTTON_TASK_LOOP_INTERVAL_MILLIS = 100;
const int BUTTON_NO_INPUT_SLEEP_MILLIS = 30000;

extern int button_last_press_millis;

bool button_init();
void button_task_fun(void *_);
