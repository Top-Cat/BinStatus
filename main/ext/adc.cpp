#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "adc.h"

BSADC adc;

void BSADC::init() {
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
        .clk_src = (adc_oneshot_clk_src_t) 0,
        .ulp_mode = ADC_ULP_MODE_DISABLE
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));

    adc_oneshot_chan_cfg_t config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, (adc_channel_t) BAT_ADC_PIN, &config));

    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id = ADC_UNIT_1,
        .chan = ADC_CHANNEL_3,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12
    };
    ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&cali_cfg, &cali_handle));
}

void BSADC::enable() {
    gpio_set_direction(BAT_LOW_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(BAT_LOW_PIN, 0);
}

void BSADC::disable() {
    gpio_set_direction(BAT_LOW_PIN, GPIO_MODE_INPUT);
}

bool BSADC::getValue(uint8_t &out, uint8_t &percent) {
    enable();

    vTaskDelay(100 / portTICK_PERIOD_MS);

    int adcValue;
    bool res = adc_oneshot_get_calibrated_result(adc_handle, cali_handle, (adc_channel_t) BAT_ADC_PIN, &adcValue) == ESP_OK;
    if (res) {
        out = (uint8_t) ((adcValue * 3) + 50) / 100; // + 50 makes rounding correct
        percent = (uint8_t) ((adcValue * 3) - 3500) / 3.5;
    }

    disable();

    return res;
}
