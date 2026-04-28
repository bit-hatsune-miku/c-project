from __future__ import annotations

from pathlib import Path
from typing import Tuple

from PIL import Image, ImageEnhance, ImageFilter


ROOT = Path(__file__).resolve().parents[1]
REFS_DIR = ROOT / "assets" / "vn" / "backgrounds" / "deadline_refs"
OUT_DIR = ROOT / "assets" / "vn" / "backgrounds" / "deadline"
TARGET_SIZE = (1280, 720)


def ensure_rgb(image: Image.Image) -> Image.Image:
    if image.mode != "RGB":
        return image.convert("RGB")
    return image


def fit_and_crop(
    image: Image.Image,
    target_size: Tuple[int, int],
    focal_x: float = 0.5,
    focal_y: float = 0.5,
) -> Image.Image:
    src_w, src_h = image.size
    target_w, target_h = target_size
    src_ratio = src_w / src_h
    target_ratio = target_w / target_h

    if src_ratio > target_ratio:
        crop_h = src_h
        crop_w = int(round(crop_h * target_ratio))
    else:
        crop_w = src_w
        crop_h = int(round(crop_w / target_ratio))

    max_left = max(0, src_w - crop_w)
    max_top = max(0, src_h - crop_h)
    left = int(round(max_left * focal_x))
    top = int(round(max_top * focal_y))
    left = min(max(left, 0), max_left)
    top = min(max(top, 0), max_top)

    cropped = image.crop((left, top, left + crop_w, top + crop_h))
    return cropped.resize(target_size, Image.Resampling.LANCZOS)


def save_deadline_bg(ref_name: str, out_name: str, focal_x: float, focal_y: float) -> None:
    image = ensure_rgb(Image.open(REFS_DIR / ref_name))
    fitted = fit_and_crop(image, TARGET_SIZE, focal_x=focal_x, focal_y=focal_y)
    fitted.save(OUT_DIR / out_name)


def save_sailor_venus() -> None:
    image = ensure_rgb(Image.open(REFS_DIR / "sailor_venus_source.png"))
    fitted = fit_and_crop(image, TARGET_SIZE, focal_x=0.46, focal_y=0.35)
    # The source is tiny, so give it a mild contrast/sharpness lift after upscale.
    fitted = ImageEnhance.Contrast(fitted).enhance(1.04)
    fitted = fitted.filter(ImageFilter.UnsharpMask(radius=1.3, percent=90, threshold=2))
    fitted.save(OUT_DIR / "sailorvenus_story.png")


def save_zhao_story() -> None:
    classroom = Image.open(ROOT / "assets" / "combat" / "skies" / "classroom.png").convert("RGBA")
    base = fit_and_crop(classroom, TARGET_SIZE, focal_x=0.5, focal_y=0.22).convert("RGBA")

    # Keep the classroom readable behind dialogue and separate Zhao from the room a bit.
    softened = base.filter(ImageFilter.GaussianBlur(radius=0.8))
    softened = ImageEnhance.Brightness(softened).enhance(0.94)
    softened = ImageEnhance.Contrast(softened).enhance(1.08)

    sprite = Image.open(ROOT / "assets" / "combat" / "sprites" / "randy.png").convert("RGBA")
    scale = 2.05
    sprite = sprite.resize(
        (int(round(sprite.width * scale)), int(round(sprite.height * scale))),
        Image.Resampling.LANCZOS,
    )

    shadow = Image.new("RGBA", softened.size, (0, 0, 0, 0))
    sprite_x = 110
    sprite_y = softened.height - sprite.height - 28
    shadow_sprite = Image.new("RGBA", sprite.size, (0, 0, 0, 135))
    shadow.alpha_composite(shadow_sprite, (sprite_x + 18, sprite_y + 20))
    shadow = shadow.filter(ImageFilter.GaussianBlur(radius=12))

    composed = Image.alpha_composite(softened, shadow)
    composed.alpha_composite(sprite, (sprite_x, sprite_y))

    # Add a subtle bottom gradient to help VN dialogue readability without changing the scene.
    overlay = Image.new("RGBA", composed.size, (0, 0, 0, 0))
    px = overlay.load()
    for y in range(composed.height):
        alpha = 0
        if y > int(composed.height * 0.62):
            progress = (y - composed.height * 0.62) / (composed.height * 0.38)
            alpha = int(110 * min(max(progress, 0.0), 1.0))
        for x in range(composed.width):
            px[x, y] = (0, 0, 0, alpha)

    composed = Image.alpha_composite(composed, overlay)
    composed.convert("RGB").save(OUT_DIR / "zhao_story.png")


def main() -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    save_deadline_bg("teto_source.png", "teto_story.png", focal_x=0.22, focal_y=0.18)
    save_deadline_bg("huafei_source.jpg", "huafei_story.png", focal_x=0.50, focal_y=0.18)
    save_sailor_venus()
    save_deadline_bg("zhoushen_source.jpg", "zhoushen_story.png", focal_x=0.50, focal_y=0.30)
    save_zhao_story()


if __name__ == "__main__":
    main()
