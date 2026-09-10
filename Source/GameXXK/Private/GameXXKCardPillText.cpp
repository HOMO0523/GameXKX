#include "GameXXKCardPillText.h"
#include "UI/GameXXKLocalization.h"

namespace
{
	struct FPill
	{
		const TCHAR* Id;
		const TCHAR* Name;
		const TCHAR* Description;
		bool bInline = true;
	};

	const FPill Pills[] = {
		{TEXT("Opener"), TEXT("冲锋"), TEXT("本回合第一张主动牌触发。")},
		{TEXT("Finisher"), TEXT("收招"), TEXT("结束回合时，最后一张主动牌触发。")},
		{TEXT("HeavyArrow"), TEXT("重箭"), TEXT("消耗全部蓄力，按消耗量强化本牌。")},
		{TEXT("Sequence"), TEXT("编序"), TEXT("按本次任务的记录顺序改变效果。")},
		{TEXT("SpellTask"), TEXT("法术任务"), TEXT("本组不同法术各主动打出1次，依序免费重放；每回合限1次。")},
		{TEXT("FormationReward"), TEXT("阵赏"), TEXT("任务重放后，领取首牌奖励。")},
		{TEXT("Universal"), TEXT("通用"), TEXT("作首牌时，第2张记录牌决定分支；核心或通用牌对应普通分支。"), false},
		{TEXT("AutoDraw"), TEXT("自动入手"), TEXT("从抽牌堆或弃牌堆自动加入手牌。")},
		{TEXT("Prescription"), TEXT("药方"), TEXT("首次打出开启本场效果；药方互不触发。")},
		{TEXT("Search"), TEXT("检索"), TEXT("从抽牌堆或弃牌堆选牌入手。")},
		{TEXT("PoisonBurst"), TEXT("毒爆"), TEXT("依次触发流血、中毒、灼烧和蚀伤各一次。")},
		{TEXT("IceBurst"), TEXT("冰爆"), TEXT("消耗全部护甲攻击全体敌方；每点护甲增加1个攻击百分点。")},
		{TEXT("HealingReversal"), TEXT("治疗反转"), TEXT("对敌方使用时，将治疗转为等量生命损失。")},
		{TEXT("IgnoreDefense"), TEXT("无视防御"), TEXT("本次攻击忽略指定数值的防御。")},
		{TEXT("Exhaust"), TEXT("消耗"), TEXT("打出后，本场不再抽到此牌。"), false},
		{TEXT("TemporaryCopy"), TEXT("临时复制"), TEXT("复制原牌；副本在生成它的回合结束时消失。")},
		{TEXT("StoredStance"), TEXT("藏式"), TEXT("保存冲锋效果，供下回合首张主动牌使用；该回合结束时失效。")},
		{TEXT("Unsheathe"), TEXT("开锋"), TEXT("本牌使用藏式时触发。")},
		{TEXT("FollowUpStance"), TEXT("余式"), TEXT("供本回合下一张主动牌再使用1次藏式；不会继续复制。")},
		{TEXT("BloodMomentum"), TEXT("血势"), TEXT("每点目标流血使指定攻击倍率增加2个百分点。")},
		{TEXT("RideMomentum"), TEXT("乘势"), TEXT("每层自身气势使指定攻击倍率增加10个百分点；未注明则不消耗。")},
		{TEXT("Armor"), TEXT("护甲"), TEXT("抵挡攻击与固定伤害；本方回合开始清空。")},
		{TEXT("Momentum"), TEXT("气势"), TEXT("每层使每段直接攻击伤害+1；普通攻击不消耗，也不衰减。")},
		{TEXT("Agility"), TEXT("灵动"), TEXT("25%概率耗1层完美闪避；否则有2层便耗2层闪避。")},
		{TEXT("Vulnerability"), TEXT("破绽"), TEXT("每层使下一段直接攻击伤害+10%；有效命中后清空。")},
		{TEXT("Mark"), TEXT("标记"), TEXT("所受直接攻击+15%，有效命中耗1层；敌人通常优先攻击有标记的友方。")},
		{TEXT("Weak"), TEXT("虚弱"), TEXT("直接攻击伤害降低50%；本方回合结束减少1层。")},
		{TEXT("Bleed"), TEXT("流血"), TEXT("被攻击命中后，额外失去等同流血值的生命。")},
		{TEXT("Poison"), TEXT("中毒"), TEXT("任意一方回合结束时，失去等同中毒值的生命。")},
		{TEXT("Burn"), TEXT("灼烧"), TEXT("自己的牌或意图结束后，失去等同灼烧值的生命。")},
		{TEXT("Corrosion"), TEXT("蚀伤"), TEXT("被毒爆或指定效果触发时，失去等同蚀伤值的生命。")},
		{TEXT("Potency"), TEXT("药效"), TEXT("治疗前全部消耗，每点使基础治疗+1；累计每获得6点，得到1层气势。")},
		{TEXT("Charge"), TEXT("蓄力"), TEXT("用于强化重箭，发动重箭时全部消耗。")},
		{TEXT("Guard"), TEXT("援护"), TEXT("替目标承受单体攻击，每次消耗1次。")},
		{TEXT("Counter"), TEXT("反击"), TEXT("对方单体直接攻击牌结算后反击，耗1次；不连锁触发。")},
		{TEXT("Block"), TEXT("格挡"), TEXT("对方单体直接攻击牌结算后，以攻击加剩余护甲反攻，耗1次；护甲保留。")},
		{TEXT("VulnerabilityImmunity"), TEXT("破绽免疫"), TEXT("免疫新施加的破绽。")},
		{TEXT("MarkFollowUp"), TEXT("追击标记"), TEXT("下次攻击的首段命中施加1层标记，出手后耗1次。")},
		{TEXT("VulnerabilityFollowUp"), TEXT("破绽追击"), TEXT("下次攻击的首段命中施加1层破绽，出手后耗1次。")},
		{TEXT("HealingBoost"), TEXT("疗愈增幅"), TEXT("下次治疗的每个目标各增加当前数值的治疗量，随后清空。")},
		{TEXT("DoubleTerrain"), TEXT("地势双效"), TEXT("下一次符合条件的地势收益额外结算1次，触发后耗1次。")},
		{TEXT("FreeTerrain"), TEXT("地势免耗"), TEXT("下一张地势牌不消耗气力。")},
		{TEXT("TerrainDiscount"), TEXT("地势减耗"), TEXT("下一张地势牌少耗1点气力。")},
		{TEXT("Redirect"), TEXT("代挡"), TEXT("下一次敌方单体攻击转向指定友方，触发后耗1次。")},
		{TEXT("RoundDoubleTerrain"), TEXT("本回合地势双效"), TEXT("本回合下一次符合条件的地势收益额外结算1次；触发或回合结束后失效。")},
		{TEXT("Wealth"), TEXT("财富"), TEXT("上限8层，强化或供钱鼠招式消耗；具体收益看意图，转阶段保留。")},
		{TEXT("Rage"), TEXT("狂怒"), TEXT("上限5层，强化赤獠招式；每张玩家主动牌造成生命伤害后+1，转阶段保留。")},
		{TEXT("Prey"), TEXT("猎物"), TEXT("老虎锁定的目标；指定猎物的招式优先攻击它。")},
		{TEXT("Terrain"), TEXT("地势"), TEXT("卡牌可触发场地收益；阵师伙伴存活时，玩家回合开始自动触发1次。")},
	};

