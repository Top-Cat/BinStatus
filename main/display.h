#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_sntp.h"

#include "lvgl.h"

// LVGL configuration
#define LVGL_TICK_PERIOD_MS     5
#define LVGL_TASK_MAX_DELAY_MS  500
#define LVGL_TASK_MIN_DELAY_MS  100

typedef enum {
    BLACK, GREEN, BROWN
} Bins;

class DisplayDriver {
    public:
        void init();
        void render();

        void updateTimes(time_t black, time_t green, time_t brown);

        bool lock(int timeout_ms);
        void unlock();
    private:
        lv_display_t *disp;
        SemaphoreHandle_t lvgl_mux = NULL;

        Bins bins[3];
        time_t times[3];

        uint8_t margin = 8;
        lv_point_precise_t line_points[4];

        // UI
        lv_obj_t* imgs[3];
        lv_obj_t* dayText[3];
        lv_obj_t* infoText[3];
        lv_obj_t* nextHead;

        void createUi();
        void updateUi();
        void drawRow(Bins bin, uint8_t row, time_t when);
};

extern DisplayDriver eink;
