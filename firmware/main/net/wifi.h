// Radio bring-up for transfer mode.
//
// Wi-Fi and Bluetooth share one antenna and one PHY on the ESP32-S3, so this
// is deliberately modal: bringing the AP up stops A2DP and forces audio back
// to the jack. Trying to run both gives dropouts on the audio and timeouts on
// the page, which is worse than making the trade explicit.
#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>

// Starts a WPA2 AP named Poket-XXXX (last two MAC bytes) and brings up the
// HTTP server. Password is stored in NVS; on first boot one is generated and
// shown on the transfer screen.
esp_err_t wifi_transfer_start(void);
void      wifi_transfer_stop(void);
bool      wifi_transfer_active(void);

// Copies the live SSID / IP / password into the caller's buffers.
void      wifi_transfer_info(char *ssid, size_t sn, char *ip, size_t in,
                             char *pass, size_t pn);
