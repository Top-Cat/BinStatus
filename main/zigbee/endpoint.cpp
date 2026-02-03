#include "sys/time.h"

#include "esp_zigbee_core.h"
#include "helpers.h"

#include "endpoint.h"

ZigbeeDevice::ZigbeeDevice(esp_zb_ha_standard_devices_t deviceId, uint8_t endpoint) {
    _endpoint = endpoint;
    _ep_config = {
        .endpoint = _endpoint,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id = (uint16_t) deviceId,
        .app_device_version = 0
    };

    if (!lock) {
        lock = xSemaphoreCreateBinary();
    }
}

void ZigbeeDevice::zbReadTimeCluster(const esp_zb_zcl_attribute_t *attribute) {
    if (attribute->id == ESP_ZB_ZCL_ATTR_TIME_TIME_ID && attribute->data.type == ESP_ZB_ZCL_ATTR_TYPE_UTC_TIME) {
        ESP_LOGV(PTAG, "Time attribute received");
        ESP_LOGV(PTAG, "Time: %lld", *(uint32_t *)attribute->data.value);
        _read_time = *(uint32_t *)attribute->data.value;
        xSemaphoreGive(lock);
    } else if (attribute->id == ESP_ZB_ZCL_ATTR_TIME_TIME_ZONE_ID && attribute->data.type == ESP_ZB_ZCL_ATTR_TYPE_S32) {
        ESP_LOGV(PTAG, "Timezone attribute received");
        ESP_LOGV(PTAG, "Timezone: %d", *(int32_t *)attribute->data.value);
        _read_timezone = *(int32_t *)attribute->data.value;
        xSemaphoreGive(lock);
    }
}

