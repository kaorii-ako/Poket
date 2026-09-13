#pragma once
#include "esp_err.h"
#include <stdbool.h>
#define SD_MOUNT "/sdcard"
esp_err_t sdcard_mount(void);
void      sdcard_unmount(void);
bool      sdcard_present(void);   // reads the socket's detect switch
bool      sdcard_mounted(void);
void      sdcard_usage(uint64_t *total, uint64_t *used);
