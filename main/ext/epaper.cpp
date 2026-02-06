#include "epaper.h"

#include "../config.h"

#include "esp_log.h"
#include "driver/gpio.h"

static const char *TAG = "EPAPER";
BDEpaper epaper;

static const struct {
    uint8_t r, g, b;
    uint8_t epaper_color;
} bw_palette[] = {
    {0,   0,   0,   EPD_PIXEL_BLACK},
    {255, 255, 255, EPD_PIXEL_WHITE},
};

static inline int clamp_byte(int v) {
    return v < 0 ? 0 : (v > 255 ? 255 : v);
}

static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *color_p) {
    BDEpaper* ctx = (BDEpaper*) lv_display_get_user_data(disp);
    ctx->flush(disp, area, color_p);
}

static inline uint32_t calc_buffer_size(uint16_t w, uint16_t h, uint8_t bpp) {
    return ((uint32_t)w * h * bpp + 7) / 8;
}

BDEpaper::BDEpaper() {
    panelSettings = {
        .softReset = NO_SOFT_RESET,
        .booster = BOOSTER_ON,
        .horizontalScanDir = HSCAN_RIGHT,
        .verticalScanDir = VSCAN_DOWN,
        .kwr = BLACK_WHITE,
        .lut = LUT_OTP,
        .resolution = RESOLUTION_480x240,

        .vcLutz = VC_LUTZ_FLOAT_VCOM,
        .norg = NORG_NO_EFFECT,
        .tieg = TIEG_VGL_GND,
        .tsAuto = TS_AUTO_NO_EFFECT,
        .vcmz = VCMZ_NO_EFFECT,
        .reserved = 0
    };

    buffer_size = calc_buffer_size(DISPLAY_X, DISPLAY_Y, DISPLAY_B); // 12480
    framebuffer = (uint8_t*) malloc(buffer_size);
    if (!framebuffer) printf("Failed to allocate framebuffer %lu\n", buffer_size);
    memset(framebuffer, 0xFF, buffer_size);

    // Allocate lvgl buffer
    uint32_t partial_lines = 10;
    lvgl_buf_size = DISPLAY_X * partial_lines * sizeof(uint16_t); // 8320
    lvgl_buf = (uint8_t*) malloc(lvgl_buf_size);
    if (!lvgl_buf) printf("Failed to allocate lvgl_buf %lu\n", lvgl_buf_size);
    partial_render_mode = true;

    // Allocate buffer for dithering
    size_t rgb_buf_size = DISPLAY_X * DISPLAY_Y * 3; // 299,520
    rgb_buf = (uint8_t*) malloc(rgb_buf_size);
    if (!rgb_buf) printf("Failed to allocate rgb_buf %u\n", rgb_buf_size);
    memset(rgb_buf, 0xFF, rgb_buf_size);
}

void BDEpaper::panelWrite(const uint8_t *data, uint32_t len) {
    spiC(E_CMD_DATA_TRASMISSION);
    spiBulk(data, len);

    spiC(E_CMD_DATA_TRASMISSION_2);
    spiBulk(data, len);
}

void BDEpaper::panelUpdate() {
    spiC(E_CMD_AUTO);
    spiD(AUTO_SEQUENCE_PON_DRF_POF);
    wait();

    spiC(E_CMD_DEEP_SLEEP);
    wait();

    gpio_set_level(HV_CTL_PIN, 0);
    pwrState = false;
}

void BDEpaper::power() {
    panelInit();
}

void BDEpaper::flushDisplay() {
    panelWrite(framebuffer, buffer_size);
    panelUpdate();
    ESP_LOGI(TAG, "Panel update complete");
}

