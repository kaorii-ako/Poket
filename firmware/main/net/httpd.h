// The web app, served off the device.
//
// One gzipped page out of rodata plus a small JSON API. Everything the page
// needs is baked in (web/build.py), because the AP has no route to the
// internet - a single external <script> would leave the UI blank.
#pragma once
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

esp_err_t poket_httpd_start(void);
void      poket_httpd_stop(void);
bool      poket_httpd_running(void);
// Clients currently associated, for the transfer screen.
uint8_t   poket_httpd_clients(void);
