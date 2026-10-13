# VoCat 配网热点使用固定名称。上游组件目前只支持“前缀 + MAC 后缀”，
# 因此在 build 中生成一份仅替换 GetSsid() 的构建副本；托管组件保持原样。
# AP 广播、WifiManager::GetApSsid()、屏幕文字及二维码均读取该方法。
function(vocat_configure_ap_name)
    set(vocat_ap_ssid "XChatbot-esp32s3")
    idf_component_get_property(wifi_dir 78__esp-wifi-connect COMPONENT_DIR)
    idf_component_get_property(wifi_lib 78__esp-wifi-connect COMPONENT_LIB)
    set(upstream_source "${wifi_dir}/wifi_configuration_ap.cc")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${upstream_source}")
    file(READ "${upstream_source}" source)
    string(REPLACE "\r\n" "\n" source "${source}")

    # 精确核对当前依赖实现。未来升级若修改此方法，应人工核对适配，
    # 避免补丁失效后热点和二维码悄悄恢复为旧名称。
    set(original_method [=[std::string WifiConfigurationAp::GetSsid()
{
    // Get MAC and use it to generate a unique SSID
    uint8_t mac[6];
#if CONFIG_IDF_TARGET_ESP32P4
    esp_wifi_get_mac(WIFI_IF_AP, mac);
#else
    ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP));
#endif
    char ssid[32];
    snprintf(ssid, sizeof(ssid), "%s-%02X%02X", ssid_prefix_.c_str(), mac[4], mac[5]);
    return std::string(ssid);
}]=])
    string(FIND "${source}" "${original_method}" method_position)
    if(method_position EQUAL -1)
        message(FATAL_ERROR "VoCat AP name: upstream GetSsid() changed; review wifi_ap_name.cmake")
    endif()
    string(REPLACE "${original_method}"
        "std::string WifiConfigurationAp::GetSsid()\n{\n    return \"${vocat_ap_ssid}\";\n}"
        patched_source "${source}")

    set(generated_source "${CMAKE_BINARY_DIR}/vocat_wifi/wifi_configuration_ap.cc")
    file(GENERATE OUTPUT "${generated_source}" CONTENT "${patched_source}")
    set_source_files_properties("${generated_source}" PROPERTIES GENERATED TRUE)
    get_target_property(wifi_sources "${wifi_lib}" SOURCES)
    list(LENGTH wifi_sources original_count)
    list(FILTER wifi_sources EXCLUDE REGEX "(^|/)wifi_configuration_ap\\.cc$")
    list(LENGTH wifi_sources remaining_count)
    math(EXPR expected_count "${original_count} - 1")
    if(NOT remaining_count EQUAL expected_count)
        message(FATAL_ERROR "VoCat AP name: expected exactly one upstream AP source")
    endif()
    list(APPEND wifi_sources "${generated_source}")
    set_property(TARGET "${wifi_lib}" PROPERTY SOURCES "${wifi_sources}")
    message(STATUS "VoCat provisioning hotspot: ${vocat_ap_ssid}")
endfunction()

vocat_configure_ap_name()
