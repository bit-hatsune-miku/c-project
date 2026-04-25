#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
import math
from dataclasses import asdict, dataclass
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


REPO_ROOT = Path(__file__).resolve().parent.parent
STAGES_PATH = REPO_ROOT / "assets" / "combat" / "stages.json"
DEFAULT_OUTPUT_DIR = REPO_ROOT / "docs" / "combat-authoring" / "backdrop-guides"
FONT_PATH = REPO_ROOT / "assets" / "fonts" / "SpaceMono-Regular.ttf"

REFERENCE_SCREEN_W = 1280
REFERENCE_SCREEN_H = 720
DEFAULT_CAMERA = {
    "posX": -405.0,
    "posY": 45.0,
    "posZ": -175.0,
    "pitchDegrees": 5.0,
    "yawDegrees": 4.0,
    "focalLength": 50000.0,
}

DEFAULT_SCREEN_TEMPLATE = (1920, 1080)
DEFAULT_PARALLAX_TEMPLATE = (2560, 1440)
DEFAULT_PANORAMA_TEMPLATE = (2048, 1024)
DEFAULT_SKYBOX_FACE_SIZE = 1024
DEFAULT_PARALLAX_STRENGTH_X = 0.18
DEFAULT_PARALLAX_STRENGTH_Y = 0.12
DEFAULT_TRANSITION_BAND_PX = 48.0
NEAR_CLIP_DEPTH = 1.0
BACKDROP_PI = 3.14159265
BACKDROP_TWO_PI = BACKDROP_PI * 2.0
BACKDROP_FOCAL_SCALE = 0.01

SCREEN_MARGIN = 200.0
FLOOR_SAMPLE_STEPS = 220
SKYBOX_SAMPLES = 900

COLOR_BORDER = (255, 255, 255, 220)
COLOR_FRAME = (255, 255, 255, 180)
COLOR_GRID = (255, 255, 255, 44)
COLOR_TEXT = (245, 245, 245, 255)
COLOR_TEXT_BG = (10, 12, 18, 214)
COLOR_CUT_FILL = (235, 64, 52, 52)
COLOR_CUT_LINE = (255, 96, 82, 255)
COLOR_BLEND_FILL = (255, 196, 74, 70)
COLOR_BLEND_LINE = (255, 210, 115, 255)
COLOR_DIAG_FILL = (166, 92, 255, 42)
COLOR_DIAG_LINE = (195, 128, 255, 255)

FACE_NAMES = ("front", "back", "left", "right", "top", "bottom")


@dataclass
class FloorDefinition:
    centerX: float
    centerY: float
    width: float
    depth: float


@dataclass
class FloorCutMetrics:
    normal_cut_y: float
    tiny_cut_min_y: float
    tiny_cut_max_y: float
    tiny_cut_average_y: float
    zero_floor_stage_keys: list[str]


@dataclass
class PanoramaWindow:
    u0: float
    u1: float
    v0: float
    v1: float


@dataclass
class ParallaxWindow:
    u0: float
    u1: float
    v0: float
    v1: float
    quad_x0: float
    quad_y0: float
    quad_x1: float
    quad_y1: float


def parse_size(value: str) -> tuple[int, int]:
    try:
        width_text, height_text = value.lower().split("x", 1)
        width = int(width_text)
        height = int(height_text)
    except ValueError as exc:
        raise argparse.ArgumentTypeError(f"Invalid size '{value}', expected WIDTHxHEIGHT.") from exc
    if width <= 0 or height <= 0:
        raise argparse.ArgumentTypeError(f"Invalid size '{value}', dimensions must be positive.")
    return width, height


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate Photoshop-ready floor cut templates for screen, parallax, panorama, and skybox backdrops."
    )
    parser.add_argument("--out-dir", type=Path, default=DEFAULT_OUTPUT_DIR, help="Output directory for the generated guides.")
    parser.add_argument(
        "--screen-size",
        type=parse_size,
        default=DEFAULT_SCREEN_TEMPLATE,
        help=f"Screen template size, default {DEFAULT_SCREEN_TEMPLATE[0]}x{DEFAULT_SCREEN_TEMPLATE[1]}.",
    )
    parser.add_argument(
        "--parallax-size",
        type=parse_size,
        default=DEFAULT_PARALLAX_TEMPLATE,
        help=f"Parallax template size, default {DEFAULT_PARALLAX_TEMPLATE[0]}x{DEFAULT_PARALLAX_TEMPLATE[1]}.",
    )
    parser.add_argument(
        "--panorama-size",
        type=parse_size,
        default=DEFAULT_PANORAMA_TEMPLATE,
        help=f"Panorama template size, default {DEFAULT_PANORAMA_TEMPLATE[0]}x{DEFAULT_PANORAMA_TEMPLATE[1]}.",
    )
    parser.add_argument(
        "--skybox-face-size",
        type=int,
        default=DEFAULT_SKYBOX_FACE_SIZE,
        help=f"Skybox face size in pixels, default {DEFAULT_SKYBOX_FACE_SIZE}.",
    )
    parser.add_argument(
        "--parallax-strength-x",
        type=float,
        default=DEFAULT_PARALLAX_STRENGTH_X,
        help=f"Parallax X strength, default {DEFAULT_PARALLAX_STRENGTH_X}.",
    )
    parser.add_argument(
        "--parallax-strength-y",
        type=float,
        default=DEFAULT_PARALLAX_STRENGTH_Y,
        help=f"Parallax Y strength, default {DEFAULT_PARALLAX_STRENGTH_Y}.",
    )
    parser.add_argument(
        "--transition-band-px",
        type=float,
        default=DEFAULT_TRANSITION_BAND_PX,
        help=f"Blend band height in 1280x720 reference pixels, default {DEFAULT_TRANSITION_BAND_PX}.",
    )
    return parser.parse_args()


