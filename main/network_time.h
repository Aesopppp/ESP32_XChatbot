#pragma once

// 由应用主任务调用。校时在 lwIP 后台执行，不等待网络、不占用音频任务。
namespace NetworkTime {
// 联网后设置 UTC+8 时区并异步请求 SNTP 校时；重复联网通知不会重启已经在线的请求。
void OnConnected();
// 停止网络校时，保留已校准的系统时间以便断网期间继续走时。
void OnDisconnected();
// 由主循环检查初次同步结果和超时，超时轮换备用服务器，不同步等待网络回复。
void Poll();
}  // namespace NetworkTime
