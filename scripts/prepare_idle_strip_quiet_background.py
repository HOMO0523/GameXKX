"""Export the generated idle plate: extract matte, align canvas, and close the wrap.

Painting is authored with built-in imagegen. This script only handles the
technical RGBA/seam export; it does not repaint characters or other UI assets.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np
from PIL import Image, ImageFilter

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'SourceArt/UI/Training/IdleStrip/QuietInkV4'
EVIDENCE = ROOT / 'Saved/Codex/IdleStripBackground-20260907'

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def extract_matte(image):
    rgb = np.asarray(image.convert('RGB')).astype(np.float32)
    chroma = np.minimum(rgb[..., 0], rgb[..., 2]) - rgb[..., 1]
    # Foreground is warm neutral/olive. Magenta lies outside that palette.
    alpha = np.clip((190.0 - chroma) / 190.0, 0, 1)
    alpha[chroma <= 0] = 1
    alpha[alpha < 0.06] = 0
    clean = chroma <= 0
    # Don't search for a donor in distant variations of the generated matte.
    near_clean = np.asarray(Image.fromarray((clean*255).astype(np.uint8)).filter(ImageFilter.MaxFilter(17))) > 0
    alpha[~near_clean] = 0
    fringe = (alpha > 0) & (chroma > 0)
    for y, x in np.argwhere(fringe):
        donor = None
        for radius in (1, 2, 4, 8):
            y0, y1 = max(0, y-radius), min(rgb.shape[0], y+radius+1)
            x0, x1 = max(0, x-radius), min(rgb.shape[1], x+radius+1)
            candidates = np.argwhere(clean[y0:y1, x0:x1])
            if len(candidates):
                distances = (candidates[:, 0]+y0-y)**2 + (candidates[:, 1]+x0-x)**2
                cy, cx = candidates[int(np.argmin(distances))]
                donor = rgb[y0+cy, x0+cx].copy()
                break
        if donor is None:
            alpha[y, x] = 0
        else:
            rgb[y, x] = donor
    rgb[alpha == 0] = 0
    return np.dstack([rgb, alpha*255]).round().astype(np.uint8)

def close_horizontal_wrap(data, band=128):
    # Pair opposing edge strips in premultiplied RGBA. At the boundary both
    # columns are exactly equal; smoothstep returns to untouched art at 128px.
    premul = data.astype(np.float64)/255
    premul[..., :3] *= premul[..., 3:4]
    original = premul.copy()
    for i in range(band):
        t = i/(band-1)
        weight = 0.5*(1-t*t*(3-2*t))
        left, right = original[:, i], original[:, -1-i]
        premul[:, i] = (1-weight)*left + weight*right
        premul[:, -1-i] = (1-weight)*right + weight*left
    alpha = premul[..., 3:4]
    rgb = np.divide(premul[..., :3], alpha, out=np.zeros_like(premul[..., :3]), where=alpha>0)
    return (np.clip(np.concatenate([rgb, alpha], axis=2), 0, 1)*255).round().astype(np.uint8)

def measurements(image):
    data = np.asarray(image.convert('RGBA')).astype(np.int16)
    alpha = data[..., 3]
    road = data[450:490, :, :3].astype(np.float32)
    lum = road @ np.array([0.2126, 0.7152, 0.0722])
    opaque = alpha > 240
    colors = data[..., :3][opaque]
    visible_chroma = np.minimum(data[..., 0], data[..., 2])-data[..., 1]
    return {'size': list(image.size), 'mode': image.mode,
            'alpha_bbox': list(image.getchannel('A').getbbox()),
            'transparent_pixels': int((alpha == 0).sum()),
            'opaque_pixels': int(opaque.sum()),
            'horizontal_edge_max_delta': int(np.abs(data[:, 0]-data[:, -1]).max()),
            'seam_neighbor_mean_delta': float(np.abs(data[:, 1]-data[:, 0]).mean()),
            'road_sample_rect': [0, 450, image.width, 490],
            'road_luminance_std': float(lum.std()),
            'road_horizontal_edge_mean': float(np.abs(np.diff(lum, axis=1)).mean()),
            'road_chroma_mean': float((road.max(axis=2)-road.min(axis=2)).mean()),
            'opaque_rgb_min': colors.min(axis=0).tolist(),
            'remaining_magenta_pixels': int(((alpha > 0) & (visible_chroma > 8)).sum())}

def main():
    global OUT
    parser = argparse.ArgumentParser()
    parser.add_argument('source', type=Path)
    parser.add_argument('--revision', choices=tuple(str(i) for i in range(4, 17)), default='16')
    parser.add_argument('--vertical-shift', type=int)
    args = parser.parse_args()
    vertical_shift = args.vertical_shift if args.vertical_shift is not None else {'14': -108, '15': -140, '16': -140}.get(args.revision, -64)
    OUT = ROOT / f'SourceArt/UI/Training/IdleStrip/QuietInkV{args.revision}'
    OUT.mkdir(parents=True, exist_ok=True)
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    source = OUT / f'idle_strip_quiet_ink_v{args.revision}_chroma_source.png'
    source.write_bytes(args.source.read_bytes())
    image = Image.open(source)
    assert 1975 <= image.width <= 1983 and 793 <= image.height <= 796, image.size
    extracted = extract_matte(image)
    extracted = extracted[:793]
    left_pad = (1983-image.width)//2
    right_pad = 1983-image.width-left_pad
    extracted = np.pad(extracted, ((0, 0), (left_pad, right_pad), (0, 0)), mode='edge')
    # Translate without stretching to match the existing foot registration.
    assert -192 <= vertical_shift <= 0
    aligned = np.zeros_like(extracted)
    if vertical_shift:
        aligned[:vertical_shift] = extracted[-vertical_shift:]
    else:
        aligned[:] = extracted
    final_data = close_horizontal_wrap(aligned)
    preserved_ground = None
    if args.revision == '16':
        approved_path = ROOT / 'SourceArt/UI/Training/IdleStrip/QuietInkV15/idle_strip_quiet_ink_v15_seamless.png'
        approved = np.asarray(Image.open(approved_path).convert('RGBA'))
        # The user approved v15 colors; retain its ground pixels exactly.
        weight = np.clip((np.arange(793)-420)/20, 0, 1)[:, None, None]
        current = final_data.astype(np.float64)/255
        reference = approved.astype(np.float64)/255
        current[..., :3] *= current[..., 3:4]
        reference[..., :3] *= reference[..., 3:4]
        blended = current*(1-weight) + reference*weight
        alpha = blended[..., 3:4]
        rgb = np.divide(blended[..., :3], alpha, out=np.zeros_like(blended[..., :3]), where=alpha>0)
        final_data = np.rint(np.clip(np.concatenate([rgb, alpha], axis=2), 0, 1)*255).astype(np.uint8)
        final_data[440:] = approved[440:]
        assert np.array_equal(final_data[440:], approved[440:])
        preserved_ground = {'source': str(approved_path.relative_to(ROOT)), 'exact_from_row': 440,
                            'pixel_sha256': hashlib.sha256(final_data[440:].tobytes()).hexdigest()}
    result = Image.fromarray(final_data)
    final = OUT / f'idle_strip_quiet_ink_v{args.revision}_seamless.png'
    result.save(final)
    before_path = ROOT / 'SourceArt/UI/ImageTruth/confirmed/training_idle_strip_background_seamless_v003.png'
    before = Image.open(before_path).convert('RGBA')
    report = {'status': 'technical-export-pass', 'generation': 'built-in imagegen',
              'source': str(source.relative_to(ROOT)), 'source_sha256': sha(source),
              'final': str(final.relative_to(ROOT)), 'sha256': sha(final),
              'vertical_translation_px': vertical_shift, 'seam_blend_px': 128,
              'source_size': list(image.size), 'horizontal_padding': [left_pad, right_pad],
              'bottom_blank_rows_removed': image.height-793,
              'original_source_sha256': sha(before_path),
              'before': measurements(before), 'after': measurements(result)}
    if preserved_ground:
        report['preserved_approved_ground'] = preserved_ground
    assert report['after']['horizontal_edge_max_delta'] == 0
    assert report['after']['remaining_magenta_pixels'] == 0
    assert report['after']['transparent_pixels'] > 800000
    (OUT / 'manifest.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    # Two-copy review at the actual 750x300 logical tile size.
    tile = result.resize((750, 300), Image.Resampling.LANCZOS)
    preview = Image.new('RGBA', (1500, 190), (48, 45, 40, 255))
    for x in (0, 750):
        preview.alpha_composite(tile, (x, -55))
    preview.convert('RGB').save(EVIDENCE / 'two-copy-seam-review.png')
    print(json.dumps(report, indent=2))

if __name__ == '__main__':
    main()
