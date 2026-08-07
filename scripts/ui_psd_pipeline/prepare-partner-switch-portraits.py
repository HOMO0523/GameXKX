from __future__ import annotations

import hashlib
import json
from dataclasses import dataclass
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter


PROJECT_ROOT = Path(__file__).resolve().parents[2]
SOURCE_ROOT = PROJECT_ROOT / "SourceAssets" / "AnimationProcessing" / "Production"
OUTPUT_ROOT = (
    PROJECT_ROOT
    / "SourceArt"
    / "UI"
    / "PSD"
    / "gamexxk-v4"
    / "ui-master"
    / "ManualEditing"
    / "PartnerSwitchPortraits"
)
TARGET_SIZE = (105, 62)


@dataclass(frozen=True)
class RoleSpec:
    role: str
    source_folder: str
    crop: tuple[int, int, int, int]
    brightness: float
    contrast: float
    saturation: float

    @property
    def source(self) -> Path:
        return SOURCE_ROOT / self.source_folder / "frames" / "frame_0000.png"


ROLE_SPECS = (
    RoleSpec("blade", "character_01_blade_idle", (210, 82, 420, 206), 1.01, 1.03, 0.95),
    RoleSpec("guard", "character_02_guard_idle", (155, 93, 365, 217), 1.06, 1.01, 0.94),
    RoleSpec("healer", "character_03_healer_idle", (120, 75, 330, 199), 0.98, 1.04, 0.90),
    RoleSpec("hunter", "character_04_hunter_idle", (130, 153, 340, 277), 1.08, 1.02, 0.90),
    RoleSpec("sorcerer", "character_05_sorcerer_idle", (155, 95, 365, 219), 1.04, 1.02, 0.92),
    RoleSpec(
        "formation_master",
        "character_06_formation_master_idle",
        (165, 105, 375, 229),
        1.01,
        1.04,
        0.90,
    ),
)


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def strong_magenta(rgb: np.ndarray) -> np.ndarray:
    red = rgb[..., 0].astype(np.float32)
    green = rgb[..., 1].astype(np.float32)
    blue = rgb[..., 2].astype(np.float32)
    return (
        (red > 75)
        & (blue > 75)
        & (((red + blue) * 0.5 - green) > 34)
        & (red > green * 1.20)
        & (blue > green * 1.20)
    )


def boundary_band(alpha: np.ndarray, radius: int = 4) -> np.ndarray:
    binary = Image.fromarray(np.where(alpha > 2, 255, 0).astype(np.uint8))
    interior = np.asarray(binary.filter(ImageFilter.MinFilter(radius * 2 + 1))) == 255
    return (alpha > 2) & ~interior


def defringe_magenta(image: Image.Image) -> Image.Image:
    data = np.asarray(image.convert("RGBA")).copy()
    rgb = data[..., :3]
    alpha = data[..., 3]
    magenta = strong_magenta(rgb)
    bad = boundary_band(alpha) & magenta
    donor = (alpha >= 96) & ~magenta

    for y, x in np.argwhere(bad):
        replacement = None
        for radius in range(1, 8):
            y0 = max(0, y - radius)
            y1 = min(data.shape[0], y + radius + 1)
            x0 = max(0, x - radius)
            x1 = min(data.shape[1], x + radius + 1)
            candidates = np.argwhere(donor[y0:y1, x0:x1])
            if candidates.size == 0:
                continue
            candidate_y = candidates[:, 0] + y0
            candidate_x = candidates[:, 1] + x0
            distances = (candidate_y - y) ** 2 + (candidate_x - x) ** 2
            weights = alpha[candidate_y, candidate_x].astype(np.int32) * 4 - distances
            best = int(np.argmax(weights))
            replacement = rgb[candidate_y[best], candidate_x[best]]
            break
        if replacement is not None:
            rgb[y, x] = replacement

    rgb[alpha == 0] = 0
    data[..., :3] = rgb
    return Image.fromarray(data)


