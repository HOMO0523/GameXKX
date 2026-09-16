"""Focused frame evidence for supplemental audio briefs; no game mutation."""
import sys,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'Saved/Codex/ExtraSfxRequirements-20260915'
sys.path.insert(0,str(ROOT/'Saved/Codex/EssentialSfxRequirements-20260914/capture-deps'))
import cv2,numpy as np
from PIL import Image,ImageDraw
import build_essential_sfx_packages as base

CASES={
 'Buff':('buff01', (340,426,80,30)),
 'Critical':('critical02',(968,435,132,32)),
 'Reject':('reject01',(1020,510,105,91)),
 'PageSwitch':('switch01',(912,256,40,20)),
 'LevelUp':('level04',(780,271,50,21)),
 'ChestOpen':('reward01',(520,174,350,414)),
}

def main():
 report={}
 for cue,(name,(x,y,w,h)) in CASES.items():
  directory=base.OUT if cue=='ChestOpen' else OUT
  path=directory/(name+'-raw.mp4');cap=cv2.VideoCapture(str(path));count=int(cap.get(cv2.CAP_PROP_FRAME_COUNT));fps=cap.get(cv2.CAP_PROP_FPS)
  crops=[];f=0
  while True:
   ok,frame=cap.read()
   if not ok:break
   crops.append(frame[y:y+h,x:x+w].copy());f+=1
  cap.release()
  ref=crops[min(75,len(crops)-1)].astype(float)
  diffs=[float(np.abs(c.astype(float)-ref).mean()) for c in crops]
  steps=[float(np.abs(crops[i].astype(float)-crops[i-1].astype(float)).mean()) for i in range(1,len(crops))]
  action=json.loads((directory/(name+'-action.json')).read_text(encoding='utf-8'))
  capture=json.loads((directory/(name+'-capture.json')).read_text(encoding='utf-8'))
  action_frame=round((action['before']['wall']-capture['video_epoch'])*60)
  row={'video':str(path),'frames':count,'fps':fps,'action_frame':action_frame,'roi':[x,y,w,h],
       'largest_changes':sorted([{'frame':i+1,'delta':v} for i,v in enumerate(steps) if i+1>110],key=lambda r:-r['delta'])[:12],
       'first_change_after_action':next((i for i in range(max(0,action_frame),len(diffs)) if diffs[i]>4),None)}
  wanted=list(range(max(0,action_frame-8),min(count,action_frame+93),8))
  if cue=='PageSwitch':wanted+=list(range(action_frame+145,min(count,action_frame+165),4))
  cols=5;cw=250;ch=160;sheet=Image.new('RGB',(cols*cw,((len(wanted)+cols-1)//cols)*ch),'#17201e');draw=ImageDraw.Draw(sheet)
  for i,n in enumerate(wanted):
   pic=Image.fromarray(cv2.cvtColor(crops[n],cv2.COLOR_BGR2RGB));pic.thumbnail((cw-10,ch-33));pic=pic.resize((max(1,int(pic.width)),max(1,int(pic.height))));ox=(i%cols)*cw;oy=(i//cols)*ch
   draw.text((ox+6,oy+5),f'{cue} F{n:04d} diff {diffs[n]:.2f}',fill='white');sheet.paste(pic,(ox+6,oy+28))
  sheet.save(OUT/(cue+'-timing-grid.png'));report[cue]=row
 (OUT/'frame-analysis.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
 print(json.dumps(report,ensure_ascii=False,indent=2))

if __name__=='__main__':main()