def load_stage_registry() -> dict:
    return json.loads(STAGES_PATH.read_text())


def clamp(value: float, minimum: float, maximum: float) -> float:
    return max(minimum, min(maximum, value))


def wrap01(value: float) -> float:
    wrapped = math.fmod(value, 1.0)
    if wrapped < 0.0:
        wrapped += 1.0
    return wrapped


def effective_focal(camera: dict) -> float:
    return max(1.0, float(camera["focalLength"]) * BACKDROP_FOCAL_SCALE)


def get_depth(camera: dict, world_x: float, world_y: float, world_z: float) -> float:
    px = world_x - float(camera["posX"])
    py = world_y - float(camera["posY"])
    pz = world_z - float(camera["posZ"])

    yaw = math.radians(float(camera["yawDegrees"]))
    cos_yaw = math.cos(yaw)
    sin_yaw = math.sin(yaw)

    y1 = -px * sin_yaw + py * cos_yaw
    z1 = pz

    pitch = math.radians(float(camera["pitchDegrees"]))
    cos_pitch = math.cos(pitch)
    sin_pitch = math.sin(pitch)
    return y1 * cos_pitch + z1 * sin_pitch


def world_to_screen(camera: dict, world_x: float, world_y: float, world_z: float, screen_w: int, screen_h: int) -> tuple[float, float] | None:
    px = world_x - float(camera["posX"])
    py = world_y - float(camera["posY"])
    pz = world_z - float(camera["posZ"])

    yaw = math.radians(float(camera["yawDegrees"]))
    cos_yaw = math.cos(yaw)
    sin_yaw = math.sin(yaw)

    x1 = px * cos_yaw + py * sin_yaw
    y1 = -px * sin_yaw + py * cos_yaw
    z1 = pz

    pitch = math.radians(float(camera["pitchDegrees"]))
    cos_pitch = math.cos(pitch)
    sin_pitch = math.sin(pitch)

    yc = y1 * cos_pitch + z1 * sin_pitch
    zc = y1 * sin_pitch - z1 * cos_pitch
    if yc <= 0.01:
        return None

    focal = effective_focal(camera)
    screen_center_x = screen_w * 0.5
    screen_center_y = screen_h * 0.5
    return (
        screen_center_x + focal * (x1 / yc),
        screen_center_y - focal * (zc / yc),
    )


def sample_floor_cut_y(camera: dict, floor: FloorDefinition, screen_w: int, screen_h: int) -> float:
    width = max(64.0, float(floor.width))
    depth = max(64.0, float(floor.depth))
    start_x = float(floor.centerX) - width * 0.5
    start_y = float(floor.centerY) - depth * 0.5

    best_y: float | None = None
    for ix in range(FLOOR_SAMPLE_STEPS + 1):
        world_x = start_x + width * (ix / FLOOR_SAMPLE_STEPS)
        for iy in range(FLOOR_SAMPLE_STEPS + 1):
            world_y = start_y + depth * (iy / FLOOR_SAMPLE_STEPS)
            depth_value = get_depth(camera, world_x, world_y, 0.0)
            if depth_value <= NEAR_CLIP_DEPTH:
                continue
            projected = world_to_screen(camera, world_x, world_y, 0.0, screen_w, screen_h)
            if projected is None:
                continue
            screen_x, screen_y = projected
            if screen_x < -SCREEN_MARGIN or screen_x > screen_w + SCREEN_MARGIN:
                continue
            if screen_y < -SCREEN_MARGIN or screen_y > screen_h + SCREEN_MARGIN:
                continue
            best_y = screen_y if best_y is None else min(best_y, screen_y)

    return best_y if best_y is not None else float(screen_h)


def merged_camera_for_stage(stage: dict) -> dict:
    camera = dict(DEFAULT_CAMERA)
    camera.update(stage.get("camera", {}))
    return camera


