#include "net/httpd.h"
#include "net/www.h"
#include "app/app_state.h"
#include "audio/player.h"
#include "storage/library.h"
#include "storage/playlist.h"
#include "ui/theme.h"
#include "audio/bt_link.h"

#include "board.h"
#include "storage/sdcard.h"
#include "esp_http_server.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>

static const char *TAG = "httpd";
static httpd_handle_t s_server;
static uint8_t s_clients;

#define UPLOAD_DIR  "/sdcard/Music"
#define UPLOAD_CHUNK 4096

// ---- helpers ------------------------------------------------------------

static esp_err_t send_json(httpd_req_t *r, cJSON *root) {
    char *s = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!s) return httpd_resp_send_500(r);
    httpd_resp_set_type(r, "application/json");
    httpd_resp_set_hdr(r, "Cache-Control", "no-store");
    esp_err_t err = httpd_resp_send(r, s, strlen(s));
    cJSON_free(s);
    return err;
}

// Reads the whole request body. Bodies here are a couple of hundred bytes of
// JSON at most; anything bigger is an upload and goes through its own handler.
static char *read_body(httpd_req_t *r, size_t limit) {
    if (r->content_len == 0 || r->content_len > limit) return NULL;
    char *buf = malloc(r->content_len + 1);
    if (!buf) return NULL;
    size_t got = 0;
    while (got < r->content_len) {
        int n = httpd_req_recv(r, buf + got, r->content_len - got);
        if (n <= 0) { free(buf); return NULL; }
        got += n;
    }
    buf[got] = 0;
    return buf;
}

// ---- GET / --------------------------------------------------------------

static esp_err_t get_index(httpd_req_t *r) {
    httpd_resp_set_type(r, "text/html");
    httpd_resp_set_hdr(r, "Content-Encoding", "gzip");
    // The page is immutable for a given firmware build, but the API under it
    // is not, so only the shell is cacheable.
    httpd_resp_set_hdr(r, "Cache-Control", "public, max-age=86400");
    return httpd_resp_send(r, (const char *)www_index_gz, WWW_INDEX_LEN);
}

// ---- GET /api/state -----------------------------------------------------

static esp_err_t get_state(httpd_req_t *r) {
    app_state_t s;
    app_state_snapshot(&s);

    cJSON *root = cJSON_CreateObject();
    cJSON *now = cJSON_AddObjectToObject(root, "now");
    cJSON_AddStringToObject(now, "title", s.track.title);
    cJSON_AddStringToObject(now, "artist", s.track.artist);
    cJSON_AddNumberToObject(now, "elapsed", s.elapsed_s);
    cJSON_AddNumberToObject(now, "duration", s.track.duration_s);
    cJSON_AddNumberToObject(now, "bitrate", s.track.bitrate_kbps);
    cJSON_AddNumberToObject(now, "pos", s.queue_pos);
    cJSON_AddNumberToObject(now, "total", s.queue_len);
    cJSON_AddNumberToObject(now, "volume", s.muted ? 0 : s.volume);
    cJSON_AddNumberToObject(now, "batt", s.batt_pct);
    cJSON_AddBoolToObject(now, "charging", s.charging);
    cJSON_AddStringToObject(now, "out", s.out == OUT_BLUETOOTH ? "bt" : "jack");
    cJSON_AddBoolToObject(now, "shuffle", s.shuffle);
    cJSON_AddStringToObject(now, "repeat", s.repeat == REPEAT_ONE ? "one"
                                          : s.repeat == REPEAT_ALL ? "all" : "off");
    cJSON_AddStringToObject(now, "queue", player_queue_name());
    cJSON_AddStringToObject(now, "state",
        s.play == PLAY_PLAYING ? "playing" : s.play == PLAY_PAUSED ? "paused" : "stopped");

    cJSON *dev = cJSON_AddObjectToObject(root, "device");
    cJSON_AddStringToObject(dev, "name", "Poket");
    cJSON_AddStringToObject(dev, "fw", POKET_FW_VERSION);
    cJSON_AddStringToObject(dev, "ssid", s.wifi_ssid);
    cJSON_AddStringToObject(dev, "ip", s.wifi_ip);
    cJSON_AddStringToObject(dev, "bt", bt_link_connected() ? bt_link_peer_name()
                        : bt_link_has_saved() ? bt_link_saved_name() : "not paired");
    cJSON_AddBoolToObject(dev, "btLinked", bt_link_connected());
    cJSON_AddNumberToObject(dev, "clients", s.clients);

    // Card figures are bytes on the wire; the page formats them.
    uint64_t total = 0, used = 0;
    sdcard_usage(&total, &used);
    cJSON_AddNumberToObject(dev, "cardTotal", total / 1073741824.0);
    cJSON_AddNumberToObject(dev, "cardUsed", used / 1073741824.0);

    char up[16];
    uint32_t secs = esp_timer_get_time() / 1000000ULL;
    snprintf(up, sizeof up, "%02u:%02u:%02u", (unsigned)(secs / 3600),
             (unsigned)(secs / 60 % 60), (unsigned)(secs % 60));
    cJSON_AddStringToObject(dev, "uptime", up);

    const theme_t *t = theme_current();
    cJSON_AddStringToObject(root, "theme", t ? t->id : "minimal");
    return send_json(r, root);
}

