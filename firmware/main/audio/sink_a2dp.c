// A2DP source: the player pushes PCM at a Bluetooth headset.
//
// NOTE: A2DP needs Bluetooth Classic (BR/EDR). Rev A of this board used an
// ESP32-S3, which has Bluetooth LE only - soc_caps.h defines SOC_BLE_SUPPORTED
// and not SOC_BT_CLASSIC_SUPPORTED - so none of this could work there. Rev B
// moved to an ESP32-WROOM-32E-R2, which has both. The guard stays so the file
// still compiles for a BLE-only target instead of failing obscurely.
//
// The stack pulls rather than pushes, so this keeps a ring buffer that the data
// callback drains. If the ring runs dry the callback emits silence instead of
// stalling, because blocking inside that callback drops the link.
#include "audio/sink.h"
#include "audio/bt_link.h"
#include "esp_log.h"
#include "soc/soc_caps.h"

#if SOC_BT_CLASSIC_SUPPORTED
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_a2dp_api.h"
#include "nvs.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "a2dp";

#define NVS_NS    "poket"
#define NVS_PEER  "bt_peer"
#define NVS_PNAME "bt_pname"

static RingbufHandle_t s_rb;
static volatile bool s_connected;
static volatile bool s_started;          // media stream actually running
static volatile bool s_scanning;
static bool     s_stack_up;
static uint8_t  s_vol = 80;
static char     s_peer[BT_NAME_LEN];
static uint8_t  s_peer_bda[6];
static bool     s_want_connect;          // connect as soon as the inquiry ends

static bt_dev_t s_found[BT_MAX_FOUND];
static int      s_found_n;

bool        a2dp_connected(void)  { return s_connected; }
const char *a2dp_peer(void)       { return s_peer; }
bool        bt_link_connected(void) { return s_connected; }
const char *bt_link_peer_name(void) { return s_peer; }
bool        bt_link_up(void)        { return s_stack_up; }
bool        bt_link_scanning(void)  { return s_scanning; }
int         bt_link_count(void)     { return s_found_n; }
const bt_dev_t *bt_link_get(int i)
{
    return (i >= 0 && i < s_found_n) ? &s_found[i] : NULL;
}

// ---- remembered headset ----------------------------------------------------

static void save_peer(const uint8_t bda[6], const char *name)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_blob(h, NVS_PEER, bda, 6);
    nvs_set_str(h, NVS_PNAME, name && *name ? name : "headphones");
    nvs_commit(h);
    nvs_close(h);
}

static bool load_peer(uint8_t bda[6], char *name, size_t n)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return false;
    size_t len = 6;
    bool ok = (nvs_get_blob(h, NVS_PEER, bda, &len) == ESP_OK && len == 6);
    if (ok && name) {
        size_t nn = n;
        if (nvs_get_str(h, NVS_PNAME, name, &nn) != ESP_OK) snprintf(name, n, "headphones");
    }
    nvs_close(h);
    return ok;
}

bool bt_link_has_saved(void)
{
    uint8_t bda[6];
    return load_peer(bda, NULL, 0);
}

const char *bt_link_saved_name(void)
{
    static char name[BT_NAME_LEN];
    uint8_t bda[6];
    if (!load_peer(bda, name, sizeof name)) return "";
    return name;
}

void bt_link_forget(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_erase_key(h, NVS_PEER);
    nvs_erase_key(h, NVS_PNAME);
    nvs_commit(h);
    nvs_close(h);
    ESP_LOGI(TAG, "forgot the saved headset");
}

// ---- PCM path --------------------------------------------------------------

static int32_t data_cb(uint8_t *buf, int32_t len)
{
    if (!buf || len <= 0) return 0;
    if (!s_rb) { memset(buf, 0, len); return len; }
    size_t got = 0;
    uint8_t *p = xRingbufferReceiveUpTo(s_rb, &got, 0, len);
    if (!p) { memset(buf, 0, len); return len; }   // underrun -> silence, never block
    memcpy(buf, p, got);
    vRingbufferReturnItem(s_rb, p);
    if (got < (size_t)len) memset(buf + got, 0, len - got);
    return len;
}

// ---- discovery -------------------------------------------------------------

