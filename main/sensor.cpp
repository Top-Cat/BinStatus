#include "esp_zigbee_core.h"
#include "esp_zigbee_cluster.h"
#include "esp_zigbee_attribute.h"
#include "zcl/esp_zigbee_zcl_power_config.h"

#include "esp_log.h"
#include "esp_timer.h"

#include "config.h"
#include "zigbee/helpers.h"

#include "sensor.h"

const char* swBuildId = SW_VERSION;
const char* dateCode = DATE_CODE;

void ZigbeeSensor::createBasicCluster(esp_zb_cluster_list_t* cluster_list) {
    esp_zb_attribute_list_t *basic_cluster = esp_zb_basic_cluster_create(&basic_cfg);
    esp_zb_cluster_list_add_basic_cluster(cluster_list, basic_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

    char zigbee_swid[16];
    fill_zcl_string(zigbee_swid, sizeof(zigbee_swid), swBuildId);
    esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_SW_BUILD_ID, (void*) &zigbee_swid);

    char zigbee_datecode[50];
    fill_zcl_string(zigbee_datecode, sizeof(zigbee_datecode), dateCode);
    esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_DATE_CODE_ID, (void*) &zigbee_datecode);

    uint16_t stack_version = 0x30;
    esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_STACK_VERSION_ID, (void*) &stack_version);

    char zb_name[50];
    fill_zcl_string(zb_name, sizeof(zb_name), manufacturer_name);
    esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, (void*) &zb_name);

    char zb_model[50];
    fill_zcl_string(zb_model, sizeof(zb_model), model_identifier);
    esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, (void*) &zb_model);
}


void ZigbeeSensor::createIdentifyCluster(esp_zb_cluster_list_t* cluster_list) {
    esp_zb_cluster_list_add_identify_cluster(cluster_list, esp_zb_identify_cluster_create(&identify_cfg), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
}

void ZigbeeSensor::createTimeCluster(esp_zb_cluster_list_t* cluster_list) {
    esp_zb_attribute_list_t *time_cluster_server = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_TIME);
    esp_zb_time_cluster_add_attr(time_cluster_server, ESP_ZB_ZCL_ATTR_TIME_TIME_ZONE_ID, (void *) &_gmt_offset);
    esp_zb_time_cluster_add_attr(time_cluster_server, ESP_ZB_ZCL_ATTR_TIME_TIME_ID, (void *) &_utc_time);
    esp_zb_time_cluster_add_attr(time_cluster_server, ESP_ZB_ZCL_ATTR_TIME_TIME_STATUS_ID, (void *) &_time_status);

    esp_zb_attribute_list_t *time_cluster_client = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_TIME);
    esp_zb_cluster_list_add_time_cluster(cluster_list, time_cluster_server, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_time_cluster(cluster_list, time_cluster_client, ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE);
}

