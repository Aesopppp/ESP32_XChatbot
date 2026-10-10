"""Generate an optional private Wi-Fi bootstrap header without logging credentials."""
import json
from pathlib import Path
import sys


def generate(source, output):
    config = json.loads(source.read_text(encoding="utf-8-sig")) if source.exists() else {}
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
    content = "#pragma once\n" + "\n".join(
        f"#define WIFI_LOCAL_{key} {json.dumps(value, ensure_ascii=True)}"
        for key, value in (("SSID", ssid), ("PASSWORD", password))
    ) + "\n"
    output.parent.mkdir(parents=True, exist_ok=True)
    if not output.exists() or output.read_text(encoding="utf-8") != content:
        output.write_text(content, encoding="utf-8")


if __name__ == "__main__":
    try:
        generate(*map(Path, sys.argv[1:3]))
    except (ValueError, TypeError):
        raise SystemExit("Invalid private Wi-Fi configuration; check SSID and password")
