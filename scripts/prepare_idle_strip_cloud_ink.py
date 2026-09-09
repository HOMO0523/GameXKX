"""Place generated cloud sea behind the user-selected base and deepen its ink."""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np
from PIL import Image
from prepare_idle_strip_quiet_background import extract_matte, close_horizontal_wrap

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'SourceArt/UI/Training/IdleStrip/QuietInkV21'

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    global OUT
    parser = argparse.ArgumentParser()
    parser.add_argument('cloud_source', type=Path)
    parser.add_argument('--revision', choices=('21','22'), default='22')
    parser.add_argument('--cloud-opacity', type=float, default=1.0)
    args = parser.parse_args()
    assert 0 < args.cloud_opacity <= 1
    OUT = ROOT / f'SourceArt/UI/Training/IdleStrip/QuietInkV{args.revision}'
    OUT.mkdir(parents=True,exist_ok=True)
    selected = OUT/'user-selected-base.png'
    previous_selected = ROOT/'SourceArt/UI/Training/IdleStrip/QuietInkV21/user-selected-base.png'
    if selected != previous_selected:
        selected.write_bytes(previous_selected.read_bytes())
    image = Image.open(selected).convert('RGBA')
    assert image.size == (1983,279)
    base_image = Image.new('RGBA',(1983,793))
    base_image.paste(image,(0,256))
    base = np.asarray(base_image).copy()

    source = OUT/'cloud_sea_chroma_source.png'
    source.write_bytes(args.cloud_source.read_bytes())
    opened = Image.open(source)
    assert 1975 <= opened.width <= 1983 and 793 <= opened.height <= 796, opened.size
    cloud = extract_matte(opened)[:793]
    pad = (1983-opened.width)//2
    cloud = np.pad(cloud,((0,0),(pad,1983-opened.width-pad),(0,0)),mode='edge')
    cloud = close_horizontal_wrap(cloud)
    cloud[...,3] = np.rint(cloud[...,3].astype(np.float64)*args.cloud_opacity).astype(np.uint8)
    # Cloud is a rear layer: its lower end stays behind opaque ground.
    cloud[524:] = 0
    Image.fromarray(cloud).save(OUT/'cloud_sea_layer.png')

    old = np.asarray(Image.open(ROOT/'SourceArt/UI/Training/IdleStrip/QuietInkV18/gray_ink_overlay_rgba.png').convert('RGBA'))[...,3]
    ink = old.copy()
    ink[470:] = 0
    ink[510:] = old[470:-40]
    coverage = np.clip(ink.astype(np.float64)/(255*0.84),0,1)
    weight = coverage*0.96*base[...,3]/255
    lum = base[...,:3] @ np.array([0.2126,0.7152,0.0722])
    weight *= np.clip((lum-44)/18,0,1)
    color = np.array([43,44,42])
    foreground = base.copy()
    foreground[...,:3] = np.rint(base[...,:3]*(1-weight[...,None])+color*weight[...,None]).astype(np.uint8)
    assert np.array_equal(foreground[...,3],base[...,3])
    assert np.array_equal(foreground[weight==0],base[weight==0])
    line_layer = np.zeros_like(base)
    line_layer[...,:3] = color
    line_layer[...,3] = np.rint(weight*255).astype(np.uint8)
    Image.fromarray(line_layer).save(OUT/'near_black_ink_layer.png')
    result = Image.alpha_composite(Image.fromarray(cloud),Image.fromarray(foreground))
    data = np.asarray(result)
    assert np.array_equal(data[:,0],data[:,-1]), 'Horizontal seam changed'
    # Every fully opaque, uninked foreground pixel remains the selected color.
    protected = (base[...,3]==255)&(weight==0)
    assert np.array_equal(data[protected],base[protected]), 'Cloud covered the selected foreground'
    assert np.array_equal(data[524:,:,3],base[524:,:,3]), 'Cloud spilled below ground'
    final = OUT/f'idle_strip_quiet_ink_v{args.revision}_seamless.png'
    result.save(final)
    result.crop((0,220,1983,535)).save(OUT/'cloud_ink_cropped.png')
    report={'status':'technical-export-pass','generation':'built-in imagegen cloud layer and existing generated ink paths',
            'final':str(final.relative_to(ROOT)),'sha256':sha(final),
            'selected_base':str(selected.relative_to(ROOT)),'selected_base_sha256':sha(selected),
            'selected_base_size':[1983,279],'canvas_origin':[0,256],'size':[1983,793],
            'cloud_source':str(source.relative_to(ROOT)),'cloud_source_sha256':sha(source),
            'cloud_opacity':args.cloud_opacity,'near_black_rgb':color.tolist(),'maximum_ink_opacity':0.96,
            'foreground_alpha_unchanged':True,'opaque_uninked_foreground_pixels_unchanged':True,
            'cloud_below_ground':False,'horizontal_edge_max_delta':0,
            'fully_opaque_under_health_bars':bool(data[460:509,:,3].min()==255)}
    (OUT/'manifest.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(report,ensure_ascii=False,indent=2))

if __name__=='__main__':
    main()
