#include "Misc/AutomationTest.h"

#include "Engine/GameInstance.h"
#include "GameXXKMVPRules.h"
#include "MVP/GameXXKMVPSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKMetaShopFacadeTest,
	"GameXXK.MetaShop.Facade",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKMetaShopFacadeTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
	State = UGameXXKMVPRules::CreateNewGame();
	State.Screen = EGameXXKScreen::Town;
	State.PlayerGold = 1000;

	const TArray<FGameXXKMetaShopProductDefinition> Products = Subsystem->GetMetaShopProducts();
	TestEqual(TEXT("facade exposes seven products"), Products.Num(), 7);
	if (Products.Num() != 7)
	{
		return false;
	}

	const FGameXXKRuntimeState BeforePreview = State;
	FGameXXKMetaShopPurchasePreview Preview;
	TestTrue(TEXT("facade previews a town product"), Subsystem->PreviewMetaShopPurchase(
		EGameXXKMetaShopProductId::PoJunPack,
		Preview));
	TestTrue(TEXT("facade preview is available"), Preview.bAvailable);
	TestTrue(TEXT("facade preview leaves runtime unchanged"),
		FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(&State, &BeforePreview, PPF_None));

	FGameXXKMetaShopPurchaseResult PurchaseResult;
	TestTrue(TEXT("facade commits a town purchase"), Subsystem->PurchaseMetaShopProduct(
		EGameXXKMetaShopProductId::PoJunPack,
		PurchaseResult));
	TestTrue(TEXT("facade returns the committed result"), PurchaseResult.bPurchased);
	TestEqual(TEXT("facade purchase debits permanent gold"), State.PlayerGold, 900);
	TestEqual(TEXT("facade purchase advances shop ordinal"), State.MetaShop.NextPurchaseOrdinal, 1);

	for (const EGameXXKScreen RejectedScreen : {EGameXXKScreen::DungeonMap, EGameXXKScreen::Battle})
	{
		FGameXXKRuntimeState& RejectedState = Subsystem->GetMutableRuntimeState();
		RejectedState = UGameXXKMVPRules::CreateNewGame();
		RejectedState.Screen = RejectedScreen;
		RejectedState.PlayerGold = 1000;
		const FGameXXKRuntimeState BeforeRejectedPurchase = RejectedState;
		FGameXXKMetaShopPurchaseResult RejectedResult;
		TestFalse(TEXT("facade rejects a non-town purchase"), Subsystem->PurchaseMetaShopProduct(
			EGameXXKMetaShopProductId::PoJunPack,
			RejectedResult));
		TestEqual(TEXT("facade reports the non-town error"), RejectedResult.Error, EGameXXKMetaShopError::NotInTown);
		TestTrue(TEXT("facade non-town rejection leaves runtime unchanged"),
			FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(
				&RejectedState,
				&BeforeRejectedPurchase,
				PPF_None));
	}
	return true;
}

#endif
