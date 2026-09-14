// Pairing and link management for the A2DP source.
//
// Poket is the *source*: it has to go and find the headphones and connect to
// them. That is the opposite of what a speaker does, and it is why "make the
// device discoverable" is not enough - nothing will ever connect to us.
//
// The flow, per ESP-IDF's own a2dp_source example:
//   inquiry -> filter on Class of Device (rendering service) -> read the name
//   out of the EIR -> esp_a2d_source_connect() -> once the link is up,
//   MEDIA_CTRL CHECK_SRC_RDY then MEDIA_CTRL_START.
// Skipping that last handshake gives a connected headset that plays silence.
#pragma once
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#define BT_NAME_LEN   32
#define BT_MAX_FOUND  12

typedef struct {
    uint8_t bda[6];
    char    name[BT_NAME_LEN];
    int8_t  rssi;
} bt_dev_t;

// Brings up the controller, Bluedroid and the A2DP source. Safe to call twice.
esp_err_t   bt_link_init(void);
void        bt_link_deinit(void);
bool        bt_link_up(void);

// Inquiry. Results accumulate in the list below; scanning stops on its own.
esp_err_t   bt_link_scan(uint8_t seconds);
void        bt_link_scan_stop(void);
bool        bt_link_scanning(void);
int         bt_link_count(void);
const bt_dev_t *bt_link_get(int i);

esp_err_t   bt_link_connect(const uint8_t bda[6]);
esp_err_t   bt_link_connect_index(int i);
void        bt_link_disconnect(void);
bool        bt_link_connected(void);
const char *bt_link_peer_name(void);

// The last headset that connected is remembered in NVS so the next boot can
// go straight back to it without making the user pick from a list again.
bool        bt_link_has_saved(void);
const char *bt_link_saved_name(void);
esp_err_t   bt_link_connect_saved(void);
void        bt_link_forget(void);
