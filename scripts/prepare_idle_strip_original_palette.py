"""Export the palette edit while retaining the current runtime alpha/layout."""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw, ImageFont
from prepare_idle_strip_quiet_background import extract_matte, close_horizontal_wrap

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'SourceArt/UI/Training/IdleStrip/QuietInkV19'

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('source', type=Path)
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / 'original_palette_chroma_source.png'
    source.write_bytes(args.source.read_bytes())
    base_path = ROOT / 'SourceArt/UI/Training/IdleStrip/QuietInkV18/idle_strip_quiet_ink_v18_seamless.png'
    base = np.asarray(Image.open(base_path).convert('RGBA'))
    assert base.shape == (793, 1983, 4)
    opened = Image.open(source)
    assert 1975 <= opened.width <= 1983 and 793 <= opened.height <= 796, opened.size
    data = extract_matte(opened)[:793]
    left = (1983-opened.width)//2
    data = np.pad(data, ((0,0),(left,1983-opened.width-left),(0,0)), mode='edge')
    valid = data[..., 3] >= 160
    needed = (base[..., 3] > 0) & ~valid
    missing_count = int(needed.sum())
    assert missing_count / np.count_nonzero(base[..., 3]) < 0.06, 'Generated outline moved too far'
    # Extend nearby generated colors beneath the original antialiased outline.
    # This repairs tiny generator edge shifts without changing the silhouette.
    for _ in range(12):
        if not needed.any():
            break
        for dy, dx in ((-1,0),(1,0),(0,-1),(0,1),(-1,-1),(-1,1),(1,-1),(1,1)):
            shifted_valid = np.roll(valid, (dy,dx), axis=(0,1))
            shifted_rgb = np.roll(data[..., :3], (dy,dx), axis=(0,1))
            take = needed & shifted_valid
            data[..., :3][take] = shifted_rgb[take]
            valid[take] = True
            needed[take] = False
    assert not needed.any(), 'Unfilled generated edge pixels'
    data[..., 3] = base[..., 3]
    final_data = close_horizontal_wrap(data)
    final_data[..., 3] = base[..., 3]
    final_data[base[..., 3] == 0, :3] = 0
    assert np.array_equal(final_data[..., 3], base[..., 3])
    edge_delta = int(np.abs(final_data[:,0].astype(np.int16)-final_data[:,-1].astype(np.int16)).max())
    assert edge_delta == 0
    final = OUT / 'idle_strip_quiet_ink_v19_seamless.png'
    Image.fromarray(final_data).save(final)
    Image.fromarray(final_data[256:491]).save(OUT / 'original_palette_cropped.png')
    report = {'status': 'technical-export-pass', 'generation': 'built-in imagegen palette-only edit',
              'source': str(source.relative_to(ROOT)), 'source_sha256': sha(source),
              'final': str(final.relative_to(ROOT)), 'sha256': sha(final),
              'previous_source': str(base_path.relative_to(ROOT)), 'previous_source_sha256': sha(base_path),
              'runtime_size': [1983,793], 'alpha_identical_to_previous': True,
              'horizontal_edge_max_delta': edge_delta, 'edge_color_pixels_extended': missing_count,
              'original_palette_reference': 'SourceArt/UI/ImageTruth/confirmed/training_idle_strip_background_seamless_v003.png',
              'palette_direction': 'cool slate-blue/gray mountains; warm ochre earth; gray-brown shadows'}
    for key, array in [('previous', base), ('current', final_data)]:
        report[key+'_palette'] = {}
        for label, y0, y1 in [('mountain',280,425),('road',450,480)]:
            region = array[y0:y1]
            colors = region[region[...,3]>240,:3]
            report[key+'_palette'][label+'_mean_rgb'] = np.round(colors.mean(axis=0),2).tolist()
    (OUT / 'manifest.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    font = ImageFont.truetype('C:/Windows/Fonts/msyh.ttc', 15)
    review = Image.new('RGB', (1100,335), (42,45,46))
    draw = ImageDraw.Draw(review)
    for label, array, y in [('调整前：偏亮青绿',base,29),('调整后：灰蓝山体／暖土黄道路',final_data,191)]:
        draw.text((14,y-24),label,font=font,fill=(218,222,218))
        strip=Image.fromarray(array[256:491]).resize((1100,130),Image.Resampling.LANCZOS)
        review.paste(strip,(0,y),strip)
    review.save(OUT / 'palette-before-after.png')
    print(json.dumps(report, ensure_ascii=False, indent=2))

if __name__ == '__main__':
    main()
