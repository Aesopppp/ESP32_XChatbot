# ESP-VoCat v1.2

本目录为 ESP32_XChatbot 的目标板型实现，使用 ESP32-S3、16 MB Flash、Octal PSRAM、
触摸圆屏和带扬声器参考回采的音频链路。硬件资料见 [乐鑫开源硬件](https://oshwhub.com/esp-college/echoear)。
当前代码保留原有 PCB 版本检测；本工程日常验证目标为 v1.2。

## 默认构建

在项目根目录且 ESP-IDF 环境已加载时执行：

```sh
python scripts/build.py espressif/esp-vocat --name esp-vocat --language zh-CN --wake-word wn9_nihaoxiaoyi_tts2
```

本机 Windows 可以使用 `tools/dev.ps1 build`。它会重新生成 `sdkconfig`；
已有 menuconfig 自定义设置时，用 `idf.py build` 进行增量构建。

默认使用表情动画界面、ACOS、你好小益唤醒模型。
构建通过 `prepare_vocat_wake_assets.py` 和 `prepare_vocat_speaker_icon.py` 自动准备模型与声波图标，
再打包表情资源，无需手工下载旧版 `expression_assets.bin` 或修改 `managed_components`。
更换模型或图标后需完整烧录资源分区。

## 代码入口

- `esp_vocat.cc` / `config.h`：板级初始化、引脚、音频、按键、触摸。
- `vocat_settings_display.*` / `vocat_touch_policy.h`：上滑菜单及手势；适用于 Emote 界面。
- `vocat_menu_font_20.c`：菜单字库；许可见同目录 `vocat_menu_font_LICENSE.txt`。
- `wifi_ap_name.cmake`：配网热点名，当前为 `XChatbot-esp32s3`。
- `config.json`：构建变体配置。

首次无有效音量记录时默认 50；之后开机恢复 NVS 保存值。
上滑菜单调节音量、亮度或重新配网；BOOT 长按约 3 秒同样进入配网。

使用与维护说明见 [项目首页](../../../../README.md)、[ACOS 指南](../../../../ACOS_GUIDE_CN.md)
和 [VoCat 设置说明](../../../../docs/VOCAT_SETTINGS_CN.md)。