	const FPill* FindPill(const FString& Name)
	{
		const FString Canonical=GameXXKCardPillText::CanonicalName(Name);
		for (const FPill& Pill : Pills)
		{
			if (Canonical == Pill.Name) return &Pill;
		}
		return nullptr;
	}
}

const TArray<FString>& GameXXKCardPillText::InlineNames()
{
	static const TArray<FString> Names = []
	{
		TArray<FString> Result;
		for (const FPill& Pill : Pills)
		{
			if (Pill.bInline) Result.Add(Pill.Name);
		}
		Result.Sort([](const FString& A, const FString& B) { return A.Len() > B.Len(); });
		return Result;
	}();
	return Names;
}

FString GameXXKCardPillText::CanonicalName(const FString& Name)
{
	const FString Value=Name.TrimStartAndEnd();
	if(Value.IsEmpty())return Value;
	for(const FPill& Pill:Pills)if(Value==Pill.Name)return Pill.Name;
	if(Value==TEXT("蓄力／重箭")||Value.Equals(GameXXKLocalization::EnglishText(TEXT("Pill.ChargeHeavyArrow.Name")),ESearchCase::IgnoreCase))return TEXT("蓄力／重箭");
	const TPair<const TCHAR*,const TCHAR*> Aliases[]={
		{TEXT("守护"),TEXT("援护")},{TEXT("地形双效"),TEXT("地势双效")},{TEXT("地形免耗"),TEXT("地势免耗")},
		{TEXT("地形减耗"),TEXT("地势减耗")},{TEXT("本回合地形双效"),TEXT("本回合地势双效")},
		{TEXT("Poisoned"),TEXT("中毒")},{TEXT("Bleeding"),TEXT("流血")},{TEXT("Burning"),TEXT("灼烧")},
		{TEXT("Marked"),TEXT("标记")},{TEXT("Vulnerable"),TEXT("破绽")},{TEXT("Armored"),TEXT("护甲")},
		{TEXT("Counterattack"),TEXT("反击")},{TEXT("Temporary Copies"),TEXT("临时复制")},{TEXT("Formation"),TEXT("阵法")}};
	for(const auto& Alias:Aliases)if(Value.Equals(Alias.Key,ESearchCase::IgnoreCase))return Alias.Value;
	for(const FPill& Pill:Pills)
	{
		const FString English=GameXXKLocalization::EnglishText(*(FString(TEXT("Pill."))+Pill.Id+TEXT(".Name")));
		if(!English.IsEmpty()&&Value.Equals(English,ESearchCase::IgnoreCase))return Pill.Name;
	}
	return Value;
}

