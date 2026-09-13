#pragma once
#include "esp_err.h"
#include "driver/i2c_master.h"
esp_err_t i2c_bus_init(void);
esp_err_t i2c_dev_add(uint8_t addr, uint32_t hz, i2c_master_dev_handle_t *out);
