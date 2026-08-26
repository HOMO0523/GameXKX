const fs = require('fs');
const path = require('path');
const crypto = require('crypto');
const PptxGenJS = require('pptxgenjs');
const sharp = require('sharp');

const ROOT = path.resolve(__dirname, '..');
const OUT_DIR = path.join(ROOT, 'Deliverables', 'GameXXK_Graduation_Presentation');
const ASSET_DIR = path.join(OUT_DIR, 'assets');
const OUT_PPTX = path.join(OUT_DIR, 'GameXXK_毕业设计答辩.pptx');
const CATALOG = path.join(ROOT, 'docs', 'design', '2026-08-11-full-card-catalog.md');

const C = {
  charcoal: '24221D', paper: 'E8D9BE', paperHi: 'F3E8D2', ink: '2D2A24',
  muted: '776F63', wood: '6B4A2D', yellow: 'F2D84B', red: 'A74635',
  pine: '526E65', rule: 'B5A58D', white: 'FFFFFF', blue: '3E6F91',
};

const pptx = new PptxGenJS();
pptx.layout = 'LAYOUT_WIDE';
pptx.author = 'GameXXK';
pptx.company = 'GameXXK Graduation Project';
pptx.subject = 'GameXXK 毕业设计答辩';
pptx.title = 'GameXXK 毕业设计答辩';
pptx.lang = 'zh-CN';
pptx.theme = {
  headFontFace: 'SimSun', bodyFontFace: 'Microsoft YaHei', lang: 'zh-CN',
};
pptx.defineSlideMaster({
  title: 'PAPER',
  background: { color: C.paper },
  objects: [
    { rect: { x: 0, y: 0, w: 13.333, h: 0.12, fill: { color: C.wood }, line: { color: C.wood } } },
    { rect: { x: 0, y: 7.34, w: 13.333, h: 0.16, fill: { color: C.charcoal }, line: { color: C.charcoal } } },
  ],
  slideNumber: { x: 12.45, y: 7.08, w: 0.45, h: 0.18, fontFace: 'Microsoft YaHei', fontSize: 8, color: C.paper, align: 'right', margin: 0 },
});