def correct_tone(image: Image.Image, spec: RoleSpec) -> Image.Image:
    data = np.asarray(image.convert("RGBA")).astype(np.float32)
    rgb = data[..., :3]
    alpha = data[..., 3]
    luma = (
        rgb[..., 0:1] * 0.299
        + rgb[..., 1:2] * 0.587
        + rgb[..., 2:3] * 0.114
    )
    rgb = luma + (rgb - luma) * spec.saturation
    rgb = (rgb - 127.5) * spec.contrast + 127.5
    rgb *= spec.brightness
    rgb = np.clip(rgb, 0, 255)
    rgb[alpha == 0] = 0
    data[..., :3] = rgb
    return Image.fromarray(data.astype(np.uint8))


def resize_premultiplied(image: Image.Image, size: tuple[int, int]) -> Image.Image:
    data = np.asarray(image.convert("RGBA")).astype(np.float32)
    alpha = data[..., 3:4] / 255.0
    premultiplied = data[..., :3] * alpha

    premultiplied_image = Image.fromarray(
        np.clip(premultiplied, 0, 255).astype(np.uint8)
    ).resize(size, Image.Resampling.LANCZOS)
    alpha_image = Image.fromarray(data[..., 3].astype(np.uint8)).resize(
        size, Image.Resampling.LANCZOS
    )

    premultiplied_resized = np.asarray(premultiplied_image).astype(np.float32)
    alpha_resized = np.asarray(alpha_image).astype(np.float32) / 255.0
    output_rgb = np.zeros_like(premultiplied_resized)
    visible = alpha_resized > 0
    output_rgb[visible] = premultiplied_resized[visible] / alpha_resized[visible, None]

    output = np.zeros((size[1], size[0], 4), dtype=np.uint8)
    output[..., :3] = np.clip(output_rgb, 0, 255).astype(np.uint8)
    output[..., 3] = np.asarray(alpha_image)
    return Image.fromarray(output)


def render_normal(spec: RoleSpec) -> Image.Image:
    source = Image.open(spec.source).convert("RGBA")
    crop = source.crop(spec.crop)
    crop = defringe_magenta(crop)
    crop = correct_tone(crop, spec)
    resized = resize_premultiplied(crop, TARGET_SIZE)
    return defringe_magenta(resized)


def render_inactive(normal: Image.Image) -> Image.Image:
    data = np.asarray(normal.convert("RGBA")).astype(np.float32)
    rgb = data[..., :3]
    alpha = data[..., 3]
    luma = (
        rgb[..., 0:1] * 0.299
        + rgb[..., 1:2] * 0.587
        + rgb[..., 2:3] * 0.114
    )
    desaturated = luma + (rgb - luma) * 0.10
    darkened = desaturated * 0.64
    neutral_ink = np.array([78.0, 75.0, 69.0], dtype=np.float32)
    inactive = darkened * 0.75 + neutral_ink * 0.25
    inactive[alpha == 0] = 0
    data[..., :3] = np.clip(inactive, 0, 255)
    return Image.fromarray(data.astype(np.uint8))


def count_magenta_boundary(image: Image.Image) -> int:
    data = np.asarray(image.convert("RGBA"))
    return int(np.count_nonzero(boundary_band(data[..., 3], radius=2) & strong_magenta(data[..., :3])))


def validate_portrait(path: Path) -> dict[str, object]:
    image = Image.open(path)
    if image.mode != "RGBA":
        raise RuntimeError(f"{path.name}: expected RGBA, got {image.mode}")
    if image.size != TARGET_SIZE:
        raise RuntimeError(f"{path.name}: expected {TARGET_SIZE}, got {image.size}")

    alpha = image.getchannel("A")
    alpha_bounds = alpha.getbbox()
    if alpha_bounds is None:
        raise RuntimeError(f"{path.name}: alpha is empty")
    extrema = alpha.getextrema()
    if extrema[0] != 0 or extrema[1] == 0:
        raise RuntimeError(f"{path.name}: expected transparent background and visible foreground")

    magenta_boundary_pixels = count_magenta_boundary(image)
    if magenta_boundary_pixels > 2:
        raise RuntimeError(
            f"{path.name}: {magenta_boundary_pixels} strong magenta boundary pixels remain"
        )

    return {
        "path": path.relative_to(PROJECT_ROOT).as_posix(),
        "sha256": sha256_file(path),
        "width": image.width,
        "height": image.height,
        "mode": image.mode,
        "alphaBounds": list(alpha_bounds),
        "alphaExtrema": list(extrema),
        "magentaBoundaryPixels": magenta_boundary_pixels,
    }


