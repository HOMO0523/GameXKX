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

	struct FStatusPresentationUnitSnapshot
	{
		FName UnitId = NAME_None;
		EGameXXKCardTargetSide Side = EGameXXKCardTargetSide::Invalid;
		int32 StableSortOrder = INDEX_NONE;
		bool bHasMetadata = false;
		TMap<EGameXXKCardStatus, int64> BeforeStacks;
		TMap<EGameXXKCardStatus, int64> AfterStacks;
	};

	void CaptureStatusSnapshot(
		const FGameXXKCardBattleRuntime& Battle,
		const bool bAfter,
		TMap<FName, FStatusPresentationUnitSnapshot>& InOutSnapshots)
	{
		for (const FGameXXKCardCombatUnit& Unit : Battle.Units)
		{
			if (Unit.UnitId.IsNone())
			{
				continue;
			}

			FStatusPresentationUnitSnapshot& Snapshot = InOutSnapshots.FindOrAdd(Unit.UnitId);
			Snapshot.UnitId = Unit.UnitId;
			// Prefer the post-mutation identity metadata when both snapshots contain the unit.
			if (bAfter || !Snapshot.bHasMetadata)
			{
				Snapshot.Side = Unit.Side;
				Snapshot.StableSortOrder = Unit.StableSortOrder;
				Snapshot.bHasMetadata = true;
			}

			TMap<EGameXXKCardStatus, int64>& CapturedStacks = bAfter
				? Snapshot.AfterStacks
				: Snapshot.BeforeStacks;
			for (const FGameXXKCardStatusStack& Stack : Unit.Statuses)
			{
				if (Stack.Status == EGameXXKCardStatus::Invalid
					|| Stack.Status == EGameXXKCardStatus::None)
				{
					continue;
				}
				CapturedStacks.FindOrAdd(Stack.Status) += static_cast<int64>(Stack.Stacks);
			}
		}
	}

	int32 ClampStatusStackValue(const int64 Value)
	{
		return static_cast<int32>(FMath::Clamp<int64>(Value, MIN_int32, MAX_int32));
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
	const float PlaybackRate = Action == EGameXXKBattleAnimationAction::Attack
		|| Action == EGameXXKBattleAnimationAction::Hit
		? 2.0f
		: 1.0f;
	return MakeClip(ClipAssetId, PlaybackRate);
}

