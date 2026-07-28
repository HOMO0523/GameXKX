#include "UI/GameXXKBattleAnimationPresentation.h"

namespace
{
	struct FRuntimeAssetMapping
	{
		const TCHAR* RuntimeToken;
		const TCHAR* AssetId;
	};

	constexpr FRuntimeAssetMapping PartyMappings[] = {
		{TEXT("Companion_Blade_"), TEXT("character_01_blade")},
		{TEXT("Companion_Guard_"), TEXT("character_02_guard")},
		{TEXT("Companion_Healer_"), TEXT("character_03_healer")},
		{TEXT("Companion_Hunter_"), TEXT("character_04_hunter")},
		{TEXT("Companion_Sorcerer_"), TEXT("character_05_sorcerer")},
		{TEXT("Companion_FormationMaster_"), TEXT("character_06_formation_master")},
		{TEXT("Npc.TusiChief"), TEXT("character_07_tusi_chief")},
		{TEXT("Npc.SongJinBao"), TEXT("character_08_song_jin_bao")},
		{TEXT("Npc.YueBai"), TEXT("character_09_yue_bai")},
		{TEXT("Npc.ZhouGuangZu"), TEXT("character_10_zhou_guang_zu")},
		{TEXT("Npc.JinGui"), TEXT("character_11_jin_gui")},
		{TEXT("Npc.QiongMeiEr"), TEXT("character_12_qiong_mei_er")},
	};

	constexpr FRuntimeAssetMapping EnemyMappings[] = {
		{TEXT("IronfeatherRooster"), TEXT("enemy_05_ironfeather")},
		{TEXT("BluehornGoatKing"), TEXT("enemy_06_bluehorn")},
		{TEXT("GraymaneWolfKing"), TEXT("enemy_11_graymane")},
		{TEXT("RedtuskBoarKing"), TEXT("enemy_12_redtusk")},
		{TEXT("SpiralHornDeer"), TEXT("enemy_18_deer")},
		{TEXT("MoneyRat"), TEXT("enemy_19_moneyrat_boss")},
		{TEXT("BlackBear"), TEXT("enemy_20_blackbear_boss")},
		{TEXT("Tiger"), TEXT("enemy_21_tiger_boss")},
		{TEXT("Rooster"), TEXT("enemy_01_rooster")},
		{TEXT("Goat"), TEXT("enemy_02_goat")},
		{TEXT("Weasel"), TEXT("enemy_03_weasel")},
		{TEXT("Civet"), TEXT("enemy_04_civet")},
		{TEXT("GrayWolf"), TEXT("enemy_07_graywolf")},
		{TEXT("Boar"), TEXT("enemy_08_boar")},
		{TEXT("Macaque"), TEXT("enemy_09_macaque")},
		{TEXT("Porcupine"), TEXT("enemy_10_porcupine")},
		{TEXT("VenomSnake"), TEXT("enemy_13_snake")},
		{TEXT("Wildcat"), TEXT("enemy_14_wildcat")},
		{TEXT("Vulture"), TEXT("enemy_15_vulture")},
		{TEXT("GiantToad"), TEXT("enemy_16_toad")},
		{TEXT("WhiteApe"), TEXT("enemy_17_whiteape")},
	};

	const TCHAR* ResolveActionSuffix(const EGameXXKBattleAnimationAction Action)
	{
		switch (Action)
		{
		case EGameXXKBattleAnimationAction::Idle: return TEXT("idle");
		case EGameXXKBattleAnimationAction::Attack: return TEXT("attack");
		case EGameXXKBattleAnimationAction::Hit: return TEXT("hit");
		case EGameXXKBattleAnimationAction::Death: return TEXT("death");
		default: return nullptr;
		}
	}

