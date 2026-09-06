#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/SafeZone.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "GameXXKCardCatalog.h"
#include "GameXXKCardQualityRules.h"
#include "GameXXKCompanionCatalog.h"
#include "GameXXKCompanionRules.h"
#include "GameXXKEquipmentRules.h"
#include "GameXXKMVPRules.h"
#include "GameXXKPartyFormationRules.h"
#include "GameXXKRouteMerchantTypes.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKRouteMerchantWidget.h"

namespace GameXXKRouteMerchantWidgetTest
{
	FGameXXKRuntimeState MakeMerchantFixture(const int32 CarriedCardCount = 8)
	{
		FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
		State.Screen = EGameXXKScreen::DungeonMap;
		State.CurrentMapId = TEXT("HuangshanRoute");
		State.bDungeonActive = true;
		State.bHasGeneratedRouteMap = true;
		State.RouteSeed = 0x6137;
		State.CurrentRouteNodeId = 9;
		State.PendingRouteNodeId = INDEX_NONE;
		State.RouteMapNodes = {
			FGameXXKRouteMapNode{9, 1, 0, EGameXXKNodeKind::Start, FVector2D(0.25f, 0.5f), TArray<int32>{10}},
			FGameXXKRouteMapNode{10, 2, 1, EGameXXKNodeKind::Merchant, FVector2D(0.55f, 0.5f), TArray<int32>{}}};
		State.RouteMapEdges = {FGameXXKRouteMapEdge{9, 10}};
		State.VisitedRouteNodeIds = {9};
		State.ReachableRouteNodeIds = {10};
		State.CardRun.RouteProgress.SchemaVersion = 1;
		State.CardRun.RouteProgress.RootSeed = State.RouteSeed;
		State.CardRun.RouteProgress.ChapterSeeds = {State.RouteSeed};
		State.CardRun.RouteProgress.CurrentChapter = 1;
		State.CardRun.RouteProgress.RouteCombatLevel = 1;
		State.CardRun.bLoadoutLockedForRoute = true;
		State.CardRun.bRouteEconomyInitialized = true;
		State.PlayerGold = 100000;
		State.CardRun.RouteTravelMoney = 777;

		State.CardRun.HeroUnlockedCardIds.Reset();
		State.CardRun.HeroSelectedCardIds.Reset();
		for (const FGameXXKCardDefinition& Definition : FGameXXKCardCatalog::GetAllCardDefinitions())
		{
			if (Definition.Owner == EGameXXKCardOwner::Hero
				&& FGameXXKCardQualityRules::GetCardBaseQuality(Definition.Id) < EGameXXKCardQuality::Epic)
			{
				State.CardRun.HeroUnlockedCardIds.Add(Definition.Id);
				State.CardRun.HeroSelectedCardIds.Add(Definition.Id);
				if (State.CardRun.HeroSelectedCardIds.Num() == CarriedCardCount)
				{
					break;
				}
			}
		}
		return State;
	}

	UGameXXKRouteMerchantWidget* MakeWidget(UGameXXKMVPSubsystem* Subsystem)
	{
		UGameXXKRouteMerchantWidget* Widget = NewObject<UGameXXKRouteMerchantWidget>();
		Widget->SetMVPSubsystem(Subsystem);
		Widget->Initialize();
		Widget->NativeConstruct();
		Widget->RefreshFromState();
		return Widget;
	}

	template <typename WidgetType>
	WidgetType* FindWidget(const UGameXXKRouteMerchantWidget* Widget, const FName WidgetName)
	{
		return Widget && Widget->WidgetTree
			? Cast<WidgetType>(Widget->WidgetTree->FindWidget(WidgetName))
			: nullptr;
	}

	FString ReadText(const UGameXXKRouteMerchantWidget* Widget, const FName WidgetName)
	{
		const UTextBlock* TextBlock = FindWidget<UTextBlock>(Widget, WidgetName);
		return TextBlock ? TextBlock->GetText().ToString() : FString();
	}

	int32 FindCardOfferIndex(const FGameXXKRouteMerchantView& View, const FName OfferId)
	{
		return View.CardOffers.IndexOfByPredicate([OfferId](const FGameXXKRouteMerchantOfferView& Offer)
		{
			return Offer.SavedOffer.OfferId == OfferId;
		});
	}

	TArray<FName> CollectPurchasableOfferIds(const FGameXXKRouteMerchantView& View)
	{
		TArray<FName> Result;
		for (const FGameXXKRouteMerchantOfferView& Offer : View.CardOffers)
		{
			if (Offer.bPurchaseEnabled)
			{
				Result.Add(Offer.SavedOffer.OfferId);
			}
		}
		return Result;
	}

	FString FriendlyQuestNpcName(const FName NpcId)
	{
		if (NpcId == TEXT("Npc.TusiChief")) return TEXT("土司首领");
		if (NpcId == TEXT("Npc.SongJinBao")) return TEXT("宋金宝");
		if (NpcId == TEXT("Npc.YueBai")) return TEXT("月白");
		if (NpcId == TEXT("Npc.ZhouGuangZu")) return TEXT("周光祖");
		if (NpcId == TEXT("Npc.JinGui")) return TEXT("金贵");
		if (NpcId == TEXT("Npc.QiongMeiEr")) return TEXT("琼梅儿");
		return FString();
	}

