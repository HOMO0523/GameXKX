#include "Misc/AutomationTest.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "GameXXKMVPRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKMetaShopWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKMetaShopWidgetTest,
	"GameXXK.MetaShop.Widget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKMetaShopWidgetTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestNotNull(TEXT("meta-shop widget subsystem exists"), Subsystem)
		|| !TestTrue(TEXT("meta-shop widget starts a materialized current game"), Subsystem->StartGame()))
	{
		return false;
	}
	FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
	State.Screen = EGameXXKScreen::Town;
	State.PlayerGold = 1000;

	UGameXXKMetaShopWidget* Widget = NewObject<UGameXXKMetaShopWidget>();
	Widget->SetMVPSubsystem(Subsystem);
	Widget->TakeWidget();
	TestTrue(TEXT("meta shop opens in town"), Widget->OpenMetaShopForTest());
	TestEqual(TEXT("meta shop renders seven product cards"), Widget->GetProductCardCountForTest(), 7);

	TestNotNull(TEXT("meta shop has a paper frame"), Cast<UBorder>(Widget->WidgetTree->FindWidget(TEXT("MetaShopPaperFrame"))));
	TestNull(TEXT("meta shop has no dimming backdrop"), Widget->WidgetTree->FindWidget(TEXT("MetaShopBackdrop")));
	TestNotNull(TEXT("meta shop places absolute page-07 product cards"), Widget->WidgetTree
		? Cast<UButton>(Widget->WidgetTree->FindWidget(TEXT("MetaShopProductCard_0")))
		: nullptr);
	TestNotNull(TEXT("meta shop has the page-07 detail slot"), Widget->WidgetTree->FindWidget(TEXT("MetaShopDetailSlot")));
	TestNotNull(TEXT("meta shop has the page-07 detail icon"), Widget->WidgetTree->FindWidget(TEXT("MetaShopDetailIcon")));
	TestNotNull(TEXT("meta shop has confirmation overlay"), Widget->WidgetTree->FindWidget(TEXT("MetaShopConfirmOverlay")));
	TestNotNull(TEXT("meta shop has result panel"), Widget->WidgetTree->FindWidget(TEXT("MetaShopResultPanel")));
	TestNull(TEXT("meta shop never creates legacy stock grid"), Widget->WidgetTree->FindWidget(TEXT("MerchantStockGrid")));
	TestNull(TEXT("meta shop never creates selling controls"), Widget->WidgetTree->FindWidget(TEXT("SellButton")));

	const TCHAR* ExpectedTexturePaths[] = {
		TEXT("/Game/GameXXK/UI/Equipment/pojun_weapon.pojun_weapon"),
		TEXT("/Game/GameXXK/UI/Equipment/xuanjia_weapon.xuanjia_weapon"),
		TEXT("/Game/GameXXK/UI/Equipment/qingnang_weapon.qingnang_weapon"),
		TEXT("/Game/GameXXK/UI/Equipment/zhuifeng_weapon.zhuifeng_weapon"),
		TEXT("/Game/GameXXK/UI/Equipment/shigu_weapon.shigu_weapon"),
		TEXT("/Game/GameXXK/UI/Equipment/shanhe_weapon.shanhe_weapon"),
		TEXT("/Game/GameXXK/UI/MetaShop/V2/T_MetaShop_CompanionPack.T_MetaShop_CompanionPack"),
	};
	for (int32 Index = 0; Index < 7; ++Index)
	{
		UImage* ProductImage = Cast<UImage>(Widget->WidgetTree->FindWidget(
			FName(*FString::Printf(TEXT("MetaShopProductImage_%d"), Index))));
		TestNotNull(FString::Printf(TEXT("product %d has an image"), Index), ProductImage);
		if (ProductImage)
		{
			const UObject* Resource = ProductImage->GetBrush().GetResourceObject();
			TestNotNull(FString::Printf(TEXT("product %d resolves a texture"), Index), Resource);
			if (Resource)
			{
				TestEqual(FString::Printf(TEXT("product %d uses approved V2 texture"), Index), Resource->GetPathName(), FString(ExpectedTexturePaths[Index]));
			}
		}
	}

	TestTrue(TEXT("player can select PoJun pack"), Widget->SelectProductForTest(EGameXXKMetaShopProductId::PoJunPack));
	UImage* ProductSelection = Cast<UImage>(
		Widget->WidgetTree->FindWidget(TEXT("MetaShopProductSelectionInk")));
	const UObject* ProductSelectionResource = ProductSelection
		? ProductSelection->GetBrush().GetResourceObject()
		: nullptr;
	TestTrue(TEXT("shop rectangular selection uses approved ButtonPurchase"),
		ProductSelectionResource
		&& ProductSelectionResource->GetPathName().Contains(TEXT("T_MasterV2_ButtonPurchase")));
	TestTrue(TEXT("selected affordable product has no disabled reason"), Widget->GetDisabledReasonForTest().IsEmpty());
	const FGameXXKRuntimeState BeforeCancel = State;
	TestTrue(TEXT("purchase request opens confirmation"), Widget->RequestPurchaseForTest());
	TestTrue(TEXT("purchase request can be cancelled"), Widget->CancelPurchaseForTest());
	TestTrue(TEXT("cancelling confirmation does not mutate runtime"),
		FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(&State, &BeforeCancel, PPF_None));

	TestTrue(TEXT("purchase request reopens confirmation"), Widget->RequestPurchaseForTest());
	TestTrue(TEXT("confirmed purchase succeeds"), Widget->ConfirmPurchaseForTest());
	const FGameXXKMetaShopPurchaseResult PurchaseResult = Widget->GetLastPurchaseResultForTest();
	TestTrue(TEXT("widget stores successful purchase result"), PurchaseResult.bPurchased);
	TestEqual(TEXT("widget purchase debits exact price"), State.PlayerGold, 900);

	State.PlayerGold = 0;
	Widget->OpenMetaShopForTest();
	TestTrue(TEXT("player can inspect unaffordable companion pack"), Widget->SelectProductForTest(EGameXXKMetaShopProductId::CompanionPack));
	TestFalse(TEXT("unaffordable product cannot request purchase"), Widget->RequestPurchaseForTest());
	TestFalse(TEXT("unaffordable product exposes disabled reason"), Widget->GetDisabledReasonForTest().IsEmpty());
	return true;
}

#endif
