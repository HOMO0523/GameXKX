"""Index, aggregate workbook and portable archive for all fourteen audio briefs."""
from pathlib import Path
import hashlib
import html
import json
import urllib.parse
import zipfile
from openpyxl import Workbook,load_workbook
from openpyxl.styles import Font,PatternFill,Alignment,Border,Side
from openpyxl.utils import get_column_letter
from essential_sfx_requirement_catalog import SPECS,folder

ROOT=Path(__file__).resolve().parents[1]
DEST=ROOT/'Deliverables/音效需求'


def load(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def esc(x):return html.escape(str(x),quote=True)


def main():
    entries=[]
    all_specs=SPECS+[dict(number=14,cue='Tool',title='工具操作',scene='强化、合成与洗炼选择',brief='短、清楚、有完成感；各工具共用。',duration=(.25,.55),rule='操作成功、采用新属性时播放；整批只响一次。',silent='仅预览、保留原属性、材料不足不响。')]
    for spec in sorted(all_specs,key=lambda s:s['number']):
        package=DEST/folder(spec);manifest=load(package/'05_文件核验.json');records=load(package/'来源与授权/音频清单.json')
        page=package/'00_音效需求.html';assert page.is_file()
        for r in manifest['files']:assert sha(package/r['file'])==r['sha256'],str(package/r['file'])
        primary=[r for r in records if r['cue']==spec['cue']]
        item={**spec,'folder':folder(spec),'html':page.relative_to(DEST).as_posix(),'html_bytes':page.stat().st_size,'html_sha256':sha(page),
              'seconds':manifest['video']['seconds'],'frame_count':manifest['video']['frame_count'],
              'primary_files':[r['name']+'.wav' for r in primary],
              'primary_events':len([e for e in manifest['sound_events'] if e['cue']==spec['cue']])}
        entries.append(item)
    count=len(entries)
    reference_count=len(set(n for e in entries for n in e['primary_files']))
    integrated=[e for e in entries if not e.get('reference_only')]
    integrated_reference_count=len(set(n for e in integrated for n in e['primary_files']))
    book=Workbook();sheet=book.active;sheet.title=f'{count}类需求总表'
    sheet.append(['编号','音效类别','触发规则','不触发','制作方向','建议时长秒','参考文件','演示秒数','网页入口','状态'])
    for e in entries:
        status='新增需求；参考配音，游戏待接入' if e.get('reference_only') else '需求包已完成；新音效待制作/调整'
        sheet.append([e['number'],e['title'],e['rule'],e['silent'],e['brief'],f"{e['duration'][0]}–{e['duration'][1]}",'\n'.join(e['primary_files']),e['seconds'],e['html'],status])
        sheet.cell(sheet.max_row,9).hyperlink=e['html']
    widths=[9,18,58,57,55,18,38,15,54,39]
    for i,w in enumerate(widths,1):sheet.column_dimensions[get_column_letter(i)].width=w
    for row in sheet:
        sheet.row_dimensions[row[0].row].height=68 if row[0].row>1 else 32
        for c in row:
            c.alignment=Alignment(wrap_text=True,vertical='center');c.font=Font(name='Microsoft YaHei',size=11,color='273630');c.border=Border(bottom=Side(style='hair',color='DBE4DF'))
            if c.row==1:c.fill=PatternFill('solid',fgColor='263C32');c.font=Font(name='Microsoft YaHei',size=11,bold=True,color='FFFFFF')
            elif c.row%2==0:c.fill=PatternFill('solid',fgColor='F0F5F2')
    sheet.freeze_panes='C2';sheet.auto_filter.ref=sheet.dimensions;sheet.sheet_view.showGridLines=False;sheet.page_setup.orientation='landscape';sheet.page_setup.fitToWidth=1;sheet.page_setup.fitToHeight=0
    path=DEST/'00_音效需求总表.xlsx';book.save(path);assert load_workbook(path,read_only=True).active.max_row==count+1
    rows=[]
    for e in entries:
        href=urllib.parse.quote(e['html']);download=f"{e['number']:02}_{e['title']}_音效需求.html"
        group=e.get('category') or ('战斗' if e['number']<=9 else '结算' if e['number']<=12 else '交互')
        status='<small style="color:#e2bd88">新增 · 参考配音 / 待接入</small>' if e.get('reference_only') else ''
        rows.append(f'''<tr><td><span class="number">{e['number']:02}</span></td><td><strong>{esc(e['title'])}</strong><small>{esc(e['scene'])}</small>{status}</td><td class="muted">{group}</td><td class="mono">{e['seconds']} 秒<small>{e['primary_events']} 个本类落点</small></td><td>{len(e['primary_files'])} 条<small>{e['html_bytes']/1024/1024:.1f} MB / HTML</small></td><td class="actions"><a class="open" href="{href}" target="_blank">打开需求 ↗</a><a href="{href}" download="{esc(download)}">保存单项 HTML</a></td></tr>''')
    content='''<!doctype html><html lang="zh-CN"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>14类音效需求总览 · GameXXK</title><style>
:root{color-scheme:dark}*{box-sizing:border-box}body{margin:0;background:#11181c;color:#eef2eb;font:14px/1.6 "Microsoft YaHei UI",system-ui,sans-serif}.shell{max-width:1260px;margin:auto;padding:34px 28px}.brand{font-size:12px;letter-spacing:.15em;color:#bce59d}h1{font-size:30px;margin:8px 0 10px}p{color:#acbbb4;margin:0}.head{display:flex;justify-content:space-between;align-items:end;gap:25px;margin-bottom:28px}.tag{font-size:12px;border:1px solid #3b4c44;color:#bce59d;border-radius:30px;padding:5px 13px;white-space:nowrap}a{color:#bce59d;text-decoration:none}a:hover{text-decoration:underline}a:focus-visible{outline:2px solid #bce59d;outline-offset:5px}.toolbar{display:flex;justify-content:space-between;gap:20px;align-items:center;padding:15px 0;border-top:1px solid #34433c;border-bottom:1px solid #34433c;font-size:12px}table{border-collapse:collapse;width:100%;table-layout:auto}th{text-align:left;font-weight:400;color:#9cacA3;font-size:12px;padding:14px 12px}td{border-top:1px solid #2c3a34;padding:14px 12px;vertical-align:middle}tr:hover td{background:#192721}strong{font-size:16px;font-weight:600;display:block}small{font-size:12px;color:#a3b4a9;display:block;margin-top:2px}.number,.mono{font-family:Consolas,monospace;font-variant-numeric:tabular-nums}.number{color:#89a496}.muted{color:#a3b4a9}.actions{white-space:nowrap;text-align:right}.actions a{display:inline-block;padding:7px 12px;font-size:12px}.actions .open{background:#bce59d;color:#1a2b20;border-radius:7px;font-weight:600;margin-right:8px}.foot{border-top:1px solid #34433c;margin-top:20px;padding-top:17px;display:flex;justify-content:space-between;gap:24px;font-size:12px}.foot p{max-width:730px}@media(max-width:760px){.shell{padding:24px 14px}.head{display:block}.tag{display:inline-block;margin-top:14px}h1{font-size:25px}.toolbar{align-items:start}.toolbar p{max-width:240px}th:nth-child(3),td:nth-child(3),th:nth-child(5),td:nth-child(5){display:none}td,th{padding:13px 6px}td:nth-child(1){width:25px}.actions{white-space:normal}.actions a{display:block;padding:6px;font-size:11px}.actions .open{margin-right:0;margin-bottom:4px}strong{font-size:14px}small{font-size:11px}.foot{display:block}.foot a{display:inline-block;margin-top:12px}}</style></head><body><main class="shell"><header class="head"><div><div class="brand">GAMEXXK / SOUND BRIEFS</div><h1>必要音效 · 需求总览</h1><p>打开一类，查看视频与发声点；导入新WAV即可对照试听、导出评审文件。</p></div><span class="tag">14 / 14 需求包齐全</span></header><div class="toolbar"><p>整套发送完整压缩包；单项点击“保存HTML”即可独立发送。</p><a href="00_%E9%9F%B3%E6%95%88%E9%9C%80%E6%B1%82%E6%80%BB%E8%A1%A8.xlsx" download>下载14类需求总表 ↓</a></div><table><thead><tr><th>编号</th><th>音效</th><th>类别</th><th>演示</th><th>参考</th><th style="text-align:right">查看 / 发送</th></tr></thead><tbody>'''+''.join(rows)+'''</tbody></table><footer class="foot"><p>参考声音沿用当前接入素材。每个需求HTML内嵌视频、参考音频与表格，可离线打开；新成品与备注需用页面内“导出评审HTML”保存。</p><a href="README_交付说明.md">查看文件说明 ↗</a></footer></main></body></html>'''
    content=content.replace('14类',f'{count}类').replace('14 / 14',f'{count} / {count}').replace('参考声音沿用当前接入素材。','参考声音包含现有接入素材与新增需求参考，新增项已标明待接入。')
    (DEST/'index.html').write_text(content,encoding='utf-8')
    readme='''# 14类必要音效需求

双击 `index.html` 打开总览。选择一类即可查看演示视频、秒/帧需求、参考音频、原始素材和授权记录。

- 单项交付：从总览“保存单项HTML”下载，或发送该目录内`00_音效需求.html`。每个HTML独立包含所需视频、声音和表格。
- 整套交付：发送`音效需求_14类_完整包.zip`，解压后保持目录结构，打开`index.html`。
- 成品交回：音效人员在对应网页中导入新WAV，试听后填版本和备注，点击“导出评审HTML”，把导出的文件发回即可。
- 浏览器刷新不会自动保存新音频，关闭前应导出。网页试配和导出均不会修改游戏里的音效文件。

总表在`00_音效需求总表.xlsx`。每类目录另有有声/无声视频、详细需求表、简版时间轴、参考WAV、原始源音频及来源授权。当前库14类共22条参考WAV；按钮、工具各共用一条。

这些是制作/调整需求包；新音效尚未制作或导入，最终听感需人工听审。参考文件的CC0记录只覆盖相应音频，不覆盖本项目画面或后来导入的成品。
'''
    readme=readme.replace('14类',f'{count}类').replace('当前库'+str(count)+'类共22条参考WAV；按钮、工具各共用一条。',f'需求共{count}类、{reference_count}条去重参考WAV。当前接入试用库仍为{len(integrated)}类{integrated_reference_count}条，新增{count-len(integrated)}类为参考配音、尚未接入；按钮、工具、换页与模式切换各自共用一条。')
    (DEST/'README_交付说明.md').write_text(readme,encoding='utf-8')
    record={'packages':entries,'count':count,'unique_reference_wavs':reference_count,'integrated_families':len(integrated),'reference_only_families':count-len(integrated),'total_video_seconds':sum(e['seconds'] for e in entries)}
    (DEST/'目录清单.json').write_text(json.dumps(record,ensure_ascii=False,indent=2),encoding='utf-8')
    archive_path=DEST/f'音效需求_{count}类_完整包.zip'
    with zipfile.ZipFile(archive_path,'w',zipfile.ZIP_DEFLATED) as z:
        for filename in ['index.html','README_交付说明.md','00_音效需求总表.xlsx','目录清单.json']:z.write(DEST/filename,filename)
        for e in entries:
            for p in sorted((DEST/e['folder']).rglob('*')):
                if p.is_file():z.write(p,p.relative_to(DEST))
    with zipfile.ZipFile(archive_path) as z:assert z.testzip() is None
    print(json.dumps({'packages':count,'unique_reference_wavs':record['unique_reference_wavs'],'video_seconds':record['total_video_seconds'],'zip_bytes':archive_path.stat().st_size,'index':str(DEST/'index.html')},ensure_ascii=False),flush=True)

if __name__=='__main__':main()
