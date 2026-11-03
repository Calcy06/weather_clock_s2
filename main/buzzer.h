#pragma once

#include <stdbool.h>
#include <time.h>

void buzzer_init(void);
void buzzer_set(bool on);

/* 闹钟状态机，由渲染循环每秒调用一次 */
void alarm_tick(const struct tm *t);
bool alarm_ringing(void);
void alarm_stop(void);
