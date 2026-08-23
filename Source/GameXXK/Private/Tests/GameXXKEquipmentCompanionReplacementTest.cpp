#include "GameXXKCardBattleAdapter.h"
#include "GameXXKCompanionCatalog.h"
#include "GameXXKCompanionRules.h"
#include "GameXXKDesktopInventoryRules.h"
#include "GameXXKEquipmentEconomyRules.h"
#include "GameXXKEquipmentRules.h"
#include "GameXXKMVPRules.h"
#include "GameXXKPartyFormationRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "MVP/GameXXKSaveMigration.h"

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
		State = UGameXXKMVPRules::CreateNewGame();
		State.Screen = EGameXXKScreen::Town;
		State.EquipmentCollection = FGameXXKEquipmentCollectionState();
		State.DesktopInventory = FGameXXKDesktopInventoryState();
		State.Inventory.Reset();
		State.EnhancementMaterial = 0;
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
		if (!Test.TestTrue(
			TEXT("replacement fixture materializes a legal ordered formation"),
			FGameXXKPartyFormationRules::Normalize(State, &CardRunError)))
		{
			Test.AddError(CardRunError);
			return false;
		}
		FGameXXKPartyFormationRules::ProjectCompatibility(State);

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
		if (!Test.TestTrue(
			TEXT("replacement fixture central collection matches the full roster"),
			FGameXXKEquipmentRules::ValidateCollectionAgainstRoster(
				State.EquipmentCollection,
				State.CardRun.CompanionRoster,
				&CollectionError)))
		{
			Test.AddError(CollectionError);
			return false;
		}
		if (!Test.TestTrue(
			TEXT("replacement fixture synchronizes equipment mirrors"),
			FGameXXKEquipmentEconomyRules::SynchronizeRuntimeMirrors(State)))
		{
			return false;
		}
		if (!Test.TestTrue(
			TEXT("replacement fixture normalizes desktop inventory"),
			FGameXXKDesktopInventoryRules::Normalize(State, &CollectionError)))
		{
			Test.AddError(CollectionError);
			return false;
		}
		if (!Test.TestTrue(
			TEXT("replacement fixture is a valid v24 runtime"),
			FGameXXKSaveMigration::ValidateRuntimeState(State, CollectionError)))
		{
			Test.AddError(CollectionError);
			return false;
		}
		return true;
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

	bool ConfigureReplacementFormation(
		FAutomationTestBase& Test,
		FReplacementFixture& Fixture,
		const bool bIncludeDismissedCompanion,
		FGameXXKOrderedPartyFormation& OutFormation)
	{
		OutFormation = FGameXXKOrderedPartyFormation();
		FGameXXKPartyMemberRef Hero;
		Hero.Kind = EGameXXKPartyMemberKind::Hero;
		Hero.MemberId = FGameXXKEquipmentRules::HeroCharacterId();
		OutFormation.Members.Add(Hero);

		if (bIncludeDismissedCompanion)
		{
			FGameXXKPartyMemberRef Dismissed;
			Dismissed.Kind = EGameXXKPartyMemberKind::PermanentCompanion;
			Dismissed.MemberId = Fixture.DismissedInstanceId;
			OutFormation.Members.Add(Dismissed);
		}

		for (const FGameXXKPermanentCompanion& Companion :
			Fixture.Subsystem->GetRuntimeState().CardRun.CompanionRoster.PermanentCompanions)
		{
			if (Companion.InstanceId == Fixture.DismissedInstanceId
				|| OutFormation.Members.ContainsByPredicate([&Companion](const FGameXXKPartyMemberRef& Ref)
				{
					return Ref.MemberId == Companion.InstanceId;
				}))
			{
				continue;
			}
			FGameXXKPartyMemberRef Ref;
			Ref.Kind = EGameXXKPartyMemberKind::PermanentCompanion;
			Ref.MemberId = Companion.InstanceId;
			OutFormation.Members.Add(Ref);
			if (OutFormation.Members.Num() == FGameXXKPartyFormationRules::PartySize)
			{
				break;
			}
		}

		if (!Test.TestEqual(TEXT("replacement formation fixture has exactly three members"),
			OutFormation.Members.Num(), FGameXXKPartyFormationRules::PartySize))
		{
			return false;
		}
		FString Error;
		if (!Test.TestTrue(TEXT("replacement formation fixture commits through the public facade"),
			Fixture.Subsystem->SetOrderedPartyFormation(OutFormation, Error)))
		{
			Test.AddError(Error);
			return false;
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKEquipmentCompanionReplacementFormationTest,
	"GameXXK.Equipment.CompanionReplacement.OrderedFormationSlotRepair",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKEquipmentCompanionReplacementFormationTest::RunTest(const FString& Parameters)
{
	FReplacementFixture DeployedDismissal;
	if (!CreateEquipmentFixture(*this, DeployedDismissal, 1, 0, false))
	{
		return false;
	}
	FGameXXKOrderedPartyFormation BeforeFormation;
	if (!ConfigureReplacementFormation(*this, DeployedDismissal, true, BeforeFormation))
	{
		return false;
	}
	TestEqual(TEXT("dismissed companion occupies the requested 2P slot"),
		BeforeFormation.Members[1].MemberId, DeployedDismissal.DismissedInstanceId);
	const FGameXXKPartyMemberRef UnchangedOneP = BeforeFormation.Members[0];
	const FGameXXKPartyMemberRef UnchangedThreeP = BeforeFormation.Members[2];
	const TArray<FName> HeroDeckBefore =
		DeployedDismissal.Subsystem->GetRuntimeState().CardRun.HeroSelectedCardIds;
	const FGameXXKPermanentCompanion PendingBefore =
		DeployedDismissal.Subsystem->GetRuntimeState().CardRun.CompanionRoster.PendingRecruitment.Candidate;
	const FGameXXKPermanentCompanion* SurvivorBefore =
		DeployedDismissal.Subsystem->GetRuntimeState().CardRun.CompanionRoster.PermanentCompanions.FindByPredicate(
			[&UnchangedThreeP](const FGameXXKPermanentCompanion& Companion)
			{
				return Companion.InstanceId == UnchangedThreeP.MemberId;
			});
	if (!TestNotNull(TEXT("formation replacement fixture finds its surviving 3P companion"), SurvivorBefore))
	{
		return false;
	}
	const TArray<FName> SurvivorDeckBefore = SurvivorBefore->SelectedCardIds;

	FGameXXKEquipmentTransactionResult Result;
	if (!TestTrue(TEXT("deployed companion full-roster replacement succeeds"),
		DeployedDismissal.Subsystem->ResolvePendingPermanentCompanionReplacement(
			DeployedDismissal.DismissedInstanceId,
			DeployedDismissal.PendingCandidateInstanceId,
			Result)))
	{
		return false;
	}
	const FGameXXKRuntimeState& After = DeployedDismissal.Subsystem->GetRuntimeState();
	const TArray<FGameXXKPartyMemberRef>& AfterMembers = After.CardRun.OrderedFormation.Members;
	if (!TestEqual(TEXT("replacement keeps exactly three formation members"), AfterMembers.Num(), 3))
	{
		return false;
	}
	TestTrue(TEXT("replacement leaves 1P bit-identical"), AfterMembers[0] == UnchangedOneP);
	TestEqual(TEXT("new recruit occupies the dismissed companion's exact 2P slot"),
		AfterMembers[1].MemberId, DeployedDismissal.PendingCandidateInstanceId);
	TestEqual(TEXT("replacement slot keeps permanent-companion kind"),
		AfterMembers[1].Kind, EGameXXKPartyMemberKind::PermanentCompanion);
	TestTrue(TEXT("replacement leaves 3P bit-identical"), AfterMembers[2] == UnchangedThreeP);
	TestFalse(TEXT("removed companion is absent from ordered formation"),
		AfterMembers.ContainsByPredicate([&DeployedDismissal](const FGameXXKPartyMemberRef& Ref)
		{
			return Ref.MemberId == DeployedDismissal.DismissedInstanceId;
		}));
	TestFalse(TEXT("removed companion is absent from roster"),
		After.CardRun.CompanionRoster.PermanentCompanions.ContainsByPredicate(
			[&DeployedDismissal](const FGameXXKPermanentCompanion& Companion)
			{
				return Companion.InstanceId == DeployedDismissal.DismissedInstanceId;
			}));
	const FGameXXKPermanentCompanion* AddedCandidate =
		After.CardRun.CompanionRoster.PermanentCompanions.FindByPredicate(
			[&DeployedDismissal](const FGameXXKPermanentCompanion& Companion)
			{
				return Companion.InstanceId == DeployedDismissal.PendingCandidateInstanceId;
			});
	if (TestNotNull(TEXT("pending candidate becomes an owned companion"), AddedCandidate))
	{
		TestEqual(TEXT("replacement preserves pending candidate personal deck"),
			AddedCandidate->PersonalCardIds, PendingBefore.PersonalCardIds);
		TestEqual(TEXT("replacement preserves pending candidate selected deck"),
			AddedCandidate->SelectedCardIds, PendingBefore.SelectedCardIds);
	}
	const FGameXXKPermanentCompanion* SurvivorAfter =
		After.CardRun.CompanionRoster.PermanentCompanions.FindByPredicate(
			[&UnchangedThreeP](const FGameXXKPermanentCompanion& Companion)
			{
				return Companion.InstanceId == UnchangedThreeP.MemberId;
			});
	if (TestNotNull(TEXT("surviving 3P remains owned"), SurvivorAfter))
	{
		TestEqual(TEXT("surviving 3P deck remains exact"), SurvivorAfter->SelectedCardIds, SurvivorDeckBefore);
	}
	TestEqual(TEXT("replacement leaves hero deck exact"), After.CardRun.HeroSelectedCardIds, HeroDeckBefore);
	TestEqual(TEXT("compatibility active companion follows first ordered companion"),
		After.CardRun.PartySelection.ActivePermanentCompanionInstanceId,
		DeployedDismissal.PendingCandidateInstanceId);
	TestEqual(TEXT("equipment refund reports the dismissed companion's equipped item"),
		Result.AffectedInstanceIds, DeployedDismissal.EquippedInstanceIds);
	TestTrue(TEXT("refunded equipment reaches the warehouse"),
		After.EquipmentCollection.WarehouseInstanceIds.Contains(DeployedDismissal.EquippedInstanceIds[0]));
	TestFalse(TEXT("dismissed equipment loadout is removed"),
		After.EquipmentCollection.CharacterLoadouts.Contains(DeployedDismissal.DismissedInstanceId));

	FString ValidationError;
	TestTrue(TEXT("post-replacement runtime passes authoritative v24 validation"),
		FGameXXKSaveMigration::ValidateRuntimeState(After, ValidationError));
	FGameXXKSaveState CurrentSave = UGameXXKMVPRules::MakeSaveState(After);
	FGameXXKSaveState RoundTrip;
	FGameXXKSaveMigrationReport RoundTripReport;
	TestTrue(TEXT("post-replacement v24 save roundtrips through migration"),
		FGameXXKSaveMigration::MigrateToCurrent(CurrentSave, RoundTrip, RoundTripReport));
	TestEqual(TEXT("v24 roundtrip preserves repaired formation order"),
		RoundTrip.RuntimeState.CardRun.OrderedFormation.Members, AfterMembers);

	FReplacementFixture ActiveAfterPromotion;
	if (!CreateEquipmentFixture(*this, ActiveAfterPromotion, 0, 0, false))
	{
		return false;
	}
	const FName ExistingSurvivorId = ActiveAfterPromotion.SurvivingActiveInstanceId;
	FGameXXKOrderedPartyFormation ActiveAfterBefore;
	FGameXXKPartyMemberRef SurvivorRef;
	SurvivorRef.Kind = EGameXXKPartyMemberKind::PermanentCompanion;
	SurvivorRef.MemberId = ExistingSurvivorId;
	FGameXXKPartyMemberRef HeroRef;
	HeroRef.Kind = EGameXXKPartyMemberKind::Hero;
	HeroRef.MemberId = FGameXXKEquipmentRules::HeroCharacterId();
	FGameXXKPartyMemberRef DismissedRef;
	DismissedRef.Kind = EGameXXKPartyMemberKind::PermanentCompanion;
	DismissedRef.MemberId = ActiveAfterPromotion.DismissedInstanceId;
	ActiveAfterBefore.Members = {SurvivorRef, HeroRef, DismissedRef};
	FString ActiveAfterFormationError;
	if (!TestTrue(TEXT("ActiveAfter fixture commits survivor/hero/dismissed order"),
		ActiveAfterPromotion.Subsystem->SetOrderedPartyFormation(
			ActiveAfterBefore,
			ActiveAfterFormationError)))
	{
		AddError(ActiveAfterFormationError);
		return false;
	}
	FGameXXKEquipmentTransactionResult ActiveAfterResult;
	TestTrue(TEXT("replacement accepts the new recruit as explicit ActiveAfter"),
		ActiveAfterPromotion.Subsystem->ResolvePendingPermanentCompanionReplacement(
			ActiveAfterPromotion.DismissedInstanceId,
			ActiveAfterPromotion.PendingCandidateInstanceId,
			ActiveAfterResult));
	const FGameXXKRuntimeState& ActiveAfterState = ActiveAfterPromotion.Subsystem->GetRuntimeState();
	const TArray<FGameXXKPartyMemberRef>& ActiveAfterMembers =
		ActiveAfterState.CardRun.OrderedFormation.Members;
	if (!TestEqual(TEXT("ActiveAfter replacement keeps three slots"), ActiveAfterMembers.Num(), 3))
	{
		return false;
	}
	TestEqual(TEXT("explicit ActiveAfter becomes first companion in its earlier companion slot"),
		ActiveAfterMembers[0].MemberId, ActiveAfterPromotion.PendingCandidateInstanceId);
	TestTrue(TEXT("ActiveAfter promotion preserves the non-companion hero slot bit-identically"),
		ActiveAfterMembers[1] == HeroRef);
	TestEqual(TEXT("displaced former first companion moves to removed companion slot"),
		ActiveAfterMembers[2].MemberId, ExistingSurvivorId);
	TestEqual(TEXT("legacy active ID follows explicit ActiveAfter"),
		ActiveAfterState.CardRun.PartySelection.ActivePermanentCompanionInstanceId,
		ActiveAfterPromotion.PendingCandidateInstanceId);

	FReplacementFixture OffFormationDismissal;
	if (!CreateEquipmentFixture(*this, OffFormationDismissal, 0, 0, false))
	{
		return false;
	}
	FGameXXKOrderedPartyFormation OffFormationBefore;
	if (!ConfigureReplacementFormation(*this, OffFormationDismissal, false, OffFormationBefore))
	{
		return false;
	}
	FGameXXKEquipmentTransactionResult OffFormationResult;
	TestTrue(TEXT("off-formation companion replacement succeeds"),
		OffFormationDismissal.Subsystem->ResolvePendingPermanentCompanionReplacement(
			OffFormationDismissal.DismissedInstanceId,
			NAME_None,
			OffFormationResult));
	TestEqual(TEXT("off-formation replacement preserves the entire ordered array"),
		OffFormationDismissal.Subsystem->GetRuntimeState().CardRun.OrderedFormation.Members,
		OffFormationBefore.Members);

	FReplacementFixture OffFormationLaterActive;
	if (!CreateEquipmentFixture(*this, OffFormationLaterActive, 0, 0, false))
	{
		return false;
	}
	FGameXXKOrderedPartyFormation OffFormationLaterBefore;
	if (!ConfigureReplacementFormation(*this, OffFormationLaterActive, false, OffFormationLaterBefore))
	{
		return false;
	}
	const FGameXXKPartyMemberRef LaterHeroBefore = OffFormationLaterBefore.Members[0];
	const FGameXXKPartyMemberRef FirstCompanionBefore = OffFormationLaterBefore.Members[1];
	const FGameXXKPartyMemberRef LaterCompanionBefore = OffFormationLaterBefore.Members[2];
	FGameXXKEquipmentTransactionResult OffFormationLaterResult;
	TestTrue(TEXT("off-formation dismissal accepts an explicitly deployed later ActiveAfter"),
		OffFormationLaterActive.Subsystem->ResolvePendingPermanentCompanionReplacement(
			OffFormationLaterActive.DismissedInstanceId,
			LaterCompanionBefore.MemberId,
			OffFormationLaterResult));
	const TArray<FGameXXKPartyMemberRef>& OffFormationLaterMembers =
		OffFormationLaterActive.Subsystem->GetRuntimeState().CardRun.OrderedFormation.Members;
	TestTrue(TEXT("later ActiveAfter preserves the hero slot bit-identically"),
		OffFormationLaterMembers[0] == LaterHeroBefore);
	TestEqual(TEXT("later ActiveAfter swaps into the first companion slot"),
		OffFormationLaterMembers[1].MemberId,
		LaterCompanionBefore.MemberId);
	TestEqual(TEXT("displaced first companion moves only to the later companion slot"),
		OffFormationLaterMembers[2].MemberId,
		FirstCompanionBefore.MemberId);
	TestEqual(TEXT("later ActiveAfter controls the compatibility active projection"),
		OffFormationLaterActive.Subsystem->GetRuntimeState().CardRun.PartySelection.ActivePermanentCompanionInstanceId,
		LaterCompanionBefore.MemberId);

	FReplacementFixture OffFormationOwnedActive;
	if (!CreateEquipmentFixture(*this, OffFormationOwnedActive, 0, 0, false))
	{
		return false;
	}
	FGameXXKRuntimeState& OwnedActiveState = OffFormationOwnedActive.Subsystem->GetMutableRuntimeState();
	FString OwnedActiveError;
	if (!TestTrue(TEXT("off-deployed ActiveAfter fixture attaches Tusi Chief"),
		FGameXXKCardBattleAdapter::SetQuestNpcForCurrentRun(
			OwnedActiveState,
			TEXT("Npc.TusiChief"),
			{},
			&OwnedActiveError)))
	{
		AddError(OwnedActiveError);
		return false;
	}
	FGameXXKOrderedPartyFormation OwnedActiveBefore;
	FGameXXKPartyMemberRef OwnedHero;
	OwnedHero.Kind = EGameXXKPartyMemberKind::Hero;
	OwnedHero.MemberId = FGameXXKEquipmentRules::HeroCharacterId();
	FGameXXKPartyMemberRef OwnedNpc;
	OwnedNpc.Kind = EGameXXKPartyMemberKind::QuestNpc;
	OwnedNpc.MemberId = TEXT("Npc.TusiChief");
	FGameXXKPartyMemberRef OwnedFirstCompanion;
	OwnedFirstCompanion.Kind = EGameXXKPartyMemberKind::PermanentCompanion;
	OwnedFirstCompanion.MemberId = OffFormationOwnedActive.SurvivingActiveInstanceId;
	OwnedActiveBefore.Members = {OwnedHero, OwnedNpc, OwnedFirstCompanion};
	if (!TestTrue(TEXT("off-deployed ActiveAfter fixture commits hero/NPC/companion formation"),
		OffFormationOwnedActive.Subsystem->SetOrderedPartyFormation(OwnedActiveBefore, OwnedActiveError)))
	{
		AddError(OwnedActiveError);
		return false;
	}
	FName RequestedOwnedActiveId = NAME_None;
	for (const FGameXXKPermanentCompanion& Companion :
		OffFormationOwnedActive.Subsystem->GetRuntimeState().CardRun.CompanionRoster.PermanentCompanions)
	{
		if (Companion.InstanceId != OffFormationOwnedActive.DismissedInstanceId
			&& !OwnedActiveBefore.Members.ContainsByPredicate([&Companion](const FGameXXKPartyMemberRef& Ref)
			{
				return Ref.MemberId == Companion.InstanceId;
			}))
		{
			RequestedOwnedActiveId = Companion.InstanceId;
			break;
		}
	}
	if (!TestFalse(TEXT("off-deployed ActiveAfter fixture finds an owned reserve companion"),
		RequestedOwnedActiveId.IsNone()))
	{
		return false;
	}
	FGameXXKEquipmentTransactionResult OffFormationOwnedResult;
	TestTrue(TEXT("off-formation dismissal accepts an owned but undeployed ActiveAfter"),
		OffFormationOwnedActive.Subsystem->ResolvePendingPermanentCompanionReplacement(
			OffFormationOwnedActive.DismissedInstanceId,
			RequestedOwnedActiveId,
			OffFormationOwnedResult));
	const FGameXXKRuntimeState& OwnedActiveAfter = OffFormationOwnedActive.Subsystem->GetRuntimeState();
	TestTrue(TEXT("off-deployed ActiveAfter preserves hero slot bit-identically"),
		OwnedActiveAfter.CardRun.OrderedFormation.Members[0] == OwnedHero);
	TestTrue(TEXT("off-deployed ActiveAfter preserves NPC slot bit-identically"),
		OwnedActiveAfter.CardRun.OrderedFormation.Members[1] == OwnedNpc);
	TestEqual(TEXT("off-deployed ActiveAfter replaces only the first companion slot"),
		OwnedActiveAfter.CardRun.OrderedFormation.Members[2].MemberId,
		RequestedOwnedActiveId);
	TestTrue(TEXT("the displaced prior first companion remains owned"),
		OwnedActiveAfter.CardRun.CompanionRoster.PermanentCompanions.ContainsByPredicate(
			[&OwnedFirstCompanion](const FGameXXKPermanentCompanion& Companion)
			{
				return Companion.InstanceId == OwnedFirstCompanion.MemberId;
			}));
	TestEqual(TEXT("off-deployed ActiveAfter controls compatibility active projection"),
		OwnedActiveAfter.CardRun.PartySelection.ActivePermanentCompanionInstanceId,
		RequestedOwnedActiveId);

	FReplacementFixture InvalidOffFormationActive;
	if (!CreateEquipmentFixture(*this, InvalidOffFormationActive, 0, 0, false))
	{
		return false;
	}
	FGameXXKOrderedPartyFormation InvalidOffFormationBefore;
	if (!ConfigureReplacementFormation(*this, InvalidOffFormationActive, false, InvalidOffFormationBefore))
	{
		return false;
	}
	const TArray<uint8> InvalidOffFormationBytes =
		SerializeRuntimeState(InvalidOffFormationActive.Subsystem->GetRuntimeState());
	FGameXXKEquipmentTransactionResult InvalidOffFormationResult;
	TestFalse(TEXT("invalid explicit off-formation ActiveAfter is rejected"),
		InvalidOffFormationActive.Subsystem->ResolvePendingPermanentCompanionReplacement(
			InvalidOffFormationActive.DismissedInstanceId,
			TEXT("Companion.Unknown.ActiveAfter"),
			InvalidOffFormationResult));
	TestEqual(TEXT("invalid explicit off-formation ActiveAfter rolls back every runtime byte"),
		SerializeRuntimeState(InvalidOffFormationActive.Subsystem->GetRuntimeState()),
		InvalidOffFormationBytes);

	FReplacementFixture NoLegalNewCandidate;
	if (!CreateEquipmentFixture(*this, NoLegalNewCandidate, 0, 0, false))
	{
		return false;
	}
	FGameXXKOrderedPartyFormation NoReplacementFormation;
	if (!ConfigureReplacementFormation(*this, NoLegalNewCandidate, true, NoReplacementFormation))
	{
		return false;
	}
	FGameXXKRuntimeState& CorruptCandidateState = NoLegalNewCandidate.Subsystem->GetMutableRuntimeState();
	const FGameXXKPermanentCompanion DuplicateProfile =
		CorruptCandidateState.CardRun.CompanionRoster.PermanentCompanions[1];
	CorruptCandidateState.CardRun.CompanionRoster.PendingRecruitment.Candidate = DuplicateProfile;
	const TArray<uint8> NoReplacementBefore = SerializeRuntimeState(CorruptCandidateState);
	FGameXXKEquipmentTransactionResult NoReplacementResult;
	TestFalse(TEXT("replacement with no unique pending candidate is rejected"),
		NoLegalNewCandidate.Subsystem->ResolvePendingPermanentCompanionReplacement(
			NoLegalNewCandidate.DismissedInstanceId,
			NAME_None,
			NoReplacementResult));
	TestFalse(TEXT("no-replacement rejection initializes typed failure"), NoReplacementResult.bSucceeded);
	TestEqual(TEXT("no-replacement rejection rolls back every runtime byte"),
		SerializeRuntimeState(NoLegalNewCandidate.Subsystem->GetRuntimeState()),
		NoReplacementBefore);
	return true;
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
	const FGameXXKPartyMemberRef* AllSixFirstCompanion =
		AllSixState.CardRun.OrderedFormation.Members.FindByPredicate([](const FGameXXKPartyMemberRef& Ref)
		{
			return Ref.Kind == EGameXXKPartyMemberKind::PermanentCompanion;
		});
	TestNotNull(TEXT("successful replacement retains an ordered permanent companion"), AllSixFirstCompanion);
	TestTrue(TEXT("successful replacement activates the first ordered companion"), AllSixState.CardRun.CompanionRoster.PermanentCompanions.ContainsByPredicate([AllSixFirstCompanion](const FGameXXKPermanentCompanion& Companion)
	{
		return AllSixFirstCompanion
			&& Companion.InstanceId == AllSixFirstCompanion->MemberId
			&& Companion.bIsActive;
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
	const FGameXXKRuntimeState& ActiveDismissalState = ActiveDismissal.Subsystem->GetRuntimeState();
	const FGameXXKPartyMemberRef* ActiveDismissalFirstCompanion =
		ActiveDismissalState.CardRun.OrderedFormation.Members.FindByPredicate([](const FGameXXKPartyMemberRef& Ref)
		{
			return Ref.Kind == EGameXXKPartyMemberKind::PermanentCompanion;
		});
	TestNotNull(TEXT("active replacement retains an ordered permanent companion"), ActiveDismissalFirstCompanion);
	TestEqual(
		TEXT("NAME_None still projects exactly one active companion from v24 formation"),
		CountActiveCompanions(ActiveDismissalState.CardRun.CompanionRoster),
		1);
	TestEqual(
		TEXT("active replacement compatibility ID follows the first ordered companion"),
		ActiveDismissalState.CardRun.PartySelection.ActivePermanentCompanionInstanceId,
		ActiveDismissalFirstCompanion ? ActiveDismissalFirstCompanion->MemberId : NAME_None);

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
