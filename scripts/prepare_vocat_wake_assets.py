"""Stage configured WakeNet models for the VoCat expression asset builder."""

import argparse
import json
from pathlib import Path

from build_default_assets import (
    get_wakenet_model_paths,
    process_sr_models,
    read_idf_target_from_sdkconfig,
    read_wake_word_type_from_sdkconfig,
    read_wakenet_from_sdkconfig,
)


# 从 sdkconfig 读取真正启用的 WakeNet 模型并放入 staging，避免表情包带错唤醒词。
# 只生成构建暂存文件，原始依赖资源及模型不在这里修改。
def prepare(sdkconfig, model_path, config_path, staging):
    staging = Path(staging)
    config = json.loads(Path(config_path).read_text(encoding="utf-8"))
    wake_type = read_wake_word_type_from_sdkconfig(str(sdkconfig))
    if wake_type["use_custom_wake_word"]:
        raise ValueError("VoCat expression assets require a WakeNet model, not MultiNet")

    # 默认取消供应商自带模型，启用唤醒时再写入本工程选择的模型包。
    config["wakenet_model"] = "none"
    if wake_type["use_afe_wake_word"] or wake_type["use_esp_wake_word"]:
        names = read_wakenet_from_sdkconfig(str(sdkconfig))
        paths = get_wakenet_model_paths(names, str(model_path))
        if not names or len(paths) != len(names):
            raise ValueError("Every configured VoCat WakeNet model must exist")
        output = staging / "wakenet"
        output.mkdir(parents=True, exist_ok=True)
        packed = process_sr_models(
            paths, [], str(staging / "model-build"), str(output),
            idf_target=read_idf_target_from_sdkconfig(str(sdkconfig)),
        )
        if not packed:
            raise RuntimeError("Failed to package VoCat WakeNet models")
        config["wakenet_model"] = packed
        print("VoCat expression WakeNet models: " + ", ".join(names))

    resolution = staging / "360_360"
    resolution.mkdir(parents=True, exist_ok=True)
    (resolution / "config.json").write_text(
        json.dumps(config, indent=4) + "\n", encoding="utf-8"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdkconfig", required=True)
    parser.add_argument("--model-path", required=True)
    parser.add_argument("--config", required=True)
    parser.add_argument("--staging", required=True)
    args = parser.parse_args()
    prepare(args.sdkconfig, args.model_path, args.config, args.staging)
