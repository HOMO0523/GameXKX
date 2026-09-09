"""Use generated earth to extend the lane by the measured health-bar deficit."""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np
from PIL import Image
from prepare_idle_strip_quiet_background import extract_matte, close_horizontal_wrap

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'SourceArt/UI/Training/IdleStrip/QuietInkV20'

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('earth_source', type=Path)
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / 'extended_earth_chroma_source.png'
    source.write_bytes(args.earth_source.read_bytes())
    opened = Image.open(source)
    assert 1975 <= opened.width <= 1983 and 793 <= opened.height <= 796, opened.size
    earth = extract_matte(opened)[:793]
    pad = (1983-opened.width)//2
    earth = np.pad(earth, ((0,0),(pad,1983-opened.width-pad),(0,0)), mode='edge')
    base_path = ROOT / 'SourceArt/UI/Training/IdleStrip/QuietInkV19/idle_strip_quiet_ink_v19_seamless.png'
    base = np.asarray(Image.open(base_path).convert('RGBA'))
    result = base.copy()
    extension = 40
    # Preserve the mountain silhouette and character-foot registration.
    result[430:, :, 3] = np.maximum(base[430:, :, 3], base[430-extension:-extension, :, 3])
    assert np.array_equal(result[:430, :, 3], base[:430, :, 3])
    assert earth[450:532,:,3].min() > 240, 'Generated earth cannot cover the extension'
    blend = np.clip((np.arange(793)-450)/20, 0, 1)[:,None,None]
    result[..., :3] = np.rint(base[..., :3]*(1-blend)+earth[..., :3]*blend).astype(np.uint8)

    # Reuse precisely the same generated ink paths; move only the lower rim.
    ink_path = ROOT / 'SourceArt/UI/Training/IdleStrip/QuietInkV18/gray_ink_overlay_rgba.png'
    old_ink = np.asarray(Image.open(ink_path).convert('RGBA'))[...,3]
    ink = old_ink.copy()
    ink[470:] = 0
    ink[470+extension:] = old_ink[470:-extension]
    lum = result[..., :3] @ np.array([0.2126,0.7152,0.0722])
    ink_weight = ink.astype(np.float64)/255 * 0.80
    ink_weight *= np.clip((lum-121)/24,0,1)
    ink_weight *= result[...,3]/255
    gray = np.array([118,121,117])
    result[..., :3] = np.rint(result[..., :3]*(1-ink_weight[...,None])+gray*ink_weight[...,None]).astype(np.uint8)
    required_alpha = result[...,3].copy()
    result = close_horizontal_wrap(result)
    result[...,3] = required_alpha
    result[result[...,3] == 0,:3] = 0
    assert np.array_equal(result[:,0],result[:,-1]), 'Seam changed'
    # Each tile can pass behind any health bar at any scrolling offset.
    # All bars occupy local y=174..192; 300px tile displays 793 source rows.
    required_row = int(np.ceil(192/300*793))
    assert result[460:required_row+1,:,3].min() >= 250, 'A bar can extend outside the earth'
    opaque = result[...,3] >= 250
    bottom_by_x = np.where(opaque, np.arange(793)[:,None], -1).max(axis=0)+1
    floor_min = float(bottom_by_x.min()*300/793)
    floor_max = float(bottom_by_x.max()*300/793)
    assert floor_min >= 198 and floor_max <= 202
    final = OUT / 'idle_strip_quiet_ink_v20_seamless.png'
    Image.fromarray(result).save(final)
    Image.fromarray(result[256:535]).save(OUT / 'thick_ground_cropped.png')
    report = {'status':'technical-export-pass','generation':'built-in imagegen; measured ground extension and existing ink-layer compositing',
              'final':str(final.relative_to(ROOT)),'sha256':sha(final),
              'base':str(base_path.relative_to(ROOT)),'base_sha256':sha(base_path),
              'earth_source':str(source.relative_to(ROOT)),'earth_source_sha256':sha(source),
              'size':[1983,793],'ground_extension_source_pixels':extension,
              'mountain_alpha_identical_above_row':430,'ink_rgb':gray.tolist(),
              'health_bar_local_y':[174,192],'required_texture_row':required_row,
              'ground_local_bottom_range':[floor_min,floor_max],
              'minimum_ground_margin_below_bars':floor_min-192,
              'viewport_local_height':202,'alpha_under_all_bar_positions_min':int(result[460:required_row+1,:,3].min()),
              'horizontal_edge_max_delta':0}
    (OUT/'manifest.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(report,ensure_ascii=False,indent=2))

if __name__ == '__main__':
    main()
