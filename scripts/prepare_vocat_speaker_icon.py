"""Replace the VoCat speaker's Zzz with sound waves in staged assets."""

import argparse
from pathlib import Path
import struct
import zlib


SIZE = 30


# 将 RGBA 像素写为 PNG 供人工预览；固件实际使用下方生成的 RGB565A8 bin。
def save_png(path, pixels, size):
    def chunk(kind, content):
        return (struct.pack(">I", len(content)) + kind + content
                + struct.pack(">I", zlib.crc32(kind + content)))

    rows = b"".join(
        b"\x00" + bytes(channel for pixel in pixels[y * size:(y + 1) * size]
                         for channel in pixel)
        for y in range(size)
    )
    Path(path).write_bytes(
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0))
        + chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b"")
    )


# 30x30 图标：12 字节头 + 1800 字节 RGB565 + 900 字节透明度。
# 仅替换右侧 x>=17 的区域，左侧喇叭的颜色和透明度原样保留。
def prepare(source, wave_alpha, output):
    data = Path(source).read_bytes()
    if len(data) != 2712 or data[:8] != bytes.fromhex("190a00001e001e00"):
        raise ValueError("Expected a 30x30 RGB565A8 speaker icon")

    rgb = bytearray(data[12:1812])
    alpha = bytearray(data[1812:])
    # Reuse the approved two-wave artwork: 13 alpha pixels per row, 30 rows.
    mask = bytes.fromhex(Path(wave_alpha).read_text(encoding="ascii"))
    if len(mask) != 13 * SIZE:
        raise ValueError("Expected a 13x30 sound-wave alpha mask")

    # Preserve the speaker's original RGB565 and alpha bytes without re-encoding.
    for y in range(SIZE):
        for x in range(17, SIZE):
            i = y * SIZE + x
            rgb[2 * i:2 * i + 2] = b"\xff\xff"
            alpha[i] = mask[y * 13 + x - 17]

    output = Path(output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(data[:12] + rgb + alpha)

    pixels = []
    for i in range(SIZE * SIZE):
        color = int.from_bytes(rgb[2 * i:2 * i + 2], "big")
        pixels.append((
            ((color >> 11) & 31) * 255 // 31,
            ((color >> 5) & 63) * 255 // 63,
            (color & 31) * 255 // 31,
            alpha[i],
        ))
    save_png(output.with_suffix(".png"), pixels, SIZE)
    preview = []
    for y in range(300):
        for x in range(300):
            red, green, blue, opacity = pixels[(y // 10) * SIZE + x // 10]
            preview.append(tuple(
                (channel * opacity + 28 * (255 - opacity) + 127) // 255
                for channel in (red, green, blue)
            ) + (255,))
    save_png(output.with_name("icon_speaker_preview.png"), preview, 300)
    print("VoCat playback icon: speaker with two sound waves")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True)
    parser.add_argument("--wave-alpha", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    prepare(args.source, args.wave_alpha, args.output)
