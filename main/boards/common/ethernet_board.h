/*
 * 有线以太网板级网络基础，统一链路 / 获取 IP 事件与网络接口。
 * 属于保留的扩展实现；当前 VoCat 默认使用 Wi-Fi，不会因文件存在就启动以太网硬件。
 */

#ifndef ETHERNET_BOARD_H
#define ETHERNET_BOARD_H

#include "board.h"

#include <cstdint>

class EthernetBoard : public Board {
protected:
    void* eth_handle_ = nullptr;
    void* eth_glue_ = nullptr;
    struct esp_netif_obj* eth_netif_ = nullptr;
    NetworkEventCallback network_event_callback_ = nullptr;
    bool connected_ = false;
    std::string ip_address_;

    virtual void OnNetworkEvent(NetworkEvent event, const std::string& data = "");
    virtual std::string GetEthernetMacAddress();
    virtual std::string GetBoardJson() override;

private:
    static void NetworkTaskEntry(void* arg);
    static void EthEventHandler(void* arg, const char* event_base, int32_t event_id, void* event_data);
    static void GotIpEventHandler(void* arg, const char* event_base, int32_t event_id, void* event_data);
    void NetworkTask();

public:
    EthernetBoard() = default;
    virtual ~EthernetBoard() = default;

    virtual std::string GetBoardType() override;
    virtual void StartNetwork() override;
    virtual NetworkInterface* GetNetwork() override;
    virtual void SetNetworkEventCallback(NetworkEventCallback callback) override;
    virtual const char* GetNetworkStateIcon() override;
    virtual void SetPowerSaveLevel(PowerSaveLevel level) override;
    virtual AudioCodec* GetAudioCodec() override { return nullptr; }
    virtual std::string GetDeviceStatusJson() override;
};

#endif // ETHERNET_BOARD_H
