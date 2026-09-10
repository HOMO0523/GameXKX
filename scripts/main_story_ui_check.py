"""Drive observed Slate refs in the isolated desktop story verification session."""
import argparse
import atexit
import base64
import json
from pathlib import Path
import re
import struct
import time
from ue_mcp_client import UnrealMCPClient

ROOT=Path(__file__).resolve().parents[1]
TOOLSET='SlateInspectorToolset.SlateInspectorToolset'
parser=argparse.ArgumentParser()
parser.add_argument('action', choices=['snapshot','click','capture','probe','start-pie','close','continue'])
parser.add_argument('value',nargs='?',default='')
parser.add_argument('--window',default='GameXXKDesktopOverlay')
parser.add_argument('--native-input',action='store_true')
parser.add_argument('--mouse-button',choices=['left','right'],default='left')
parser.add_argument('--port',type=int,default=18765)
parser.add_argument('--native-coordinate-scale',type=float,default=1.0,
                    help='Explicit OS-screen calibration after comparing Slate and desktop captures')
args=parser.parse_args()
if args.mouse_button=='right' and not args.native_input:
    parser.error('Right-click verification requires --native-input')
client=UnrealMCPClient(timeout=60,port=args.port)
assert client.connect()
if args.action=='start-pie':
    client.start_pie(warmup_seconds=2,play_mode='PlayMode_InEditorFloating')
    print(client.run_project_python_file('Content/Python/gamexxk_main_story_probe.py',['open-backpack'])['stdout'])
elif args.action=='probe':
    values=json.loads(args.value) if args.value else []
    raw=client.run_project_python_file('Content/Python/gamexxk_main_story_probe.py',values)['stdout']
    data=json.loads(raw.strip().splitlines()[-1])
    (ROOT/'Saved/StorySystem/live-probe.json').write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf-8')
    if 'story' in data:
        data['story']['nodes']=[n for n in data['story']['nodes'] if n['state']>0]
    data.pop('widgets',None)
    print(json.dumps(data,ensure_ascii=False))
else:
    roots=client.call_tool('Snapshot',{'ref':'','maxDepth':0},toolset_name=TOOLSET)
    candidates=[line for line in roots.splitlines() if line.startswith('window ') and args.window in line]
    assert len(candidates)==1, roots
    ref=re.search(r'\[ref=([^\]]+)\]',candidates[0]).group(1)
    observer=client.call_tool('Observe',{'ref':ref,'maxDepth':40},toolset_name=TOOLSET)
    def release_observer():
        try:client.call_tool('Unobserve',{'identifier':observer},toolset_name=TOOLSET)
        except Exception:pass
    atexit.register(release_observer)
    if args.action=='capture':
        result=client.call_tool('Screenshot',{'ref':ref},toolset_name=TOOLSET)
        name=args.value or 'story-current'
        assert re.fullmatch(r'[a-zA-Z0-9_-]+',name)
        path=ROOT/'Saved/StorySystem'/f'{name}.png'
        path.write_bytes(base64.b64decode(result['data']))
        print(str(path))
    else:
        snapshot=client.call_tool('Snapshot',{'ref':ref,'maxDepth':40},toolset_name=TOOLSET)
        (ROOT/'Saved/StorySystem/slate-last.txt').write_text(snapshot,encoding='utf-8')
        if args.action=='snapshot':
            print('\n'.join(line for line in snapshot.splitlines() if 'size=0,0' not in line and any(word in line for word in ('button','generic','text ','scrollable'))))
        else:
            matches=[]
            for line in snapshot.splitlines():
                if '[disabled]' in line or 'size=0,0' in line:continue
                if args.action=='close':
                    blank=re.search(r'^\s*button \[.*\[ref=([^\]]+)\]',line)
                    if blank:matches.append(('close',blank.group(1)))
                    continue
                if args.action=='continue':
                    prompt=re.search(r'^\s*text "点击 / 空格继续.*\[ref=([^\]]+)\]',line)
                    if prompt:matches.append(('continue',prompt.group(1)))
                    continue
                match=re.search(r'^\s*(?:generic|button) "((?:\\.|[^"\\])*)".*\[ref=([^\]]+)\]',line)
                if not match:continue
                label=json.loads('"'+match.group(1)+'"')
                if label==args.value or label.startswith(args.value+'\n') or label.startswith(args.value+'\r\n'):
                    matches.append((label,match.group(2)))
            assert len(matches)==1, f'Expected one observed active widget for {args.value!r}; got {matches}'
            label,target=matches[0]
            if args.native_input:
                import gamexxk_real_play_flow_mcp as native
                native.PREVIEW_WINDOW_TITLE_PATTERN=re.compile('^'+re.escape(args.window)+r'(?:\s|$)')
                input_controller=native.PreviewWindowController()
                # Keep Win32 window rectangles and real cursor coordinates in
                # the same physical-pixel space on a scaled desktop.
                input_controller.user32.SetProcessDPIAware()
                window=input_controller.find_preview_window()
                target_line=next(line for line in snapshot.splitlines() if f'[ref={target}]' in line)
                px,py=map(float,re.search(r'pos=([\d.-]+),([\d.-]+)',target_line).groups())
                capture=client.call_tool('Screenshot',{'ref':target},toolset_name=TOOLSET)
                width,height=struct.unpack('>II',base64.b64decode(capture['data'])[16:24])
                sx,sy,sw,sh=map(float,re.search(r'pos=([\d.-]+),([\d.-]+) size=([\d.-]+),([\d.-]+)',candidates[0]).groups())
                left,top,right,bottom=window['rect']
                # Child positions are already absolute screen pixels, while
                # the root window's reported size is Slate units. Rescaling
                # by the window-size ratio would apply desktop DPI twice.
                assert 0.5 <= args.native_coordinate_scale <= 3.0
                x=round(left+(px-sx+width/2)*args.native_coordinate_scale)
                y=round(top+(py-sy+height/2)*args.native_coordinate_scale)
                from ctypes import wintypes
                input_controller.focus(window)
                input_controller.user32.SetCursorPos(x,y)
                time.sleep(.1)
                hit=int(input_controller.user32.WindowFromPoint(wintypes.POINT(x,y)) or 0)
                assert hit==window['hwnd'], f'Observed control is covered by another native window: target={window["hwnd"]}, hit={hit}'
                if args.mouse_button=='right':
                    input_controller.user32.mouse_event(0x0008,0,0,0,0)
                    time.sleep(.05)
                    input_controller.user32.mouse_event(0x0010,0,0,0,0)
                    clicked={'x':x,'y':y,'button':'right'}
                else:
                    clicked=input_controller.click_absolute_point(window,x,y)
            else:
                clicked=client.call_tool('Click',{'ref':target},toolset_name=TOOLSET)
            print(json.dumps({'label':label,'ref':target,'clicked':clicked},ensure_ascii=False))
            time.sleep(.2)
