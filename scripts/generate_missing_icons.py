#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
from pathlib import Path

from PIL import Image


REPO_ROOT = Path(__file__).resolve().parent.parent
COMBAT_ICONS_DIR = REPO_ROOT / "assets" / "combat" / "icons"
COMBAT_SPRITES_DIR = REPO_ROOT / "assets" / "combat" / "sprites"
BABY_LYOO_ICON = REPO_ROOT / "assets" / "vn" / "icons" / "lyoo" / "baby.png"
LYOO_PLACEHOLDER_SOURCE = REPO_ROOT / "assets" / "vn" / "icons" / "lyoo" / "0.png"
OUTPUT_SIZE = 96
OPAQUE_START_THRESHOLD = 40
OPAQUE_TOP_PADDING = 12


def relative_path(path: Path) -> str:
    return path.relative_to(REPO_ROOT).as_posix()


def load_json(path: Path) -> dict:
    return json.loads(path.read_text())


def used_combat_asset_ids() -> list[str]:
    used: set[str] = set()
    for json_path in (
        REPO_ROOT / "assets" / "combat" / "characters.json",
        REPO_ROOT / "assets" / "combat" / "boss.json",
    ):
        for spec in load_json(json_path).values():
            asset_id = str(spec.get("assets", "")).strip()
            if asset_id:
                used.add(asset_id)
    return sorted(used)


def icon_exists(asset_id: str) -> bool:
    return any((COMBAT_ICONS_DIR / f"{asset_id}{extension}").exists() for extension in (".png", ".webp"))


def sprite_path_for_asset(asset_id: str) -> Path | None:
    for extension in (".png", ".webp"):
        candidate = COMBAT_SPRITES_DIR / f"{asset_id}{extension}"
        if candidate.exists():
            return candidate
    return None


def alpha_aware_crop(source_path: Path) -> tuple[int, int, int, int]:
    with Image.open(source_path).convert("RGBA") as image:
        width, height = image.size
        side = max(1, min(width, height))
        crop_x = max(0, (width - side) // 2)
        crop_y = 0

        bbox = image.getchannel("A").getbbox()
        if bbox is not None and bbox[1] >= OPAQUE_START_THRESHOLD:
            crop_y = max(0, min(height - side, bbox[1] - OPAQUE_TOP_PADDING))

        return crop_x, crop_y, side, side


def run_ffmpeg(source_path: Path, output_path: Path, crop_rect: tuple[int, int, int, int]) -> None:
    crop_x, crop_y, crop_w, crop_h = crop_rect
    output_path.parent.mkdir(parents=True, exist_ok=True)
    filter_graph = (
        f"crop={crop_w}:{crop_h}:{crop_x}:{crop_y},"
        f"scale={OUTPUT_SIZE}:{OUTPUT_SIZE}:flags=neighbor"
    )
    subprocess.run(
        [
            "ffmpeg",
            "-hide_banner",
            "-loglevel",
            "error",
            "-y",
            "-i",
            str(source_path),
            "-vf",
            filter_graph,
            "-frames:v",
            "1",
            str(output_path),
        ],
        check=True,
    )


def build_generation_plan() -> tuple[list[tuple[str, Path, Path]], list[str], bool]:
    to_generate: list[tuple[str, Path, Path]] = []
    unresolved: list[str] = []

    for asset_id in used_combat_asset_ids():
        if icon_exists(asset_id):
            continue

        sprite_path = sprite_path_for_asset(asset_id)
        if sprite_path is None:
            unresolved.append(asset_id)
            continue

        to_generate.append((asset_id, sprite_path, COMBAT_ICONS_DIR / f"{asset_id}.png"))

    return to_generate, unresolved, not BABY_LYOO_ICON.exists()


def print_plan(generatable: list[tuple[str, Path, Path]], unresolved: list[str], needs_baby_placeholder: bool) -> None:
    print(f"combat_icons_to_generate={len(generatable)}")
    for asset_id, source_path, output_path in generatable:
        crop_x, crop_y, crop_w, crop_h = alpha_aware_crop(source_path)
        print(
            f"  {asset_id}: {relative_path(source_path)} -> {relative_path(output_path)} "
            f"[crop={crop_w}x{crop_h}+{crop_x}+{crop_y}]"
        )

    print(f"unresolved_assets_without_sprite={len(unresolved)}")
    for asset_id in unresolved:
        print(f"  {asset_id}")

    print(f"needs_baby_lyoo_placeholder={'yes' if needs_baby_placeholder else 'no'}")
    if needs_baby_placeholder:
        print(f"  {relative_path(LYOO_PLACEHOLDER_SOURCE)} -> {relative_path(BABY_LYOO_ICON)}")


def ensure_ffmpeg_available() -> None:
    if shutil.which("ffmpeg") is None:
        raise SystemExit("ffmpeg is required to generate icon crops.")


def ensure_baby_placeholder() -> None:
    if BABY_LYOO_ICON.exists():
        return
    if not LYOO_PLACEHOLDER_SOURCE.exists():
        raise SystemExit(f"Missing Baby Lyoo placeholder source: {relative_path(LYOO_PLACEHOLDER_SOURCE)}")
    run_ffmpeg(LYOO_PLACEHOLDER_SOURCE, BABY_LYOO_ICON, (0, 0, OUTPUT_SIZE, OUTPUT_SIZE))


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Generate missing combat icons and VN icon placeholders.")
    parser.add_argument(
        "--check",
        action="store_true",
        help="Report which assets still need generated icons without writing files.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    generatable, unresolved, needs_baby_placeholder = build_generation_plan()
    print_plan(generatable, unresolved, needs_baby_placeholder)

    if args.check:
        return 1 if generatable or needs_baby_placeholder else 0

    ensure_ffmpeg_available()

    for _, source_path, output_path in generatable:
        run_ffmpeg(source_path, output_path, alpha_aware_crop(source_path))

    ensure_baby_placeholder()

    generated_count = len(generatable) + (1 if needs_baby_placeholder else 0)
    print(f"generated_files={generated_count}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
