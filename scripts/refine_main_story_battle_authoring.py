"""One-time scoped source update for the four approved battle illustrations."""
import ast
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
UPDATES={
 'S00-04': {
  'enemies':['Enemy.Ch1.Rooster','Enemy.Ch1.Weasel'],
  'pre':[
   ('narrator','松根石阶尽头，一位山客气喘吁吁地跑来，怀里空得很有说服力。'),
   ('mountain_man','少侠，前头石滩有两位不识字的书友，把我的手稿围住了！'),
   ('hero','书友？'),
   ('mountain_man','一只公鸡，一只黄鼠狼。它们看书不用眼，用嘴。'),
   ('you_bai','本座听着，像是你的著作终于有了读者。'),
   ('mountain_man','手稿背面包过芝麻饼！那是半辈子的路，不是半辈子的饼！'),
   ('hero','你留在安全处指路。我去把手稿取回来，读者也劝退。'),
   ('you_bai','先备好家伙再动身。江湖第一课，别让祖传地图输给蘸油手稿。')],
  'post':[
   ('narrator','兽群终于退开。你用竹竿挑起散页，山客张开布袋接住，饼屑比掌声落得还响。'),
   ('mountain_man','稿在，人也在。那两块饼就当稿费了。'),
   ('hero','下回另找张油纸，别让读者吃到路线结局。'),
   ('you_bai','本座负责照路，不负责给芝麻提亮。'),
   ('mountain_man','国清寺还存着旧路记录。你们带图去对照，比听我这张饿嘴靠谱。'),
   ('hero','好。先补图，再补午饭。')]},
 'S01-07': {
  'enemies':['Enemy.Ch1.Weasel','Enemy.Ch1.Rooster','Enemy.Ch1.Civet'],
  'pre':[
   ('narrator','松林深处响起短哨，接着是一声比哨还急的喊。'),
   ('hunter','这边！脚扭了！我的午饭把半座山的胃都叫来了！'),
   ('hero','你求援还发用餐请柬？'),
   ('hunter','黄鼠狼、公鸡、狸猫，已经凑齐一桌。就差我当桌！'),
   ('you_bai','你先靠稳石头，别举着干粮和它们比谁站得高。'),
   ('hunter','手记也滑到水边了。人、笔记，能救就救；饼排最后！'),
   ('hero','次序总算对了。待在原地，我们从稳当的山径过去。'),
   ('you_bai','先整备再进山。救人之前，别添一位需要救的。')],
  'post':[
   ('narrator','山兽退回松影。你横竿护住湿处，挑回手记，扶猎人挪到干燥的石边。'),
   ('hunter','我这条命和这本记，今天都算你捞回来的。'),
   ('hero','饼呢？'),
   ('hunter','不提饼。人与饼，得有一个肯先长记性。'),
   ('you_bai','很好，行家终于从嘴硬练成了脚稳。'),
   ('hunter','我认得暗溪入口。等喘匀气，能肯定的、不能肯定的，都给你说清。')]},
 'S03-10': {
  'summary':'一头灰狼循肉味堵路，脚夫却惦记着你的图袋。前路要守，行李也要守；金贵决定把烟杆收好，先用眼睛盯人。',
  'enemies':['Enemy.Ch2.GrayWolf'],
  'pre':[
   ('narrator','林口散着新鲜肉干，灰狼的影子在树后晃。脚夫一边催你赶路，一边盯着图袋。'),
   ('porter','怕什么？你去探路，行李交我就是。'),
   ('jin_gui','你那袋肉自己裂了口，倒惦记起别人完好的袋子。'),
   ('hero','先不进。狼在前面，手在后面，这条路排得太热闹。'),
   ('you_bai','本座数过：一头狼，外加一个需要重点观察的脚夫。'),
   ('jin_gui','图袋我看着。你把前路清出来，别分心追人。'),
   ('hero','好。我们一起进，谁也别擅自换位。'),
   ('porter','诸位这防贼的架势，实在伤和气。')],
  'post':[
   ('narrator','灰狼退去。方才你横竿护住前路，金贵已抱稳图袋；脚夫伸手抓空，趁乱钻进了林子。'),
   ('jin_gui','图在。烟杆也在腰里，没兼职挑担。'),
   ('hero','人和图先点齐，不拿命追他那点胆量。'),
   ('you_bai','点齐了。本座一团，二位两人，行李未减；和气倒是少了一个。'),
   ('jin_gui','他跑得急，痕迹不会太干净。歇口气，再看脚下。')]},
 'S05-06': {
  'enemies':['Enemy.Ch2.GrayWolf','Enemy.Ch2.Porcupine'],
  'pre':[
   ('qiong_yao_er','前边窄道堵了，一头灰狼，一只豪猪。稳石在左，别往碎石上冲。'),
   ('hero','今日英雄救美，先保证英雄不滑成笑话。'),
   ('you_bai','本座先把这句话记下。真滑了，好知道笑哪一句。'),
   ('qiong_yao_er','我看住落脚点。等你把前路清开，我们再相互接应。'),
   ('hero','好，先备齐再上。英雄称号晚点领，路得先走稳。')],
  'post':[
   ('narrator','兽群退入山影。你撑竿站稳，琼幺儿扶了一下你的肩，把你接过松动的石沿。'),
   ('qiong_yao_er','好了。人都站住了，哪位英雄刚才差点把包留在原地？'),
   ('hero','包是诱敌之计。人不是，幸亏你接得快。'),
   ('you_bai','本座记明白了：今日互相救美，双方都有出力。'),
   ('qiong_yao_er','那就谁也别抢功。前路还长，互相搭把手。')]},
}

def literal_lines(lines):
 return '[\n'+''.join('        '+repr(tuple(line))+',\n' for line in lines)+'    ]'

def main():
 for node_id,data in UPDATES.items():
  path=ROOT/'scripts'/('main_story_'+node_id[:3].lower()+'.py')
  raw=path.read_bytes();source=raw.decode('utf-8');lines=raw.splitlines(keepends=True)
  def span(node):return sum(map(len,lines[:node.lineno-1]))+node.col_offset,sum(map(len,lines[:node.end_lineno-1]))+node.end_col_offset
  call=next(n for n in ast.walk(ast.parse(source)) if isinstance(n,ast.Call) and isinstance(n.func,ast.Name)
   and n.func.id=='n' and n.args and isinstance(n.args[0],ast.Constant) and n.args[0].value==node_id)
  if any(k.arg=='after_battle' for k in call.keywords):continue
  edits=[(*span(call.args[4]),literal_lines(data['pre']).encode('utf-8'))]
  if 'summary' in data:edits.append((*span(call.args[2]),repr(data['summary']).encode('utf-8')))
  end=span(call)[1]-1
  edits.append((end,end,(',\n    enemies='+repr(data['enemies'])+',\n    after_battle='+literal_lines(data['post'])).encode('utf-8')))
  for start,end,value in sorted(edits,reverse=True):raw=raw[:start]+value+raw[end:]
  ast.parse(raw.decode('utf-8'));path.write_bytes(raw)
  print(node_id,len(data['pre']),len(data['post']),data['enemies'])
if __name__=='__main__':main()