static void name_from_eir(uint8_t *eir, char *out, size_t n)
{
    uint8_t len = 0;
    uint8_t *p = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_CMPL_LOCAL_NAME, &len);
    if (!p) p = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_SHORT_LOCAL_NAME, &len);
    if (!p || !len) { out[0] = 0; return; }
    if (len > n - 1) len = n - 1;
    memcpy(out, p, len);
    out[len] = 0;
}

static void note_device(esp_bt_gap_cb_param_t *param)
{
    uint32_t cod = 0;
    int8_t rssi = -127;
    uint8_t *eir = NULL;

    for (int i = 0; i < param->disc_res.num_prop; i++) {
        esp_bt_gap_dev_prop_t *p = param->disc_res.prop + i;
        switch (p->type) {
        case ESP_BT_GAP_DEV_PROP_COD:  cod  = *(uint32_t *)(p->val); break;
        case ESP_BT_GAP_DEV_PROP_RSSI: rssi = *(int8_t *)(p->val);   break;
        case ESP_BT_GAP_DEV_PROP_EIR:  eir  = (uint8_t *)(p->val);   break;
        default: break;
        }
    }

    // Only audio sinks are of any use to a source. The "rendering" service bit
    // in the Class of Device is what headphones and speakers set; without this
    // filter the list fills up with phones and laptops that will never accept
    // an A2DP stream from us.
    if (!esp_bt_gap_is_valid_cod(cod) ||
            !(esp_bt_gap_get_cod_srvc(cod) & ESP_BT_COD_SRVC_RENDERING))
        return;

    for (int i = 0; i < s_found_n; i++)
        if (memcmp(s_found[i].bda, param->disc_res.bda, 6) == 0) {
            s_found[i].rssi = rssi;
            return;                                  // already have it
        }
    if (s_found_n >= BT_MAX_FOUND) return;

    bt_dev_t *d = &s_found[s_found_n];
    memcpy(d->bda, param->disc_res.bda, 6);
    d->rssi = rssi;
    d->name[0] = 0;
    if (eir) name_from_eir(eir, d->name, sizeof d->name);
    if (!d->name[0])
        snprintf(d->name, sizeof d->name, "%02X:%02X:%02X:%02X:%02X:%02X",
                 d->bda[0], d->bda[1], d->bda[2], d->bda[3], d->bda[4], d->bda[5]);
    s_found_n++;
    ESP_LOGI(TAG, "found sink %-20s rssi %d", d->name, rssi);
}

static void gap_cb(esp_bt_gap_cb_event_t ev, esp_bt_gap_cb_param_t *param)
{
    switch (ev) {
    case ESP_BT_GAP_DISC_RES_EVT:
        note_device(param);
        break;

    case ESP_BT_GAP_DISC_STATE_CHANGED_EVT:
        s_scanning = (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STARTED);
        if (!s_scanning) {
            ESP_LOGI(TAG, "inquiry done, %d sink(s)", s_found_n);
            // A connect request made mid-inquiry has to wait: the controller
            // will not page a device while it is still scanning.
            if (s_want_connect) {
                s_want_connect = false;
                esp_a2d_source_connect(s_peer_bda);
            }
        }
        break;

    case ESP_BT_GAP_AUTH_CMPL_EVT:
        if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS)
            ESP_LOGI(TAG, "paired with %s", (char *)param->auth_cmpl.device_name);
        else
            ESP_LOGE(TAG, "pairing failed, status %d", param->auth_cmpl.stat);
        break;

    default:
        break;
    }
}

// ---- A2DP ------------------------------------------------------------------