const TArray<FString>& GameXXKCardPillText::DisplayInlineNames()
{
	static uint64 CachedRevision=MAX_uint64;
	static TArray<FString> Names;
	const uint64 Revision=GameXXKLocalization::GetRevision();
	if(CachedRevision!=Revision)
	{
		Names=InlineNames();
		for(const FPill& Pill:Pills)if(Pill.bInline)
		{
			const FString English=GameXXKLocalization::EnglishText(*(FString(TEXT("Pill."))+Pill.Id+TEXT(".Name")));
			if(!English.IsEmpty())Names.AddUnique(English);
		}
		Names.Append({TEXT("本回合地形双效"),TEXT("地形双效"),TEXT("地形免耗"),TEXT("地形减耗"),TEXT("守护"),
			TEXT("Poisoned"),TEXT("Bleeding"),TEXT("Burning"),TEXT("Marked"),TEXT("Vulnerable"),TEXT("Armored"),TEXT("Counterattack"),TEXT("Temporary Copies")});
		Names.Sort([](const FString& A,const FString& B){return A.Len()>B.Len();});
		CachedRevision=Revision;
	}
	return Names;
}

bool GameXXKCardPillText::MatchesAt(const FString& Text,int32 Offset,const FString& Name)
{
	if(Offset<0||Name.IsEmpty()||Offset+Name.Len()>Text.Len()||!Text.Mid(Offset,Name.Len()).Equals(Name,ESearchCase::IgnoreCase))return false;
	const auto IsAsciiWord=[](TCHAR C){return (C>=TEXT('A')&&C<=TEXT('Z'))||(C>=TEXT('a')&&C<=TEXT('z'))||(C>=TEXT('0')&&C<=TEXT('9'))||C==TEXT('_');};
	if(IsAsciiWord(Name[0])&&Offset>0&&IsAsciiWord(Text[Offset-1]))return false;
	const int32 End=Offset+Name.Len();
	if(IsAsciiWord(Name[Name.Len()-1])&&End<Text.Len()&&IsAsciiWord(Text[End]))return false;
	return true;
}

