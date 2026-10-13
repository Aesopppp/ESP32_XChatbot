# ACOS 语音对话接入

当前工程：`D:\ESP32_XChatbot_v1.2`，开发板 ESP-VoCat v1.2，SDK ESP-IDF 6.1。
在小智工程保留板级驱动、配网、屏幕及 ESP-SR 本地唤醒；使用独立的
`AcosProtocol` 接入原 ACOS 实时服务，不使用小智的激活码、MQTT 和 OTA 查询。

## 使用流程

1. 烧录后正常配网。后台连接 ACOS，看到 `Websocket authenticated` 表示鉴权成功。
2. 待机不上传麦克风音频。说“你好小益”（“你好小易”同音）或触摸屏幕开启对话。
3. 可以连续提问。回答期间继续上传经过本地 AEC 的语音；由 ACOS
   的讲话事件确认打断，停止旧回答。仍支持用唤醒词或触摸屏幕打断。
4. 回答播放完毕后，无输入一分钟自动回到待唤醒。聆听时触摸屏幕也可结束会话。
5. 每次开机音量为 50/100；屏幕底部上滑打开设备设置，可调音量、亮度和重新配网。详见 [VoCat 设置说明](docs/VOCAT_SETTINGS_CN.md)。

自由讲话打断由 ACOS 的 `input_audio_buffer.speech_started` 确认，不再用本地
VAD 持续 240ms 直接停止播放，避免残留回声触发本地误打断。停止播放需要等待
服务器讲话事件，延迟取决于网络和服务器 VAD；本地唤醒词和触摸仍可直接打断。
麦克风在回答期间
一直采集和上传，因此新问题句首在打断事件到达前已经送往服务器，不用在打断后
重新开始录音。待机不上传、不保存 SD 卡录音；弱网或重连期间的过期语音会丢弃，
需要重新提问。实际识别和误打断率需要在设备上测试，编译不能验证声学效果。

## 配置和编译

私密文件 `.local/acos.json` 已加入 Git 忽略。可从 `tools/acos-config.example.json`
复制生成；其中 `url` 填 ACOS 的 `wss://` 地址，`token` 填对应模型的凭据。
配置只在编译时生成到 build 头文件，不打印 token。NVS 的 `acos/url`、`acos/token`
若存在，会优先于此文件。修改文件后必须重新编译、烧录；不要分享带真实 token 的固件。

```powershell
cd D:\ESP32_XChatbot_v1.2
. .\tools\enter-idf.ps1
idf.py build merge-bin
```

在 VS Code 也可以执行 ESP-IDF Build，再选择设备的实际端口烧录。命令行示例：

```powershell
.\tools\dev.ps1 flash-monitor -Port COM6
```

menuconfig → Xiaozhi Assistant：

- `Use ACOS WebSocket instead of XiaoZhi cloud`：ACOS / 原小智服务切换；VoCat 默认开启。
- `ACOS continuous microphone and speech interruption`：默认开启，显式启用板载回采 AEC；
  关闭后使用唤醒词打断模式。
- `ACOS conversation silence timeout`：默认 60 秒。

本地唤醒模型为 `wn9_nihaoxiaoyi_tts2`，官方名称“你好小益”。`tools/dev.ps1 build`
默认选择此模型，表情资源中的 `srmodels.bin` 也按 sdkconfig 的 WakeNet 选择重新生成。
以后修改唤醒词可在 menuconfig 的 ESP Speech Recognition 中更换 WakeNet 模型，
再执行 `idf.py build merge-bin`。必须完整烧录资源分区，单独烧录应用 bin 不会更换模型。
启动日志应出现 `Model 0: wn9_nihaoxiaoyi_tts2`；识别率需要实际开口测试。

本次迁移没有实现 ACOS 的工具调用适配。小智云端提供的音量 MCP、音乐、天气等
能力不会自动成为 ACOS 的能力；回答内容和工具能力取决于 ACOS 所连接的模型。
不改变 Wi-Fi 保存信息、不清空 NVS，也不修改旧 ACOS 工程。

## 传输和状态处理