static void a2d_cb(esp_a2d_cb_event_t ev, esp_a2d_cb_param_t *p)
{
    switch (ev) {
    case ESP_A2D_CONNECTION_STATE_EVT:
        if (p->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
            s_connected = true;
            memcpy(s_peer_bda, p->conn_stat.remote_bda, 6);
            for (int i = 0; i < s_found_n; i++)
                if (memcmp(s_found[i].bda, s_peer_bda, 6) == 0)
                    snprintf(s_peer, sizeof s_peer, "%s", s_found[i].name);
            if (!s_peer[0]) snprintf(s_peer, sizeof s_peer, "headphones");
            save_peer(s_peer_bda, s_peer);
            ESP_LOGI(TAG, "link up: %s", s_peer);
            // Connected is not the same as streaming. The source must ask the
            // stack whether it is ready and then explicitly START, or the data
            // callback never fires and the headset plays silence.
            esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY);
        } else if (p->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
            s_connected = false;
            s_started = false;
            ESP_LOGI(TAG, "link down");
        }
        break;

    case ESP_A2D_MEDIA_CTRL_ACK_EVT:
        if (p->media_ctrl_stat.status != ESP_A2D_MEDIA_CTRL_ACK_SUCCESS) {
            ESP_LOGW(TAG, "media ctrl %d rejected", p->media_ctrl_stat.cmd);
            break;
        }
        if (p->media_ctrl_stat.cmd == ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY) {
            esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_START);
        } else if (p->media_ctrl_stat.cmd == ESP_A2D_MEDIA_CTRL_START) {
            s_started = true;
            ESP_LOGI(TAG, "streaming");
        } else if (p->media_ctrl_stat.cmd == ESP_A2D_MEDIA_CTRL_SUSPEND) {
            s_started = false;
        }
        break;

    default:
        break;
    }
}

// ---- stack bring-up --------------------------------------------------------

esp_err_t bt_link_init(void)
{
    if (s_stack_up) return ESP_OK;

    esp_bt_controller_config_t cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_err_t err = esp_bt_controller_init(&cfg);
    if (err != ESP_OK) return err;
    if ((err = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT)) != ESP_OK) return err;
    if ((err = esp_bluedroid_init()) != ESP_OK) return err;
    if ((err = esp_bluedroid_enable()) != ESP_OK) return err;

    esp_bt_gap_set_device_name("Poket");
    esp_bt_gap_register_callback(gap_cb);

    // Just-works pairing: there is no keypad on this thing to enter a code on.
    esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_NONE;
    esp_bt_gap_set_security_param(ESP_BT_SP_IOCAP_MODE, &iocap, sizeof(iocap));

    if ((err = esp_a2d_register_callback(a2d_cb)) != ESP_OK) return err;
    if ((err = esp_a2d_source_register_data_callback(data_cb)) != ESP_OK) return err;
    if ((err = esp_a2d_source_init()) != ESP_OK) return err;

    // A source is not a speaker: nothing should be connecting to us, so we stay
    // off the air as a target. This also keeps the inquiry window clear.
    esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);

    s_stack_up = true;
    ESP_LOGI(TAG, "a2dp source up");
    return ESP_OK;
}

void bt_link_deinit(void)
{
    if (!s_stack_up) return;
    if (s_scanning) esp_bt_gap_cancel_discovery();
    if (s_connected) esp_a2d_source_disconnect(s_peer_bda);
    esp_a2d_source_deinit();
    esp_bluedroid_disable();
    esp_bluedroid_deinit();
    esp_bt_controller_disable();
    esp_bt_controller_deinit();
    s_stack_up = s_connected = s_started = s_scanning = false;
    s_found_n = 0;
    ESP_LOGI(TAG, "a2dp source down");
}

esp_err_t bt_link_scan(uint8_t seconds)
{
    esp_err_t err = bt_link_init();
    if (err != ESP_OK) return err;
    s_found_n = 0;
    if (seconds < 3) seconds = 3;
    if (seconds > 30) seconds = 30;
    // the inquiry length unit is 1.28 s
    return esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY,
                                      (uint8_t)(seconds * 100 / 128), 0);
}

void bt_link_scan_stop(void)
{
    if (s_scanning) esp_bt_gap_cancel_discovery();
}

esp_err_t bt_link_connect(const uint8_t bda[6])
{
    esp_err_t err = bt_link_init();
    if (err != ESP_OK) return err;
    memcpy(s_peer_bda, bda, 6);
    s_peer[0] = 0;
    for (int i = 0; i < s_found_n; i++)
        if (memcmp(s_found[i].bda, bda, 6) == 0)
            snprintf(s_peer, sizeof s_peer, "%s", s_found[i].name);
    if (s_scanning) {                 // finish the inquiry first, then page
        s_want_connect = true;
        esp_bt_gap_cancel_discovery();
        return ESP_OK;
    }
    return esp_a2d_source_connect(s_peer_bda);
}

