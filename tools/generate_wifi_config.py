"""Generate an optional private Wi-Fi bootstrap header without logging credentials."""
import json
from pathlib import Path
import sys


# 校验可选预置网络的字节长度及密码格式；空配置保留设备正常热点配网流程。
def validate_network(config):
    if not isinstance(config, dict):
        raise ValueError("Wi-Fi configuration must be an object")
    ssid, password = config.get("ssid", ""), config.get("password", "")
    if not isinstance(ssid, str) or not isinstance(password, str):
        raise ValueError("Wi-Fi SSID and password must be strings")
    if any(c in ssid + password for c in "\r\n\0"):
        raise ValueError("Invalid Wi-Fi configuration")
    if len(ssid.encode("utf-8")) > 32:
        raise ValueError("Wi-Fi SSID exceeds 32 bytes")
    size = len(password.encode("utf-8"))
    if ssid and size and not (8 <= size <= 63 or
                             (size == 64 and all(c in "0123456789abcdefABCDEF" for c in password))):
        raise ValueError("Invalid Wi-Fi password length or format")
    if not ssid and password:
        raise ValueError("Wi-Fi password requires an SSID")
    return ssid, password


def generate(source, output):
    config = json.loads(source.read_text(encoding="utf-8-sig")) if source.exists() else {}
    if not isinstance(config, dict):
        raise ValueError("Wi-Fi configuration must be an object")
    # 兼容原来的单热点格式；多热点使用 networks 数组，最多与 NVS 一样保存 10 组。
    if "networks" in config:
        if "ssid" in config or "password" in config:
            raise ValueError("Use either networks or a single Wi-Fi configuration")
        entries = config["networks"]
        if not isinstance(entries, list) or len(entries) > 10:
            raise ValueError("Wi-Fi networks must be a list of at most 10 entries")
        networks = [validate_network(entry) for entry in entries]
        if any(not ssid for ssid, _ in networks):
            raise ValueError("Wi-Fi network entries require an SSID")
    else:
        network = validate_network(config)
        networks = [network] if network[0] else []
    if len({ssid for ssid, _ in networks}) != len(networks):
        raise ValueError("Duplicate Wi-Fi SSIDs are not allowed")

    # UTF-8 SSID 按字节校验；生成头文件使用转义字面量，不泄露到公共默认配置。
    content = (
        "#pragma once\n#include <array>\n\nnamespace wifi_local {\n"
        "struct Network { const char* ssid; const char* password; };\n"
        f"inline constexpr std::array<Network, {len(networks)}> kNetworks = {{{{\n"
    )
    for ssid, password in networks:
        content += ("    {" + json.dumps(ssid, ensure_ascii=True) + ", "
                    + json.dumps(password, ensure_ascii=True) + "},\n")
    content += "}};\n}  // namespace wifi_local\n"
    output.parent.mkdir(parents=True, exist_ok=True)
    if not output.exists() or output.read_text(encoding="utf-8") != content:
        output.write_text(content, encoding="utf-8")


if __name__ == "__main__":
    try:
        generate(*map(Path, sys.argv[1:3]))
    except (ValueError, TypeError):
        raise SystemExit("Invalid private Wi-Fi configuration; check SSID and password")
