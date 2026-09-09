"""Clean background only from the requested bold navigation glyph edits."""
import hashlib
import json
import shutil
from pathlib import Path
from PIL import Image, ImageChops

ROOT = Path(__file__).resolve().parents[1]
FOLDER = ROOT / 'SourceArt/UI/ImageTruth/revisions/20260909-navigation-bold'
GENERATED = Path('C:/Users/shxuw/.codex/generated_images/01a081fe-c2ea-7c52-9b93-e50433252402')
INPUTS = {
    'Warehouse': ('exec-acdb0b60-523f-4469-9936-240294cf6f21.png', 'training.nav.warehouse.ink.monochrome.v002'),
    'Formation': ('exec-6d61c7e6-8807-4612-9cf2-ea8822d4e301.png', 'training.nav.formation.ink.v002'),
}

def run():
    FOLDER.mkdir(parents=True, exist_ok=True)
    entries = []
    for kind, (filename, semantic_id) in INPUTS.items():
        raw = FOLDER / (kind + '-generated.png')
        if not raw.exists():
            shutil.copy2(GENERATED / filename, raw)
        image = Image.open(raw).convert('RGBA')
        original_rgb = image.convert('RGB').tobytes()
        r, g, b = image.convert('RGB').split()
        high = ImageChops.lighter(ImageChops.lighter(r, g), b)
        low = ImageChops.darker(ImageChops.darker(r, g), b)
        neutral = ImageChops.subtract(high, low).point(lambda v: 255 if v <= 32 else 0)
        light = low.point(lambda v: 255 if v >= 65 else 0)
        alpha = ImageChops.invert(ImageChops.multiply(neutral, light))
        image.putalpha(alpha)
        assert image.convert('RGB').tobytes() == original_rgb
        image = Image.composite(image, Image.new('RGBA', image.size, (0, 0, 0, 0)), alpha)
        assert alpha.getextrema() == (0, 255)
        assert all(alpha.getpixel(p) == 0 for p in ((0,0),(1253,0),(0,1253),(1253,1253)))
        empty_probe = {'Formation': (625, 450), 'Warehouse': (400, 740), 'Training': (600, 520)}[kind]
        assert alpha.getpixel(empty_probe) == 0
        target = FOLDER / ('T_TrainingNav' + kind + '.png')
        image.save(target)
        entries.append({'kind': kind, 'id': semantic_id, 'final': target.relative_to(ROOT).as_posix(),
                        'generated': raw.relative_to(ROOT).as_posix(), 'size': list(image.size),
                        'sha256': hashlib.sha256(target.read_bytes()).hexdigest(), 'alpha_bbox': list(alpha.getbbox()),
                        'asset': '/Game/GameXXK/UI/ImageTruth/Training/T_TrainingNav' + kind,
                        'cleanup': 'Removed exterior and enclosed checker only; opaque ink RGB unchanged'})
    manifest = {'status': 'user-requested-art-revision', 'visualApproval': 'not-yet-recorded',
                'tool': 'built-in image_gen', 'prompts': 'prompts.json', 'runtime_imported': False,
                'images': entries, 'presentation': {'disc': 100, 'glyph': 82, 'map_glyph': 88,
                'tools_glyph': 70, 'talents_tools_tint_linear': [0.01, 0.01, 0.01, 1.0]}}
    (FOLDER / 'manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print(json.dumps(manifest, ensure_ascii=False))

if __name__ == '__main__':
    run()
