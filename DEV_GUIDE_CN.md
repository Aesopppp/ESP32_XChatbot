# ESP-VoCat v1.2 独立开发测试工程

工程根目录：`D:\ESP32_XChatbot_v1.2`。
上游：<https://github.com/78/xiaozhi-esp32>。
初始源码提交：`0d576d3d4c049c6f55eaf879725dc23e516511b4`，工程版本 2.5.1。

GitHub 仓库名为 `ESP32_XChatbot`，本机目录和板型仍为 ESP-VoCat v1.2。
当前已接入 ACOS WebSocket，支持本地唤醒、连续对话、回答中讲话打断，
以及无输入一分钟回到待唤醒。配置、音频协议和限制见 `ACOS_GUIDE_CN.md`。
以下带日期的记录描述当时的准备和诊断状态；当前版本已经过用户真机测试。

中文源码阅读指南见 [docs/SOURCE_GUIDE_CN.md](docs/SOURCE_GUIDE_CN.md)。
指南按当前 VoCat/ACOS 的执行流程说明模块、线程、缓存和常用修改入口，
可与源码中的中文注释一起阅读。

## 开发环境

- ESP-IDF 6.1：`D:\Develop_C\Espnew\v6.1\esp-idf`
- 独立编译工具及 Python：`D:\Develop_C\Espnew\610_Tool`
- 芯片目标：ESP32-S3
- 板型：`Espressif ESP-VoCat`，源码自动识别 PCB v1.2
- 上游构建变体：`espressif/esp-vocat` / `esp-vocat`
- 配置：16 MB Flash、Octal PSRAM、中文、VoCat 表情动画资源
- 当前唤醒词：**你好小益**（“你好小易”同音）。构建入口选择 `wn9_nihaoxiaoyi_tts2`，VoCat 表情资源包按 sdkconfig 重新打包所选 WakeNet；以启动日志的 `AfeAudioEngine: Model` 为准。
- 编译并行度：2
- 开机音量：40/100。每次启动恢复到 40，运行中可用语音调节；下次开机再次恢复到 40。

用 VSCode 打开 `ESP32_XChatbot_v1.2.code-workspace`，或直接打开工程根目录。
此目录可直接开发，不包含多层 examples/agent/chatbot 结构。

## 初次构建

在项目根目录的 PowerShell 中执行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\dev.ps1 build
```

也可以按 `Ctrl+Shift+B`，执行“VoCat: 编译原版测试固件”。
该入口调用上游构建脚本，自动生成板型配置和表情资源，并构建、合并固件。
它会重新生成 sdkconfig，适合初次构建或恢复原版测试配置。

## 后续修改与编译

如果已经通过 menuconfig 修改了配置，使用普通增量构建来保留这些配置：

```powershell
. .\tools\enter-idf.ps1
idf.py --version
idf.py menuconfig
idf.py build
```

版本应显示 ESP-IDF v6.1。上游最新 main 要求至少 6.0.1，不支持 5.5.5。
以后新建终端，需要再次点加载 `enter-idf.ps1`。

## 烧录与运行

先确认设备管理器中的实际串口。下面 COM6 只是示例：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\dev.ps1 flash-monitor -Port COM6
```

首次测试需要烧录完整项目的分区及资源，推荐上述 `idf.py flash` 路径，
而不是只手动烧录应用程序 bin。串口监视器用 `Ctrl+]` 退出。

设备上电后按照屏幕提示配网。使用默认小智云服务时，按设备提示到对应
控制台绑定/激活，再说“你好小益”进行对话。原 ACOS token 不能用于小智云。

## 主要代码位置

### 手机热点测试

可在 `.local/wifi.json` 中填写 `ssid` 和 `password`，重新编译、烧录后，
VoCat 开机会自动将此热点加入已保存的 Wi-Fi，不必用同一部手机打开配网页面。
此文件被 Git 忽略，密码不会写入公共源码或日志，但会包含在本机固件中。
热点须使用 2.4 GHz，开机前打开并靠近设备。

已有网络会保留，设备按原有 Wi-Fi 扫描与连接逻辑选择已保存的网络，
不会强制只连接预置热点。以 `Connected to WiFi: Phooooone` 确认热点连接。
未连接成功时仍会在超时后进入原配网页面。
移除本地配置后重新编译会停止
预置热点，但已写入设备的热点信息需在配网页面删除。


- `main/boards/espressif/esp-vocat/`：VoCat 板级驱动、硬件版本、引脚和交互。
- `main/application.cc`：应用状态、监听、播放、通知和协议回调。
- `main/audio/`：采集、播放、AFE/AEC、唤醒、Opus 与有界缓存。
- `main/protocols/`：小智 WebSocket 和 MQTT/UDP 协议。
- `main/Kconfig.projbuild`：menuconfig 的功能选项。
- `main/boards/espressif/esp-vocat/config.json`：上游构建变体。
- `docs/notify.md`：上游主动语音通知协议。

