#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
typedef struct { uint8_t pct; uint16_t mv; int16_t ma; bool charging; bool gauge_ok; } batt_t;
esp_err_t battery_init(void);
void      battery_read(batt_t *out);
