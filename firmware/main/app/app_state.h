// One struct describing everything the UI is allowed to look at.
//
// The UI never reaches into the player, the SD card or the radio. A single
// snapshot is published under a mutex and every screen renders from that, so
// a slow redraw can never tear against a decoder callback.
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#define TRACK_TITLE_LEN  64
#define TRACK_ARTIST_LEN 48
#define TRACK_PATH_LEN  192
#define BROWSE_ROWS       6

typedef enum {
    SCREEN_SPLASH = 0,
    SCREEN_NOW_PLAYING,
    SCREEN_BROWSER,
    SCREEN_MENU,
    SCREEN_TRANSFER,
} screen_t;

typedef enum {
    OUT_JACK = 0,       // PAM8908 -> 3.5 mm
    OUT_BLUETOOTH,      // A2DP source
} audio_out_t;

typedef enum {
    PLAY_STOPPED = 0,
    PLAY_PLAYING,
    PLAY_PAUSED,
} play_state_t;

typedef enum {
    REPEAT_OFF = 0,
    REPEAT_ALL,
    REPEAT_ONE,
} repeat_t;

typedef struct {
    char     title[TRACK_TITLE_LEN];
    char     artist[TRACK_ARTIST_LEN];
    char     path[TRACK_PATH_LEN];
    uint32_t duration_s;
    uint32_t bitrate_kbps;
    uint32_t sample_rate;
} track_t;

typedef struct {
    char    name[TRACK_TITLE_LEN];
    bool    is_dir;
} browse_row_t;

typedef struct {
    // playback
    play_state_t  play;
    track_t       track;
    uint32_t      elapsed_s;
    uint8_t       volume;          // 0..100
    bool          muted;
    repeat_t      repeat;
    bool          shuffle;
    audio_out_t   out;
    bool          jack_present;
    bool          bt_connected;
    char          bt_peer[32];
    uint16_t      queue_pos, queue_len;
    uint8_t       vu_left, vu_right;   // 0..15, smoothed, for themes that want it

    // library browser
    browse_row_t  rows[BROWSE_ROWS];
    uint8_t       row_count;
    uint8_t       row_sel;
    uint16_t      browse_total;
    uint16_t      browse_offset;
    char          browse_path[TRACK_PATH_LEN];

    // menu
    uint8_t       menu_sel;
    uint8_t       menu_count;

    // transfer mode
    bool          wifi_up;
    bool          wifi_is_ap;
    char          wifi_ssid[33];
    char          wifi_ip[16];
    uint8_t       clients;
    uint32_t      upload_pct;
    char          upload_name[TRACK_TITLE_LEN];

    // power
    uint8_t       batt_pct;
    uint16_t      batt_mv;
    int16_t       batt_ma;          // negative = discharging
    bool          charging;
    bool          card_present;

    // shell
    screen_t      screen;
    uint32_t      volume_hud_until_ms;
    bool          ready;
} app_state_t;

void              app_state_init(void);
// Take a consistent copy. Cheap: one mutex, one memcpy.
void              app_state_snapshot(app_state_t *out);
// Mutate under the lock. Pass a function so callers cannot hold the lock.
typedef void (*app_state_fn)(app_state_t *s, void *ctx);
void              app_state_update(app_state_fn fn, void *ctx);

// Convenience setters used from tasks that only touch one field. Pass an
// empty name with 0 percent to clear the transfer screen's progress line.
void              app_state_set_upload(const char *name, uint8_t pct);
void              app_state_set_clients(uint8_t n);
