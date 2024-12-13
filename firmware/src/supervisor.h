#pragma once

const int SUPERVISOR_TASK_LOOP_DELAY_MILLIS = 2000;

void supervisor_setup();
void supervisor_task_loop(void *_);