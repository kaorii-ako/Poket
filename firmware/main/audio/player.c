// Playback engine.
//
// One task owns the whole pipeline: read -> decode -> sink. It never touches
// the UI and the UI never touches it; everything crosses through app_state.
// Buffers live in PSRAM because internal RAM is contended by the radio.
#include "audio/player.h"
#include "audio/sink.h"
#include "storage/library.h"
#include "board.h"
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_random.h"
#include "driver/gpio.h"

#define MINIMP3_IMPLEMENTATION
#define MINIMP3_ONLY_MP3
#include "minimp3.h"

static const char *TAG = "player";
#define FILE_BUF   (32 * 1024)
#define PCM_FRAMES  MINIMP3_MAX_SAMPLES_PER_FRAME

typedef struct {
    mp3dec_t        dec;
    FILE           *f;
    uint8_t        *fbuf;
    size_t          fill, pos;
    int16_t        *pcm;
    const audio_sink_t *sink;
    uint16_t        index;
    play_state_t    state;
    uint32_t        elapsed_ms;
    uint32_t        rate;
    uint32_t        bitrate;
    uint8_t         volume;
    repeat_t        repeat;
    bool            shuffle;
    audio_out_t     out;
    bool            want_next, want_prev, want_stop;
    uint32_t        seek_to;
    bool            want_seek;
    SemaphoreHandle_t lock;
} player_t;

static player_t P;

static void open_sink(void) {
    const audio_sink_t *want = (P.out == OUT_BLUETOOTH) ? &sink_a2dp : &sink_i2s;
    if (P.sink == want) return;
    if (P.sink) P.sink->stop();
    P.sink = want;
    P.sink->start(P.rate ? P.rate : 44100, 2);
    P.sink->set_volume(P.volume);
}

static bool open_track(uint16_t idx) {
    const lib_entry_t *e = library_get(idx);
    if (!e) return false;
    if (P.f) { fclose(P.f); P.f = NULL; }
    P.f = fopen(e->path, "rb");
    if (!P.f) { ESP_LOGE(TAG, "open %s failed", e->path); return false; }
    setvbuf(P.f, NULL, _IOFBF, 8192);
    mp3dec_init(&P.dec);
    P.fill = P.pos = 0;
    P.elapsed_ms = 0;
    P.index = idx;
    ESP_LOGI(TAG, "playing [%u] %s", idx, e->title);
    return true;
}

static uint16_t pick_next(void) {
    uint16_t n = library_count();
    if (n == 0) return 0;
    if (P.repeat == REPEAT_ONE) return P.index;
    if (P.shuffle) return (uint16_t)(esp_random() % n);
    uint16_t nx = P.index + 1;
    if (nx >= n) return (P.repeat == REPEAT_ALL) ? 0 : P.index;
    return nx;
}

static void player_task(void *arg) {
    (void)arg;
    for (;;) {
        if (P.state != PLAY_PLAYING || !P.f) { vTaskDelay(pdMS_TO_TICKS(20)); continue; }

        if (P.want_stop) { P.want_stop = false; P.state = PLAY_STOPPED;
                           if (P.sink) P.sink->stop(); P.sink = NULL; continue; }
        if (P.want_next) { P.want_next = false; open_track(pick_next()); continue; }
        if (P.want_prev) { P.want_prev = false;
                           open_track(P.index ? P.index - 1 : 0); continue; }
        if (P.want_seek) {
            P.want_seek = false;
            // Byte-seek using the average bitrate. Not frame accurate, and on a
            // VBR file it will land somewhere nearby; minimp3 resyncs on the
            // next frame header so the worst case is a short glitch.
            if (P.bitrate) {
                long off = (long)((uint64_t)P.seek_to * P.bitrate * 125);
                fseek(P.f, off, SEEK_SET);
                P.fill = P.pos = 0;
                P.elapsed_ms = P.seek_to * 1000;
            }
        }

        if (P.fill - P.pos < 4 * 1024) {                 // top the buffer up
            memmove(P.fbuf, P.fbuf + P.pos, P.fill - P.pos);
            P.fill -= P.pos; P.pos = 0;
            size_t got = fread(P.fbuf + P.fill, 1, FILE_BUF - P.fill, P.f);
            P.fill += got;
            if (got == 0 && P.fill == 0) {               // end of track
                uint16_t nx = pick_next();
                if (nx == P.index && P.repeat == REPEAT_OFF) {
                    P.state = PLAY_STOPPED;
                    if (P.sink) { P.sink->stop(); P.sink = NULL; }
                } else {
                    open_track(nx);
                }
                continue;
            }
        }

        mp3dec_frame_info_t info;
        int samples = mp3dec_decode_frame(&P.dec, P.fbuf + P.pos, (int)(P.fill - P.pos),
                                          P.pcm, &info);
        P.pos += info.frame_bytes;
        if (info.frame_bytes == 0) { P.pos = P.fill; continue; }   // resync
        if (samples == 0) continue;

        if ((uint32_t)info.hz != P.rate) {
            P.rate = (uint32_t)info.hz;
            if (P.sink) { P.sink->stop(); P.sink = NULL; }
            open_sink();
        }
        P.bitrate = (uint32_t)info.bitrate_kbps;
        if (!P.sink) open_sink();

        P.sink->write(P.pcm, (size_t)samples);
        P.elapsed_ms += (uint32_t)samples * 1000u / (P.rate ? P.rate : 44100);
    }
}

