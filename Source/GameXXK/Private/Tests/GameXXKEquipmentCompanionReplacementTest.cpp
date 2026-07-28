#include "GameXXKCardBattleAdapter.h"
#include "GameXXKCompanionCatalog.h"
#include "GameXXKCompanionRules.h"
#include "GameXXKEquipmentRules.h"
#include "GameXXKMVPRules.h"
#include "MVP/GameXXKMVPSubsystem.h"

#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EGameXXKEquipmentSlot OrderedSlots[] = {
		EGameXXKEquipmentSlot::Weapon,
		EGameXXKEquipmentSlot::Head,
		EGameXXKEquipmentSlot::Armor,
		EGameXXKEquipmentSlot::Belt,
		EGameXXKEquipmentSlot::Shoes,
		EGameXXKEquipmentSlot::Accessory,
	};

	struct FReplacementFixture
	{
		UGameInstance* GameInstance = nullptr;
		UGameXXKMVPSubsystem* Subsystem = nullptr;
		FName DismissedInstanceId = NAME_None;
		FName SurvivingActiveInstanceId = NAME_None;
		FName PendingCandidateInstanceId = NAME_None;
		TArray<FName> EquippedInstanceIds;
	};

	TArray<uint8> SerializeRuntimeState(const FGameXXKRuntimeState& State)
	{
		TArray<uint8> Bytes;
		FMemoryWriter Writer(Bytes, true);
		FObjectAndNameAsStringProxyArchive Archive(Writer, false);
		FGameXXKRuntimeState Copy = State;
		FGameXXKRuntimeState::StaticStruct()->SerializeItem(Archive, &Copy, nullptr);
		return Bytes;
	}

	TArray<uint8> SerializeEquipmentCollection(const FGameXXKEquipmentCollectionState& Collection)
	{
		TArray<uint8> Bytes;
		FMemoryWriter Writer(Bytes, true);
		FObjectAndNameAsStringProxyArchive Archive(Writer, false);
		FGameXXKEquipmentCollectionState Copy = Collection;
		FGameXXKEquipmentCollectionState::StaticStruct()->SerializeItem(Archive, &Copy, nullptr);
		return Bytes;
	}

	bool BuildFullRoster(
		FAutomationTestBase& Test,
		const bool bDismissedCompanionIsActive,
		FGameXXKCompanionRosterState& OutRoster,
		FName& OutDismissedInstanceId,
		FName& OutSurvivingActiveInstanceId,
		FName& OutPendingCandidateInstanceId)
	{
		OutRoster = FGameXXKCompanionRosterState();
		const TArray<FGameXXKCompanionTemplateDefinition>& Templates = FGameXXKCompanionCatalog::GetRecruitTemplates();
		if (!Test.TestTrue(TEXT("replacement fixture has thirteen distinct recruit templates"), Templates.Num() >= 13))
		{
			return false;
		}

		for (int32 Index = 0; Index < FGameXXKCompanionRules::MaxPermanentCompanions; ++Index)
		{
			FGameXXKCompanionRecruitResult RecruitResult;
			FString Error;
			if (!Test.TestTrue(
				FString::Printf(TEXT("replacement fixture recruits permanent companion %d"), Index),
				FGameXXKCompanionRules::RecruitPermanentCompanion(
					OutRoster,
					Templates[Index].TemplateId,
					7000 + Index,
					RecruitResult,
					&Error)))
			{
				Test.AddError(Error);
				return false;
			}
			if (!Test.TestEqual(
				FString::Printf(TEXT("replacement fixture companion %d joins immediately"), Index),
				RecruitResult.Outcome,
				EGameXXKCompanionRecruitOutcome::Recruited))
			{
				return false;
			}
		}

		OutDismissedInstanceId = OutRoster.PermanentCompanions[0].InstanceId;
		OutSurvivingActiveInstanceId = OutRoster.PermanentCompanions[1].InstanceId;
		FString ActiveError;
		const FName InitialActiveInstanceId = bDismissedCompanionIsActive
			? OutDismissedInstanceId
			: OutSurvivingActiveInstanceId;
		if (!Test.TestTrue(
			TEXT("replacement fixture selects exactly one active companion"),
			FGameXXKCompanionRules::SetActivePermanentCompanion(OutRoster, InitialActiveInstanceId, &ActiveError)))
		{
			Test.AddError(ActiveError);
			return false;
		}

		FGameXXKCompanionRecruitResult PendingResult;
		FString PendingError;
		if (!Test.TestTrue(
			TEXT("replacement fixture persists the thirteenth recruit as the fixed candidate"),
			FGameXXKCompanionRules::RecruitPermanentCompanion(
				OutRoster,
				Templates[12].TemplateId,
				7012,
				PendingResult,
				&PendingError)))
		{
			Test.AddError(PendingError);
			return false;
		}
		if (!Test.TestEqual(
			TEXT("replacement fixture is at capacity and requires replacement"),
			PendingResult.Outcome,
			EGameXXKCompanionRecruitOutcome::PendingReplacement))
		{
			return false;
		}
		OutPendingCandidateInstanceId = PendingResult.Companion.InstanceId;
		return true;
	}

	bool CreateEquipmentFixture(
		FAutomationTestBase& Test,
		FReplacementFixture& Fixture,
		const int32 EquippedCount,
		const int32 WarehouseCount,
		const bool bDismissedCompanionIsActive)
	{
		Fixture = FReplacementFixture();
		if (!Test.TestTrue(TEXT("equipped fixture count is within the six-slot contract"), EquippedCount >= 0 && EquippedCount <= 6)
			|| !Test.TestTrue(TEXT("warehouse fixture count is within the normal capacity"), WarehouseCount >= 0 && WarehouseCount <= 200))
		{
			return false;
		}

		Fixture.GameInstance = NewObject<UGameInstance>();
		Fixture.Subsystem = NewObject<UGameXXKMVPSubsystem>(Fixture.GameInstance);
		if (!Test.TestNotNull(TEXT("replacement fixture subsystem exists"), Fixture.Subsystem))
		{
			return false;
		}

		FGameXXKRuntimeState& State = Fixture.Subsystem->GetMutableRuntimeState();
		State = FGameXXKRuntimeState();
		State.Screen = EGameXXKScreen::Town;
		if (!BuildFullRoster(
			Test,
			bDismissedCompanionIsActive,
			State.CardRun.CompanionRoster,
			Fixture.DismissedInstanceId,
			Fixture.SurvivingActiveInstanceId,
			Fixture.PendingCandidateInstanceId))
		{
			return false;
		}

		FString CardRunError;
		if (!Test.TestTrue(
			TEXT("replacement fixture has a valid synchronized card run"),
			FGameXXKCardBattleAdapter::EnsureCardRunInitialized(State, &CardRunError)))
		{
			Test.AddError(CardRunError);
			return false;
		}

		for (int32 SlotIndex = 0; SlotIndex < EquippedCount; ++SlotIndex)
		{
			FGameXXKEquipmentCreateRequest Request;
			Request.Set = EGameXXKEquipmentSet::PoJun;
			Request.Quality = EGameXXKEquipmentQuality::Common;
			Request.ItemLevel = 1;
			Request.bForceSlot = true;
			Request.ForcedSlot = OrderedSlots[SlotIndex];
			FName InstanceId;
			FString CreateError;
			if (!Test.TestTrue(
				FString::Printf(TEXT("replacement fixture creates equipped slot %d"), SlotIndex),
				FGameXXKEquipmentRules::CreateRolledInstance(State.EquipmentCollection, Request, InstanceId, &CreateError)))
			{
				Test.AddError(CreateError);
				return false;
			}
			const FGameXXKEquipmentTransactionResult EquipResult = FGameXXKEquipmentRules::EquipInstance(
				State.EquipmentCollection,
				State.CardRun.CompanionRoster,
				Fixture.DismissedInstanceId,
				OrderedSlots[SlotIndex],
				InstanceId);
			if (!Test.TestTrue(
				FString::Printf(TEXT("replacement fixture equips slot %d on the dismissed companion"), SlotIndex),
				EquipResult.bSucceeded))
			{
				return false;
			}
			Fixture.EquippedInstanceIds.Add(InstanceId);
		}

		for (int32 WarehouseIndex = 0; WarehouseIndex < WarehouseCount; ++WarehouseIndex)
		{
			FGameXXKEquipmentCreateRequest Request;
			Request.Set = EGameXXKEquipmentSet::PoJun;
			Request.Quality = EGameXXKEquipmentQuality::Common;
			Request.ItemLevel = 1;
			Request.bForceSlot = true;
			Request.ForcedSlot = OrderedSlots[WarehouseIndex % UE_ARRAY_COUNT(OrderedSlots)];
			FName IgnoredInstanceId;
			FString CreateError;
			if (!FGameXXKEquipmentRules::CreateRolledInstance(
				State.EquipmentCollection,
				Request,
				IgnoredInstanceId,
				&CreateError))
			{
				Test.AddError(FString::Printf(
					TEXT("replacement fixture could not create warehouse item %d: %s"),
					WarehouseIndex,
					*CreateError));
				return false;
			}
		}

		FString CollectionError;
		return Test.TestTrue(
			TEXT("replacement fixture central collection matches the full roster"),
			FGameXXKEquipmentRules::ValidateCollectionAgainstRoster(
				State.EquipmentCollection,
				State.CardRun.CompanionRoster,
				&CollectionError));
	}

	void TestTransactionFailure(
		FAutomationTestBase& Test,
		const FString& Label,
		FReplacementFixture& Fixture,
		const FName DismissedInstanceId,
		const FName ActiveAfterReplacement,
		const EGameXXKEquipmentTransactionError ExpectedError)
	{
		const TArray<uint8> Before = SerializeRuntimeState(Fixture.Subsystem->GetRuntimeState());
		const FName PendingIdBefore = Fixture.Subsystem->GetRuntimeState().CardRun.CompanionRoster.PendingRecruitment.Candidate.InstanceId;
		FGameXXKEquipmentTransactionResult Result;
		Result.bSucceeded = true;
		Result.Error = EGameXXKEquipmentTransactionError::SaveMigrationFailed;
		Result.Message = FText::FromString(TEXT("stale-result-payload"));
		Result.AffectedInstanceIds = {TEXT("EquipmentInstance.Stale")};

		const bool bReturned = Fixture.Subsystem->ResolvePendingPermanentCompanionReplacement(
			DismissedInstanceId,
			ActiveAfterReplacement,
			Result);
		Test.TestFalse(Label + TEXT(" returns false"), bReturned);
		Test.TestEqual(Label + TEXT(" return value equals typed success"), bReturned, Result.bSucceeded);
		Test.TestFalse(Label + TEXT(" initializes bSucceeded"), Result.bSucceeded);
		Test.TestEqual(Label + TEXT(" preserves the exact typed error"), Result.Error, ExpectedError);
		Test.TestTrue(
			Label + TEXT(" uses the central localized error mapper"),
			Result.Message.EqualTo(FGameXXKEquipmentRules::GetTransactionErrorMessage(ExpectedError)));
		Test.TestTrue(Label + TEXT(" clears stale affected IDs"), Result.AffectedInstanceIds.IsEmpty());
		Test.TestEqual(
			Label + TEXT(" preserves the pending candidate identity"),
			Fixture.Subsystem->GetRuntimeState().CardRun.CompanionRoster.PendingRecruitment.Candidate.InstanceId,
			PendingIdBefore);
		Test.TestEqual(
			Label + TEXT(" preserves every RuntimeState byte"),
			SerializeRuntimeState(Fixture.Subsystem->GetRuntimeState()),
			Before);
	}

	int32 CountActiveCompanions(const FGameXXKCompanionRosterState& Roster)
	{
		int32 Count = 0;
		for (const FGameXXKPermanentCompanion& Companion : Roster.PermanentCompanions)
		{
			Count += Companion.bIsActive ? 1 : 0;
		}
		return Count;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKEquipmentCompanionReplacementTest,
	"GameXXK.Equipment.CompanionReplacement.AtomicTransaction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKEquipmentCompanionReplacementTest::RunTest(const FString& Parameters)
{
	for (int32 EquippedCount = 0; EquippedCount <= 6; ++EquippedCount)
	{
		for (int32 WarehouseCount = 194; WarehouseCount <= 200; ++WarehouseCount)
		{
			FReplacementFixture Fixture;
			if (!CreateEquipmentFixture(*this, Fixture, EquippedCount, WarehouseCount, false))
			{
				return false;
			}

			if (WarehouseCount + EquippedCount > FGameXXKEquipmentRules::WarehouseCapacity)
			{
				TestTransactionFailure(
					*this,
					FString::Printf(TEXT("%d equipped items at warehouse count %d"), EquippedCount, WarehouseCount),
					Fixture,
					Fixture.DismissedInstanceId,
					NAME_None,
					EGameXXKEquipmentTransactionError::WarehouseFull);
				continue;
			}

			FGameXXKEquipmentTransactionResult Result;
			const bool bReturned = Fixture.Subsystem->ResolvePendingPermanentCompanionReplacement(
				Fixture.DismissedInstanceId,
				NAME_None,
				Result);
			TestTrue(
				FString::Printf(TEXT("%d equipped items at warehouse count %d replace successfully"), EquippedCount, WarehouseCount),
				bReturned);
			TestEqual(TEXT("successful transaction return equals typed success"), bReturned, Result.bSucceeded);
			TestEqual(TEXT("successful transaction has no typed error"), Result.Error, EGameXXKEquipmentTransactionError::None);
			TestEqual(TEXT("successful transaction returns every equipped item in slot order"), Result.AffectedInstanceIds, Fixture.EquippedInstanceIds);
		}
	}

	FReplacementFixture FullWarehouseEmptyLoadout;
	if (!CreateEquipmentFixture(*this, FullWarehouseEmptyLoadout, 0, 200, false))
	{
		return false;
	}
	FGameXXKEquipmentTransactionResult EmptyLoadoutResult;
	TestTrue(
		TEXT("a full warehouse still permits replacement when the dismissed companion has no equipment"),
		FullWarehouseEmptyLoadout.Subsystem->ResolvePendingPermanentCompanionReplacement(
			FullWarehouseEmptyLoadout.DismissedInstanceId,
			NAME_None,
			EmptyLoadoutResult));

	FReplacementFixture ExactCapacity;
	if (!CreateEquipmentFixture(*this, ExactCapacity, 1, 199, false))
	{
		return false;
	}
	FGameXXKEquipmentTransactionResult ExactCapacityResult;
	TestTrue(
		TEXT("one equipped item returns when it exactly fills warehouse slot 200"),
		ExactCapacity.Subsystem->ResolvePendingPermanentCompanionReplacement(
			ExactCapacity.DismissedInstanceId,
			NAME_None,
			ExactCapacityResult));
	TestEqual(TEXT("exact-capacity return reports the one moved instance"), ExactCapacityResult.AffectedInstanceIds, ExactCapacity.EquippedInstanceIds);

	FReplacementFixture FullWarehouseOneEquipped;
	if (!CreateEquipmentFixture(*this, FullWarehouseOneEquipped, 1, 200, false))
	{
		return false;
	}
	TestTransactionFailure(
		*this,
		TEXT("warehouse 200 plus one equipped item"),
		FullWarehouseOneEquipped,
		FullWarehouseOneEquipped.DismissedInstanceId,
		NAME_None,
		EGameXXKEquipmentTransactionError::WarehouseFull);

	FReplacementFixture AllSix;
	if (!CreateEquipmentFixture(*this, AllSix, 6, 194, false))
	{
		return false;
	}
	FGameXXKEquipmentTransactionResult AllSixResult;
	TestTrue(
		TEXT("six equipped items return when six warehouse slots remain"),
		AllSix.Subsystem->ResolvePendingPermanentCompanionReplacement(
			AllSix.DismissedInstanceId,
			AllSix.PendingCandidateInstanceId,
			AllSixResult));
	TestEqual(
		TEXT("six-slot return order is Weapon Head Armor Belt Shoes Accessory"),
		AllSixResult.AffectedInstanceIds,
		AllSix.EquippedInstanceIds);
	const FGameXXKRuntimeState& AllSixState = AllSix.Subsystem->GetRuntimeState();
	TestEqual(TEXT("all six returned items append after the 194 existing warehouse items"), AllSixState.EquipmentCollection.WarehouseInstanceIds.Num(), 200);
	TestFalse(TEXT("dismissed companion loadout is removed"), AllSixState.EquipmentCollection.CharacterLoadouts.Contains(AllSix.DismissedInstanceId));
	TestTrue(TEXT("the requested pending candidate becomes active"), AllSixState.CardRun.CompanionRoster.PermanentCompanions.ContainsByPredicate([&AllSix](const FGameXXKPermanentCompanion& Companion)
	{
		return Companion.InstanceId == AllSix.PendingCandidateInstanceId && Companion.bIsActive;
	}));
	TestEqual(TEXT("successful replacement keeps exactly one active companion"), CountActiveCompanions(AllSixState.CardRun.CompanionRoster), 1);

	FReplacementFixture ActiveDismissal;
	if (!CreateEquipmentFixture(*this, ActiveDismissal, 2, 198, true))
	{
		return false;
	}
	FGameXXKEquipmentTransactionResult ActiveDismissalResult;
	TestTrue(
		TEXT("active companion replacement permits an explicitly empty active slot"),
		ActiveDismissal.Subsystem->ResolvePendingPermanentCompanionReplacement(
			ActiveDismissal.DismissedInstanceId,
			NAME_None,
			ActiveDismissalResult));
	TestEqual(
		TEXT("NAME_None leaves no active companion after replacing the active companion"),
		CountActiveCompanions(ActiveDismissal.Subsystem->GetRuntimeState().CardRun.CompanionRoster),
		0);

	FReplacementFixture InactiveDismissal;
	if (!CreateEquipmentFixture(*this, InactiveDismissal, 2, 198, false))
	{
		return false;
	}
	FGameXXKEquipmentTransactionResult InactiveDismissalResult;
	TestTrue(
		TEXT("inactive companion replacement succeeds with NAME_None"),
		InactiveDismissal.Subsystem->ResolvePendingPermanentCompanionReplacement(
			InactiveDismissal.DismissedInstanceId,
			NAME_None,
			InactiveDismissalResult));
	TestTrue(TEXT("NAME_None preserves the surviving active companion when replacing an inactive one"), InactiveDismissal.Subsystem->GetRuntimeState().CardRun.CompanionRoster.PermanentCompanions.ContainsByPredicate([&InactiveDismissal](const FGameXXKPermanentCompanion& Companion)
	{
		return Companion.InstanceId == InactiveDismissal.SurvivingActiveInstanceId && Companion.bIsActive;
	}));

	FReplacementFixture RouteLocked;
	if (!CreateEquipmentFixture(*this, RouteLocked, 1, 199, false))
	{
		return false;
	}
	RouteLocked.Subsystem->GetMutableRuntimeState().CardRun.bLoadoutLockedForRoute = true;
	TestTransactionFailure(
		*this,
		TEXT("route lock has priority over stale companion IDs"),
		RouteLocked,
		TEXT("Companion.Instance.Stale"),
		TEXT("Companion.Instance.StaleActive"),
		EGameXXKEquipmentTransactionError::RouteLocked);

	FReplacementFixture BattleLocked;
	if (!CreateEquipmentFixture(*this, BattleLocked, 1, 199, false))
	{
		return false;
	}
	BattleLocked.Subsystem->GetMutableRuntimeState().CardRun.bHasActiveCardBattle = true;
	TestTransactionFailure(
		*this,
		TEXT("active battle lock has priority over owner validation"),
		BattleLocked,
		TEXT("Companion.Instance.Stale"),
		TEXT("Companion.Instance.StaleActive"),
		EGameXXKEquipmentTransactionError::RouteLocked);

	FReplacementFixture StaleDismissed;
	if (!CreateEquipmentFixture(*this, StaleDismissed, 1, 200, false))
	{
		return false;
	}
	TestTransactionFailure(
		*this,
		TEXT("stale dismissed companion is rejected before warehouse capacity"),
		StaleDismissed,
		TEXT("Companion.Instance.Stale"),
		NAME_None,
		EGameXXKEquipmentTransactionError::InvalidOwner);

	FReplacementFixture StaleActive;
	if (!CreateEquipmentFixture(*this, StaleActive, 1, 200, false))
	{
		return false;
	}
	TestTransactionFailure(
		*this,
		TEXT("stale post-replacement active companion is rejected before warehouse capacity"),
		StaleActive,
		StaleActive.DismissedInstanceId,
		TEXT("Companion.Instance.StaleActive"),
		EGameXXKEquipmentTransactionError::InvalidOwner);

	FReplacementFixture DismissedCannotRemainActive;
	if (!CreateEquipmentFixture(*this, DismissedCannotRemainActive, 1, 200, true))
	{
		return false;
	}
	TestTransactionFailure(
		*this,
		TEXT("dismissed companion cannot be selected as the post-replacement active owner"),
		DismissedCannotRemainActive,
		DismissedCannotRemainActive.DismissedInstanceId,
		DismissedCannotRemainActive.DismissedInstanceId,
		EGameXXKEquipmentTransactionError::InvalidOwner);

	FReplacementFixture DiscardedCandidate;
	if (!CreateEquipmentFixture(*this, DiscardedCandidate, 1, 199, false))
	{
		return false;
	}
	const TArray<uint8> EquipmentBeforeDiscard = SerializeEquipmentCollection(DiscardedCandidate.Subsystem->GetRuntimeState().EquipmentCollection);
	TestTrue(TEXT("saved replacement candidate can be explicitly discarded"), DiscardedCandidate.Subsystem->DiscardPendingPermanentCompanionRecruitment());
	FGameXXKPermanentCompanion IgnoredPendingCandidate;
	TestFalse(TEXT("candidate discard clears only the saved pending candidate"), DiscardedCandidate.Subsystem->TryGetPendingPermanentCompanionRecruitment(IgnoredPendingCandidate));
	TestEqual(
		TEXT("candidate discard does not touch central equipment"),
		SerializeEquipmentCollection(DiscardedCandidate.Subsystem->GetRuntimeState().EquipmentCollection),
		EquipmentBeforeDiscard);
	const TArray<uint8> DiscardedStateBeforeReplacementAttempt = SerializeRuntimeState(DiscardedCandidate.Subsystem->GetRuntimeState());
	FGameXXKEquipmentTransactionResult DiscardedResult;
	const bool bDiscardedReplacement = DiscardedCandidate.Subsystem->ResolvePendingPermanentCompanionReplacement(
		DiscardedCandidate.DismissedInstanceId,
		NAME_None,
		DiscardedResult);
	TestFalse(TEXT("discarded candidate cannot later replace a companion"), bDiscardedReplacement);
	TestEqual(TEXT("discarded replacement return equals typed success"), bDiscardedReplacement, DiscardedResult.bSucceeded);
	TestEqual(
		TEXT("discarded replacement attempt preserves every RuntimeState byte"),
		SerializeRuntimeState(DiscardedCandidate.Subsystem->GetRuntimeState()),
		DiscardedStateBeforeReplacementAttempt);

	FReplacementFixture ExperienceRefund;
	if (!CreateEquipmentFixture(*this, ExperienceRefund, 1, 199, false))
	{
		return false;
	}
	FGameXXKPermanentCompanion* InvestedCompanion = ExperienceRefund.Subsystem->GetMutableRuntimeState().CardRun.CompanionRoster.PermanentCompanions.FindByPredicate([&ExperienceRefund](const FGameXXKPermanentCompanion& Companion)
	{
		return Companion.InstanceId == ExperienceRefund.DismissedInstanceId;
	});
	if (!TestNotNull(TEXT("experience-refund fixture finds dismissed companion"), InvestedCompanion))
	{
		return false;
	}
	InvestedCompanion->Experience = 1;
	const TArray<uint8> ExperienceRefundBefore = SerializeRuntimeState(ExperienceRefund.Subsystem->GetRuntimeState());
	FGameXXKEquipmentTransactionResult ExperienceRefundResult;
	TestFalse(
		TEXT("replacement still rejects unclaimed experience material"),
		ExperienceRefund.Subsystem->ResolvePendingPermanentCompanionReplacement(
			ExperienceRefund.DismissedInstanceId,
			NAME_None,
			ExperienceRefundResult));
	TestEqual(
		TEXT("experience-refund rejection rolls central equipment and complete state back"),
		SerializeRuntimeState(ExperienceRefund.Subsystem->GetRuntimeState()),
		ExperienceRefundBefore);

	FReplacementFixture DeprecatedLegacyField;
	if (!CreateEquipmentFixture(*this, DeprecatedLegacyField, 0, 200, false))
	{
		return false;
	}
	FGameXXKPermanentCompanion* LegacyFieldCompanion = DeprecatedLegacyField.Subsystem->GetMutableRuntimeState().CardRun.CompanionRoster.PermanentCompanions.FindByPredicate([&DeprecatedLegacyField](const FGameXXKPermanentCompanion& Companion)
	{
		return Companion.InstanceId == DeprecatedLegacyField.DismissedInstanceId;
	});
	if (!TestNotNull(TEXT("legacy-field fixture finds dismissed companion"), LegacyFieldCompanion))
	{
		return false;
	}
	LegacyFieldCompanion->EquippedItemIds = {TEXT("Item.WoodenSword")};
	FGameXXKEquipmentTransactionResult LegacyFieldResult;
	TestTrue(
		TEXT("deprecated pre-v7 field is not treated as authoritative equipment after migration"),
		DeprecatedLegacyField.Subsystem->ResolvePendingPermanentCompanionReplacement(
			DeprecatedLegacyField.DismissedInstanceId,
			NAME_None,
			LegacyFieldResult));

	FReplacementFixture TypedFixture;
	FReplacementFixture WrapperFixture;
	if (!CreateEquipmentFixture(*this, TypedFixture, 2, 198, false)
		|| !CreateEquipmentFixture(*this, WrapperFixture, 2, 198, false))
	{
		return false;
	}
	FGameXXKEquipmentTransactionResult TypedResult;
	const bool bTyped = TypedFixture.Subsystem->ResolvePendingPermanentCompanionReplacement(
		TypedFixture.DismissedInstanceId,
		NAME_None,
		TypedResult);
	const bool bWrapper = WrapperFixture.Subsystem->ResolvePendingPermanentCompanionReplacement(
		WrapperFixture.DismissedInstanceId,
		NAME_None);
	TestEqual(TEXT("legacy two-argument wrapper returns the authoritative overload boolean"), bWrapper, bTyped);
	TestEqual(TEXT("authoritative overload return equals its typed result"), bTyped, TypedResult.bSucceeded);
	TestEqual(
		TEXT("legacy wrapper and authoritative overload produce identical RuntimeState"),
		SerializeRuntimeState(WrapperFixture.Subsystem->GetRuntimeState()),
		SerializeRuntimeState(TypedFixture.Subsystem->GetRuntimeState()));
	return true;
}

#endif
