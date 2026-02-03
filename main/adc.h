#include "config.h"

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"

class BSADC {
    public:
        void init();
        bool getValue(uint8_t &out);
    private:
        adc_oneshot_unit_handle_t adc_handle;
        adc_cali_handle_t cali_handle;

        void enable();
        void disable();
};

extern BSADC adc;