- 连接后发送 `{"type":"auth","token":"..."}`，等待 `__proxy_connected__`。
- 上行：16kHz 单声道 Opus，每帧 20ms，经 Base64 包装为 `input_audio_buffer.append`。
- 下行：沿用旧工程 16kHz Opus 二进制帧，解码器按 60ms 容量接收并按板级输出重采样。
- WebSocket 回调只将帧放入有界 PSRAM 队列；网络发送在独立线程进行。
  主任务分批处理接收，解码队列满时保留帧，不堵塞 WebSocket 回调。
  接收队列最多 2048 帧、512 KiB，允许 TTS 比播放更快到达；超过上限会重连并清理旧音频。
- 开始播放前积累约 300ms（按下行 60ms/帧计）的音频；播放断粮后重新缓冲。
  短回答收到结束事件后立即放行；有音频但未攒够时最多等待 600ms。
  该策略平滑短时抖动，会增加部分起播延迟；不能消除持续的网络/服务端停顿。
  缓冲不阻塞主任务，麦克风上传及服务器讲话事件处理继续运行。
- 打断优先清理旧回答，按本地回答序号过滤迟到音频；不清除当前麦克风上行队列。
  服务器已经结束回答时不重复发送 `response.cancel`。
  用户讲话结束不作为取消确认；取消 10 秒仍未确认时重连，避免旧音频混入或永久卡住。
- 同时识别 `response.audio.done`、`response.done`、`response.cancelled/canceled` 和
  “没有活动回答”的取消确认。打断后还需收到 `speech_stopped` 才放行下一轮音频，
  防止取消确认早于讲话结束时，旧尾音被当成新回答。
  原始二进制音频不含 response_id，过滤依赖 ACOS 正确发送、排序这些边界事件。
- `response.audio.done` 不代表扬声器播放完毕，等音频队列实际排空再回到监听。
- 断线后每 5 秒重试并重新鉴权；保持对话的唤醒状态，但丢弃旧连接音频。
  待机保持连接，并通过 Ping/Pong 检查存活；休眠不等同于断网。
  上述保持唤醒针对 WebSocket 重连；Wi-Fi 完全断开时，应用会结束会话，恢复后需重新唤醒。

## 硬件验收

1. 开机保持安静，确认完成 ACOS 鉴权且未主动对话。
2. 说唤醒词并连续完成三轮问答。
3. 让设备讲较长内容，保持安静，确认没有被扬声器回声误打断。
4. 回答中直接说“昨天的日期呢”，检查句首完整和新回答正确；重复至少五次。
5. 测试唤醒词和触摸打断，确认旧声音不恢复。
6. 等播放完毕后安静一分钟，确认需再次唤醒。
7. 短暂关闭路由器或热点，再恢复，确认能重新鉴权并继续/重新唤醒。
8. 卡顿/等待缓冲时直接讲话，确认收到服务器讲话事件后切回监听，旧音频不再续播。
   保持安静测试长回答；若仍误打断，检查服务器讲话事件及回采/AEC 音频。

关键日志：`Websocket authenticated`、`Conversation awake`、
`ACOS server speech confirmed; old playback cleared`、`Response cancel sent`、
`ACOS silence timeout`。本工程包含取消边界的 C++ 编译期断言，另有配置生成测试
`python tools/test_acos_config.py`；它们不代替上述硬件测试。

播放诊断日志：

- `ACOS barge-in: server speech confirmation`：启用持续 AEC 上传及服务器确认打断。
- `ACOS server speech confirmed`：收到服务器讲话事件后清理旧播放；本地 VAD 不触发取消。
- `Playback buffered`：开始或恢复放行，包含轮号及已缓存时长。
- `Playback underrun`：解码/PCM 队列已空且又有该轮音频到达，进入重新缓冲。
- `Event ...`：只打印相关协议事件名称和本地轮号，不打印 token/消息正文。
- `Audio stats`：每 10 秒统计接收帧数、收发队列深度、丢弃上行/旧下行帧数、
  断粮次数及该窗口内最大下行音频间隔（ms）。大间隔提示到达不连续，不能单凭它
  区分 Wi-Fi、服务器生成停顿或设备接收任务调度。
