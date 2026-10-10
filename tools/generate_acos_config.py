"""Generate a private build header; never print credentials or commit this output."""
import json
from pathlib import Path
import sys

source, output = map(Path, sys.argv[1:3])
config = json.loads(source.read_text(encoding="utf-8-sig")) if source.exists() else {}
url = config.get("url", "wss://acos-platform.emicloud.com/acos-realtime")
token = config.get("token", "")
if not isinstance(url, str) or not isinstance(token, str):
    raise SystemExit("ACOS url and token must be strings")
if not url.startswith("wss://") or any(c in url + token for c in "\r\n\0"):
    raise SystemExit("Invalid ACOS configuration")
content = "#pragma once\n" + "\n".join(
    f"#define ACOS_LOCAL_{key} {json.dumps(value, ensure_ascii=True)}"
    for key, value in (("URL", url), ("TOKEN", token))
) + "\n"
output.parent.mkdir(parents=True, exist_ok=True)
if not output.exists() or output.read_text(encoding="utf-8") != content:
    output.write_text(content, encoding="utf-8")
