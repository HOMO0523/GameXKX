#include "Misc/AutomationTest.h"

#include "MVP/GameXXKMVPSubsystem.h"
#include "Engine/GameInstance.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTalentSubsystemFacadeTest,
	"GameXXK.Talents.Facade.AuthoritativePurchaseAndViews",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTalentSubsystemFacadeTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem =
		NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestTrue(TEXT("talent facade fixture starts"), Subsystem && Subsystem->StartGame()))
	{
		return false;
	}
	const TArray<FGameXXKTalentNodeView> InitialViews = Subsystem->GetTalentNodeViews();
	int32 InitiallyVisible = 0;
	for (const FGameXXKTalentNodeView& View : InitialViews)
	{
		InitiallyVisible += View.State != EGameXXKTalentNodeState::Hidden ? 1 : 0;
	}
	TestEqual(TEXT("fresh tree reveals only the center root"), InitiallyVisible, 1);

	Subsystem->GetMutableRuntimeState().PlayerGold = 2500;
	FGameXXKTalentPurchaseResult Result;
	TestTrue(TEXT("facade purchases root"), Subsystem->PurchaseTalentNode(TEXT("Talent.Root"), Result));
	TestEqual(TEXT("facade commit persists root"),
		Subsystem->GetRuntimeState().Talents.NodeRanks.FindRef(TEXT("Talent.Root")), 1);
	TestEqual(TEXT("facade commit unlocks Warehouse page two"),
		Subsystem->GetTalentProjection().WarehousePageCount, 2);
	int32 VisibleAfterRoot = 0;
	for (const FGameXXKTalentNodeView& View : Subsystem->GetTalentNodeViews())
	{
		VisibleAfterRoot += View.State != EGameXXKTalentNodeState::Hidden ? 1 : 0;
	}
	TestEqual(TEXT("root reveals exactly four branch entries"), VisibleAfterRoot, 5);

	const FGameXXKRuntimeState BeforeInvalid = Subsystem->GetRuntimeStateCopy();
	TestFalse(TEXT("facade rejects unknown talent"),
		Subsystem->PurchaseTalentNode(TEXT("Talent.Unknown"), Result));
	TestTrue(TEXT("unknown talent leaves authoritative state unchanged"),
		FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(
			&BeforeInvalid,
			&Subsystem->GetRuntimeState(),
			PPF_None));
	return true;
}

#endif