esp_err_t player_init(void) {
    memset(&P, 0, sizeof P);
    P.volume = 70; P.rate = 44100; P.out = OUT_JACK;
    P.lock = xSemaphoreCreateMutex();
    P.fbuf = heap_caps_malloc(FILE_BUF, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    P.pcm  = heap_caps_malloc(PCM_FRAMES * sizeof(int16_t),
                              MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!P.fbuf || !P.pcm) return ESP_ERR_NO_MEM;

    gpio_config_t jd = { .pin_bit_mask = 1ULL << PIN_JACK_DET,
                         .mode = GPIO_MODE_INPUT, .pull_up_en = GPIO_PULLUP_ENABLE };
    gpio_config(&jd);

    // The decoder has to keep ahead of a 10 ms DMA buffer, so it sits above the
    // UI but below the radio tasks.
    xTaskCreatePinnedToCore(player_task, "player", 8192, NULL, 8, NULL, 1);
    return ESP_OK;
}

esp_err_t player_play_index(uint16_t idx) {
    if (!open_track(idx)) return ESP_FAIL;
    P.state = PLAY_PLAYING;
    open_sink();
    return ESP_OK;
}
void player_toggle(void) {
    if (P.state == PLAY_PLAYING) P.state = PLAY_PAUSED;
    else if (P.state == PLAY_PAUSED) P.state = PLAY_PLAYING;
    else if (library_count()) player_play_index(P.index);
}
void player_stop(void)  { P.want_stop = true; }
void player_next(void)  { P.want_next = true; }
void player_prev(void)  { P.want_prev = true; }
void player_seek(uint32_t s) { P.seek_to = s; P.want_seek = true; }
void player_set_volume(uint8_t v) {
    P.volume = v > 100 ? 100 : v;
    if (P.sink) P.sink->set_volume(P.volume);
}
uint8_t player_volume(void) { return P.volume; }
void player_set_repeat(repeat_t r) { P.repeat = r; }
void player_set_shuffle(bool on)   { P.shuffle = on; }
audio_out_t player_output(void)    { return P.out; }
void player_set_output(audio_out_t o) {
    if (o == P.out) return;
    P.out = o;
    if (P.sink) { P.sink->stop(); P.sink = NULL; }
    open_sink();
}

bool a2dp_connected(void);
const char *a2dp_peer(void);

void player_publish(app_state_t *s) {
    s->play = P.state;
    s->volume = P.volume;
    s->repeat = P.repeat;
    s->shuffle = P.shuffle;
    s->out = P.out;
    s->elapsed_s = P.elapsed_ms / 1000;
    s->queue_pos = P.index;
    s->queue_len = library_count();
    s->jack_present = gpio_get_level(PIN_JACK_DET) == 0;
    s->bt_connected = (P.out == OUT_BLUETOOTH) && a2dp_connected();
    if (s->bt_connected) strlcpy(s->bt_peer, a2dp_peer(), sizeof s->bt_peer);
    const lib_entry_t *e = library_get(P.index);
    if (e) {
        strlcpy(s->track.title, e->title, TRACK_TITLE_LEN);
        strlcpy(s->track.artist, e->artist, TRACK_ARTIST_LEN);
        strlcpy(s->track.path, e->path, TRACK_PATH_LEN);
        s->track.duration_s = e->duration_s;
    }
    s->track.bitrate_kbps = P.bitrate;
    s->track.sample_rate = P.rate;
}
