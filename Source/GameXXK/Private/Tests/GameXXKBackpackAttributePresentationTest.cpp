#include "Misc/AutomationTest.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/GameInstance.h"
#include "GameXXKEquipmentRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKCharacterUiPresentation.h"
#include "UI/GameXXKDesktopTrainingWorkbenchWidget.h"
#include "UI/GameXXKInventoryWindowWidget.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKBackpackAttributePresentationTest,
	"GameXXK.DesktopTraining.BackpackAttributes.FriendlyNamesAndLiveValues",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBackpackAttributePresentationTest::RunTest(const FString& Parameters)
{
	auto* Sub=NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if(!Sub->StartGame()) return false;
	auto* Workbench=NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	Workbench->SetMVPSubsystem(Sub); Workbench->ConstructForTest(); Workbench->OpenBackpack();
	TArray<FName> Ids={FGameXXKEquipmentRules::HeroCharacterId()};
	Ids.Append(Workbench->GetCompanionCharacterIdsForTest());
	Ids.Append(Workbench->GetNpcCharacterIdsForTest());
	TestEqual(TEXT("the fixture covers hero, six partners and six NPCs"),Ids.Num(),13);
	for(const FName Id:Ids)
	{
		if(!TestTrue(TEXT("the owned character opens"),Workbench->SelectBackpackCharacterForTest(Id))) continue;
		auto* Inventory=Cast<UGameXXKInventoryWindowWidget>(Workbench->WidgetTree->FindWidget(TEXT("EmbeddedApprovedBackpack")));
		if(!TestNotNull(TEXT("the real attribute editor exists"),Inventory)) continue;
		Inventory->OpenCharacterBackpackTabForTest(EGameXXKCharacterBackpackTab::Attributes);
		const FString Body=Inventory->GetCharacterTabBodyTextForTest().ToString();
		TestFalse(TEXT("attributes never expose companion instance IDs"),Body.Contains(TEXT("CompanionInstance")));
		TestFalse(TEXT("attributes never expose NPC backend IDs"),Body.Contains(TEXT("Npc.")));
		TestTrue(TEXT("the visible summary includes the friendly name"),Body.Contains(GameXXKCharacterUiPresentation::GetDisplayName(Sub,Id)));
		FGameXXKEquipmentLoadoutSnapshot Snapshot;
		if(TestTrue(TEXT("the authoritative loadout is available"),Sub->GetEquipmentLoadoutSnapshot(Id,Snapshot)))
		{
			TestTrue(TEXT("the displayed attack follows the loadout snapshot"),Body.Contains(FString::Printf(TEXT("攻击 %d"),Snapshot.AttributesBeforeRoute.Attack)));
			TestTrue(TEXT("the displayed defense follows the loadout snapshot"),Body.Contains(FString::Printf(TEXT("防御 %d"),Snapshot.AttributesBeforeRoute.Defense)));
		}
	}
	TestEqual(TEXT("the NPC presentation uses its player-facing name"),GameXXKCharacterUiPresentation::GetDisplayName(Sub,TEXT("Npc.TusiChief")),FString(TEXT("土司首领")));
	return true;
}
#endif
