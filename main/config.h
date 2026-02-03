#include "driver/gpio.h"

#define DO_EXPAND(VAL)  VAL ## 1
#define EXPAND(VAL)     DO_EXPAND(VAL)
#define CI              (EXPAND(FW_VERSION) != 1)

#if !defined(DATE_CODE) || !CI
#undef DATE_CODE
#define DATE_CODE "19700101"
#endif

#if !defined(SW_VERSION) || !CI
#undef SW_VERSION
#define SW_VERSION "0.0.0"
#endif

#if !defined(FW_VERSION) || !CI
#undef FW_VERSION
#define FW_VERSION 0x00000001
#endif

#define ENDPOINT_NUMBER 1
#define HEARTBEAT_INTERVAL 60000000

#define E_BUSY_PIN  GPIO_NUM_23
#define E_RST_PIN   GPIO_NUM_22
#define E_DC_PIN    GPIO_NUM_20
#define E_CS_PIN    GPIO_NUM_18
#define E_SCL_PIN   GPIO_NUM_21
#define E_SDA_PIN   GPIO_NUM_19

#define BUTTON_PIN  GPIO_NUM_9
#define ACK_PIN     GPIO_NUM_0