#include "GameXXKCardOutcomePreview.h"

#include "GameXXKBattlePresentation.h"
#include "GameXXKCardBattleAdapter.h"

namespace
{
	struct FEnemySlotSnapshot
	{
		FName UnitId = NAME_None;
		int32 SlotNumber = INDEX_NONE;
	};

	struct FTargetAggregate
	{
		FGameXXKCardOutcomeTarget Target;
		bool bDirectAttempt = false;
		bool bGroupAttempt = false;
		bool bBleedAttempt = false;
		bool bPoisonAttempt = false;
		bool bBurnAttempt = false;
		bool bToxicAttempt = false;
		bool bMedicineAttempt = false;
		bool bLinkedAttempt = false;
		bool bHealingAttempt = false;
		bool bArmorAttempt = false;
	};

	enum class EDamageBucket : uint8
	{
		None,
		Direct,
		Group,
		Bleed,
		Poison,
		Burn,
		Toxic,
		Medicine,
		Linked
	};

	const FGameXXKCardCombatUnit* FindCombatUnitByStableId(
		const TArray<FGameXXKCardCombatUnit>& Units,
		const FName UnitId)
	{
		if (UnitId.IsNone())
		{
			return nullptr;
		}
		return Units.FindByPredicate([UnitId](const FGameXXKCardCombatUnit& Unit)
		{
			return Unit.UnitId == UnitId;
		});
	}

	bool SetError(FString* OutError, const FString& Error)
	{
		if (OutError)
		{
			*OutError = Error;
		}
		return false;
	}

	bool SnapshotLivingEnemySlots(
		const FGameXXKCardBattleRuntime& Runtime,
		TArray<FEnemySlotSnapshot>& OutSlots,
		FString* OutError)
	{
		OutSlots.Reset();
		TSet<int32> SeenSlots;
		for (const FGameXXKCardCombatUnit& Unit : Runtime.Units)
		{
			if (Unit.Side != EGameXXKCardTargetSide::Enemy || !Unit.bLiving)
			{
				continue;
			}
			const int32 SlotNumber = FGameXXKBattlePresentation::GetSlotNumber(Runtime, Unit.UnitId);
			if (SlotNumber < 1 || SlotNumber > 3)
			{
				OutSlots.Reset();
				return SetError(OutError, TEXT("A living enemy has no valid 1P, 2P, or 3P presentation slot."));
			}
			if (SeenSlots.Contains(SlotNumber))
			{
				OutSlots.Reset();
				return SetError(OutError, TEXT("Living enemies have duplicate presentation slots."));
			}
			SeenSlots.Add(SlotNumber);
			FEnemySlotSnapshot& Snapshot = OutSlots.AddDefaulted_GetRef();
			Snapshot.UnitId = Unit.UnitId;
			Snapshot.SlotNumber = SlotNumber;
		}
		OutSlots.Sort([](const FEnemySlotSnapshot& Left, const FEnemySlotSnapshot& Right)
		{
			return Left.SlotNumber < Right.SlotNumber;
		});
		return true;
	}

	void ResetAsFailure(
		FGameXXKCardOutcomePreview& OutPreview,
		const FName CardInstanceId,
		const FName HoveredTargetUnitId)
	{
		OutPreview = FGameXXKCardOutcomePreview();
		OutPreview.CardInstanceId = CardInstanceId;
		OutPreview.HoveredTargetUnitId = HoveredTargetUnitId;
		OutPreview.FailureText = TEXT("无法预演");
	}

	int32 AddActual(const int32 Current, const int32 Delta)
	{
		return static_cast<int32>(FMath::Min<int64>(
			MAX_int32,
			static_cast<int64>(Current) + FMath::Max(0, Delta)));
	}

