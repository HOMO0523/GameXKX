"""Composite generated pale-gray ink over the current artist-edited source."""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw, ImageFont
from prepare_idle_strip_quiet_background import extract_matte, close_horizontal_wrap

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'SourceArt/UI/Training/IdleStrip/QuietInkV17'

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    global OUT
    parser = argparse.ArgumentParser()
    parser.add_argument('ink_source', type=Path)
    parser.add_argument('--opacity', type=float, default=0.84)
    parser.add_argument('--revision', choices=('17', '18'), default='18')
    args = parser.parse_args()
    assert 0 < args.opacity <= 1
    original_base_path = ROOT / 'SourceArt/UI/Training/IdleStrip/QuietInkV17/current-approved-base.png'
    OUT = ROOT / f'SourceArt/UI/Training/IdleStrip/QuietInkV{args.revision}'
    OUT.mkdir(parents=True, exist_ok=True)
    base_path = OUT / 'current-approved-base.png'
    if base_path != original_base_path:
        base_path.write_bytes(original_base_path.read_bytes())
    base = Image.open(base_path).convert('RGBA')
    assert base.size == (1983, 235)
    original = Image.new('RGBA', (1983, 793))
    original.paste(base, (0, 256))
    base_data = np.asarray(original).copy()

    source = OUT / 'gray_ink_overlay_chroma_source.png'
    source.write_bytes(args.ink_source.read_bytes())
    opened = Image.open(source)
    assert 1975 <= opened.width <= 1983 and 793 <= opened.height <= 796, opened.size
    ink = extract_matte(opened)[:793]
    pad_left = (1983-opened.width)//2
    ink = np.pad(ink, ((0, 0), (pad_left, 1983-opened.width-pad_left), (0, 0)), mode='edge')
    ink = close_horizontal_wrap(ink)
    weight = ink[..., 3].astype(np.float64)/255 * args.opacity
    base_luminance = base_data[..., :3] @ np.array([0.2126, 0.7152, 0.0722])
    # Existing dark ink remains intact; pale-gray ink must not act as a highlight.
    weight *= np.clip((base_luminance-133)/30, 0, 1)
    weight *= base_data[..., 3]/255
    weight[weight < 0.025] = 0
    gray = np.array([137, 139, 135])
    result = base_data.copy()
    result[..., :3] = np.rint(base_data[..., :3]*(1-weight[..., None]) + gray*weight[..., None]).astype(np.uint8)
    assert np.array_equal(result[..., 3], base_data[..., 3]), 'Original silhouette changed'
    assert np.array_equal(result[weight == 0], base_data[weight == 0]), 'Uninked base pixels changed'
    edge_delta = int(np.abs(result[:, 0].astype(np.int16)-result[:, -1].astype(np.int16)).max())
    assert edge_delta == 0, f'Horizontal seam changed: {edge_delta}'
    final = OUT / f'idle_strip_quiet_ink_v{args.revision}_seamless.png'
    Image.fromarray(result).save(final)
    Image.fromarray(result[256:491]).save(OUT / 'idle_strip_gray_ink_cropped.png')
    ink_layer = np.zeros_like(result)
    ink_layer[..., :3] = gray
    ink_layer[..., 3] = np.rint(weight*255).astype(np.uint8)
    Image.fromarray(ink_layer).save(OUT / 'gray_ink_overlay_rgba.png')
    changed = np.any(result[..., :3] != base_data[..., :3], axis=2) & (base_data[..., 3] > 0)
    report = {'status': 'technical-export-pass', 'generation': 'built-in imagegen, separate gray-ink layer',
              'final': str(final.relative_to(ROOT)), 'sha256': sha(final),
              'current_artist_base': str(base_path.relative_to(ROOT)), 'base_sha256': sha(base_path),
              'base_size': list(base.size), 'runtime_size': [1983, 793], 'base_canvas_origin': [0, 256],
              'ink_source': str(source.relative_to(ROOT)), 'ink_source_sha256': sha(source),
              'gray_rgb': gray.tolist(), 'maximum_ink_opacity': args.opacity,
              'changed_visible_pixels': int(changed.sum()),
              'changed_fraction_of_visible_base': float(changed.sum()/np.count_nonzero(base_data[..., 3])),
              'alpha_identical_to_current_base': True, 'uninked_pixels_identical': True,
              'horizontal_edge_max_delta': edge_delta}
    (OUT / 'manifest.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    font = ImageFont.truetype('C:/Windows/Fonts/msyh.ttc', 15)
    review = Image.new('RGB', (1100, 335), (42, 46, 43))
    draw = ImageDraw.Draw(review)
    for label, data, y in [('当前底图', base_data[256:491], 29), ('淡灰墨迹勾线', result[256:491], 191)]:
        draw.text((14, y-24), label, font=font, fill=(215, 221, 215))
        row = Image.fromarray(data).resize((1100, 130), Image.Resampling.LANCZOS)
        review.paste(row, (0, y), row)
    review.save(OUT / 'before-after-gray-ink.png')
    print(json.dumps(report, ensure_ascii=False, indent=2))

if __name__ == '__main__':
    main()
