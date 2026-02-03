idf_component_register(
    SRC_DIRS  "." "./zigbee" INCLUDE_DIRS "." "./zigbee"
    PRIV_REQUIRES nvs_flash esp_driver_uart ieee802154 esp_timer app_update esp_delta_ota
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    DATE_CODE=\"${DATE_CODE}\"
    SW_VERSION=\"${SW_VERSION}\"
    FW_VERSION=${FW_VERSION}
)
