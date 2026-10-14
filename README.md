# ESP32_XChatbot

基于 [XiaoZhi ESP32](https://github.com/78/xiaozhi-esp32) 开发的 **ESP-VoCat v1.2 / ACOS 语音助手**。
本仓库已整理为 VoCat 专用工程：只保留 ESP32-S3 / ESP-VoCat 板型以及简体中文、英文资源。
默认分支为 `dev/esp-vocat-v1.2`，本机开发目录为 `D:\ESP32_XChatbot_v1.2`。

## 当前功能

- 本地 ESP-SR 唤醒词：**你好小益**（“你好小易”同音）。
- ACOS WebSocket 语音对话；首次唤醒后可连续提问，回答播完后无输入 60 秒回到待唤醒。
- 回答期间持续上传经过 AEC 的麦克风音频，由 ACOS 讲话事件确认打断；支持唤醒词、触摸打断。
- 有界音频队列、播放预缓冲、断线重连和过期回答过滤。
- VoCat 表情屏及声波播放图标；底部上滑打开音量、亮度、重新配网菜单。
- 音量和亮度保存到 NVS；开机恢复上次音量，无有效记录时默认 **50/100**。
- 手机热点网页配网，热点名 **XChatbot-esp32s3**；BOOT 长按约 3 秒可进入配网。
- 联网后 SNTP 自动校时，按北京时间显示。

默认使用 ACOS，无需输入小智激活码。小智云端的 MCP、账号和模型能力不自动适用于 ACOS。
打断效果取决于回采/AEC、网络和服务器语音检测，仓库精简不会改变这些参数。

## 开发与构建

推荐使用 **ESP-IDF 6.1**；最低支持 6.0.1，**不支持 ESP-IDF 5.x**。
本机 PowerShell 脚本和 VS Code 工作区使用原开发机路径；其他电脑先按实际安装位置调整
`tools/enter-idf.ps1` 和 `ESP32_XChatbot_v1.2.code-workspace`，或在已导出 ESP-IDF 环境的终端使用 Python 构建入口。

1. 从 `tools/acos-config.example.json` 复制生成 `.local/acos.json`，填写自己的 ACOS 地址和 token。
2. 可选：按 [设置说明](docs/VOCAT_SETTINGS_CN.md#本机预置多个-wi-fi) 创建 `.local/wifi.json`，填写预置 Wi-Fi；也可直接用设备配网页面。
3. 首次构建或恢复默认 VoCat 配置：

```powershell
cd D:\ESP32_XChatbot_v1.2
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\dev.ps1 build
```

Linux 或已加载 ESP-IDF 环境的终端使用同一构建入口：

```sh
python scripts/build.py espressif/esp-vocat --name esp-vocat --language zh-CN --wake-word wn9_nihaoxiaoyi_tts2
```

该入口会重新生成 `sdkconfig`，选择 16 MB Flash、Octal PSRAM、表情资源和指定唤醒模型，
并生成完整合并固件。之后如需保留 menuconfig 自定义设置，使用增量构建：

```powershell
. .\tools\enter-idf.ps1
idf.py menuconfig
idf.py build
```

烧录时将 `COM6` 换成实际串口，先关闭占用串口的监视器：

```powershell
.\tools\dev.ps1 flash-monitor -Port COM6
```

必须烧录应用、分区表和资源，尤其在更换唤醒模型或图标后；正常烧录不必擦除 NVS。
监视器用 `Ctrl+]` 退出。

`.local/`、`.vscode/`、`sdkconfig`、下载的依赖和构建产物均不提交 Git。
真实 token、Wi-Fi 密码只保留在本机；包含这些配置的固件也应私下保存。
公开 CI 不携带私有配置，其产物用于编译检查，需要配置 ACOS 凭据后才能实际对话。

## 目录与说明

| 位置 | 用途 |
| --- | --- |
| `main/main.cc`、`main/application.*` | 程序入口、事件主循环与对话控制 |
| `main/boards/espressif/esp-vocat/` | VoCat 硬件、触屏菜单和按键 |
| `main/boards/common/` | 通用板级接口及联网、背光等辅助代码 |
| `main/audio/`、`main/protocols/` | 唤醒、AEC、编解码、播放和 ACOS 通信 |
| `main/display/`、`main/assets/` | 表情屏、图标、字体引用和中英文提示资源 |
| `scripts/`、`tools/` | 构建、资源准备、本机配置生成及测试 |
| `partitions/v2/16m.csv`、`sdkconfig.defaults*` | 当前硬件所需分区与默认配置 |
| `docs/` | 使用、架构和协议文档 |

- [ACOS 使用与音频行为](ACOS_GUIDE_CN.md)
- [开发指南](DEV_GUIDE_CN.md)
- [触屏设置、音量保存与配网](docs/VOCAT_SETTINGS_CN.md)
- [中文源码阅读指南](docs/SOURCE_GUIDE_CN.md)
- [精简范围与旧文件恢复方法](docs/REPOSITORY_SCOPE_CN.md)

保留的通用代码和第三方依赖存在交叉引用，本次未进一步裁剪运行时模块。
精简主要减少仓库文件和维护范围，不代表固件体积、RAM 占用或克隆历史体积按相同比例减少。

## 验证

```sh
python tools/run_host_tests.py
python -m unittest discover -s tools -p 'test_*.py' -v
```

GitHub Actions 使用 ESP-IDF 6.1，运行主机测试并编译当前中文 VoCat 变体。
音频、触屏、配网等实际效果仍需要真机测试。

## 来源与许可

保留 [XiaoZhi ESP32](https://github.com/78/xiaozhi-esp32) 的上游代码、版权及 [MIT 许可证](LICENSE)。
字体和第三方组件遵循各自的许可；菜单字体说明在板型目录的 `vocat_menu_font_LICENSE.txt`。
上游其他板型和语言可从上游工程或精简前标签获取，当前分支不再提供它们的构建支持。
