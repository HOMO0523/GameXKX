#include "GameXXKCardQualityRules.h"

#include <initializer_list>

namespace
{
	TArray<FName> MakeIdList(const std::initializer_list<const TCHAR*> Ids)
	{
		TArray<FName> Result;
		Result.Reserve(static_cast<int32>(Ids.size()));
		for (const TCHAR* Id : Ids)
		{
			Result.Add(FName(Id));
		}
		return Result;
	}

	const TArray<FName>& GetEpicCardIds()
	{
		static const TArray<FName> Ids = MakeIdList({
			TEXT("Npc.TusiChief.MengZhaiShiYue"), TEXT("Npc.SongJinBao.YiNuoQianJin"),
			TEXT("Npc.YueBai.ShanHeCanTu"), TEXT("Npc.ZhouGuangZu.YanFenFengMai"),
			TEXT("Npc.JinGui.HouXiangTuoShen"), TEXT("Npc.QiongMeiEr.ShanGeHuanLing"),
			TEXT("Profession.Blade.CanYueSanDie"), TEXT("Profession.Blade.ZhanJin"), TEXT("Profession.Blade.YiShiDuanJiang"),
			TEXT("Profession.Guard.BuDongRuShan"), TEXT("Profession.Guard.TieSuoHengJiang"), TEXT("Profession.Guard.YiFuDangGuan"),
			TEXT("Profession.Healer.YaoWangGuiYuan"), TEXT("Profession.Healer.YaoNangFeiTou"), TEXT("Profession.Healer.WuWeiTiaoHe"),
			TEXT("Profession.Hunter.ShouHun"), TEXT("Profession.Hunter.BaiBuChuanYang"), TEXT("Profession.Hunter.YingLuo"),
			TEXT("Profession.Sorcerer.XingHuoLiaoYuan"), TEXT("Profession.Sorcerer.FenTianJue"), TEXT("Profession.Sorcerer.ChiYanFengJie"),
			TEXT("Profession.FormationMaster.ZhenShaZhen"), TEXT("Profession.FormationMaster.WanXiangGuiZhen"),
			TEXT("Profession.FormationMaster.SiXiangLianHuan"),
			TEXT("Route.Boss.XiongPiPiJia"), TEXT("Route.Boss.HanDiYiShi"), TEXT("Route.Boss.HuPoZhenDan"),
			TEXT("Route.Boss.DuKouLieFeng"), TEXT("Route.Boss.FuHuDuanJiang")
		});
		return Ids;
	}