void ZigbeeSensor::createCustomClusters(esp_zb_cluster_list_t* cluster_list) {
    esp_zb_attribute_list_t *bin_cluster = esp_zb_zcl_attr_list_create(MS_BIN_CLUSTER_ID);
    esp_zb_cluster_list_add_custom_cluster(cluster_list, bin_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
}

void ZigbeeSensor::createPowerCluster(esp_zb_cluster_list_t* cluster_list) {
    uint8_t battery_percentage = 0xff;
    uint8_t battery_voltage = 0xff;

    esp_zb_attribute_list_t *power_config_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_POWER_CONFIG);
    esp_zb_power_config_cluster_add_attr(power_config_cluster, ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_REMAINING_ID, (void *)&battery_percentage);
    esp_zb_power_config_cluster_add_attr(power_config_cluster, ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_ID, (void *)&battery_voltage);
    esp_zb_cluster_list_add_power_config_cluster(cluster_list, power_config_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
}

void ZigbeeSensor::createOtaCluster(esp_zb_cluster_list_t* cluster_list) {
    esp_zb_attribute_list_t *ota_cluster = esp_zb_ota_cluster_create(&ota_cluster_cfg);

    esp_zb_zcl_ota_upgrade_client_variable_t variable_config = {};
    variable_config.timer_query = ESP_ZB_ZCL_OTA_UPGRADE_QUERY_TIMER_COUNT_DEF;
    variable_config.hw_version = 3;
    variable_config.max_data_size = 223;

    uint16_t ota_upgrade_server_addr = 0xffff;
    uint8_t ota_upgrade_server_ep = 0xff;

    esp_zb_ota_cluster_add_attr(ota_cluster, ESP_ZB_ZCL_ATTR_OTA_UPGRADE_CLIENT_DATA_ID, (void *)&variable_config);
    esp_zb_ota_cluster_add_attr(ota_cluster, ESP_ZB_ZCL_ATTR_OTA_UPGRADE_SERVER_ADDR_ID, (void *)&ota_upgrade_server_addr);
    esp_zb_ota_cluster_add_attr(ota_cluster, ESP_ZB_ZCL_ATTR_OTA_UPGRADE_SERVER_ENDPOINT_ID, (void *)&ota_upgrade_server_ep);

    esp_zb_cluster_list_add_ota_cluster(cluster_list, ota_cluster, ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE);
}

static void findOTAServer(esp_zb_zdp_status_t zdo_status, uint16_t addr, uint8_t endpoint, void *user_ctx) {
    if (zdo_status == ESP_ZB_ZDP_STATUS_SUCCESS) {
        esp_zb_ota_upgrade_client_query_interval_set(*((uint8_t *)user_ctx), OTA_UPGRADE_QUERY_INTERVAL);
        esp_zb_ota_upgrade_client_query_image_req(addr, endpoint);
        ESP_LOGI("FIND_OTA", "Query OTA upgrade from server endpoint: %d after %d seconds", endpoint, OTA_UPGRADE_QUERY_INTERVAL);
    } else {
        ESP_LOGW("FIND_OTA", "No OTA Server found");
    }
}

void ZigbeeSensor::requestOTA() {
    esp_zb_zdo_match_desc_req_param_t req;
    uint16_t cluster_list[] = {ESP_ZB_ZCL_CLUSTER_ID_OTA_UPGRADE};

    req.addr_of_interest = 0x0000;
    req.dst_nwk_addr = 0x0000;
    req.num_in_clusters = 1;
    req.num_out_clusters = 0;
    req.profile_id = ESP_ZB_AF_HA_PROFILE_ID;
    req.cluster_list = cluster_list;
    esp_zb_lock_acquire(portMAX_DELAY);
    if (esp_zb_bdb_dev_joined()) {
        esp_zb_zdo_match_cluster(&req, findOTAServer, &_endpoint);
    }
    esp_zb_lock_release();
}

esp_zb_cluster_list_t* ZigbeeSensor::createClusters() {
    esp_zb_cluster_list_t *cluster_list = esp_zb_zcl_cluster_list_create();

    createBasicCluster(cluster_list);
    createIdentifyCluster(cluster_list);
    createPowerCluster(cluster_list);
    createOtaCluster(cluster_list);
    createTimeCluster(cluster_list);
    createCustomClusters(cluster_list);

    return cluster_list;
}

void ZigbeeSensor::zbCustomCommand(const esp_zb_zcl_custom_cluster_command_message_t *message) {
    if (message->info.cluster != MS_BIN_CLUSTER_ID ||
        message->info.command.id != CMD_SET_DISPLAY_TIMES) {
        return;
    }

    if (message->data.size != sizeof(set_display_times_cmd_t)) {
        ESP_LOGW(TAG, "Invalid payload length: %d", message->data.size);
        return;
    }

    set_display_times_cmd_t payload;
    memcpy(&payload, message->data.value, sizeof(payload));
    ESP_LOGI(TAG, "New times: %lu %lu %lu", payload.black, payload.green, payload.brown);

    prefs.putUInt(NVS_BLACK, payload.black);
    prefs.putUInt(NVS_GREEN, payload.green);
    prefs.putUInt(NVS_BROWN, payload.brown);
    _on_bin_update(
        false,
        payload.black + OneJanuary2000,
        payload.green + OneJanuary2000, 
        payload.brown + OneJanuary2000
    );

    return;
}

void ZigbeeSensor::setBattery(uint8_t battery, uint8_t percentage) {
    esp_zb_zcl_set_attribute_val(
        _endpoint,
        ESP_ZB_ZCL_CLUSTER_ID_POWER_CONFIG,
        ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_REMAINING_ID,
        &percentage,
        false
    );

    esp_zb_zcl_set_attribute_val(
        _endpoint,
        ESP_ZB_ZCL_CLUSTER_ID_POWER_CONFIG,
        ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_ID,
        &battery,
        false
    );
}

void ZigbeeSensor::onBinUpdate(void (*callback)(bool, time_t, time_t, time_t)) {
    _on_bin_update = callback;
}

void ZigbeeSensor::init() {
    prefs.begin(NVS_NAMESPACE, false);
    uint32_t blackBinTime = prefs.getUInt(NVS_BLACK, 0);
    uint32_t greenBinTime = prefs.getUInt(NVS_GREEN, 0);
    uint32_t brownBinTime = prefs.getUInt(NVS_BROWN, 0);

    _on_bin_update(
        true,
        blackBinTime + OneJanuary2000,
        greenBinTime + OneJanuary2000,
        brownBinTime + OneJanuary2000
    );
}

ZigbeeSensor::~ZigbeeSensor() {
    prefs.end();
}

void ZigbeeSensor::onConnect() {
    // Update any state from prefs
}

ZigbeeSensor::ZigbeeSensor(uint8_t endpoint) : ZigbeeDevice(ESP_ZB_HA_SIMPLE_SENSOR_DEVICE_ID, endpoint) {    
    basic_cfg = {
        .zcl_version = ESP_ZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE,
        .power_source = ESP_ZB_ZCL_BASIC_POWER_SOURCE_BATTERY
    };
    identify_cfg = {
        .identify_time = ESP_ZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE
    };
    ota_cluster_cfg = {
        .ota_upgrade_file_version = FW_VERSION,
        .ota_upgrade_manufacturer = 0x1001,
        .ota_upgrade_image_type = 0x1012,
        .ota_min_block_reque = 0,
        .ota_upgrade_file_offset = 0,
        .ota_upgrade_downloaded_file_ver = ESP_ZB_ZCL_OTA_UPGRADE_DOWNLOADED_FILE_VERSION_DEF_VALUE,
        .ota_upgrade_server_id = 0,
        .ota_image_upgrade_status = 0
    };

    _cluster_list = createClusters();
}

esp_err_t doReport(uint8_t _endpoint, esp_zb_zcl_cluster_id_t cluster, uint16_t attr) {
    // Must already have zb lock
    esp_zb_zcl_report_attr_cmd_t report_attr_cmd = {
        {
            .dst_addr_u = {},
            .dst_endpoint = 0,
            .src_endpoint = _endpoint
        },
        ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT,
        cluster,
        {0, ESP_ZB_ZCL_CMD_DIRECTION_TO_CLI, 0},
        ESP_ZB_ZCL_ATTR_NON_MANUFACTURER_SPECIFIC,
        attr
    };

    return esp_zb_zcl_report_attr_cmd_req(&report_attr_cmd);
}

bool ZigbeeSensor::report() {
    esp_zb_lock_acquire(portMAX_DELAY);
    esp_err_t ret = doReport(_endpoint, ESP_ZB_ZCL_CLUSTER_ID_POWER_CONFIG, ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_REMAINING_ID) |
        doReport(_endpoint, ESP_ZB_ZCL_CLUSTER_ID_POWER_CONFIG, ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_ID);
    esp_zb_lock_release();

    return ret == ESP_OK;
}

tm ZigbeeSensor::fetchTime() {
    time_t unixTime;
    time(&unixTime);

    // Unix time is large enough to be correct and has been updated in the last day
    if (unixTime > 86400 * 30 && esp_timer_get_time() - lastTime <= 86400000000) {
        tm time;
        localtime_r(&unixTime, &time);
        return time;
    }

    return getTime(_endpoint);
}
