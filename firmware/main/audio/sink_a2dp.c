// A2DP source: the player pushes PCM at a Bluetooth headset.
//
// The stack pulls rather than pushes, so this keeps a ring buffer that the
// data callback drains. A2DP wants 44.1 kHz stereo; anything else gets resampled
// upstream. If the ring runs dry the callback emits silence instead of stalling,
// because blocking inside that callback drops the link.
#include "audio/sink.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_a2dp_api.h"
#include <string.h>

static const char *TAG = "a2dp";
static RingbufHandle_t s_rb;
static volatile bool s_connected;
static uint8_t s_vol = 80;
static char s_peer[32];

bool a2dp_connected(void) { return s_connected; }
const char *a2dp_peer(void) { return s_peer; }

static int32_t data_cb(uint8_t *buf, int32_t len) {
    if (!buf || len <= 0) return 0;
    size_t got = 0;
    uint8_t *p = xRingbufferReceiveUpTo(s_rb, &got, 0, len);
    if (!p) { memset(buf, 0, len); return len; }   // underrun -> silence, never block
    memcpy(buf, p, got);
    vRingbufferReturnItem(s_rb, p);
    if (got < (size_t)len) memset(buf + got, 0, len - got);
    return len;
}

static void a2d_cb(esp_a2d_cb_event_t ev, esp_a2d_cb_param_t *p) {
    if (ev == ESP_A2D_CONNECTION_STATE_EVT) {
        s_connected = (p->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTED);
        ESP_LOGI(TAG, "link %s", s_connected ? "up" : "down");
    }
}

static esp_err_t start(uint32_t rate, uint8_t ch) {
    (void)rate; (void)ch;
    if (s_rb) return ESP_OK;
    s_rb = xRingbufferCreate(16 * 1024, RINGBUF_TYPE_BYTEBUF);
    if (!s_rb) return ESP_ERR_NO_MEM;

    esp_bt_controller_config_t cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    cfg.mode = ESP_BT_MODE_CLASSIC_BT;
    ESP_ERROR_CHECK(esp_bt_controller_init(&cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT));
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());
    esp_bt_gap_set_device_name("Poket");
    ESP_ERROR_CHECK(esp_a2d_register_callback(a2d_cb));
    ESP_ERROR_CHECK(esp_a2d_source_register_data_callback(data_cb));
    ESP_ERROR_CHECK(esp_a2d_source_init());
    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
    ESP_LOGI(TAG, "a2dp source ready, discoverable as 'Poket'");
    return ESP_OK;
}

static size_t write_pcm(const int16_t *pcm, size_t frames) {
    if (!s_rb) return 0;
    static int16_t scaled[1024 * 2];
    size_t n = frames > 1024 ? 1024 : frames;
    int32_t g = (int32_t)s_vol * s_vol / 100;
    for (size_t i = 0; i < n * 2; i++)
        scaled[i] = (int16_t)((int32_t)pcm[i] * g / 100);
    // Drop rather than block: a full ring means the headset is behind, and
    // waiting here would stall the decoder and make it worse.
    if (xRingbufferSend(s_rb, scaled, n * 2 * sizeof(int16_t), pdMS_TO_TICKS(60)) != pdTRUE)
        return 0;
    return n;
}

static void stop(void) {
    if (!s_rb) return;
    esp_a2d_source_deinit();
    esp_bluedroid_disable(); esp_bluedroid_deinit();
    esp_bt_controller_disable(); esp_bt_controller_deinit();
    vRingbufferDelete(s_rb); s_rb = NULL;
    s_connected = false;
}

static void set_volume(uint8_t v) { s_vol = v > 100 ? 100 : v; }

const audio_sink_t sink_a2dp = { "bluetooth", start, write_pcm, stop, set_volume };