const imageMeta = new Map();
async function imgMeta(file) {
  if (!imageMeta.has(file)) imageMeta.set(file, await sharp(file).metadata());
  return imageMeta.get(file);
}
async function addImage(slide, file, x, y, w, h, mode = 'contain') {
  const m = await imgMeta(file);
  const ratio = m.width / m.height;
  const boxRatio = w / h;
  let ix = x, iy = y, iw = w, ih = h;
  if (mode === 'contain') {
    if (ratio > boxRatio) { ih = w / ratio; iy = y + (h - ih) / 2; }
    else { iw = h * ratio; ix = x + (w - iw) / 2; }
  } else {
    if (ratio > boxRatio) { iw = h * ratio; ix = x - (iw - w) / 2; }
    else { ih = w / ratio; iy = y - (ih - h) / 2; }
  }
  slide.addImage({ path: file, x: ix, y: iy, w: iw, h: ih });
}
function addPaperBase(section, title, subtitle = '') {
  const slide = pptx.addSlide('PAPER');
  slide.addText(section, { x: 0.55, y: 0.34, w: 2.7, h: 0.24, fontFace: 'Microsoft YaHei', fontSize: 9, bold: true, color: C.red, charSpacing: 1.4, margin: 0 });
  slide.addText(title, { x: 0.55, y: 0.63, w: 11.9, h: 0.55, fontFace: 'SimSun', fontSize: 27, bold: true, color: C.ink, margin: 0.02, breakLine: false, fit: 'shrink' });
  if (subtitle) slide.addText(subtitle, { x: 0.58, y: 1.14, w: 11.8, h: 0.3, fontFace: 'Microsoft YaHei', fontSize: 11, color: C.muted, margin: 0, fit: 'shrink' });
  slide.addText('GameXXK 毕业设计 · 2026', { x: 0.55, y: 7.08, w: 3.0, h: 0.18, fontFace: 'Microsoft YaHei', fontSize: 8, color: C.paper, margin: 0 });
  return slide;
}
function addHighlight(slide, text, x, y, w, h, opts = {}) {
  slide.addShape(pptx.ShapeType.roundRect, { x, y, w, h, rectRadius: 0.05, fill: { color: opts.color || C.yellow, transparency: opts.transparency ?? 10 }, line: { color: opts.color || C.yellow, transparency: 100 } });
  slide.addText(text, { x: x + 0.08, y: y + 0.03, w: w - 0.16, h: h - 0.06, fontFace: 'Microsoft YaHei', fontSize: opts.fontSize || 12, bold: opts.bold !== false, color: opts.textColor || C.ink, margin: 0.02, valign: 'mid', align: opts.align || 'center', fit: 'shrink' });
}
function addBullets(slide, items, x, y, w, h, opts = {}) {
  const runs = [];
  items.forEach((item, index) => {
    runs.push({ text: item, options: { bullet: { indent: 12 }, breakLine: index < items.length - 1, hanging: 3 } });
  });
  slide.addText(runs, { x, y, w, h, fontFace: 'Microsoft YaHei', fontSize: opts.fontSize || 15, color: opts.color || C.ink, margin: 0.08, breakLine: false, paraSpaceAfterPt: 8, valign: 'top', fit: 'shrink' });
}
function addPanel(slide, x, y, w, h, title, body, accent = C.pine) {
  slide.addShape(pptx.ShapeType.roundRect, { x, y, w, h, rectRadius: 0.04, fill: { color: C.paperHi }, line: { color: C.rule, width: 0.8 } });
  slide.addShape(pptx.ShapeType.rect, { x, y, w, h: 0.08, fill: { color: accent }, line: { color: accent } });
  slide.addText(title, { x: x + 0.16, y: y + 0.16, w: w - 0.32, h: 0.32, fontFace: 'SimSun', fontSize: 17, bold: true, color: C.ink, margin: 0, fit: 'shrink' });
  slide.addText(body, { x: x + 0.16, y: y + 0.56, w: w - 0.32, h: h - 0.7, fontFace: 'Microsoft YaHei', fontSize: 11.5, color: C.ink, margin: 0, breakLine: false, valign: 'top', fit: 'shrink' });
}
function addMetric(slide, x, y, w, h, value, label, accent = C.red) {
  slide.addShape(pptx.ShapeType.roundRect, { x, y, w, h, fill: { color: C.paperHi }, line: { color: C.rule } });
  slide.addText(value, { x: x + 0.1, y: y + 0.16, w: w - 0.2, h: 0.52, fontFace: 'SimSun', fontSize: 27, bold: true, color: accent, align: 'center', margin: 0, fit: 'shrink' });
  slide.addText(label, { x: x + 0.15, y: y + 0.79, w: w - 0.3, h: h - 0.9, fontFace: 'Microsoft YaHei', fontSize: 10.5, color: C.ink, align: 'center', margin: 0, fit: 'shrink' });
}
function addArrow(slide, x1, y1, x2, y2, color = C.red) {
  slide.addShape(pptx.ShapeType.line, { x: x1, y: y1, w: x2 - x1, h: y2 - y1, line: { color, width: 2, beginArrowType: 'none', endArrowType: 'triangle' } });
}
function addImageFrame(slide, file, x, y, w, h, caption, lightFrame = false) {
  const inset = lightFrame ? 0.02 : 0.07;
  slide.addShape(pptx.ShapeType.rect, { x, y, w, h, fill: { color: lightFrame ? C.paperHi : C.charcoal }, line: { color: C.wood, width: lightFrame ? 0.6 : 1.2 } });
  return addImage(slide, file, x + inset, y + inset, w - inset * 2, h - 0.35 - inset, 'cover').then(() => {
    slide.addShape(pptx.ShapeType.rect, { x: x + inset, y: y + h - 0.34, w: w - inset * 2, h: 0.27, fill: { color: C.charcoal }, line: { color: C.charcoal } });
    slide.addText(caption, { x: x + 0.12, y: y + h - 0.3, w: w - 0.24, h: 0.2, fontFace: 'Microsoft YaHei', fontSize: 8.5, color: C.paper, margin: 0, fit: 'shrink' });
  });
}

