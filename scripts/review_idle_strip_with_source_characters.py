"""Composite original character frames for offline art review; not a PIE capture."""
from pathlib import Path
import argparse
import hashlib
import json
from PIL import Image, ImageDraw, ImageFont, ImageOps

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--revision', choices=('14', '15', '16', '17', '18'), default='18')
args = parser.parse_args()
OUT = ROOT / f'SourceArt/UI/Training/IdleStrip/QuietInkV{args.revision}'
background = Image.open(OUT / f'idle_strip_quiet_ink_v{args.revision}_seamless.png').convert('RGBA')
canvas = Image.new('RGBA', (1000, 230), (43, 46, 40, 255))
tile = background.resize((750, 300), Image.Resampling.LANCZOS)
for x in (0, 750):
    canvas.alpha_composite(tile, (x, 10))
actors = [('enemy_01_rooster_idle', 70), ('enemy_02_goat_idle', 200),
          ('enemy_04_civet_idle', 330), ('character_00_hero_idle', 590),
          ('character_01_blade_idle', 720), ('character_07_tusi_chief_idle', 850)]
records = []
for name, center_x in actors:
    source = ROOT / 'SourceAssets/AnimationProcessing/Production' / name / 'frames/frame_0000.png'
    frame = Image.open(source).convert('RGBA')
    bounds = frame.getchannel('A').getbbox()
    assert bounds, source
    sprite = ImageOps.contain(frame.crop(bounds), (125, 130), Image.Resampling.LANCZOS)
    canvas.alpha_composite(sprite, (center_x-sprite.width//2, 173-sprite.height))
    records.append({'source': str(source.relative_to(ROOT)), 'sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
                    'review_center_x': center_x, 'review_foot_y': 173})
draw = ImageDraw.Draw(canvas)
font = ImageFont.truetype('C:/Windows/Fonts/msyh.ttc', 14)
draw.text((12, 207), '原角色素材叠加预览 · 非实机截图 · 角色尺寸仅用于美术对照', font=font, fill=(212, 219, 207))
canvas.convert('RGB').save(OUT / 'with-original-characters-review.png')
(OUT / 'character-review-manifest.json').write_text(json.dumps({'kind': 'offline-source-frame-composite', 'actors': records}, ensure_ascii=False, indent=2), encoding='utf-8')
print(OUT / 'with-original-characters-review.png')
