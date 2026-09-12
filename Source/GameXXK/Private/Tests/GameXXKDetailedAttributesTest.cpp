#include "Misc/AutomationTest.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/GameInstance.h"
#include "GameXXKEquipmentRules.h"
#include "GameXXKEquipmentBonusRules.h"
#include "GameXXKGemRules.h"
#include "GameXXKTalentCatalog.h"
#include "UI/GameXXKCharacterDetailedAttributes.h"
#include "UI/GameXXKCharacterUiPresentation.h"
#include "UI/GameXXKInRunUiStyle.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKDesktopTrainingWorkbenchWidget.h"
#include "UI/GameXXKInventoryWindowWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	bool EquipGemFixture(FAutomationTestBase& Test, FGameXXKRuntimeState& State, FName Owner)
	{
		const EGameXXKEquipmentSlot Slots[] = {EGameXXKEquipmentSlot::Weapon, EGameXXKEquipmentSlot::Head,
			EGameXXKEquipmentSlot::Armor, EGameXXKEquipmentSlot::Belt, EGameXXKEquipmentSlot::Shoes, EGameXXKEquipmentSlot::Accessory};
		const EGameXXKGemType Types[] = {EGameXXKGemType::Attack, EGameXXKGemType::Defense, EGameXXKGemType::MaxHealth};
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Slots); ++Index)
		{
			FGameXXKEquipmentCreateRequest Request;
			Request.Set = EGameXXKEquipmentSet::XuanJia;
			Request.Quality = EGameXXKEquipmentQuality::Treasure;
			Request.ItemLevel = 1;
			Request.bForceSlot = true;
			Request.ForcedSlot = Slots[Index];
			FName Id;
			FString Error;
			if (!Test.TestTrue(TEXT("gem fixture equipment is created"),
				FGameXXKEquipmentRules::CreateRolledInstance(State.EquipmentCollection, Request, Id, &Error))) return false;
			auto* Item = State.EquipmentCollection.EquipmentInstances.FindByPredicate(
				[Id](const auto& Candidate) { return Candidate.InstanceId == Id; });
			Item->RolledAffixes.Reset();
			for (const TCHAR* Kind : {TEXT("ArmorGain"), TEXT("ArmorRetention"), TEXT("CounterDamage"), TEXT("GuardReduction"), TEXT("LowHealthProtection")})
			{
				FGameXXKEquipmentAffixRoll Roll;
				Roll.AffixId = FName(*FString::Printf(TEXT("Affix.XuanJia.%s"), Kind));
				Roll.Tier = EGameXXKAffixTier::Treasure;
				Roll.Magnitude = 3500;
				Roll.Unit = EGameXXKEquipmentMagnitudeUnit::BasisPoints;
				Item->RolledAffixes.Add(Roll);
			}
			for (auto& Gem : Item->SocketedGems)
			{
				Gem.Type = Types[Index / 2];
				Gem.Quality = EGameXXKGemQuality::Treasure;
			}
			const auto Result = FGameXXKEquipmentRules::EquipInstance(
				State.EquipmentCollection, State.CardRun.CompanionRoster, Owner, Slots[Index], Id);
			if (!Test.TestTrue(TEXT("gem fixture equips each slot"), Result.bSucceeded)) return false;
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKGemQualityProjectionTest,
	"GameXXK.Equipment.DetailedAttributes.GemQualityProjection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKGemQualityProjectionTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("invalid type has no contribution"), FGameXXKGemRules::GetStatBonus(EGameXXKGemType::Invalid, EGameXXKGemQuality::Cosmic), 0);
	TestEqual(TEXT("invalid quality has no contribution"), FGameXXKGemRules::GetStatBonus(EGameXXKGemType::Attack, EGameXXKGemQuality::Invalid), 0);
	auto* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestTrue(TEXT("the gem fixture starts"), Subsystem->StartGame())) return false;
	const FName Owner = FGameXXKEquipmentRules::HeroCharacterId();
	if (!EquipGemFixture(*this, Subsystem->GetMutableRuntimeState(), Owner)) return false;
	FGameXXKEquipmentLoadoutSnapshot Snapshot;
	if (!TestTrue(TEXT("the authoritative gem projection is available"), Subsystem->GetEquipmentLoadoutSnapshot(Owner, Snapshot))) return false;
	TestEqual(TEXT("four treasure attack gems add without a quantity penalty"), Snapshot.SocketGemFlatStats.Attack, 128);
	TestEqual(TEXT("four treasure defense gems add without a quantity penalty"), Snapshot.SocketGemFlatStats.Defense, 128);
	TestEqual(TEXT("four treasure health gems add without a quantity penalty"), Snapshot.SocketGemFlatStats.MaxHealth, 640);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKAffixMarginalLimitTest,
	"GameXXK.Equipment.DetailedAttributes.AffixMarginalLimitAndPrecision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKAffixMarginalLimitTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("negative input has no bonus"), FGameXXKEquipmentBonusRules::GetEffectiveAffixBasisPoints(-1), 0.0);
	TestEqual(TEXT("zero input has no bonus"), FGameXXKEquipmentBonusRules::GetEffectiveAffixBasisPoints(0), 0.0);
	double Previous = 0.0, PreviousDelta = 10000.0;
	for (int32 Pieces = 1; Pieces <= 100; ++Pieces)
	{
		const double Current = FGameXXKEquipmentBonusRules::GetEffectiveAffixBasisPoints(3500 * Pieces);
		const double Delta = Current - Previous;
		TestTrue(TEXT("more raw bonus still improves the final value"), Delta > 0);
		TestTrue(TEXT("each equal addition yields less"), Delta < PreviousDelta);
		TestTrue(TEXT("the affix stays below the 75 percent ceiling"), Current < 7500);
		Previous = Current; PreviousDelta = Delta;
	}
	FGameXXKEquipmentArmorBonus Bonus;
	Bonus.RawAffixBasisPoints = 21000;
	Bonus.FixedBasisPoints = 1000;
	TestEqual(TEXT("six perfect Treasure rolls plus the fixed set bonus"), Bonus.GetFinalPercent(), 53.75);
	TestEqual(TEXT("round armor only at final settlement"), Bonus.ApplyTo(100), 154);
	Bonus.RawAffixBasisPoints = 0;
	TestEqual(TEXT("an exact fixed-only result never rounds one point too high"), Bonus.ApplyTo(100), 110);
	Bonus.RawAffixBasisPoints = 1;
	TestEqual(TEXT("half a nominal basis point is retained until final rounding"), Bonus.ApplyTo(100), 111);
	Bonus.RawAffixBasisPoints = MAX_int32;
	Bonus.FixedBasisPoints = MAX_int32;
	TestEqual(TEXT("extreme totals safely saturate armor"), Bonus.ApplyTo(MAX_int32), MAX_int32);
	TestEqual(TEXT("zero armor cannot create armor from a percentage"), Bonus.ApplyTo(0), 0);

	FGameXXKEquipmentArmorBonus Isolated;
	FGameXXKEquipmentActiveEffect Effect;
	Effect.EffectId = TEXT("EquipmentAffixAggregate.2.11");
	Effect.SourceCharacterId = TEXT("Wearer");
	Effect.ModifierKind = EGameXXKEquipmentModifierKind::ArmorGain;
	Effect.Magnitude = 3500;
	Effect.Unit = EGameXXKEquipmentMagnitudeUnit::BasisPoints;
	Isolated.AddEffect(Effect, TEXT("Other"));
	TestEqual(TEXT("a different source cannot borrow this bonus"), Isolated.GetFinalPercent(), 0.0);
	Isolated.AddEffect(Effect, TEXT("Wearer"));
	TestTrue(TEXT("source-matched random values use the marginal curve"), FMath::IsNearlyEqual(Isolated.GetFinalPercent(), 14.189189189189, 0.000001));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKDetailedAttributesEntryTest,
	"GameXXK.DesktopTraining.BackpackAttributes.DetailedEntryAndReturn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDetailedAttributesEntryTest::RunTest(const FString& Parameters)
{
	auto* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestTrue(TEXT("the detailed-attribute fixture starts"), Subsystem->StartGame())) return false;
	auto* Workbench = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	Workbench->SetMVPSubsystem(Subsystem);
	Workbench->ConstructForTest();
	Workbench->OpenBackpack();
	auto* Inventory = Cast<UGameXXKInventoryWindowWidget>(
		Workbench->WidgetTree->FindWidget(TEXT("EmbeddedApprovedBackpack")));
	if (!TestNotNull(TEXT("the actual backpack is embedded"), Inventory)) return false;
	Inventory->OpenCharacterBackpackTabForTest(EGameXXKCharacterBackpackTab::Attributes);
	auto* Open = Cast<UButton>(Inventory->WidgetTree->FindWidget(TEXT("InventoryDetailedAttributesButton")));
	if (!TestNotNull(TEXT("the Attributes page has a detailed-attributes button"), Open)) return false;
	TestTrue(TEXT("the detailed button is enabled"), Open->GetIsEnabled());
	Open->OnClicked.Broadcast();
	auto* Panel = Inventory->WidgetTree->FindWidget(TEXT("InventoryDetailedAttributesPanel"));
	if (!TestNotNull(TEXT("the detailed panel exists"), Panel)) return false;
	TestTrue(TEXT("the real button opens the detailed panel"), Panel->GetVisibility() != ESlateVisibility::Collapsed);
	auto* Back = Cast<UButton>(Inventory->WidgetTree->FindWidget(TEXT("InventoryDetailedAttributesBackButton")));
	if (!TestNotNull(TEXT("detailed attributes offer a return button"), Back)) return false;
	Back->OnClicked.Broadcast();
	TestEqual(TEXT("return collapses detailed attributes"), Panel->GetVisibility(), ESlateVisibility::Collapsed);
	TestTrue(TEXT("return restores the current character's basic attributes"),
		Inventory->GetCharacterTabBodyTextForTest().ToString().Contains(TEXT("攻击")));
	TArray<FName> Ids = {FGameXXKEquipmentRules::HeroCharacterId()};
	Ids.Append(Workbench->GetCompanionCharacterIdsForTest());
	Ids.Append(Workbench->GetNpcCharacterIdsForTest());
	TestEqual(TEXT("detailed attributes cover thirteen owned characters"), Ids.Num(), 13);
	for (const FName Id : Ids)
	{
		if (!TestTrue(TEXT("the current character can be selected"), Workbench->SelectBackpackCharacterForTest(Id))) return false;
		Inventory = Cast<UGameXXKInventoryWindowWidget>(Workbench->WidgetTree->FindWidget(TEXT("EmbeddedApprovedBackpack")));
		Inventory->OpenCharacterBackpackTabForTest(EGameXXKCharacterBackpackTab::Attributes);
		Inventory->SetDetailedAttributesOpen(true);
		const FString Body = Inventory->GetCharacterTabBodyTextForTest().ToString();
		TestTrue(TEXT("details belong to the selected character"), Body.Contains(GameXXKCharacterUiPresentation::GetDisplayName(Subsystem, Id)));
		TestTrue(TEXT("critical damage includes its base multiplier"), Body.Contains(TEXT("暴击伤害倍率 150%")));
		TestFalse(TEXT("details never expose internal NPC IDs"), Body.Contains(TEXT("Npc.")));
		Inventory->SetDetailedAttributesOpen(false);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKDetailedAttributesRefreshTest,
	"GameXXK.DesktopTraining.BackpackAttributes.DetailedFinalValuesAndRefresh",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDetailedAttributesRefreshTest::RunTest(const FString& Parameters)
{
	auto* Sub = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!Sub->StartGame()) return false;
	auto* Inventory = NewObject<UGameXXKInventoryWindowWidget>();
	Inventory->SetMVPSubsystem(Sub);
	Inventory->OpenFreeInventory();
	Inventory->TakeWidget();
	Inventory->OpenCharacterBackpackTabForTest(EGameXXKCharacterBackpackTab::Attributes);
	Inventory->SetDetailedAttributesOpen(true);
	const FName Owner = FGameXXKEquipmentRules::HeroCharacterId();
	if (!EquipGemFixture(*this, Sub->GetMutableRuntimeState(), Owner)) return false;
	Inventory->RefreshVisibleRuntimeValues();
	FString Body = Inventory->GetCharacterTabBodyTextForTest().ToString();
	TestTrue(TEXT("the open page refreshes after equipping"), Body.Contains(TEXT("护甲获得量加成 +53.75%")));
	TestTrue(TEXT("the final retention includes the active fixed set"), Body.Contains(TEXT("回合开始护甲保留 50.00%")));
	TestFalse(TEXT("details do not split off socket contributions"), Body.Contains(TEXT("宝石")) || Body.Contains(TEXT("镶嵌")));
	TestFalse(TEXT("details omit explanatory and conditional text"), Body.Contains(TEXT("尚未生效")) || Body.Contains(TEXT("条件生效")));
	const auto Rows = GameXXKCharacterDetailedAttributes::Build(Sub, Owner);
	TSet<FString> AttributeLabels;
	for (const auto& Row : Rows)
	{
		AttributeLabels.Add(Row.Label);
		TestTrue(TEXT("each main row remains one value and holds a separate hover description"), Row.Note.Contains(TEXT("\n\n")) && !Row.Value.IsEmpty() && !Row.bSection);
		TestFalse(TEXT("inactive affixes are not presented as additional attributes"), Row.Id.ToString().StartsWith(TEXT("Affix.")));
	}
	TestEqual(TEXT("each attribute appears only once"), AttributeLabels.Num(), Rows.Num());
	TestFalse(TEXT("ice rewards share the ice-damage row"), Rows.ContainsByPredicate([](const auto& Row) { return Row.Id == FName(TEXT("GemFrostReward")); }));
	for (const auto& Node : FGameXXKTalentCatalog::GetDefinitions()) Sub->GetMutableRuntimeState().Talents.NodeRanks.Add(Node.Id, Node.MaxRank);
	Inventory->RefreshVisibleRuntimeValues();
	Body = Inventory->GetCharacterTabBodyTextForTest().ToString();
	TestTrue(TEXT("permanent talent changes refresh the final critical multiplier"), Body.Contains(TEXT("暴击伤害倍率 200%")));
	TestTrue(TEXT("critical chance reflects the purchased talents"), Body.Contains(TEXT("暴击率 20%")));
	auto& Collection = Sub->GetMutableRuntimeState().EquipmentCollection;
	const auto* Loadout = Collection.CharacterLoadouts.Find(Owner);
	if (!TestNotNull(TEXT("the equipped fixture loadout exists"), Loadout)) return false;
	auto* Weapon = Collection.EquipmentInstances.FindByPredicate([Loadout](const auto& Item) { return Item.InstanceId == Loadout->WeaponInstanceId; });
	if (!TestNotNull(TEXT("the equipped fixture weapon exists"), Weapon) || Weapon->SocketedGems.IsEmpty()) return false;
	Weapon->SocketedGems[0].Type = EGameXXKGemType::AttackPercent;
	Inventory->RefreshVisibleRuntimeValues();
	Body = Inventory->GetCharacterTabBodyTextForTest().ToString();
	TestTrue(TEXT("one attack row includes the 8-percent gem margin and the 100-percent talent multiplier"), Body.Contains(TEXT("攻击加成 +114.46%")));
	auto* Scroll = Cast<UScrollBox>(Inventory->WidgetTree->FindWidget(TEXT("InventoryDetailedAttributesScrollBox")));
	if (!TestNotNull(TEXT("details have a scrollable list"), Scroll)) return false;
	Scroll->TakeWidget(); // UScrollBox reads zero until its Slate widget exists.
	Scroll->SetScrollOffset(40);
	const auto Session = Inventory->CaptureEmbeddedSessionState();
	auto* Restored = NewObject<UGameXXKInventoryWindowWidget>();
	Restored->SetMVPSubsystem(Sub); Restored->OpenFreeInventory(); Restored->TakeWidget();
	Restored->RestoreEmbeddedSessionState(Session);
	TestTrue(TEXT("returning to the backpack preserves the detailed page"), Restored->IsDetailedAttributesOpenForTest());
	auto* RestoredScroll = Cast<UScrollBox>(Restored->WidgetTree->FindWidget(TEXT("InventoryDetailedAttributesScrollBox")));
	RestoredScroll->TakeWidget();
	TestEqual(TEXT("restoring the page preserves its scroll position"), RestoredScroll->GetScrollOffset(), 40.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKDetailedAttributeTooltipTest,
	"GameXXK.DesktopTraining.BackpackAttributes.TooltipRulesSourcesAndFont",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDetailedAttributeTooltipTest::RunTest(const FString& Parameters)
{
	auto* Sub = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!Sub->StartGame()) return false;
	const FName Owner = FGameXXKEquipmentRules::HeroCharacterId();
	if (!EquipGemFixture(*this, Sub->GetMutableRuntimeState(), Owner)) return false;
	auto* Inventory = NewObject<UGameXXKInventoryWindowWidget>();
	Inventory->SetMVPSubsystem(Sub);
	Inventory->OpenFreeInventory();
	Inventory->TakeWidget();
	Inventory->OpenCharacterBackpackTabForTest(EGameXXKCharacterBackpackTab::Attributes);
	Inventory->SetDetailedAttributesOpen(true);
	const auto Rows = GameXXKCharacterDetailedAttributes::Build(Sub, Owner);
	auto* List = Cast<UVerticalBox>(Inventory->WidgetTree->FindWidget(TEXT("InventoryDetailedAttributesList")));
	if (!TestNotNull(TEXT("the compact attribute list exists"), List)) return false;
	for (int32 I = 0; I < Rows.Num(); ++I)
	{
		auto* Row = List->GetChildAt(I);
		TestNotNull(TEXT("every attribute row has a tooltip"), Row->GetToolTip());
		TestEqual(TEXT("the entire row can receive hover"), Row->GetVisibility(), ESlateVisibility::Visible);
		for (const TCHAR* Part : {TEXT("Title"), TEXT("Value"), TEXT("RuleHeading"), TEXT("Rule"), TEXT("SourceHeading"), TEXT("Sources")})
		{
			auto* Text = FindObject<UTextBlock>(Inventory->WidgetTree, *FString::Printf(TEXT("InventoryDetailTooltip%s_%d"), Part, I));
			if (TestNotNull(TEXT("tooltip sections use explicit text widgets"), Text))
				TestTrue(TEXT("all detailed-attribute tooltip text uses the body font"), Text->GetFont().FontObject && Text->GetFont().FontObject->GetPathName()==FGameXXKInRunUiStyle::FontPath(EGameXXKFontRole::Body));
		}
	}
	const auto* Armor = Rows.FindByPredicate([](const auto& Row) { return Row.Id == FName(TEXT("ArmorGain")); });
	if (TestNotNull(TEXT("armor has a source breakdown"), Armor))
	{
		TestTrue(TEXT("armor explains the shared marginal limit"), Armor->Note.Contains(TEXT("75%")) && Armor->Note.Contains(TEXT("折半后+105.00%")));
		TestTrue(TEXT("armor separates affixes, gems and fixed sets"), Armor->Note.Contains(TEXT("装备词缀")) && Armor->Note.Contains(TEXT("护甲宝石")) && Armor->Note.Contains(TEXT("固定套装：+10.00%")));
	}
	const FString MainBody = Inventory->GetCharacterTabBodyTextForTest().ToString();
	TestFalse(TEXT("hover explanations never return to the main list"), MainBody.Contains(TEXT("递减")) || MainBody.Contains(TEXT("名义")) || MainBody.Contains(TEXT("固定套装")));

	// Change a real source while the displayed final value stays rounded to 64.74%.
	auto& Collection = Sub->GetMutableRuntimeState().EquipmentCollection;
	const auto* Loadout = Collection.CharacterLoadouts.Find(Owner);
	if (!TestNotNull(TEXT("the fixture loadout exists"), Loadout)) return false;
	TArray<FGameXXKSocketedGem*> Gems;
	for (const auto Slot : {EGameXXKEquipmentSlot::Weapon, EGameXXKEquipmentSlot::Head, EGameXXKEquipmentSlot::Armor,
		EGameXXKEquipmentSlot::Belt, EGameXXKEquipmentSlot::Shoes, EGameXXKEquipmentSlot::Accessory})
	{
		const FName Id = FGameXXKEquipmentRules::GetLoadoutSlotInstanceId(*Loadout, Slot);
		auto* Item = Collection.EquipmentInstances.FindByPredicate([Id](const auto& Candidate) { return Candidate.InstanceId == Id; });
		if (!TestNotNull(TEXT("each fixture slot is equipped"), Item)) return false;
		for (auto& Gem : Item->SocketedGems)
		{
			Gem.Type = EGameXXKGemType::AttackPercent;
			Gem.Quality = EGameXXKGemQuality::Cosmic;
			Gems.Add(&Gem);
		}
	}
	if (!TestEqual(TEXT("the fixture supplies twelve sockets"), Gems.Num(), 12)) return false;
	Gems.Last()->Quality = EGameXXKGemQuality::Common;
	Inventory->RefreshVisibleRuntimeValues();
	const FString Before = Inventory->GetCharacterTabBodyTextForTest().ToString();
	const int32 AttackIndex = Rows.IndexOfByPredicate([](const auto& Row) { return Row.Id == FName(TEXT("RouteAttack")); });
	auto* Sources = FindObject<UTextBlock>(Inventory->WidgetTree, *FString::Printf(TEXT("InventoryDetailTooltipSources_%d"), AttackIndex));
	if (!TestNotNull(TEXT("attack tooltip has a source section"), Sources)) return false;
	TestTrue(TEXT("the first nominal source is shown"), Sources->GetText().ToString().Contains(TEXT("+473.25%")));
	Gems.Last()->Quality = EGameXXKGemQuality::Rare;
	Inventory->RefreshVisibleRuntimeValues();
	TestEqual(TEXT("this source change leaves all displayed totals unchanged"), Inventory->GetCharacterTabBodyTextForTest().ToString(), Before);
	TestTrue(TEXT("the tooltip still refreshes the changed nominal source"), Sources->GetText().ToString().Contains(TEXT("+473.50%")));
	TestTrue(TEXT("the final attack percent remains 64.74"), Before.Contains(TEXT("攻击加成 +64.74%")));
	return true;
}

#endif