function splitRow(line) {
  const body = line.trim().slice(1, -1);
  return body.split(/(?<!\\)\|/).map(s => s.trim().replace(/\\\|/g, '|'));
}
function parseCards() {
  const text = fs.readFileSync(CATALOG, 'utf8');
  const cards = [];
  let section = '';
  for (const line of text.split(/\r?\n/)) {
    const h = line.match(/^## \d+\.\s*(.*?)(?:（|$)/);
    if (h) { section = h[1].trim(); continue; }
    if (!/^\| \d{3} \|/.test(line)) continue;
    const c = splitRow(line);
    const effect = c[7].replace(/<br\s*\/?>/gi, '；').replace(/。\s*；/g, '；').replace(/；\s*；+/g, '；').replace(/\s+；/g, '；');
    cards.push({ number: Number(c[0]), name: c[1], id: c[2].replace(/`/g, ''), quality: c[3], cost: c[4], target: c[5], acquisition: c[6], effect, signature: c[8].replace(/^`|`$/g, ''), section });
  }
  if (cards.length !== 198) throw new Error(`目录卡牌数错误: ${cards.length}`);
  const active = cards.filter(c => !c.id.startsWith('Route.') || c.id.startsWith('Route.Boss.'));
  if (active.length !== 173) throw new Error(`现役卡牌数错误: ${active.length}`);
  const buckets = { hero: 0, partner: 0, npc: 0, boss: 0 };
  for (const c of active) {
    if (c.id.startsWith('Hero.')) buckets.hero++;
    else if (c.id.startsWith('Profession.')) buckets.partner++;
    else if (c.id.startsWith('Npc.')) buckets.npc++;
    else if (c.id.startsWith('Route.Boss.')) buckets.boss++;
  }
  if (JSON.stringify(buckets) !== JSON.stringify({ hero: 36, partner: 108, npc: 24, boss: 5 })) throw new Error(`分类错误: ${JSON.stringify(buckets)}`);
  return active;
}
function roleKey(number) {
  if (number <= 36) return 'hero';
  if (number >= 61 && number <= 78) return 'blade';
  if (number <= 96 && number >= 79) return 'guard';
  if (number <= 114 && number >= 97) return 'healer';
  if (number <= 132 && number >= 115) return 'hunter';
  if (number <= 150 && number >= 133) return 'sorcerer';
  if (number <= 168 && number >= 151) return 'formation';
  if (number >= 37 && number <= 60) return 'npc';
  if (number >= 194) return 'boss';
  throw new Error(`未分类卡牌 ${number}`);
}
const roleInfo = {
  hero: ['主角', '泛用工具与六职业联动'], blade: ['刀客', '冲锋/收招与气势/流血'], guard: ['守卫', '护甲/援护/格挡'],
  healer: ['药师', '药效/药方与生命变化'], hunter: ['弓手', '蓄力/重箭与多段'], sorcerer: ['法师', '五牌任务与元素分支'],
  formation: ['阵师', '六地势与全队收益'], npc: ['任务NPC', '六名双职业角色'], boss: ['首领牌', '黑熊2张/老虎3张'],
};
function cardDesign(card) {
  const e = card.effect;
  let kind = '机制桥接';
  if (/冲锋|收招/.test(e)) kind = '顺序编排';
  else if (/重箭|蓄力/.test(e)) kind = '蓄力生产/兑现';
  else if (/药方|药效/.test(e)) kind = '药效引擎';
  else if (/消耗.*护甲/.test(e)) kind = '护甲终结';
  else if (/检索|任务/.test(e)) kind = '检索/任务推进';
  else if (/地势|地形/.test(e)) kind = '地势联动';
  else if (/格挡|援护/.test(e)) kind = '防守反应';
  else if (/毒爆/.test(e)) kind = '状态兑现';
  else if (/抽|回复1点气力|获得\d+点内力/.test(e)) kind = '资源循环';
  else if (/全体/.test(e)) kind = '群体转折';
  else if (/攻击伤害/.test(e)) kind = '直接输出';
  else if (/恢复|护甲/.test(e)) kind = '续航/防守';
  const values = (e.match(/\d+(?:\.\d+)?%?/g) || []).slice(0, 4).join(' / ');
  return `${kind}；关键值 ${values || '按条件结算'}`;
}
function chunks(list, n) { const out = []; for (let i = 0; i < list.length; i += n) out.push(list.slice(i, i + n)); return out; }
function addCardBox(slide, card, x, y, w, h, density = 6) {
  const qualityColor = card.quality === '珍稀' ? C.yellow : card.quality === '稀有' ? C.blue : C.rule;
  const titleSize = density <= 4 ? 15 : density <= 6 ? 14 : 11.5;
  const metaSize = density <= 4 ? 8.8 : density <= 6 ? 7.4 : 6.4;
  const effectSize = density <= 4 ? 10.2 : density <= 6 ? 8.6 : 6.5;
  const noteSize = density <= 4 ? 8.2 : density <= 6 ? 7.2 : 6.1;
  slide.addShape(pptx.ShapeType.roundRect, { x, y, w, h, fill: { color: C.paperHi }, line: { color: C.rule, width: 0.8 } });
  slide.addShape(pptx.ShapeType.rect, { x, y, w, h: 0.07, fill: { color: qualityColor }, line: { color: qualityColor } });
  slide.addText(card.name, { x: x + 0.12, y: y + 0.11, w: w * 0.54, h: 0.26, fontFace: 'SimSun', fontSize: titleSize, bold: true, color: C.ink, margin: 0, fit: 'shrink' });
  slide.addText(`${card.quality}｜${card.cost}`, { x: x + w * 0.57, y: y + 0.12, w: w * 0.39 - 0.12, h: 0.22, fontFace: 'Microsoft YaHei', fontSize: metaSize, bold: true, color: C.red, align: 'right', margin: 0, fit: 'shrink' });
  slide.addText(card.target, { x: x + 0.12, y: y + 0.4, w: w - 0.24, h: 0.17, fontFace: 'Microsoft YaHei', fontSize: metaSize, color: C.muted, margin: 0, fit: 'shrink' });
  slide.addText(card.effect, { x: x + 0.12, y: y + 0.61, w: w - 0.24, h: h - 1.11, fontFace: 'Microsoft YaHei', fontSize: effectSize, color: C.ink, margin: 0, breakLine: false, valign: 'top', fit: 'shrink' });
  slide.addShape(pptx.ShapeType.rect, { x: x + 0.08, y: y + h - 0.42, w: w - 0.16, h: 0.26, fill: { color: C.yellow, transparency: 25 }, line: { color: C.yellow, transparency: 100 } });
  slide.addText(cardDesign(card), { x: x + 0.14, y: y + h - 0.39, w: w - 0.28, h: 0.2, fontFace: 'Microsoft YaHei', fontSize: noteSize, bold: true, color: C.ink, margin: 0, fit: 'shrink' });
  slide.addText(card.id, { x: x + 0.12, y: y + h - 0.13, w: w - 0.24, h: 0.1, fontFace: 'Consolas', fontSize: 5.8, color: C.muted, margin: 0, fit: 'shrink' });
}

async function buildNarrativeSlides() {
  const A = n => path.join(ASSET_DIR, n);
  // 1 cover
  let s = pptx.addSlide(); s.background = { color: C.charcoal };
  await addImage(s, A('battle.png'), 0, 0, 13.333, 7.5, 'cover');
  s.addShape(pptx.ShapeType.rect, { x: 0, y: 0, w: 13.333, h: 7.5, fill: { color: C.charcoal, transparency: 28 }, line: { transparency: 100 } });
  s.addShape(pptx.ShapeType.rect, { x: 0.65, y: 0.65, w: 0.1, h: 5.8, fill: { color: C.yellow }, line: { color: C.yellow } });
  s.addText('GameXXK', { x: 1.05, y: 1.25, w: 8.5, h: 1.0, fontFace: 'SimSun', fontSize: 51, bold: true, color: C.white, margin: 0 });
  s.addText('城镇有故事，局内有策略，桌面有成长', { x: 1.08, y: 2.42, w: 8.9, h: 0.62, fontFace: 'SimSun', fontSize: 25, bold: true, color: C.yellow, margin: 0, fit: 'shrink' });
  s.addText('毕业设计答辩｜HD2D城镇 × 队伍共享牌组 × 桌面放置刷宝', { x: 1.1, y: 3.18, w: 8.7, h: 0.4, fontFace: 'Microsoft YaHei', fontSize: 14, color: C.white, margin: 0, fit: 'shrink' });
  s.addText('2026', { x: 11.4, y: 6.58, w: 1.0, h: 0.28, fontFace: 'Microsoft YaHei', fontSize: 12, color: C.white, align: 'right', margin: 0 });
  // 2
  s = addPaperBase('01 · 迭代纠偏', '旧版工作记录，已经变成可验证的游戏系统', '保留旧版纸张与重点标记；删除早期TODO、教程链接和失效路线');
  addPanel(s, 0.65, 1.65, 5.8, 4.85, '旧版 B4_01', '天台山/大小地图作为主流程\nPaperZD与序列帧“待研究”\n开发清单、DDL、教程链接占据主要篇幅\n路线临时卡与早期叙事未区分概念/实机', C.muted);
  addPanel(s, 6.85, 1.65, 5.8, 4.85, '当前实机版', '默认2D工作台→挑战路线→全屏BattleBoard\nHD2D城镇、桌面放置、路线卡牌三大支柱\nC++驱动Slate/UMG与SafeStage坐标合同\n173张现役卡 + 分层数值测试体系', C.red);
  addHighlight(s, '从“需求清单”升级为“玩法—技术—数据—证据”的完整毕设叙事', 2.3, 6.05, 8.7, 0.42);
  // 3
  s = addPaperBase('02 · 项目定位', 'GameXXK：把“组队”直接翻译成牌组语言');
  await addImageFrame(s, A('html-cover.png'), 0.65, 1.45, 6.25, 4.9, '当前HTML介绍页：游戏定位与文档化表达');
  addBullets(s, ['单机队伍构筑RPG', '主角、永久伙伴、任务NPC共享同一副手牌', '共享气力，但卡牌保留来源、职业和所有者', '不同角色通过资源生产、条件触发与效果兑现接力'], 7.25, 1.65, 5.35, 3.8, { fontSize: 16 });
  addHighlight(s, '核心差异：不是“三个人各打一套牌”，而是“三个人共同组成一套牌”', 7.2, 5.5, 5.45, 0.7, { fontSize: 14 });
  // 4
  s = addPaperBase('03 · 核心内容', '三大主心骨共同构成长期循环');
  addPanel(s, 0.6, 1.55, 3.9, 4.75, 'HD2D城镇探索', '像素角色置于具有空间、光照和景深的城镇环境中；NPC、任务、招募与商店让角色和卡牌拥有世界来源。', C.pine);
  addPanel(s, 4.72, 1.55, 3.9, 4.75, '类《杀戮尖塔》挑战', '路线节点、敌方意图和回合制出牌构成主动挑战；GameXXK用共享队伍牌组建立自己的差异。', C.red);
  addPanel(s, 8.84, 1.55, 3.9, 4.75, '桌面放置刷宝', '挑战负责首次通关与高价值奖励；游历在已通关关卡低耗刷取，装备与资源回流下一次构筑。', C.yellow);
  addHighlight(s, '城镇提供意义｜挑战验证策略｜工作台提供持续成长', 2.2, 6.2, 9.0, 0.48);
  // 5
  s = addPaperBase('04 · 核心循环', '探索、挑战、刷宝与成长首尾相接');
  const loop = [
    ['城镇', '任务 / NPC / 招募 / 世界', C.pine], ['主动挑战', '路线 / 意图 / 出牌 / 首领', C.red],
    ['工作台成长', '刷宝 / 装备 / 编队 / 再构筑', C.yellow], ['更高难度', '新章节 / 新队伍 / 新策略', C.wood],
  ];
  loop.forEach((it, i) => { const x = 0.7 + i * 3.12; addPanel(s, x, 2.3, 2.6, 2.15, it[0], it[1], it[2]); if (i < 3) addArrow(s, x + 2.62, 3.35, x + 3.02, 3.35); });
  addHighlight(s, '当前默认演示路径：2D工作台 → 挑战路线图 → 全屏BattleBoard → 返回路线或工作台', 1.35, 5.25, 10.65, 0.62, { fontSize: 14 });
  // 6
  s = addPaperBase('05 · HD2D城镇', '青山镇让卡牌系统拥有“人、地点与目的”');
  await addImageFrame(s, A('town.png'), 0.65, 1.45, 8.0, 5.25, 'HD2D青山镇实机：像素角色、空间、光照与景深');
  addBullets(s, ['自由移动与NPC交互', '任务、跟随、商店和路线入口', '2D角色与3D场景层次融合', '作为世界与叙事支柱保留，不冒充当前默认启动界面'], 9.0, 1.75, 3.65, 4.6, { fontSize: 14 });
  // 7
  s = addPaperBase('06 · 桌面放置', '工作台既是挂机界面，也是成长中枢');
  await addImageFrame(s, A('workbench.png'), 0.55, 1.45, 8.55, 5.25, '桌面历练工作台实机：仓库、背包、角色与历练');
  addBullets(s, ['挑战解锁、游历刷取，两套状态机互不污染', '仓库、背包、装备、编队、天赋与工具统一管理', '离线/低耗收益进入持久化奖励账本', '减少重复操作，把注意力留给构筑和首次挑战'], 9.38, 1.65, 3.35, 4.75, { fontSize: 13.5 });
  // 8
  s = addPaperBase('07 · 路线挑战', '七节点挑战把风险选择放在战斗之前');
  await addImageFrame(s, A('route.png'), 0.65, 1.45, 8.0, 5.25, '当前挑战路线图：入口、分支、普通、精英与首领');
  addBullets(s, ['点击可达节点进入真实卡牌战斗', '胜利后返回路线继续选择', '首领胜利才完成整次挑战', '自动战斗只影响单场战斗，不替玩家自动连点路线'], 9.0, 1.75, 3.65, 4.5, { fontSize: 14 });
  // 9
  s = addPaperBase('08 · 局内战斗', '意图、目标、卡文和结算日志让结果可预测');
  await addImageFrame(s, A('battle.png'), 0.55, 1.42, 8.7, 5.35, '全屏BattleBoard：共享手牌、角色站位与敌方意图');
  addBullets(s, ['主角/伙伴/NPC三人共享手牌与气力', '敌方意图提前展示，目标通过箭头确认', '每个伤害包逐条写入结算日志', '退出战斗回到路线；关闭挑战再回工作台'], 9.5, 1.7, 3.15, 4.6, { fontSize: 13.5 });
  // 10
  s = addPaperBase('09 · 共享牌组', '8 + 5 + 3：角色配置直接决定局内语言');
  addMetric(s, 0.7, 1.75, 2.6, 2.0, '8', '主角配置牌\n泛用 + 六职业联动', C.red);
  addMetric(s, 3.55, 1.75, 2.6, 2.0, '5', '永久伙伴携带牌\n出生6选5', C.pine);
  addMetric(s, 6.4, 1.75, 2.6, 2.0, '3', '任务NPC携带牌\n四选三', C.wood);
  addMetric(s, 9.25, 1.75, 2.6, 2.0, '≤3', '首领卡槽\n只容纳首领牌', C.yellow);
  addHighlight(s, '共享手牌 ≠ 抹掉来源：任务、职业、套装、药效、反应与重放仍读取卡牌所有者', 1.05, 4.55, 11.25, 0.72, { fontSize: 14 });
  addBullets(s, ['自动重放、协战、反击、格挡、DoT不计主动出牌', '目标、支付与移牌保持原子性', '首领牌占用专属槽，不再进入已删除的普通路线牌体系'], 1.25, 5.45, 10.8, 1.0, { fontSize: 12.8 });
  // 11
  s = addPaperBase('10 · 卡牌语言', '所有角色共享一套资源、目标和触发语法');
  addPanel(s, 0.65, 1.5, 3.85, 2.15, '资源', '共享气力决定一回合可打几张牌；内力属于角色并限制高倍率、群体和重放。', C.yellow);
  addPanel(s, 4.74, 1.5, 3.85, 2.15, '品质', '完整效果按基础品质展示：普通×1，稀有伤害/治疗/护甲×2，珍稀×4；层数按阶数增长。', C.red);
  addPanel(s, 8.83, 1.5, 3.85, 2.15, '目标', '预览与确认使用同一目标模式；条件不满足保留基础效果，资源不足或目标失效不产生半结算。', C.pine);
  addPanel(s, 0.65, 4.05, 3.85, 2.15, '状态', '流血、中毒、灼烧、蚀伤、破绽、标记、气势、灵动、蓄力、药效都有固定触发时点。', C.red);
  addPanel(s, 4.74, 4.05, 3.85, 2.15, '顺序', '刀客关注首牌/末牌；法师记录首次出牌顺序；弓手锁定全部蓄力再结算重箭。', C.wood);
  addPanel(s, 8.83, 4.05, 3.85, 2.15, '自动效果', '免费重放不支付、不推进任务、不计主动牌，避免递归和阈值偷触发。', C.pine);
  // 12
  s = addPaperBase('11 · 角色机制地图', '六职业的强度接近，但操作体验必须不同');
  const roles = [
    ['刀客','首尾排序','冲锋/收招',C.red],['守卫','防守转攻','护甲/格挡',C.pine],['药师','生命循环','药效/药方',C.yellow],
    ['弓手','蓄力释放','重箭/多段',C.wood],['法师','顺序任务','炎冰雷',C.blue],['阵师','环境决策','六地势',C.pine],
    ['主角','队伍桥接','泛用/联动',C.red],['任务NPC','双职业','四选三',C.yellow],['首领牌','里程碑','三专属槽',C.wood],
  ];
  roles.forEach((r,i)=>{const col=i%3,row=Math.floor(i/3);addPanel(s,0.65+col*4.15,1.45+row*1.72,3.82,1.45,r[0],`${r[1]}｜${r[2]}`,r[3]);});
  // 13
  s = addPaperBase('12 · Slate/UMG', '状态—协调—界面—绘制：四层界面架构');
  await addImageFrame(s, A('html-slate.png'), 0.55, 1.45, 6.2, 5.2, 'HTML中的Slate架构说明', true);
  const arch = [['Subsystem','权威状态'],['PlayerController','生命周期/焦点'],['Workbench/Route/Battle','C++ WidgetTree'],['Slate绘制','NativePaint/Atlas']];
  arch.forEach((a,i)=>{const y=1.55+i*1.22;addPanel(s,7.1,y,4.85,0.96,a[0],a[1],i===1?C.red:C.pine);if(i<3)addArrow(s,9.52,y+0.98,9.52,y+1.16,C.red);});
  // 14
  s = addPaperBase('13 · SafeStage案例', '把“箭头看起来偏了”变成可验证的坐标合同');
  await addImageFrame(s, A('safestage.png'), 0.55, 1.45, 7.3, 5.2, '目标箭头实机验收：Board-local坐标与可见箭尖hotspot');
  addBullets(s, ['读取viewport-client本地DPI坐标', '经BattleHudSafeStage Offset/Scale进入1920×1080 stage', 'NativePaint只使用Board-local坐标', '禁止LocalToAbsolute→AbsoluteToLocal跨Geometry往返', '方向向量只参与旋转；位置由明确hotspot决定'], 8.2, 1.65, 4.45, 4.35, { fontSize: 12.8 });
  addHighlight(s, '水平 / 垂直 / 对角几何自动化 + 多窗口原点真实PIE', 8.2, 5.82, 4.35, 0.58, { fontSize: 11.5 });
  // 15
  s = addPaperBase('14 · 资源生命周期', 'HUD-only界面按需创建，而不是一次加载整个游戏UI');
  const life = [['启动','只创建桌面工作台与游历条'],['打开','按页面ensure路线、商人或BattleBoard'],['折叠','保留轻量状态，延迟释放重资源'],['恢复','先恢复纸框，再异步载入头像/Atlas'],['销毁','过期回调按token拒绝，输入上下文回滚']];
  life.forEach((a,i)=>{const x=0.55+i*2.53;addPanel(s,x,2.15,2.25,2.7,a[0],a[1],i===0?C.yellow:i===4?C.red:C.pine);if(i<4)addArrow(s,x+2.27,3.48,x+2.48,3.48);});
  addHighlight(s, '目标：让常驻桌面游历保持轻量，同时保证路线和战斗需要时完整出现', 1.65, 5.45, 10.0, 0.65, { fontSize: 14 });
  // 16
  s = addPaperBase('15 · 数值测试体系', '八层验证把“感觉不对”拆成可定位的证据');
  const layers = [['1 目录合同','ID/费用/目标/卡文'],['2 逐卡执行','198×七地势'],['3 规则集成','职业/状态/重放'],['4 模拟核心','正式解析器完整回合'],['5 锁定矩阵','2400场'],['6 正交矩阵','2520场'],['7 统计建议','Schema3/Wilson'],['8 实机门禁','冷UBT/Automation/PIE']];
  layers.forEach((a,i)=>{const col=i%4,row=Math.floor(i/4);addPanel(s,0.55+col*3.15,1.55+row*2.35,2.85,1.95,a[0],a[1],i<4?C.pine:C.red);});
  addHighlight(s, '模拟是诊断仪器，不是“最终平衡已经证明”', 3.5, 6.25, 6.3, 0.45, { fontSize: 13 });
  // 17
  s = addPaperBase('16 · 模拟对战', '2400锁定矩阵看整体；2520正交矩阵找原因');
  await addImageFrame(s, A('html-numeric-tests.png'), 0.55, 1.45, 6.15, 5.2, 'HTML中的数值测试体系', true);
  addMetric(s, 7.05, 1.55, 2.45, 1.6, '2400', '8 cohort × 3节点 × 100 seed', C.red);
  addMetric(s, 9.8, 1.55, 2.45, 1.6, '2520', '职业/套装/NPC/地势/成长档', C.pine);
  addMetric(s, 7.05, 3.45, 2.45, 1.6, '1386', '198张目录卡 × 七代码地势', C.wood);
  addMetric(s, 9.8, 3.45, 2.45, 1.6, 'Schema 3', '资源、伤害、状态、队列指标', C.yellow);
  addHighlight(s, '历史FullMatrix（2026-08-11）：2118胜 / 282负 / 0 MaxRounds；仅代表当时版本', 7.05, 5.45, 5.2, 0.72, { fontSize: 11.5 });
  // 18
  s = addPaperBase('17 · 当前卡池', '页面展示173张现役卡；回归仍保护198个目录定义');
  addMetric(s, 0.7, 1.65, 2.65, 2.1, '36', '主角\n12泛用+24联动', C.red);
  addMetric(s, 3.55, 1.65, 2.65, 2.1, '108', '六职业伙伴\n各18张', C.pine);
  addMetric(s, 6.4, 1.65, 2.65, 2.1, '24', '六名任务NPC\n各4张', C.wood);
  addMetric(s, 9.25, 1.65, 2.65, 2.1, '5', '首领牌\n黑熊2+老虎3', C.yellow);
  addHighlight(s, '173 = 36 + 108 + 24 + 5', 3.4, 4.3, 6.5, 0.68, { fontSize: 20 });
  addPanel(s, 2.1, 5.25, 9.1, 1.0, '排除边界', '25张普通/地势/稀有路线牌仍保留在代码目录用于兼容与回归，但奖励、事件、宝箱和商人已不再发放。', C.red);
  // 19
  s = addPaperBase('18 · 制作迭代', '从早期工作版到当前2D默认流程');
  const tl = [['早期','天台山/大小地图设想\n工具与资产待研究'],['MVP','青山镇→路线→战斗\n共享牌组逐步落地'],['系统化','198卡目录/装备/状态\n自动化与模拟矩阵'],['当前','2D工作台默认\n挑战路线+全屏战斗']];
  tl.forEach((a,i)=>{const x=0.65+i*3.1;addPanel(s,x,1.75,2.75,3.25,a[0],a[1],i===3?C.red:C.pine);if(i<3)addArrow(s,x+2.77,3.38,x+3.0,3.38);});
  addHighlight(s, '旧版不是被删除，而是作为“为什么要迭代”的对照证据', 2.4, 5.55, 8.5, 0.62, { fontSize: 14 });
  // 20
  s = addPaperBase('19 · 主体总结', 'GameXXK的毕设价值来自五个彼此连接的层面');
  const sum = [['世界','HD2D城镇赋予角色与卡牌来源'],['策略','共享牌组与路线挑战'],['成长','桌面放置刷宝与装备构筑'],['工程','Slate/UMG、SafeStage与生命周期'],['验证','目录合同、模拟矩阵与真实PIE']];
  sum.forEach((a,i)=>addPanel(s,0.75+(i%3)*4.1,1.55+Math.floor(i/3)*2.25,3.75,1.9,a[0],a[1],[C.pine,C.red,C.yellow,C.wood,C.blue][i]));
  addHighlight(s, '下面进入全卡附录：173张现役卡，逐张保留效果、数值与设计定位', 1.6, 6.15, 10.1, 0.5, { fontSize: 14 });
}

function buildCardSlides(cards) {
  const groups = [
    ['hero', cards.filter(c=>roleKey(c.number)==='hero')], ['blade', cards.filter(c=>roleKey(c.number)==='blade')],
    ['guard', cards.filter(c=>roleKey(c.number)==='guard')], ['healer', cards.filter(c=>roleKey(c.number)==='healer')],
    ['hunter', cards.filter(c=>roleKey(c.number)==='hunter')], ['sorcerer', cards.filter(c=>roleKey(c.number)==='sorcerer')],
    ['formation', cards.filter(c=>roleKey(c.number)==='formation')], ['npc', cards.filter(c=>roleKey(c.number)==='npc')],
    ['boss', cards.filter(c=>roleKey(c.number)==='boss')],
  ];
  for (const [key, list] of groups) {
    let pages;
    if (key === 'hero') pages = [list.slice(0,6),list.slice(6,12),list.slice(12,16),list.slice(16,20),list.slice(20,24),list.slice(24,28),list.slice(28,32),list.slice(32,36)];
    else if (['blade','guard','hunter','formation'].includes(key)) pages = [list.slice(0,9),list.slice(9,18)];
    else if (key === 'healer') pages = [list.slice(0,4),list.slice(4,9),list.slice(9,14),list.slice(14,18)];
    else if (key === 'sorcerer') pages = [list.slice(0,4),list.slice(4,9),list.slice(9,13),list.slice(13,18)];
    else if (key === 'npc') pages = [list.slice(0,4),list.slice(4,8),list.slice(8,16),list.slice(16,24)];
    else pages = [list];
    pages.forEach((page, pageIndex) => {
      const [name, desc] = roleInfo[key];
      const slide = addPaperBase('20 · 全卡附录', `${name}全卡 ${pageIndex + 1}/${pages.length}`, `${desc}｜完整效果、数值与设计定位`);
      page.forEach((card, i) => {
        const cols = page.length <= 4 ? 2 : page.length <= 6 ? 2 : 3;
        const rows = page.length <= 4 ? 2 : 3;
        const w = cols === 2 ? 5.82 : 3.84;
        const h = rows === 2 ? 2.46 : 1.68;
        const gapX = cols === 2 ? 6.13 : 4.08;
        const gapY = rows === 2 ? 2.65 : 1.88;
        const col = i % cols, row = Math.floor(i / cols);
        addCardBox(slide, card, 0.58 + col * gapX, 1.43 + row * gapY, w, h, page.length);
      });
      if (key === 'boss' && page.length < 6) addPanel(slide, 6.71, 5.19, 5.82, 1.68, '首领卡槽规则', '最多3个专属槽；重复卡与第四张奖励被拒绝。普通/地势/稀有路线牌已退出获取流程。', C.red);
    });
  }
}

function addSourceSlide(cards) {
  const s = addPaperBase('21 · 资料口径', '所有结论都有版本边界；不把历史证据冒充当前实时结果');
  addPanel(s, 0.65, 1.45, 5.9, 2.1, '主要真源', '当前实机截图与HTML交付\nFGameXXKCardCatalog生成的全卡目录\n当前目标与验收记录\nSlate/UMG源码与测试\n数值观测脚本和报告', C.pine);
  addPanel(s, 6.8, 1.45, 5.9, 2.1, '旧版的用途', 'WPS《B4_01》只作为早期格式与迭代对照。天台山、大小地图主流程、PaperZD待研究与未核对长剧情均不写成当前实机。', C.red);
  addPanel(s, 0.65, 3.9, 5.9, 2.1, '交付', 'HTML：GameXXK_Graduation_Showcase/index.html\nPPT：GameXXK_毕业设计答辩.pptx\nPDF：GameXXK_毕业设计答辩.pdf', C.yellow);
  addPanel(s, 6.8, 3.9, 5.9, 2.1, '卡牌口径', `现役 ${cards.length} 张：主角36 + 伙伴108 + NPC24 + 首领5。\n25张非首领路线牌不进入本PPT。`, C.wood);
  addHighlight(s, '结论：世界、策略、成长、工程与验证，共同构成当前GameXXK', 2.0, 6.35, 9.4, 0.48, { fontSize: 14 });
}

async function main() {
  fs.mkdirSync(OUT_DIR, { recursive: true });
  const cards = parseCards();
  await buildNarrativeSlides();
  buildCardSlides(cards);
  addSourceSlide(cards);
  if (pptx._slides.length !== 50) throw new Error(`预期50页，实际${pptx._slides.length}页`);
  await pptx.writeFile({ fileName: OUT_PPTX });
  const hash = crypto.createHash('sha256').update(fs.readFileSync(OUT_PPTX)).digest('hex');
  console.log(JSON.stringify({ ok: true, output: OUT_PPTX, slides: pptx._slides.length, activeCards: cards.length, sha256: hash }, null, 2));
}

main().catch(error => { console.error(error); process.exit(1); });