	const TArray<FName>& GetRareCardIds()
	{
		static const TArray<FName> Ids = MakeIdList({
			TEXT("Npc.TusiChief.ZhaiZhuHaoLing"), TEXT("Npc.SongJinBao.ErMuMiBao"),
			TEXT("Npc.YueBai.CanJuanPiZhu"), TEXT("Npc.ZhouGuangZu.DiZhiMoTu"),
			TEXT("Npc.JinGui.ShiJingErMu"), TEXT("Npc.QiongMeiEr.TengQiaoFeiDu"),
			TEXT("Profession.Blade.DuanYue"), TEXT("Profession.Blade.YinXueDao"), TEXT("Profession.Blade.PoJun"),
			TEXT("Profession.Blade.ZhanYiFeiTeng"), TEXT("Profession.Blade.XiaoJiaLianJi"), TEXT("Profession.Blade.DaoYiShouShu"),
			TEXT("Profession.Guard.FanZhenJia"), TEXT("Profession.Guard.ZhenYueLing"), TEXT("Profession.Guard.QinWangDunJi"),
			TEXT("Profession.Guard.TieBiRuShan"), TEXT("Profession.Guard.BiLeiFanGong"), TEXT("Profession.Guard.SuiJiaHuiJi"),
			TEXT("Profession.Healer.LingZhiXuMing"), TEXT("Profession.Healer.HuiChunLu"), TEXT("Profession.Healer.WenYangGao"),
			TEXT("Profession.Healer.FuGuSan"), TEXT("Profession.Healer.JinChuangXuMing"), TEXT("Profession.Healer.KuShenMaSan"),
			TEXT("Profession.Hunter.ChuanYang"), TEXT("Profession.Hunter.LianZhuJian"), TEXT("Profession.Hunter.YinZong"),
			TEXT("Profession.Hunter.DuanMaiShi"), TEXT("Profession.Hunter.PoJiaDing"), TEXT("Profession.Hunter.FuYeXianJing"),
			TEXT("Profession.Sorcerer.BaoYanShu"), TEXT("Profession.Sorcerer.SheLingHuo"), TEXT("Profession.Sorcerer.LingYanLianDan"),
			TEXT("Profession.Sorcerer.HuLingMu"), TEXT("Profession.Sorcerer.ChiXiaoFenXing"), TEXT("Profession.Sorcerer.XingHuoHuiShou"),
			TEXT("Profession.FormationMaster.CunZhaiYuanZhen"), TEXT("Profession.FormationMaster.HuiShengZhenSha"),
			TEXT("Profession.FormationMaster.BaMenLunZhuan"), TEXT("Profession.FormationMaster.ShuiJingZheGuang"),
			TEXT("Profession.FormationMaster.ZhenQiGuWu"), TEXT("Profession.FormationMaster.DiMaiJieLi"),
			TEXT("Route.Rare.GuJuanCanZhang"), TEXT("Route.Rare.TieYiYiJue"), TEXT("Route.Rare.LingQuanYiYin"),
			TEXT("Route.Rare.JueJingFanJi"), TEXT("Route.Rare.TongXinHeBi")
		});
		return Ids;
	}

	const TArray<FName>& GetEpicRelicIds()
	{
		static const TArray<FName> Ids = MakeIdList({
			TEXT("Relic.BambooTally"), TEXT("Relic.CraneFeather"), TEXT("Relic.ChessStone"),
			TEXT("Relic.DrumCharm"), TEXT("Relic.OldMap")
		});
		return Ids;
	}

	const TArray<FName>& GetRareRelicIds()
	{
		static const TArray<FName> Ids = MakeIdList({
			TEXT("Relic.TigerSeal"), TEXT("Relic.InkTalisman"), TEXT("Relic.CloudMirror"), TEXT("Relic.StoneBead"),
			TEXT("Relic.IronKnot"), TEXT("Relic.Compass"), TEXT("Relic.RedCord"), TEXT("Relic.BronzeNeedle"),
			TEXT("Relic.LotusSeed"), TEXT("Relic.SwordGuard")
		});
		return Ids;
	}

	bool IsConcreteQuality(const EGameXXKCardQuality Quality)
	{
		return Quality == EGameXXKCardQuality::Common
			|| Quality == EGameXXKCardQuality::Rare
			|| Quality == EGameXXKCardQuality::Epic;
	}

	EGameXXKCardQuality ResolveQuality(
		const FGameXXKCardDefinition& BaseDefinition,
		const EGameXXKCardQuality RequestedQuality)
	{
		if (IsConcreteQuality(RequestedQuality))
		{
			return RequestedQuality;
		}
		return IsConcreteQuality(BaseDefinition.BaseQuality)
			? BaseDefinition.BaseQuality
			: EGameXXKCardQuality::Common;
	}

	int32 ClampToInt32(const int64 Value)
	{
		if (Value > static_cast<int64>(TNumericLimits<int32>::Max()))
		{
			return TNumericLimits<int32>::Max();
		}
		if (Value < static_cast<int64>(TNumericLimits<int32>::Min()))
		{
			return TNumericLimits<int32>::Min();
		}
		return static_cast<int32>(Value);
	}

