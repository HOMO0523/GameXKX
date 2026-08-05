#include "Misc/AutomationTest.h"

#include "GameXXKCompanionCatalog.h"
#include "GameXXKCompanionRules.h"
#include "GameXXKEquipmentCatalog.h"
#include "GameXXKEquipmentRules.h"
#include "GameXXKMetaShopRules.h"
#include "GameXXKMVPRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	int32 BuildCompanionPackOrderSeed(const FGameXXKRuntimeState& State)
	{
		const uint32 Mixed = HashCombine(
			HashCombine(GetTypeHash(State.MetaShop.Seed), GetTypeHash(State.MetaShop.NextPurchaseOrdinal)),
			GetTypeHash(static_cast<uint8>(EGameXXKMetaShopProductId::CompanionPack)));
		return FMath::Max(1, static_cast<int32>(Mixed & 0x7fffffffU));
	}

	bool FillRosterWithoutTemplate(
		FAutomationTestBase& Test,
		FGameXXKCompanionRosterState& Roster,
		const FName ExcludedTemplateId)
	{
		for (const FGameXXKCompanionTemplateDefinition& Template : FGameXXKCompanionCatalog::GetRecruitTemplates())
		{
			if (Template.TemplateId == ExcludedTemplateId
				|| Roster.PermanentCompanions.Num() >= FGameXXKCompanionRules::MaxPermanentCompanions)
			{
				continue;
			}
			FGameXXKCompanionRecruitResult Result;
			if (!FGameXXKCompanionRules::RecruitPermanentCompanion(
				Roster,
				Template.TemplateId,
				100000 + Roster.PermanentCompanions.Num(),
				Result))
			{
				Test.AddError(TEXT("failed to fill a valid companion roster fixture"));
				return false;
			}
		}
		return Test.TestEqual(
			TEXT("full-roster fixture reaches twelve companions"),
			Roster.PermanentCompanions.Num(),
			FGameXXKCompanionRules::MaxPermanentCompanions);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKMetaShopCatalogTest,
	"GameXXK.MetaShop.Catalog",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKMetaShopCatalogTest::RunTest(const FString& Parameters)
{
	const TArray<FGameXXKMetaShopProductDefinition>& Products = FGameXXKMetaShopRules::GetProducts();
	TestEqual(TEXT("seven fixed products"), Products.Num(), 7);
	if (Products.Num() != 7)
	{
		return false;
	}

	const EGameXXKMetaShopProductId ExpectedIds[] = {
		EGameXXKMetaShopProductId::PoJunPack,
		EGameXXKMetaShopProductId::XuanJiaPack,
		EGameXXKMetaShopProductId::QingNangPack,
		EGameXXKMetaShopProductId::ZhuiFengPack,
		EGameXXKMetaShopProductId::ShiGuPack,
		EGameXXKMetaShopProductId::ShanHePack,
		EGameXXKMetaShopProductId::CompanionPack,
	};
	const EGameXXKEquipmentSet ExpectedSets[] = {
		EGameXXKEquipmentSet::PoJun,
		EGameXXKEquipmentSet::XuanJia,
		EGameXXKEquipmentSet::QingNang,
		EGameXXKEquipmentSet::ZhuiFeng,
		EGameXXKEquipmentSet::ShiGu,
		EGameXXKEquipmentSet::ShanHe,
	};

	TSet<EGameXXKMetaShopProductId> UniqueIds;
	for (int32 Index = 0; Index < 6; ++Index)
	{
		const FGameXXKMetaShopProductDefinition& Product = Products[Index];
		TestEqual(FString::Printf(TEXT("equipment product %d keeps stable id"), Index), Product.ProductId, ExpectedIds[Index]);
		TestEqual(FString::Printf(TEXT("equipment product %d keeps set order"), Index), Product.EquipmentSet, ExpectedSets[Index]);
		TestEqual(FString::Printf(TEXT("equipment product %d is an equipment pack"), Index), Product.Kind, EGameXXKMetaShopProductKind::EquipmentPack);
		TestEqual(FString::Printf(TEXT("equipment product %d costs 100"), Index), Product.Price, FGameXXKMetaShopRules::EquipmentPackPrice);
		TestFalse(FString::Printf(TEXT("equipment product %d has a display name"), Index), Product.DisplayName.IsEmpty());
		TestFalse(FString::Printf(TEXT("equipment product %d has an icon path"), Index), Product.IconSoftPath.IsNull());
		UniqueIds.Add(Product.ProductId);
		TestTrue(FString::Printf(TEXT("equipment product %d can be found by id"), Index), FGameXXKMetaShopRules::FindProduct(Product.ProductId) == &Product);
	}

	const FGameXXKMetaShopProductDefinition& Companion = Products[6];
	TestEqual(TEXT("companion product keeps stable id"), Companion.ProductId, EGameXXKMetaShopProductId::CompanionPack);
	TestEqual(TEXT("companion is last"), Companion.Kind, EGameXXKMetaShopProductKind::CompanionPack);
	TestEqual(TEXT("companion has no equipment set"), Companion.EquipmentSet, EGameXXKEquipmentSet::Invalid);
	TestEqual(TEXT("companion costs 500"), Companion.Price, FGameXXKMetaShopRules::CompanionPackPrice);
	TestFalse(TEXT("companion has a display name"), Companion.DisplayName.IsEmpty());
	TestFalse(TEXT("companion has an icon path"), Companion.IconSoftPath.IsNull());
	UniqueIds.Add(Companion.ProductId);
	TestEqual(TEXT("all product ids are unique"), UniqueIds.Num(), 7);
	TestTrue(TEXT("companion can be found by id"), FGameXXKMetaShopRules::FindProduct(EGameXXKMetaShopProductId::CompanionPack) == &Companion);
	TestTrue(TEXT("invalid product id is not found"), FGameXXKMetaShopRules::FindProduct(EGameXXKMetaShopProductId::Invalid) == nullptr);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKMetaShopEquipmentPurchaseTest,
	"GameXXK.MetaShop.EquipmentPurchase",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKMetaShopEquipmentPurchaseTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("roll 1 is common"), FGameXXKMetaShopRules::QualityFromRoll(1), EGameXXKEquipmentQuality::Common);
	TestEqual(TEXT("roll 70 is common"), FGameXXKMetaShopRules::QualityFromRoll(70), EGameXXKEquipmentQuality::Common);
	TestEqual(TEXT("roll 71 is rare"), FGameXXKMetaShopRules::QualityFromRoll(71), EGameXXKEquipmentQuality::Rare);
	TestEqual(TEXT("roll 95 is rare"), FGameXXKMetaShopRules::QualityFromRoll(95), EGameXXKEquipmentQuality::Rare);
	TestEqual(TEXT("roll 96 is epic"), FGameXXKMetaShopRules::QualityFromRoll(96), EGameXXKEquipmentQuality::Epic);
	TestEqual(TEXT("roll 100 is epic"), FGameXXKMetaShopRules::QualityFromRoll(100), EGameXXKEquipmentQuality::Epic);
	TestEqual(TEXT("roll zero is invalid"), FGameXXKMetaShopRules::QualityFromRoll(0), EGameXXKEquipmentQuality::Invalid);
	TestEqual(TEXT("roll above 100 is invalid"), FGameXXKMetaShopRules::QualityFromRoll(101), EGameXXKEquipmentQuality::Invalid);

	const EGameXXKMetaShopProductId ProductIds[] = {
		EGameXXKMetaShopProductId::PoJunPack,
		EGameXXKMetaShopProductId::XuanJiaPack,
		EGameXXKMetaShopProductId::QingNangPack,
		EGameXXKMetaShopProductId::ZhuiFengPack,
		EGameXXKMetaShopProductId::ShiGuPack,
		EGameXXKMetaShopProductId::ShanHePack,
	};
	const EGameXXKEquipmentSet ExpectedSets[] = {
		EGameXXKEquipmentSet::PoJun,
		EGameXXKEquipmentSet::XuanJia,
		EGameXXKEquipmentSet::QingNang,
		EGameXXKEquipmentSet::ZhuiFeng,
		EGameXXKEquipmentSet::ShiGu,
		EGameXXKEquipmentSet::ShanHe,
	};

	for (int32 Index = 0; Index < 6; ++Index)
	{
		FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
		State.Screen = EGameXXKScreen::Town;
		State.PlayerGold = 1000;
		State.PlayerLevel = Index + 1;
		UGameXXKMVPRules::RecalculatePlayerStatsFromEquipment(State);
		const FGameXXKRuntimeState BeforePreview = State;
		FGameXXKMetaShopPurchasePreview Preview;
		TestTrue(FString::Printf(TEXT("equipment product %d previews"), Index), FGameXXKMetaShopRules::PreviewPurchase(State, ProductIds[Index], Preview));
		TestTrue(FString::Printf(TEXT("equipment product %d preview is enabled"), Index), Preview.bAvailable);
		TestEqual(FString::Printf(TEXT("equipment product %d preview price"), Index), Preview.Price, 100);
		TestTrue(
			FString::Printf(TEXT("equipment product %d preview is pure"), Index),
			FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(&State, &BeforePreview, PPF_None));

		const int32 WarehouseBefore = State.EquipmentCollection.WarehouseInstanceIds.Num();
		const int32 OrdinalBefore = State.MetaShop.NextPurchaseOrdinal;
		FGameXXKMetaShopPurchaseResult Result;
		TestTrue(FString::Printf(TEXT("equipment product %d purchases"), Index), FGameXXKMetaShopRules::Purchase(State, ProductIds[Index], Result));
		TestTrue(FString::Printf(TEXT("equipment product %d reports success"), Index), Result.bPurchased);
		TestEqual(FString::Printf(TEXT("equipment product %d spends exactly 100"), Index), State.PlayerGold, 900);
		TestEqual(FString::Printf(TEXT("equipment product %d reports gold delta"), Index), Result.GoldDelta, -100);
		TestEqual(FString::Printf(TEXT("equipment product %d adds one warehouse item"), Index), State.EquipmentCollection.WarehouseInstanceIds.Num(), WarehouseBefore + 1);
		TestEqual(FString::Printf(TEXT("equipment product %d advances the shop ordinal"), Index), State.MetaShop.NextPurchaseOrdinal, OrdinalBefore + 1);
		const FGameXXKEquipmentInstance* Instance = FGameXXKEquipmentRules::FindInstance(State.EquipmentCollection, Result.GeneratedEquipmentId);
		TestNotNull(FString::Printf(TEXT("equipment product %d resolves its generated instance"), Index), Instance);
		if (Instance)
		{
			const FGameXXKEquipmentDefinition* Definition = FGameXXKEquipmentCatalog::FindDefinition(Instance->BaseEquipmentId);
			TestNotNull(FString::Printf(TEXT("equipment product %d resolves its definition"), Index), Definition);
			if (Definition)
			{
				TestEqual(FString::Printf(TEXT("equipment product %d forces its set"), Index), Definition->Set, ExpectedSets[Index]);
				TestTrue(FString::Printf(TEXT("equipment product %d rolls a legal slot"), Index), Definition->Slot >= EGameXXKEquipmentSlot::Weapon && Definition->Slot <= EGameXXKEquipmentSlot::Accessory);
			}
			TestEqual(FString::Printf(TEXT("equipment product %d matches player level"), Index), Instance->ItemLevel, Index + 1);
			TestTrue(FString::Printf(TEXT("equipment product %d rolls a legal quality"), Index), Instance->Quality >= EGameXXKEquipmentQuality::Common && Instance->Quality <= EGameXXKEquipmentQuality::Epic);
		}
	}

	FGameXXKRuntimeState ReplayA = UGameXXKMVPRules::CreateNewGame();
	ReplayA.Screen = EGameXXKScreen::Town;
	ReplayA.PlayerGold = 1000;
	FGameXXKRuntimeState ReplayB = ReplayA;
	FGameXXKMetaShopPurchaseResult ReplayResultA;
	FGameXXKMetaShopPurchaseResult ReplayResultB;
	TestTrue(TEXT("first deterministic replay purchases"), FGameXXKMetaShopRules::Purchase(ReplayA, EGameXXKMetaShopProductId::PoJunPack, ReplayResultA));
	TestTrue(TEXT("second deterministic replay purchases"), FGameXXKMetaShopRules::Purchase(ReplayB, EGameXXKMetaShopProductId::PoJunPack, ReplayResultB));
	TestTrue(TEXT("same pre-purchase state produces byte-identical runtime"), FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(&ReplayA, &ReplayB, PPF_None));
	TestEqual(TEXT("same pre-purchase state produces the same instance id"), ReplayResultA.GeneratedEquipmentId, ReplayResultB.GeneratedEquipmentId);

	auto TestAtomicFailure = [this](FGameXXKRuntimeState State, const EGameXXKMetaShopProductId ProductId, const EGameXXKMetaShopError ExpectedError, const TCHAR* Label)
	{
		const FGameXXKRuntimeState Before = State;
		FGameXXKMetaShopPurchaseResult Result;
		TestFalse(Label, FGameXXKMetaShopRules::Purchase(State, ProductId, Result));
		TestEqual(FString::Printf(TEXT("%s returns typed error"), Label), Result.Error, ExpectedError);
		TestTrue(FString::Printf(TEXT("%s leaves runtime unchanged"), Label), FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(&State, &Before, PPF_None));
	};

	FGameXXKRuntimeState Insufficient = UGameXXKMVPRules::CreateNewGame();
	Insufficient.Screen = EGameXXKScreen::Town;
	Insufficient.PlayerGold = 99;
	TestAtomicFailure(Insufficient, EGameXXKMetaShopProductId::PoJunPack, EGameXXKMetaShopError::InsufficientGold, TEXT("insufficient gold"));

	FGameXXKRuntimeState Full = UGameXXKMVPRules::CreateNewGame();
	Full.Screen = EGameXXKScreen::Town;
	Full.PlayerGold = 1000;
	while (FGameXXKEquipmentRules::HasWarehouseCapacity(Full.EquipmentCollection))
	{
		FGameXXKEquipmentCreateRequest Request;
		Request.Set = EGameXXKEquipmentSet::PoJun;
		Request.Quality = EGameXXKEquipmentQuality::Common;
		Request.ItemLevel = 1;
		Request.bForceSlot = true;
		Request.ForcedSlot = EGameXXKEquipmentSlot::Weapon;
		FName InstanceId;
		if (!FGameXXKEquipmentRules::CreateRolledInstance(Full.EquipmentCollection, Request, InstanceId))
		{
			AddError(TEXT("warehouse fixture failed before reaching capacity"));
			return false;
		}
	}
	TestEqual(TEXT("warehouse fixture reaches 200"), Full.EquipmentCollection.WarehouseInstanceIds.Num(), FGameXXKEquipmentRules::WarehouseCapacity);
	TestAtomicFailure(Full, EGameXXKMetaShopProductId::PoJunPack, EGameXXKMetaShopError::WarehouseFull, TEXT("full warehouse"));

	FGameXXKRuntimeState Exhausted = UGameXXKMVPRules::CreateNewGame();
	Exhausted.Screen = EGameXXKScreen::Town;
	Exhausted.PlayerGold = 1000;
	Exhausted.MetaShop.NextPurchaseOrdinal = MAX_int32;
	TestAtomicFailure(Exhausted, EGameXXKMetaShopProductId::PoJunPack, EGameXXKMetaShopError::PurchaseOrdinalExhausted, TEXT("exhausted purchase ordinal"));

	FGameXXKRuntimeState Corrupt = UGameXXKMVPRules::CreateNewGame();
	Corrupt.Screen = EGameXXKScreen::Town;
	Corrupt.PlayerGold = 1000;
	Corrupt.EquipmentCollection.CollectionSeed = 0;
	TestAtomicFailure(Corrupt, EGameXXKMetaShopProductId::PoJunPack, EGameXXKMetaShopError::InvalidRuntimeState, TEXT("corrupt runtime"));

	FGameXXKRuntimeState InvalidProduct = UGameXXKMVPRules::CreateNewGame();
	InvalidProduct.Screen = EGameXXKScreen::Town;
	InvalidProduct.PlayerGold = 1000;
	TestAtomicFailure(InvalidProduct, EGameXXKMetaShopProductId::Invalid, EGameXXKMetaShopError::InvalidProduct, TEXT("invalid product"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKMetaShopCompanionPurchaseTest,
	"GameXXK.MetaShop.CompanionPurchase",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKMetaShopCompanionPurchaseTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
	State.Screen = EGameXXKScreen::Town;
	State.PlayerGold = FGameXXKMetaShopRules::CompanionPackPrice;

	FGameXXKCompanionRosterState OrderProbe = State.CardRun.CompanionRoster;
	FGameXXKCompanionRecruitOrder ExpectedOrder;
	TestTrue(TEXT("the explicit companion-pack order seed resolves"), FGameXXKCompanionRules::CreateRecruitOrder(
		OrderProbe,
		BuildCompanionPackOrderSeed(State),
		ExpectedOrder));

	FGameXXKMetaShopPurchasePreview Preview;
	TestTrue(TEXT("companion pack previews in town"), FGameXXKMetaShopRules::PreviewPurchase(
		State,
		EGameXXKMetaShopProductId::CompanionPack,
		Preview));
	TestEqual(TEXT("companion preview costs 500"), Preview.Price, FGameXXKMetaShopRules::CompanionPackPrice);

	const FGameXXKRuntimeState ReplaySource = State;
	FGameXXKMetaShopPurchaseResult Result;
	TestTrue(TEXT("companion pack succeeds below roster capacity"), FGameXXKMetaShopRules::Purchase(
		State,
		EGameXXKMetaShopProductId::CompanionPack,
		Result));
	TestTrue(TEXT("companion result reports purchase"), Result.bPurchased);
	TestEqual(TEXT("companion pack spends exactly 500"), State.PlayerGold, 0);
	TestEqual(TEXT("companion result reports gold delta"), Result.GoldDelta, -FGameXXKMetaShopRules::CompanionPackPrice);
	TestEqual(TEXT("companion pack advances meta-shop ordinal"), State.MetaShop.NextPurchaseOrdinal, 1);
	TestEqual(TEXT("direct companion pack recruits one permanent companion"), State.CardRun.CompanionRoster.PermanentCompanions.Num(), 1);
	TestEqual(TEXT("direct companion pack reports recruited"), Result.CompanionResult.Outcome, EGameXXKCompanionRecruitOutcome::Recruited);
	TestEqual(TEXT("explicit saved order chooses the purchased template"), Result.CompanionResult.Companion.RecruitTemplateId, ExpectedOrder.ResolvedTemplateId);
	TestFalse(TEXT("direct recruit consumes its pending order"), State.CardRun.CompanionRoster.PendingRecruitOrder.bHasPendingOrder);

	FGameXXKRuntimeState Replay = ReplaySource;
	FGameXXKMetaShopPurchaseResult ReplayResult;
	TestTrue(TEXT("identical companion purchase replays"), FGameXXKMetaShopRules::Purchase(
		Replay,
		EGameXXKMetaShopProductId::CompanionPack,
		ReplayResult));
	TestTrue(TEXT("identical companion purchase produces byte-identical runtime"),
		FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(&Replay, &State, PPF_None));
	TestEqual(TEXT("identical companion purchase produces the same candidate"),
		ReplayResult.CompanionResult.Companion.InstanceId,
		Result.CompanionResult.Companion.InstanceId);

	FGameXXKRuntimeState Full = UGameXXKMVPRules::CreateNewGame();
	Full.Screen = EGameXXKScreen::Town;
	Full.PlayerGold = FGameXXKMetaShopRules::CompanionPackPrice;
	FGameXXKCompanionRosterState FullOrderProbe = Full.CardRun.CompanionRoster;
	FGameXXKCompanionRecruitOrder FullExpectedOrder;
	TestTrue(TEXT("full-roster explicit order resolves"), FGameXXKCompanionRules::CreateRecruitOrder(
		FullOrderProbe,
		BuildCompanionPackOrderSeed(Full),
		FullExpectedOrder));
	if (!FillRosterWithoutTemplate(*this, Full.CardRun.CompanionRoster, FullExpectedOrder.ResolvedTemplateId))
	{
		return false;
	}

	FGameXXKMetaShopPurchaseResult FullResult;
	TestTrue(TEXT("full roster buys one fixed replacement candidate"), FGameXXKMetaShopRules::Purchase(
		Full,
		EGameXXKMetaShopProductId::CompanionPack,
		FullResult));
	TestEqual(TEXT("full roster purchase reports pending replacement"), FullResult.CompanionResult.Outcome, EGameXXKCompanionRecruitOutcome::PendingReplacement);
	TestEqual(TEXT("full roster persists the explicit candidate"), Full.CardRun.CompanionRoster.PendingRecruitment.Candidate.RecruitTemplateId, FullExpectedOrder.ResolvedTemplateId);
	TestTrue(TEXT("full roster retains the no-reroll order"), Full.CardRun.CompanionRoster.PendingRecruitOrder.bHasPendingOrder);
	TestEqual(TEXT("full roster purchase spends its 500 gold"), Full.PlayerGold, 0);
	TestEqual(TEXT("full roster purchase advances meta-shop ordinal"), Full.MetaShop.NextPurchaseOrdinal, 1);

	FGameXXKRuntimeState PendingBlocked = Full;
	PendingBlocked.PlayerGold = FGameXXKMetaShopRules::CompanionPackPrice;
	const FGameXXKRuntimeState PendingBlockedBefore = PendingBlocked;
	FGameXXKMetaShopPurchaseResult PendingBlockedResult;
	TestFalse(TEXT("a pending companion rejects another pack"), FGameXXKMetaShopRules::Purchase(
		PendingBlocked,
		EGameXXKMetaShopProductId::CompanionPack,
		PendingBlockedResult));
	TestEqual(TEXT("pending companion returns typed error"), PendingBlockedResult.Error, EGameXXKMetaShopError::PendingCompanionExists);
	TestTrue(TEXT("pending companion rejection is atomic"),
		FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(&PendingBlocked, &PendingBlockedBefore, PPF_None));

	TestTrue(TEXT("the paid full-roster candidate can be discarded"), FGameXXKCompanionRules::DiscardPendingRecruitment(
		Full.CardRun.CompanionRoster));
	TestEqual(TEXT("discard never refunds the meta-shop purchase"), Full.PlayerGold, 0);
	TestEqual(TEXT("discard never rewinds the meta-shop ordinal"), Full.MetaShop.NextPurchaseOrdinal, 1);

	FGameXXKRuntimeState Corrupt = UGameXXKMVPRules::CreateNewGame();
	Corrupt.Screen = EGameXXKScreen::Town;
	Corrupt.PlayerGold = FGameXXKMetaShopRules::CompanionPackPrice;
	Corrupt.CardRun.CompanionRoster.SigilCount = -1;
	const FGameXXKRuntimeState CorruptBefore = Corrupt;
	FGameXXKMetaShopPurchaseResult CorruptResult;
	TestFalse(TEXT("invalid roster rejects companion purchase"), FGameXXKMetaShopRules::Purchase(
		Corrupt,
		EGameXXKMetaShopProductId::CompanionPack,
		CorruptResult));
	TestEqual(TEXT("invalid roster reports runtime-state error"), CorruptResult.Error, EGameXXKMetaShopError::InvalidRuntimeState);
	TestTrue(TEXT("invalid roster rejection is atomic"),
		FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(&Corrupt, &CorruptBefore, PPF_None));
	return true;
}

#endif
