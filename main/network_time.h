#pragma once

// 由应用主任务调用。校时在 lwIP 后台执行，不等待网络、不占用音频任务。
namespace NetworkTime {
void OnConnected();
void OnDisconnected();
void Poll();
}  // namespace NetworkTime
