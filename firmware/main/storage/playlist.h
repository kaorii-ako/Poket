// Playlists, stored as .m3u files in /sdcard/Music/Playlists.
//
// Deliberately not a private binary format: an .m3u is a list of paths, one per
// line, and every other music player on earth can read it. Pull the card out,
// drop it in a laptop, and the playlists are still there and still editable.
#pragma once
#include "esp_err.h"
#include "app/app_state.h"
#include <stdint.h>
#include <stdbool.h>

#define PL_DIR        "/sdcard/Music/Playlists"
#define PL_NAME_LEN   40
#define PL_MAX        24      // playlists on the card
#define PL_MAX_TRACKS 256     // entries in one playlist

typedef struct {
    char     name[PL_NAME_LEN];      // no .m3u, this is what the user typed
    uint16_t count;
} playlist_t;

// Refreshes the directory listing. Cheap; call it after any change.
esp_err_t   playlist_scan(void);
int         playlist_count(void);
const playlist_t *playlist_at(int i);
int         playlist_find(const char *name);

// Reads a playlist into library indices, skipping entries whose file is no
// longer on the card. Returns how many were resolved.
int         playlist_load(const char *name, uint16_t *out, int max);

esp_err_t   playlist_create(const char *name);
esp_err_t   playlist_delete(const char *name);
esp_err_t   playlist_add(const char *name, const char *track_path);
esp_err_t   playlist_remove_at(const char *name, int index);
// Replaces the whole file. Used for reordering.
esp_err_t   playlist_set(const char *name, const char *const *paths, int n);

// A playlist name becomes a filename, so it gets the same treatment as an
// uploaded file: no separators, no dot-files, no climbing out of the folder.
bool        playlist_name_ok(const char *name);
