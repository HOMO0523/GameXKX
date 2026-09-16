"""Apply the tested portable review interaction to any essential SFX family."""
import base64
import hashlib
import html
import json
import re
import zipfile
from pathlib import Path
from essential_sfx_requirement_catalog import SPECS,folder

ROOT=Path(__file__).resolve().parents[1]
DEST=ROOT/'Deliverables/音效需求'
OUT=ROOT/'Saved/Codex/EssentialSfxRequirements-20260914'


def load(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def esc(x):return html.escape(str(x),quote=True)
def embedded(p,mime,name=None):return {'name':name or p.name,'mime':mime,'base64':base64.b64encode(p.read_bytes()).decode(),'sha256':sha(p)}


def audio_card(record,key):
    badge='参考配音素材' if record.get('reference_only') else ('实录使用' if record['used_in_video'] else '同类变体参考')
    label=next((s['title'] for s in SPECS if s['cue']==record['cue']),record['cue'])
    return f'''<div class="sound-card"><div class="sound-top"><span class="sound-title">{esc(label)} · 变体{record['variant']:02}</span><span class="badge">{badge}</span></div>
<button class="filename mono" data-play-name="{key}" aria-label="试听 {esc(record['name'])}.wav">{esc(record['name'])}.wav</button>
<canvas class="waveform" id="wave_{key}" width="570" height="80" aria-label="{esc(record['name'])}波形"></canvas>
<p class="sound-meta">{record['seconds']:.3f} 秒 · {record['sample_rate']/1000:g} kHz · {record['channels']} 声道 · {record['pcm_bits']}-bit</p>
<div class="sound-actions"><button data-audition="{key}" class="small">试听</button><button data-download="{key}" class="small quiet">下载 WAV</button></div></div>'''


def replace_function(text,name,body):
    pattern=rf'^(?:async )?function {name}\([^\n]*$'
    result,count=re.subn(pattern,lambda _:body,text,flags=re.M)
    assert count==1,(name,count)
    return result


def review_html(spec,package):
    p=load(OUT/(spec['cue']+'-delivery-plan.json'));records=load(package/'来源与授权/音频清单.json');manifest=load(package/'05_文件核验.json')
    video_name='01_演示_参考配音.mp4' if spec.get('reference_only') else '01_演示_现有效果.mp4'
    assets={'video':embedded(package/video_name,'video/mp4'),
            'silentVideo':embedded(package/'02_演示_无声版.mp4','video/mp4'),
            'sheet':embedded(package/'03_时间轴需求.xlsx','application/vnd.openxmlformats-officedocument.spreadsheetml.sheet')}
    keys={};primary=[];accompanied=[];reference_keys=[];source_buttons=[];seen_packs=set()
    for i,r in enumerate(records):
        key=f'ref_{i}';keys[r['name']+'.wav']=key;reference_keys.append(key);assets[key]=embedded(package/r['file'],'audio/wav')
        (primary if r['primary'] else accompanied).append(audio_card(r,key))
        source_key=f'original_{i}';path=package/r['source_file'];assets[source_key]=embedded(path,'audio/ogg' if path.suffix=='.ogg' else 'audio/wav')
        source_buttons.append(f'<p>{esc(r["name"])} · {esc(r["author"])} · {esc(r["license"])}<br><a href="{esc(r["source"])}" target="_blank" rel="noopener noreferrer">作者发布页 ↗</a> <button class="small quiet" data-download="{source_key}">原始素材</button></p>')
        if r['pack'] not in seen_packs:
            seen_packs.add(r['pack']);directory=package/'来源与授权'/r['pack']
            license_file=directory/'LICENSE-1.txt'
            if not license_file.exists():license_file=directory/'source-manifest.json'
            lk=f'license_{len(seen_packs)}';assets[lk]=embedded(license_file,'text/plain;charset=utf-8',r['pack']+'_'+license_file.name)
            source_buttons.append(f'<p><button class="small quiet" data-download="{lk}">{esc(r["pack"])} 授权记录</button></p>')
    targets=[e for e in p['events'] if e['primary']]
    scenes=[]
    for i,e in enumerate(targets):
        section=p['sections'][e['section']]
        label=e.get('label') or (spec['scene'] if len(targets)==1 else (['按下洗炼按钮','选择原属性'][i] if spec['cue']=='Button' else f"{spec['title']} · 第{i+1}次触发"))
        scenes.append({'title':label,'note':e['file'],'start':max(section['offset'],e['target_frame']/60-1),
                       'end':min(section['offset']+section['duration'],e['target_frame']/60+2),
                       'targetFrame':e['target_frame'],'tool':True,'countLabel':spec['title']+' × 1'})
    accomp=[{'key':keys[e['file']],'seconds':e['seconds'],'gain':.339 if e['cue']=='Button' else .282} for e in p['events'] if not e['primary']]
    payload={'schema':2,'pageVersion':'2.0','package':folder(spec),'shortTitle':spec['title'],'primaryCue':spec['cue'],
             'durationMax':spec['duration'][1],'primaryGain':.339 if spec['cue']=='Button' else .282,
             'video':manifest['video'],'assets':assets,'referenceKeys':reference_keys,'soundEvents':p['events'],
             'accompaniment':accomp,'scenes':scenes,'review':None}
    text=(ROOT/'scripts/templates/tool-sfx-review.html').read_text(encoding='utf-8')
    text=text.replace('工具操作 · 音效需求',esc(spec['title'])+' · 音效需求').replace('Sound brief 14','Sound brief '+str(spec['number'])).replace('25 秒 · 60 fps · 5 段操作 · 1 条共用工具音',f'{p["seconds"]} 秒 · 60 fps · {len(targets)} 个落点 · {len(primary)} 条本类参考')
    begin=text.index('      <section class="panel">');end=text.index('      <section class="panel">',begin+10)
    more_primary=f'<details><summary>更多本类参考（{len(primary)-1}条）</summary>'+''.join(primary[1:])+'</details>' if len(primary)>1 else ''
    other_sounds=f'<details><summary>随片其他音效（{len(accompanied)}条）</summary>'+''.join(accompanied)+'</details>' if accompanied else ''
    panel=f'''      <section class="panel"><div class="panel-header"><h2>声音要求与参考</h2><span class="badge">{len(primary)} 个文件</span></div>
<p class="brief" style="margin-bottom:12px">{esc(spec['brief'])}<br><strong>建议 {spec['duration'][0]}–{spec['duration'][1]} 秒 · 单次</strong></p>
{primary[0]}{more_primary}
{other_sounds}
<details><summary>触发规则与交付</summary><div class="brief"><p>{esc(spec['rule'])}</p><p>不触发：{esc(spec['silent'])}</p><p>先交付1条主版本，现有变体供参考。建议命名 SFX_{spec['cue']}_v02.wav，48kHz、单声道、24-bit PCM。</p></div></details>
<details><summary>素材来源与下载</summary><p>参考音频使用已记录的CC0素材；音频授权不覆盖游戏画面或新导入的成品。</p>{''.join(source_buttons)}<p><button data-download="silentVideo" class="small quiet">无声视频</button> <button data-download="video" class="small quiet">现有效果视频</button></p></details></section>
'''
    text=text[:begin]+panel+text[end:]
    if not accomp:text=text.replace('style="font-size:12px;margin-top:6px"','style="font-size:12px;margin-top:6px;display:none"')
    replacements={
      '工具操作演示视频':spec['title']+'演示视频',
      '一条声音自动用于三个完成落点':f'一条声音自动用于本页{len(targets)}个落点',
      '新工具音波形':'新音效波形','试配时保留按钮声':'试配时保留随片其他声音',
      '点条目定位结果 · 点“播放”回看整段':'点条目定位起音 · 点“播放”查看前后',
      'FPS=60, END=25, LAST=1499':'FPS=payload.video.fps, END=payload.video.seconds, LAST=payload.video.frame_count-1',
      'Math.min(4,':'Math.min(scenes.length-1,',
      'BASE_TOOL_GAIN=.282, BASE_BUTTON_GAIN=.339':'BASE_TOOL_GAIN=payload.primaryGain, BASE_BUTTON_GAIN=1',
      "tail.textContent='/ 25.000'":"tail.textContent='/ '+END.toFixed(3)",
      'if(!video.paused)chooseScene(sceneAt(Math.min(mediaTime,24.99)))':'if(!video.paused&&activeScope===null)chooseScene(sceneAt(Math.min(mediaTime,END-.01)))',
      "?'新工具音 + 按钮参考'":"?'新音效 + 随片参考'",
      "decoded.duration>.55":"decoded.duration>payload.durationMax",
      '超过建议的 0.55 秒':'超过建议的 ${payload.durationMax} 秒',
      "'已导入，将自动用于三个工具完成落点。'":"'已导入，将用于本页 '+scenes.length+' 个音效落点。'",
      '`工具操作_评审_${version}.html`':'`${payload.shortTitle}_评审_${version}.html`',
      "tag.textContent=scene.tool?'工具音 × 1':'只有按钮声'":"tag.textContent=scene.countLabel||'本类音效 × 1'",
      "run.textContent='播放'":"run.textContent='播放'",
      "String(scene.start).padStart(2,'0')":"Number(scene.start).toFixed(3)",
      "String(scene.end).padStart(2,'0')":"Number(scene.end).toFixed(3)",
      "async function init(){renderScenes();":"async function init(){$('seek').max=String(LAST);renderScenes();",
    }
    for before,after in replacements.items():
        assert before in text,before
        text=text.replace(before,after)
    text=replace_function(text,'sceneAt',"function sceneAt(t){let index=0;scenes.forEach((s,i)=>{if(s.targetFrame/FPS<=t)index=i;});return index;}")
    text=replace_function(text,'decodeReferences',"""function decodeReferences(){if(decodePromise)return decodePromise;decodePromise=(async()=>{const context=audioContext();for(const key of payload.referenceKeys){references[key]=await context.decodeAudioData(bytesFrom64(assets[key].base64).buffer);const canvas=$('wave_'+key);if(canvas)paintWave(canvas,references[key]);}buttonStem=context.createBuffer(2,Math.ceil(END*context.sampleRate),context.sampleRate);for(const event of payload.accompaniment){const source=references[event.key],offset=Math.round(event.seconds*context.sampleRate);for(let ch=0;ch<2;ch++){const out=buttonStem.getChannelData(ch),input=source.getChannelData(Math.min(ch,source.numberOfChannels-1));for(let i=0;i<input.length&&offset+i<out.length;i++)if(offset+i>=0)out[offset+i]+=input[i]*event.gain;}}})();return decodePromise;}""")
    text=replace_function(text,'stopAudition',"""function stopAudition(){if(audition){const old=audition;audition=null;try{old.source.stop();}catch{}old.source.disconnect();old.gain.disconnect();}document.querySelectorAll('[data-audition]').forEach(button=>button.textContent=button.dataset.audition==='candidate'?'试听新音效':'试听');}""")
    # Replace the sample-specific instructions, preserving the tested file-import
    # and self-export code. No network dependency is introduced.
    details_begin=text.index('        <details><summary>使用说明与时间约定</summary>')
    details_end=text.index('</details>',details_begin)+len('</details>')
    text=text[:details_begin]+'''        <details><summary>使用说明与时间约定</summary><ul><li>点击文件名试听；导入新WAV后选择“新音效试配”。本类声音按标注帧播放，可保留随片其他声音。</li><li>F0000起算，区间含起不含止。“−1帧 / +1帧”用于查看动作。实际音轨起音来自波形匹配，建议落点取最近视频帧并结合画面核对。</li><li>输出为60fps，实录可能含重复帧。修改视频速度、剪接或帧率后需要重新标注。</li><li>调节只影响网页试听，不修改WAV。刷新或关闭前导出，才能保存新音频和备注。</li><li>单独发送本HTML即可离线打开；来源网站链接需联网，其他功能不需联网。</li></ul></details>'''+text[details_end:]
    # HTML labels visible before the script initializes also reflect this brief.
    text=text.replace('00.000 s <span>/ 25.000</span> · F0000',f'00.000 s <span>/ {p["seconds"]:.3f}</span> · F0000')
    if spec.get('reference_only'):
        text=text.replace('>原效果</button>','>参考配音</button>').replace('游戏实录声音','参考配音 · 待接入')
        text=text.replace('<span class="badge">需求 v001</span>','<span class="badge">新增 · 待接入</span>')
        timing_note=spec.get('timing_note','落点按实录中的动作或反馈画面逐帧标注；本页声音是新增参考配音，尚未接入游戏。')
        if spec['cue']=='Reject':timing_note='此项没有独立拒绝动画，按实际出牌被拒绝的操作时刻映射到视频帧；画面中的“需求标注”为后期字幕。声音为新增参考配音，尚未接入游戏。'
        text=text.replace('实际音轨起音来自波形匹配，建议落点取最近视频帧并结合画面核对。',timing_note)
        text=text.replace('现有效果视频</button>','参考配音视频</button>')
        text=text.replace('新音频、版本和备注会一起装进导出的评审 HTML。',f'本页仅为{esc(spec["title"])}配音，其他声音已静音。新音频、版本和备注会一起导出。')
    packed=json.dumps(payload,ensure_ascii=False,separators=(',',':')).replace('<','\\u003c')
    assert text.count('__REVIEW_PAYLOAD__')==1
    return text.replace('__REVIEW_PAYLOAD__',packed)


def complete_package(spec):
    package=DEST/folder(spec);target=package/'00_音效需求.html';target.write_text(review_html(spec,package),encoding='utf-8')
    manifest=load(package/'05_文件核验.json');manifest['web_review']={'file':target.name,'standalone':True,'page_version':'2.0'}
    manifest['files']=[{'file':p.relative_to(package).as_posix(),'bytes':p.stat().st_size,'sha256':sha(p)} for p in sorted(package.rglob('*')) if p.is_file() and p.name!='05_文件核验.json']
    (package/'05_文件核验.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
    with zipfile.ZipFile(package.with_suffix('.zip'),'w',zipfile.ZIP_DEFLATED) as z:
        for p in sorted(package.rglob('*')):
            if p.is_file():z.write(p,p.relative_to(DEST))
    return {'cue':spec['cue'],'html_bytes':target.stat().st_size,'status':'html_ready'}


if __name__=='__main__':
    import argparse
    parser=argparse.ArgumentParser();parser.add_argument('--cue',default='');args=parser.parse_args()
    for spec in SPECS:
        if args.cue and spec['cue']!=args.cue:continue
        print(json.dumps(complete_package(spec),ensure_ascii=False),flush=True)
