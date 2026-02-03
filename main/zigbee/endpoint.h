#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <list>
#include "time.h"

#include "esp_zigbee_type.h"
#include "zcl/esp_zigbee_zcl_common.h"
#include "zcl/esp_zigbee_zcl_core.h"

#define ZB_CMD_TIMEOUT 10000
#define ZB_ARRAY_LENGTH(arr) (sizeof(arr) / sizeof(arr[0]))

typedef struct zb_device_params_s {
    esp_zb_ieee_addr_t ieee_addr;
    uint8_t endpoint;
    uint16_t short_addr;
} zb_device_params_t;

class ZigbeeDevice {
    public:
        ZigbeeDevice(esp_zb_ha_standard_devices_t deviceId, uint8_t endpoint = 10);
        ~ZigbeeDevice() {}

        void zbReadTimeCluster(const esp_zb_zcl_attribute_t *attribute);

        std::list<zb_device_params_t *> getBoundDevices() const {
            return _bound_devices;
        }

        virtual void addBoundDevice(zb_device_params_t *device) {
            _bound_devices.push_back(device);
            _is_bound = true;
        }

        virtual void removeBoundDevice(uint8_t endpoint, esp_zb_ieee_addr_t ieee_addr);
        virtual void removeBoundDevice(zb_device_params_t *device);

        virtual void clearBoundDevices() {
            _bound_devices.clear();
            _is_bound = false;
        }
    protected:
        uint8_t _endpoint;
        esp_zb_endpoint_config_t _ep_config;
        esp_zb_cluster_list_t* _cluster_list;
        uint8_t _time_status;
        uint32_t OneJanuary2000 = 946684800;

        struct tm getTime(uint8_t endpoint = 1, int32_t short_addr = 0x0000, esp_zb_ieee_addr_t ieee_addr = {0});
        int32_t getTimezone(uint8_t endpoint = 1, int32_t short_addr = 0x0000, esp_zb_ieee_addr_t ieee_addr = {0});

        virtual esp_zb_cluster_list_t* createClusters() {
            return NULL;
        }
        virtual void zbAttributeSet(const esp_zb_zcl_set_attr_value_message_t *message) {}
        virtual void zbCustomCommand(const esp_zb_zcl_custom_cluster_command_message_t *message) {};
        virtual void zbAttributeRead(uint16_t cluster_id, const esp_zb_zcl_attribute_t *attribute, uint8_t src_endpoint, esp_zb_zcl_addr_t src_address) {}

        std::list<zb_device_params_t *> _bound_devices;
        bool _is_bound;
    private:
        const char *PTAG = "TC-ZBD";
        time_t _read_time;
        int32_t _read_timezone;
        SemaphoreHandle_t lock;

        bool setTime(tm time);
        bool setTimezone(int32_t gmt_offset);

    friend class ZigbeeCore;
    friend class ZigbeeHandlers;
};