首次先验证原版硬件和服务连接。设备端 AEC 选项默认没有启用；进行语音
打断测试时，再在 menuconfig 中启用 `Enable Device-Side AEC`，确认回采和
服务端实时模式有效。它不能仅凭选项保证完全消除扬声器回声。

## 主机检查

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\dev.ps1 verify
```

Windows 入口为上游测试的临时目录清理处理了当前目录占用问题，并使用
UTF-8。没有本机 C++ 编译器时，Ogg 解复用器的本机执行测试会明确跳过；
ESP32 交叉编译器不能用来执行 Windows 本机测试程序。
主机检查和固件编译都不能替代开发板上的实际测试。

## 本次准备与验证结果（2026-10-08）

- ESP-IDF 6.1 环境已安装并验证，VoCat 中文测试固件已完整编译、合并成功。
- 应用固件：`build/xiaozhi.bin`，2,553,888 字节，应用分区剩余约 38%。
- 合并固件：`build/merged-binary.bin`，11,357,491 字节；完整烧录入口仍推荐上面的 `flash-monitor` 命令。
- 表情资源：`build/expression_assets.bin`，2,968,883 字节，分区大小检查通过。
- 主机测试共 98 项：97 项通过，1 项因没有 Windows 本机 C++ 编译器而跳过。
- 首次构建发现语言头文件生成与应用编译的顺序问题，已在 `main/CMakeLists.txt`
  增加组件对 `lang_header` 的依赖。使用上游生成器生成文件，没有手工编辑生成内容。
- 开发分支：`dev/esp-vocat-v1.2`；上游 `main` 基线保留。
- 构建日志：`.cache/setup-logs/build-retry.log`；测试日志：`.cache/setup-logs/host-checks-final.log`。
- 尚未烧录或进行真机验证；配网、唤醒、音频、屏幕和云服务需要在设备上测试。

## WiFi 连接成功后 HTTP 400 的排查（2026-10-09）

运行日志中的 `Ota: Failed to check version, status code: 400` 表示启动配置接口
拒绝了请求；该接口还负责提供激活及对话连接配置，不能仅跳过版本检查。
用日志中的设备身份和重建的设备信息在电脑请求同一接口，返回 HTTP 200。
这只能说明电脑上的该请求被接受，不能证明板端实际请求内容或网络路径相同。

已在 `main/ota.cc` 增加完整请求 JSON 校验和最多 512 字节的错误响应读取，
打印服务器的 message/error 字段；成功响应中的连接凭证不打印。
新固件和合并固件已编译通过，主机测试 97 项通过、1 项跳过。
这是一版诊断固件，HTTP 400 根因仍需设备端新增日志确认，尚未完成真机验证。

烧录后如仍失败，请保留以下两类日志及其前后内容：

```text
Ota: Checking startup configuration: method=..., payload=... bytes, activation=v...
Ota: Startup configuration rejected: HTTP 400, server detail: ...
```

日志路径：`.cache/setup-logs/ota-diagnostics-build.log` 和
`.cache/setup-logs/ota-diagnostics-tests.log`。

后续设备日志已定位到旧 NVS 数据兼容问题：`board/uuid` 的值包含 JSON 字符串
外层的双引号。上游把 UUID 直接拼入请求 JSON，也直接用作 Client-Id 请求头。
用日志里的原值重现了 JSON 格式错误。已在 `main/boards/common/board.cc` 中
读取并解码这种旧值，校验 UUID 格式后保存，保留原来的设备身份。
没有清空 NVS，也没有重新生成 UUID。

修复版本已编译和合并成功，主机测试 97 项通过、1 项跳过；尚未烧录验证。
首次启动应看到 `Migrated legacy JSON-encoded UUID; device identity preserved`，
随后 `Board: UUID=...` 的值不再带外层双引号。继续观察配置请求是否成功，
以及设备是否提示绑定/激活。若仍有 HTTP 错误，诊断日志会保留具体服务器原因。
构建日志：`.cache/setup-logs/uuid-migration-build.log`；
测试日志：`.cache/setup-logs/uuid-migration-tests.log`。

## 上游更新

工程已保留 Git 历史；`upstream` 指向小智上游，`origin` 指向自己的 GitHub 仓库，
便于查看差异和同步上游维护。
默认 .gitignore 已排除 build、managed_components、sdkconfig、个人 VSCode 配置。
提交开发改动前检查 `git status`；本地新增的工作区、说明和工具脚本可以随
自己的工程保留。后续应在分支上继续开发，保留上游板型身份。