esp_err_t bt_link_connect_index(int i)
{
    const bt_dev_t *d = bt_link_get(i);
    return d ? bt_link_connect(d->bda) : ESP_ERR_INVALID_ARG;
}

esp_err_t bt_link_connect_saved(void)
{
    uint8_t bda[6];
    char name[BT_NAME_LEN];
    if (!load_peer(bda, name, sizeof name)) return ESP_ERR_NOT_FOUND;
    esp_err_t err = bt_link_connect(bda);
    if (err == ESP_OK) snprintf(s_peer, sizeof s_peer, "%s", name);
    return err;
}

void bt_link_disconnect(void)
{
    if (s_connected) esp_a2d_source_disconnect(s_peer_bda);
}

// ---- sink interface --------------------------------------------------------

static esp_err_t start(uint32_t rate, uint8_t ch)
{
    (void)rate; (void)ch;
    esp_err_t err = bt_link_init();
    if (err != ESP_OK) return err;
    if (!s_rb) {
        s_rb = xRingbufferCreate(16 * 1024, RINGBUF_TYPE_BYTEBUF);
        if (!s_rb) return ESP_ERR_NO_MEM;
    }
    if (!s_connected) {
        // Nothing paired yet - try the remembered headset, otherwise the user
        // has to pick one from a scan.
        if (bt_link_connect_saved() != ESP_OK)
            ESP_LOGW(TAG, "no headset linked yet; scan and connect one first");
    } else if (!s_started) {
        esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY);
    }
    return ESP_OK;
}

static size_t write_pcm(const int16_t *pcm, size_t frames)
{
    if (!s_rb || !s_started) return 0;
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

static void stop(void)
{
    // Suspend the stream but leave the link up: switching to the jack and back
    // should not make the user pair again.
    if (s_started) esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_SUSPEND);
    if (s_rb) { vRingbufferDelete(s_rb); s_rb = NULL; }
}

static void set_volume(uint8_t v) { s_vol = v > 100 ? 100 : v; }

const audio_sink_t sink_a2dp = { "bluetooth", start, write_pcm, stop, set_volume };

#else  /* !SOC_BT_CLASSIC_SUPPORTED */

static esp_err_t start(uint32_t rate, uint8_t ch)
{
    (void)rate; (void)ch;
    ESP_LOGE("a2dp", "this chip has Bluetooth LE only - A2DP needs BR/EDR");
    return ESP_ERR_NOT_SUPPORTED;
}
static size_t write_pcm(const int16_t *pcm, size_t frames) { (void)pcm; (void)frames; return 0; }
static void stop(void) {}
static void set_volume(uint8_t v) { (void)v; }

bool a2dp_connected(void) { return false; }
const char *a2dp_peer(void) { return ""; }

esp_err_t bt_link_init(void) { return ESP_ERR_NOT_SUPPORTED; }
void bt_link_deinit(void) {}
bool bt_link_up(void) { return false; }
esp_err_t bt_link_scan(uint8_t s) { (void)s; return ESP_ERR_NOT_SUPPORTED; }
void bt_link_scan_stop(void) {}
bool bt_link_scanning(void) { return false; }
int bt_link_count(void) { return 0; }
const bt_dev_t *bt_link_get(int i) { (void)i; return NULL; }
esp_err_t bt_link_connect(const uint8_t bda[6]) { (void)bda; return ESP_ERR_NOT_SUPPORTED; }
esp_err_t bt_link_connect_index(int i) { (void)i; return ESP_ERR_NOT_SUPPORTED; }
void bt_link_disconnect(void) {}
bool bt_link_connected(void) { return false; }
const char *bt_link_peer_name(void) { return ""; }
bool bt_link_has_saved(void) { return false; }
const char *bt_link_saved_name(void) { return ""; }
esp_err_t bt_link_connect_saved(void) { return ESP_ERR_NOT_SUPPORTED; }
void bt_link_forget(void) {}

const audio_sink_t sink_a2dp = { "bluetooth", start, write_pcm, stop, set_volume };

#endif