// ---- GET /api/library ---------------------------------------------------

static esp_err_t get_library(httpd_req_t *r) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "scanning", library_scanning());
    cJSON *arr = cJSON_AddArrayToObject(root, "tracks");
    uint16_t n = library_count();
    for (uint16_t i = 0; i < n; i++) {
        const lib_entry_t *e = library_get(i);
        if (!e) break;
        cJSON *o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, "n", i + 1);
        cJSON_AddStringToObject(o, "title", e->title);
        cJSON_AddStringToObject(o, "artist", e->artist);
        cJSON_AddStringToObject(o, "path", e->path);
        cJSON_AddNumberToObject(o, "dur", e->duration_s);
        cJSON_AddNumberToObject(o, "size", e->size / 1048576.0);
        cJSON_AddItemToArray(arr, o);
    }
    return send_json(r, root);
}

// ---- GET /api/themes ----------------------------------------------------

static esp_err_t get_themes(httpd_req_t *r) {
    cJSON *root = cJSON_CreateObject();
    cJSON *arr = cJSON_AddArrayToObject(root, "themes");
    for (int i = 0; i < theme_count(); i++) {
        const theme_t *t = theme_at(i);
        if (!t) continue;
        cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "id", t->id);
        cJSON_AddStringToObject(o, "name", t->name);
        cJSON_AddBoolToObject(o, "custom", i >= THEME_BUILTIN_COUNT);
        cJSON_AddItemToArray(arr, o);
    }
    const theme_t *cur = theme_current();
    cJSON_AddStringToObject(root, "current", cur ? cur->id : "minimal");
    return send_json(r, root);
}

// ---- POST /api/transport ------------------------------------------------

static esp_err_t post_transport(httpd_req_t *r) {
    char *body = read_body(r, 512);
    if (!body) return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "body");
    cJSON *j = cJSON_Parse(body);
    free(body);
    if (!j) return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "json");

    const cJSON *action = cJSON_GetObjectItem(j, "action");
    const char *a = cJSON_IsString(action) ? action->valuestring : "";

    if (!strcmp(a, "play") || !strcmp(a, "pause")) {
        const cJSON *path = cJSON_GetObjectItem(j, "path");
        if (cJSON_IsString(path)) {
            int idx = library_find(path->valuestring);
            if (idx >= 0) player_play_index((uint16_t)idx);
            else ESP_LOGW(TAG, "play: %s not in library", path->valuestring);
        } else {
            player_toggle();
        }
    } else if (!strcmp(a, "next")) {
        player_next();
    } else if (!strcmp(a, "prev")) {
        player_prev();
    } else if (!strcmp(a, "stop")) {
        player_stop();
    } else if (!strcmp(a, "seek")) {
        const cJSON *v = cJSON_GetObjectItem(j, "value");
        if (cJSON_IsNumber(v)) player_seek((uint32_t)v->valuedouble);
    } else {
        cJSON_Delete(j);
        return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "action");
    }
    cJSON_Delete(j);
    httpd_resp_sendstr(r, "{\"ok\":true}");
    return ESP_OK;
}

// ---- POST /api/volume, /api/out, /api/theme -----------------------------

static esp_err_t post_volume(httpd_req_t *r) {
    char *body = read_body(r, 128);
    if (!body) return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "body");
    cJSON *j = cJSON_Parse(body);
    free(body);
    const cJSON *v = j ? cJSON_GetObjectItem(j, "value") : NULL;
    if (!cJSON_IsNumber(v)) { cJSON_Delete(j); return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "value"); }
    int val = (int)v->valuedouble;
    player_set_volume(val < 0 ? 0 : val > 100 ? 100 : (uint8_t)val);
    cJSON_Delete(j);
    httpd_resp_sendstr(r, "{\"ok\":true}");
    return ESP_OK;
}