void BDEpaper::flush(lv_display_t *disp, const lv_area_t *area, uint8_t *color_p) {
    uint16_t *buffer = (uint16_t *)color_p;

    // Store RGB888 in buffer for dithering
    for (int y = area->y1; y <= area->y2; y++) {
        for (int x = area->x1; x <= area->x2; x++) {
            // RGB565 to RGB888 with proper bit expansion
            // This ensures white (31,63,31) -> (255,255,255)
            uint16_t c = *buffer++;
            uint8_t r5 = (c >> 11) & 0x1F;
            uint8_t g6 = (c >> 5) & 0x3F;
            uint8_t b5 = c & 0x1F;
            uint8_t r = (r5 << 3) | (r5 >> 2);  // 5-bit to 8-bit
            uint8_t g = (g6 << 2) | (g6 >> 4);  // 6-bit to 8-bit
            uint8_t b = (b5 << 3) | (b5 >> 2);  // 5-bit to 8-bit

            int idx = (y * DISPLAY_X + x) * 3;
            rgb_buf[idx + 0] = r;
            rgb_buf[idx + 1] = g;
            rgb_buf[idx + 2] = b;
        }
        // Yield periodically
        if ((y % 50) == 0) {
            vTaskDelay(1);
        }
    }

    // Apply dithering after all pixels collected
    bool is_last = (area->x2 == DISPLAY_X - 1) && (area->y2 == DISPLAY_Y - 1);
    if (is_last) {
        // Clear framebuffer first using bits_per_pixel from panel
        uint8_t fill_byte;
        if (DISPLAY_B == 4) {
            fill_byte = (EPD_PIXEL_WHITE << 4) | EPD_PIXEL_WHITE;
        } else if (DISPLAY_B == 2) {
            fill_byte = 0x55;  // White (01) for all 4 pixels
        } else {
            fill_byte = 0xFF;  // 1-bit white
        }
        memset(framebuffer, fill_byte, buffer_size);

        applyDithering();
    }

    // Mark framebuffer as dirty
    fb_dirty = true;

    // Only update display when:
    // 1. Full render mode (not partial) - update every flush
    // 2. Partial render mode - only update on last chunk
    if (!partial_render_mode || is_last) {
        // Update display
        flushDisplay();
        fb_dirty = false;
    }

    lv_display_flush_ready(disp);
}

static uint8_t find_nearest_color_idx(int r, int g, int b) {
    // BW mode: simple threshold
    int gray = (r * 299 + g * 587 + b * 114) / 1000;
    return (gray < 128) ? 0 : 1;  // 0=black, 1=white
}

static inline void set_fb_pixel(uint8_t *fb, int y, int x, int width, uint8_t color)
{
    // 1-bit per pixel (BW)
    uint16_t byte_idx = y * (width / 8) + (x / 8);
    uint8_t bit = 7 - (x % 8);
    if (color == EPD_PIXEL_WHITE) {
        fb[byte_idx] |= (1 << bit);
    } else {
        fb[byte_idx] &= ~(1 << bit);
    }
}

