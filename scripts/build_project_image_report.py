"""Generate a searchable inventory from verified UE and WebP evidence."""
from pathlib import Path
import html
import json

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'Saved/ImageOptimization'
plan=json.loads((ROOT/'SourceAssets/ImageDelivery/texture-budget-plan.json').read_text(encoding='utf-8'))
applied=json.loads((OUT/'applied-textures.json').read_text(encoding='utf-8'))['textures']
webp=json.loads((ROOT/'SourceAssets/ImageDelivery/webp-manifest.json').read_text(encoding='utf-8'))['images']
rows=[]
for n in plan['records']:
    done=applied.get(n['package'],{})
    preview=webp.get(n['package'],{})
    status='已验证' if done.get('status')=='ready' else '待处理' if n['action']=='optimize' else '保留专用规格'
    rows.append(dict(asset=n['package'],status=status,reason=n['reason'],source_size=n['original_size'],
        target_size=n.get('target_size',n['original_size']),format=done.get('format',n['settings'].get('compression_settings','')),
        before_estimate=n.get('estimated_before_bytes'),after_actual=done.get('resource_bytes'),
        pixel_hash=done.get('source_pixels_sha1',''),webp_file=preview.get('file'),webp_bytes=preview.get('bytes')))
verified=[n for n in rows if n['status']=='已验证']
summary=dict(registered=len(rows),optimized_verified=len(verified),pending=sum(n['status']=='待处理' for n in rows),
    retained=sum(n['status']=='保留专用规格' for n in rows),webp_count=len(webp),
    webp_source_bytes=sum(n['source_bytes'] for n in webp.values()),webp_bytes=sum(n['bytes'] for n in webp.values()),
    optimized_old_payload_estimate=sum(n['before_estimate'] or 0 for n in verified),
    optimized_new_resident_actual=sum(n['after_actual'] or 0 for n in verified))
(OUT/'summary.json').write_text(json.dumps(summary,indent=2),encoding='utf-8')
document='''<!doctype html><html lang="zh-CN"><meta charset="utf-8"><title>GameXXK 图片优化核对</title><style>
body{margin:28px;background:#f2eadb;color:#342e27;font:15px/1.6 "Microsoft YaHei",sans-serif}h1{font-size:28px}button,input,select{font:inherit;padding:8px;border:1px solid #a79b85;background:#fffaf0;margin:4px}table{width:100%;border-collapse:collapse}th,td{border-bottom:1px solid #cfc3ad;padding:9px;text-align:left;vertical-align:top}th{position:sticky;top:0;background:#e6ddcb}code{word-break:break-all;font-size:12px}td:first-child{max-width:450px}.cards{display:flex;gap:15px;margin:20px 0}.cards div{background:#e3e9dc;padding:18px;min-width:140px}.cards b{display:block;font-size:28px}small{color:#716656}.preview{display:none;position:fixed;inset:8%;z-index:10;background:#ede2cd;border:2px solid #4b5742;padding:18px;box-shadow:0 0 0 100vmax #0008}.preview.open{display:block}.preview img{width:100%;height:85%;object-fit:contain}.preview button{float:right}</style>
<h1>GameXXK 图片优化核对</h1><p>游戏资产继续使用原 Texture2D 路径。显示类大图使用 BC7；法线/数据图保留专用格式；母版、精灵帧布局和角色原设保持。WebP供轻量预览。</p><div class="cards" id="cards"></div>
<p><small>表中旧占用是原尺寸BGRA8像素估算；新占用是UE实际读回、含平台分配差异的单资源数值。合计不等于游戏同时驻留内存，也不等于Cook后的发行包大小。34个非对齐精灵格图保留原格尺寸，不能为了套用BC7破坏帧坐标。</small></p>
<input id="search" placeholder="筛选路径，例如 StoryNodes、Relics、hero" size="44"><select id="filter"><option>全部</option><option>已验证</option><option>待处理</option><option>保留专用规格</option></select><span id="count"></span>
<table><thead><tr><th>资源</th><th>处理</th><th>尺寸</th><th>纹理格式</th><th>像素/内存</th><th>原图校验</th><th>WebP</th></tr></thead><tbody id="body"></tbody></table><div id="preview" class="preview"><button onclick="document.getElementById('preview').classList.remove('open')">关闭</button><p id="title"></p><img id="image"></div>
<script>const rows=DATA,summary=SUMMARY;const esc=s=>String(s).replace(/[&<>\"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','\"':'&quot;'}[c]));const mib=n=>n==null?'—':(n/1048576).toFixed(2)+' MiB';document.getElementById('cards').innerHTML=[['登记纹理',summary.registered],['BC7已验证',summary.optimized_verified],['待处理',summary.pending],['WebP预览',summary.webp_count]].map(([t,n])=>'<div>'+t+'<b>'+n+'</b></div>').join('');function render(){let s=document.getElementById('search').value.toLowerCase(),f=document.getElementById('filter').value;let list=rows.filter(n=>n.asset.toLowerCase().includes(s)&&(f==='全部'||n.status===f));document.getElementById('count').textContent=list.length+'项';document.getElementById('body').innerHTML=list.map(n=>'<tr><td><code>'+esc(n.asset)+'</code></td><td>'+n.status+'<br><small>'+esc(n.reason)+'</small></td><td>'+n.source_size.join('×')+'<br>→ '+n.target_size.join('×')+'</td><td>'+esc(n.format)+'</td><td>旧估算 '+mib(n.before_estimate)+'<br>新实测 '+mib(n.after_actual)+'</td><td>'+(n.pixel_hash?'<small title="'+n.pixel_hash+'">原像素哈希已核对</small>':'—')+'</td><td>'+(n.webp_file?'<button data-path="'+esc(n.webp_file)+'">'+(n.webp_bytes/1000).toFixed(0)+' KB</button>':'—')+'</td></tr>').join('');document.querySelectorAll('button[data-path]').forEach(b=>b.onclick=()=>{document.getElementById('title').textContent=b.dataset.path;document.getElementById('image').src='../../'+b.dataset.path;document.getElementById('preview').classList.add('open')});}document.getElementById('search').oninput=render;document.getElementById('filter').onchange=render;render();</script></html>'''
document=document.replace('DATA',json.dumps(rows,ensure_ascii=False)).replace('SUMMARY',json.dumps(summary))
(OUT/'report.html').write_text(document,encoding='utf-8')
print(json.dumps(dict(summary=summary,report=str(OUT/'report.html')),ensure_ascii=False))
