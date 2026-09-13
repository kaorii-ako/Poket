// BQ27441-G1 fuel gauge, with the ADC divider as a fallback.
//
// The gauge is the accurate source but it needs a learning cycle before its
// state of charge means much, and it can be absent on a partly-built board. So
// the divider on GPIO1 is always read too, and if the gauge does not answer we
// fall back to a voltage curve. A player that shows no battery at all is worse
// than one showing an approximate number.
#include "drivers/battery.h"
#include "drivers/i2c_bus.h"
#include "board.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"

static const char *TAG = "batt";
static i2c_master_dev_handle_t s_gauge;
static bool s_gauge_ok;
static adc_oneshot_unit_handle_t s_adc;
static adc_cali_handle_t s_cali;

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
    ESP_LOGI(TAG, "fuel gauge %s", s_gauge_ok ? "present" : "absent, using divider");

    adc_oneshot_unit_init_cfg_t u = { .unit_id = ADC_UNIT_1 };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&u, &s_adc));
    adc_oneshot_chan_cfg_t c = { .atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_DEFAULT };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc, ADC_CHANNEL_0, &c));   // GPIO1
    adc_cali_curve_fitting_config_t cc = {
        .unit_id = ADC_UNIT_1, .atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    adc_cali_create_scheme_curve_fitting(&cc, &s_cali);
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
    int raw = 0, mv_adc = 0;
    if (adc_oneshot_read(s_adc, ADC_CHANNEL_0, &raw) == ESP_OK && s_cali)
        adc_cali_raw_to_voltage(s_cali, raw, &mv_adc);
    uint16_t divider_mv = (uint16_t)(mv_adc * VBAT_DIVIDER_RATIO);

    o->gauge_ok = s_gauge_ok;
    o->mv = divider_mv;
    o->pct = curve_pct(divider_mv);
    o->ma = 0;

    if (s_gauge_ok) {
        uint16_t v, soc, cur, flags;
        if (bq_read16(BQ_VOLTAGE, &v) == ESP_OK) o->mv = v;
        if (bq_read16(BQ_SOC, &soc) == ESP_OK && soc <= 100) o->pct = (uint8_t)soc;
        if (bq_read16(BQ_CURRENT, &cur) == ESP_OK) o->ma = (int16_t)cur;
        if (bq_read16(BQ_FLAGS, &flags) == ESP_OK) { /* bit 0 = DSG */ }
    }
    // The charger's STAT line is not wired to the MCU, so "charging" is
    // inferred: current flowing in, or the divider pinned high on USB power.
    o->charging = (o->ma > 20) || (!s_gauge_ok && divider_mv > 4150);
}