def load_floor_metrics(stage_registry: dict) -> FloorCutMetrics:
    stages = stage_registry["stages"]
    default_stage = stages["default_stage"]
    default_floor_json = default_stage["floor"]
    default_floor = FloorDefinition(
        centerX=float(default_floor_json["centerX"]),
        centerY=float(default_floor_json["centerY"]),
        width=float(default_floor_json["width"]),
        depth=float(default_floor_json["depth"]),
    )
    normal_cut = sample_floor_cut_y(DEFAULT_CAMERA, default_floor, REFERENCE_SCREEN_W, REFERENCE_SCREEN_H)

    zero_floor_values: list[float] = []
    zero_floor_keys: list[str] = []
    for key, stage in stages.items():
        floor_json = stage.get("floor", {})
        raw_width = float(floor_json.get("width", default_floor.width))
        raw_depth = float(floor_json.get("depth", default_floor.depth))
        if raw_width > 0.0 and raw_depth > 0.0:
            continue
        zero_floor_keys.append(key)
        zero_floor = FloorDefinition(
            centerX=float(floor_json.get("centerX", default_floor.centerX)),
            centerY=float(floor_json.get("centerY", default_floor.centerY)),
            width=raw_width,
            depth=raw_depth,
        )
        cut_y = sample_floor_cut_y(merged_camera_for_stage(stage), zero_floor, REFERENCE_SCREEN_W, REFERENCE_SCREEN_H)
        zero_floor_values.append(cut_y)

    if zero_floor_values:
        tiny_min = min(zero_floor_values)
        tiny_max = max(zero_floor_values)
        tiny_avg = sum(zero_floor_values) / len(zero_floor_values)
    else:
        tiny_min = tiny_max = tiny_avg = normal_cut

    return FloorCutMetrics(
        normal_cut_y=normal_cut,
        tiny_cut_min_y=tiny_min,
        tiny_cut_max_y=tiny_max,
        tiny_cut_average_y=tiny_avg,
        zero_floor_stage_keys=sorted(zero_floor_keys),
    )


def horizontal_fov_radians(camera: dict, screen_w: int) -> float:
    return 2.0 * math.atan(max(1.0, float(screen_w)) / (2.0 * effective_focal(camera)))


def vertical_fov_radians(camera: dict, screen_h: int) -> float:
    return 2.0 * math.atan(max(1.0, float(screen_h)) / (2.0 * effective_focal(camera)))


def compute_panorama_window(camera: dict) -> PanoramaWindow:
    yaw_radians = math.radians(float(camera["yawDegrees"]))
    pitch_radians = math.radians(float(camera["pitchDegrees"]))
    horizontal_span = clamp(horizontal_fov_radians(camera, REFERENCE_SCREEN_W) / BACKDROP_TWO_PI, 0.08, 1.0)
    vertical_span = clamp(vertical_fov_radians(camera, REFERENCE_SCREEN_H) / BACKDROP_PI, 0.08, 1.0)
    center_u = wrap01(0.5 - yaw_radians / BACKDROP_TWO_PI)
    center_v = clamp(0.5 + pitch_radians / BACKDROP_PI, vertical_span * 0.5, 1.0 - vertical_span * 0.5)
    return PanoramaWindow(
        u0=center_u - horizontal_span * 0.5,
        u1=center_u + horizontal_span * 0.5,
        v0=center_v - vertical_span * 0.5,
        v1=center_v + vertical_span * 0.5,
    )


def compute_parallax_window(camera: dict, strength_x: float, strength_y: float) -> ParallaxWindow:
    safe_strength_x = clamp(strength_x, 0.0, 1.0)
    safe_strength_y = clamp(strength_y, 0.0, 1.0)
    draw_width = REFERENCE_SCREEN_W * (1.18 + safe_strength_x * 0.35)
    draw_height = REFERENCE_SCREEN_H * (1.14 + safe_strength_y * 0.28)
    max_shift_x = max(0.0, (draw_width - REFERENCE_SCREEN_W) * 0.5)
    max_shift_y = max(0.0, (draw_height - REFERENCE_SCREEN_H) * 0.5)
    yaw_norm = clamp(float(camera["yawDegrees"]) / 12.0, -1.0, 1.0)
    pitch_norm = clamp(float(camera["pitchDegrees"]) / 18.0, -1.0, 1.0)
    shift_x = yaw_norm * max_shift_x * safe_strength_x
    shift_y = -pitch_norm * max_shift_y * safe_strength_y

    quad_x0 = (REFERENCE_SCREEN_W - draw_width) * 0.5 + shift_x
    quad_y0 = (REFERENCE_SCREEN_H - draw_height) * 0.5 + shift_y
    quad_x1 = quad_x0 + draw_width
    quad_y1 = quad_y0 + draw_height
    quad_width = quad_x1 - quad_x0
    quad_height = quad_y1 - quad_y0

    return ParallaxWindow(
        u0=clamp((0.0 - quad_x0) / quad_width, 0.0, 1.0),
        u1=clamp((REFERENCE_SCREEN_W - quad_x0) / quad_width, 0.0, 1.0),
        v0=clamp((0.0 - quad_y0) / quad_height, 0.0, 1.0),
        v1=clamp((REFERENCE_SCREEN_H - quad_y0) / quad_height, 0.0, 1.0),
        quad_x0=quad_x0,
        quad_y0=quad_y0,
        quad_x1=quad_x1,
        quad_y1=quad_y1,
    )


def normalize_point_from_reference(x: float, y: float, output_size: tuple[int, int]) -> tuple[float, float]:
    output_w, output_h = output_size
    return (
        (x / REFERENCE_SCREEN_W) * output_w,
        (y / REFERENCE_SCREEN_H) * output_h,
    )


def norm_to_pixel_y(norm_y: float, height: int) -> float:
    return norm_y * height


