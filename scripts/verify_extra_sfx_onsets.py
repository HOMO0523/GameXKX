"""Adjacent-frame crops around exact proposed cue boundaries."""
import sys,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Saved/Codex/ExtraSfxRequirements-20260915'
sys.path.insert(0,str(ROOT/'Saved/Codex/EssentialSfxRequirements-20260914/capture-deps'))
import cv2
from PIL import Image,ImageDraw
checks={
 'Buff':('buff01',[146,147,148],(332,417,104,42)),
 'Critical':('critical02',list(range(160,176)),(932,341,253,142)),
 'PageSwitch':('switch01',[154,155,156,306,307,308],(906,249,92,47)),
 'LevelUp':('level04',[160,161,162],(777,265,61,32)),
 'ChestOpen':('reward01',[157,158,159],(520,174,145,120))}
for cue,(name,frames,roi) in checks.items():
 directory=ROOT/'Saved/Codex/EssentialSfxRequirements-20260914' if cue=='ChestOpen' else OUT
 cap=cv2.VideoCapture(str(directory/(name+'-raw.mp4')));cols=min(4,len(frames));cw,ch=320,220;sheet=Image.new('RGB',(cw*cols,ch*((len(frames)+cols-1)//cols)),'#17201e');draw=ImageDraw.Draw(sheet)
 for i,f in enumerate(frames):
  cap.set(cv2.CAP_PROP_POS_FRAMES,f);ok,frame=cap.read();assert ok
  x,y,w,h=roi;pic=Image.fromarray(cv2.cvtColor(frame[y:y+h,x:x+w],cv2.COLOR_BGR2RGB));pic=pic.resize((w*2,h*2));pic.thumbnail((cw-12,ch-30))
  ox=i%cols*cw;oy=i//cols*ch;draw.text((ox+6,oy+6),f'{cue} F{f:04}',fill='white');sheet.paste(pic,(ox+6,oy+29))
 cap.release();sheet.save(OUT/(cue+'-adjacent.png'))
cap=json.loads((OUT/'reject01-capture.json').read_text());action=json.loads((OUT/'reject01-action.json').read_text())
print(json.dumps({'reject_action_before_frame':(action['before']['wall']-cap['video_epoch'])*60,'reject_action_after_frame':(action['after']['wall']-cap['video_epoch'])*60}))
