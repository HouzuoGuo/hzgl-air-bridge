#pragma once

#define BUTTON_TASK_LOOP_INTERVAL_MILLIS 50

bool button_init();
void button_task_fun(void *_);