void BDEpaper::applyDithering() {
    for (int y = 0; y < DISPLAY_Y; y++) {
        // Yield every 20 rows to prevent watchdog
        if (y % 20 == 0) {
            vTaskDelay(1);
        }

        for (int x = 0; x < DISPLAY_X; x++) {
            int idx = (y * DISPLAY_X + x) * 3;

            // Get current pixel RGB
            int r = rgb_buf[idx + 0];
            int g = rgb_buf[idx + 1];
            int b = rgb_buf[idx + 2];

            // Find nearest palette color
            uint8_t pal_idx = find_nearest_color_idx(r, g, b);
            uint8_t epaper_color;
            int pal_r, pal_g, pal_b;

            epaper_color = (pal_idx == 0) ? EPD_PIXEL_BLACK : EPD_PIXEL_WHITE;
            pal_r = bw_palette[pal_idx].r;
            pal_g = bw_palette[pal_idx].g;
            pal_b = bw_palette[pal_idx].b;

            // Set pixel in framebuffer
            set_fb_pixel(framebuffer, x, y, DISPLAY_Y, epaper_color);

            // Calculate quantization error
            int err_r = r - pal_r;
            int err_g = g - pal_g;
            int err_b = b - pal_b;

            // Distribute error to neighbors (Floyd-Steinberg)
            // Right pixel: 7/16
            if (x + 1 < DISPLAY_X) {
                int ni = idx + 3;
                rgb_buf[ni + 0] = clamp_byte(rgb_buf[ni + 0] + err_r * 7 / 16);
                rgb_buf[ni + 1] = clamp_byte(rgb_buf[ni + 1] + err_g * 7 / 16);
                rgb_buf[ni + 2] = clamp_byte(rgb_buf[ni + 2] + err_b * 7 / 16);
            }
            // Bottom-left pixel: 3/16
            if (y + 1 < DISPLAY_Y && x > 0) {
                int ni = ((y + 1) * DISPLAY_X + x - 1) * 3;
                rgb_buf[ni + 0] = clamp_byte(rgb_buf[ni + 0] + err_r * 3 / 16);
                rgb_buf[ni + 1] = clamp_byte(rgb_buf[ni + 1] + err_g * 3 / 16);
                rgb_buf[ni + 2] = clamp_byte(rgb_buf[ni + 2] + err_b * 3 / 16);
            }
            // Bottom pixel: 5/16
            if (y + 1 < DISPLAY_Y) {
                int ni = ((y + 1) * DISPLAY_X + x) * 3;
                rgb_buf[ni + 0] = clamp_byte(rgb_buf[ni + 0] + err_r * 5 / 16);
                rgb_buf[ni + 1] = clamp_byte(rgb_buf[ni + 1] + err_g * 5 / 16);
                rgb_buf[ni + 2] = clamp_byte(rgb_buf[ni + 2] + err_b * 5 / 16);
            }
            // Bottom-right pixel: 1/16
            if (y + 1 < DISPLAY_Y && x + 1 < DISPLAY_X) {
                int ni = ((y + 1) * DISPLAY_X + x + 1) * 3;
                rgb_buf[ni + 0] = clamp_byte(rgb_buf[ni + 0] + err_r / 16);
                rgb_buf[ni + 1] = clamp_byte(rgb_buf[ni + 1] + err_g / 16);
                rgb_buf[ni + 2] = clamp_byte(rgb_buf[ni + 2] + err_b / 16);
            }
        }
    }
}

