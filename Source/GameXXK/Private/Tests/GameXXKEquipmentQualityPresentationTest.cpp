#include "Misc/AutomationTest.h"
#include "UI/GameXXKEquipmentQualityStyle.h"
#include "UI/GameXXKEquipmentTooltipPresentation.h"
#include "UI/GameXXKInRunUiStyle.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "GameXXKEquipmentRules.h"
#include "GameXXKGemRules.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Materials/MaterialInterface.h"
#include "Engine/GameInstance.h"
#include "UI/GameXXKInventoryWindowWidget.h"
#include "GameXXKEquipmentEconomyRules.h"
#include "Components/OverlaySlot.h"
#include "Widgets/SWidget.h"
#include "Components/VerticalBox.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "UI/GameXXKCardTooltipPresentation.h"
#include "Materials/MaterialInstanceDynamic.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKEquipmentQualityLayersTest,"GameXXK.Equipment.Presentation.QualityLayers",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKEquipmentQualityLayersTest::RunTest(const FString& Parameters)
{
	auto* Tree=NewObject<UWidgetTree>();auto* Button=Tree->ConstructWidget<UButton>();auto* Overlay=Tree->ConstructWidget<UOverlay>();auto* Art=Tree->ConstructWidget<UImage>();
	Overlay->AddChildToOverlay(Art);Button->SetContent(Overlay);Tree->RootWidget=Button;
	auto* Name=Tree->ConstructWidget<UTextBlock>();Name->SetText(FText::FromString(TEXT("破军护手")));Name->SetFont(FGameXXKInRunUiStyle::TitleFont(26,true));
	for(int32 Rank=1;Rank<=10;++Rank)
	{
		const auto Q=static_cast<EGameXXKEquipmentQuality>(Rank);
		for(const auto* Layer:{TEXT("Fill"),TEXT("Outline"),TEXT("Surface"),TEXT("Frame"),TEXT("Header")})TestNotNull(*GameXXKEquipmentQualityStyle::MaterialPath(Q,Layer),LoadObject<UMaterialInterface>(nullptr,*GameXXKEquipmentQualityStyle::MaterialPath(Q,Layer)));
		GameXXKEquipmentQualityStyle::ApplyName(Name,Q);const auto Font=Name->GetFont();
		TestEqual(TEXT("common has no outline; higher ranks use shared upgrade sizes"),Font.OutlineSettings.OutlineSize,Rank==1?0:Rank>=6?3:2);
		TestEqual(TEXT("fill and outline alpha are separate only for upgraded equipment"),Font.OutlineSettings.bSeparateFillAlpha,Rank>1);
		GameXXKEquipmentQualityStyle::ApplySlot(Tree,Button,Art,Q,FVector2D(100,100));
		TestEqual(TEXT("layer count does not grow on quality changes"),Overlay->GetChildrenCount(),4);
		TestTrue(TEXT("equipment art stays above paper and aura"),Overlay->GetChildAt(2)==Art);
		TestEqual(TEXT("aura begins at sixth tier"),Overlay->GetChildAt(1)->GetVisibility(),Rank>=6?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);
	}
	GameXXKEquipmentQualityStyle::ApplyName(Name,EGameXXKEquipmentQuality::Common);
	TestNull(TEXT("common clears upgraded fill material"),Name->GetFont().FontMaterial);
	GameXXKEquipmentQualityStyle::ApplySlot(Tree,Button,Art,EGameXXKEquipmentQuality::Invalid,FVector2D(100,100));
	TestEqual(TEXT("empty cell clears rarity frame"),Overlay->GetChildAt(3)->GetVisibility(),ESlateVisibility::Collapsed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKEquipmentStructuredTooltipTest,"GameXXK.Equipment.Presentation.StructuredTooltip",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKEquipmentStructuredTooltipTest::RunTest(const FString& Parameters)
{
	FGameXXKEquipmentAffixRoll Roll;Roll.AffixId=TEXT("Affix.PoJun.VulnerableTargetDamage");Roll.Magnitude=393;Roll.Unit=EGameXXKEquipmentMagnitudeUnit::BasisPoints;
	const FString Line=GameXXKEquipmentTooltipPresentation::AffixLine(Roll);
	TestTrue(TEXT("condition and bonus are explicit"),Line.Contains(TEXT("破绽")) && Line.Contains(TEXT("3.93%")) && !Line.Contains(TEXT("乘隙")));
	auto* MVP=NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());if(!MVP->StartGame())return false;
	auto& State=MVP->GetMutableRuntimeState();FGameXXKEquipmentCreateRequest Request;Request.Set=EGameXXKEquipmentSet::PoJun;Request.Quality=EGameXXKEquipmentQuality::Common;Request.ItemLevel=1;
	FName Id;if(!FGameXXKEquipmentRules::CreateRolledInstance(State.EquipmentCollection,Request,Id))return false;
	auto* Tree=NewObject<UWidgetTree>();auto* Frame=Cast<UBorder>(GameXXKEquipmentTooltipPresentation::Build(Tree,MVP,Id));Tree->RootWidget=Frame;
	if(!TestNotNull(TEXT("equipment paper is built"),Frame))return false;
	auto CountIcons=[&](const FString& Path)
	{
		int32 Count=0;TArray<UWidget*> Pending={Frame};while(!Pending.IsEmpty()){auto* W=Pending.Pop();if(auto* I=Cast<UImage>(W))if(const auto* Resource=I->GetBrush().GetResourceObject())if(Resource->GetPathName().Contains(Path))++Count;if(auto* Panel=Cast<UPanelWidget>(W))Pending.Append(Panel->GetAllChildren());}return Count;
	};
	TestEqual(TEXT("empty socket is an item slot image"),CountIcons(TEXT("T_MasterV2_ItemSlot")),1);
	auto* Item=State.EquipmentCollection.EquipmentInstances.FindByPredicate([&](const auto& E){return E.InstanceId==Id;});
	if(!TestNotNull(TEXT("rolled item exists"),Item))return false;
	Item->SocketedGems[0].Type=EGameXXKGemType::Attack;Item->SocketedGems[0].Quality=EGameXXKGemQuality::Rare;
	GameXXKEquipmentTooltipPresentation::Populate(Frame,Tree,MVP,Id);
	const auto GemPath=FGameXXKGemRules::GetIconTexturePath(EGameXXKGemType::Attack,EGameXXKGemQuality::Rare).ToString();
	TestEqual(TEXT("socket shows the real gem icon"),CountIcons(GemPath),1);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKEquipmentTooltipStabilityTest,"GameXXK.Equipment.Presentation.StableHoverAndCenteredArt",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKEquipmentTooltipStabilityTest::RunTest(const FString& Parameters)
{
	auto* MVP=NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());if(!MVP->StartGame())return false;
	auto& State=MVP->GetMutableRuntimeState();FGameXXKEquipmentCreateRequest Request;Request.Set=EGameXXKEquipmentSet::PoJun;Request.Quality=EGameXXKEquipmentQuality::Rare;Request.ItemLevel=1;Request.bForceSlot=true;Request.ForcedSlot=EGameXXKEquipmentSlot::Weapon;
	FName Id;if(!FGameXXKEquipmentRules::CreateRolledInstance(State.EquipmentCollection,Request,Id))return false;
	FGameXXKEquipmentTransactionResult Equipped;if(!FGameXXKEquipmentEconomyRules::Equip(State,FGameXXKEquipmentRules::HeroCharacterId(),EGameXXKEquipmentSlot::Weapon,Id,Equipped))return false;
	auto* Inventory=NewObject<UGameXXKInventoryWindowWidget>();Inventory->SetMVPSubsystem(MVP);Inventory->TakeWidget();Inventory->OpenFreeInventoryForTest();Inventory->RefreshVisibleRuntimeValues();
	auto* Button=Cast<UButton>(Inventory->WidgetTree->FindWidget(TEXT("InventoryEquipmentSlot_Weapon")));
	auto* Icon=Cast<UImage>(Inventory->WidgetTree->FindWidget(TEXT("InventoryEquipmentIcon_Weapon")));
	if(!TestNotNull(TEXT("equipped item button"),Button)||!TestNotNull(TEXT("equipped item art"),Icon))return false;
	auto* IconSlot=Cast<UOverlaySlot>(Icon->Slot);TestTrue(TEXT("art is centered on both axes"),IconSlot && IconSlot->GetHorizontalAlignment()==HAlign_Center && IconSlot->GetVerticalAlignment()==VAlign_Center);
	TestEqual(TEXT("art fills the approved 96px safe area"),FVector2D(Icon->GetBrush().ImageSize),FVector2D(96,96));
	const auto SlateButton=Button->TakeWidget();const auto Before=SlateButton->GetToolTip();
	if(!TestTrue(TEXT("real Slate tooltip exists"),Before.IsValid()))return false;
	for(int32 I=0;I<12;++I)Inventory->RefreshVisibleRuntimeValues();
	TestTrue(TEXT("refresh does not recreate an active hover tooltip"),SlateButton->GetToolTip()==Before);
	if(auto* Tip=Button->GetToolTip())
	{
		Tip->TakeWidget()->SlatePrepass(1.f);TestTrue(TEXT("tooltip height stays within desktop budget"),Tip->GetDesiredSize().Y<=762);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKEquipmentTooltipPaperFitTest,"GameXXK.Equipment.Presentation.CompactQualityPaper",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKEquipmentTooltipPaperFitTest::RunTest(const FString&)
{
	auto* MVP=NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());if(!MVP->StartGame())return false;
	auto& State=MVP->GetMutableRuntimeState();FGameXXKEquipmentCreateRequest Request;Request.Set=EGameXXKEquipmentSet::XuanJia;Request.Quality=EGameXXKEquipmentQuality::Cosmic;Request.ItemLevel=100;
	FName Id;if(!FGameXXKEquipmentRules::CreateRolledInstance(State.EquipmentCollection,Request,Id))return false;
	auto* Item=State.EquipmentCollection.EquipmentInstances.FindByPredicate([&](const auto& E){return E.InstanceId==Id;});
	if(!Item)return false;
	for(int32 I=0;I<Item->SocketedGems.Num();++I){Item->SocketedGems[I].Type=static_cast<EGameXXKGemType>(I%3+1);Item->SocketedGems[I].Quality=EGameXXKGemQuality::Cosmic;}
	auto* Tree=NewObject<UWidgetTree>();auto* Frame=Cast<UBorder>(GameXXKEquipmentTooltipPresentation::Build(Tree,MVP,Id));Tree->RootWidget=Frame;
	if(!Frame)return false;
	Frame->TakeWidget()->SlatePrepass(1.f);
	TestTrue(TEXT("long equipment paper fits a compact reading width"),Frame->GetDesiredSize().X<=493);
	TestTrue(TEXT("long equipment paper stays within the work-area height budget"),Frame->GetDesiredSize().Y<=762);
	TestNotNull(TEXT("paper and header use an alpha-masked UI material"),Cast<UMaterialInterface>(Frame->Background.GetResourceObject()));
	TArray<UWidget*> Pending={Frame};bool HasTitle=false,HasFit=false,HasRim=false;
	while(!Pending.IsEmpty())
	{
		auto* W=Pending.Pop();
		if(auto* T=Cast<UTextBlock>(W))
		{
			if(T->GetFont().Size==26)HasTitle=true;
			if(T->GetText().ToString().Contains(TEXT("+172")))TestFalse(TEXT("gem label remains on one line"),T->GetAutoWrapText());
		}
		if(auto* Fit=Cast<UScaleBox>(W))
		{
			HasFit=true;auto* Slot=Cast<UScaleBoxSlot>(Fit->GetContent()->Slot);
			TestTrue(TEXT("height fitting cannot center the body inside oversized side gutters"),Slot && Slot->GetHorizontalAlignment()==HAlign_Left);
		}
		if(auto* I=Cast<UImage>(W))if(Cast<UMaterialInstanceDynamic>(I->GetBrush().GetResourceObject()))HasRim=true;
		if(auto* Panel=Cast<UPanelWidget>(W))Pending.Append(Panel->GetAllChildren());
	}
	TestTrue(TEXT("name keeps its original title size"),HasTitle);TestTrue(TEXT("body has measured fitting"),HasFit);TestTrue(TEXT("quality rim is present"),HasRim);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKEquipmentInlinePillParityTest,"GameXXK.Equipment.Presentation.InlinePillMatchesBattle",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKEquipmentInlinePillParityTest::RunTest(const FString&)
{
	auto* Tree=NewObject<UWidgetTree>();
	for(const TCHAR* Term:{TEXT("格挡"),TEXT("冲锋"),TEXT("收招"),TEXT("护甲"),TEXT("反击")})
	{
		auto Make=[&](const FString& Prose,bool Equipment)->UBorder*
		{
			auto* Root=Tree->ConstructWidget<UVerticalBox>();FGameXXKCardTooltipPresentationStyle Style;Style.bDisplayBodyFont=Equipment;Style.BodyFontSize=Equipment?16:20;
			GameXXKCardTooltipPresentation::PopulateBody(Tree,Root,TEXT(""),Prose,Style);
			TArray<UWidget*> Pending={Root};while(!Pending.IsEmpty()){auto* W=Pending.Pop();if(auto* B=Cast<UBorder>(W))if(auto* T=Cast<UTextBlock>(B->GetContent()))if(T->GetText().ToString()==Term)return B;if(auto* P=Cast<UPanelWidget>(W))Pending.Append(P->GetAllChildren());}return nullptr;
		};
		const FString BattleProse=FString(Term)==TEXT("护甲")?TEXT("获得护甲"):FString(Term)+TEXT("：效果");
		auto* Battle=Make(BattleProse,false);auto* Equipment=Make(TEXT("装备使")+FString(Term)+TEXT("效果提高"),true);
		if(!TestNotNull(TEXT("battle pill"),Battle)||!TestNotNull(TEXT("inline equipment pill"),Equipment))return false;
		TestEqual(TEXT("same term retains battle color anywhere in prose"),Equipment->Background.TintColor.GetSpecifiedColor(),Battle->Background.TintColor.GetSpecifiedColor());
		TestEqual(TEXT("pill padding matches battle"),Equipment->GetPadding(),Battle->GetPadding());
		TestTrue(TEXT("pill font matches battle"),Cast<UTextBlock>(Equipment->GetContent())->GetFont()==Cast<UTextBlock>(Battle->GetContent())->GetFont());
	}
	return true;
}
#endif