	FGameXXKBattleAnimationClipDescriptor MakeClip(const FString& AssetId, const float PlaybackRate)
	{
		FGameXXKBattleAnimationClipDescriptor Clip;
		Clip.AssetId = AssetId;
		Clip.FrameCount = 60;
		Clip.PlaybackRate = PlaybackRate;
		const FString TextureName = FString::Printf(TEXT("T_%s_atlas"), *AssetId);
		Clip.TexturePath = FSoftObjectPath(FString::Printf(
			TEXT("/Game/GameXXK/BattleAnimations/Atlases/%s.%s"),
			*TextureName,
			*TextureName));
		return Clip;
	}
}

FString FGameXXKBattleAnimationPresentation::ResolveUnitAssetId(const FName RuntimeUnitId, const bool bEnemy)
{
	const FString RuntimeId = RuntimeUnitId.ToString();
	if (!bEnemy)
	{
		for (const FRuntimeAssetMapping& Mapping : PartyMappings)
		{
			if (RuntimeId.Contains(Mapping.RuntimeToken))
			{
				return Mapping.AssetId;
			}
		}
		return TEXT("character_00_hero");
	}

	for (const FRuntimeAssetMapping& Mapping : EnemyMappings)
	{
		if (RuntimeId.Contains(Mapping.RuntimeToken))
		{
			return Mapping.AssetId;
		}
	}
	return TEXT("enemy_01_rooster");
}

FGameXXKBattleAnimationClipDescriptor FGameXXKBattleAnimationPresentation::ResolveClip(
	const FName RuntimeUnitId,
	const bool bEnemy,
	const EGameXXKBattleAnimationAction Action)
{
	const TCHAR* ActionSuffix = ResolveActionSuffix(Action);
	if (!ActionSuffix)
	{
		return {};
	}
	const FString UnitAssetId = ResolveUnitAssetId(RuntimeUnitId, bEnemy);
	if (UnitAssetId == TEXT("enemy_07_graywolf") && Action == EGameXXKBattleAnimationAction::Attack)
	{
		return {};
	}
	const FString ClipAssetId = FString::Printf(TEXT("%s_%s"), *UnitAssetId, ActionSuffix);
	const float PlaybackRate = Action == EGameXXKBattleAnimationAction::Idle ? 1.0f : 2.0f;
	return MakeClip(ClipAssetId, PlaybackRate);
}

FGameXXKBattleAnimationClipDescriptor FGameXXKBattleAnimationPresentation::ResolveGenericClip(
	const EGameXXKBattleAnimationAction Action)
{
	switch (Action)
	{
	case EGameXXKBattleAnimationAction::Buff:
		return MakeClip(TEXT("status_buff_generic"), 1.0f);
	case EGameXXKBattleAnimationAction::Debuff:
		return MakeClip(TEXT("status_debuff_generic"), 1.0f);
	case EGameXXKBattleAnimationAction::Impact:
		return MakeClip(TEXT("impact_ink_generic"), 4.0f);
	default:
		return {};
	}
}

FSoftObjectPath FGameXXKBattleAnimationPresentation::ResolveIdleFlipbookPath(
	const FName RuntimeUnitId,
	const bool bEnemy)
{
	const FString UnitAssetId = ResolveUnitAssetId(RuntimeUnitId, bEnemy);
	const FString FlipbookName = FString::Printf(TEXT("FB_%s_idle"), *UnitAssetId);
	return FSoftObjectPath(FString::Printf(
		TEXT("/Game/GameXXK/BattleAnimations/IdleFlipbooks/%s.%s"),
		*FlipbookName,
		*FlipbookName));
}

