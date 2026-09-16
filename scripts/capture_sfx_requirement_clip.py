"""Capture a verified in-editor 2D game rectangle and its real mixer output."""
import argparse
import json
import subprocess
import time
import sys
import threading
from pathlib import Path
import imageio_ffmpeg
from ue_mcp_client import UnrealMCPClient

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'Saved/Codex/EssentialSfxRequirements-20260914'
SCRIPT='Content/Python/gamexxk_record_sfx_requirements.py'


def main():
    global OUT
    p=argparse.ArgumentParser();p.add_argument('name');p.add_argument('--scenario',default='Outcome.Single');p.add_argument('--card',default='');p.add_argument('--seconds',type=float,default=12);p.add_argument('--ally',action='store_true');p.add_argument('--skip-prepare',action='store_true');p.add_argument('--action',default='play');p.add_argument('--id',default='600');p.add_argument('--turns',type=int,default=1);p.add_argument('--layout',choices=['battle','reward','desktop'],default='battle');p.add_argument('--output',default='');p.add_argument('--variant',default='');p.add_argument('--second-id',default='');a=p.parse_args()
    if a.output:
        OUT=(ROOT/a.output).resolve();assert OUT.is_relative_to(ROOT/'Saved/Codex');OUT.mkdir(parents=True,exist_ok=True)
    # Extra options preserve the original recording defaults and evidence paths.
    c=UnrealMCPClient();assert c.connect() and c.is_in_pie()
    def call(args):
        if a.output:args=args+['--output',a.output]
        r=c.run_project_python_file(SCRIPT,args);assert r.get('success'),r
        return json.loads(r['stdout'])
    if not a.skip_prepare:call(['prepare','--scenario',a.scenario,'--card',a.card,'--variant',a.variant,'--name',a.name])
    time.sleep(.6)
    before=call(['state']);assert before['audio']['master_volume']>0,'Activate the editor before capture'
    assert not (OUT/(a.name+'-capture.json')).exists(),'Do not overwrite a completed recording'
    sys.path.insert(0,str(ROOT/'Saved/Codex/EssentialSfxRequirements-20260914/capture-deps'))
    from windows_capture import WindowsCapture
    import numpy as np
    import cv2
    ff=imageio_ffmpeg.get_ffmpeg_exe()
    args=[ff,'-hide_banner','-y','-f','rawvideo','-pixel_format','bgr24','-video_size','1280x720','-framerate','60','-i','pipe:0','-an','-c:v','libx264','-preset','veryfast','-crf','17','-pix_fmt','yuv420p','-threads','4','-r','60','-fps_mode','cfr',str(OUT/(a.name+'-raw.mp4'))]
    log=(OUT/(a.name+'-ffmpeg.log')).open('w',encoding='utf-8')
    rec=subprocess.Popen(args,stdin=subprocess.PIPE,stdout=subprocess.DEVNULL,stderr=log)
    ready=threading.Event();state={'epoch':None,'timespan':None,'frames':0,'arrivals':0,'last':None,'error':None}
    capture=WindowsCapture(cursor_capture=False,draw_border=False,secondary_window=False,window_name='GameXXK - Unreal Editor' if a.layout=='battle' else 'GameXXKDesktopOverlay')
    @capture.event
    def on_frame_arrived(frame,control):
        try:
            now=time.time()
            if state['epoch'] is None:state.update(epoch=now,timespan=frame.timespan)
            state['arrivals']+=1
            relative=(frame.timespan-state['timespan'])/10000000.0
            target=int(relative*60)
            output=np.empty((720,1280,3),np.uint8);output[:]=[28,24,17]
            if a.layout=='battle':
                crop=frame.frame_buffer[171:941,8:1734,:3]
                assert crop.shape==(770,1726,3),crop.shape
                output[72:642]=cv2.resize(crop,(1280,570),interpolation=cv2.INTER_AREA)
            elif a.layout=='reward':
                output[174:588,520:870]=frame.frame_buffer[317:731,995:1345,:3]
                output[204:328,1010:1138]=cv2.resize(frame.frame_buffer[78:140,1341:1405,:3],(128,124))
                output[630:658,520:1120]=cv2.resize(frame.frame_buffer[229:249,390:794,:3],(600,28))
            else:
                crop=frame.frame_buffer[:,:,:3]
                scale=min(1280/crop.shape[1],570/crop.shape[0])
                width,height=round(crop.shape[1]*scale),round(crop.shape[0]*scale)
                left=(1280-width)//2;top=72+(570-height)//2
                output[top:top+height,left:left+width]=cv2.resize(crop,(width,height),interpolation=cv2.INTER_AREA)
            current=output.tobytes()
            while state['frames']<target:
                rec.stdin.write(state['last'] or current);state['frames']+=1
            if state['frames']==target:
                rec.stdin.write(current);state['frames']+=1
            state['last']=current;ready.set()
        except Exception as error:
            state['error']=repr(error);ready.set();control.stop()
    @capture.event
    def on_closed():state['error']='Target editor window closed';ready.set()
    capture_control=capture.start_free_threaded()
    audio=False
    try:
        assert ready.wait(8),'No captured editor frame';assert not state['error'],state['error']
        time.sleep(.5);assert rec.poll() is None
        start=call(['record-start','--name',a.name]);audio=True
        time.sleep(1.8)
        action=['action','--name',a.name,'--action',a.action,'--id',a.id]
        if a.ally:action.append('--ally')
        result=call(action)
        action_started=time.monotonic()
        if a.second_id:
            time.sleep(2.5)
            result={'first':result,'second':call(['action','--name',a.name+'-second','--action','desktop','--id',a.second_id])}
        for extra in range(1,a.turns):
            time.sleep(6)
            current=call(['state'])
            if 'DEFEAT' in current['phase'] or 'VICTORY' in current['phase']:break
            call(['action','--name',a.name+'-turn'+str(extra+1),'--action','end-turn'])
        time.sleep(max(1,a.seconds-1.8-(time.monotonic()-action_started)))
        after=call(['record-stop','--name',a.name]);audio=False
        time.sleep(.4)
        report={'name':a.name,'scenario':a.scenario,'card':a.card,'seconds':a.seconds,'video_epoch':state['epoch'],'capture_api':'Windows.Graphics.Capture','layout':a.layout,'before':before,'start':start,'action':result,'after':after}
        (OUT/(a.name+'-capture.json')).write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
        print(json.dumps({'name':a.name,'played':after['audio']['played'],'phase':after['phase'],'screen':after['screen']},ensure_ascii=False),flush=True)
    finally:
        if audio:call(['record-stop','--name',a.name])
        capture_control.stop()
        if rec.poll() is None:
            rec.stdin.close();rec.wait(timeout=20)
        (OUT/(a.name+'-video-clock.json')).write_text(json.dumps({k:v for k,v in state.items() if k!='last'},indent=2),encoding='utf-8')
        log.close()

if __name__=='__main__':main()
