#ifndef _DEVICE_STATE_H_
#define _DEVICE_STATE_H_

// 应用状态描述当前交互阶段，不直接代表网络是否在线。
// Idle = 等待本地唤醒；Listening = 接收用户输入；Speaking = 正在处理/播放回答。
// ACOS 实时模式在 Speaking 仍采集麦克风，因此屏幕状态不等于麦克风开关。
enum DeviceState {
    kDeviceStateUnknown,
    kDeviceStateStarting,
    kDeviceStateWifiConfiguring,
    kDeviceStateIdle,
    kDeviceStateConnecting,
    kDeviceStateListening,
    kDeviceStateSpeaking,
    kDeviceStateNotifying,
    kDeviceStateUpgrading,
    kDeviceStateActivating,
    kDeviceStateAudioTesting,
    kDeviceStateFatalError
};

#endif // _DEVICE_STATE_H_
