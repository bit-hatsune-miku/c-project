#!/usr/bin/env python3

from __future__ import annotations

from pathlib import Path

from PIL import Image


REPO_ROOT = Path(__file__).resolve().parents[1]
SOURCE = REPO_ROOT / "assets" / "vn" / "credits" / "grouppicture.png"
OUTPUT = REPO_ROOT / "assets" / "vn" / "backgrounds" / "credits" / "group_cast_placeholder.png"
TARGET_SIZE = (1280, 720)
PADDING = 24


def main() -> int:
    if not SOURCE.exists():
        raise SystemExit(f"Missing source image: {SOURCE}")

    OUTPUT.parent.mkdir(parents=True, exist_ok=True)

    source = Image.open(SOURCE).convert("RGBA")
    canvas = Image.new("RGBA", TARGET_SIZE, (255, 255, 255, 255))

    max_width = TARGET_SIZE[0] - PADDING * 2
    max_height = TARGET_SIZE[1] - PADDING * 2
    scale = min(max_width / source.width, max_height / source.height)
    scaled_size = (
        max(1, int(round(source.width * scale))),
        max(1, int(round(source.height * scale))),
    )
    fitted = source.resize(scaled_size, Image.Resampling.LANCZOS)

    offset = (
        (TARGET_SIZE[0] - fitted.width) // 2,
        (TARGET_SIZE[1] - fitted.height) // 2,
    )
    canvas.alpha_composite(fitted, dest=offset)
    canvas.convert("RGB").save(OUTPUT)
    print(OUTPUT.relative_to(REPO_ROOT).as_posix())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