def make_canvas(size: tuple[int, int]) -> Image.Image:
    return Image.new("RGBA", size, (0, 0, 0, 0))


def try_font(size: int) -> ImageFont.FreeTypeFont | ImageFont.ImageFont:
    if FONT_PATH.exists():
        return ImageFont.truetype(str(FONT_PATH), size=size)
    return ImageFont.load_default()


def draw_grid(draw: ImageDraw.ImageDraw, size: tuple[int, int], step_x: int, step_y: int) -> None:
    width, height = size
    for x in range(step_x, width, step_x):
        draw.line([(x, 0), (x, height)], fill=COLOR_GRID, width=1)
    for y in range(step_y, height, step_y):
        draw.line([(0, y), (width, y)], fill=COLOR_GRID, width=1)
    draw.rectangle([(0, 0), (width - 1, height - 1)], outline=COLOR_BORDER, width=2)


def draw_label(draw: ImageDraw.ImageDraw, text: str, xy: tuple[float, float], font: ImageFont.ImageFont) -> None:
    bbox = draw.multiline_textbbox((0, 0), text, font=font, spacing=3)
    padding_x = 10
    padding_y = 8
    x = xy[0]
    y = xy[1]
    draw.rounded_rectangle(
        [
            (x, y),
            (x + (bbox[2] - bbox[0]) + padding_x * 2, y + (bbox[3] - bbox[1]) + padding_y * 2),
        ],
        radius=10,
        fill=COLOR_TEXT_BG,
        outline=(255, 255, 255, 70),
        width=1,
    )
    draw.multiline_text((x + padding_x, y + padding_y), text, font=font, fill=COLOR_TEXT, spacing=3)


def draw_dashed_line(
    draw: ImageDraw.ImageDraw,
    start: tuple[float, float],
    end: tuple[float, float],
    fill: tuple[int, int, int, int],
    width: int = 3,
    dash: float = 18.0,
    gap: float = 12.0,
) -> None:
    dx = end[0] - start[0]
    dy = end[1] - start[1]
    length = math.hypot(dx, dy)
    if length <= 0.001:
        return
    ux = dx / length
    uy = dy / length
    distance = 0.0
    while distance < length:
        seg_start = distance
        seg_end = min(length, distance + dash)
        a = (start[0] + ux * seg_start, start[1] + uy * seg_start)
        b = (start[0] + ux * seg_end, start[1] + uy * seg_end)
        draw.line([a, b], fill=fill, width=width)
        distance += dash + gap


def draw_dashed_polyline(
    draw: ImageDraw.ImageDraw,
    points: list[tuple[float, float]],
    fill: tuple[int, int, int, int],
    width: int = 3,
) -> None:
    if len(points) < 2:
        return
    for index in range(len(points) - 1):
        draw_dashed_line(draw, points[index], points[index + 1], fill=fill, width=width, dash=14.0, gap=10.0)