static esp_err_t post_out(httpd_req_t *r) {
    char *body = read_body(r, 128);
    if (!body) return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "body");
    cJSON *j = cJSON_Parse(body);
    free(body);
    const cJSON *o = j ? cJSON_GetObjectItem(j, "out") : NULL;
    if (!cJSON_IsString(o)) { cJSON_Delete(j); return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "out"); }
    bool bt = !strcmp(o->valuestring, "bt");
    cJSON_Delete(j);
    if (bt && !bt_link_connected() && !bt_link_has_saved()) {
        // Nothing to send audio to yet. Say so plainly instead of switching the
        // output to a link that does not exist and going silent.
        httpd_resp_set_status(r, "409 Conflict");
        httpd_resp_set_type(r, "application/json");
        httpd_resp_sendstr(r, "{\"ok\":false,\"error\":\"No headphones linked yet - scan and connect one first\"}");
        return ESP_OK;
    }
    player_set_output(bt ? OUT_BLUETOOTH : OUT_JACK);
    httpd_resp_sendstr(r, "{\"ok\":true}");
    return ESP_OK;
}

// ---- Bluetooth pairing --------------------------------------------------
// Poket is the A2DP *source*, so it has to go and find the headphones: an
// inquiry, filtered to devices that advertise the audio "rendering" service,
// then a page and connect. Being discoverable would achieve nothing.

static void bda_str(const uint8_t *b, char *out, size_t n) {
    snprintf(out, n, "%02X:%02X:%02X:%02X:%02X:%02X",
             b[0], b[1], b[2], b[3], b[4], b[5]);
}

static esp_err_t get_bt(httpd_req_t *r) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "up", bt_link_up());
    cJSON_AddBoolToObject(root, "scanning", bt_link_scanning());
    cJSON_AddBoolToObject(root, "connected", bt_link_connected());
    cJSON_AddStringToObject(root, "peer", bt_link_peer_name());
    cJSON_AddBoolToObject(root, "saved", bt_link_has_saved());
    cJSON_AddStringToObject(root, "savedName", bt_link_saved_name());
    cJSON *arr = cJSON_AddArrayToObject(root, "devices");
    char s[20];
    for (int i = 0; i < bt_link_count(); i++) {
        const bt_dev_t *d = bt_link_get(i);
        if (!d) break;
        cJSON *o = cJSON_CreateObject();
        bda_str(d->bda, s, sizeof s);
        cJSON_AddStringToObject(o, "addr", s);
        cJSON_AddStringToObject(o, "name", d->name);
        cJSON_AddNumberToObject(o, "rssi", d->rssi);
        cJSON_AddItemToArray(arr, o);
    }
    return send_json(r, root);
}

static esp_err_t post_bt_scan(httpd_req_t *r) {
    esp_err_t err = bt_link_scan(8);
    if (err != ESP_OK)
        return httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, esp_err_to_name(err));
    httpd_resp_sendstr(r, "{\"ok\":true}");
    return ESP_OK;
}

static esp_err_t post_bt_connect(httpd_req_t *r) {
    char *body = read_body(r, 128);
    if (!body) return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "body");
    cJSON *j = cJSON_Parse(body);
    free(body);
    if (!j) return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "json");
    const cJSON *idx = cJSON_GetObjectItem(j, "index");
    esp_err_t err;
    if (cJSON_IsNumber(idx)) {
        err = bt_link_connect_index((int)idx->valuedouble);
    } else {
        err = bt_link_connect_saved();
    }
    cJSON_Delete(j);
    if (err != ESP_OK)
        return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, esp_err_to_name(err));
    httpd_resp_sendstr(r, "{\"ok\":true}");
    return ESP_OK;
}

static esp_err_t post_bt_forget(httpd_req_t *r) {
    bt_link_disconnect();
    bt_link_forget();
    httpd_resp_sendstr(r, "{\"ok\":true}");
    return ESP_OK;
}

