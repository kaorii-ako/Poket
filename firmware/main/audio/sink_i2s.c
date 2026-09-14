// PCM5102A over I2S.
//
// There is NO MCLK trace on this board - the DAC runs from its internal PLL off
// BCK/LRCK. Handing i2s_std_gpio_config_t an MCLK pin would compile fine and
// produce silence on hardware, so it is explicitly I2S_GPIO_UNUSED here.
#include "audio/sink.h"
#include "board.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "i2s";
static i2s_chan_handle_t s_tx;
static uint8_t s_vol = 80;

static esp_err_t start(uint32_t rate, uint8_t ch) {
    if (s_tx) return ESP_OK;

    gpio_config_t io = {
        .pin_bit_mask = (1ULL << PIN_DAC_XSMT) | (1ULL << PIN_AMP_EN),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io);
    gpio_set_level(PIN_DAC_XSMT, 0);        // hold the DAC muted while we set up
    gpio_set_level(PIN_AMP_EN, 0);

    i2s_chan_config_t cc = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    cc.dma_desc_num = 6;
    cc.dma_frame_num = 480;                 // ~10 ms at 48k, 4 frames of slack
    ESP_ERROR_CHECK(i2s_new_channel(&cc, &s_tx, NULL));

    i2s_std_config_t sc = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(rate),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
                        I2S_DATA_BIT_WIDTH_16BIT,
                        ch == 1 ? I2S_SLOT_MODE_MONO : I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,        // see the note above
            .bclk = PIN_I2S_BCK,
            .ws   = PIN_I2S_LRCK,
            .dout = PIN_I2S_DIN,
            .din  = I2S_GPIO_UNUSED,
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(s_tx, &sc));
    ESP_ERROR_CHECK(i2s_channel_enable(s_tx));

    // Unmute the DAC first, then the amp, ~40 ms apart. Doing it the other way
    // round puts the DAC's power-up transient straight through the headphones.
    gpio_set_level(PIN_DAC_XSMT, 1);
    vTaskDelay(pdMS_TO_TICKS(40));
    gpio_set_level(PIN_AMP_EN, 1);
    ESP_LOGI(TAG, "i2s up at %lu Hz, %u ch", (unsigned long)rate, ch);
    return ESP_OK;
}

static size_t write_pcm(const int16_t *pcm, size_t frames) {
    if (!s_tx) return 0;
    // Software volume. The PAM8908 has fixed gain and the PCM5102A has no
    // volume register wired, so this is the only control there is.
    static int16_t scaled[1024 * 2];
    size_t n = frames > 1024 ? 1024 : frames;
    int32_t g = (int32_t)s_vol * s_vol / 100;        // perceptual-ish taper
    for (size_t i = 0; i < n * 2; i++)
        scaled[i] = (int16_t)((int32_t)pcm[i] * g / 100);
    size_t wrote = 0;
    i2s_channel_write(s_tx, scaled, n * 2 * sizeof(int16_t), &wrote, portMAX_DELAY);
    return wrote / (2 * sizeof(int16_t));
}

static void stop(void) {
    if (!s_tx) return;
    gpio_set_level(PIN_AMP_EN, 0);           // amp down first, again for the pop
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(PIN_DAC_XSMT, 0);
    i2s_channel_disable(s_tx);
    i2s_del_channel(s_tx);
    s_tx = NULL;
}

static void set_volume(uint8_t v) { s_vol = v > 100 ? 100 : v; }

const audio_sink_t sink_i2s = { "jack", start, write_pcm, stop, set_volume };
