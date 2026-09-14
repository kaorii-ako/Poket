#include "storage/playlist.h"
#include "storage/library.h"
#include "storage/sdcard.h"
#include "esp_log.h"
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>

static const char *TAG = "playlist";
static playlist_t s_pl[PL_MAX];
static int        s_n;

bool playlist_name_ok(const char *name)
{
    if (!name) return false;
    size_t len = strlen(name);
    if (len == 0 || len >= PL_NAME_LEN - 4) return false;
    if (name[0] == '.' || name[0] == ' ') return false;
    if (strstr(name, "..")) return false;
    for (const char *p = name; *p; p++) {
        if (*p == '/' || *p == '\\' || *p == ':' || *p == '*' || *p == '?' ||
            *p == '"' || *p == '<' || *p == '>' || *p == '|')
            return false;
        if ((unsigned char)*p < 0x20) return false;
    }
    return true;
}

static void path_for(const char *name, char *out, size_t n)
{
    snprintf(out, n, "%s/%s.m3u", PL_DIR, name);
}

static uint16_t count_lines(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[TRACK_PATH_LEN];
    uint16_t n = 0;
    while (fgets(line, sizeof line, f))
        if (line[0] != '#' && line[0] != '\n' && line[0] != '\r') n++;
    fclose(f);
    return n;
}

esp_err_t playlist_scan(void)
{
    s_n = 0;
    if (!sdcard_mounted()) return ESP_ERR_INVALID_STATE;
    mkdir(PL_DIR, 0777);                       // first run: the folder is absent

    DIR *d = opendir(PL_DIR);
    if (!d) return ESP_FAIL;
    struct dirent *e;
    while ((e = readdir(d)) && s_n < PL_MAX) {
        if (e->d_name[0] == '.') continue;
        const char *dot = strrchr(e->d_name, '.');
        if (!dot || strcasecmp(dot, ".m3u") != 0) continue;
        playlist_t *p = &s_pl[s_n];
        size_t stem = (size_t)(dot - e->d_name);
        if (stem >= PL_NAME_LEN) stem = PL_NAME_LEN - 1;
        memcpy(p->name, e->d_name, stem);
        p->name[stem] = 0;
        char full[TRACK_PATH_LEN];
        int w = snprintf(full, sizeof full, "%s/%s", PL_DIR, e->d_name);
        p->count = (w > 0 && w < (int)sizeof full) ? count_lines(full) : 0;
        s_n++;
    }
    closedir(d);
    ESP_LOGI(TAG, "%d playlist(s)", s_n);
    return ESP_OK;
}

int playlist_count(void) { return s_n; }

const playlist_t *playlist_at(int i)
{
    return (i >= 0 && i < s_n) ? &s_pl[i] : NULL;
}

int playlist_find(const char *name)
{
    for (int i = 0; i < s_n; i++)
        if (strcasecmp(s_pl[i].name, name) == 0) return i;
    return -1;
}

int playlist_load(const char *name, uint16_t *out, int max)
{
    if (!playlist_name_ok(name)) return 0;
    char path[TRACK_PATH_LEN];
    path_for(name, path, sizeof path);
    FILE *f = fopen(path, "r");
    if (!f) return 0;

    char line[TRACK_PATH_LEN];
    int n = 0;
    while (n < max && fgets(line, sizeof line, f)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\n' || *p == '\r' || !*p) continue;
        p[strcspn(p, "\r\n")] = 0;
        // A track deleted off the card leaves a stale line behind; skip it
        // rather than refusing to load the whole playlist.
        int idx = library_find(p);
        if (idx >= 0) out[n++] = (uint16_t)idx;
    }
    fclose(f);
    return n;
}

esp_err_t playlist_create(const char *name)
{
    if (!playlist_name_ok(name)) return ESP_ERR_INVALID_ARG;
    if (playlist_find(name) >= 0) return ESP_ERR_INVALID_STATE;
    mkdir(PL_DIR, 0777);
    char path[TRACK_PATH_LEN];
    path_for(name, path, sizeof path);
    FILE *f = fopen(path, "w");
    if (!f) return ESP_FAIL;
    fprintf(f, "#EXTM3U\n");
    fclose(f);
    return playlist_scan();
}

esp_err_t playlist_delete(const char *name)
{
    if (!playlist_name_ok(name)) return ESP_ERR_INVALID_ARG;
    char path[TRACK_PATH_LEN];
    path_for(name, path, sizeof path);
    if (unlink(path) != 0) return ESP_ERR_NOT_FOUND;
    return playlist_scan();
}

esp_err_t playlist_add(const char *name, const char *track)
{
    if (!playlist_name_ok(name) || !track || !*track) return ESP_ERR_INVALID_ARG;
    if (library_find(track) < 0) return ESP_ERR_NOT_FOUND;
    char path[TRACK_PATH_LEN];
    path_for(name, path, sizeof path);
    FILE *f = fopen(path, "a");
    if (!f) return ESP_FAIL;
    fprintf(f, "%s\n", track);
    fclose(f);
    return playlist_scan();
}

// Rewrites via a temp file and renames, so an interrupted write cannot leave a
// half-truncated playlist where the original used to be.
esp_err_t playlist_set(const char *name, const char *const *paths, int n)
{
    if (!playlist_name_ok(name) || n > PL_MAX_TRACKS) return ESP_ERR_INVALID_ARG;
    mkdir(PL_DIR, 0777);
    char path[TRACK_PATH_LEN], tmp[TRACK_PATH_LEN];
    path_for(name, path, sizeof path);
    snprintf(tmp, sizeof tmp, "%s/.pl.tmp", PL_DIR);

    FILE *f = fopen(tmp, "w");
    if (!f) return ESP_FAIL;
    fprintf(f, "#EXTM3U\n");
    for (int i = 0; i < n; i++)
        if (paths[i] && *paths[i]) fprintf(f, "%s\n", paths[i]);
    fclose(f);

    unlink(path);
    if (rename(tmp, path) != 0) { unlink(tmp); return ESP_FAIL; }
    return playlist_scan();
}

esp_err_t playlist_remove_at(const char *name, int index)
{
    if (!playlist_name_ok(name) || index < 0) return ESP_ERR_INVALID_ARG;
    char path[TRACK_PATH_LEN], tmp[TRACK_PATH_LEN];
    path_for(name, path, sizeof path);
    snprintf(tmp, sizeof tmp, "%s/.pl.tmp", PL_DIR);

    FILE *in = fopen(path, "r");
    if (!in) return ESP_ERR_NOT_FOUND;
    FILE *out = fopen(tmp, "w");
    if (!out) { fclose(in); return ESP_FAIL; }

    // Streamed line by line on purpose: holding the whole playlist in a static
    // buffer cost 48 kB of DRAM, which is most of what this chip has.
    fprintf(out, "#EXTM3U\n");
    char line[TRACK_PATH_LEN];
    int seen = 0;
    while (fgets(line, sizeof line, in)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\n' || *p == '\r' || !*p) continue;
        p[strcspn(p, "\r\n")] = 0;
        if (seen++ == index) continue;               // the one being dropped
        fprintf(out, "%s\n", p);
    }
    fclose(in);
    fclose(out);

    if (index >= seen) { unlink(tmp); return ESP_ERR_NOT_FOUND; }
    unlink(path);
    if (rename(tmp, path) != 0) { unlink(tmp); return ESP_FAIL; }
    return playlist_scan();
}