	EDamageBucket GetDamageBucket(const FGameXXKCardDamageResult& Packet)
	{
		if (Packet.Cause == EGameXXKCardDamageCause::DirectAttack)
		{
			if (Packet.Kind == EGameXXKCardDamageKind::SingleTargetAttack)
			{
				return EDamageBucket::Direct;
			}
			if (Packet.Kind == EGameXXKCardDamageKind::GroupAttack)
			{
				return EDamageBucket::Group;
			}
		}
		switch (Packet.Cause)
		{
		case EGameXXKCardDamageCause::Bleed:
			return EDamageBucket::Bleed;
		case EGameXXKCardDamageCause::Poison:
			return EDamageBucket::Poison;
		case EGameXXKCardDamageCause::Burn:
			return EDamageBucket::Burn;
		case EGameXXKCardDamageCause::ToxicExplosionBleed:
		case EGameXXKCardDamageCause::ToxicExplosionPoison:
		case EGameXXKCardDamageCause::ToxicExplosionBurn:
			return EDamageBucket::Toxic;
		case EGameXXKCardDamageCause::Medicine:
			return EDamageBucket::Medicine;
		case EGameXXKCardDamageCause::Relic:
		case EGameXXKCardDamageCause::Counter:
		case EGameXXKCardDamageCause::Block:
			return EDamageBucket::Linked;
		case EGameXXKCardDamageCause::SelfLoss:
		case EGameXXKCardDamageCause::Environment:
		case EGameXXKCardDamageCause::Rot:
		case EGameXXKCardDamageCause::Invalid:
		default:
			return EDamageBucket::None;
		}
	}

	void MarkDamageAttempt(FTargetAggregate& Aggregate, const EDamageBucket Bucket)
	{
		switch (Bucket)
		{
		case EDamageBucket::Direct: Aggregate.bDirectAttempt = true; break;
		case EDamageBucket::Group: Aggregate.bGroupAttempt = true; break;
		case EDamageBucket::Bleed: Aggregate.bBleedAttempt = true; break;
		case EDamageBucket::Poison: Aggregate.bPoisonAttempt = true; break;
		case EDamageBucket::Burn: Aggregate.bBurnAttempt = true; break;
		case EDamageBucket::Toxic: Aggregate.bToxicAttempt = true; break;
		case EDamageBucket::Medicine: Aggregate.bMedicineAttempt = true; break;
		case EDamageBucket::Linked: Aggregate.bLinkedAttempt = true; break;
		case EDamageBucket::None: break;
		}
	}

	void AddDamage(FTargetAggregate& Aggregate, const FGameXXKCardDamageResult& Packet)
	{
		const EDamageBucket Bucket = GetDamageBucket(Packet);
		if (Bucket == EDamageBucket::None)
		{
			return;
		}
		MarkDamageAttempt(Aggregate, Bucket);
		switch (Bucket)
		{
		case EDamageBucket::Direct:
			Aggregate.Target.DirectDamage = AddActual(Aggregate.Target.DirectDamage, Packet.HealthDamage);
			break;
		case EDamageBucket::Group:
			Aggregate.Target.GroupDamage = AddActual(Aggregate.Target.GroupDamage, Packet.HealthDamage);
			break;
		case EDamageBucket::Bleed:
			Aggregate.Target.BleedDamage = AddActual(Aggregate.Target.BleedDamage, Packet.HealthDamage);
			break;
		case EDamageBucket::Poison:
			Aggregate.Target.PoisonDamage = AddActual(Aggregate.Target.PoisonDamage, Packet.HealthDamage);
			break;
		case EDamageBucket::Burn:
			Aggregate.Target.BurnDamage = AddActual(Aggregate.Target.BurnDamage, Packet.HealthDamage);
			break;
		case EDamageBucket::Toxic:
			Aggregate.Target.ToxicExplosionDamage = AddActual(Aggregate.Target.ToxicExplosionDamage, Packet.HealthDamage);
			break;
		case EDamageBucket::Medicine:
			Aggregate.Target.MedicineDamage = AddActual(Aggregate.Target.MedicineDamage, Packet.HealthDamage);
			break;
		case EDamageBucket::Linked:
			Aggregate.Target.LinkedDamage = AddActual(Aggregate.Target.LinkedDamage, Packet.HealthDamage);
			break;
		case EDamageBucket::None:
			break;
		}
		Aggregate.Target.bAvoided |= Packet.bAvoidedByAgility;
		Aggregate.Target.bRedirected |= Packet.bRedirected;
		Aggregate.Target.bLethal |= Packet.TargetHealthBefore > 0 && Packet.TargetHealthAfter <= 0;
	}

