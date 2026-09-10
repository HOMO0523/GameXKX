#include "Guide/GameXXKAcademyRules.h"
#include "GameXXKCardCatalog.h"
#include "UI/GameXXKLocalization.h"

namespace
{
	using G=EGameXXKAcademyGoal;
	FGameXXKAcademyGoalDefinition Goal(G Kind,const TCHAR* Text,int32 Count=1) {return {Kind,Count,FText::FromString(Text)};}
	FGameXXKAcademyLesson Lesson(const TCHAR* Title,const TCHAR* Text,const TCHAR* Prefix,
		std::initializer_list<const TCHAR*> Cards,std::initializer_list<FGameXXKAcademyGoalDefinition> Goals)
	{
		FGameXXKAcademyLesson L;L.Title=FText::FromString(Title);L.Instruction=FText::FromString(Text);
		for(const TCHAR* Card:Cards)L.Cards.Add(FName(*(FString(Prefix)+Card)));
		for(const auto& Item:Goals)L.Goals.Add(Item);return L;
	}
	TArray<FGameXXKAcademyCourse> BuildCourses()
	{
		TArray<FGameXXKAcademyCourse> C;
		auto Add=[&](const TCHAR* Id,const TCHAR* Title,const TCHAR* Summary,EGameXXKCharacterRole Role,const TCHAR* Npc,std::initializer_list<FGameXXKAcademyLesson> Lessons)
		{
			FGameXXKAcademyCourse Course;Course.Id=Id;Course.Title=FText::FromString(Title);Course.Summary=FText::FromString(Summary);Course.Role=Role;Course.NpcId=Npc;
			for(const auto& L:Lessons)Course.Lessons.Add(L);C.Add(MoveTemp(Course));
		};
		Add(TEXT("Academy.Basic"),TEXT("初入江湖"),TEXT("认识资源、意图与回合，完成基本战斗操作。"),EGameXXKCharacterRole::Hero,TEXT(""),{
			Lesson(TEXT("出牌与回合"),TEXT("气力由队伍共享，内力由出牌角色分别消耗。选择手牌，再选择合法目标。完成出牌后，点击结束回合。"),TEXT("Hero.Generic."),{TEXT("QingFengYiShi"),TEXT("HeYuZhan"),TEXT("SuiYanJi"),TEXT("PoYunYiShan"),TEXT("GuiYuanShu"),TEXT("NingShenTuNa"),TEXT("GuanXi"),TEXT("HengJianShouShi")},{Goal(G::ActiveCards,TEXT("主动打出两张不同的牌"),2),Goal(G::EndRound,TEXT("结束一次回合")),Goal(G::Damage,TEXT("对敌人造成实际伤害"))}),
			Lesson(TEXT("观察与防护"),TEXT("先查看敌方意图，再为队伍提供防护。护甲能够抵挡伤害；气血与内力是不同的资源。"),TEXT("Hero.Generic."),{TEXT("QingFengYiShi"),TEXT("HeYuZhan"),TEXT("SuiYanJi"),TEXT("PoYunYiShan"),TEXT("GuiYuanShu"),TEXT("NingShenTuNa"),TEXT("GuanXi"),TEXT("HengJianShouShi")},{Goal(G::Armor,TEXT("获得一次护甲")),Goal(G::EndRound,TEXT("让敌方完成一次行动")),Goal(G::Damage,TEXT("完成反击阶段的进攻"))})});
		Add(TEXT("Academy.Blade"),TEXT("刀客·首尾成招"),TEXT("安排首张与末张主动牌，使队友参与连招。"),EGameXXKCharacterRole::Blade,TEXT(""),{
			Lesson(TEXT("冲锋与收招"),TEXT("先打出具有冲锋效果的刀客牌，再接续其他主动牌。将有收招效果的牌留在最后，结束回合后观察结算。"),TEXT("Profession.Blade."),{TEXT("LieFengZhan"),TEXT("HuiFengJiaShi"),TEXT("FengHou"),TEXT("JiYuLianZhan"),TEXT("YinXueDao")},{Goal(G::BladeOpening,TEXT("以刀客牌触发一次首牌连动")),Goal(G::BladeFinish,TEXT("以刀客牌触发一次收招"))}),
			Lesson(TEXT("气势与破绽"),TEXT("先建立气势或敌方破绽，再使用进攻牌。不要只比较基础伤害，应同时观察当前状态。"),TEXT("Profession.Blade."),{TEXT("LieFengZhan"),TEXT("DuanYue"),TEXT("PoJun"),TEXT("ZhanYiFeiTeng"),TEXT("ZhanJin")},{Goal(G::ActiveCards,TEXT("使用三张不同的刀客牌"),3),Goal(G::Damage,TEXT("完成实际伤害结算"))}),
			Lesson(TEXT("借势与归鞘"),TEXT("安排反应与下一次出牌的配合。最后一张主动牌决定收招，免费重放不会代替主动出牌。"),TEXT("Profession.Blade."),{TEXT("HuiFengJiaShi"),TEXT("JieShiHuiFeng"),TEXT("ZhuYing"),TEXT("LianXiGuiQiao"),TEXT("BaoDaoShouYe")},{Goal(G::Reaction,TEXT("触发一次反应")),Goal(G::BladeFinish,TEXT("完成一次收招"))})});
		Add(TEXT("Academy.Guard"),TEXT("守卫·护友反制"),TEXT("区分护甲、格挡与援护，学习以防护创造进攻机会。"),EGameXXKCharacterRole::Guard,TEXT(""),{
			Lesson(TEXT("护甲与格挡"),TEXT("先获得护甲和格挡次数，再结束回合。护甲与格挡不是同一个资源，必须观察敌方攻击后的实际反应。"),TEXT("Profession.Guard."),{TEXT("TieBi"),TEXT("HuZhu"),TEXT("GuShou"),TEXT("FanZhenJia"),TEXT("YuanHuBu")},{Goal(G::Armor,TEXT("获得护甲")),Goal(G::Reaction,TEXT("触发一次格挡、反击或援护"))}),
			Lesson(TEXT("护甲转为伤害"),TEXT("先保留一定护甲，再使用能够利用护甲的攻击牌。比较使用前后的护甲与伤害变化。"),TEXT("Profession.Guard."),{TEXT("TieBi"),TEXT("ZhenDun"),TEXT("QinWangDunJi"),TEXT("DunZhenTuiJin"),TEXT("SuiJiaHuiJi")},{Goal(G::Armor,TEXT("建立护甲")),Goal(G::ArmorDamage,TEXT("用护甲相关效果造成伤害"))})});
		Add(TEXT("Academy.Healer"),TEXT("药师·药理相生"),TEXT("开启药方，完成实际救治，再学习药效进攻。"),EGameXXKCharacterRole::Healer,TEXT(""),{
			Lesson(TEXT("开方与救治"),TEXT("首次主动打出药师牌可开启对应药方。先治疗受伤友方，再查看药方图标和药效变化。"),TEXT("Profession.Healer."),{TEXT("YaoYin"),TEXT("XingQiZhen"),TEXT("CaoMuFuZhi"),TEXT("QingXinSan"),TEXT("LingZhiXuMing")},{Goal(G::Formula,TEXT("开启药方")),Goal(G::Healing,TEXT("实际恢复友方气血")),Goal(G::Medicine,TEXT("获得药效"))}),
			Lesson(TEXT("毒与药的取舍"),TEXT("先给敌人施加持续伤害状态，再使用毒爆。根据目标是敌人还是友方，确认药师牌将产生的效果。"),TEXT("Profession.Healer."),{TEXT("YaoYin"),TEXT("BaiCaoDu"),TEXT("FuGuSan"),TEXT("HuiQiXiang"),TEXT("LianQiaoJieDu")},{Goal(G::ToxicExplosion,TEXT("完成一次有效毒爆")),Goal(G::ActiveCards,TEXT("主动使用三张不同的药师牌"),3)})});
		Add(TEXT("Academy.Hunter"),TEXT("弓手·蓄势穿杨"),TEXT("积累蓄力并用于重箭，认识资源消耗带来的追加效果。"),EGameXXKCharacterRole::Hunter,TEXT(""),{
			Lesson(TEXT("蓄力与重箭"),TEXT("先积累蓄力，再使用带有重箭效果的攻击牌。观察蓄力消耗和追加攻击，不要只看牌面的基础伤害。"),TEXT("Profession.Hunter."),{TEXT("FuBu"),TEXT("YingYan"),TEXT("LieWang"),TEXT("ChuanYang"),TEXT("ShouHun")},{Goal(G::Charge,TEXT("获得蓄力")),Goal(G::HeavyArrow,TEXT("消耗蓄力触发重箭"))}),
			Lesson(TEXT("状态与连射"),TEXT("利用流血、中毒或破绽加强进攻，再选择合适的重箭。保留资源，避免在不合适的时机消耗全部蓄力。"),TEXT("Profession.Hunter."),{TEXT("FuBu"),TEXT("XunXiJian"),TEXT("FuZuShi"),TEXT("FuYeXianJing"),TEXT("YingLuo")},{Goal(G::ActiveCards,TEXT("使用三张不同的弓手牌"),3),Goal(G::HeavyArrow,TEXT("完成一次重箭联动"))})});
		Add(TEXT("Academy.Sorcerer"),TEXT("法师·五法成阵"),TEXT("主动完成五张不同的牌，按顺序重放并结算首牌阵赏。"),EGameXXKCharacterRole::Sorcerer,TEXT(""),{
			Lesson(TEXT("雷系任务"),TEXT("先选择希望获得阵赏的首牌，再主动打出其余四张不同的法师牌。标记决定雷击次数；重放不计作新的主动出牌。"),TEXT("Profession.Sorcerer."),{TEXT("RanLingHuanYuan"),TEXT("ChiXiaoFenXing"),TEXT("FenTianJue"),TEXT("NingYanChengRen"),TEXT("YanMuHuTi")},{Goal(G::SpellTask,TEXT("完成五牌任务并触发重放")),Goal(G::ActiveCards,TEXT("主动打出五张不同的法师牌"),5)}),
			Lesson(TEXT("通用编序"),TEXT("通用牌也能参与任务。请查看每张牌的编序说明，安排首次出牌顺序，再完成本组任务。"),TEXT("Profession.Sorcerer."),{TEXT("LingHuoFu"),TEXT("YanMuHuTi"),TEXT("LieFu"),TEXT("XingHuoHuiShou"),TEXT("ChiYanFengJie")},{Goal(G::SpellTask,TEXT("完成一轮通用任务")),Goal(G::ActiveCards,TEXT("主动使用五张不同的法师牌"),5)})});
		Add(TEXT("Academy.Formation"),TEXT("阵师·因地布阵"),TEXT("读取地势、实际换场，并利用地势收益协助队伍。"),EGameXXKCharacterRole::FormationMaster,TEXT(""),{
			Lesson(TEXT("换场与收益"),TEXT("先查看当前地势，再切换到另一种地势。重复选择同一地势不算换场；随后使用能够触发地势收益的牌。"),TEXT("Profession.FormationMaster."),{TEXT("GuanShi"),TEXT("DingZhen"),TEXT("YinShuiHuiYuan"),TEXT("LinYingMiZong"),TEXT("JieShanWeiZhang")},{Goal(G::TerrainChange,TEXT("实际切换一次地势")),Goal(G::TerrainBenefit,TEXT("触发地势收益"))}),
			Lesson(TEXT("布置与应变"),TEXT("根据敌方意图选择进攻或防护地势，再利用增加收益次数的效果。地势收益与卡牌重放是不同的结算。"),TEXT("Profession.FormationMaster."),{TEXT("GuanShi"),TEXT("CunZhaiYuanZhen"),TEXT("HuiShengZhenSha"),TEXT("YiWeiZhen"),TEXT("WanXiangGuiZhen")},{Goal(G::TerrainBenefit,TEXT("触发三次地势收益"),3),Goal(G::ActiveCards,TEXT("使用三张不同的阵师牌"),3)})});
		Add(TEXT("Academy.Npc.Tusi"),TEXT("土司首领·护友合击"),TEXT("辨认协作对象，利用护甲、反应和合击支援队伍。"),EGameXXKCharacterRole::Blade,TEXT("Npc.TusiChief"),{
			Lesson(TEXT("号令与守势"),TEXT("查看最高攻击友方是谁，再使用寨主号令。为受攻击的友方提供护甲和反应，随后让敌人行动。"),TEXT("Npc.TusiChief."),{TEXT("ZhaiZhuHaoLing"),TEXT("ShiMenShouShi"),TEXT("TuSiJunLing")},{Goal(G::ActiveCards,TEXT("主动使用两张不同的土司首领牌"),2),Goal(G::Reaction,TEXT("完成一次防护反应"))}),
			Lesson(TEXT("队伍合击"),TEXT("安排盟寨誓约与其他牌的先后顺序，观察由哪位友方执行攻击。"),TEXT("Npc.TusiChief."),{TEXT("ZhaiZhuHaoLing"),TEXT("TuSiJunLing"),TEXT("MengZhaiShiYue")},{Goal(G::ActiveCards,TEXT("主动使用三张土司首领牌"),3),Goal(G::Damage,TEXT("造成实际伤害"))})});
		Add(TEXT("Academy.Npc.Song"),TEXT("宋金宝·借势连招"),TEXT("运用支援与减费，完成三牌任务。"),EGameXXKCharacterRole::Blade,TEXT("Npc.SongJinBao"),{
			Lesson(TEXT("支援与成约"),TEXT("先选择首牌阵赏，再主动使用携带的三张不同宋金宝牌。注意减费的持续范围，免费重放不额外支付气力。"),TEXT("Npc.SongJinBao."),{TEXT("ShangQianGuWu"),TEXT("ErMuMiBao"),TEXT("YiNuoQianJin")},{Goal(G::SpellTask,TEXT("完成三牌任务")),Goal(G::ActiveCards,TEXT("主动使用三张不同的宋金宝牌"),3)})});
		Add(TEXT("Academy.Npc.YueBai"),TEXT("幽白·残卷成阵"),TEXT("规划气力，将地势收益与三牌任务结合。"),EGameXXKCharacterRole::FormationMaster,TEXT("Npc.YueBai"),{
			Lesson(TEXT("气力与三牌任务"),TEXT("幽白四张牌的基础气力消耗均为1。先预算主动出牌费用，再利用检索完成三张不同的任务牌。"),TEXT("Npc.YueBai."),{TEXT("QingYanDianDeng"),TEXT("CanJuanPiZhu"),TEXT("ShanHeCanTu")},{Goal(G::SpellTask,TEXT("完成幽白三牌任务")),Goal(G::TerrainBenefit,TEXT("触发地势收益"))}),
			Lesson(TEXT("幽白照夜"),TEXT("以幽白照夜作为首张任务牌。完成三牌任务后，观察标记与逐次落雷的阵赏。"),TEXT("Npc.YueBai."),{TEXT("YueBaiZhaoYe"),TEXT("CanJuanPiZhu"),TEXT("ShanHeCanTu")},{Goal(G::SpellTask,TEXT("完成三牌任务并结算阵赏")),Goal(G::ActiveCards,TEXT("主动使用三张不同的幽白牌"),3)})});
		Add(TEXT("Academy.Npc.Zhou"),TEXT("周光祖·药地相济"),TEXT("根据队伍血线，选择救治、地势收益或毒爆。"),EGameXXKCharacterRole::FormationMaster,TEXT("Npc.ZhouGuangZu"),{
			Lesson(TEXT("药效与救治"),TEXT("选择受伤友方完成有效治疗，再使用地志摹图触发地势收益。请注意敌我目标对应的不同效果。"),TEXT("Npc.ZhouGuangZu."),{TEXT("YiCaoBianShi"),TEXT("HuangShanFuZhi"),TEXT("DiZhiMoTu")},{Goal(G::Healing,TEXT("实际恢复友方气血")),Goal(G::TerrainBenefit,TEXT("触发地势收益"))}),
			Lesson(TEXT("岩粉进攻"),TEXT("先确认敌人的持续伤害状态，再使用岩粉封脉。保留必要的治疗能力。"),TEXT("Npc.ZhouGuangZu."),{TEXT("YiCaoBianShi"),TEXT("DiZhiMoTu"),TEXT("YanFenFengMai")},{Goal(G::ToxicExplosion,TEXT("完成有效毒爆")),Goal(G::ActiveCards,TEXT("使用两张不同的周光祖牌"),2)})});
		Add(TEXT("Academy.Npc.JinGui"),TEXT("金贵·蓄势周旋"),TEXT("为合适的队友提供蓄力和防护。"),EGameXXKCharacterRole::Hunter,TEXT("Npc.JinGui"),{
			Lesson(TEXT("借用队友蓄力"),TEXT("先确认最高攻击友方，再使用市井耳目或杂役筹备。重箭读取的是牌面指定角色的蓄力。"),TEXT("Npc.JinGui."),{TEXT("ShiJingErMu"),TEXT("QiaoYanZhouXuan"),TEXT("ZaYiChouBei")},{Goal(G::Charge,TEXT("为友方增加蓄力")),Goal(G::HeavyArrow,TEXT("触发一次蓄力联动"))}),
			Lesson(TEXT("周旋与防护"),TEXT("使用巧言周旋保护合适的友方，再结束回合观察反应。后巷脱身提供全队灵动，不能把它理解为无条件免伤。"),TEXT("Npc.JinGui."),{TEXT("ShiJingErMu"),TEXT("QiaoYanZhouXuan"),TEXT("HouXiangTuoShen")},{Goal(G::Reaction,TEXT("触发一次防护反应")),Goal(G::ActiveCards,TEXT("使用两张不同的金贵牌"),2)})});
		Add(TEXT("Academy.Npc.Qiong"),TEXT("琼幺儿·蛊箭济人"),TEXT("利用蓄力和持续伤害进攻，同时照顾队伍血线。"),EGameXXKCharacterRole::Hunter,TEXT("Npc.QiongMeiEr"),{
			Lesson(TEXT("蛊箭与救治"),TEXT("给敌人施加持续伤害状态并触发毒爆，再为受伤友方提供治疗。"),TEXT("Npc.QiongMeiEr."),{TEXT("TengQiaoFeiDu"),TEXT("GuWuMiZong"),TEXT("YinLingZhenXin")},{Goal(G::ToxicExplosion,TEXT("完成一次有效毒爆")),Goal(G::Healing,TEXT("实际治疗友方"))}),
			Lesson(TEXT("队伍支援"),TEXT("利用藤桥飞渡帮助队友，再用山歌唤灵恢复队伍气血。先观察血线，再决定出牌顺序。"),TEXT("Npc.QiongMeiEr."),{TEXT("TengQiaoFeiDu"),TEXT("YinLingZhenXin"),TEXT("ShanGeHuanLing")},{Goal(G::Healing,TEXT("完成有效治疗")),Goal(G::Charge,TEXT("为队友增加蓄力")),Goal(G::ActiveCards,TEXT("主动使用三张不同的琼幺儿牌"),3)})});
		if(auto* Mage=C.FindByPredicate([](const auto& Course){return Course.Id==FName(TEXT("Academy.Sorcerer"));}))
		{
			for(auto Family:{EGameXXKSorcererCardFamily::Fire,EGameXXKSorcererCardFamily::Ice})
			{
				FGameXXKAcademyLesson Extra;
				Extra.Title=FText::FromString(Family==EGameXXKSorcererCardFamily::Fire?TEXT("火系任务"):TEXT("冰系任务"));
				Extra.Instruction=FText::FromString(TEXT("先使用本系牌确定阵赏，再主动完成五张不同的法师牌。观察首牌对应的阵赏与重放顺序。"));
				const auto Cards=FGameXXKCardCatalog::GetCardDefinitionsForOwner(TEXT("Profession.Sorcerer"));
				for(const auto& Card:Cards)if(Card.SorcererRule.Family==Family && Extra.Cards.Num()<5)Extra.Cards.Add(Card.Id);
				for(const auto& Card:Cards)if(Card.SorcererRule.Family==EGameXXKSorcererCardFamily::Core && Extra.Cards.Num()<5)Extra.Cards.AddUnique(Card.Id);
				Extra.Goals={Goal(G::SpellTask,TEXT("完成本系法术任务并触发重放"))};
				Mage->Lessons.Add(MoveTemp(Extra));
			}
		}
        if(auto* Hero=C.FindByPredicate([](const auto& Course){return Course.Id==FName(TEXT("Academy.Basic"));}))
        {
            FGameXXKAcademyLesson Extra;
            Extra.Cards={TEXT("Hero.Mage.YanXuLiaoYuan"),TEXT("Hero.Mage.HanXuNingChuan"),TEXT("Hero.Mage.LeiXuYinTing"),TEXT("Hero.Mage.GuiXuTongXuan"),
                TEXT("Hero.Generic.QingFengYiShi"),TEXT("Hero.Generic.GuiYuanShu"),TEXT("Hero.Generic.NingShenTuNa"),TEXT("Hero.Generic.HengJianShouShi")};
            Extra.Goals={Goal(G::SpellTask,TEXT("完成主角四牌任务")),Goal(G::ActiveCards,TEXT("使用不同主动牌"),4)};
            Hero->Lessons.Add(MoveTemp(Extra));
        }
        // Existing lesson indices and rewards stay stable; presentation uses the
        // reviewed, concise bilingual teaching copy instead of raw catalog prose.
        const TCHAR* GoalNames[]={TEXT("ActiveCards"),TEXT("EndRound"),TEXT("Damage"),TEXT("Armor"),TEXT("Healing"),TEXT("Cleanse"),TEXT("Medicine"),TEXT("Formula"),TEXT("Reaction"),TEXT("ArmorDamage"),TEXT("Charge"),TEXT("HeavyArrow"),TEXT("TerrainChange"),TEXT("TerrainBenefit"),TEXT("SpellTask"),TEXT("BladeOpening"),TEXT("BladeFinish"),TEXT("ToxicExplosion")};
        for(auto& Course:C)
        {
            const FString Prefix=Course.Id.ToString();
            Course.Title=GameXXKLocalization::Text(*(Prefix+TEXT(".Name")));
            Course.Summary=GameXXKLocalization::Text(*(Prefix+TEXT(".Summary")));
            for(int32 Index=0;Index<Course.Lessons.Num();++Index)
            {
                auto& Item=Course.Lessons[Index];const FString Key=Prefix+FString::Printf(TEXT(".Lesson.%d"),Index);
                Item.Title=GameXXKLocalization::Text(*(Key+TEXT(".Title")));
                Item.Instruction=GameXXKLocalization::Text(*(Key+TEXT(".Instruction")));
                for(auto& Objective:Item.Goals)
                    Objective.Text=GameXXKLocalization::Text(*(FString(TEXT("Academy.Goal."))+GoalNames[static_cast<int32>(Objective.Kind)]));
            }
        }
		return C;
	}
}

const TArray<FGameXXKAcademyCourse>& FGameXXKAcademyRules::Courses()
{
	static const auto Result=BuildCourses();return Result;
}