static esp_err_t post_theme(httpd_req_t *r) {
    char *body = read_body(r, 128);
    if (!body) return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "body");
    cJSON *j = cJSON_Parse(body);
    free(body);
    const cJSON *id = j ? cJSON_GetObjectItem(j, "id") : NULL;
    if (!cJSON_IsString(id)) { cJSON_Delete(j); return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "id"); }
    esp_err_t err = theme_set(id->valuestring);
    cJSON_Delete(j);
    if (err != ESP_OK) return httpd_resp_send_err(r, HTTPD_404_NOT_FOUND, "no such theme");
    httpd_resp_sendstr(r, "{\"ok\":true}");
    return ESP_OK;
}

// ---- POST /api/upload ---------------------------------------------------

// The browser percent-encodes the file name, so it is decoded before any of
// the checks below run - otherwise an encoded "%2e%2e%2f" walks straight past
// them. Decodes in place; the result is never longer than the input.
static void url_decode(char *s) {
    char *w = s;
    for (const char *r = s; *r; r++) {
        if (*r == '%' && isxdigit((unsigned char)r[1]) && isxdigit((unsigned char)r[2])) {
            char hex[3] = { r[1], r[2], 0 };
            *w++ = (char)strtol(hex, NULL, 16);
            r += 2;
        } else if (*r == '+') {
            *w++ = ' ';
        } else {
            *w++ = *r;
        }
    }
    *w = 0;
}

// Rejects anything that could climb out of /sdcard/Music. The name comes
// from a browser, so it is attacker-controlled as far as this is concerned.
static bool safe_name(const char *n) {
    size_t len = strlen(n);
    if (len == 0 || len > 96) return false;
    if (strstr(n, "..") || strchr(n, '/') || strchr(n, '\\')) return false;
    if (n[0] == '.') return false;
    const char *dot = strrchr(n, '.');
    return dot && (!strcasecmp(dot, ".mp3"));
}

static esp_err_t post_upload(httpd_req_t *r) {
    char query[160], name[112];
    if (httpd_req_get_url_query_str(r, query, sizeof query) != ESP_OK ||
        httpd_query_key_value(query, "name", name, sizeof name) != ESP_OK)
        return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "name");

    url_decode(name);
    if (!safe_name(name))
        return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "bad name");

    char path[TRACK_PATH_LEN];
    snprintf(path, sizeof path, "%s/%s", UPLOAD_DIR, name);
    // Write to a temp name and rename at the end, so an interrupted upload
    // never leaves a half file that the scanner would list as a track.
    char tmp[TRACK_PATH_LEN];
    snprintf(tmp, sizeof tmp, "%s/.part", UPLOAD_DIR);

    FILE *f = fopen(tmp, "wb");
    if (!f) return httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "open");

    char *buf = malloc(UPLOAD_CHUNK);
    if (!buf) { fclose(f); unlink(tmp); return httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "mem"); }

    int remaining = r->content_len, total = r->content_len;
    while (remaining > 0) {
        int n = httpd_req_recv(r, buf, remaining < UPLOAD_CHUNK ? remaining : UPLOAD_CHUNK);
        if (n == HTTPD_SOCK_ERR_TIMEOUT) continue;
        if (n <= 0) break;
        if (fwrite(buf, 1, n, f) != (size_t)n) { remaining = -1; break; }
        remaining -= n;
        app_state_set_upload(name, total ? (total - remaining) * 100 / total : 100);
    }
    free(buf);
    fclose(f);

    if (remaining != 0) {
        unlink(tmp);
        app_state_set_upload("", 0);
        return httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "write");
    }
    unlink(path);                       // overwrite an existing track
    if (rename(tmp, path) != 0) {
        unlink(tmp);
        app_state_set_upload("", 0);
        return httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "rename");
    }
    app_state_set_upload("", 0);
    ESP_LOGI(TAG, "stored %s (%d bytes)", path, total);
    library_scan();                     // pick the new file up
    httpd_resp_sendstr(r, "{\"ok\":true}");
    return ESP_OK;
}

// ---- POST /api/theme/custom ---------------------------------------------

static esp_err_t post_theme_pack(httpd_req_t *r) {
    if (r->content_len == 0 || r->content_len > 16384)
        return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "size");
    uint8_t *pack = malloc(r->content_len);
    if (!pack) return httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "mem");
    size_t got = 0;
    while (got < r->content_len) {
        int n = httpd_req_recv(r, (char *)pack + got, r->content_len - got);
        if (n <= 0) { free(pack); return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "recv"); }
        got += n;
    }
    esp_err_t err = theme_custom_load(pack, got);   // validates magic + bounds
    free(pack);
    if (err != ESP_OK) return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "bad pack");
    httpd_resp_sendstr(r, "{\"ok\":true}");
    return ESP_OK;
}


