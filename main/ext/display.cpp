#include "display.h"

#include "epaper.h"
#include "../config.h"
#include "../images.h"

#include <algorithm>
#include "esp_timer.h"

uint8_t daysGreen = 9;
uint8_t daysBlack = 2;
uint8_t daysBrown = 5;

const lv_image_dsc_t* binImg[] = { &trash, &recycle, &gardenWaste };
uint32_t imageScale[] = { 750, 800, 1024, 525, 560, 716 };
uint32_t imagePivot[] = { 9, 10, 8 };
const char * binName[] = {"BLACK", "GREEN", "BROWN"};

bool DisplayDriver::lock(int timeout_ms) {
    const TickType_t timeout_ticks = (timeout_ms == -1) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTake(lvgl_mux, timeout_ticks) == pdTRUE;
}

void DisplayDriver::unlock() {
    xSemaphoreGive(lvgl_mux);
}

void DisplayDriver::drawRow(Bins bin, uint8_t row, time_t when) {
    uint16_t w = lv_display_get_horizontal_resolution(disp);
    lv_obj_t *scr = lv_screen_active();
    uint8_t offset = 93 + (row * 75);

    // Separator line
    lv_point_precise_t* lp = &line_points[row * 2];
    lp[0] = {8, offset};
    lp[1] = {w - 8, offset};

    lv_obj_t *line = lv_line_create(scr);
    lv_line_set_points(line, lp, 2);
    lv_obj_set_style_line_width(line, 1, 0);
    lv_obj_set_style_line_color(line, lv_color_black(), 0);

    imgs[row + 1] = lv_image_create(scr);
    lv_obj_align(imgs[row + 1], LV_ALIGN_TOP_LEFT, 30, offset + 17);

    infoText[row + 1] = lv_label_create(scr);
    lv_obj_set_style_text_font(infoText[row + 1], &lv_font_montserrat_20, 0);
    lv_obj_align(infoText[row + 1], LV_ALIGN_TOP_LEFT, 100, offset + 27);

    dayText[row + 1] = lv_label_create(scr);
    lv_obj_set_style_text_font(dayText[row + 1], &lv_font_montserrat_20, 0);
    lv_obj_align(dayText[row + 1], LV_ALIGN_TOP_RIGHT, -30, offset + 27);
}

void DisplayDriver::createUi() {
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_white(), 0);

    imgs[0] = lv_image_create(scr);
    lv_obj_align(imgs[0], LV_ALIGN_TOP_LEFT, 30, 15);

    nextHead = lv_label_create(scr);
    lv_obj_set_style_text_font(nextHead, &lv_font_montserrat_20, 0);
    lv_obj_align(nextHead, LV_ALIGN_TOP_LEFT, 90, 20);

    infoText[0] = lv_label_create(scr);
    lv_obj_set_style_text_font(infoText[0], &lv_font_montserrat_14, 0);
    lv_obj_align(infoText[0], LV_ALIGN_TOP_LEFT, 100, 50);

    dayText[0] = lv_label_create(scr);
    lv_obj_set_style_text_font(dayText[0], &lv_font_montserrat_48, 0);
    lv_obj_align(dayText[0], LV_ALIGN_TOP_RIGHT, -30, 10);

    lv_obj_t* daysLabel = lv_label_create(scr);
    lv_label_set_text(daysLabel, "days");
    lv_obj_set_style_text_font(daysLabel, &lv_font_montserrat_14, 0);
    lv_obj_align(daysLabel, LV_ALIGN_TOP_RIGHT, -30, 55);

    drawRow(GREEN, 0, 0);
    drawRow(BROWN, 1, 0);
}

void DisplayDriver::init() {
    lv_init();
    disp = epaper.init();

    // Create LVGL task
    lvgl_mux = xSemaphoreCreateMutex();

    // Create UI
    if (lock(-1)) {
        createUi();
        unlock();
    }
}

void DisplayDriver::render() {
    updateUi();
    lv_timer_handler(); // Ensure screen is updated

    epaper.power();
    if (lock(100)) {
        lv_obj_invalidate(lv_screen_active());
        lv_refr_now(disp);
        unlock();
    }
}

void DisplayDriver::updateUi() {
    time_t now;
    time(&now);

    if (!imgs[0] || !lock(1000)) return;

    for (uint8_t i = 0; i < 3; i++) {
        uint16_t dayCount = ((times[i] - now) / 86400) + 1;

        lv_image_set_src(imgs[i], binImg[bins[i]]);
        lv_image_set_pivot(imgs[i], imagePivot[bins[i]], 0);
        lv_image_set_scale(imgs[i], imageScale[bins[i] + (i == 0 ? 0 : 3)]);

        tm* ti = localtime(&times[i]);
        if (i == 0) {
            lv_label_set_text_fmt(dayText[i], "%d", dayCount);
            lv_label_set_text_fmt(infoText[i], "On %02d/%02d/%04d", ti->tm_mday, ti->tm_mon + 1, ti->tm_year + 1900);
            lv_label_set_text_fmt(nextHead, "Next collection: %s", binName[bins[i]]);
        } else {
            lv_label_set_text_fmt(dayText[i], "[%d]", dayCount);
            lv_label_set_text_fmt(infoText[i], "%s on %02d/%02d/%04d", binName[bins[i]], ti->tm_mday, ti->tm_mon + 1, ti->tm_year + 1900);
        }
    }

    unlock();
}

void DisplayDriver::updateTimes(time_t black, time_t green, time_t brown) {
    bins[0] = Bins::BLACK;
    bins[1] = Bins::GREEN;
    bins[2] = Bins::BROWN;
    times[0] = black;
    times[1] = green;
    times[2] = brown;

    for (uint8_t i = 0; i < 2; ++i) {
        for (uint8_t j = i+1; j < 3; ++j) {
            if (times[i] > times[j]) {
                std::swap(times[i], times[j]);
                std::swap(bins[i], bins[j]);
            }
        }
    }
}

DisplayDriver eink;