FGameXXKBattleAnimationClipDescriptor FGameXXKBattleAnimationPresentation::ResolveClipForDefinition(
	const FName RuntimeUnitId,
	const FName EnemyDefinitionId,
	const bool bEnemy,
	const EGameXXKBattleAnimationAction Action)
{
	const FName AuthoritativeUnitId = bEnemy && !EnemyDefinitionId.IsNone()
		? EnemyDefinitionId
		: RuntimeUnitId;
	return ResolveClip(AuthoritativeUnitId, bEnemy, Action);
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

TArray<FGameXXKBattlePresentationEvent> FGameXXKBattleAnimationPresentation::BuildPresentationEvents(
	const FGameXXKCardBattleRuntime& PostDamageBattle,
	const FName IgnoredFallbackAttackerUnitId,
	const TArray<FGameXXKCardDamageResult>& DamageResults)
{
	(void)IgnoredFallbackAttackerUnitId;
	TMap<FName, int32> FinalLethalTransitionByTarget;
	for (int32 Index = 0; Index < DamageResults.Num(); ++Index)
	{
		const FGameXXKCardDamageResult& Damage = DamageResults[Index];
		const FName TargetUnitId = Damage.ResolvedTargetUnitId.IsNone()
			? Damage.OriginalTargetUnitId
			: Damage.ResolvedTargetUnitId;
		if (!TargetUnitId.IsNone() && Damage.TargetHealthBefore > 0 && Damage.TargetHealthAfter <= 0)
		{
			FinalLethalTransitionByTarget.Add(TargetUnitId, Index);
		}
	}

	const auto FindUnit = [&PostDamageBattle](const FName UnitId) -> const FGameXXKCardCombatUnit*
	{
		return PostDamageBattle.Units.FindByPredicate([UnitId](const FGameXXKCardCombatUnit& Unit)
		{
			return Unit.UnitId == UnitId;
		});
	};

	TArray<FGameXXKBattlePresentationEvent> Events;
	Events.Reserve(DamageResults.Num());
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

		FGameXXKBattlePresentationEvent& Event = Events.AddDefaulted_GetRef();
		Event.EventId = static_cast<uint64>(Index) + 1;
		Event.HitOrdinal = Index;
		Event.AttackerUnitId = Damage.SourceUnitId;
		Event.TargetUnitId = TargetUnitId;
		if (const FGameXXKCardCombatUnit* Attacker = FindUnit(Event.AttackerUnitId))
		{
			Event.bAttackerEnemy = Attacker->Side == EGameXXKCardTargetSide::Enemy;
		}
		if (const FGameXXKCardCombatUnit* Target = FindUnit(TargetUnitId))
		{
			Event.bTargetEnemy = Target->Side == EGameXXKCardTargetSide::Enemy;
		}
		Event.HealthDamage = Damage.HealthDamage;
		Event.TargetHealthBefore = Damage.TargetHealthBefore;
		Event.TargetHealthAfter = Damage.TargetHealthAfter;
		Event.bAvoided = Damage.bAvoidedByAgility;
		const int32* FinalLethalIndex = FinalLethalTransitionByTarget.Find(TargetUnitId);
		Event.bTargetDefeated = FinalLethalIndex && *FinalLethalIndex == Index;
	}
	return Events;
}

TArray<FGameXXKBattleStatusPresentationEvent> FGameXXKBattleAnimationPresentation::BuildStatusPresentationEvents(
	const FGameXXKCardBattleRuntime& BeforeBattle,
	const FGameXXKCardBattleRuntime& AfterBattle)
{
	TMap<FName, FStatusPresentationUnitSnapshot> SnapshotByUnitId;
	CaptureStatusSnapshot(BeforeBattle, false, SnapshotByUnitId);
	CaptureStatusSnapshot(AfterBattle, true, SnapshotByUnitId);

	TArray<FStatusPresentationUnitSnapshot> OrderedSnapshots;
	SnapshotByUnitId.GenerateValueArray(OrderedSnapshots);
	OrderedSnapshots.Sort([](
		const FStatusPresentationUnitSnapshot& Left,
		const FStatusPresentationUnitSnapshot& Right)
	{
		const int32 LeftOrder = Left.StableSortOrder == INDEX_NONE ? MAX_int32 : Left.StableSortOrder;
		const int32 RightOrder = Right.StableSortOrder == INDEX_NONE ? MAX_int32 : Right.StableSortOrder;
		if (LeftOrder != RightOrder)
		{
			return LeftOrder < RightOrder;
		}
		return Left.UnitId.LexicalLess(Right.UnitId);
	});

	TArray<FGameXXKBattleStatusPresentationEvent> Events;
	for (const FStatusPresentationUnitSnapshot& Snapshot : OrderedSnapshots)
	{
		TArray<EGameXXKCardStatus> OrderedStatuses;
		Snapshot.BeforeStacks.GenerateKeyArray(OrderedStatuses);
		for (const TPair<EGameXXKCardStatus, int64>& Pair : Snapshot.AfterStacks)
		{
			OrderedStatuses.AddUnique(Pair.Key);
		}
		OrderedStatuses.Sort([](const EGameXXKCardStatus Left, const EGameXXKCardStatus Right)
		{
			return static_cast<uint8>(Left) < static_cast<uint8>(Right);
		});

		for (const EGameXXKCardStatus Status : OrderedStatuses)
		{
			const int64 BeforeStacks = Snapshot.BeforeStacks.FindRef(Status);
			const int64 AfterStacks = Snapshot.AfterStacks.FindRef(Status);
			const int64 StackDelta = AfterStacks - BeforeStacks;
			if (StackDelta == 0)
			{
				continue;
			}

			FGameXXKBattleStatusPresentationEvent& Event = Events.AddDefaulted_GetRef();
			Event.EventId = static_cast<uint64>(Events.Num());
			Event.UnitId = Snapshot.UnitId;
			Event.bUnitEnemy = Snapshot.Side == EGameXXKCardTargetSide::Enemy;
			Event.Status = Status;
			Event.StackBefore = ClampStatusStackValue(BeforeStacks);
			Event.StackAfter = ClampStatusStackValue(AfterStacks);
			Event.StackDelta = ClampStatusStackValue(StackDelta);
			Event.AnimationAction = StackDelta > 0
				? EGameXXKBattleAnimationAction::Buff
				: EGameXXKBattleAnimationAction::Debuff;
		}
	}
	return Events;
}