def make_contact_sheet(portraits: list[tuple[str, str, Image.Image]]) -> Path:
    scale = 4
    portrait_size = (TARGET_SIZE[0] * scale, TARGET_SIZE[1] * scale)
    label_height = 26
    cell_size = (portrait_size[0], portrait_size[1] + label_height)
    columns = 3
    rows = 4
    sheet = Image.new(
        "RGBA",
        (cell_size[0] * columns, cell_size[1] * rows),
        (229, 216, 195, 255),
    )
    draw = ImageDraw.Draw(sheet)

    for index, (role, state, portrait) in enumerate(portraits):
        x = (index % columns) * cell_size[0]
        y = (index // columns) * cell_size[1]
        draw.rectangle(
            (x, y, x + cell_size[0] - 1, y + cell_size[1] - 1),
            outline=(89, 78, 64, 110),
            width=2,
        )
        draw.text((x + 8, y + 6), f"{role} / {state}", fill=(45, 39, 32, 255))
        enlarged = portrait.resize(portrait_size, Image.Resampling.NEAREST)
        sheet.alpha_composite(enlarged, (x, y + label_height))

    contact_sheet_path = OUTPUT_ROOT / "partner_switch_portraits_contact_sheet.png"
    sheet.save(contact_sheet_path)
    return contact_sheet_path


def main() -> None:
    OUTPUT_ROOT.mkdir(parents=True, exist_ok=True)
    for spec in ROLE_SPECS:
        if not spec.source.is_file():
            raise FileNotFoundError(spec.source)
        crop_width = spec.crop[2] - spec.crop[0]
        crop_height = spec.crop[3] - spec.crop[1]
        if crop_width * TARGET_SIZE[1] != crop_height * TARGET_SIZE[0]:
            raise RuntimeError(f"{spec.role}: crop does not match target aspect ratio")

    rendered: list[tuple[str, str, Image.Image]] = []
    records: list[dict[str, object]] = []
    for spec in ROLE_SPECS:
        normal = render_normal(spec)
        inactive = render_inactive(normal)
        for state, portrait in (("normal", normal), ("inactive", inactive)):
            suffix = "" if state == "normal" else "_inactive"
            output_path = OUTPUT_ROOT / f"partner_portrait_{spec.role}{suffix}.png"
            portrait.save(output_path, optimize=True)
            record = validate_portrait(output_path)
            record.update(
                {
                    "role": spec.role,
                    "state": state,
                    "source": spec.source.relative_to(PROJECT_ROOT).as_posix(),
                    "sourceSha256": sha256_file(spec.source),
                    "crop": list(spec.crop),
                    "tone": {
                        "brightness": spec.brightness,
                        "contrast": spec.contrast,
                        "saturation": spec.saturation,
                    },
                }
            )
            records.append(record)
            rendered.append((spec.role, state, portrait))

    normal_first = [entry for entry in rendered if entry[1] == "normal"]
    inactive_second = [entry for entry in rendered if entry[1] == "inactive"]
    contact_sheet = make_contact_sheet(normal_first + inactive_second)
    manifest = {
        "status": "PASS",
        "targetSize": list(TARGET_SIZE),
        "sourcePolicy": "current final in-run idle frame 0000 only",
        "portraitStyle": "C_face_closeup",
        "records": records,
        "contactSheet": {
            "path": contact_sheet.relative_to(PROJECT_ROOT).as_posix(),
            "sha256": sha256_file(contact_sheet),
            "width": Image.open(contact_sheet).width,
            "height": Image.open(contact_sheet).height,
        },
    }
    manifest_path = OUTPUT_ROOT / "partner_switch_portraits_manifest.json"
    manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("Generated 12 partner switch portraits at 105x62")
    print(OUTPUT_ROOT)


if __name__ == "__main__":
    main()