	FTargetAggregate MakeAggregate(
		const FGameXXKCardBattleRuntime& Runtime,
		const FGameXXKCardCombatUnit& Unit)
	{
		FTargetAggregate Aggregate;
		Aggregate.Target.UnitId = Unit.UnitId;
		Aggregate.Target.Side = Unit.Side;
		Aggregate.Target.SlotNumber = FGameXXKBattlePresentation::GetSlotNumber(Runtime, Unit.UnitId);
		return Aggregate;
	}

	FTargetAggregate* FindOrAddActualAggregate(
		TMap<FName, FTargetAggregate>& Aggregates,
		const FGameXXKCardBattleRuntime& RuntimeBefore,
		const FName TargetUnitId)
	{
		if (TargetUnitId.IsNone())
		{
			return nullptr;
		}
		if (FTargetAggregate* Existing = Aggregates.Find(TargetUnitId))
		{
			return Existing;
		}
		const FGameXXKCardCombatUnit* Unit = FindCombatUnitByStableId(RuntimeBefore.Units, TargetUnitId);
		if (!Unit)
		{
			return nullptr;
		}
		return &Aggregates.Add(TargetUnitId, MakeAggregate(RuntimeBefore, *Unit));
	}

	void ApplyOriginalAttackAttempt(
		FTargetAggregate& Aggregate,
		const FGameXXKCardDamageResult& Packet,
		const FName OriginalTargetUnitId)
	{
		if (Packet.OriginalTargetUnitId != OriginalTargetUnitId
			|| (Packet.Kind != EGameXXKCardDamageKind::SingleTargetAttack
				&& Packet.Kind != EGameXXKCardDamageKind::GroupAttack))
		{
			return;
		}
		MarkDamageAttempt(Aggregate, GetDamageBucket(Packet));
		Aggregate.Target.bAvoided |= Packet.bAvoidedByAgility;
		Aggregate.Target.bRedirected |= Packet.bRedirected;
	}

	void AddSegment(
		FGameXXKCardOutcomeTextLine& Line,
		const FString& Text,
		const EGameXXKCardOutcomeTone Tone)
	{
		FGameXXKCardOutcomeTextSegment& Segment = Line.Segments.AddDefaulted_GetRef();
		Segment.Text = FText::FromString(Text);
		Segment.Tone = Tone;
	}

	int32 GetDamageCategoryCount(const FTargetAggregate& Aggregate)
	{
		return (Aggregate.bDirectAttempt ? 1 : 0)
			+ (Aggregate.bGroupAttempt ? 1 : 0)
			+ (Aggregate.bBleedAttempt ? 1 : 0)
			+ (Aggregate.bPoisonAttempt ? 1 : 0)
			+ (Aggregate.bBurnAttempt ? 1 : 0)
			+ (Aggregate.bToxicAttempt ? 1 : 0)
			+ (Aggregate.bMedicineAttempt ? 1 : 0)
			+ (Aggregate.bLinkedAttempt ? 1 : 0);
	}

	int32 GetTotalDamage(const FGameXXKCardOutcomeTarget& Target)
	{
		const int64 Total = static_cast<int64>(Target.DirectDamage)
			+ Target.GroupDamage
			+ Target.BleedDamage
			+ Target.PoisonDamage
			+ Target.BurnDamage
			+ Target.ToxicExplosionDamage
			+ Target.MedicineDamage
			+ Target.LinkedDamage;
		return static_cast<int32>(FMath::Min<int64>(MAX_int32, Total));
	}