esp_err_t BDEpaper::spiInit() {
    gpio_config_t io_conf = {
        .pin_bit_mask = 0,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    // DC, RST, CS as output
    io_conf.pin_bit_mask = BIT(E_DC_PIN) | BIT(E_RST_PIN) | BIT(E_CS_PIN);
    gpio_config(&io_conf);

    // BUSY as input
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = BIT(E_BUSY_PIN);
    gpio_config(&io_conf);

    // Set initial state
    gpio_set_level(E_CS_PIN, 1);
    gpio_set_level(E_DC_PIN, 1);
    gpio_set_level(E_RST_PIN, 1);

    // Configure SPI bus (large buffer for big displays like 800x480)
    spi_bus_config_t buscfg = {
        .mosi_io_num = E_SDA_PIN,
        .miso_io_num = -1,  // Not used
        .sclk_io_num = E_SCL_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .data4_io_num = -1,
        .data5_io_num = -1,
        .data6_io_num = -1,
        .data7_io_num = -1,
        .data_io_default_level = false,
        .max_transfer_sz = SPI_MAX_CHUNK_SIZE,  // Match chunk size
        .flags = 0,
        .isr_cpu_id = ESP_INTR_CPU_AFFINITY_AUTO,
        .intr_flags = 0
    };

    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "SPI bus init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Add SPI device
    spi_device_interface_config_t devcfg = {
        .command_bits = 0,
        .address_bits = 0,
        .dummy_bits = 0,
        .mode = 0,
        .clock_source = SPI_CLK_SRC_DEFAULT,
        .duty_cycle_pos = 128,
        .cs_ena_pretrans = 0,
        .cs_ena_posttrans = 0,
        .clock_speed_hz = SPI_SPEED,
        .input_delay_ns = 0,
        .sample_point = SPI_SAMPLING_POINT_PHASE_0,
        .spics_io_num = -1,
        .flags = 0,
        .queue_size = 1,
        .pre_cb = NULL,
        .post_cb = NULL
    };

    ret = spi_bus_add_device(SPI2_HOST, &devcfg, &spiHandle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI device add failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "SPI initialized");
    return ESP_OK;
}

void BDEpaper::panelInit() {
    powerOn();

    vTaskDelay(pdMS_TO_TICKS(100));
    spiR();
    vTaskDelay(pdMS_TO_TICKS(50));
    wait();

    uint16_t settingInt;
    memcpy(&settingInt, &panelSettings, 2);

    spiC(E_CMD_PANEL_SETTINGS);
    spiD(settingInt);
    spiD(settingInt >> 8);

    initialized = true;
    ESP_LOGI(TAG, "Panel initialized");
}

void BDEpaper::spiR() {
    gpio_set_level(E_RST_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(E_RST_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
}

void BDEpaper::wait(uint32_t timeout_ms) {
    uint32_t start = xTaskGetTickCount();
    while (gpio_get_level(E_BUSY_PIN) == 0) {
        if (timeout_ms > 0) {
            uint32_t elapsed = (xTaskGetTickCount() - start) * portTICK_PERIOD_MS;
            if (elapsed >= timeout_ms) {
                ESP_LOGW(TAG, "Wait busy timeout (%lu ms)", timeout_ms);
                return;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void BDEpaper::spiC(PanelCommands cmd) {
    gpio_set_level(E_CS_PIN, 0);
    gpio_set_level(E_DC_PIN, 0);  // Command mode

    spi_transaction_t t = {
        .flags = 0,
        .cmd = 0,
        .addr = 0,
        .length = 8,
        .rxlength = 0,
        .override_freq_hz = 0,
        .user = NULL,
        .tx_buffer = &cmd,
        .rx_buffer = NULL
    };
    spi_device_polling_transmit(spiHandle, &t);

    gpio_set_level(E_CS_PIN, 1);
}

void BDEpaper::spiD(uint8_t data) {
    gpio_set_level(E_CS_PIN, 0);
    gpio_set_level(E_DC_PIN, 1);  // Data mode

    spi_transaction_t t = {
        .flags = 0,
        .cmd = 0,
        .addr = 0,
        .length = 8,
        .rxlength = 0,
        .override_freq_hz = 0,
        .user = NULL,
        .tx_buffer = &data,
        .rx_buffer = NULL
    };
    spi_device_polling_transmit(spiHandle, &t);

    gpio_set_level(E_CS_PIN, 1);
}

void BDEpaper::spiBulk(const uint8_t *data, uint32_t len) {
    gpio_set_level(E_CS_PIN, 0);
    gpio_set_level(E_DC_PIN, 1);  // Data mode

    // Send in chunks to avoid SPI transfer limit
    uint32_t offset = 0;
    while (offset < len) {
        uint32_t chunk_size = (len - offset > SPI_MAX_CHUNK_SIZE) ? SPI_MAX_CHUNK_SIZE : (len - offset);
        spi_transaction_t t = {
            .flags = 0,
            .cmd = 0,
            .addr = 0,
            .length = chunk_size * 8,
            .rxlength = 0,
            .override_freq_hz = 0,
            .user = NULL,
            .tx_buffer = data + offset,
            .rx_buffer = NULL
        };
        spi_device_polling_transmit(spiHandle, &t);
        offset += chunk_size;
    }

    gpio_set_level(E_CS_PIN, 1);
}

void BDEpaper::powerOn() {
    if (pwrState) return;

    // Turn peripherals on
    gpio_set_level(HV_CTL_PIN, 1);
    pwrState = true;
}

lv_display_t* BDEpaper::init() {
    spiInit();

    // Create LVGL display
    lv_display_t *disp = lv_display_create(DISPLAY_X, DISPLAY_Y);

    lv_display_set_user_data(disp, this);
    // Run timer before registering callback
    lv_timer_handler();
    lv_display_set_flush_cb(disp, lvgl_flush_cb);

    lv_display_render_mode_t render_mode = LV_DISPLAY_RENDER_MODE_PARTIAL;
    lv_display_set_buffers(disp, lvgl_buf, NULL, lvgl_buf_size, render_mode);

    return disp;
}
