"""Deterministic checks and an offline review gallery for all authored story illustrations."""
import argparse
import hashlib
import html
import json
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--require-import', action='store_true')
    args = parser.parse_args()
    folder = ROOT / 'SourceArt/UI/StoryNodes'
    manifest = json.loads((folder / 'manifest.json').read_text(encoding='utf-8'))
    campaign = json.loads((ROOT / 'SourceAssets/Narrative/MainStory/campaign.json').read_text(encoding='utf-8'))
    nodes = {n['id']: n for c in campaign['chapters'] for n in c['nodes']}
    assert len(nodes) == 61 and set(nodes) == set(manifest['nodes'])
    hashes = set()
    for node_id, node in nodes.items():
        entry = manifest['nodes'][node_id]
        path = (ROOT / entry['file']).resolve(strict=True)
        assert path.is_relative_to(folder.resolve())
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        assert digest == entry['sha256'] == entry['reviewed_sha256'], node_id
        assert digest not in hashes, f'{node_id}: another node reuses this image'
        hashes.add(digest)
        assert entry['visual_review'] == 'pass'
        assert entry['display_phase'] == node['art']['display_phase']
        assert entry.get('style_version') == 'clean_ink_v2'
        assert entry['cast'] == node['art']['cast'] and entry['texture'] == node['art']['texture']
        contract = json.dumps({'art': node['art'], 'result': node['result']}, ensure_ascii=False, sort_keys=True)
        assert hashlib.sha256(contract.encode('utf-8')).hexdigest() == entry['contract_sha256'], node_id
        with Image.open(path) as image:
            assert list(image.size) == entry['size']
            assert image.width >= 2100 and 2.8 <= image.width / image.height <= 3.2
            assert image.mode in ('RGB', 'RGBA')
    if args.require_import:
        report = json.loads((ROOT / 'Saved/StorySystem/art-import-report.json').read_text(encoding='utf-8'))
        assert report['complete'] and len(report['textures']) == 61
        for texture in report['textures']:
            assert texture['sha256'] == manifest['nodes'][texture['id']]['sha256']
            package = texture['asset'].split('.')[0].replace('/Game/', 'Content/') + '.uasset'
            assert (ROOT / package).is_file(), package
    sections = []
    for chapter in campaign['chapters']:
        cards = []
        for n in chapter['nodes']:
            entry = manifest['nodes'][n['id']]
            title = html.escape(n['id'] + ' · ' + n['title'])
            filename = html.escape(Path(entry['file']).name)
            lines = ''.join('<p><b>'+html.escape(campaign['characters'][name]['name'])+'</b> '+html.escape(text)+'</p>' for name, text in n['lines'])
            cards.append(f'<article><h3>{title}</h3><a href="{filename}" target="_blank"><img loading="lazy" src="{filename}" alt="{title}"></a><p>{html.escape(n["result"])}</p><details><summary>对白与任务描述</summary><p>{html.escape(n["summary"])}</p>{lines}</details></article>')
        sections.append('<section id="'+chapter['id']+'"><h2>'+html.escape(chapter['title'])+'</h2>'+''.join(cards)+'</section>')
    document = '''<!doctype html><html lang="zh-CN"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>六章剧情 · 61张节点插图</title><style>body{margin:0;background:#ece4d3;color:#362e22;font:17px/1.75 system-ui,"Microsoft YaHei",sans-serif}header,main{max-width:1280px;margin:auto;padding:28px}header{border-bottom:1px solid #bcae94}h1{font-size:30px}nav{display:flex;gap:18px;flex-wrap:wrap}a{color:#805331}article{background:#f7f0e2;border:1px solid #cdbd9d;border-radius:8px;margin:24px 0;padding:18px 24px}img{display:block;width:100%;height:auto}h3{margin:0 0 12px}details{border-top:1px solid #d7cab5;padding-top:10px}summary{cursor:pointer}h2{padding-top:25px}p{margin:10px 0}</style><header><h1>六章剧情 · 61张节点插图</h1><p>一节点一张图；图中呈现该节点已发生的结果。点击图片查看原图，展开卡片可核对白。</p><nav>'''
    document += ''.join(f'<a href="#{c["id"]}">{html.escape(c["title"])}</a>' for c in campaign['chapters'])
    document += '</nav></header><main>'+''.join(sections)+'</main></html>'
    (folder / 'gallery.html').write_text(document, encoding='utf-8')
    report = {'nodes': len(nodes), 'unique_images': len(hashes), 'visually_reviewed': 61, 'import_verified': args.require_import, 'gallery': str(folder / 'gallery.html')}
    (ROOT / 'Saved/StorySystem/art-validation.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps(report, ensure_ascii=False))

if __name__ == '__main__':
    main()
