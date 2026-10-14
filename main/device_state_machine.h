/*
 * 设备状态机：校验允许的状态迁移，再同步通知注册的观察者。
 * 音频、显示和联网的副作用属于 Application；状态机本身不操作硬件。
 * 原子状态方便跨任务读取，但不会把任意多个并发状态变更自动串行化，应由应用主任务统一调度。
 */

#ifndef DEVICE_STATE_MACHINE_H
#define DEVICE_STATE_MACHINE_H

#include <atomic>
#include <functional>
#include <mutex>
#include <vector>

#include "device_state.h"

/**
 * DeviceStateMachine - Manages device state transitions with validation
 * 
 * This class ensures strict state transition rules and provides a callback mechanism
 * for components to react to state changes.
 */
class DeviceStateMachine {
public:
    DeviceStateMachine();
    ~DeviceStateMachine() = default;

    // Delete copy constructor and assignment operator
    DeviceStateMachine(const DeviceStateMachine&) = delete;
    DeviceStateMachine& operator=(const DeviceStateMachine&) = delete;

    /**
     * Get the current device state
     */
    DeviceState GetState() const { return current_state_.load(); }

    /**
     * Attempt to transition to a new state
     * @param new_state The target state
     * @return true if transition was successful, false if invalid transition
     */
    // 重复目标状态为成功空操作，非法路径返回 false；成功后同步通知 old_state / new_state。
    bool TransitionTo(DeviceState new_state);

    /**
     * Check if transition to target state is valid from current state
     */
    // 只根据当前状态快照判断，不能作为并发修改后的有效性保证。
    bool CanTransitionTo(DeviceState target) const;

    /**
     * State change callback type
     * Parameters: old_state, new_state
     */
    using StateCallback = std::function<void(DeviceState, DeviceState)>;

    /**
     * Add a state change listener (observer pattern)
     * Callback is invoked in the context of the caller of TransitionTo()
     * @return listener id for removal
     */
    // 回调在调用 TransitionTo 的线程同步执行；需切到主任务的操作应由回调自行 Schedule。
    int AddStateChangeListener(StateCallback callback);

    /**
     * Remove a state change listener by id
     */
    // 从列表移除观察者；已复制到正在执行的通知批次中的回调仍可能被调用。
    void RemoveStateChangeListener(int listener_id);

    /**
     * Get state name string for logging
     */
    static const char* GetStateName(DeviceState state);

private:
    std::atomic<DeviceState> current_state_{kDeviceStateUnknown};
    std::vector<std::pair<int, StateCallback>> listeners_;
    int next_listener_id_{0};
    // 此锁保护监听器列表，不包住所有状态迁移；应用层负责串行驱动状态。
    std::mutex mutex_;

    /**
     * Check if transition from source to target is valid
     */
    bool IsValidTransition(DeviceState from, DeviceState to) const;

    /**
     * Notify callback of state change
     */
    void NotifyStateChange(DeviceState old_state, DeviceState new_state);
};

#endif // DEVICE_STATE_MACHINE_H