bool ZigbeeDevice::setTime(tm time) {
    esp_zb_zcl_status_t ret = ESP_ZB_ZCL_STATUS_SUCCESS;
    time_t utc_time = mktime(&time) - OneJanuary2000;
    ESP_LOGD(PTAG, "Setting time to %lld", utc_time);
    esp_zb_lock_acquire(portMAX_DELAY);
    ret = esp_zb_zcl_set_attribute_val(_endpoint, ESP_ZB_ZCL_CLUSTER_ID_TIME, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_TIME_TIME_ID, &utc_time, false);
    esp_zb_lock_release();
    if (ret != ESP_ZB_ZCL_STATUS_SUCCESS) {
        ESP_LOGE(PTAG, "Failed to set time: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
        return false;
    }
    return true;
}

bool ZigbeeDevice::setTimezone(int32_t gmt_offset) {
    esp_zb_zcl_status_t ret = ESP_ZB_ZCL_STATUS_SUCCESS;
    ESP_LOGD(PTAG, "Setting timezone to %d", gmt_offset);
    esp_zb_lock_acquire(portMAX_DELAY);
    ret =  esp_zb_zcl_set_attribute_val(_endpoint, ESP_ZB_ZCL_CLUSTER_ID_TIME, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_TIME_TIME_ZONE_ID, &gmt_offset, false);
    esp_zb_lock_release();
    if (ret != ESP_ZB_ZCL_STATUS_SUCCESS) {
        ESP_LOGE(PTAG, "Failed to set timezone: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
        return false;
    }
    return true;
}

tm ZigbeeDevice::getTime(uint8_t endpoint, int32_t short_addr, esp_zb_ieee_addr_t ieee_addr) {
    esp_zb_zcl_read_attr_cmd_t read_req;
    memset(&read_req, 0, sizeof(read_req));

    if (short_addr >= 0) {
        read_req.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
        read_req.zcl_basic_cmd.dst_addr_u.addr_short = (uint16_t)short_addr;
    } else {
        read_req.address_mode = ESP_ZB_APS_ADDR_MODE_64_ENDP_PRESENT;
        memcpy(read_req.zcl_basic_cmd.dst_addr_u.addr_long, ieee_addr, sizeof(esp_zb_ieee_addr_t));
    }

    uint16_t attributes[] = {ESP_ZB_ZCL_ATTR_TIME_TIME_ID};
    read_req.attr_number = ZB_ARRAY_LENGTH(attributes);
    read_req.attr_field = attributes;

    read_req.clusterID = ESP_ZB_ZCL_CLUSTER_ID_TIME;

    read_req.zcl_basic_cmd.dst_endpoint = endpoint;
    read_req.zcl_basic_cmd.src_endpoint = _endpoint;

    _read_time = 0;

    ESP_LOGV(PTAG, "Reading time from endpoint %d", endpoint);
    esp_zb_zcl_read_attr_cmd_req(&read_req);

    if (xSemaphoreTake(lock, ZB_CMD_TIMEOUT) != pdTRUE) {
        ESP_LOGE(PTAG, "Error while reading time");
        return tm();
    }

    time_t unixTime = OneJanuary2000 + _read_time;
    tm timeinfo;
    if (localtime_r(&unixTime, &timeinfo)) {
        // Update RTC
        timeval tv = {unixTime, 0};
        settimeofday(&tv, NULL);

        // Update attribute
        setTime(timeinfo);

        _time_status |= 0x02;
        esp_zb_lock_acquire(portMAX_DELAY);
        esp_zb_zcl_set_attribute_val(_endpoint, ESP_ZB_ZCL_CLUSTER_ID_TIME, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_TIME_TIME_STATUS_ID, &_time_status, false);
        esp_zb_lock_release();

        return timeinfo;
    } else {
        ESP_LOGE(PTAG, "Error while converting time");
        return tm();
    }
}

int32_t ZigbeeDevice::getTimezone(uint8_t endpoint, int32_t short_addr, esp_zb_ieee_addr_t ieee_addr) {
    esp_zb_zcl_read_attr_cmd_t read_req;
    memset(&read_req, 0, sizeof(read_req));

    if (short_addr >= 0) {
        read_req.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
        read_req.zcl_basic_cmd.dst_addr_u.addr_short = (uint16_t)short_addr;
    } else {
        read_req.address_mode = ESP_ZB_APS_ADDR_MODE_64_ENDP_PRESENT;
        memcpy(read_req.zcl_basic_cmd.dst_addr_u.addr_long, ieee_addr, sizeof(esp_zb_ieee_addr_t));
    }

    uint16_t attributes[] = {ESP_ZB_ZCL_ATTR_TIME_TIME_ZONE_ID};
    read_req.attr_number = ZB_ARRAY_LENGTH(attributes);
    read_req.attr_field = attributes;

    read_req.clusterID = ESP_ZB_ZCL_CLUSTER_ID_TIME;

    read_req.zcl_basic_cmd.dst_endpoint = endpoint;
    read_req.zcl_basic_cmd.src_endpoint = _endpoint;

    _read_timezone = 0;

    ESP_LOGV(PTAG, "Reading timezone from endpoint %d", endpoint);
    esp_zb_zcl_read_attr_cmd_req(&read_req);

    //Wait for response or timeout
    if (xSemaphoreTake(lock, ZB_CMD_TIMEOUT) != pdTRUE) {
        ESP_LOGE(PTAG, "Error while reading timezone");
        return 0;
    }
    setTimezone(_read_timezone);
    return _read_timezone;
}

void ZigbeeDevice::removeBoundDevice(uint8_t endpoint, esp_zb_ieee_addr_t ieee_addr) {
    ESP_LOGD(
        PTAG,
        "Attempting to remove device with endpoint %d and IEEE address %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x", endpoint, ieee_addr[7], ieee_addr[6], ieee_addr[5],
        ieee_addr[4], ieee_addr[3], ieee_addr[2], ieee_addr[1], ieee_addr[0]
    );

    for (std::list<zb_device_params_t *>::iterator it = _bound_devices.begin(); it != _bound_devices.end(); ++it) {
        if ((*it)->endpoint == endpoint && memcmp((*it)->ieee_addr, ieee_addr, sizeof(esp_zb_ieee_addr_t)) == 0) {
            ESP_LOGD(PTAG, "Found matching device, removing it");
            _bound_devices.erase(it);
            if (_bound_devices.empty()) {
                _is_bound = false;
            }
            return;
        }
    }
    ESP_LOGW(PTAG, "No matching device found for removal");
}

void ZigbeeDevice::removeBoundDevice(zb_device_params_t *device) {
    if (!device) {
        ESP_LOGE(PTAG, "Invalid device parameters provided");
        return;
    }

    ESP_LOGD(
        PTAG,
        "Attempting to remove device with endpoint %d, short address 0x%04x, IEEE address %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x", device->endpoint,
        device->short_addr, device->ieee_addr[7], device->ieee_addr[6], device->ieee_addr[5], device->ieee_addr[4], device->ieee_addr[3], device->ieee_addr[2],
        device->ieee_addr[1], device->ieee_addr[0]
    );

    for (std::list<zb_device_params_t *>::iterator it = _bound_devices.begin(); it != _bound_devices.end(); ++it) {
        bool endpoint_matches = ((*it)->endpoint == device->endpoint);
        bool short_addr_matches = (device->short_addr != 0xFFFF && (*it)->short_addr == device->short_addr);
        bool ieee_addr_matches = (memcmp((*it)->ieee_addr, device->ieee_addr, sizeof(esp_zb_ieee_addr_t)) == 0);

        if (endpoint_matches && (short_addr_matches || ieee_addr_matches)) {
            ESP_LOGD(PTAG, "Found matching device by %s, removing it", short_addr_matches ? "short address" : "IEEE address");
            _bound_devices.erase(it);
            if (_bound_devices.empty()) {
                _is_bound = false;
            }
            return;
        }
    }
    ESP_LOGW(PTAG, "No matching device found for removal");
}