	FGameXXKRuntimeState MakeMixedOwnerMerchantFixture(
		FName& OutCompanionId,
		FString& OutCompanionName,
		FName& OutQuestNpcId,
		FString& OutQuestNpcName)
	{
		FGameXXKRuntimeState State = MakeMerchantFixture();
		State.CardRun.HeroSelectedCardIds.SetNum(FMath::Min(1, State.CardRun.HeroSelectedCardIds.Num()));

		FGameXXKPermanentCompanion Companion;
		Companion.InstanceId = TEXT("Companion.Widget.Mixed.Blade");
		Companion.Role = EGameXXKCharacterRole::Blade;
		Companion.NameSeed = 7331;
		Companion.bIsActive = true;
		for (const FGameXXKCardDefinition& Definition : FGameXXKCardCatalog::GetAllCardDefinitions())
		{
			if (Definition.Owner == EGameXXKCardOwner::Profession
				&& Definition.Role == Companion.Role
				&& FGameXXKCardQualityRules::GetCardBaseQuality(Definition.Id) < EGameXXKCardQuality::Epic)
			{
				Companion.SelectedCardIds = {Definition.Id};
				break;
			}
		}
		State.CardRun.CompanionRoster.PermanentCompanions.Reset();
		State.CardRun.CompanionRoster.PermanentCompanions.Add(Companion);
		OutCompanionId = Companion.InstanceId;
		OutCompanionName = FGameXXKCompanionRules::GetCompanionDisplayName(Companion.Role, Companion.NameSeed);

		for (const FGameXXKQuestNpcDefinition& QuestNpc : FGameXXKCompanionCatalog::GetQuestNpcDefinitions())
		{
			TArray<FName> EligibleCards;
			for (const FName CardId : QuestNpc.FixedCardIds)
			{
				if (FGameXXKCardQualityRules::GetCardBaseQuality(CardId) < EGameXXKCardQuality::Epic)
				{
					EligibleCards.Add(CardId);
					if (EligibleCards.Num() == 2)
					{
						break;
					}
				}
			}
			const FString FriendlyName = FriendlyQuestNpcName(QuestNpc.NpcId);
			if (EligibleCards.Num() == 2 && !FriendlyName.IsEmpty())
			{
				OutQuestNpcId = QuestNpc.NpcId;
				OutQuestNpcName = FriendlyName;
				State.CardRun.PartySelection.QuestNpcCardLoadouts.FindOrAdd(
					QuestNpc.NpcId).SelectedCardIds = MoveTemp(EligibleCards);
				FGameXXKPartyMemberRef HeroRef;
				HeroRef.Kind = EGameXXKPartyMemberKind::Hero;
				HeroRef.MemberId = FGameXXKEquipmentRules::HeroCharacterId();
				FGameXXKPartyMemberRef CompanionRef;
				CompanionRef.Kind = EGameXXKPartyMemberKind::PermanentCompanion;
				CompanionRef.MemberId = Companion.InstanceId;
				FGameXXKPartyMemberRef NpcRef;
				NpcRef.Kind = EGameXXKPartyMemberKind::QuestNpc;
				NpcRef.MemberId = QuestNpc.NpcId;
				State.CardRun.OrderedFormation.Members = {
					HeroRef, CompanionRef, NpcRef};
				FGameXXKPartyFormationRules::ProjectCompatibility(State);
				break;
			}
		}
		return State;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteMerchantWidgetStructureTest,
	"GameXXK.MVP.RouteMerchant.Widget.TwoCompactRowsOfEightOffers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteMerchantWidgetStructureTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKRouteMerchantWidgetTest;
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(GameInstance);
	Subsystem->GetMutableRuntimeState() = MakeMerchantFixture();
	TestTrue(TEXT("merchant node opens and generates stable stock"), Subsystem->SelectRouteNodeById(10));
	UGameXXKRouteMerchantWidget* Widget = MakeWidget(Subsystem);

	TestNotNull(TEXT("dedicated merchant widget exists"), Widget);
	TestEqual(TEXT("merchant root is self-hit-test-invisible"), Widget->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
	TestEqual(TEXT("merchant design width targets 1920"), Widget->GetDesignResolutionForTest().X, 1920.0);
	TestEqual(TEXT("merchant design height targets 1080"), Widget->GetDesignResolutionForTest().Y, 1080.0);
	TestTrue(TEXT("no empty merchant placeholder column"), FMath::IsNearlyEqual(Widget->GetMerchantColumnFractionForTest(), 0.0f, 0.01f));
	TestTrue(TEXT("offers use the full content area"), FMath::IsNearlyEqual(Widget->GetOffersColumnFractionForTest(), 1.0f, 0.01f));
	TestTrue(TEXT("programmatic root contains a safe-area widget"), FindWidget<USafeZone>(Widget, TEXT("RouteMerchantSafeArea")) != nullptr);

	UHorizontalBox* CardRow = FindWidget<UHorizontalBox>(Widget, TEXT("RouteMerchantCardRow"));
	UHorizontalBox* RelicRow = FindWidget<UHorizontalBox>(Widget, TEXT("RouteMerchantRelicRow"));
	TestNotNull(TEXT("top card row exists"), CardRow);
	TestEqual(TEXT("card row renders exactly four portrait cells"), CardRow ? CardRow->GetChildrenCount() : 0, 4);
	TestNotNull(TEXT("bottom relic row exists"), RelicRow);
	TestEqual(TEXT("relic row renders exactly four compact cells"), RelicRow ? RelicRow->GetChildrenCount() : 0, 4);
	TestEqual(TEXT("read model renders exactly four card offers"), Widget->GetRenderedCardOfferCountForTest(), 4);
	TestEqual(TEXT("read model renders exactly four relic offers"), Widget->GetRenderedRelicOfferCountForTest(), 4);
	TestTrue(TEXT("card offer frames are portrait-shaped"), Widget->GetCardFrameSizeForTest().Y > Widget->GetCardFrameSizeForTest().X);
	TestTrue(TEXT("card offer frames leave space for a larger readable title"),
		Widget->GetCardFrameSizeForTest().X >= 200.0f && Widget->GetCardFrameSizeForTest().Y >= 280.0f);
	USizeBox* RelicVisual = FindWidget<USizeBox>(Widget, TEXT("RouteMerchantOfferVisualSize4"));
	TestTrue(TEXT("relic offer uses a shallow contained visual"),
		RelicVisual
		&& RelicVisual->GetWidthOverride() > RelicVisual->GetHeightOverride()
		&& RelicVisual->GetWidthOverride() <= 310.0f);
	TestTrue(TEXT("card row reuses the shared approved MasterV2 frame"), Widget->GetCardFrameResourcePathForTest().Contains(TEXT("T_MasterV2_CardFrame")));
	TestEqual(TEXT("all eight offer bodies expose tooltips"), Widget->GetOfferTooltipCountForTest(), 8);
	TestTrue(TEXT("only buttons remain hit-testable in the merchant tree"), Widget->HasOnlyButtonHitTargetsForTest());

	TestNull(TEXT("merchant has no top-right close button"), Widget->WidgetTree->FindWidget(TEXT("RouteMerchantCloseButton")));
	TestNull(TEXT("merchant has no close-ink widget"), Widget->WidgetTree->FindWidget(TEXT("RouteMerchantCloseInk")));
	TestFalse(TEXT("merchant close-button seam is permanently false"), Widget->HasTopRightCloseButtonForTest());
	TestNull(TEXT("merchant has no replacement selection panel"), Widget->WidgetTree->FindWidget(TEXT("RouteMerchantReplacementSelectionPanel")));
	TestNull(TEXT("merchant has no cancel-replacement button"), Widget->WidgetTree->FindWidget(TEXT("RouteMerchantCancelButton")));
	TestNotNull(TEXT("merchant retains a visible refresh button"), FindWidget<UButton>(Widget, TEXT("RouteMerchantRefreshButton")));
	TestNotNull(TEXT("merchant retains leave as the visible exit"), FindWidget<UButton>(Widget, TEXT("RouteMerchantLeaveButton")));
	TestTrue(TEXT("leave action is explicitly labelled"), ReadText(Widget, TEXT("RouteMerchantLeaveLabel")).Contains(TEXT("离开商店")));

	const FString GoldCopy = ReadText(Widget, TEXT("RouteMerchantOrdinaryGold"));
	TestEqual(TEXT("ordinary-gold test seam matches the visible label"), Widget->GetOrdinaryGoldTextForTest().ToString(), GoldCopy);
	TestTrue(TEXT("ordinary gold label identifies normal gold"), GoldCopy.Contains(TEXT("金币")));
	TestFalse(TEXT("ordinary gold label never calls the balance travel money"), GoldCopy.Contains(TEXT("行旅钱")));
	TestTrue(TEXT("ordinary gold label displays PlayerGold"), GoldCopy.Contains(TEXT("100,000")) || GoldCopy.Contains(TEXT("100000")));

	FGameXXKRouteMerchantView View;
	FString Error;
	TestTrue(TEXT("four-card view remains available"), Subsystem->GetRouteMerchantView(View, &Error));
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const FString Owner = ReadText(Widget, *FString::Printf(TEXT("RouteMerchantOfferOwner%d"), Index));
		const FString Name = ReadText(Widget, *FString::Printf(TEXT("RouteMerchantOfferName%d"), Index));
		const FString Quality = ReadText(Widget, *FString::Printf(TEXT("RouteMerchantOfferQuality%d"), Index));
		const FString Effect = ReadText(Widget, *FString::Printf(TEXT("RouteMerchantOfferEffect%d"), Index));
		const FString Price = ReadText(Widget, *FString::Printf(TEXT("RouteMerchantOfferPrice%d"), Index));
		TestFalse(*FString::Printf(TEXT("slot %d displays a friendly owner"), Index), Owner.IsEmpty());
		TestTrue(*FString::Printf(TEXT("slot %d hero owner is friendly rather than raw Player id"), Index), Owner.Contains(TEXT("主角")) && !Owner.Contains(TEXT("Player")));
		TestFalse(*FString::Printf(TEXT("slot %d displays the card name"), Index), Name.IsEmpty());
		TestTrue(*FString::Printf(TEXT("slot %d displays current to next quality"), Index), Quality.Contains(TEXT("→")));
		TestFalse(*FString::Printf(TEXT("slot %d displays a next-quality effect preview"), Index), Effect.IsEmpty());
		TestTrue(*FString::Printf(TEXT("slot %d price is ordinary gold"), Index), Price.Contains(TEXT("金币")) && !Price.Contains(TEXT("行旅钱")));
		UImage* Art = FindWidget<UImage>(Widget, *FString::Printf(TEXT("RouteMerchantOfferArt%d"), Index));
		TestTrue(*FString::Printf(TEXT("slot %d displays card art"), Index), Art && Art->GetVisibility() != ESlateVisibility::Collapsed);
	}
	for (int32 Index = 4; Index < 8; ++Index)
	{
		const FString Owner = ReadText(Widget, *FString::Printf(TEXT("RouteMerchantOfferOwner%d"), Index));
		const FString Name = ReadText(Widget, *FString::Printf(TEXT("RouteMerchantOfferName%d"), Index));
		const FString Quality = ReadText(Widget, *FString::Printf(TEXT("RouteMerchantOfferQuality%d"), Index));
		const FString Effect = ReadText(Widget, *FString::Printf(TEXT("RouteMerchantOfferEffect%d"), Index));
		const FString Price = ReadText(Widget, *FString::Printf(TEXT("RouteMerchantOfferPrice%d"), Index));
		TestTrue(*FString::Printf(TEXT("relic slot %d uses a simple relic label"), Index), Owner.Contains(TEXT("遗物")));
		TestFalse(*FString::Printf(TEXT("relic slot %d displays its name"), Index), Name.IsEmpty());
		TestFalse(*FString::Printf(TEXT("relic slot %d displays its quality"), Index), Quality.IsEmpty());
		TestFalse(*FString::Printf(TEXT("relic slot %d displays its concise effect"), Index), Effect.IsEmpty());
		TestTrue(*FString::Printf(TEXT("relic slot %d price is ordinary gold"), Index), Price.Contains(TEXT("金币")) && !Price.Contains(TEXT("行旅钱")));
		UImage* Art = FindWidget<UImage>(Widget, *FString::Printf(TEXT("RouteMerchantOfferArt%d"), Index));
		TestTrue(*FString::Printf(TEXT("relic slot %d displays catalog icon art"), Index), Art && Art->GetVisibility() != ESlateVisibility::Collapsed);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteMerchantWidgetMixedOwnerLabelsTest,
	"GameXXK.MVP.RouteMerchant.Widget.MixedPartyOwnerLabelsAreFriendly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteMerchantWidgetMixedOwnerLabelsTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKRouteMerchantWidgetTest;
	FName CompanionId = NAME_None;
	FString CompanionName;
	FName QuestNpcId = NAME_None;
	FString QuestNpcName;
	FGameXXKRuntimeState State = MakeMixedOwnerMerchantFixture(
		CompanionId,
		CompanionName,
		QuestNpcId,
		QuestNpcName);
	TestFalse(TEXT("mixed fixture has a stable companion id"), CompanionId.IsNone());
	TestFalse(TEXT("mixed fixture has a friendly companion name"), CompanionName.IsEmpty());
	TestFalse(TEXT("mixed fixture has a stable quest NPC id"), QuestNpcId.IsNone());
	TestFalse(TEXT("mixed fixture has a friendly quest NPC name"), QuestNpcName.IsEmpty());

	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(GameInstance);
	Subsystem->GetMutableRuntimeState() = MoveTemp(State);
	TestTrue(TEXT("mixed-owner merchant opens"), Subsystem->SelectRouteNodeById(10));
	UGameXXKRouteMerchantWidget* Widget = MakeWidget(Subsystem);
	FGameXXKRouteMerchantView View;
	FString Error;
	TestTrue(TEXT("mixed-owner merchant exposes a view"), Subsystem->GetRouteMerchantView(View, &Error));
	TestEqual(TEXT("mixed-owner fixture produces exactly four offers"), View.CardOffers.Num(), 4);

	int32 HeroOwnerCount = 0;
	int32 CompanionOwnerCount = 0;
	int32 QuestNpcOwnerCount = 0;
	for (int32 Index = 0; Index < View.CardOffers.Num(); ++Index)
	{
		const FName OwnerId = View.CardOffers[Index].SavedOffer.OwnerMemberId;
		const FString OwnerText = ReadText(
			Widget,
			*FString::Printf(TEXT("RouteMerchantOfferOwner%d"), Index));
		TestFalse(*FString::Printf(TEXT("offer %d owner label is never empty"), Index), OwnerText.IsEmpty());
		TestFalse(*FString::Printf(TEXT("offer %d owner label never exposes its raw id"), Index),
			OwnerText.Contains(OwnerId.ToString()));
		if (OwnerId == TEXT("Player"))
		{
			++HeroOwnerCount;
			TestTrue(TEXT("hero offer uses the friendly protagonist label"), OwnerText.Contains(TEXT("主角")));
		}
		else if (OwnerId == CompanionId)
		{
			++CompanionOwnerCount;
			TestTrue(TEXT("companion offer uses the existing friendly companion name"), OwnerText.Contains(CompanionName));
		}
		else if (OwnerId == QuestNpcId)
		{
			++QuestNpcOwnerCount;
			TestTrue(TEXT("quest NPC offer uses the friendly catalog label"), OwnerText.Contains(QuestNpcName));
			const auto* Art = FindWidget<UImage>(Widget,*FString::Printf(TEXT("RouteMerchantOfferArt%d"),Index));
			TestTrue(TEXT("quest NPC offers reuse the existing battle card portrait"),Art && Art->GetBrush().GetResourceObject()
				&& Art->GetBrush().GetResourceObject()->GetPathName().Contains(TEXT("T_CardPortrait_Npc_")));
		}
	}
	TestEqual(TEXT("mixed stock contains one hero-owned card"), HeroOwnerCount, 1);
	TestEqual(TEXT("mixed stock contains one companion-owned card"), CompanionOwnerCount, 1);
	TestEqual(TEXT("mixed stock contains two quest-NPC-owned cards"), QuestNpcOwnerCount, 2);

	Subsystem->GetMutableRuntimeState().CardRun.RouteMerchant.Offers[0].OwnerMemberId = TEXT("Unknown.Owner.Fallback");
	Widget->RefreshFromState();
	const FString FallbackText = ReadText(Widget, TEXT("RouteMerchantOfferOwner0"));
	TestTrue(TEXT("unknown stale owner uses a stable friendly fallback"), FallbackText.Contains(TEXT("队伍成员")));
	TestFalse(TEXT("unknown stale owner never leaks its raw id"), FallbackText.Contains(TEXT("Unknown.Owner.Fallback")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteMerchantWidgetActionsTest,
	"GameXXK.MVP.RouteMerchant.Widget.PurchaseTwoRefreshUnsoldAndLeave",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteMerchantWidgetActionsTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKRouteMerchantWidgetTest;
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(GameInstance);
	Subsystem->GetMutableRuntimeState() = MakeMerchantFixture();
	TestTrue(TEXT("merchant node opens for action test"), Subsystem->SelectRouteNodeById(10));
	UGameXXKRouteMerchantWidget* Widget = MakeWidget(Subsystem);

	FGameXXKRouteMerchantView InitialView;
	FString Error;
	TestTrue(TEXT("merchant view is available before purchase"), Subsystem->GetRouteMerchantView(InitialView, &Error));
	const TArray<FName> PurchasableOfferIds = CollectPurchasableOfferIds(InitialView);
	TestTrue(TEXT("affordable four-card fixture enables at least two independent purchases"), PurchasableOfferIds.Num() >= 2);
	if (PurchasableOfferIds.Num() < 2)
	{
		return false;
	}

	const int32 FirstInitialIndex = FindCardOfferIndex(InitialView, PurchasableOfferIds[0]);
	const int32 SecondInitialIndex = FindCardOfferIndex(InitialView, PurchasableOfferIds[1]);
	const FGameXXKRouteMerchantOfferView FirstOffer = InitialView.CardOffers[FirstInitialIndex];
	const FGameXXKRouteMerchantOfferView SecondOffer = InitialView.CardOffers[SecondInitialIndex];
	const int32 InitialPlayerGold = Subsystem->GetRuntimeState().PlayerGold;
	const int32 InitialRouteTravelMoney = Subsystem->GetRuntimeState().CardRun.RouteTravelMoney;
	const int32 ExpectedAfterTwoPurchases = InitialPlayerGold - FirstOffer.SavedOffer.Price - SecondOffer.SavedOffer.Price;
	const FName FirstCardId = FirstOffer.SavedOffer.ContentId;
	const FName SecondCardId = SecondOffer.SavedOffer.ContentId;
	const EGameXXKCardQuality FirstNextQuality = FirstOffer.SavedOffer.NextQuality;
	const EGameXXKCardQuality SecondNextQuality = SecondOffer.SavedOffer.NextQuality;

	UButton* FirstPurchaseButton = FindWidget<UButton>(Widget, *FString::Printf(TEXT("RouteMerchantOfferBuy%d"), FirstInitialIndex));
	TestTrue(TEXT("first portrait card exposes a real purchase button delegate"), FirstPurchaseButton && FirstPurchaseButton->GetIsEnabled());
	if (!FirstPurchaseButton)
	{
		return false;
	}
	FirstPurchaseButton->OnClicked.Broadcast();
	TestTrue(TEXT("first button click commits the carried-card upgrade"), Widget->GetLastPurchaseResultForTest().bPurchased);
	TestFalse(TEXT("first purchased slot remains visible but sold"), Widget->IsOfferPurchaseEnabledForTest(PurchasableOfferIds[0]));
	TestTrue(TEXT("first purchased slot visibly says sold"),
		ReadText(Widget, *FString::Printf(TEXT("RouteMerchantOfferBuyLabel%d"), FirstInitialIndex)).Contains(TEXT("已售出")));

	FGameXXKRouteMerchantView AfterFirstView;
	TestTrue(TEXT("view refreshes after first purchase"), Subsystem->GetRouteMerchantView(AfterFirstView, &Error));
	const int32 SecondIndexAfterFirst = FindCardOfferIndex(AfterFirstView, PurchasableOfferIds[1]);
	UButton* SecondPurchaseButton = FindWidget<UButton>(Widget, *FString::Printf(TEXT("RouteMerchantOfferBuy%d"), SecondIndexAfterFirst));
	TestTrue(TEXT("second portrait card remains independently purchasable"), SecondPurchaseButton && SecondPurchaseButton->GetIsEnabled());
	if (!SecondPurchaseButton)
	{
		return false;
	}
	SecondPurchaseButton->OnClicked.Broadcast();

	const FGameXXKRuntimeState& AfterPurchases = Subsystem->GetRuntimeState();
	TestEqual(TEXT("two purchases debit only ordinary PlayerGold"), AfterPurchases.PlayerGold, ExpectedAfterTwoPurchases);
	TestEqual(TEXT("merchant purchases never debit RouteTravelMoney"), AfterPurchases.CardRun.RouteTravelMoney, InitialRouteTravelMoney);
	const EGameXXKCardQuality* FirstUpgrade = AfterPurchases.CardRun.UpgradedCardQualities.Find(FirstCardId);
	const EGameXXKCardQuality* SecondUpgrade = AfterPurchases.CardRun.UpgradedCardQualities.Find(SecondCardId);
	TestTrue(TEXT("first carried card is upgraded in authoritative state"), FirstUpgrade && *FirstUpgrade == FirstNextQuality);
	TestTrue(TEXT("second carried card is upgraded in authoritative state"), SecondUpgrade && *SecondUpgrade == SecondNextQuality);
	TestFalse(TEXT("second purchased slot is sold and cannot be bought twice"), Widget->IsOfferPurchaseEnabledForTest(PurchasableOfferIds[1]));
	TestTrue(TEXT("second purchased slot visibly says sold"),
		ReadText(Widget, *FString::Printf(TEXT("RouteMerchantOfferBuyLabel%d"), SecondIndexAfterFirst)).Contains(TEXT("已售出")));

	TMap<int32, FName> SoldContentBySlot;
	for (int32 Index = 0; Index < AfterPurchases.CardRun.RouteMerchant.Offers.Num(); ++Index)
	{
		if (AfterPurchases.CardRun.RouteMerchant.Offers[Index].bSold)
		{
			SoldContentBySlot.Add(Index, AfterPurchases.CardRun.RouteMerchant.Offers[Index].ContentId);
		}
	}
	const int32 GoldBeforeRefresh = AfterPurchases.PlayerGold;
	const FString RefreshBefore = ReadText(Widget, TEXT("RouteMerchantRefreshLabel"));
	UButton* RefreshButton = FindWidget<UButton>(Widget, TEXT("RouteMerchantRefreshButton"));
	TestTrue(TEXT("refresh is a real enabled button"), RefreshButton && RefreshButton->GetIsEnabled());
	TestTrue(TEXT("first refresh displays its ordinary-gold fee"), RefreshBefore.Contains(TEXT("20")) && RefreshBefore.Contains(TEXT("金币")));
	if (!RefreshButton)
	{
		return false;
	}
	RefreshButton->OnClicked.Broadcast();

	const FGameXXKRuntimeState& AfterRefresh = Subsystem->GetRuntimeState();
	TestEqual(TEXT("refresh advances stable stock count"), AfterRefresh.CardRun.RouteMerchant.RefreshCount, 1);
	TestEqual(TEXT("refresh debits PlayerGold"), AfterRefresh.PlayerGold, GoldBeforeRefresh - 20);
	TestEqual(TEXT("refresh never debits RouteTravelMoney"), AfterRefresh.CardRun.RouteTravelMoney, InitialRouteTravelMoney);
	for (const TPair<int32, FName>& Sold : SoldContentBySlot)
	{
		TestTrue(*FString::Printf(TEXT("sold slot %d remains sold after refresh"), Sold.Key),
			AfterRefresh.CardRun.RouteMerchant.Offers.IsValidIndex(Sold.Key)
			&& AfterRefresh.CardRun.RouteMerchant.Offers[Sold.Key].bSold);
		TestEqual(*FString::Printf(TEXT("sold slot %d keeps the purchased card"), Sold.Key),
			AfterRefresh.CardRun.RouteMerchant.Offers[Sold.Key].ContentId,
			Sold.Value);
	}
	const FString RefreshAfter = ReadText(Widget, TEXT("RouteMerchantRefreshLabel"));
	TestTrue(TEXT("next refresh displays the increased thirty-gold fee"), RefreshAfter.Contains(TEXT("30")) && RefreshAfter.Contains(TEXT("金币")));

	UButton* LeaveButton = FindWidget<UButton>(Widget, TEXT("RouteMerchantLeaveButton"));
	TestTrue(TEXT("leave merchant is the only visible exit and is actionable"), LeaveButton && LeaveButton->GetIsEnabled());
	if (!LeaveButton)
	{
		return false;
	}
	LeaveButton->OnClicked.Broadcast();
	TestEqual(TEXT("leave settles and returns to the route map"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::DungeonMap);
	TestTrue(TEXT("leave marks the merchant node visited"), Subsystem->GetRuntimeState().VisitedRouteNodeIds.Contains(10));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteMerchantWidgetRebuildIdempotencyTest,
	"GameXXK.MVP.RouteMerchant.Widget.RebuildAndReopenRemainIdempotent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteMerchantWidgetRebuildIdempotencyTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKRouteMerchantWidgetTest;
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(GameInstance);
	Subsystem->GetMutableRuntimeState() = MakeMerchantFixture();
	TestTrue(TEXT("rebuild fixture opens merchant"), Subsystem->SelectRouteNodeById(10));
	UGameXXKRouteMerchantWidget* Widget = MakeWidget(Subsystem);

	Widget->TakeWidget();
	Widget->RebuildWidget();
	Widget->NativeConstruct();
	Widget->TakeWidget();
	Widget->RebuildWidget();
	Widget->NativeConstruct();
	Widget->SetVisibility(ESlateVisibility::Collapsed);
	Widget->RefreshFromState();

	UHorizontalBox* CardRow = FindWidget<UHorizontalBox>(Widget, TEXT("RouteMerchantCardRow"));
	UHorizontalBox* RelicRow = FindWidget<UHorizontalBox>(Widget, TEXT("RouteMerchantRelicRow"));
	TestEqual(TEXT("repeated build and reopen keep exactly four card cells"),
		CardRow ? CardRow->GetChildrenCount() : 0, 4);
	TestEqual(TEXT("repeated build and reopen keep exactly four relic cells"),
		RelicRow ? RelicRow->GetChildrenCount() : 0, 4);
	TestEqual(TEXT("repeated build and reopen keep exactly eight tooltips"),
		Widget->GetOfferTooltipCountForTest(), 8);
	TestEqual(TEXT("reopen restores the merchant HUD"),
		Widget->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);

	const int32 GoldBeforeRefresh = Subsystem->GetRuntimeState().PlayerGold;
	UButton* RefreshButton = FindWidget<UButton>(Widget, TEXT("RouteMerchantRefreshButton"));
	TestTrue(TEXT("rebuilt refresh button remains enabled"), RefreshButton && RefreshButton->GetIsEnabled());
	if (RefreshButton)
	{
		RefreshButton->OnClicked.Broadcast();
	}
	TestEqual(TEXT("one rebuilt refresh click fires exactly one delegate"),
		Subsystem->GetRuntimeState().CardRun.RouteMerchant.RefreshCount, 1);
	TestEqual(TEXT("one rebuilt refresh click charges exactly one refresh"),
		Subsystem->GetRuntimeState().PlayerGold, GoldBeforeRefresh - 20);

	UButton* LeaveButton = FindWidget<UButton>(Widget, TEXT("RouteMerchantLeaveButton"));
	TestTrue(TEXT("rebuilt leave button remains enabled"), LeaveButton && LeaveButton->GetIsEnabled());
	if (LeaveButton)
	{
		LeaveButton->OnClicked.Broadcast();
	}
	TestEqual(TEXT("one rebuilt leave click settles exactly once and returns to route map"),
		Subsystem->GetRuntimeState().Screen, EGameXXKScreen::DungeonMap);
	TestTrue(TEXT("one rebuilt leave click visits the merchant once"),
		Subsystem->GetRuntimeState().VisitedRouteNodeIds.Contains(10));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteMerchantWidgetDisabledReasonsTest,
	"GameXXK.MVP.RouteMerchant.Widget.UnavailableExhaustedAndInsufficientReasons",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteMerchantWidgetDisabledReasonsTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKRouteMerchantWidgetTest;
	UGameInstance* ExhaustedGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* ExhaustedSubsystem = NewObject<UGameXXKMVPSubsystem>(ExhaustedGameInstance);
	ExhaustedSubsystem->GetMutableRuntimeState() = MakeMerchantFixture(1);
	TestTrue(TEXT("single-card merchant opens"), ExhaustedSubsystem->SelectRouteNodeById(10));
	UGameXXKRouteMerchantWidget* ExhaustedWidget = MakeWidget(ExhaustedSubsystem);
	FGameXXKRouteMerchantView ExhaustedView;
	FString Error;
	TestTrue(TEXT("single-card merchant view is valid"), ExhaustedSubsystem->GetRouteMerchantView(ExhaustedView, &Error));
	const TArray<FName> SinglePurchasable = CollectPurchasableOfferIds(ExhaustedView);
	TestEqual(TEXT("single carried card produces one purchasable offer"), SinglePurchasable.Num(), 1);
	for (int32 Index = 1; Index < 4; ++Index)
	{
		TestEqual(*FString::Printf(TEXT("unavailable slot %d is explicitly labelled"), Index),
			ReadText(ExhaustedWidget, *FString::Printf(TEXT("RouteMerchantOfferName%d"), Index)),
			FString(TEXT("没有可强化卡牌")));
		UImage* Art = FindWidget<UImage>(ExhaustedWidget, *FString::Printf(TEXT("RouteMerchantOfferArt%d"), Index));
		TestTrue(*FString::Printf(TEXT("unavailable slot %d does not leave a blank art placeholder"), Index),
			Art && Art->GetVisibility() == ESlateVisibility::Collapsed);
	}
	if (SinglePurchasable.Num() == 1)
	{
		const int32 PurchasableIndex = FindCardOfferIndex(ExhaustedView, SinglePurchasable[0]);
		UButton* BuyButton = FindWidget<UButton>(ExhaustedWidget, *FString::Printf(TEXT("RouteMerchantOfferBuy%d"), PurchasableIndex));
		if (BuyButton)
		{
			BuyButton->OnClicked.Broadcast();
		}
	}
	UButton* ExhaustedRefresh = FindWidget<UButton>(ExhaustedWidget, TEXT("RouteMerchantRefreshButton"));
	TestTrue(TEXT("refresh remains enabled while the relic row still has unsold targets"),
		ExhaustedRefresh && ExhaustedRefresh->GetIsEnabled());
	for (FGameXXKRouteMerchantOffer& Offer : ExhaustedSubsystem->GetMutableRuntimeState().CardRun.RouteMerchant.Offers)
	{
		Offer.bSold = !Offer.bUnavailable;
	}
	ExhaustedWidget->RefreshFromState();
	TestTrue(TEXT("refresh disables only after both rows have no unsold targets"),
		ExhaustedRefresh && !ExhaustedRefresh->GetIsEnabled());
	TestFalse(TEXT("fully exhausted refresh exposes a reason"),
		ExhaustedRefresh ? ExhaustedRefresh->GetToolTipText().IsEmpty() : true);

	UGameInstance* PoorGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* PoorSubsystem = NewObject<UGameXXKMVPSubsystem>(PoorGameInstance);
	PoorSubsystem->GetMutableRuntimeState() = MakeMerchantFixture();
	PoorSubsystem->GetMutableRuntimeState().PlayerGold = 0;
	TestTrue(TEXT("poor merchant fixture opens"), PoorSubsystem->SelectRouteNodeById(10));
	UGameXXKRouteMerchantWidget* PoorWidget = MakeWidget(PoorSubsystem);
	UButton* PoorRefresh = FindWidget<UButton>(PoorWidget, TEXT("RouteMerchantRefreshButton"));
	TestTrue(TEXT("refresh disables when ordinary gold is insufficient"), PoorRefresh && !PoorRefresh->GetIsEnabled());
	TestTrue(TEXT("refresh names the ordinary-gold shortage"), PoorRefresh && PoorRefresh->GetToolTipText().ToString().Contains(TEXT("金币不足")));
	FGameXXKRouteMerchantView PoorView;
	TestTrue(TEXT("poor merchant view remains valid"), PoorSubsystem->GetRouteMerchantView(PoorView, &Error));
	TestFalse(TEXT("poor merchant offer is disabled"), PoorWidget->IsOfferPurchaseEnabledForTest(PoorView.CardOffers[0].SavedOffer.OfferId));
	TestTrue(TEXT("poor merchant offer exposes an ordinary-gold reason"),
		PoorWidget->GetOfferDisabledReasonForTest(PoorView.CardOffers[0].SavedOffer.OfferId).Contains(TEXT("金币不足")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteMerchantWidgetLocalizedFailuresTest,
	"GameXXK.MVP.RouteMerchant.Widget.StalePurchaseAndRefreshErrorsStayChinese",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteMerchantWidgetLocalizedFailuresTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKRouteMerchantWidgetTest;
	auto HasRawRuleEnglish = [](const FString& Text)
	{
		return Text.Contains(TEXT("no longer"), ESearchCase::IgnoreCase)
			|| Text.Contains(TEXT("merchant refresh rejected"), ESearchCase::IgnoreCase)
			|| Text.Contains(TEXT("ordinary gold"), ESearchCase::IgnoreCase)
			|| Text.Contains(TEXT("already sold"), ESearchCase::IgnoreCase)
			|| Text.Contains(TEXT("upgradable carried card"), ESearchCase::IgnoreCase);
	};

	UGameInstance* CarryGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* CarrySubsystem = NewObject<UGameXXKMVPSubsystem>(CarryGameInstance);
	CarrySubsystem->GetMutableRuntimeState() = MakeMerchantFixture();
	TestTrue(TEXT("stale-carry fixture opens"), CarrySubsystem->SelectRouteNodeById(10));
	UGameXXKRouteMerchantWidget* CarryWidget = MakeWidget(CarrySubsystem);
	FGameXXKRouteMerchantView CarryView;
	FString Error;
	TestTrue(TEXT("stale-carry fixture exposes view"), CarrySubsystem->GetRouteMerchantView(CarryView, &Error));
	const FGameXXKRouteMerchantOffer CarryOffer = CarryView.CardOffers[0].SavedOffer;
	CarrySubsystem->GetMutableRuntimeState().CardRun.HeroSelectedCardIds.Remove(CarryOffer.ContentId);
	TestFalse(TEXT("stale carried-card purchase is rejected"), CarryWidget->PurchaseOffer(CarryOffer.OfferId));
	TestEqual(TEXT("stale carried-card purchase keeps its typed failure"),
		CarryWidget->GetLastPurchaseResultForTest().Failure,
		EGameXXKRouteMerchantPurchaseFailure::CardNoLongerCarried);
	const FString CarryError = CarryWidget->GetDisplayedLastActionErrorForTest().ToString();
	TestTrue(TEXT("stale carried-card purchase displays a Chinese explanation"), CarryError.Contains(TEXT("不再携带")));
	TestFalse(TEXT("stale carried-card purchase never leaks the raw rules sentence"), HasRawRuleEnglish(CarryError));
	const FString CarryDisabled = CarryWidget->GetOfferDisabledReasonForTest(CarryOffer.OfferId);
	TestTrue(TEXT("stale carried-card offer displays a Chinese disabled reason"), CarryDisabled.Contains(TEXT("不再携带")));
	TestFalse(TEXT("stale carried-card disabled reason never leaks raw English"), HasRawRuleEnglish(CarryDisabled));

	UGameInstance* RefreshGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* RefreshSubsystem = NewObject<UGameXXKMVPSubsystem>(RefreshGameInstance);
	RefreshSubsystem->GetMutableRuntimeState() = MakeMerchantFixture();
	TestTrue(TEXT("stale-refresh fixture opens"), RefreshSubsystem->SelectRouteNodeById(10));
	UGameXXKRouteMerchantWidget* RefreshWidget = MakeWidget(RefreshSubsystem);
	FGameXXKRouteMerchantView RefreshView;
	TestTrue(TEXT("stale-refresh fixture exposes view"), RefreshSubsystem->GetRouteMerchantView(RefreshView, &Error));
	const FGameXXKRouteMerchantOffer RefreshOffer = RefreshView.CardOffers[0].SavedOffer;
	RefreshSubsystem->GetMutableRuntimeState().CardRun.HeroSelectedCardIds.Remove(RefreshOffer.ContentId);
	TestFalse(TEXT("stale unsold refresh is rejected"), RefreshWidget->RefreshStock());
	const FString RefreshError = RefreshWidget->GetDisplayedLastActionErrorForTest().ToString();
	TestTrue(TEXT("stale refresh displays a Chinese explanation"), RefreshError.Contains(TEXT("不再携带")));
	TestFalse(TEXT("stale refresh never leaks the compound raw rules sentence"), HasRawRuleEnglish(RefreshError));
	UButton* RefreshButton = FindWidget<UButton>(RefreshWidget, TEXT("RouteMerchantRefreshButton"));
	const FString RefreshTooltip = RefreshButton ? RefreshButton->GetToolTipText().ToString() : FString();
	TestFalse(TEXT("stale refresh disables its button"), RefreshButton && RefreshButton->GetIsEnabled());
	TestTrue(TEXT("stale refresh button tooltip is Chinese"), RefreshTooltip.Contains(TEXT("不再携带")));
	TestFalse(TEXT("stale refresh tooltip never leaks raw English"), HasRawRuleEnglish(RefreshTooltip));

	UGameInstance* SoldGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* SoldSubsystem = NewObject<UGameXXKMVPSubsystem>(SoldGameInstance);
	SoldSubsystem->GetMutableRuntimeState() = MakeMerchantFixture();
	TestTrue(TEXT("sold fixture opens"), SoldSubsystem->SelectRouteNodeById(10));
	UGameXXKRouteMerchantWidget* SoldWidget = MakeWidget(SoldSubsystem);
	FGameXXKRouteMerchantView SoldView;
	TestTrue(TEXT("sold fixture exposes view"), SoldSubsystem->GetRouteMerchantView(SoldView, &Error));
	const FName SoldOfferId = SoldView.CardOffers[0].SavedOffer.OfferId;
	TestTrue(TEXT("sold fixture purchases once"), SoldWidget->PurchaseOffer(SoldOfferId));
	TestFalse(TEXT("sold offer rejects a second widget purchase"), SoldWidget->PurchaseOffer(SoldOfferId));
	TestEqual(TEXT("sold widget failure stays typed"),
		SoldWidget->GetLastPurchaseResultForTest().Failure,
		EGameXXKRouteMerchantPurchaseFailure::OfferAlreadySold);
	const FString SoldError = SoldWidget->GetDisplayedLastActionErrorForTest().ToString();
	TestTrue(TEXT("sold offer error is Chinese"), SoldError.Contains(TEXT("已售")));
	TestFalse(TEXT("sold offer error never leaks raw English"), HasRawRuleEnglish(SoldError));

	UGameInstance* PoorGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* PoorSubsystem = NewObject<UGameXXKMVPSubsystem>(PoorGameInstance);
	PoorSubsystem->GetMutableRuntimeState() = MakeMerchantFixture();
	PoorSubsystem->GetMutableRuntimeState().PlayerGold = 0;
	TestTrue(TEXT("poor purchase fixture opens"), PoorSubsystem->SelectRouteNodeById(10));
	UGameXXKRouteMerchantWidget* PoorWidget = MakeWidget(PoorSubsystem);
	FGameXXKRouteMerchantView PoorView;
	TestTrue(TEXT("poor purchase fixture exposes view"), PoorSubsystem->GetRouteMerchantView(PoorView, &Error));
	TestFalse(TEXT("poor widget purchase is rejected"), PoorWidget->PurchaseOffer(PoorView.CardOffers[0].SavedOffer.OfferId));
	TestEqual(TEXT("poor widget failure stays typed"),
		PoorWidget->GetLastPurchaseResultForTest().Failure,
		EGameXXKRouteMerchantPurchaseFailure::InsufficientOrdinaryGold);
	const FString PoorError = PoorWidget->GetDisplayedLastActionErrorForTest().ToString();
	TestTrue(TEXT("poor purchase error is Chinese"), PoorError.Contains(TEXT("金币不足")));
	TestFalse(TEXT("poor purchase error never leaks raw English"), HasRawRuleEnglish(PoorError));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteMerchantWidgetLegacyNormalizationTest,
	"GameXXK.MVP.RouteMerchant.Widget.LegacySnapshotNormalizesOnFirstRefresh",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteMerchantWidgetLegacyNormalizationTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKRouteMerchantWidgetTest;
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(GameInstance);
	Subsystem->GetMutableRuntimeState() = MakeMerchantFixture();
	TestTrue(TEXT("legacy fixture first opens current merchant"), Subsystem->SelectRouteNodeById(10));
	Subsystem->GetMutableRuntimeState().CardRun.RouteMerchant.Offers.SetNum(4);
	for (FGameXXKRouteMerchantOffer& Offer : Subsystem->GetMutableRuntimeState().CardRun.RouteMerchant.Offers)
	{
		Offer.Kind = EGameXXKRouteMerchantOfferKind::Relic;
	}
	FGameXXKPendingRouteMerchantPurchase& LegacyPending =
		Subsystem->GetMutableRuntimeState().CardRun.RouteMerchant.PendingPurchase;
	LegacyPending.bActive = true;
	LegacyPending.OfferId = Subsystem->GetRuntimeState().CardRun.RouteMerchant.Offers[0].OfferId;
	LegacyPending.CardId = Subsystem->GetRuntimeState().CardRun.RouteMerchant.Offers[0].ContentId;
	LegacyPending.Price = Subsystem->GetRuntimeState().CardRun.RouteMerchant.Offers[0].Price;

	UGameXXKRouteMerchantWidget* Widget = MakeWidget(Subsystem);
	TestEqual(TEXT("first widget refresh presents four normalized cards"), Widget->GetRenderedCardOfferCountForTest(), 4);
	TestEqual(TEXT("first widget refresh presents four normalized relics"), Widget->GetRenderedRelicOfferCountForTest(), 4);
	UHorizontalBox* CardRow = FindWidget<UHorizontalBox>(Widget, TEXT("RouteMerchantCardRow"));
	TestEqual(TEXT("normalized legacy view renders all four card cells"), CardRow ? CardRow->GetChildrenCount() : 0, 4);
	for (int32 Index = 0; Index < Subsystem->GetRuntimeState().CardRun.RouteMerchant.Offers.Num(); ++Index)
	{
		const EGameXXKRouteMerchantOfferKind ExpectedKind = Index < 4
			? EGameXXKRouteMerchantOfferKind::Card
			: EGameXXKRouteMerchantOfferKind::Relic;
		TestEqual(TEXT("mutable first view persists normalized row kind"),
			Subsystem->GetRuntimeState().CardRun.RouteMerchant.Offers[Index].Kind,
			ExpectedKind);
	}
	TestFalse(TEXT("first widget refresh discards the legacy pending transaction"),
		Subsystem->GetRuntimeState().CardRun.RouteMerchant.PendingPurchase.bActive);
	UButton* RefreshButton = FindWidget<UButton>(Widget, TEXT("RouteMerchantRefreshButton"));
	TestTrue(TEXT("normalized legacy widget exposes an enabled refresh action"),
		RefreshButton && RefreshButton->GetIsEnabled());
	if (RefreshButton)
	{
		RefreshButton->OnClicked.Broadcast();
	}
	TestEqual(TEXT("normalized legacy widget refresh advances once"),
		Subsystem->GetRuntimeState().CardRun.RouteMerchant.RefreshCount, 1);
	UButton* LeaveButton = FindWidget<UButton>(Widget, TEXT("RouteMerchantLeaveButton"));
	TestTrue(TEXT("normalized legacy widget exposes the only leave action"),
		LeaveButton && LeaveButton->GetIsEnabled());
	if (LeaveButton)
	{
		LeaveButton->OnClicked.Broadcast();
	}
	TestEqual(TEXT("normalized legacy widget leave returns to route map"),
		Subsystem->GetRuntimeState().Screen, EGameXXKScreen::DungeonMap);
	return true;
}

#endif