	void AppendValueSegments(
		const FTargetAggregate& Aggregate,
		FGameXXKCardOutcomeTextLine& Line)
	{
		if (Aggregate.bDirectAttempt)
		{
			AddSegment(Line, FString::Printf(TEXT("伤害 %d"), Aggregate.Target.DirectDamage), EGameXXKCardOutcomeTone::Damage);
		}
		if (Aggregate.bGroupAttempt)
		{
			AddSegment(Line, FString::Printf(TEXT("群体伤害 %d"), Aggregate.Target.GroupDamage), EGameXXKCardOutcomeTone::Damage);
		}
		if (Aggregate.bBleedAttempt)
		{
			AddSegment(Line, FString::Printf(TEXT("流血 %d"), Aggregate.Target.BleedDamage), EGameXXKCardOutcomeTone::Dot);
		}
		if (Aggregate.bPoisonAttempt)
		{
			AddSegment(Line, FString::Printf(TEXT("中毒 %d"), Aggregate.Target.PoisonDamage), EGameXXKCardOutcomeTone::Dot);
		}
		if (Aggregate.bBurnAttempt)
		{
			AddSegment(Line, FString::Printf(TEXT("灼烧 %d"), Aggregate.Target.BurnDamage), EGameXXKCardOutcomeTone::Dot);
		}
		if (Aggregate.bToxicAttempt)
		{
			AddSegment(Line, FString::Printf(TEXT("毒爆 %d"), Aggregate.Target.ToxicExplosionDamage), EGameXXKCardOutcomeTone::Dot);
		}
		if (Aggregate.bMedicineAttempt)
		{
			AddSegment(Line, FString::Printf(TEXT("药效伤害 %d"), Aggregate.Target.MedicineDamage), EGameXXKCardOutcomeTone::Medicine);
		}
		if (Aggregate.bLinkedAttempt)
		{
			AddSegment(Line, FString::Printf(TEXT("联动伤害 %d"), Aggregate.Target.LinkedDamage), EGameXXKCardOutcomeTone::Damage);
		}
		if (Aggregate.bHealingAttempt)
		{
			AddSegment(Line, FString::Printf(TEXT("治疗 +%d"), Aggregate.Target.EffectiveHealing), EGameXXKCardOutcomeTone::Healing);
		}
		if (Aggregate.bArmorAttempt)
		{
			AddSegment(Line, FString::Printf(TEXT("护甲 +%d"), Aggregate.Target.EffectiveArmor), EGameXXKCardOutcomeTone::Armor);
		}
	}

	void AppendOutcomeFlags(
		const FTargetAggregate& Aggregate,
		FGameXXKCardOutcomeTextLine& Line)
	{
		if (Aggregate.Target.bRedirected)
		{
			AddSegment(Line, TEXT("已改向"), EGameXXKCardOutcomeTone::Neutral);
		}
		if (Aggregate.Target.bLethal)
		{
			AddSegment(Line, TEXT("致死"), EGameXXKCardOutcomeTone::Lethal);
		}
	}

