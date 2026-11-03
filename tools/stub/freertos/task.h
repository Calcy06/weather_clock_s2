#pragma once

#include "freertos/FreeRTOS.h"

typedef void *TaskHandle_t;
typedef void (*TaskFunction_t)(void *);

void vTaskDelay(TickType_t ticks);
void vTaskDelete(TaskHandle_t task);
TickType_t xTaskGetTickCount(void);
BaseType_t xTaskCreate(TaskFunction_t fn, const char *name, uint32_t stack, void *arg,
                       UBaseType_t prio, TaskHandle_t *handle);