	int32 ScaleMagnitude(
		const EGameXXKCardEffectType EffectType,
		const int32 BaseMagnitude,
		const EGameXXKCardQuality Quality)
	{
		const int64 QualityStep = Quality == EGameXXKCardQuality::Epic
			? 2
			: (Quality == EGameXXKCardQuality::Rare ? 1 : 0);
		const int64 Multiplier = Quality == EGameXXKCardQuality::Epic
			? 4
			: (Quality == EGameXXKCardQuality::Rare ? 2 : 1);

		switch (EffectType)
		{
		case EGameXXKCardEffectType::DamagePercentAttack:
		case EGameXXKCardEffectType::DamageFlat:
		case EGameXXKCardEffectType::EachLivingAllyAttackSelectedTarget:
		case EGameXXKCardEffectType::Heal:
		case EGameXXKCardEffectType::AddArmor:
			return ClampToInt32(static_cast<int64>(BaseMagnitude) * Multiplier);

		case EGameXXKCardEffectType::DrawCards:
		case EGameXXKCardEffectType::ApplyStatus:
		case EGameXXKCardEffectType::RemoveStatus:
		case EGameXXKCardEffectType::RemoveAnyDamageOverTime:
			return ClampToInt32(static_cast<int64>(BaseMagnitude) + QualityStep);

		case EGameXXKCardEffectType::GainMana:
		case EGameXXKCardEffectType::GainManaPerConsumedStatus:
			return ClampToInt32(static_cast<int64>(BaseMagnitude) + QualityStep * 2);

		default:
			return BaseMagnitude;
		}
	}

	bool ValidateClassificationLists(
		const TCHAR* CatalogLabel,
		const TArray<FName>& RareIds,
		const int32 ExpectedRareCount,
		const TArray<FName>& EpicIds,
		const int32 ExpectedEpicCount,
		const TSet<FName>& CatalogIds,
		FString& OutError)
	{
		if (RareIds.Num() != ExpectedRareCount)
		{
			OutError = FString::Printf(TEXT("%s Rare classification expected %d IDs but found %d."), CatalogLabel, ExpectedRareCount, RareIds.Num());
			return false;
		}
		if (EpicIds.Num() != ExpectedEpicCount)
		{
			OutError = FString::Printf(TEXT("%s Epic classification expected %d IDs but found %d."), CatalogLabel, ExpectedEpicCount, EpicIds.Num());
			return false;
		}

		TSet<FName> ClassifiedIds;
		ClassifiedIds.Reserve(RareIds.Num() + EpicIds.Num());
		for (const FName Id : RareIds)
		{
			if (Id.IsNone())
			{
				OutError = FString::Printf(TEXT("%s Rare classification contains an empty ID."), CatalogLabel);
				return false;
			}
			if (ClassifiedIds.Contains(Id))
			{
				OutError = FString::Printf(TEXT("%s classification contains a duplicate Rare ID: %s."), CatalogLabel, *Id.ToString());
				return false;
			}
			if (!CatalogIds.Contains(Id))
			{
				OutError = FString::Printf(TEXT("%s Rare classification references an ID outside the catalog: %s."), CatalogLabel, *Id.ToString());
				return false;
			}
			ClassifiedIds.Add(Id);
		}
		for (const FName Id : EpicIds)
		{
			if (Id.IsNone())
			{
				OutError = FString::Printf(TEXT("%s Epic classification contains an empty ID."), CatalogLabel);
				return false;
			}
			if (ClassifiedIds.Contains(Id))
			{
				OutError = FString::Printf(TEXT("%s Rare/Epic classifications overlap or duplicate ID: %s."), CatalogLabel, *Id.ToString());
				return false;
			}
			if (!CatalogIds.Contains(Id))
			{
				OutError = FString::Printf(TEXT("%s Epic classification references an ID outside the catalog: %s."), CatalogLabel, *Id.ToString());
				return false;
			}
			ClassifiedIds.Add(Id);
		}
		return true;
	}

