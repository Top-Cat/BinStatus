#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_sntp.h"

#include "lvgl.h"

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
