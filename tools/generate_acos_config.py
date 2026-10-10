"""Generate a private build header; never print credentials or commit this output."""
import json
from pathlib import Path
import sys

# 输入为本机私密 JSON，输出为 build 下的 C/C++ 头文件；不在日志中打印 token。
# 配置不存在时生成默认地址和空 token，由运行时配置校验处理。
source, output = map(Path, sys.argv[1:3])
config = json.loads(source.read_text(encoding="utf-8-sig")) if source.exists() else {}
url = config.get("url", "wss://acos-platform.emicloud.com/acos-realtime")
token = config.get("token", "")
if not isinstance(url, str) or not isinstance(token, str):
    raise SystemExit("ACOS url and token must be strings")
if not url.startswith("wss://") or any(c in url + token for c in "\r\n\0"):
    raise SystemExit("Invalid ACOS configuration")
# 用 JSON 的字符串转义生成 C 字面量，避免引号、反斜线破坏生成的宏。
content = "#pragma once\n" + "\n".join(
    f"#define ACOS_LOCAL_{key} {json.dumps(value, ensure_ascii=True)}"
    for key, value in (("URL", url), ("TOKEN", token))
) + "\n"
output.parent.mkdir(parents=True, exist_ok=True)
if not output.exists() or output.read_text(encoding="utf-8") != content:
    output.write_text(content, encoding="utf-8")
