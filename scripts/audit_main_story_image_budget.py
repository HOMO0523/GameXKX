"""Measure encoding/size tradeoffs on one user-selected image; never redraw or replace assets."""
import argparse
import hashlib
import html
import json
from pathlib import Path
import shutil

import numpy as np
from PIL import Image, ImageOps, __version__ as pillow_version

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('source', type=Path)
    args = parser.parse_args()
    source = args.source.resolve(strict=True)
    source_sha = hashlib.sha256(source.read_bytes()).hexdigest()
    out = ROOT / 'Saved/StorySystem/ImageBudget' / source_sha[:12]
    out.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, out / 'original.png')
    master = Image.open(source).convert('RGB')
    rows = []

    def measure(path, baseline, label, role):
        decoded = Image.open(path).convert('RGB')
        error = np.asarray(decoded, dtype=np.float32) - np.asarray(baseline, dtype=np.float32)
        mse = float(np.mean(error * error))
        row = dict(label=label, file=path.name, size=list(decoded.size), bytes=path.stat().st_size,
                   saving_percent=round(100*(1-path.stat().st_size/source.stat().st_size), 2),
                   encoding_psnr_db=round(float(10*np.log10(255*255/mse)), 2) if mse else None,
                   pixel_identical=not bool(mse), role=role,
                   sha256=hashlib.sha256(path.read_bytes()).hexdigest())
        rows.append(row)
        return row

    measure(out/'original.png', master, '原始 PNG', 'source')
    for size, prefix in [(master.size, 'full'), ((1536,512), 'detail'), ((384,128), 'thumb')]:
        # Keep aspect ratio. Any 1–2px fit padding is plain paper, not stretching the faces.
        fitted = ImageOps.contain(master, size, Image.Resampling.LANCZOS)
        baseline = Image.new('RGB', size, '#f4ead5')
        baseline.paste(fitted, ((size[0]-fitted.width)//2, (size[1]-fitted.height)//2))
        for fmt, opts, suffix in [('PNG',dict(optimize=True,compress_level=9),'lossless.png'),
                                  ('JPEG',dict(quality=92,subsampling=0,optimize=True,progressive=True),'q92.jpg'),
                                  ('WEBP',dict(quality=90,method=6),'q90.webp')]:
            if prefix == 'thumb' and fmt == 'JPEG':
                continue
            path = out / f'{prefix}_{suffix}'
            baseline.save(path, format=fmt, **opts)
            measure(path, baseline, f'{size[0]}×{size[1]} {fmt}'+(' Q92 / 4:4:4' if fmt=='JPEG' else ' Q90' if fmt=='WEBP' else ' 无损编码'), prefix)

    manifest = json.loads((ROOT/'SourceArt/UI/StoryNodes/manifest.json').read_text(encoding='utf-8'))
    selected = [ROOT/n['file'] for n in manifest['nodes'].values()]
    imported = list((ROOT/'Content/GameXXK/UI/StoryNodes').glob('T_Story*.uasset'))
    report = dict(source=str(source), source_sha256=source_sha, pillow_version=pillow_version,
        note='Measurements are actual encoded file sizes, not cooked game sizes. PSNR compares each decoded candidate only with its same-size pre-encoding image; it does not score resize quality or character identity.',
        selected_manifest_count=len(selected), selected_manifest_bytes=sum(p.stat().st_size for p in selected),
        historical_source_png_count=len(list((ROOT/'SourceArt/UI/StoryNodes').glob('*.png'))),
        historical_source_png_bytes=sum(p.stat().st_size for p in (ROOT/'SourceArt/UI/StoryNodes').glob('*.png')),
        imported_uasset_count=len(imported), imported_uasset_bytes=sum(p.stat().st_size for p in imported),
        gpu_estimates_no_mips_or_allocator_overhead=dict(original_bgra8=master.width*master.height*4,
            detail_bgra8=1536*512*4, detail_bc7=1536*512, thumb_bc7=384*128,
            all_61_original_bgra8=61*master.width*master.height*4,
            all_61_detail_bc7=61*1536*512, all_61_thumb_bc7=61*384*128), candidates=rows)
    (out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')

    cards=''.join(f'<button data-file="{r["file"]}">{html.escape(r["label"])} · {r["bytes"]/1024:.0f} KiB</button>' for r in rows if r['role'] in ('full','detail'))
    table=''.join(f'<tr><td>{html.escape(r["label"])}</td><td>{r["bytes"]/1024:.1f} KiB</td><td>{r["saving_percent"]}%</td><td>{"像素一致" if r["pixel_identical"] else str(r["encoding_psnr_db"])+" dB"}</td></tr>' for r in rows)
    page='''<!doctype html><html lang="zh-CN"><meta charset="utf-8"><title>同一张剧情图：体积与画质</title>
<style>*{box-sizing:border-box}body{margin:28px;background:#eee5d4;color:#302a25;font:16px/1.65 "Microsoft YaHei",sans-serif}main{max-width:1600px;margin:auto}h1{font-size:27px}button,select{border:1px solid #a49c88;background:#faf4e7;border-radius:5px;padding:9px 14px;cursor:pointer;margin:4px}button.active{background:#315445;color:#fff}.frame{overflow:auto;border:1px solid #b0a58f;background:#f4ead5;padding:0}.compare{position:relative;width:897px;aspect-ratio:2170/725}.compare img{position:absolute;width:100%;height:100%;object-fit:fill;max-width:none}.cut{position:absolute;inset:0;clip-path:inset(0 50% 0 0)}.line{position:absolute;left:50%;top:0;bottom:0;border-left:2px solid #ab472c;pointer-events:none}.labels{display:flex;justify-content:space-between;font-size:14px}input[type=range]{width:100%;margin:15px 0}table{border-collapse:collapse;margin:22px 0;width:100%}td,th{text-align:left;border-bottom:1px solid #cbbfa8;padding:10px}small{color:#6c6354}.note{max-width:1000px}.controls{margin:14px 0}</style>
<main><h1>同一张剧情图：保留笔触，压缩交付体积</h1>
<p class="note">这是用户所贴原图的编码与尺寸对照，没有重新生成，也未修改人物。人物偏离原设的问题仍需单独修正。左侧原图，右侧候选；拖动分界看眼眉、墨线、纸底与衣料笔触。</p>
<div class="controls">'''+cards+'''</div><label>显示宽度 <select id="width"><option value="448">448px（桌面50%）</option><option value="673">673px（桌面75%）</option><option selected value="897">897px（旧界面图容器宽）</option><option value="1536">1536px（细节图像素）</option><option value="2170">2170px（原图像素）</option></select></label>
<p id="which"></p><div class="labels"><span>左：原始 PNG</span><span>右：当前候选</span></div>
<div class="frame"><div class="compare" id="compare"><img id="candidate" src="detail_q92.jpg"><div class="cut" id="cut"><img src="original.png"></div><div class="line" id="line"></div></div></div>
<input id="slider" type="range" min="0" max="100" value="50" aria-label="原图与候选分界"><small>宽度是对照展示尺寸；尚未代表新界面实际物理像素或UE压缩后的效果。全屏可能放大，正式上限需随界面验证。</small>
<table><thead><tr><th>编码方案</th><th>实测文件大小</th><th>比原PNG减少</th><th>同尺寸编码指标</th></tr></thead><tbody>'''+table+'''</tbody></table>
<p class="note">JPG / WebP 文件小，不等于GPU纹理内存小。游戏内还需调整尺寸与纹理压缩。WebP是审阅分发候选，不假定UE现有导入管线支持。PSNR只衡量编码误差；不衡量构图、角色一致性或缩放后的清晰度。</p></main>
<script>const c=document.getElementById('candidate'),s=document.getElementById('slider');document.querySelectorAll('button[data-file]').forEach(b=>b.onclick=()=>{c.src=b.dataset.file;document.querySelectorAll('button').forEach(x=>x.classList.remove('active'));b.classList.add('active');document.getElementById('which').textContent='当前候选：'+b.textContent;});s.oninput=()=>{document.getElementById('cut').style.clipPath=`inset(0 ${100-s.value}% 0 0)`;document.getElementById('line').style.left=s.value+'%';};document.getElementById('width').onchange=e=>document.getElementById('compare').style.width=e.target.value+'px';document.querySelector('[data-file="detail_q92.jpg"]').click();</script></html>'''
    (out/'compare.html').write_text(page,encoding='utf-8')
    assert hashlib.sha256(source.read_bytes()).hexdigest()==source_sha
    print(json.dumps({'report':str(out/'report.json'),'comparison':str(out/'compare.html'),**report},ensure_ascii=False))


if __name__=='__main__':
    main()