	template <typename DefinitionType, typename QualityGetter>
	bool ValidateCatalog(
		const TCHAR* CatalogLabel,
		const TArray<DefinitionType>& Definitions,
		const int32 ExpectedTotalCount,
		const int32 ExpectedCommonCount,
		const TArray<FName>& RareIds,
		const int32 ExpectedRareCount,
		const TArray<FName>& EpicIds,
		const int32 ExpectedEpicCount,
		QualityGetter&& GetExpectedQuality,
		FString& OutError)
	{
		OutError.Reset();
		if (Definitions.Num() != ExpectedTotalCount)
		{
			OutError = FString::Printf(TEXT("%s catalog expected %d definitions but found %d."), CatalogLabel, ExpectedTotalCount, Definitions.Num());
			return false;
		}

		TSet<FName> CatalogIds;
		CatalogIds.Reserve(Definitions.Num());
		for (const DefinitionType& Definition : Definitions)
		{
			if (Definition.Id.IsNone())
			{
				OutError = FString::Printf(TEXT("%s catalog contains an empty ID."), CatalogLabel);
				return false;
			}
			if (CatalogIds.Contains(Definition.Id))
			{
				OutError = FString::Printf(TEXT("%s catalog contains duplicate ID: %s."), CatalogLabel, *Definition.Id.ToString());
				return false;
			}
			CatalogIds.Add(Definition.Id);
		}

		if (!ValidateClassificationLists(
			CatalogLabel,
			RareIds,
			ExpectedRareCount,
			EpicIds,
			ExpectedEpicCount,
			CatalogIds,
			OutError))
		{
			return false;
		}

		int32 CommonCount = 0;
		int32 RareCount = 0;
		int32 EpicCount = 0;
		for (const DefinitionType& Definition : Definitions)
		{
			if (!IsConcreteQuality(Definition.BaseQuality))
			{
				OutError = FString::Printf(TEXT("%s catalog has an invalid quality for %s."), CatalogLabel, *Definition.Id.ToString());
				return false;
			}
			const EGameXXKCardQuality ExpectedQuality = GetExpectedQuality(Definition.Id);
			if (Definition.BaseQuality != ExpectedQuality)
			{
				OutError = FString::Printf(TEXT("%s catalog quality does not match classification for %s."), CatalogLabel, *Definition.Id.ToString());
				return false;
			}
			switch (Definition.BaseQuality)
			{
			case EGameXXKCardQuality::Common: ++CommonCount; break;
			case EGameXXKCardQuality::Rare: ++RareCount; break;
			case EGameXXKCardQuality::Epic: ++EpicCount; break;
			default: break;
			}
		}

		if (CommonCount != ExpectedCommonCount || RareCount != ExpectedRareCount || EpicCount != ExpectedEpicCount)
		{
			OutError = FString::Printf(
				TEXT("%s catalog quality counts expected %d/%d/%d but found %d/%d/%d."),
				CatalogLabel,
				ExpectedCommonCount,
				ExpectedRareCount,
				ExpectedEpicCount,
				CommonCount,
				RareCount,
				EpicCount);
			return false;
		}
		return true;
	}
}

FGameXXKCardDefinition FGameXXKCardQualityRules::BuildEffectiveDefinition(
	const FGameXXKCardDefinition& BaseDefinition,
	const EGameXXKCardQuality CurrentQuality)
{
	FGameXXKCardDefinition EffectiveDefinition = BaseDefinition;
	const EGameXXKCardQuality ResolvedQuality = ResolveQuality(BaseDefinition, CurrentQuality);
	for (FGameXXKCardEffect& Effect : EffectiveDefinition.Effects)
	{
		Effect.Magnitude = ScaleMagnitude(Effect.Type, Effect.Magnitude, ResolvedQuality);
		if (Effect.Type == EGameXXKCardEffectType::ApplyBattleModifier)
		{
			Effect.Modifier.Magnitude = ScaleMagnitude(
				Effect.Modifier.EffectType,
				Effect.Modifier.Magnitude,
				ResolvedQuality);
		}
	}
	return EffectiveDefinition;
}