def draw_screen_template(output_path: Path, metrics: FloorCutMetrics, transition_band_px: float, size: tuple[int, int]) -> dict:
    image = make_canvas(size)
    draw = ImageDraw.Draw(image)
    title_font = try_font(28)
    body_font = try_font(18)

    draw_grid(draw, size, max(1, size[0] // 8), max(1, size[1] // 8))

    scale_y = size[1] / REFERENCE_SCREEN_H
    normal_cut_y = metrics.normal_cut_y * scale_y
    transition_top_y = (metrics.normal_cut_y - transition_band_px) * scale_y
    tiny_min_y = metrics.tiny_cut_min_y * scale_y
    tiny_max_y = metrics.tiny_cut_max_y * scale_y

    draw.rectangle([(0, normal_cut_y), (size[0], size[1])], fill=COLOR_CUT_FILL)
    draw.rectangle([(0, transition_top_y), (size[0], normal_cut_y)], fill=COLOR_BLEND_FILL)
    if tiny_max_y > tiny_min_y:
        draw.rectangle([(0, tiny_min_y), (size[0], tiny_max_y)], fill=COLOR_DIAG_FILL)

    draw.line([(0, transition_top_y), (size[0], transition_top_y)], fill=COLOR_BLEND_LINE, width=4)
    draw.line([(0, normal_cut_y), (size[0], normal_cut_y)], fill=COLOR_CUT_LINE, width=6)
    if tiny_max_y > tiny_min_y:
        draw_dashed_line(draw, (0, tiny_min_y), (size[0], tiny_min_y), fill=COLOR_DIAG_LINE, width=3)
        draw_dashed_line(draw, (0, tiny_max_y), (size[0], tiny_max_y), fill=COLOR_DIAG_LINE, width=3)

    draw_label(draw, "Screen Backdrop Template", (28, 24), title_font)
    draw_label(draw, "Blend the photo ground into the game floor\ninside this amber band.", (28, max(72.0, transition_top_y - 86.0)), body_font)
    draw_label(draw, "Normal floor cut.\nAnything important below this red line will be hidden.", (28, normal_cut_y + 16.0), body_font)
    draw_label(draw, "Current size-0 floor stages land around this band.\nUse it as a diagnostic, not the target.", (28, max(normal_cut_y + 118.0, tiny_min_y + 18.0)), body_font)

    image.save(output_path)
    return {
        "template_size": list(size),
        "normal_cut_y": round(normal_cut_y, 2),
        "transition_top_y": round(transition_top_y, 2),
        "tiny_cut_range_y": [round(tiny_min_y, 2), round(tiny_max_y, 2)],
    }


def draw_parallax_template(
    output_path: Path,
    camera: dict,
    metrics: FloorCutMetrics,
    transition_band_px: float,
    size: tuple[int, int],
    strength_x: float,
    strength_y: float,
) -> dict:
    image = make_canvas(size)
    draw = ImageDraw.Draw(image)
    title_font = try_font(26)
    body_font = try_font(18)

    draw_grid(draw, size, max(1, size[0] // 10), max(1, size[1] // 10))

    parallax = compute_parallax_window(camera, strength_x, strength_y)
    crop_left = parallax.u0 * size[0]
    crop_right = parallax.u1 * size[0]
    crop_top = parallax.v0 * size[1]
    crop_bottom = parallax.v1 * size[1]
    crop_height = crop_bottom - crop_top

    normal_cut_norm = (metrics.normal_cut_y - parallax.quad_y0) / (parallax.quad_y1 - parallax.quad_y0)
    transition_top_norm = ((metrics.normal_cut_y - transition_band_px) - parallax.quad_y0) / (parallax.quad_y1 - parallax.quad_y0)
    tiny_min_norm = (metrics.tiny_cut_min_y - parallax.quad_y0) / (parallax.quad_y1 - parallax.quad_y0)
    tiny_max_norm = (metrics.tiny_cut_max_y - parallax.quad_y0) / (parallax.quad_y1 - parallax.quad_y0)

    normal_cut_y = clamp(normal_cut_norm, 0.0, 1.0) * size[1]
    transition_top_y = clamp(transition_top_norm, 0.0, 1.0) * size[1]
    tiny_min_y = clamp(tiny_min_norm, 0.0, 1.0) * size[1]
    tiny_max_y = clamp(tiny_max_norm, 0.0, 1.0) * size[1]

    draw.rectangle([(crop_left, crop_top), (crop_right, crop_bottom)], outline=COLOR_FRAME, width=4)
    draw.rectangle([(crop_left, normal_cut_y), (crop_right, crop_bottom)], fill=COLOR_CUT_FILL)
    draw.rectangle([(crop_left, transition_top_y), (crop_right, normal_cut_y)], fill=COLOR_BLEND_FILL)
    if tiny_max_y > tiny_min_y:
        draw.rectangle([(crop_left, tiny_min_y), (crop_right, tiny_max_y)], fill=COLOR_DIAG_FILL)

    draw.line([(crop_left, transition_top_y), (crop_right, transition_top_y)], fill=COLOR_BLEND_LINE, width=4)
    draw.line([(crop_left, normal_cut_y), (crop_right, normal_cut_y)], fill=COLOR_CUT_LINE, width=6)
    if tiny_max_y > tiny_min_y:
        draw_dashed_line(draw, (crop_left, tiny_min_y), (crop_right, tiny_min_y), fill=COLOR_DIAG_LINE, width=3)
        draw_dashed_line(draw, (crop_left, tiny_max_y), (crop_right, tiny_max_y), fill=COLOR_DIAG_LINE, width=3)

    draw_label(draw, "Parallax Backdrop Template", (28, 24), title_font)
    draw_label(
        draw,
        "White rectangle = what the game actually sees.\nOuter area is overscan drift space.",
        (28, 86),
        body_font,
    )
    draw_label(draw, "Blend band for the photo ground.", (crop_left + 20.0, max(28.0, transition_top_y - 56.0)), body_font)
    draw_label(draw, "Normal floor cut inside the visible frame.", (crop_left + 20.0, normal_cut_y + 16.0), body_font)
    if tiny_max_y > tiny_min_y:
        draw_label(draw, "Size-0 floor diagnostic band.", (crop_left + 20.0, min(size[1] - 70.0, tiny_max_y + 18.0)), body_font)

    image.save(output_path)
    return {
        "template_size": list(size),
        "parallax_strength": [strength_x, strength_y],
        "visible_uv_window": {
            "u0": round(parallax.u0, 6),
            "u1": round(parallax.u1, 6),
            "v0": round(parallax.v0, 6),
            "v1": round(parallax.v1, 6),
        },
        "normal_cut_y": round(normal_cut_y, 2),
        "transition_top_y": round(transition_top_y, 2),
        "tiny_cut_range_y": [round(tiny_min_y, 2), round(tiny_max_y, 2)],
        "visible_frame_pixels": {
            "left": round(crop_left, 2),
            "top": round(crop_top, 2),
            "right": round(crop_right, 2),
            "bottom": round(crop_bottom, 2),
            "height": round(crop_height, 2),
        },
    }


def panorama_rectangles(window: PanoramaWindow, output_w: int, output_h: int) -> list[tuple[float, float, float, float]]:
    if window.u0 >= 0.0 and window.u1 <= 1.0:
        return [(window.u0 * output_w, window.v0 * output_h, window.u1 * output_w, window.v1 * output_h)]
    if window.u0 < 0.0:
        return [
            ((1.0 + window.u0) * output_w, window.v0 * output_h, output_w, window.v1 * output_h),
            (0.0, window.v0 * output_h, window.u1 * output_w, window.v1 * output_h),
        ]
    return [
        (window.u0 * output_w, window.v0 * output_h, output_w, window.v1 * output_h),
        (0.0, window.v0 * output_h, (window.u1 - 1.0) * output_w, window.v1 * output_h),
    ]


def draw_panorama_template(
    output_path: Path,
    camera: dict,
    metrics: FloorCutMetrics,
    transition_band_px: float,
    size: tuple[int, int],
) -> dict:
    image = make_canvas(size)
    draw = ImageDraw.Draw(image)
    title_font = try_font(26)
    body_font = try_font(18)

    draw_grid(draw, size, max(1, size[0] // 16), max(1, size[1] // 8))

    window = compute_panorama_window(camera)
    vertical_span = window.v1 - window.v0
    normal_cut_v = window.v0 + (metrics.normal_cut_y / REFERENCE_SCREEN_H) * vertical_span
    transition_top_v = window.v0 + ((metrics.normal_cut_y - transition_band_px) / REFERENCE_SCREEN_H) * vertical_span
    tiny_min_v = window.v0 + (metrics.tiny_cut_min_y / REFERENCE_SCREEN_H) * vertical_span
    tiny_max_v = window.v0 + (metrics.tiny_cut_max_y / REFERENCE_SCREEN_H) * vertical_span

    normal_cut_y = norm_to_pixel_y(normal_cut_v, size[1])
    transition_top_y = norm_to_pixel_y(transition_top_v, size[1])
    tiny_min_y = norm_to_pixel_y(tiny_min_v, size[1])
    tiny_max_y = norm_to_pixel_y(tiny_max_v, size[1])

    draw.rectangle([(0, normal_cut_y), (size[0], size[1])], fill=COLOR_CUT_FILL)
    draw.rectangle([(0, transition_top_y), (size[0], normal_cut_y)], fill=COLOR_BLEND_FILL)
    if tiny_max_y > tiny_min_y:
        draw.rectangle([(0, tiny_min_y), (size[0], tiny_max_y)], fill=COLOR_DIAG_FILL)

    draw.line([(0, transition_top_y), (size[0], transition_top_y)], fill=COLOR_BLEND_LINE, width=4)
    draw.line([(0, normal_cut_y), (size[0], normal_cut_y)], fill=COLOR_CUT_LINE, width=6)
    if tiny_max_y > tiny_min_y:
        draw_dashed_line(draw, (0, tiny_min_y), (size[0], tiny_min_y), fill=COLOR_DIAG_LINE, width=3)
        draw_dashed_line(draw, (0, tiny_max_y), (size[0], tiny_max_y), fill=COLOR_DIAG_LINE, width=3)

    crop_rects = panorama_rectangles(window, size[0], size[1])
    for rect in crop_rects:
        draw.rectangle([(rect[0], rect[1]), (rect[2], rect[3])], outline=COLOR_FRAME, width=4)

    draw_label(draw, "Panorama Template", (28, 24), title_font)
    draw_label(
        draw,
        "White rectangle = default camera crop.\nAnything important below the red line is a bad handoff into the game floor.",
        (28, 86),
        body_font,
    )
    draw_label(draw, "Blend the panorama ground through this amber band.", (28, max(136.0, transition_top_y - 62.0)), body_font)
    draw_label(draw, "Current size-0 floors land around this diagnostic band.", (28, min(size[1] - 70.0, tiny_max_y + 16.0)), body_font)

    image.save(output_path)
    return {
        "template_size": list(size),
        "crop_window": {
            "u0": round(window.u0, 6),
            "u1": round(window.u1, 6),
            "v0": round(window.v0, 6),
            "v1": round(window.v1, 6),
        },
        "normal_cut_v": round(normal_cut_v, 6),
        "transition_top_v": round(transition_top_v, 6),
        "tiny_cut_v_range": [round(tiny_min_v, 6), round(tiny_max_v, 6)],
    }


def normalize_vector(x: float, y: float, z: float) -> tuple[float, float, float]:
    length = math.sqrt(x * x + y * y + z * z)
    if length <= 0.00001:
        return (0.0, 1.0, 0.0)
    return (x / length, y / length, z / length)


def camera_space_to_world_direction(camera: dict, x: float, y: float, z: float) -> tuple[float, float, float]:
    x, y, z = normalize_vector(x, y, z)

    pitch = math.radians(float(camera["pitchDegrees"]))
    cos_pitch = math.cos(pitch)
    sin_pitch = math.sin(pitch)
    x1 = x
    y1 = y * cos_pitch + z * sin_pitch
    z1 = y * sin_pitch - z * cos_pitch

    yaw = math.radians(float(camera["yawDegrees"]))
    cos_yaw = math.cos(yaw)
    sin_yaw = math.sin(yaw)
    return normalize_vector(
        x1 * cos_yaw - y1 * sin_yaw,
        x1 * sin_yaw + y1 * cos_yaw,
        z1,
    )


def world_direction_for_screen_point(camera: dict, screen_x: float, screen_y: float) -> tuple[float, float, float]:
    focal = effective_focal(camera)
    projected_x = (screen_x - (REFERENCE_SCREEN_W * 0.5)) / focal
    projected_z = -(screen_y - (REFERENCE_SCREEN_H * 0.5)) / focal
    return camera_space_to_world_direction(camera, projected_x, 1.0, projected_z)


def project_direction_to_face(direction: tuple[float, float, float], face: str) -> tuple[float, float]:
    x, y, z = direction
    abs_x = abs(x)
    abs_y = abs(y)
    abs_z = abs(z)
    major_axis = 1.0

    if face == "front":
        major_axis = max(0.0001, abs_y)
        u = 0.5 + x / (2.0 * major_axis)
        v = 0.5 - z / (2.0 * major_axis)
    elif face == "back":
        major_axis = max(0.0001, abs_y)
        u = 0.5 - x / (2.0 * major_axis)
        v = 0.5 - z / (2.0 * major_axis)
    elif face == "left":
        major_axis = max(0.0001, abs_x)
        u = 0.5 + y / (2.0 * major_axis)
        v = 0.5 - z / (2.0 * major_axis)
    elif face == "right":
        major_axis = max(0.0001, abs_x)
        u = 0.5 - y / (2.0 * major_axis)
        v = 0.5 - z / (2.0 * major_axis)
    elif face == "top":
        major_axis = max(0.0001, abs_z)
        u = 0.5 + x / (2.0 * major_axis)
        v = 0.5 + y / (2.0 * major_axis)
    else:
        major_axis = max(0.0001, abs_z)
        u = 0.5 + x / (2.0 * major_axis)
        v = 0.5 - y / (2.0 * major_axis)

    return (clamp(u, 0.0, 1.0), clamp(1.0 - v, 0.0, 1.0))


def project_direction_to_skybox(direction: tuple[float, float, float]) -> tuple[str, float, float]:
    x, y, z = direction
    abs_x = abs(x)
    abs_y = abs(y)
    abs_z = abs(z)

    if abs_y >= abs_x and abs_y >= abs_z:
        face = "front" if y >= 0.0 else "back"
    elif abs_x >= abs_y and abs_x >= abs_z:
        face = "right" if x >= 0.0 else "left"
    else:
        face = "top" if z >= 0.0 else "bottom"
    u, v = project_direction_to_face(direction, face)
    return (face, u, v)


def sample_skybox_curve(camera: dict, screen_y: float) -> dict[str, list[list[tuple[float, float]]]]:
    segments: dict[str, list[list[tuple[float, float]]]] = {face: [] for face in FACE_NAMES}
    current_face: str | None = None
    current_points: list[tuple[float, float]] = []

    for index in range(SKYBOX_SAMPLES):
        x = (REFERENCE_SCREEN_W - 1) * (index / max(1, SKYBOX_SAMPLES - 1))
        face, u, v = project_direction_to_skybox(world_direction_for_screen_point(camera, x, screen_y))
        point = (u, v)
        if current_face is None:
            current_face = face
            current_points = [point]
            continue

        if face != current_face:
            if current_points:
                segments[current_face].append(current_points)
            current_face = face
            current_points = [point]
            continue

        if current_points:
            prev_u, prev_v = current_points[-1]
            if abs(prev_u - u) > 0.30 or abs(prev_v - v) > 0.30:
                segments[current_face].append(current_points)
                current_points = [point]
                continue

        current_points.append(point)

    if current_face is not None and current_points:
        segments[current_face].append(current_points)
    return segments


def scaled_segments(segments: list[list[tuple[float, float]]], face_size: int) -> list[list[tuple[float, float]]]:
    return [[(u * face_size, v * face_size) for u, v in segment] for segment in segments]


def draw_edge_labels(draw: ImageDraw.ImageDraw, size: int, font: ImageFont.ImageFont) -> None:
    labels = (
        ("TOP", "top"),
        ("BOTTOM", "bottom"),
        ("LEFT", "left"),
        ("RIGHT", "right"),
    )
    for text, edge in labels:
        bbox = draw.textbbox((0, 0), text, font=font)
        width = bbox[2] - bbox[0]
        height = bbox[3] - bbox[1]
        if edge == "top":
            xy = ((size - width) * 0.5, 8)
        elif edge == "bottom":
            xy = ((size - width) * 0.5, size - height - 8)
        elif edge == "left":
            xy = (8, (size - height) * 0.5)
        else:
            xy = (size - width - 8, (size - height) * 0.5)
        draw.text(xy, text, fill=(255, 255, 255, 110), font=font)


def draw_skybox_guides(
    output_dir: Path,
    camera: dict,
    metrics: FloorCutMetrics,
    transition_band_px: float,
    face_size: int,
) -> dict:
    transition_segments = sample_skybox_curve(camera, metrics.normal_cut_y - transition_band_px)
    normal_segments = sample_skybox_curve(camera, metrics.normal_cut_y)
    tiny_segments = sample_skybox_curve(camera, metrics.tiny_cut_average_y)

    title_font = try_font(max(18, face_size // 32))
    edge_font = try_font(max(14, face_size // 54))
    body_font = try_font(max(14, face_size // 56))

    result: dict[str, dict] = {}
    for face in FACE_NAMES:
        image = make_canvas((face_size, face_size))
        draw = ImageDraw.Draw(image)
        draw_grid(draw, (face_size, face_size), max(1, face_size // 8), max(1, face_size // 8))
        draw_edge_labels(draw, face_size, edge_font)

        transition = scaled_segments(transition_segments[face], face_size)
        normal = scaled_segments(normal_segments[face], face_size)
        tiny = scaled_segments(tiny_segments[face], face_size)

        for segment in transition:
            if len(segment) >= 2:
                draw.line(segment, fill=COLOR_BLEND_LINE, width=max(3, face_size // 170))
        for segment in normal:
            if len(segment) >= 2:
                draw.line(segment, fill=COLOR_CUT_LINE, width=max(5, face_size // 128))
        for segment in tiny:
            draw_dashed_polyline(draw, segment, fill=COLOR_DIAG_LINE, width=max(3, face_size // 220))

        draw_label(draw, f"Skybox Face: {face}", (18, 18), title_font)
        if normal:
            draw_label(draw, "Red curve = floor cut\nAmber curve = blend band", (18, 76), body_font)
        else:
            draw_label(draw, "Default camera floor cut does not hit this face.\nKeep it consistent for seams anyway.", (18, 76), body_font)

        output_path = output_dir / f"skybox_{face}.png"
        image.save(output_path)
        result[face] = {
            "output_path": output_path.name,
            "segment_count": len(normal),
            "transition_segment_count": len(transition),
            "diagnostic_segment_count": len(tiny),
        }

    return result


def write_readme(output_dir: Path) -> None:
    content = """# Backdrop Floor Cut Guides

These guides are Photoshop-ready overlays for the current battle camera.

How to read them:

- red = the in-game floor is covering the backdrop here
- amber = blend the real photo ground through this band so the handoff feels intentional
- magenta = current size-0 floor diagnostic range from the repo; useful for spotting floating/cut-in-half stages, not the target
- white rectangle = what the game actually sees from the larger source image

Files:

- `screen_template.png`: fixed full-screen backdrops
- `parallax_template.png`: oversized parallax source art
- `panorama_template.png`: 2:1 panorama source art
- `skybox_*.png`: one guide for each cube face
- `guide_notes.json`: numeric values used to generate the guides

Regenerate with:

```bash
python3 scripts/generate_floor_cut_guides.py
```
"""
    (output_dir / "README.md").write_text(content)


def main() -> int:
    args = parse_args()
    args.out_dir.mkdir(parents=True, exist_ok=True)

    stage_registry = load_stage_registry()
    metrics = load_floor_metrics(stage_registry)

    transition_norm = args.transition_band_px / REFERENCE_SCREEN_H
    notes = {
        "reference_scene": {
            "screen_size": [REFERENCE_SCREEN_W, REFERENCE_SCREEN_H],
            "camera": dict(DEFAULT_CAMERA),
            "transition_band_px": args.transition_band_px,
        },
        "floor_metrics": {
            "normal_cut_y_at_1280x720": round(metrics.normal_cut_y, 4),
            "tiny_cut_range_y_at_1280x720": [round(metrics.tiny_cut_min_y, 4), round(metrics.tiny_cut_max_y, 4)],
            "tiny_cut_average_y_at_1280x720": round(metrics.tiny_cut_average_y, 4),
            "size_zero_floor_stage_keys": metrics.zero_floor_stage_keys,
            "transition_band_norm": round(transition_norm, 6),
        },
    }

    notes["screen"] = draw_screen_template(
        args.out_dir / "screen_template.png",
        metrics,
        args.transition_band_px,
        args.screen_size,
    )
    notes["parallax"] = draw_parallax_template(
        args.out_dir / "parallax_template.png",
        dict(DEFAULT_CAMERA),
        metrics,
        args.transition_band_px,
        args.parallax_size,
        args.parallax_strength_x,
        args.parallax_strength_y,
    )
    notes["panorama"] = draw_panorama_template(
        args.out_dir / "panorama_template.png",
        dict(DEFAULT_CAMERA),
        metrics,
        args.transition_band_px,
        args.panorama_size,
    )
    notes["skybox"] = draw_skybox_guides(
        args.out_dir,
        dict(DEFAULT_CAMERA),
        metrics,
        args.transition_band_px,
        args.skybox_face_size,
    )

    (args.out_dir / "guide_notes.json").write_text(json.dumps(notes, indent=2))
    write_readme(args.out_dir)

    print(f"generated_guides_dir={args.out_dir.relative_to(REPO_ROOT).as_posix()}")
    for name in (
        "screen_template.png",
        "parallax_template.png",
        "panorama_template.png",
        "skybox_front.png",
        "skybox_back.png",
        "skybox_left.png",
        "skybox_right.png",
        "skybox_top.png",
        "skybox_bottom.png",
        "guide_notes.json",
        "README.md",
    ):
        print(f"  {name}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