TArray<FGameXXKBattleAnimationCombatRequest> FGameXXKBattleAnimationPresentation::BuildCombatRequests(
	const FGameXXKCardBattleRuntime& PostDamageBattle,
	const FName FallbackAttackerUnitId,
	const TArray<FGameXXKCardDamageResult>& DamageResults)
{
	const TArray<FGameXXKBattlePresentationEvent> Events = BuildPresentationEvents(
		PostDamageBattle,
		FallbackAttackerUnitId,
		DamageResults);
	TArray<FGameXXKBattleAnimationCombatRequest> Requests;
	Requests.Reserve(Events.Num());
	for (const FGameXXKBattlePresentationEvent& Event : Events)
	{
		FGameXXKBattleAnimationCombatRequest& Request = Requests.AddDefaulted_GetRef();
		Request.AttackerUnitId = Event.AttackerUnitId.IsNone()
			? FallbackAttackerUnitId
			: Event.AttackerUnitId;
		Request.TargetUnitId = Event.TargetUnitId;
		Request.bAttackerEnemy = Event.bAttackerEnemy;
		if (Event.AttackerUnitId.IsNone() && !FallbackAttackerUnitId.IsNone())
		{
			if (const FGameXXKCardCombatUnit* const LegacyAttacker =
				PostDamageBattle.Units.FindByPredicate([FallbackAttackerUnitId](const FGameXXKCardCombatUnit& Unit)
				{
					return Unit.UnitId == FallbackAttackerUnitId;
				}))
			{
				Request.bAttackerEnemy = LegacyAttacker->Side == EGameXXKCardTargetSide::Enemy;
			}
		}
		Request.bTargetEnemy = Event.bTargetEnemy;
		Request.bTargetDefeated = Event.bTargetDefeated;
	}
	return Requests;
}

int32 FGameXXKBattleAnimationPresentation::CalculateFrameIndex(
	const FGameXXKBattleAnimationClipDescriptor& Clip,
	const float RuntimeElapsedSeconds,
	const bool bLooping)
{
	if (!Clip.IsValid() || !FMath::IsFinite(RuntimeElapsedSeconds))
	{
		return 0;
	}

	const double SafeElapsedSeconds = FMath::Max(0.0, static_cast<double>(RuntimeElapsedSeconds));
	const double UnboundedFrame = FMath::FloorToDouble(
		SafeElapsedSeconds
		* static_cast<double>(Clip.PlaybackRate)
		* static_cast<double>(Clip.SourceFramesPerSecond));
	if (!FMath::IsFinite(UnboundedFrame))
	{
		return 0;
	}

	const double LastFrame = static_cast<double>(Clip.FrameCount - 1);
	const double SafeFrame = bLooping
		? FMath::Clamp(FMath::Fmod(UnboundedFrame, static_cast<double>(Clip.FrameCount)), 0.0, LastFrame)
		: FMath::Clamp(UnboundedFrame, 0.0, LastFrame);
	return static_cast<int32>(SafeFrame);
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
