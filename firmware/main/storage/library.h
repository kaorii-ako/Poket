#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "app/app_state.h"

#define LIB_MAX_TRACKS 2048

typedef struct {
    char     path[TRACK_PATH_LEN];
    char     title[TRACK_TITLE_LEN];
    char     artist[TRACK_ARTIST_LEN];
    uint32_t duration_s;
    uint32_t size;
} lib_entry_t;

esp_err_t        library_scan(void);          // walks /sdcard/Music
uint16_t         library_count(void);
const lib_entry_t *library_get(uint16_t i);
int              library_find(const char *path);
bool             library_scanning(void);
uint16_t         library_scan_progress(void);

// ID3v2 / ID3v1 title+artist, plus a duration estimate from the bitrate.
esp_err_t id3_read(const char *path, char *title, size_t tn,
                   char *artist, size_t an, uint32_t *duration_s);