bool GameXXKCardPillText::IsKeyword(const FString& Name)
{
	return CanonicalName(Name) == TEXT("蓄力／重箭") || FindPill(Name) != nullptr;
}

FString GameXXKCardPillText::DescribeTerm(const FString& Name)
{
	const FPill* Pill = FindPill(Name);
	return Pill ? FString(Pill->Description) : FString();
}

FString GameXXKCardPillText::DescribeHelp(const FString& CardText, const EGameXXKCardQuality Quality, const int32 TaskCardCount)
{
	TArray<FString> Names;
	for (int32 Pos = 0; Pos < CardText.Len();)
	{
		const FString* Found = InlineNames().FindByPredicate([&](const FString& Name)
		{
			return CardText.Mid(Pos, Name.Len()) == Name;
		});
		if (Found)
		{
			Names.AddUnique(*Found);
			Pos += Found->Len();
		}
		else ++Pos;
	}
	TArray<FString> SourceLines;
	CardText.ParseIntoArrayLines(SourceLines);
	for (const FPill& Pill : Pills)
	{
		if (!Pill.bInline && SourceLines.ContainsByPredicate([&](const FString& Line)
		{
			return Line == Pill.Name || Line.StartsWith(FString(Pill.Name) + TEXT("："));
		})) Names.AddUnique(Pill.Name);
	}
	if (TaskCardCount > 0) Names.AddUnique(TEXT("法术任务"));
	const bool bMergeHeavy = Names.Contains(TEXT("蓄力")) && Names.Contains(TEXT("重箭"));
	bool bMergedHeavy = false;
	bool bHasDot = false;
	TArray<FString> Lines = {TEXT("本牌术语")};
	const int32 QualityPercent = Quality == EGameXXKCardQuality::Epic ? 140 : Quality == EGameXXKCardQuality::Rare ? 120 : 100;
	for (const FString& Name : Names)
	{
		if (bMergeHeavy && (Name == TEXT("蓄力") || Name == TEXT("重箭")))
		{
			if (!bMergedHeavy) Lines.Add(TEXT("蓄力／重箭：消耗全部蓄力，按消耗量强化本牌。"));
			bMergedHeavy = true;
			continue;
		}
		const FPill* Pill = FindPill(Name);
		if (!Pill) continue;
		FString Description = Pill->Description;
		if (Name == TEXT("法术任务") && TaskCardCount > 0)
		{
			Description = FString::Printf(TEXT("本组%d种牌各主动打出1次，依序免费重放；每回合限1次。"), TaskCardCount);
		}
		else if (Name == TEXT("反击"))
		{
			Description = FString::Printf(TEXT("对方单体直接攻击牌结算后反击，%d%%攻击伤害；耗1次，不连锁。"), QualityPercent);
		}
		else if (Name == TEXT("格挡"))
		{
			Description = FString::Printf(TEXT("对方单体直接攻击牌结算后，以%d%%攻击＋剩余护甲反击；耗1次，护甲保留。"), QualityPercent);
		}
		Lines.Add(Name + TEXT("：") + Description);
		bHasDot |= Name == TEXT("流血") || Name == TEXT("中毒") || Name == TEXT("灼烧") || Name == TEXT("蚀伤") || Name == TEXT("毒爆");
	}
	if (Lines.Num() == 1) Lines.Add(TEXT("无额外关键词说明。"));
	if (bHasDot) Lines.Add(TEXT("持续伤害直接损失生命，触发不消耗数值。"));
	Lines.Add(TEXT("Ctrl：返回牌面 · Shift：详述"));
	return FString::Join(Lines, TEXT("\n"));
}