int32 FGameXXKCardQualityRules::GetCardPrice(const EGameXXKCardQuality Quality)
{
	switch (Quality)
	{
	case EGameXXKCardQuality::Common: return 25;
	case EGameXXKCardQuality::Rare: return 40;
	case EGameXXKCardQuality::Epic: return 60;
	default: return 0;
	}
}

int32 FGameXXKCardQualityRules::GetRelicPrice(const EGameXXKCardQuality Quality)
{
	switch (Quality)
	{
	case EGameXXKCardQuality::Common: return 70;
	case EGameXXKCardQuality::Rare: return 100;
	case EGameXXKCardQuality::Epic: return 140;
	default: return 0;
	}
}

FText FGameXXKCardQualityRules::GetDisplayName(const EGameXXKCardQuality Quality)
{
	switch (Quality)
	{
	case EGameXXKCardQuality::Common: return NSLOCTEXT("GameXXKCardQuality", "Common", "普通");
	case EGameXXKCardQuality::Rare: return NSLOCTEXT("GameXXKCardQuality", "Rare", "稀有");
	case EGameXXKCardQuality::Epic: return NSLOCTEXT("GameXXKCardQuality", "Epic", "珍稀");
	default: return NSLOCTEXT("GameXXKCardQuality", "Invalid", "无效品质");
	}
}

FLinearColor FGameXXKCardQualityRules::GetDisplayColor(const EGameXXKCardQuality Quality)
{
	switch (Quality)
	{
	case EGameXXKCardQuality::Common: return FLinearColor(0.94f, 0.91f, 0.82f, 1.0f);
	case EGameXXKCardQuality::Rare: return FLinearColor(0.30f, 0.58f, 0.86f, 1.0f);
	case EGameXXKCardQuality::Epic: return FLinearColor(0.55f, 0.35f, 0.78f, 1.0f);
	default: return FLinearColor(0.50f, 0.50f, 0.50f, 1.0f);
	}
}

EGameXXKCardQuality FGameXXKCardQualityRules::GetCardBaseQuality(const FName CardId)
{
	if (GetEpicCardIds().Contains(CardId))
	{
		return EGameXXKCardQuality::Epic;
	}
	if (GetRareCardIds().Contains(CardId))
	{
		return EGameXXKCardQuality::Rare;
	}
	return EGameXXKCardQuality::Common;
}

EGameXXKCardQuality FGameXXKCardQualityRules::GetRelicBaseQuality(const FName RelicId)
{
	if (GetEpicRelicIds().Contains(RelicId))
	{
		return EGameXXKCardQuality::Epic;
	}
	if (GetRareRelicIds().Contains(RelicId))
	{
		return EGameXXKCardQuality::Rare;
	}
	return EGameXXKCardQuality::Common;
}

bool FGameXXKCardQualityRules::ValidateCardCatalog(
	const TArray<FGameXXKCardDefinition>& Definitions,
	FString& OutError)
{
	return ValidateCatalog(
		TEXT("Card"),
		Definitions,
		198,
		122,
		GetRareCardIds(),
		47,
		GetEpicCardIds(),
		29,
		[](const FName Id) { return FGameXXKCardQualityRules::GetCardBaseQuality(Id); },
		OutError);
}

bool FGameXXKCardQualityRules::ValidateRelicCatalog(
	const TArray<FGameXXKRelicDefinition>& Definitions,
	FString& OutError)
{
	return ValidateCatalog(
		TEXT("Relic"),
		Definitions,
		30,
		15,
		GetRareRelicIds(),
		10,
		GetEpicRelicIds(),
		5,
		[](const FName Id) { return FGameXXKCardQualityRules::GetRelicBaseQuality(Id); },
		OutError);
}
