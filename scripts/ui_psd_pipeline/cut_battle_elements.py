# -*- coding: utf-8 -*-
"""Cut every battle UI/UX element from the three real screenshots using the
luna inventory (luna-battle-ui-inventory-20260813.md) as the source lock,
then build a labeled contact sheet for user review.
"""
import hashlib
import json
import os
import re

from PIL import Image, ImageDraw, ImageFont

ROOT = 'D:/UE5 demo/GameXXK'
INVENTORY = ROOT + '/Saved/Codex/luna-battle-ui-inventory-20260813.md'
IMAGES = {
    1: ROOT + '/Saved/Codex/battle_open_pie.png',
    2: ROOT + '/Saved/Codex/battle_target_select_pie.png',
    3: ROOT + '/Saved/Codex/battle_reward_pie.png',
}
OUT_DIR = ROOT + '/SourceArt/UI/PSD/gamexxk-v4/ui-master/Generated/battle-cuts-20260813'
SHEET = ROOT + '/SourceArt/UI/PSD/gamexxk-v4/ui-master/Review/RedesignPages/BattleCuts/battle-cuts-contact-sheet.png'
MANIFEST = ROOT + '/SourceArt/UI/PSD/gamexxk-v4/ui-master/Generated/battle-cuts-20260813/cut-manifest.json'


def parse_inventory():
    text = open(INVENTORY, encoding='utf-8').read()
    # rows: | 1-01 | 名称 | 分类 | 状态 | 文字/字号 | [x1,y1,x2,y2] | [..] |
    pattern = re.compile(r'\|\s*(\d)-(\d\d)\s*\|\s*([^|]+?)\s*\|\s*([^|]+?)\s*\|\s*([^|]+?)\s*\|.*?\|\s*`?\s*\[(\d+),(\d+),(\d+),(\d+)\]\s*`?\s*\|')
    rows = []
    seen = set()
    for m in pattern.finditer(text):
        img, num, name, category, state = m.group(1), m.group(2), m.group(3).strip(), m.group(4).strip(), m.group(5).strip()
        box = [int(m.group(6)), int(m.group(7)), int(m.group(8)), int(m.group(9))]
        key = (img, num)
        if key in seen:
            continue
        seen.add(key)
        rows.append({'img': int(img), 'num': num, 'name': name, 'category': category, 'state': state, 'box': box})
    # drop window title bars (row 01)
    rows = [r for r in rows if r['num'] != '01']
    # replace img-3 rows with the clean recapture spec (settled reward overlay)
    rows = [r for r in rows if r['img'] != 3]
    spec = json.load(open(ROOT + '/SourceArt/UI/PSD/gamexxk-v4/ui-master/Generated/battle-cuts-20260813/reward-cuts-new.json', encoding='utf-8'))
    for s in spec['rows']:
        rows.append({'img': 3, 'num': s['num'], 'name': s['name'], 'category': s['category'], 'state': s['state'], 'box': s['box']})
    return rows


def sha256(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(65536), b''):
            h.update(chunk)
    return h.hexdigest()


def main():
    rows = parse_inventory()
    print('parsed', len(rows), 'element rows')
    os.makedirs(OUT_DIR, exist_ok=True)
    os.makedirs(os.path.dirname(SHEET), exist_ok=True)
    manifest = []
    for r in rows:
        src = Image.open(IMAGES[r['img']])
        x1, y1, x2, y2 = r['box']
        crop = src.crop((x1, y1, x2, y2))
        safe = re.sub(r'[\\/:*?"<>|]', '_', r['name'])
        fname = f"{r['img']}-{r['num']}_{safe}.png"
        path = OUT_DIR + '/' + fname
        crop.save(path)
        manifest.append({
            'file': 'battle-cuts-20260813/' + fname,
            'source': os.path.basename(IMAGES[r['img']]),
            'boxNative': r['box'],
            'size': list(crop.size),
            'sha256': sha256(path),
            'category': r['category'],
            'state': r['state'],
        })
    # contact sheet: grid of 6 columns, cell 260x230, label under crop
    cols = 6
    cell_w, cell_h, label_h = 260, 200, 46
    n = len(rows)
    rows_n = (n + cols - 1) // cols
    sheet = Image.new('RGB', (cols * cell_w, rows_n * (cell_h + label_h)), (24, 26, 25))
    draw = ImageDraw.Draw(sheet)
    try:
        font = ImageFont.truetype('C:/Windows/Fonts/msyh.ttc', 20)
        font_small = ImageFont.truetype('C:/Windows/Fonts/msyh.ttc', 16)
    except OSError:
        font = font_small = ImageFont.load_default()
    for i, r in enumerate(rows):
        col, row = i % cols, i // cols
        x0, y0 = col * cell_w, row * (cell_h + label_h)
        fname = f"{r['img']}-{r['num']}_{re.sub(r'[\\/:*?"<>|]', '_', r['name'])}.png"
        crop = Image.open(OUT_DIR + '/' + fname)
        crop.thumbnail((cell_w - 16, cell_h - 16))
        cx = x0 + (cell_w - crop.size[0]) // 2
        cy = y0 + (cell_h - crop.size[1]) // 2
        sheet.paste(crop, (cx, cy))
        draw.rectangle([x0, y0, x0 + cell_w - 1, y0 + cell_h - 1], outline=(90, 86, 78))
        label = f"{r['img']}-{r['num']} {r['name']} ({r['box'][2]-r['box'][0]}x{r['box'][3]-r['box'][1]})"
        draw.text((x0 + 8, y0 + cell_h + 8), label, fill=(222, 214, 198), font=font_small)
    sheet.save(SHEET)
    with open(MANIFEST, 'w', encoding='utf-8') as f:
        json.dump({'status': 'PASS', 'elementCount': n, 'elements': manifest}, f, ensure_ascii=False, indent=1)
    print('cuts:', n, 'sheet:', SHEET)
    print('manifest:', MANIFEST)


if __name__ == '__main__':
    main()