TArray<FGameXXKBattleAnimationCombatRequest> FGameXXKBattleAnimationPresentation::BuildCombatRequests(
	const FGameXXKCardBattleRuntime& PostDamageBattle,
	const FName FallbackAttackerUnitId,
	const TArray<FGameXXKCardDamageResult>& DamageResults)
{
	TMap<FName, int32> LastPacketByTarget;
	for (int32 Index = 0; Index < DamageResults.Num(); ++Index)
	{
		const FGameXXKCardDamageResult& Damage = DamageResults[Index];
		const FName TargetUnitId = Damage.ResolvedTargetUnitId.IsNone()
			? Damage.OriginalTargetUnitId
			: Damage.ResolvedTargetUnitId;
		if (!TargetUnitId.IsNone())
		{
			LastPacketByTarget.Add(TargetUnitId, Index);
		}
	}

	const auto FindUnit = [&PostDamageBattle](const FName UnitId) -> const FGameXXKCardCombatUnit*
	{
		return PostDamageBattle.Units.FindByPredicate([UnitId](const FGameXXKCardCombatUnit& Unit)
		{
			return Unit.UnitId == UnitId;
		});
	};

	TArray<FGameXXKBattleAnimationCombatRequest> Requests;
	Requests.Reserve(DamageResults.Num());
	for (int32 Index = 0; Index < DamageResults.Num(); ++Index)
	{
		const FGameXXKCardDamageResult& Damage = DamageResults[Index];
		const FName TargetUnitId = Damage.ResolvedTargetUnitId.IsNone()
			? Damage.OriginalTargetUnitId
			: Damage.ResolvedTargetUnitId;
		if (TargetUnitId.IsNone())
		{
			continue;
		}

		FGameXXKBattleAnimationCombatRequest& Request = Requests.AddDefaulted_GetRef();
		Request.AttackerUnitId = Damage.SourceUnitId.IsNone() ? FallbackAttackerUnitId : Damage.SourceUnitId;
		Request.TargetUnitId = TargetUnitId;
		if (const FGameXXKCardCombatUnit* Attacker = FindUnit(Request.AttackerUnitId))
		{
			Request.bAttackerEnemy = Attacker->Side == EGameXXKCardTargetSide::Enemy;
		}
		if (const FGameXXKCardCombatUnit* Target = FindUnit(TargetUnitId))
		{
			Request.bTargetEnemy = Target->Side == EGameXXKCardTargetSide::Enemy;
			Request.bTargetDefeated = LastPacketByTarget.FindRef(TargetUnitId) == Index
				&& (!Target->bLiving || Target->HP <= 0);
		}
	}
	return Requests;
}

int32 FGameXXKBattleAnimationPresentation::CalculateFrameIndex(
	const FGameXXKBattleAnimationClipDescriptor& Clip,
	const float RuntimeElapsedSeconds,
	const bool bLooping)
{
	if (!Clip.IsValid())
	{
		return 0;
	}
	const int32 UnboundedFrame = FMath::FloorToInt(
		FMath::Max(0.0f, RuntimeElapsedSeconds) * Clip.PlaybackRate * Clip.SourceFramesPerSecond);
	return bLooping
		? UnboundedFrame % Clip.FrameCount
		: FMath::Clamp(UnboundedFrame, 0, Clip.FrameCount - 1);
}

FBox2f FGameXXKBattleAnimationPresentation::CalculateUvRegion(
	const FGameXXKBattleAnimationClipDescriptor& Clip,
	const int32 FrameIndex)
{
	const int32 SafeColumns = FMath::Max(1, Clip.Columns);
	const int32 SafeRows = FMath::Max(1, Clip.Rows);
	const int32 SafeFrame = FMath::Clamp(FrameIndex, 0, FMath::Max(0, Clip.FrameCount - 1));
	const int32 Column = SafeFrame % SafeColumns;
	const int32 Row = SafeFrame / SafeColumns;
	const FVector2f CellSize(1.0f / SafeColumns, 1.0f / SafeRows);
	const FVector2f Minimum(Column * CellSize.X, Row * CellSize.Y);
	return FBox2f(Minimum, Minimum + CellSize);
}

float FGameXXKBattleAnimationPresentation::GetRuntimeDuration(
	const FGameXXKBattleAnimationClipDescriptor& Clip)
{
	return Clip.IsValid()
		? Clip.FrameCount / (Clip.SourceFramesPerSecond * Clip.PlaybackRate)
		: 0.0f;
}

float FGameXXKBattleAnimationPresentation::GetImpactRuntimeSeconds()
{
	return 2.2f / 2.0f;
}
