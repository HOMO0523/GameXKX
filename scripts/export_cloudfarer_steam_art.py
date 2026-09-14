"""Export user-approved crops/layouts from the original generated Steam key art.

No gameplay screenshot is generated or altered here. Source art remains untouched.
"""
from pathlib import Path
import csv
import hashlib
import json
from PIL import Image, ImageDraw, ImageFont, ImageOps

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'Deliverables/Steam/CloudFarer'
SOURCE = OUT / 'drafts/key-art-v1.png'
FONT = ROOT / 'Saved/FontPreview/20260912-keinann-maru-pop/fonts/KeinannMaruPOP.ttf'


def run():
    art = Image.open(SOURCE).convert('RGB')
    # Coordinates were reviewed against this specific source composition.
    w, h = art.size
    def box(x0, y0, x1, y1):
        return (round(x0*w/1657), round(y0*h/949), round(x1*w/1657), round(y1*h/949))
    title = art.crop(box(455, 125, 1215, 500)).convert('RGBA')
    gray = title.convert('L')
    alpha = gray.point(lambda v: max(0, min(255, round((170-v)*255/140))))
    ink = Image.new('RGBA', title.size, (24, 25, 23, 0))
    clean = ImageDraw.Draw(alpha)
    clean.rectangle((0, round(title.height*.84), round(title.width*.22), title.height), fill=0)
    clean.rectangle((round(title.width*.84), round(title.height*.84), title.width, title.height), fill=0)
    ink.putalpha(alpha)
    landscape = art.crop(box(0, 515, 1657, 949))
    entries = []

    def save(name, label, size, image, section, required=True):
        path = OUT / section / name
        path.parent.mkdir(parents=True, exist_ok=True)
        assert image.size == size
        if path.suffix == '.jpg':
            image.convert('RGB').save(path, quality=95, subsampling=0)
        else:
            image.save(path)
        entries.append(dict(file=str(path.relative_to(OUT)).replace('\\', '/'), purpose=label,
                            width=size[0], height=size[1], required=required, count=1,
                            sha256=hashlib.sha256(path.read_bytes()).hexdigest(), status='exported; artistic review pending'))

    def logo(size, padding=0):
        result = Image.new('RGBA', size, (0, 0, 0, 0))
        fitted = ImageOps.contain(ink, (size[0]-padding*2, size[1]-padding*2), Image.Resampling.LANCZOS)
        result.alpha_composite(fitted, ((size[0]-fitted.width)//2, (size[1]-fitted.height)//2))
        return result

    def portrait(size):
        pw, ph = size
        canvas = Image.new('RGB', size, (235, 225, 205))
        scenery = ImageOps.fit(landscape, (pw, round(ph*.70)), Image.Resampling.LANCZOS, centering=(.34, .5))
        top = ph - scenery.height
        mask = Image.new('L', scenery.size, 255)
        fade = ImageDraw.Draw(mask)
        for y in range(round(ph*.15)):
            fade.line((0,y,pw,y), fill=round(255*y/max(1,round(ph*.15)-1)))
        canvas.paste(scenery, (0, top), mask)
        mark = logo((round(pw*.9), round(ph*.32)))
        canvas.paste(mark, ((pw-mark.width)//2, round(ph*.06)), mark)
        return canvas

    header = ImageOps.fit(art.crop(box(0,30,1657,825)), (920,430), Image.Resampling.LANCZOS)
    save('01_HeaderCapsule_920x430.png', '商店形象图片 Header Capsule', (920,430), header, '01_store')
    small = Image.new('RGB', (462,174), (237,226,204))
    mark = logo((434,166))
    small.paste(mark, (14,4), mark)
    save('02_SmallCapsule_462x174.png', '商店小宣传图 Small Capsule', (462,174), small, '01_store')
    save('03_MainCapsule_1232x706.png', '商店主宣传图 Main Capsule', (1232,706), ImageOps.fit(art,(1232,706),Image.Resampling.LANCZOS), '01_store')
    save('04_VerticalCapsule_748x896.png', '商店竖向宣传图 Vertical Capsule', (748,896), portrait((748,896)), '01_store')
    save('05_LibraryCapsule_600x900.png', '库宣传图 Library Capsule', (600,900), portrait((600,900)), '02_library')
    save('06_LibraryHeader_920x430.png', '库形象图片 Library Header', (920,430), header.copy(), '02_library')
    save('07_LibraryHero_NO_TEXT_3840x1240.png', '库主页背景 Library Hero（无文字）', (3840,1240), ImageOps.fit(landscape,(3840,1240),Image.Resampling.LANCZOS), '02_library')
    save('08_LibraryLogo_TRANSPARENT_1280x720.png', '库徽标 Library Logo（透明）', (1280,720), logo((1280,720),30), '02_library')
    icon = Image.open(ROOT / 'SourceArt/UI/AppIcon/GameXXK-icon-source.png').convert('RGB')
    save('09_ShortcutIcon_512x512.png', '快捷方式图标 Shortcut Icon', (512,512), ImageOps.fit(icon,(512,512),Image.Resampling.LANCZOS), '03_icons')
    save('10_AppIcon_184x184.jpg', '应用图标 App Icon', (184,184), ImageOps.fit(icon,(184,184),Image.Resampling.LANCZOS), '03_icons')
    save('11_StoreBackground_OPTIONAL_1438x810.png', '商店页面背景（可选）', (1438,810), ImageOps.fit(landscape,(1438,810),Image.Resampling.LANCZOS), '04_optional',False)

    (OUT/'art-manifest.json').write_text(json.dumps(entries,ensure_ascii=False,indent=2),encoding='utf-8')
    with (OUT/'art-manifest.csv').open('w',encoding='utf-8-sig',newline='') as f:
        writer=csv.DictWriter(f,fieldnames=list(entries[0]));writer.writeheader();writer.writerows(entries)
    sheet=Image.new('RGB',(1500,4*350+90),(244,241,234));draw=ImageDraw.Draw(sheet)
    font=ImageFont.truetype(str(FONT),22);smallfont=ImageFont.truetype(str(FONT),17)
    draw.text((25,20),'霞客行 / CloudFarer — Steam 素材用途总览',fill=(30,35,35),font=font)
    for i,e in enumerate(entries):
        x=20+(i%3)*500;y=90+(i//3)*350
        preview=Image.open(OUT/e['file']).convert('RGBA')
        preview.thumbnail((460,260),Image.Resampling.LANCZOS)
        sheet.paste(preview,(x+(460-preview.width)//2,y),preview)
        draw.text((x,y+270),e['purpose'],fill=(30,35,35),font=smallfont)
        draw.text((x,y+295),f"{e['width']} x {e['height']}  |  1 张  |  "+('必需' if e['required'] else '可选'),fill=(85,85,80),font=smallfont)
    sheet.save(OUT/'00_素材总览_不要上传商店.png')
    print(json.dumps({'exported':len(entries),'output':str(OUT)},ensure_ascii=True))


if __name__ == '__main__':
    run()
