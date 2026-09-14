// Library index.
//
// Held in PSRAM, because 2048 entries at ~310 bytes is 600 kB and internal RAM
// is needed by the radio. Scanning happens once on mount and then on demand
// from the web app; it is NOT done on the audio task, since reading ID3 frames
// off the card can block for tens of milliseconds and that is an audible gap.
#include "storage/library.h"
#include "storage/sdcard.h"
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "esp_heap_caps.h"
#include "esp_log.h"

static const char *TAG = "lib";
static lib_entry_t *s_entries;
static uint16_t s_count;
static volatile bool s_scanning;
static volatile uint16_t s_progress;

uint16_t library_count(void) { return s_count; }
bool     library_scanning(void) { return s_scanning; }
uint16_t library_scan_progress(void) { return s_progress; }
const lib_entry_t *library_get(uint16_t i) { return i < s_count ? &s_entries[i] : NULL; }

int library_find(const char *path) {
    for (uint16_t i = 0; i < s_count; i++)
        if (!strcmp(s_entries[i].path, path)) return i;
    return -1;
}

static bool is_mp3(const char *name) {
    size_t n = strlen(name);
    return n > 4 && !strcasecmp(name + n - 4, ".mp3");
}

static void scan_dir(const char *dir, int depth) {
    if (depth > 4 || s_count >= LIB_MAX_TRACKS) return;   // stop runaway trees
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d)) && s_count < LIB_MAX_TRACKS) {
        if (e->d_name[0] == '.') continue;
        // A FAT long name can be 255 bytes; a path that would not fit gets
        // skipped rather than silently truncated into a file that won't open.
        char path[TRACK_PATH_LEN];
        int n = snprintf(path, sizeof path, "%s/%s", dir, e->d_name);
        if (n < 0 || n >= (int)sizeof path) continue;
        if (e->d_type == DT_DIR) {
            scan_dir(path, depth + 1);
            continue;
        }
        if (!is_mp3(e->d_name)) continue;
        lib_entry_t *t = &s_entries[s_count];
        memset(t, 0, sizeof *t);
        strlcpy(t->path, path, sizeof t->path);
        struct stat st;
        if (stat(path, &st) == 0) t->size = (uint32_t)st.st_size;
        if (id3_read(path, t->title, sizeof t->title,
                     t->artist, sizeof t->artist, &t->duration_s) != ESP_OK
            || t->title[0] == 0) {
            // fall back to the filename with the extension trimmed
            strlcpy(t->title, e->d_name, sizeof t->title);
            char *dot = strrchr(t->title, '.');
            if (dot) *dot = 0;
        }
        s_count++;
        s_progress = s_count;
    }
    closedir(d);
}

esp_err_t library_scan(void) {
    if (!sdcard_mounted()) return ESP_ERR_INVALID_STATE;
    if (s_scanning) return ESP_ERR_INVALID_STATE;
    if (!s_entries) {
        size_t bytes = sizeof(lib_entry_t) * LIB_MAX_TRACKS;
        s_entries = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!s_entries) {
            ESP_LOGE(TAG, "no PSRAM for the index (%u bytes)", (unsigned)bytes);
            return ESP_ERR_NO_MEM;
        }
    }
    s_scanning = true; s_count = 0; s_progress = 0;
    scan_dir(SD_MOUNT "/Music", 0);
    if (s_count == 0) scan_dir(SD_MOUNT, 0);      // tolerate a flat card
    s_scanning = false;
    ESP_LOGI(TAG, "indexed %u tracks", s_count);
    return ESP_OK;
}
