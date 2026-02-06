#include "lvgl.h"
#include "driver/spi_master.h"

#include "estructs.h"

#define DISPLAY_X 416
#define DISPLAY_Y 240
#define DISPLAY_B 1 // bits per pixel
#define SPI_MAX_CHUNK_SIZE 4096
#define SPI_SPEED 4000000

#define EPD_PIXEL_BLACK     0x0
#define EPD_PIXEL_WHITE     0x1
#define EPD_PIXEL_YELLOW    0x2
#define EPD_PIXEL_RED       0x3
#define EPD_PIXEL_ORANGE    0x4
#define EPD_PIXEL_BLUE      0x5
#define EPD_PIXEL_GREEN     0x6

class BDEpaper {
    public:
        BDEpaper();
        ~BDEpaper() {};

        lv_display_t* init();
        void power();
        void flush(lv_display_t *disp, const lv_area_t *area, uint8_t *color_p);
    private:
        uint8_t* rgb_buf;
        uint8_t* lvgl_buf;
        uint8_t* framebuffer;
        bool fb_dirty;

        uint32_t lvgl_buf_size;
        uint32_t buffer_size;
        bool partial_render_mode;

        // LVGL handling
        bool initialized;
        void applyDithering();
        void flushDisplay();

        // SPI
        spi_device_handle_t spiHandle;
        esp_err_t spiInit();
        void spiR();
        void wait(uint32_t timeout_ms = 0);
        void spiC(PanelCommands cmd);
        void spiD(uint8_t data);
        void spiBulk(const uint8_t *data, uint32_t len);

        // Epaper specific
        PanelSettings panelSettings;
        bool pwrState;
        void panelInit();
        void powerOn();
        void panelWrite(const uint8_t *data, uint32_t len);
        void panelUpdate();
};

extern BDEpaper epaper;
