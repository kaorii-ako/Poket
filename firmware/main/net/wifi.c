#include "net/wifi.h"
#include "net/httpd.h"
#include "app/app_state.h"
#include "audio/player.h"

#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "wifi";
static bool s_active;
static esp_netif_t *s_ap;
static char s_ssid[24], s_ip[16], s_pass[16];
static uint8_t s_clients;

#define NVS_NS   "poket"
#define NVS_PASS "ap_pass"

// A fixed default password would be the same on every unit ever built, so one
// is generated on first boot and kept. The transfer screen shows it.
static void load_or_make_password(void) {
    nvs_handle_t h;
    size_t len = sizeof s_pass;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) == ESP_OK) {
        if (nvs_get_str(h, NVS_PASS, s_pass, &len) == ESP_OK && strlen(s_pass) >= 8) {
            nvs_close(h);
            return;
        }
        // Avoid 0/O and 1/l - this gets read off a 128x64 screen and typed.
        static const char alphabet[] = "abcdefghijkmnpqrstuvwxyz23456789";
        for (int i = 0; i < 10; i++) s_pass[i] = alphabet[esp_random() % (sizeof alphabet - 1)];
        s_pass[10] = 0;
        nvs_set_str(h, NVS_PASS, s_pass);
        nvs_commit(h);
        nvs_close(h);
    } else {
        snprintf(s_pass, sizeof s_pass, "poketpoket");
    }
}

static void publish(app_state_t *s, void *ctx) {
    (void)ctx;
    s->wifi_up = s_active;
    s->wifi_is_ap = true;
    s->clients = s_clients;
    snprintf(s->wifi_ssid, sizeof s->wifi_ssid, "%s", s_ssid);
    snprintf(s->wifi_ip, sizeof s->wifi_ip, "%s", s_ip);
}

static void on_wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data) {
    (void)arg; (void)base; (void)data;
    if (id == WIFI_EVENT_AP_STACONNECTED) s_clients++;
    else if (id == WIFI_EVENT_AP_STADISCONNECTED && s_clients) s_clients--;
    else return;
    ESP_LOGI(TAG, "%u client(s)", s_clients);
    app_state_update(publish, NULL);
}

esp_err_t wifi_transfer_start(void) {
    if (s_active) return ESP_OK;

    // One antenna, one PHY: A2DP has to go before the AP comes up, or both
    // stutter. Forcing the jack here makes that trade visible rather than
    // letting the user wonder why their headphones dropped.
    player_set_output(OUT_JACK);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    s_ap = esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, on_wifi_event, NULL, NULL));

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    snprintf(s_ssid, sizeof s_ssid, "Poket-%02X%02X", mac[4], mac[5]);
    load_or_make_password();

    wifi_config_t wc = { 0 };
    snprintf((char *)wc.ap.ssid, sizeof wc.ap.ssid, "%s", s_ssid);
    wc.ap.ssid_len = strlen(s_ssid);
    snprintf((char *)wc.ap.password, sizeof wc.ap.password, "%s", s_pass);
    wc.ap.max_connection = 4;
    wc.ap.authmode = WIFI_AUTH_WPA2_PSK;
    wc.ap.channel = 6;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wc));
    ESP_ERROR_CHECK(esp_wifi_start());
    // The radio is idle between requests; letting it sleep costs a few ms of
    // latency and saves a lot of current on a 400 mAh cell.
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);

    esp_netif_ip_info_t ip;
    esp_netif_get_ip_info(s_ap, &ip);
    snprintf(s_ip, sizeof s_ip, IPSTR, IP2STR(&ip.ip));

    esp_err_t err = poket_httpd_start();
    if (err != ESP_OK) { wifi_transfer_stop(); return err; }

    s_active = true;
    s_clients = 0;
    app_state_update(publish, NULL);
    ESP_LOGI(TAG, "AP %s up on %s", s_ssid, s_ip);
    return ESP_OK;
}

void wifi_transfer_stop(void) {
    poket_httpd_stop();
    esp_wifi_stop();
    esp_wifi_deinit();
    if (s_ap) { esp_netif_destroy_default_wifi(s_ap); s_ap = NULL; }
    s_active = false;
    s_clients = 0;
    s_ssid[0] = s_ip[0] = 0;
    app_state_update(publish, NULL);
    ESP_LOGI(TAG, "AP down");
}

bool wifi_transfer_active(void) { return s_active; }

void wifi_transfer_info(char *ssid, size_t sn, char *ip, size_t in, char *pass, size_t pn) {
    if (ssid && sn) snprintf(ssid, sn, "%s", s_ssid);
    if (ip && in)   snprintf(ip, in, "%s", s_ip);
    if (pass && pn) snprintf(pass, pn, "%s", s_pass);
}
