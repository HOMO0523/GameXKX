"""Prepare the user-authorized background-only cleanup and 12-icon review boards.

Artwork and percent relief were generated with imagegen. This script removes
border-connected neutral checker backgrounds, normalizes delivery size and lays
out the review. The user additionally authorized one shared percent layer at an
identical size with three light tints. Gem bodies are never redrawn or recolored.
"""
from __future__ import annotations

import hashlib
import argparse
import json
import shutil
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFont, ImageOps

ROOT = Path(__file__).resolve().parents[1]
FOLDER = ROOT / "SourceArt/UI/Items/Gems/contour-review-20260909"
NAMES = {
    "Attack": ("攻击", "红色刃晶"), "Defense": ("防御", "厚盾晶体"), "MaxHealth": ("生命", "饱满心形"),
    "AttackPercent": ("攻击百分比", "中心浅浮雕 %"), "DefensePercent": ("防御百分比", "中心浅浮雕 %"), "MaxHealthPercent": ("生命百分比", "中心浅浮雕 %"),
    "DirectDamage": ("直接伤害", "拳形晶体"), "ArmorGain": ("护甲获得量", "层叠甲晶"), "Healing": ("治疗效果", "药葫芦晶体"),
    "CounterDamage": ("反击伤害", "回钩刃晶"), "FireDamage": ("火焰伤害", "弯曲火舌晶体"), "DamageOverTime": ("持续伤害", "倒钩毒滴晶体"),
    "FrostDamage": ("冰霜伤害", "厚雪晶 · 补充候选"), "LightningDamage": ("雷击伤害", "折角闪电 · 补充候选"),
}
FONT = ROOT / "SourceArt/UI/Fonts/Readability/SourceHanSansCN-Bold.otf"


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def cleanup_background(source: Path):
    original = Image.open(source)
    original.load()
    image = original.convert("RGBA")
    before = image.convert("RGB").tobytes()
    alpha = image.getchannel("A")
    removed = 0
    if alpha.getextrema()[0] == 255:
        red, green, blue = image.convert("RGB").split()
        high = ImageChops.lighter(ImageChops.lighter(red, green), blue)
        low = ImageChops.darker(ImageChops.darker(red, green), blue)
        # The generated checker is neutral and light. Dark outlines and saturated
        # gem surfaces form a barrier; enclosed white highlights stay untouched.
        neutral = ImageChops.subtract(high, low).point(lambda value: 255 if value <= 32 else 0)
        light = low.point(lambda value: 255 if value >= 60 else 0)
        eligible = ImageChops.multiply(neutral, light)
        w, h = image.size
        for point in ((0, 0), (w-1, 0), (0, h-1), (w-1, h-1)):
            if eligible.getpixel(point) == 255:
                ImageDraw.floodfill(eligible, point, 128)
        removed = eligible.histogram()[128]
        if not removed:
            raise RuntimeError(f"No removable exterior checker found: {source}")
        alpha = eligible.point(lambda value: 0 if value == 128 else 255)
        image.putalpha(alpha)
    # Generation can leave alpha=1 dust in otherwise empty corners. This is
    # background residue, below the visible-alpha threshold used for the art.
    image.putalpha(image.getchannel("A").point(lambda value: 0 if value <= 3 else value))
    assert image.convert("RGB").tobytes() == before, "Background cleanup changed RGB artwork"
    assert all(image.getpixel(p)[3] == 0 for p in ((0,0),(image.width-1,0),(0,image.height-1),(image.width-1,image.height-1)))
    bbox = image.getchannel("A").point(lambda value: 255 if value >= 16 else 0).getbbox()
    assert bbox and (bbox[2]-bbox[0]) > image.width * 0.4 and (bbox[3]-bbox[1]) > image.height * 0.4
    return image, {"original_mode": original.mode, "removed_background_pixels": removed,
                   "rgb_artwork_preserved_before_resize": True, "source_alpha_bbox": list(bbox)}