// ---- playlists -----------------------------------------------------------
// Stored as .m3u on the card, so they survive being read on a laptop.

static esp_err_t get_playlists(httpd_req_t *r) {
    playlist_scan();
    cJSON *root = cJSON_CreateObject();
    cJSON *arr = cJSON_AddArrayToObject(root, "playlists");
    for (int i = 0; i < playlist_count(); i++) {
        const playlist_t *pl = playlist_at(i);
        if (!pl) break;
        cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "name", pl->name);
        cJSON_AddNumberToObject(o, "count", pl->count);
        cJSON_AddItemToArray(arr, o);
    }
    cJSON_AddStringToObject(root, "playing", player_queue_name());
    return send_json(r, root);
}

// GET /api/playlist?name=Foo -> the tracks in it, resolved against the library
static esp_err_t get_playlist(httpd_req_t *r) {
    char query[160], name[PL_NAME_LEN];
    if (httpd_req_get_url_query_str(r, query, sizeof query) != ESP_OK ||
        httpd_query_key_value(query, "name", name, sizeof name) != ESP_OK)
        return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "name");
    url_decode(name);
    if (!playlist_name_ok(name))
        return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "bad name");

    static uint16_t idx[PL_MAX_TRACKS];
    int n = playlist_load(name, idx, PL_MAX_TRACKS);
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "name", name);
    cJSON *arr = cJSON_AddArrayToObject(root, "tracks");
    for (int i = 0; i < n; i++) {
        const lib_entry_t *e = library_get(idx[i]);
        if (!e) continue;
        cJSON *o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, "n", idx[i] + 1);
        cJSON_AddStringToObject(o, "title", e->title);
        cJSON_AddStringToObject(o, "artist", e->artist);
        cJSON_AddStringToObject(o, "path", e->path);
        cJSON_AddNumberToObject(o, "dur", e->duration_s);
        cJSON_AddNumberToObject(o, "size", e->size / 1048576.0);
        cJSON_AddItemToArray(arr, o);
    }
    return send_json(r, root);
}

static esp_err_t post_playlist(httpd_req_t *r) {
    char *body = read_body(r, 1024);
    if (!body) return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "body");
    cJSON *j = cJSON_Parse(body);
    free(body);
    if (!j) return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "json");

    const cJSON *act  = cJSON_GetObjectItem(j, "action");
    const cJSON *name = cJSON_GetObjectItem(j, "name");
    const char *a = cJSON_IsString(act) ? act->valuestring : "";
    const char *nm = cJSON_IsString(name) ? name->valuestring : "";
    esp_err_t err = ESP_ERR_INVALID_ARG;

    if (!strcmp(a, "create")) {
        err = playlist_create(nm);
    } else if (!strcmp(a, "delete")) {
        err = playlist_delete(nm);
    } else if (!strcmp(a, "add")) {
        const cJSON *path = cJSON_GetObjectItem(j, "path");
        if (cJSON_IsString(path)) err = playlist_add(nm, path->valuestring);
    } else if (!strcmp(a, "removeAt")) {
        const cJSON *at = cJSON_GetObjectItem(j, "index");
        if (cJSON_IsNumber(at)) err = playlist_remove_at(nm, (int)at->valuedouble);
    } else if (!strcmp(a, "play")) {
        static uint16_t idx[PL_MAX_TRACKS];
        int n = playlist_load(nm, idx, PL_MAX_TRACKS);
        const cJSON *st = cJSON_GetObjectItem(j, "start");
        if (n <= 0) err = ESP_ERR_NOT_FOUND;
        else err = player_play_queue(nm, idx, n,
                                     cJSON_IsNumber(st) ? (int)st->valuedouble : 0);
    }
    cJSON_Delete(j);
    if (err != ESP_OK)
        return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, esp_err_to_name(err));
    httpd_resp_sendstr(r, "{\"ok\":true}");
    return ESP_OK;
}

// ---- shuffle / repeat ----------------------------------------------------

static esp_err_t post_mode(httpd_req_t *r) {
    char *body = read_body(r, 128);
    if (!body) return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "body");
    cJSON *j = cJSON_Parse(body);
    free(body);
    if (!j) return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "json");
    const cJSON *sh = cJSON_GetObjectItem(j, "shuffle");
    const cJSON *rp = cJSON_GetObjectItem(j, "repeat");
    if (cJSON_IsBool(sh)) player_set_shuffle(cJSON_IsTrue(sh));
    if (cJSON_IsString(rp)) {
        const char *v = rp->valuestring;
        player_set_repeat(!strcmp(v, "one") ? REPEAT_ONE
                        : !strcmp(v, "all") ? REPEAT_ALL : REPEAT_OFF);
    }
    cJSON_Delete(j);
    httpd_resp_sendstr(r, "{\"ok\":true}");
    return ESP_OK;
}

