#include <stdint.h>

typedef enum {
    E_CMD_PANEL_SETTINGS = 0x00,
    E_CMD_POWER_SETTINGS = 0x01,
    E_CMD_POWER_OFF = 0x02,
    E_CMD_POWER_ON = 0x04,
    E_CMD_DEEP_SLEEP = 0x07,
    E_CMD_DATA_TRASMISSION = 0x10,
    E_CMD_DATA_STOP = 0x11,
    E_CMD_REFRESH = 0x12,
    E_CMD_DATA_TRASMISSION_2 = 0x13,
    E_CMD_AUTO = 0x17,
    E_CMD_PARTIAL_WINDOW = 0x90,
    E_CMD_PARTIAL_ENTER = 0x91,
    E_CMD_PARTIAL_EXIT = 0x92
} PanelCommands;

typedef enum {
    AUTO_SEQUENCE_PON_DRF_POF = 0xA5,
    AUTO_SEQUENCE_PON_DRF_POF_DSLP = 0xA7
} EpaperAuto;

typedef enum {
    RESOLUTION_240x120,
    RESOLUTION_320x160,
    RESOLUTION_400x200,
    RESOLUTION_480x240
} PanelResolution;

typedef enum {
    BLACK_WHITE_RED,
    BLACK_WHITE
} EpaperKwr;

typedef enum {
    SOFT_RESET,
    NO_SOFT_RESET
} EpaperSoftReset;

typedef enum {
    BOOSTER_OFF,
    BOOSTER_ON
} EpaperBooster;

typedef enum {
    LUT_OTP,
    LUT_REGISTER
} EpaperLut;

typedef enum {
    HSCAN_LEFT,
    HSCAN_RIGHT
} EpaperHScan;

typedef enum {
    VSCAN_DOWN,
    VSCAN_UP
} EpaperVScan;

typedef enum {
    VC_LUTZ_NO_EFFECT,
    VC_LUTZ_FLOAT_VCOM
} EpaperVcLutz;

typedef enum {
    NORG_NO_EFFECT,
    NORG_VCOM_GND
} EpaperNorg;

typedef enum {
    TIEG_NO_EFFECT,
    TIEG_VGL_GND
} EpaperTieg;

typedef enum {
    TS_AUTO_NO_EFFECT,
    TS_AUTO_SENSE
} EpaperTsAuto;

typedef enum {
    VCMZ_NO_EFFECT,
    VCMZ_VCOM_FLOAT
} EpaperVcmz;

struct PanelSettings {
    EpaperSoftReset softReset : 1;
    EpaperBooster booster : 1;
    EpaperHScan horizontalScanDir : 1;
    EpaperVScan verticalScanDir : 1;
    EpaperKwr kwr : 1;
    EpaperLut lut : 1;
    PanelResolution resolution : 2;

    EpaperVcLutz vcLutz : 1;
    EpaperNorg norg : 1;
    EpaperTieg tieg : 1;
    EpaperTsAuto tsAuto : 1;
    EpaperVcmz vcmz : 1;
    uint8_t reserved : 3;
};

typedef enum {
    VGH_20V_VGL_N20V,
    VGH_19V_VGL_N19V,
    VGH_18V_VGL_N18V,
    VGH_17V_VGL_N17V,
    VGH_16V_VGL_N16V,
    VGH_15V_VGL_N15V,
    VGH_14V_VGL_N14V,
    VGH_13V_VGL_N13V,
    VGH_12V_VGL_N12V,
    VGH_11V_VGL_N11V,
    VGH_10V_VGL_N10V
} EpaperVghlLv;