def normalize(image):
    bbox = image.getchannel("A").point(lambda value: 255 if value >= 16 else 0).getbbox()
    crop = image.crop(bbox)
    scale = 450 / max(crop.size)
    size = (max(1, round(crop.width*scale)), max(1, round(crop.height*scale)))
    crop = crop.resize(size, Image.Resampling.LANCZOS)
    canvas = Image.new("RGBA", (512,512), (0,0,0,0))
    canvas.alpha_composite(crop, ((512-size[0])//2, (512-size[1])//2))
    return canvas


def font(size):
    return ImageFont.truetype(str(FONT), size)


def prepare_percent_layer(data, raw_dir):
    config = data.get("shared_percent")
    if not config:
        return None
    assert data.get("percent_composition_authorized")
    source = Path(config["generated_path"])
    raw = raw_dir/"SharedPercentGlyph.png"
    if raw.exists(): assert sha(raw) == sha(source)
    else: shutil.copy2(source, raw)
    icon = Image.open(raw).convert("RGBA")
    if icon.getchannel("A").getextrema()[0] == 255:
        keyed = Image.new("L", icon.size, 255)
        rgb, mask = icon.load(), keyed.load()
        for y in range(icon.height):
            for x in range(icon.width):
                r,g,b,_ = rgb[x,y]
                if r > 180 and b > 180 and g < 120: mask[x,y] = 0
        icon.putalpha(keyed)
    icon.putalpha(icon.getchannel("A").point(lambda a: 0 if a <= 3 else a))
    bbox = icon.getchannel("A").point(lambda a: 255 if a >= 16 else 0).getbbox()
    assert bbox and bbox != (0,0,icon.width,icon.height), "Shared symbol requires an isolated alpha mask"
    icon = icon.crop(bbox)
    scale = min(config["box"][0]/icon.width, config["box"][1]/icon.height)
    icon = icon.resize((round(icon.width*scale),round(icon.height*scale)),Image.Resampling.LANCZOS)
    config["source_sha256"] = sha(source)
    config["render_size"] = list(icon.size)
    config["alpha_sha256"] = hashlib.sha256(icon.getchannel("A").tobytes()).hexdigest()
    return icon


def composite_percent(base, glyph, data, kind):
    settings = data["shared_percent"]
    tint = settings["tints"][kind]
    painted = ImageOps.colorize(ImageOps.grayscale(glyph), tuple(tint["dark"]), tuple(tint["light"])).convert("RGBA")
    painted.putalpha(glyph.getchannel("A"))
    pos = (settings["center"][0]-painted.width//2,settings["center"][1]-painted.height//2)
    overlay = Image.new("RGBA", base.size, (0,0,0,0)); overlay.alpha_composite(painted,pos)
    outside = ImageChops.multiply(overlay.getchannel("A"),ImageOps.invert(base.getchannel("A")))
    assert outside.getextrema()[1] <= 8, "The percent mark extends outside the gem body"
    output = Image.alpha_composite(base,overlay)
    differences = ImageChops.difference(base,output).convert("RGB")
    unchanged_mask = overlay.getchannel("A").point(lambda a: 255 if a == 0 else 0)
    assert ImageChops.multiply(differences,Image.merge("RGB",(unchanged_mask,)*3)).getbbox() is None
    return output, {"shared_percent":True,"percent_size":list(painted.size),"percent_position":list(pos),
                    "percent_alpha_sha256":settings["alpha_sha256"],"percent_tint":tint,"gem_body_unchanged_outside_percent":True}


def center_text(draw, center, y, text, size, fill):
    f = font(size)
    box = draw.textbbox((0,0), text, font=f)
    draw.text((center-(box[2]-box[0])/2, y), text, font=f, fill=fill)


def make_board(records, output):
    columns = 7 if len(records) == 14 else 6
    width, height, cell = columns*300+120, 990, 300
    board = Image.new("RGB", (width,height), "#EDE7D8")
    draw = ImageDraw.Draw(board)
    draw.text((60,30), f"宝石类型审阅稿 · {len(records)}种" + ("候选" if len(records)==14 else ""), font=font(40), fill="#303B37")
    draw.text((60,91), "每类一张图标，全部品质共用；名称、道具底、tooltip及镶嵌文本复用装备品质表现", font=font(23), fill="#646C62")
    for index, r in enumerate(records):
        col, row = index % columns, index // columns
        x, y = 60+col*cell, 170+row*385
        draw.rounded_rectangle((x+5,y,x+285,y+340), radius=18, fill="#F7F3E9", outline="#CDCABB", width=2)
        art = Image.open(ROOT / r["normalized_png"]).convert("RGBA").resize((258,258), Image.Resampling.LANCZOS)
        board.paste(art, (x+16,y+9), art)
        label, caption = NAMES[r["type"]]
        center_text(draw, x+145, y+272, label, 25, "#293F3B")
        center_text(draw, x+145, y+310, caption, 18, "#777B70")
    draw.text((60,932), "审阅稿 · 未导入或替换项目资产 · 不制作九档次级图标", font=font(21), fill="#687066")
    board.save(output, optimize=True)


def make_small_board(records, output):
    columns = 7 if len(records) == 14 else 6
    board = Image.new("RGB", (24+columns*188,535), "#EDE7D8")
    draw = ImageDraw.Draw(board)
    draw.text((24,14), "64px彩色 / 48px彩色、灰度与剪影辨识检查", font=font(23), fill="#303B37")
    for index, r in enumerate(records):
        x, y = 24+(index%columns)*188, 68+(index//columns)*222
        source = Image.open(ROOT / r["normalized_png"]).convert("RGBA")
        large = source.resize((64,64),Image.Resampling.LANCZOS)
        board.paste(large,(x,y),large); draw.text((x+77,y+22),"64px",font=font(14),fill="#667367")
        art = source.resize((48,48), Image.Resampling.LANCZOS)
        gray = ImageOps.grayscale(art).convert("RGBA"); gray.putalpha(art.getchannel("A"))
        silhouette = Image.new("RGBA", art.size, (41,52,49,0)); silhouette.putalpha(art.getchannel("A"))
        for offset, value in ((0,art),(56,gray),(112,silhouette)):
            board.paste(value, (x+offset,y+80), value)
        draw.text((x,y+147), NAMES[r["type"]][0], font=font(16), fill="#35423C")
    board.save(output, optimize=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--revision", default="v5", choices=("v2", "v3", "v4", "v5"))
    revision = parser.parse_args().revision
    data = json.loads((FOLDER / f"type-icon-generation-records-{revision}.json").read_text(encoding="utf-8"))
    count = data["count"]
    assert data["background_cleanup_authorized"] and count in (12,14)
    assert len(data["records"]) == count and len({r["type"] for r in data["records"]}) == count
    assert {r["type"] for r in data["records"]}.issubset(NAMES)
    raw_dir, clean_dir, final_dir = [FOLDER / (n+"-"+revision) for n in ("type-raw", "type-clean", "type-icons")]
    for folder in (raw_dir,clean_dir,final_dir): folder.mkdir(parents=True, exist_ok=True)
    glyph = prepare_percent_layer(data,raw_dir)
    report = []
    for r in data["records"]:
        source = Path(r["generated_path"])
        digest = sha(source)
        stem = "T_Item_Gem_" + r["type"]
        raw = raw_dir / (stem+".png")
        if raw.exists(): assert sha(raw) == digest, "Versioned raw file already exists with different content"
        else: shutil.copy2(source, raw)
        image, checks = cleanup_background(raw)
        clean = clean_dir / (stem+".png"); image.save(clean, optimize=True)
        normalized = normalize(image)
        composition = {}
        if r.get("percent_overlay"):
            normalized, composition = composite_percent(normalized,glyph,data,r["type"])
        final = final_dir / (stem+".png"); normalized.save(final, optimize=True)
        alpha = normalized.getchannel("A")
        border = [alpha.crop(box).getextrema()[1] for box in ((0,0,512,1),(0,511,512,512),(0,0,1,512),(511,0,512,512))]
        assert border == [0,0,0,0]
        assert sha(source) == digest and sha(raw) == digest
        report.append({**r, **checks, **composition, "source_sha256":digest, "raw_png":str(raw.relative_to(ROOT)),
                       "clean_png":str(clean.relative_to(ROOT)), "normalized_png":str(final.relative_to(ROOT)),
                       "size":[512,512], "mode":"RGBA", "transparent_border":True,
                       "normalized_sha256":sha(final)})
    assert len({r["normalized_sha256"] for r in report}) == count
    overlays = [r for r in report if r.get("shared_percent")]
    if glyph:
        assert len(overlays) == 3 and len({r["percent_alpha_sha256"] for r in overlays}) == 1
        assert len({tuple(r["percent_size"]) for r in overlays}) == 1
    large, small = FOLDER/f"gem-types-review-{revision}.png", FOLDER/f"gem-types-48px-review-{revision}.png"
    make_board(report, large); make_small_board(report, small)
    result = {"status":"review-only", "count":count, "quality_agnostic":True, "runtime_imported":False,
              "generated_with":"built-in imagegen", "background_cleanup":"user-authorized local script",
              "shared_percent":data.get("shared_percent"),
              "board":str(large.relative_to(ROOT)), "small_board":str(small.relative_to(ROOT)), "records":report}
    (FOLDER/f"type-icon-manifest-{revision}.json").write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding="utf-8")
    print(json.dumps({"ok":True,"count":count,"backgrounds_cleaned":sum(r["removed_background_pixels"]>0 for r in report),
                      "board":str(large),"small_board":str(small),"all_source_files_preserved":True},ensure_ascii=False))


if __name__ == "__main__":
    main()