// ---- delete a track ------------------------------------------------------

static esp_err_t post_delete(httpd_req_t *r) {
    char *body = read_body(r, 512);
    if (!body) return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "body");
    cJSON *j = cJSON_Parse(body);
    free(body);
    const cJSON *path = j ? cJSON_GetObjectItem(j, "path") : NULL;
    if (!cJSON_IsString(path)) { cJSON_Delete(j); return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "path"); }
    // Only ever delete inside the music folder, whatever the caller says.
    if (strncmp(path->valuestring, "/sdcard/Music/", 14) != 0 ||
            strstr(path->valuestring, "..")) {
        cJSON_Delete(j);
        return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "outside /Music");
    }
    int rc = unlink(path->valuestring);
    cJSON_Delete(j);
    if (rc != 0) return httpd_resp_send_err(r, HTTPD_404_NOT_FOUND, "no such track");
    library_scan();
    httpd_resp_sendstr(r, "{\"ok\":true}");
    return ESP_OK;
}

// ---- wiring -------------------------------------------------------------

static const httpd_uri_t k_routes[] = {
    { .uri = "/",                  .method = HTTP_GET,  .handler = get_index },
    { .uri = "/api/state",         .method = HTTP_GET,  .handler = get_state },
    { .uri = "/api/library",       .method = HTTP_GET,  .handler = get_library },
    { .uri = "/api/themes",        .method = HTTP_GET,  .handler = get_themes },
    { .uri = "/api/transport",     .method = HTTP_POST, .handler = post_transport },
    { .uri = "/api/volume",        .method = HTTP_POST, .handler = post_volume },
    { .uri = "/api/out",           .method = HTTP_POST, .handler = post_out },
    { .uri = "/api/theme",         .method = HTTP_POST, .handler = post_theme },
    { .uri = "/api/theme/custom",  .method = HTTP_POST, .handler = post_theme_pack },
    { .uri = "/api/upload",        .method = HTTP_POST, .handler = post_upload },
    { .uri = "/api/bt",            .method = HTTP_GET,  .handler = get_bt },
    { .uri = "/api/bt/scan",       .method = HTTP_POST, .handler = post_bt_scan },
    { .uri = "/api/bt/connect",    .method = HTTP_POST, .handler = post_bt_connect },
    { .uri = "/api/bt/forget",     .method = HTTP_POST, .handler = post_bt_forget },
    { .uri = "/api/playlists",     .method = HTTP_GET,  .handler = get_playlists },
    { .uri = "/api/playlist",      .method = HTTP_GET,  .handler = get_playlist },
    { .uri = "/api/playlist",      .method = HTTP_POST, .handler = post_playlist },
    { .uri = "/api/mode",          .method = HTTP_POST, .handler = post_mode },
    { .uri = "/api/delete",        .method = HTTP_POST, .handler = post_delete },
};

esp_err_t poket_httpd_start(void) {
    if (s_server) return ESP_OK;
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.max_uri_handlers = sizeof k_routes / sizeof k_routes[0] + 2;
    cfg.stack_size = 6144;
    cfg.lru_purge_enable = true;
    // An upload of a 20 MB file over a slow link holds the socket a long time.
    cfg.recv_wait_timeout = 20;
    cfg.send_wait_timeout = 20;

    esp_err_t err = httpd_start(&s_server, &cfg);
    if (err != ESP_OK) { ESP_LOGE(TAG, "start: %s", esp_err_to_name(err)); return err; }
    for (size_t i = 0; i < sizeof k_routes / sizeof k_routes[0]; i++)
        ESP_ERROR_CHECK(httpd_register_uri_handler(s_server, &k_routes[i]));
    ESP_LOGI(TAG, "serving %d bytes of UI on :80", WWW_INDEX_LEN);
    return ESP_OK;
}

void poket_httpd_stop(void) {
    if (!s_server) return;
    httpd_stop(s_server);
    s_server = NULL;
}

bool poket_httpd_running(void) { return s_server != NULL; }
uint8_t poket_httpd_clients(void) { return s_clients; }
