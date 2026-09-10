"""Compose exact generated relics for visual review; never redraw their pixels."""
import json
import argparse
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / "SourceArt/UI/Relics/gemstyle-20260910"


def main():
    global ART
    parser = argparse.ArgumentParser()
    parser.add_argument("--group", choices=("relic", "hunt"), default="relic")
    args = parser.parse_args()
    expected, title = 45, "遗物图标"
    if args.group == "hunt":
        ART = ROOT / "SourceArt/UI/Hunt/gemstyle-20260910"
        expected, title = 5, "讨伐令与宝箱"
    manifest = json.loads((ART / "manifest.json").read_text(encoding="utf-8"))
    jobs = json.loads((ART / "art-jobs.json").read_text(encoding="utf-8"))["jobs"]
    records = {row["slug"]: row for row in manifest["icons"]}
    selected = [job for job in jobs if job["slug"] in records]
    columns = min(9, len(selected))
    rows = (len(selected) + columns - 1) // columns
    title_font = ImageFont.truetype("C:/Windows/Fonts/msyh.ttc", 25)
    label_font = ImageFont.truetype("C:/Windows/Fonts/msyh.ttc", 18)
    small_font = ImageFont.truetype("C:/Windows/Fonts/msyh.ttc", 13)
    background, ink = (238, 232, 214), (35, 54, 46)
    review = Image.new("RGB", (columns * 170 + 32, rows * 200 + 92), background)
    sizes = Image.new("RGB", (columns * 170 + 32, rows * 145 + 92), background)
    draw, size_draw = ImageDraw.Draw(review), ImageDraw.Draw(sizes)
    draw.text((20, 16), f"{title} · 宝石同系列画风 · {len(selected)}/{expected}", font=title_font, fill=ink)
    size_draw.text((20, 16), f"64px / 48px 与灰度 · {len(selected)}/{expected}", font=title_font, fill=ink)
    for index, job in enumerate(selected):
        record = records[job["slug"]]
        item = Image.open(ROOT / record["icon"]).convert("RGBA")
        x, y = 16 + (index % columns) * 170, 70 + (index // columns) * 200
        thumb = item.resize((150, 150), Image.Resampling.LANCZOS)
        review.paste(thumb, (x + 10, y), thumb)
        draw.text((x + 85, y + 163), job["name"], anchor="mm", font=label_font, fill=ink)
        sx, sy = x, 77 + (index // columns) * 145
        for offset, dimension, grayscale in ((0, 64, False), (64, 48, False), (115, 48, True)):
            image = item.resize((dimension, dimension), Image.Resampling.LANCZOS)
            if grayscale:
                alpha = image.getchannel("A")
                image = image.convert("L").convert("RGBA")
                image.putalpha(alpha)
            sizes.paste(image, (sx + offset, sy), image)
        size_draw.text((sx + 85, sy + 88), job["name"], anchor="mm", font=label_font, fill=ink)
        size_draw.text((sx + 85, sy + 112), "64px / 48px / 灰度", anchor="mm", font=small_font, fill=ink)
    out = ART / "review"
    out.mkdir(exist_ok=True)
    review.save(out / (args.group + "-overview.png"))
    sizes.save(out / (args.group + "-small-sizes.png"))
    print(json.dumps({"completedArt": len(selected), "expectedArt": expected,
                      "overview": str(out / (args.group + "-overview.png")), "sizes": str(out / (args.group + "-small-sizes.png"))}, ensure_ascii=False))


if __name__ == "__main__":
    main()