	void AppendValueLines(
		const FTargetAggregate& Aggregate,
		TArray<FGameXXKCardOutcomeTextLine>& Lines)
	{
		// Mixed damage types stack into one row per type instead of running
		// side by side in a single line: direct/physical damage first, then the
		// damage-over-time row, then medicine, healing and armor rows.
		FGameXXKCardOutcomeTextLine* DamageLine = nullptr;
		if (Aggregate.bDirectAttempt || Aggregate.bGroupAttempt || Aggregate.bLinkedAttempt)
		{
			DamageLine = &Lines.AddDefaulted_GetRef();
		}
		if (Aggregate.bDirectAttempt)
		{
			AddSegment(*DamageLine, FString::Printf(TEXT("伤害 %d"), Aggregate.Target.DirectDamage), EGameXXKCardOutcomeTone::Damage);
		}
		if (Aggregate.bGroupAttempt)
		{
			AddSegment(*DamageLine, FString::Printf(TEXT("群体伤害 %d"), Aggregate.Target.GroupDamage), EGameXXKCardOutcomeTone::Damage);
		}
		if (Aggregate.bLinkedAttempt)
		{
			AddSegment(*DamageLine, FString::Printf(TEXT("联动伤害 %d"), Aggregate.Target.LinkedDamage), EGameXXKCardOutcomeTone::Damage);
		}

		FGameXXKCardOutcomeTextLine* DotLine = nullptr;
		if (Aggregate.bBleedAttempt || Aggregate.bPoisonAttempt || Aggregate.bBurnAttempt || Aggregate.bToxicAttempt)
		{
			DotLine = &Lines.AddDefaulted_GetRef();
		}
		if (Aggregate.bBleedAttempt)
		{
			AddSegment(*DotLine, FString::Printf(TEXT("流血 %d"), Aggregate.Target.BleedDamage), EGameXXKCardOutcomeTone::Dot);
		}
		if (Aggregate.bPoisonAttempt)
		{
			AddSegment(*DotLine, FString::Printf(TEXT("中毒 %d"), Aggregate.Target.PoisonDamage), EGameXXKCardOutcomeTone::Dot);
		}
		if (Aggregate.bBurnAttempt)
		{
			AddSegment(*DotLine, FString::Printf(TEXT("灼烧 %d"), Aggregate.Target.BurnDamage), EGameXXKCardOutcomeTone::Dot);
		}
		if (Aggregate.bToxicAttempt)
		{
			AddSegment(*DotLine, FString::Printf(TEXT("毒爆 %d"), Aggregate.Target.ToxicExplosionDamage), EGameXXKCardOutcomeTone::Dot);
		}

		if (Aggregate.bMedicineAttempt)
		{
			AddSegment(Lines.AddDefaulted_GetRef(), FString::Printf(TEXT("药效伤害 %d"), Aggregate.Target.MedicineDamage), EGameXXKCardOutcomeTone::Medicine);
		}
		if (Aggregate.bHealingAttempt)
		{
			AddSegment(Lines.AddDefaulted_GetRef(), FString::Printf(TEXT("治疗 +%d"), Aggregate.Target.EffectiveHealing), EGameXXKCardOutcomeTone::Healing);
		}
		if (Aggregate.bArmorAttempt)
		{
			AddSegment(Lines.AddDefaulted_GetRef(), FString::Printf(TEXT("护甲 +%d"), Aggregate.Target.EffectiveArmor), EGameXXKCardOutcomeTone::Armor);
		}
	}

	TArray<FGameXXKCardOutcomeTextLine> BuildFocusedLines(const FTargetAggregate& Aggregate)
	{
		TArray<FGameXXKCardOutcomeTextLine> Lines;
		const int32 DamageCategoryCount = GetDamageCategoryCount(Aggregate);
		if (DamageCategoryCount == 0 && !Aggregate.bHealingAttempt && !Aggregate.bArmorAttempt)
		{
			return Lines;
		}
		AppendValueLines(Aggregate, Lines);
		if (DamageCategoryCount >= 2)
		{
			FGameXXKCardOutcomeTextLine& TotalLine = Lines.AddDefaulted_GetRef();
			AddSegment(
				TotalLine,
				FString::Printf(TEXT("合计 %d"), GetTotalDamage(Aggregate.Target)),
				EGameXXKCardOutcomeTone::Neutral);
			AppendOutcomeFlags(Aggregate, TotalLine);
		}
		else if (!Lines.IsEmpty())
		{
			AppendOutcomeFlags(Aggregate, Lines.Last());
		}
		return Lines;
	}

	FGameXXKCardOutcomeTextLine BuildEnemyPositionLine(const FTargetAggregate& Aggregate)
	{
		FGameXXKCardOutcomeTextLine Line;
		AddSegment(Line, FString::Printf(TEXT("%dP"), Aggregate.Target.SlotNumber), EGameXXKCardOutcomeTone::Neutral);
		AppendValueSegments(Aggregate, Line);
		if (GetDamageCategoryCount(Aggregate) >= 2)
		{
			AddSegment(
				Line,
				FString::Printf(TEXT("合计 %d"), GetTotalDamage(Aggregate.Target)),
				EGameXXKCardOutcomeTone::Neutral);
		}
		AppendOutcomeFlags(Aggregate, Line);
		return Line;
	}
}

