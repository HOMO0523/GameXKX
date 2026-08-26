#include "Misc/AutomationTest.h"

#include "Engine/GameInstance.h"
#include "GameXXKEquipmentRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKCompanionRosterWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FName CreatePartnerBackpackEquipment(
		FAutomationTestBase& Test,
		FGameXXKRuntimeState& State,
		const EGameXXKEquipmentSlot Slot)
	{
		FGameXXKEquipmentCreateRequest Request;
		Request.Set = EGameXXKEquipmentSet::XuanJia;
		Request.Quality = EGameXXKEquipmentQuality::Rare;
		Request.ItemLevel = 1;
		Request.bForceSlot = true;
		Request.ForcedSlot = Slot;

		FName InstanceId;
		FString Error;
		if (!Test.TestTrue(
			TEXT("partner-backpack fixture creates an instance equipment item"),
			FGameXXKEquipmentRules::CreateRolledInstance(State.EquipmentCollection, Request, InstanceId, &Error)))
		{
			Test.AddError(Error);
		}
		return InstanceId;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKPartnerBackpackWidgetTest,
	"GameXXK.UI.CompanionRoster.FinalEquipmentAndCardTabs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKPartnerBackpackWidgetTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	TestTrue(TEXT("partner-backpack fixture enters town"), Subsystem && Subsystem->EnsureQingshanTownRuntimeForDirectMap());
	if (!Subsystem)
	{
		return false;
	}

	FGameXXKCompanionRecruitResult Recruit;
	TestTrue(TEXT("partner-backpack fixture recruits one selectable partner"), Subsystem->RecruitPermanentCompanionFromSeed(44601, Recruit));
	TestEqual(TEXT("partner-backpack fixture resolves the recruit immediately"), Recruit.Outcome, EGameXXKCompanionRecruitOutcome::Recruited);
	if (Recruit.Companion.InstanceId.IsNone())
	{
		return false;
	}

	FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
	const FName FirstWeapon = CreatePartnerBackpackEquipment(*this, State, EGameXXKEquipmentSlot::Weapon);
	const FName ReplacementWeapon = CreatePartnerBackpackEquipment(*this, State, EGameXXKEquipmentSlot::Weapon);

	UGameXXKCompanionRosterWidget* Widget = NewObject<UGameXXKCompanionRosterWidget>();
	Widget->SetMVPSubsystem(Subsystem);
	Widget->TakeWidget();
	Widget->RefreshFromState();

	TestEqual(TEXT("the final partner backpack title is the approved concise title"), Widget->GetTitleTextForTest().ToString(), FString(TEXT("伙伴")));
	TestEqual(TEXT("the selected partner has six square equipment slots"), Widget->GetEquipmentSlotCountForTest(), 6);
	TestEqual(TEXT("the equipment page exposes a twenty-cell viewport"), Widget->GetEquipmentBackpackViewportSlotCountForTest(), 20);
	TestEqual(TEXT("the equipment page scrolls through the shared two-hundred-cell warehouse"), Widget->GetEquipmentBackpackStorageCapacityForTest(), FGameXXKEquipmentRules::WarehouseCapacity);
	TestTrue(TEXT("the equipment page owns a real scroll box"), Widget->HasEquipmentBackpackScrollBoxForTest());
	TestTrue(TEXT("the partner backpack initially shows equipment"), Widget->IsEquipmentBackpackTabOpenForTest());
	TestTrue(TEXT("the partner backpack uses the approved close ink"), Widget->GetCloseButtonResourcePathForTest().Contains(TEXT("T_MasterV2_CloseInk")));
	TestTrue(TEXT("the shared warehouse exposes the first weapon"), Widget->GetVisibleEquipmentInstanceIdsForTest().Contains(FirstWeapon));
	TestTrue(TEXT("the shared warehouse exposes the replacement weapon"), Widget->GetVisibleEquipmentInstanceIdsForTest().Contains(ReplacementWeapon));

	const int32 FirstWeaponWarehouseIndex = Widget->GetVisibleEquipmentInstanceIdsForTest().IndexOfByKey(FirstWeapon);
	TestTrue(TEXT("the actual warehouse right-click handler targets the selected partner"), Widget->HandleConfiguredEquipmentSlotRightClicked(
		EGameXXKCompanionEquipmentSlotSource::Warehouse,
		FirstWeaponWarehouseIndex,
		EGameXXKEquipmentSlot::Invalid));
	TestEqual(TEXT("the first weapon fills the selected partner weapon slot"), Widget->GetSelectedCompanionEquippedInstanceForTest(EGameXXKEquipmentSlot::Weapon), FirstWeapon);
	const int32 ReplacementWeaponWarehouseIndex = Widget->GetVisibleEquipmentInstanceIdsForTest().IndexOfByKey(ReplacementWeapon);
	TestTrue(TEXT("the actual warehouse right-click handler replaces an occupied partner slot"), Widget->HandleConfiguredEquipmentSlotRightClicked(
		EGameXXKCompanionEquipmentSlotSource::Warehouse,
		ReplacementWeaponWarehouseIndex,
		EGameXXKEquipmentSlot::Invalid));
	TestEqual(TEXT("the replacement weapon becomes authoritative"), Widget->GetSelectedCompanionEquippedInstanceForTest(EGameXXKEquipmentSlot::Weapon), ReplacementWeapon);
	TestTrue(TEXT("the replaced weapon returns to the shared warehouse"), State.EquipmentCollection.WarehouseInstanceIds.Contains(FirstWeapon));
	TestTrue(TEXT("the actual equipped-slot right-click handler unequips the partner weapon"), Widget->HandleConfiguredEquipmentSlotRightClicked(
		EGameXXKCompanionEquipmentSlotSource::Equipped,
		INDEX_NONE,
		EGameXXKEquipmentSlot::Weapon));
	TestTrue(TEXT("partner unequip clears the six-slot snapshot"), Widget->GetSelectedCompanionEquippedInstanceForTest(EGameXXKEquipmentSlot::Weapon).IsNone());

	TestTrue(TEXT("the card tab can replace the equipment warehouse"), Widget->OpenCardBackpackTabForTest());
	TestTrue(TEXT("the card tab becomes the active right-hand content"), Widget->IsCardBackpackTabOpenForTest());
	TestEqual(TEXT("the selected partner card backpack exposes all eighteen profession cards"), Widget->GetVisiblePersonalCardIds().Num(), 18);
	TestTrue(TEXT("cards use the approved final card frame"), Widget->GetPersonalCardFrameResourcePathForTest().Contains(TEXT("T_MasterV2_CardFrame")));
	TestTrue(TEXT("locked cards use the approved simplified ink lock"), Widget->GetLockedCardIconResourcePathForTest().Contains(TEXT("T_MasterV2_CardLockedIcon")));
	TestTrue(TEXT("the equipment tab can be restored without losing equipment state"), Widget->OpenEquipmentBackpackTabForTest());
	TestTrue(TEXT("returning to equipment preserves the unequipped result"), Widget->GetSelectedCompanionEquippedInstanceForTest(EGameXXKEquipmentSlot::Weapon).IsNone());
	return true;
}

#endif
