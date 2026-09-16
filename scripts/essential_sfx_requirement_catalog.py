"""Approved essential sound families; reference variations do not add categories."""
SPECS = [
 dict(number=1,cue="HitLight",title="轻击",clip="physical03",scene="普通物理命中",brief="短促、结实，清楚传达打中了。高频使用时保持轻量。",duration=(.12,.35),rule="普通物理伤害落点响一次；按实际命中包触发。",silent="闪避、零伤害不响；元素与护甲吸收按各自类别处理。"),
 dict(number=2,cue="HitHeavy",title="重击",clip="heavy02",scene="重物理命中",brief="比轻击更厚、更有冲击，起音集中，尾音迅速收住。",duration=(.25,.55),rule="进入重击或致命物理反馈档位时，替代轻击。",silent="普通轻击不使用；不要再叠一条相同的轻击音。"),
 dict(number=3,cue="CardPlay",title="出牌",clip="physical03",scene="卡牌提交并飞出",brief="轻巧的纸牌掠动或短促弹出感，把出牌和随后命中分开。",duration=(.10,.30),rule="卡牌成功提交、开始飞出时响一次。",silent="只选牌、悬停、取消目标、无效出牌不响。"),
 dict(number=4,cue="Block",title="格挡",clip="block01",scene="护甲吸收攻击",brief="短而钝的金属防护碰撞，突出伤害被挡住的反馈。",duration=(.15,.45),rule="物理攻击被护甲吸收或实际格挡时播放，部分吸收也使用。",silent="完全闪避不响；元素命中保持元素声音。"),
 dict(number=5,cue="Lightning",title="雷系命中",clip="lightning01",scene="引雷定标命中三名敌人",brief="干脆的电击起音，带短电弧尾音，逐个命中仍能分辨。",duration=(.30,.80),rule="雷击或雷元素的有效伤害落点播放。",silent="没有伤害的标记添加不单独响命中音；避免额外叠重击。"),
 dict(number=6,cue="Fire",title="火系命中",clip="fire02",scene="灵火点灯命中三名敌人",brief="短促的火焰爆燃，温暖、略有低频，尾焰不拖长。",duration=(.25,.65),rule="火元素实际命中时播放，群体命中按现有顺序触发。",silent="只有施法准备、未命中或零伤害时不响。"),
 dict(number=7,cue="Frost",title="冰系命中",clip="frost01",scene="护甲转化产生冰系命中",brief="冷而清晰的冰晶破裂感，控制高频尖锐度，与雷声区分。",duration=(.25,.70),rule="冰元素有效伤害落点播放。本例为护甲转化攻击。",silent="只增加护甲、恢复内力但没有冰伤害时不响冰命中音。"),
 dict(number=8,cue="Heal",title="治疗",clip="heal01",scene="归元术产生有效恢复",brief="柔和、短暂的灵力回流，明确是恢复，保持战斗节奏。",duration=(.40,.85),rule="实际治疗生效，在对应演出结束/恢复反馈时播放一次。",silent="满血无效治疗、单纯改变生命上限不响。"),
 dict(number=9,cue="Down",title="倒下",clip="down01",scene="敌人被击倒",brief="简短的倒地收束感，与致命命中衔接，不使用拖长叫声。",duration=(.20,.50),rule="角色进入倒下演出时播放一次，与致命命中属于两个反馈阶段。",silent="已经倒下的角色刷新或移除时不重播。"),
 dict(number=10,cue="Victory",title="胜利",clip="victory01",scene="击败最后敌人并显示奖励",brief="明亮、轻快的短胜利提示，让成功结算有一个清楚的落点。",duration=(.70,1.30),rule="战斗进入胜利结算时，每场只播放一次。",silent="重复刷新结果或仍有敌人存活时不重播。",pre=3),
 dict(number=11,cue="Defeat",title="失败",clip="defeat03",scene="主角倒下后失败并自动重开",brief="克制的下行提示，简短收束，给重新尝试留出空间。",duration=(.60,1.10),rule="进入败北结算时每场一次；本例主角倒下后自动重开。",silent="一般受伤、单名非关键队员倒下、重新打开界面不重播。",pre=3),
 dict(number=12,cue="Reward",title="奖励",clip="reward01",scene="开箱后装备进入背包",brief="清楚而轻快的获得感，短促落定，避免一件物品连续多响。",duration=(.15,.40),rule="奖励成功领取/入账后播放；本例开箱成功一次。",silent="只展示奖励、打开空箱或提交失败不响。",layout="side"),
 dict(number=13,cue="Button",title="按钮点击",clip="button",scene="按钮和属性选择共用点击音",brief="极短、轻、清晰的按下反馈，频繁点击也不刺耳。",duration=(.04,.12),rule="可用按钮被按下时共用一条声音。",silent="悬停与禁用按钮不响，不给每种按钮另做一条。",layout="side"),
 dict(number=15,cue="CardDraw",title="抽牌",clip="card-draw-reference",scene="回合补牌进入手牌",brief="轻短的翻纸、滑入感，表现新卡进入手牌；比出牌提交更轻。",duration=(.08,.20),rule="新卡入手动画开始时播放；单张一次，同批多张合并一次。",silent="手牌整理、重排、悬停、没有实际入手的抽牌不响；出牌使用另一类声音。",category="战斗",reference_only=True),
 dict(number=16,cue="Buff",title="护甲与增益生效",clip="buff01",scene="横剑守势：护甲由0增加到24",brief="短而柔和的聚气、撑起感，结尾轻收；提示防御或增益已生效，避免做成撞击声。",duration=(.25,.60),rule="护甲实际增加或正面状态实际获得时，在对应HUD/效果出现处播放。同一动作多项、群体生效合并一次。",silent="只出牌尚未生效、状态刷新无增加、负面状态、治疗或护甲吸收攻击不响本音。",category="战斗",reference_only=True),
 dict(number=17,cue="Reject",title="操作受阻提示",clip="reject01",scene="气力不足，出牌被拒绝",brief="短、轻、克制的下行提示，让玩家知道操作没成功，不使用刺耳警报。",duration=(.10,.30),rule="玩家实际尝试被拒绝并显示原因时播放一次；无效出牌、错误目标和不合规则的物品放置共用。",silent="悬停、置灰按钮、取消操作、后台检查和同一错误持续刷新不响；快速重复尝试限频。",category="交互",reference_only=True),
 dict(number=18,cue="ChestOpen",title="宝箱开箱",clip="reward01",scene="打开宝箱并揭示物品",brief="短促开扣、箱盖打开并轻轻落定，有打开东西的实感；不要做成长奖励乐句。",duration=(.30,.75),rule="开箱成功、箱体打开或结果首次揭示时播放一次，批量开箱合并一次。本项目现阶段对齐结果揭示；同一瞬间不再叠奖励音。",silent="查看宝箱、数量为0、开箱失败、结果页刷新不响；独立领取其他奖励仍使用奖励音。",category="结算",reference_only=True),
 dict(number=19,cue="Critical",title="暴击",clip="critical02",scene="真实天赋暴击命中",brief="比普通命中更利落、更有爆点，短起音加结实的低频收束；有力度但不盖过战斗。",duration=(.20,.50),rule="由实际暴击判定触发，在伤害落点播放。物理暴击替代本次轻击/重击；元素暴击保留元素音色，制作时预留可替换的强化版本。",silent="普通大伤害、非暴击的致命攻击、闪避、无有效伤害不使用；不把高伤害当作暴击依据。",category="战斗",reference_only=True),
 dict(number=20,cue="PageSwitch",title="换页与模式切换",clip="switch01",scene="工具模式切换",brief="轻短的翻页、滑动落定感，频繁操作不刺耳；换页和切模式共用一条。",duration=(.08,.22),rule="新页面或新模式实际显示时播放一次；该控件使用切换音替代普通按钮点击音，避免同次操作双响。",silent="重复点击当前页、悬停、切换失败、数据刷新和普通按钮不响本音。",category="交互",reference_only=True),
 dict(number=21,cue="LevelUp",title="升级",clip="level04",scene="领取经验，角色等级提升",brief="明亮而简短的上扬乐句，体现成长和达成感；比普通奖励更有层次，避免胜利曲的长度。",duration=(.60,1.20),rule="经验结算后等级实际增加，在升级提示出现时播放一次；同次连续跨级合并一次，优先于同批普通奖励声。",silent="获得经验但未升级、打开角色页、读档、属性刷新和测试直接改等级不触发。",category="结算",reference_only=True),
]


def folder(spec):
    return f"{spec['number']:02}_{spec['title']}_v001"