#if WITH_DEV_AUTOMATION_TESTS
namespace GameXXKCardOutcomePreviewTestBridge
{
	FGameXXKCardOutcomeTarget AggregateDamagePackets(
		const TArray<FGameXXKCardDamageResult>& Packets)
	{
		FTargetAggregate Aggregate;
		for (const FGameXXKCardDamageResult& Packet : Packets)
		{
			AddDamage(Aggregate, Packet);
		}
		return Aggregate.Target;
	}
}
#endif

bool FGameXXKCardOutcomePreviewRules::Build(
	const FGameXXKRuntimeState& State,
	const FName CardInstanceId,
	const FName HoveredTargetUnitId,
	FGameXXKCardOutcomePreview& OutPreview,
	FString* OutError)
{
	OutPreview = FGameXXKCardOutcomePreview();
	OutPreview.CardInstanceId = CardInstanceId;
	OutPreview.HoveredTargetUnitId = HoveredTargetUnitId;

	FGameXXKCardPlayPreview Playability;
	if (!FGameXXKCardBattleAdapter::BuildCardPlayPreview(
			State, CardInstanceId, Playability, OutError)
		|| !Playability.bCanPlay)
	{
		if (Playability.bCanPlay == false && OutError && OutError->IsEmpty() && !Playability.FailureReason.IsEmpty())
		{
			*OutError = Playability.FailureReason;
		}
		ResetAsFailure(OutPreview, CardInstanceId, HoveredTargetUnitId);
		return false;
	}

	FGameXXKRuntimeState WorkingState = State;
	TArray<FEnemySlotSnapshot> EnemySlots;
	if (!SnapshotLivingEnemySlots(State.CardRun.ActiveBattle, EnemySlots, OutError))
	{
		ResetAsFailure(OutPreview, CardInstanceId, HoveredTargetUnitId);
		return false;
	}
	const FGameXXKCardCombatUnit* HoveredBefore = HoveredTargetUnitId.IsNone()
		? nullptr
		: FindCombatUnitByStableId(State.CardRun.ActiveBattle.Units, HoveredTargetUnitId);

	FGameXXKCardPlayResult PlayResult;
	if (!FGameXXKCardBattleAdapter::ResolveCardPlay(
			WorkingState,
			CardInstanceId,
			HoveredTargetUnitId,
			PlayResult,
			OutError))
	{
		ResetAsFailure(OutPreview, CardInstanceId, HoveredTargetUnitId);
		return false;
	}

	const bool bHasGroupPacket = PlayResult.DamageResults.ContainsByPredicate(
		[](const FGameXXKCardDamageResult& Packet)
		{
			return Packet.Kind == EGameXXKCardDamageKind::GroupAttack;
		});
	if (Playability.TargetRequest.bRequiresManualSelection)
	{
		OutPreview.Classification = EGameXXKCardOutcomePreviewClass::ManualUnit;
	}
	else if (bHasGroupPacket
		&& Playability.TargetRequest.EffectiveMode == EGameXXKCardTargetMode::AllEnemies)
	{
		OutPreview.Classification = EGameXXKCardOutcomePreviewClass::PureEnemyGroup;
	}
	else
	{
		OutPreview.Classification = EGameXXKCardOutcomePreviewClass::None;
	}
	OutPreview.bUsesEnemyPositionList = bHasGroupPacket;

	TMap<FName, FTargetAggregate> ActualAggregates;
	for (const FGameXXKCardDamageResult& Packet : PlayResult.DamageResults)
	{
		if (FTargetAggregate* Aggregate = FindOrAddActualAggregate(
			ActualAggregates, State.CardRun.ActiveBattle, Packet.ResolvedTargetUnitId))
		{
			AddDamage(*Aggregate, Packet);
		}
	}
	for (const FGameXXKCardHealingResult& Healing : PlayResult.HealingResults)
	{
		if (FTargetAggregate* Aggregate = FindOrAddActualAggregate(
			ActualAggregates, State.CardRun.ActiveBattle, Healing.TargetUnitId))
		{
			Aggregate->bHealingAttempt = true;
			Aggregate->Target.EffectiveHealing = AddActual(
				Aggregate->Target.EffectiveHealing, Healing.EffectiveHealing);
		}
	}
	for (const FGameXXKCardArmorResult& Armor : PlayResult.ArmorResults)
	{
		if (FTargetAggregate* Aggregate = FindOrAddActualAggregate(
			ActualAggregates, State.CardRun.ActiveBattle, Armor.TargetUnitId))
		{
			Aggregate->bArmorAttempt = true;
			Aggregate->Target.EffectiveArmor = AddActual(
				Aggregate->Target.EffectiveArmor, Armor.EffectiveArmor);
		}
	}

	if (Playability.TargetRequest.bRequiresManualSelection
		&& HoveredBefore
		&& !(bHasGroupPacket && HoveredBefore->Side == EGameXXKCardTargetSide::Enemy))
	{
		FTargetAggregate Focused = MakeAggregate(State.CardRun.ActiveBattle, *HoveredBefore);
		if (HoveredBefore->Side == EGameXXKCardTargetSide::Party)
		{
			for (const FGameXXKCardHealingResult& Healing : PlayResult.HealingResults)
			{
				if (Healing.TargetUnitId == HoveredTargetUnitId)
				{
					Focused.bHealingAttempt = true;
					Focused.Target.EffectiveHealing = AddActual(
						Focused.Target.EffectiveHealing, Healing.EffectiveHealing);
				}
			}
			for (const FGameXXKCardArmorResult& Armor : PlayResult.ArmorResults)
			{
				if (Armor.TargetUnitId == HoveredTargetUnitId)
				{
					Focused.bArmorAttempt = true;
					Focused.Target.EffectiveArmor = AddActual(
						Focused.Target.EffectiveArmor, Armor.EffectiveArmor);
				}
			}
		}
		else
		{
			if (const FTargetAggregate* Actual = ActualAggregates.Find(HoveredTargetUnitId))
			{
				Focused = *Actual;
			}
			for (const FGameXXKCardDamageResult& Packet : PlayResult.DamageResults)
			{
				ApplyOriginalAttackAttempt(Focused, Packet, HoveredTargetUnitId);
			}
		}
		OutPreview.FocusedTarget = Focused.Target;
		OutPreview.FocusedLines = BuildFocusedLines(Focused);
	}

	if (bHasGroupPacket)
	{
		for (const FEnemySlotSnapshot& Slot : EnemySlots)
		{
			const FGameXXKCardCombatUnit* Unit = FindCombatUnitByStableId(
				State.CardRun.ActiveBattle.Units, Slot.UnitId);
			if (!Unit)
			{
				ResetAsFailure(OutPreview, CardInstanceId, HoveredTargetUnitId);
				SetError(OutError, TEXT("A snapshotted enemy disappeared before aggregation."));
				return false;
			}
			FTargetAggregate Aggregate = MakeAggregate(State.CardRun.ActiveBattle, *Unit);
			if (const FTargetAggregate* Actual = ActualAggregates.Find(Slot.UnitId))
			{
				Aggregate = *Actual;
			}
			Aggregate.Target.SlotNumber = Slot.SlotNumber;
			Aggregate.bGroupAttempt = true;
			if (Slot.UnitId == HoveredTargetUnitId)
			{
				for (const FGameXXKCardDamageResult& Packet : PlayResult.DamageResults)
				{
					ApplyOriginalAttackAttempt(Aggregate, Packet, HoveredTargetUnitId);
				}
			}
			OutPreview.EnemyPositionTargets.Add(Aggregate.Target);
			OutPreview.EnemyPositionLines.Add(BuildEnemyPositionLine(Aggregate));
		}
	}

	OutPreview.bSuccess = true;
	return true;
}
