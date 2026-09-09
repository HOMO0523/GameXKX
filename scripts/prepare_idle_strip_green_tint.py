"""Apply the small generated mountain tint to the selected pre-cloud base."""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np
from PIL import Image
from prepare_idle_strip_quiet_background import extract_matte,close_horizontal_wrap

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'SourceArt/UI/Training/IdleStrip/QuietInkV23'

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    global OUT
    parser=argparse.ArgumentParser()
    parser.add_argument('source',type=Path)
    parser.add_argument('--revision',choices=('23','24'),default='24')
    parser.add_argument('--with-clouds',action='store_true')
    args=parser.parse_args()
    OUT=ROOT/f'SourceArt/UI/Training/IdleStrip/QuietInkV{args.revision}'
    OUT.mkdir(parents=True,exist_ok=True)
    source=OUT/'mountain_gray_green_chroma_source.png'
    source.write_bytes(args.source.read_bytes())
    selected_path=ROOT/'SourceArt/UI/Training/IdleStrip/QuietInkV21/user-selected-base.png'
    selected=Image.open(selected_path).convert('RGBA')
    canvas=Image.new('RGBA',(1983,793))
    canvas.paste(selected,(0,256))
    base=np.asarray(canvas).copy()
    prior=np.asarray(Image.open(ROOT/'SourceArt/UI/Training/IdleStrip/QuietInkV22/idle_strip_quiet_ink_v22_seamless.png').convert('RGBA'))
    opened=Image.open(source)
    assert 1975<=opened.width<=1983 and 793<=opened.height<=796,opened.size
    data=extract_matte(opened)[:793]
    pad=(1983-opened.width)//2
    data=np.pad(data,((0,0),(pad,1983-opened.width-pad),(0,0)),mode='edge')
    valid=data[...,3]>=160
    need=(base[...,3]>0)&~valid
    for _ in range(12):
        if not need.any():break
        for dy,dx in ((-1,0),(1,0),(0,-1),(0,1),(-1,-1),(-1,1),(1,-1),(1,1)):
            shifted_valid=np.roll(valid,(dy,dx),axis=(0,1))
            shifted_rgb=np.roll(data[...,:3],(dy,dx),axis=(0,1))
            take=need&shifted_valid
            data[...,:3][take]=shifted_rgb[take]
            valid[take]=True
            need[take]=False
    assert not need.any()
    ink=np.asarray(Image.open(ROOT/'SourceArt/UI/Training/IdleStrip/QuietInkV22/near_black_ink_layer.png').convert('RGBA'))
    weight=ink[...,3].astype(np.float64)/255
    tinted=np.rint(data[...,:3]*(1-weight[...,None])+ink[...,:3]*weight[...,None]).astype(np.uint8)
    # Only mountain color changes; selected ground and its current ink stay exact.
    blend=np.clip((np.arange(793)-420)/10,0,1)[:,None,None]
    result=base.copy()
    result[...,:3]=np.rint(tinted*(1-blend)+prior[...,:3]*blend).astype(np.uint8)
    result=close_horizontal_wrap(result)
    result[...,3]=base[...,3]
    result[430:,:,:3]=prior[430:,:,:3]
    result[base[...,3]==0,:3]=0
    assert np.array_equal(result[...,3],base[...,3])
    opaque_ground=(base[430:,:,3]==255)
    assert np.array_equal(result[430:][opaque_ground],prior[430:][opaque_ground])
    assert np.array_equal(result[:,0],result[:,-1])
    if args.with_clouds:
        cloud=Image.open(ROOT/'SourceArt/UI/Training/IdleStrip/QuietInkV22/cloud_sea_layer.png').convert('RGBA')
        combined=np.asarray(Image.alpha_composite(cloud,Image.fromarray(result))).copy()
        opaque=result[...,3]==255
        assert np.array_equal(combined[opaque],result[opaque]),'Cloud covered the foreground'
        result=combined
        assert np.array_equal(result[:,0],result[:,-1])
    final=OUT/f'idle_strip_quiet_ink_v{args.revision}_seamless.png'
    Image.fromarray(result).save(final)
    Image.fromarray(result[220 if args.with_clouds else 256:535]).save(OUT/'gray_green_cropped.png')
    report={'status':'technical-export-pass','generation':'built-in imagegen small mountain color edit',
            'final':str(final.relative_to(ROOT)),'sha256':sha(final),
            'source':str(source.relative_to(ROOT)),'source_sha256':sha(source),
            'selected_pre_cloud_base':str(selected_path.relative_to(ROOT)),'selected_base_sha256':sha(selected_path),
            'size':[1983,793],'foreground_alpha_identical_to_selected_base':True,'cloud_layer_removed':not args.with_clouds,
            'near_black_ink_rgb':[43,44,42],
            'cloud_source':'SourceArt/UI/Training/IdleStrip/QuietInkV22/cloud_sea_layer.png' if args.with_clouds else None,
            'opaque_ground_and_ink_unchanged_from_row':430,'horizontal_edge_max_delta':0,
            'fully_opaque_under_health_bars':bool(result[460:509,:,3].min()==255)}
    (OUT/'manifest.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(report,ensure_ascii=False,indent=2))

if __name__=='__main__':main()
