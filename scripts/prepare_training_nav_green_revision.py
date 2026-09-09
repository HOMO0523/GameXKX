"""Background-only cleanup for the generated ink-map navigation revision."""
import hashlib
import json
import shutil
from pathlib import Path

from PIL import Image, ImageChops

ROOT = Path(__file__).resolve().parents[1]
FOLDER = ROOT / 'SourceArt/UI/ImageTruth/revisions/20260909-training-green'
SOURCE = Path('C:/Users/shxuw/.codex/generated_images/01a081fe-c2ea-7c52-9b93-e50433252402/exec-db8fd1ac-012f-484e-8b18-7d53c3bddcd8.png')


def run():
    FOLDER.mkdir(parents=True, exist_ok=True)
    raw = FOLDER / 'generated-map.png'
    if raw.exists():
        assert raw.read_bytes() == SOURCE.read_bytes()
    else:
        shutil.copy2(SOURCE, raw)
    image = Image.open(raw).convert('RGBA')
    before = image.convert('RGB').tobytes()
    r, g, b = image.convert('RGB').split()
    high = ImageChops.lighter(ImageChops.lighter(r, g), b)
    low = ImageChops.darker(ImageChops.darker(r, g), b)
    # The scroll is an outline: the neutral checker inside it is background too.
    # Dark charcoal and saturated green ink remain unchanged, including RGB.
    neutral = ImageChops.subtract(high, low).point(lambda value: 255 if value <= 32 else 0)
    light = low.point(lambda value: 255 if value >= 65 else 0)
    background = ImageChops.multiply(neutral, light)
    image.putalpha(ImageChops.invert(background))
    assert image.convert('RGB').tobytes() == before
    assert image.getpixel((600, 520))[3] == 0, 'Scroll interior must be transparent'
    assert image.getpixel((800, 470))[3] == 255, 'Green flag must remain solid'
    alpha = image.getchannel('A')
    # Clear RGB only where alpha is zero so filtering cannot sample checker color.
    image = Image.composite(image, Image.new('RGBA', image.size, (0, 0, 0, 0)), alpha)
    assert all(alpha.getpixel(p) == 0 for p in ((0, 0), (1253, 0), (0, 1253), (1253, 1253)))
    target = FOLDER / 'training_nav_training_ink_green_v002.png'
    image.save(target)
    manifest = {
        'status': 'user-requested-art-revision',
        'visualApproval': 'not-yet-recorded',
        'request': '地图描边有点细，地图中间线条着色单色墨绿色，和别的图标区分一下，强调一下',
        'tool': 'built-in image_gen',
        'prompt': 'prompt.txt',
        'reference': 'SourceArt/UI/ImageTruth/confirmed/training_nav_training_ink_v001.png',
        'generated': raw.relative_to(ROOT).as_posix(),
        'final': target.relative_to(ROOT).as_posix(),
        'sha256': hashlib.sha256(target.read_bytes()).hexdigest(),
        'size': list(image.size),
        'alpha_bbox': list(alpha.getbbox()),
        'cleanup': {'background_pixels': background.histogram()[255], 'rgb_artwork_unchanged': True,
                    'includes_enclosed_checker': True},
        'asset': '/Game/GameXXK/UI/ImageTruth/Training/T_TrainingNavTraining.T_TrainingNavTraining',
        'runtime_imported': False,
    }
    (FOLDER / 'manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(manifest, ensure_ascii=False))


if __name__ == '__main__':
    run()
