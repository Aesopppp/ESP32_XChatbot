/*
 * 可选的 BLE Blufi 配网实现，负责蓝牙会话、Wi-Fi 扫描和配置交换。
 * 支持的蓝牙协议栈由 Kconfig 选择；不是通过设备 AP 打开的网页配网界面。
 * 安全上下文管理密钥协商、AES 和校验，停止配网时需释放相应协议栈 / 加密资源。
 */

#pragma once

#include <cassert>
#include <cstring>
#include <vector>
#include "esp_blufi_api.h"
#include "esp_err.h"
#include "esp_idf_version.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "psa/crypto.h"
#include "wifi_manager.h"

class Blufi {
public:
    /**
     * @brief Get the singleton instance of the Blufi class.
     */
    static Blufi &GetInstance();

    /**
     * @brief Start WiFi scan for Blufi provisioning
     * This method intelligently handles WiFi scanning based on current WiFi state:
     * - If WiFi config mode is active, it uses the existing scan results from WifiConfigurationAp
     * - Otherwise, it performs a dedicated scan without interfering with normal WiFi operations
     * @return true if a scan was started (or was already in progress); false on failure.
     */
    // 配网模式下复用 AP 扫描缓存，否则独立扫描；true 表示已发起或已有扫描，不代表已返回列表。
    bool start_wifi_scan();

    /**
     * @brief Initializes the Bluetooth controller, host, and Blufi profile.
     * This is the main entry point to start the Blufi process.
     * @return ESP_OK on success, otherwise an error code.
     */
    // 启动蓝牙控制器、所选协议栈与 Blufi profile；调用方应检查 ESP_OK。
    esp_err_t init();

    /**
     * @brief Deinitializes Blufi and the Bluetooth stack.
     * @return ESP_OK on success, otherwise an error code.
     */
    // 按所选协议栈停止 host / controller；安全上下文释放由相应事件与析构路径处理。
    esp_err_t deinit();

    // Delete copy constructor and assignment operator for singleton
    Blufi(const Blufi &) = delete;

    Blufi &operator=(const Blufi &) = delete;

private:
    bool inited_ = false;

    Blufi();

    ~Blufi();

    // Initialization logic
    static esp_err_t _controller_init();

    static esp_err_t _controller_deinit();

    static esp_err_t _host_init();

    static esp_err_t _host_deinit();

    static esp_err_t _gap_register_callback();

    static esp_err_t _host_and_cb_init();

    void _security_init();

    void _security_deinit();

    void _dh_negotiate_data_handler(uint8_t *data, int len, uint8_t **output_data, int *output_len,
                                    bool *need_free);

    int _aes_encrypt(uint8_t iv8, uint8_t *crypt_data, int crypt_len);

    int _aes_decrypt(uint8_t iv8, uint8_t *crypt_data, int crypt_len);

    static uint16_t _crc_checksum(uint8_t iv8, uint8_t *data, int len);

    void _handle_event(esp_blufi_cb_event_t event, esp_blufi_cb_param_t *param);

    static int _get_softap_conn_num();

    // WiFi scan methods
    void _send_wifi_list();
    void _start_dedicated_wifi_scan();
    static void _wifi_scan_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
                                         void *event_data);

    // These C-style functions are registered with ESP-IDF and call the corresponding instance
    // methods.

    static void _event_callback_trampoline(esp_blufi_cb_event_t event, esp_blufi_cb_param_t *param);

    static void _negotiate_data_handler_trampoline(uint8_t *data, int len, uint8_t **output_data,
                                                   int *output_len, bool *need_free);

    static int _encrypt_func_trampoline(uint8_t iv8, uint8_t *crypt_data, int crypt_len);

    static int _decrypt_func_trampoline(uint8_t iv8, uint8_t *crypt_data, int crypt_len);

    static uint16_t _checksum_func_trampoline(uint8_t iv8, uint8_t *data, int len);

#ifdef CONFIG_BT_NIMBLE_ENABLED
    static void _nimble_on_reset(int reason);
    static void _nimble_on_sync();
    static void _nimble_host_task(void *param);
#endif

    // Security context, formerly blufi_sec struct
    // 本次蓝牙会话的协商数据、PSA AES 密钥与操作上下文，不是 ACOS token。
    struct BlufiSecurity {
#define DH_PARAM_LEN_MAX 1024
#define DH_SELF_PUB_KEY_LEN 384
        uint8_t self_public_key[DH_SELF_PUB_KEY_LEN];
#define SHARE_KEY_LEN 384
        uint8_t share_key[SHARE_KEY_LEN];
        size_t share_len;
#define PSK_LEN 32
        uint8_t psk[PSK_LEN];
        uint8_t *dh_param;
        int dh_param_len;
        psa_key_id_t aes_key;
        psa_cipher_operation_t enc_operation;
        psa_cipher_operation_t dec_operation;
    };

    BlufiSecurity *m_sec;

    // State variables
    wifi_config_t m_sta_config{};
    bool m_ble_is_connected;
    bool m_sta_connected;
    bool m_sta_got_ip;
    bool m_provisioned;
    bool m_deinited;
    uint8_t m_sta_bssid[6]{};
    uint8_t m_sta_ssid[32]{};
    int m_sta_ssid_len;
    bool m_sta_is_connecting;
    esp_blufi_extra_info_t m_sta_conn_info{};

    // WiFi scan related
    std::vector<wifi_ap_record_t> m_ap_records;
    bool m_scan_in_progress = false;
    // When true, scan results are stored in m_ap_records on scan completion.
    // Cleared during connect-to-AP so that the connect-time scan does not
    // overwrite the cache with results gathered for connection purposes.
    // 配网列表扫描才更新缓存，连接 AP 所触发的扫描不覆盖用户请求的列表。
    bool m_scan_should_save_ssid = true;
    // When true, the next scan-done event responds to a pending GET_WIFI_LIST
    // request from the App. Set by the GET_WIFI_LIST handler when no cache is
    // available or a scan is already in flight; cleared by the scan-done
    // handler after dispatching the response.
    // 记录待回应的手机列表请求；扫描完成回调发送后清除，避免没有缓存时提前返回空列表。
    bool m_send_list_after_scan = false;
};
