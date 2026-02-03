#include "esp_zigbee_type.h"

#include "zigbee/endpoint.h"
#include "prefs.h"

#define MANUFACTURER_CODE        0x1234

#define MS_BIN_CLUSTER_ID        0xFC12
#define CMD_SET_DISPLAY_TIMES    0x01
/*#define ATTR_AMBER_LEVEL_ID      0x0001
#define ATTR_WARM_WHITE_LEVEL_ID 0x0002
#define ATTR_COOL_WHITE_LEVEL_ID 0x0003
#define ATTR_LED_COUNT_ID        0x0010
#define ATTR_ANIMATION_ID        0x0011
#define ATTR_SPEED_ID            0x0012

#define MS_LUX_CLUSTER_ID         0xFC11
#define ATTR_INHIBIT_THRESHOLD_ID 0x0001*/

#define OTA_UPGRADE_QUERY_INTERVAL (1 * 60)
#define NVS_NAMESPACE         "config"
#define NVS_BLACK             "black"
#define NVS_GREEN             "green"
#define NVS_BROWN             "brown"

typedef struct {
    uint32_t black;
    uint32_t green;
    uint32_t brown;
} set_display_times_cmd_t;

class ZigbeeSensor : public ZigbeeDevice {
    public:
        ZigbeeSensor(uint8_t endpoint);
        ~ZigbeeSensor();

        void zbCustomCommand(const esp_zb_zcl_custom_cluster_command_message_t *message) override;

        void init();

        void onConnect();
        void onBinUpdate(void (*callback)(time_t, time_t, time_t));
        void requestOTA();
        bool report();

        tm fetchTime();
    private:
        const char* TAG = "TC-ZBS";
        const char* manufacturer_name = "TC";
        const char* model_identifier = "BinStatus";

        time_t _utc_time = 0;
        int32_t _gmt_offset = 0;
        uint64_t lastTime = 0;

        Preferences prefs;

        esp_zb_basic_cluster_cfg_t basic_cfg;
        esp_zb_identify_cluster_cfg_t identify_cfg;
        esp_zb_ota_cluster_cfg_t ota_cluster_cfg;

        esp_zb_cluster_list_t* createClusters() override;
        void createBasicCluster(esp_zb_cluster_list_t* cluster_list);
        void createIdentifyCluster(esp_zb_cluster_list_t* cluster_list);
        void createPowerCluster(esp_zb_cluster_list_t* cluster_list);
        void createOtaCluster(esp_zb_cluster_list_t* cluster_list);
        void createTimeCluster(esp_zb_cluster_list_t* cluster_list);
        void createCustomClusters(esp_zb_cluster_list_t* cluster_list);

        void (*_on_bin_update)(time_t, time_t, time_t);
};
