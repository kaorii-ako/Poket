// BQ27441-G1 fuel gauge.
//
// REV B dropped the ADC divider that used to back this up: on the classic
// ESP32 every ADC1 pin is taken by the encoder and the three input-only
// buttons, and ADC2 stops working the moment Wi-Fi is on - which is exactly
// when the transfer screen wants to show a battery. The gauge reports voltage
// and current over I2C, so the divider was redundant anyway. If the gauge is
// absent the UI shows an unknown battery rather than a wrong one.
#include "drivers/battery.h"
#include "drivers/i2c_bus.h"
#include "board.h"
#include "esp_log.h"

static const char *TAG = "batt";
static i2c_master_dev_handle_t s_gauge;
static bool s_gauge_ok;

#define BQ_VOLTAGE 0x04
#define BQ_SOC     0x1C
#define BQ_CURRENT 0x10
#define BQ_FLAGS   0x06

static esp_err_t bq_read16(uint8_t reg, uint16_t *out) {
    uint8_t rx[2];
    esp_err_t e = i2c_master_transmit_receive(s_gauge, &reg, 1, rx, 2, 100);
    if (e == ESP_OK) *out = (uint16_t)rx[0] | ((uint16_t)rx[1] << 8);
    return e;
}

esp_err_t battery_init(void) {
    ESP_ERROR_CHECK(i2c_bus_init());
    if (i2c_dev_add(I2C_ADDR_GAUGE, 100000, &s_gauge) == ESP_OK) {
        uint16_t v = 0;
        s_gauge_ok = (bq_read16(BQ_VOLTAGE, &v) == ESP_OK && v > 2000 && v < 5000);
    }
    ESP_LOGI(TAG, "fuel gauge %s", s_gauge_ok ? "present" : "absent");
    return ESP_OK;
}

static uint8_t curve_pct(uint16_t mv) {
    if (mv >= BATT_FULL_MV) return 100;
    if (mv <= BATT_EMPTY_MV) return 0;
    // Piecewise, because a straight line across a LiPo curve reads badly:
    // it sits on 100% for ages then collapses. These knees track a 4.2 V cell
    // under a light load well enough for a status icon.
    static const uint16_t v[] = { 3300, 3600, 3700, 3800, 3900, 4000, 4200 };
    static const uint8_t  p[] = {    0,   10,   25,   45,   65,   85,  100 };
    for (int i = 1; i < 7; i++) {
        if (mv <= v[i]) {
            int span = v[i] - v[i - 1];
            return (uint8_t)(p[i - 1] + (p[i] - p[i - 1]) * (mv - v[i - 1]) / span);
        }
    }
    return 100;
}

void battery_read(batt_t *o) {
    o->gauge_ok = s_gauge_ok;
    o->mv = 0;
    o->pct = 0;
    o->ma = 0;

    if (s_gauge_ok) {
        uint16_t v, soc, cur, flags;
        if (bq_read16(BQ_VOLTAGE, &v) == ESP_OK) o->mv = v;
        // The gauge's own SoC needs a learning cycle before it means much, so
        // the voltage curve stands in until it has one.
        if (bq_read16(BQ_SOC, &soc) == ESP_OK && soc > 0 && soc <= 100) o->pct = (uint8_t)soc;
        else o->pct = curve_pct(o->mv);
        if (bq_read16(BQ_CURRENT, &cur) == ESP_OK) o->ma = (int16_t)cur;
        if (bq_read16(BQ_FLAGS, &flags) == ESP_OK) { /* bit 0 = DSG */ }
    }
    // The charger's STAT line is not wired to the MCU, so "charging" is
    // inferred from current flowing into the cell.
    o->charging = (o->ma > 20);
}
