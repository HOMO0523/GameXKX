#include "UI/GameXXKEquipmentTooltipPresentation.h"
#include "UI/GameXXKLocalization.h"
#include "GameXXKTravelMoneyRules.h"
#include "GameXXKHuntRules.h"
#include "UI/GameXXKEquipmentQualityStyle.h"
#include "UI/GameXXKCardTooltipPresentation.h"
#include "UI/GameXXKInRunUiStyle.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "GameXXKEquipmentRules.h"
#include "GameXXKEquipmentCatalog.h"
#include "GameXXKEquipmentSetCatalog.h"
#include "GameXXKAffixCatalog.h"
#include "GameXXKGemRules.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/SizeBox.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Fonts/FontMeasure.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Widgets/SToolTip.h"
#include "GenericPlatform/GenericWindow.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

namespace
{
	class SEquipmentToolTip final : public SToolTip
	{
	public:
		void Construct(const FArguments& Args){SToolTip::Construct(Args);SetCanTick(true);}
		virtual ~SEquipmentToolTip() override{ClearWindowMask();}
		virtual void OnClosed() override{ClearWindowMask();SToolTip::OnClosed();}
		virtual void Tick(const FGeometry& Geometry,double Time,float Delta) override
		{
			SToolTip::Tick(Geometry,Time,Delta);
#if PLATFORM_WINDOWS
			if(!FSlateApplication::IsInitialized())return;
			const auto Window=FSlateApplication::Get().FindWidgetWindow(AsShared());
			if(!Window || Window->GetType()!=EWindowType::ToolTip || !Window->GetNativeWindow())return;
			const HWND Handle=static_cast<HWND>(Window->GetNativeWindow()->GetOSWindowHandle());
			if(!Handle || !::IsWindow(Handle))return;
			const float Scale=Geometry.GetAccumulatedLayoutTransform().GetScale();
			// Native tooltip HWNDs retain a larger allocation for frequent resizing.
			// Only Slate's current painted size is visible, never GetClientRect().
			const FVector2D PaintedSize=Window->GetSizeInScreen();
			const FIntPoint Size(FMath::CeilToInt(PaintedSize.X),FMath::CeilToInt(PaintedSize.Y));
			if(Window==MaskedWindow.Pin() && Size==MaskedSize && FMath::IsNearlyEqual(Scale,MaskedScale))return;
			ClearWindowMask();
			// UE native tooltips use PerWindow transparency, so texture alpha alone
			// exposes a black rectangular popup. Clip its native silhouette too;
			// the UI material still provides the fine antialiased paper/frame edge.
			const int32 Inset=FMath::Max(1,FMath::RoundToInt(3.f*Scale));
			const int32 Diameter=FMath::Max(2,FMath::RoundToInt(18.f*Scale));
			HRGN Region=::CreateRoundRectRgn(Inset,Inset,Size.X-Inset+1,Size.Y-Inset+1,Diameter,Diameter);
			if(Region && ::SetWindowRgn(Handle,Region,true)){MaskedWindow=Window;MaskedSize=Size;MaskedScale=Scale;}
			else if(Region)::DeleteObject(Region);
#endif
		}
	private:
		void ClearWindowMask()
		{
#if PLATFORM_WINDOWS
			if(const auto Window=MaskedWindow.Pin())if(Window->GetNativeWindow())
			{
				const HWND Handle=static_cast<HWND>(Window->GetNativeWindow()->GetOSWindowHandle());
				if(Handle && ::IsWindow(Handle))
				{
					const FVector2D Size=Window->GetSizeInScreen();
					HRGN Region=::CreateRectRgn(0,0,FMath::CeilToInt(Size.X),FMath::CeilToInt(Size.Y));
					if(Region && !::SetWindowRgn(Handle,Region,true))::DeleteObject(Region);
				}
			}
#endif
			MaskedWindow.Reset();MaskedSize=FIntPoint::ZeroValue;MaskedScale=0;
		}
		TWeakPtr<SWindow> MaskedWindow;
		FIntPoint MaskedSize=FIntPoint::ZeroValue;
		float MaskedScale=0;
	};
	FText ModifierLabel(EGameXXKEquipmentModifierKind Kind)
	{
		using K=EGameXXKEquipmentModifierKind;
#define LABEL(Kind, Text) case K::Kind: return NSLOCTEXT("EquipmentAffixUI", #Kind, Text)
		switch(Kind)
		{
		LABEL(MaxHealth,"气血");LABEL(MaxMana,"内力");LABEL(Attack,"攻击");LABEL(Defense,"防御");LABEL(Speed,"速度");
		LABEL(DirectDamage,"物理伤害");LABEL(MultiHitDamage,"多段伤害");LABEL(ArmorBreakStacks,"破甲层数");
		LABEL(VulnerableTargetDamage,"对破绽目标的伤害");LABEL(FirstAttackDamage,"每回合首次攻击伤害");
		LABEL(ArmorGain,"护甲获得量");LABEL(ArmorRetention,"护甲保留比例");LABEL(CounterDamage,"反击伤害");
		LABEL(GuardReduction,"援护减伤");LABEL(LowHealthProtection,"低气血防护");LABEL(Healing,"治疗量");
		LABEL(Cleanse,"净化加成");LABEL(OverhealConversion,"溢出治疗转化比例");LABEL(ManaRecovery,"内力恢复量");
		LABEL(EmergencyHealing,"低气血治疗量");LABEL(Draw,"抽牌数量");LABEL(LowCostBonus,"低费牌效果");
		LABEL(SharedEnergy,"气力生成");LABEL(ComboCount,"连击计数");LABEL(TemporaryCostReduction,"临时费用减免");
		LABEL(Poison,"施加中毒层数");LABEL(Bleed,"施加流血层数");LABEL(Burn,"施加灼烧层数");
		LABEL(DamageOverTime,"持续伤害");LABEL(StatusRetention,"保留状态层数");LABEL(TerrainPower,"地势效果");
		LABEL(TerrainCostReduction,"地势牌费用减免");LABEL(AdjacentAllyPower,"相邻队友增益");
		LABEL(FormationPower,"阵型收益");LABEL(TeamTerrainPower,"全队地势增益");
		default:return FText::GetEmpty();
		}
#undef LABEL
	}
	UTextBlock* Text(UWidgetTree* Tree,const FText& Value,int32 Size,FLinearColor Color,const EGameXXKFontRole Role = EGameXXKFontRole::Body)
	{
		auto* T=Tree->ConstructWidget<UTextBlock>();T->SetText(GameXXKLocalization::Localize(Value));T->SetFont(FGameXXKInRunUiStyle::Font(Role,Size));T->SetColorAndOpacity(Color);T->SetAutoWrapText(true);return T;
	}
	float TextWidth(const FString& Value,int32 Size,const EGameXXKFontRole Role = EGameXXKFontRole::Body)
	{
		return FSlateApplication::IsInitialized()
			? FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Value,FGameXXKInRunUiStyle::Font(Role,Size)).X
			: Value.Len()*Size*1.3f;
	}
	UImage* QualityLayer(UWidgetTree* Tree,EGameXXKEquipmentQuality Quality,const TCHAR* Layer,FVector2D Size)
	{
		auto* Image=Tree->ConstructWidget<UImage>();
		Image->SetVisibility(ESlateVisibility::HitTestInvisible);
		Image->SetDesiredSizeOverride(FVector2D::ZeroVector);
		if(auto* Parent=LoadObject<UMaterialInterface>(nullptr,*GameXXKEquipmentQualityStyle::MaterialPath(Quality,Layer)))
		{
			auto* Material=UMaterialInstanceDynamic::Create(Parent,Image);
			Material->SetScalarParameterValue(TEXT("CardWidth"),Size.X);
			Material->SetScalarParameterValue(TEXT("CardHeight"),Size.Y);
			Material->SetScalarParameterValue(TEXT("TextAspect"),Size.X/FMath::Max(1.0,Size.Y));
			FSlateBrush Brush;Brush.DrawAs=ESlateBrushDrawType::Image;Brush.ImageSize=Size;Brush.SetResourceObject(Material);Image->SetBrush(Brush);
		}
		Image->ForceVolatile(Quality>EGameXXKEquipmentQuality::Common);
		return Image;
	}
}

FText GameXXKEquipmentTooltipPresentation::GetModifierLabel(const EGameXXKEquipmentModifierKind Kind)
{
	return ModifierLabel(Kind);
}

void GameXXKEquipmentTooltipPresentation::Bind(UWidget* Owner,UWidget* Content)
{
	if(!Owner || !Content || Owner->GetToolTip()==Content)return;
	Owner->SetToolTip(Content);
	const auto Popup=SNew(SEquipmentToolTip).TextMargin(FMargin(0)).BorderImage(nullptr)[Content->TakeWidget()];
	Owner->TakeWidget()->SetToolTip(Popup);
}

UWidget* GameXXKEquipmentTooltipPresentation::BuildGem(UWidgetTree* Tree,FName ItemId)
{
	EGameXXKGemType Type{};EGameXXKGemQuality GemQuality{};
	const bool bOrder=FGameXXKHuntRules::IsOrder(ItemId);
	if(!Tree || (!bOrder&&!FGameXXKGemRules::TryParseItemId(ItemId,Type,GemQuality)))return nullptr;
	const auto Quality=bOrder?FGameXXKHuntRules::OrderQuality(ItemId):FGameXXKGemRules::GetPresentationQuality(GemQuality);
	auto* Frame=Tree->ConstructWidget<UBorder>();Frame->SetPadding(FMargin(0));Frame->SetVisibility(ESlateVisibility::HitTestInvisible);
	auto* Layers=Tree->ConstructWidget<UOverlay>();Frame->SetContent(Layers);
	auto* Bounds=Tree->ConstructWidget<USizeBox>();Bounds->SetWidthOverride(380);
	auto* Body=Tree->ConstructWidget<UVerticalBox>();Bounds->SetContent(Body);
	auto* ContentSlot=Layers->AddChildToOverlay(Bounds);ContentSlot->SetPadding(FMargin(18,14));
	auto* Name=Text(Tree,bOrder?FGameXXKHuntRules::OrderName(ItemId):FGameXXKGemRules::GetDisplayName(Type,GemQuality),24,FGameXXKInRunUiStyle::Ink(),EGameXXKFontRole::Title);
	Name->SetWrapTextAt(380);GameXXKEquipmentQualityStyle::ApplyName(Name,Quality);Body->AddChildToVerticalBox(Name);
	auto* Description=Text(Tree,bOrder?FGameXXKHuntRules::OrderDescription(ItemId):FGameXXKGemRules::GetDescription(Type,GemQuality),17,FGameXXKInRunUiStyle::Ink());
	Description->SetWrapTextAt(380);Body->AddChildToVerticalBox(Description)->SetPadding(FMargin(0,12,0,0));
	Body->TakeWidget()->SlatePrepass(1.f);Name->TakeWidget()->SlatePrepass(1.f);
	const FVector2D Size(416,Body->GetDesiredSize().Y+28);
	auto* Surface=QualityLayer(Tree,Quality,TEXT("Header"),Size);
	if(auto* Material=Cast<UMaterialInstanceDynamic>(Surface->GetBrush().GetResourceObject()))Material->SetScalarParameterValue(TEXT("HeaderHeight"),Name->GetDesiredSize().Y+28);
	Frame->SetBrush(Surface->GetBrush());Frame->ForceVolatile(Quality>=EGameXXKEquipmentQuality::Ascendant);
	auto* RimSlot=Layers->AddChildToOverlay(QualityLayer(Tree,Quality,TEXT("Frame"),Size));
	RimSlot->SetHorizontalAlignment(HAlign_Fill);RimSlot->SetVerticalAlignment(VAlign_Fill);
	return Frame;
}

bool GameXXKEquipmentTooltipPresentation::ApplyGem(UWidgetTree* Tree,UButton* Button,FName ItemId,FVector2D Size,UImage* Icon)
{
	if (Button && ItemId == FGameXXKTravelMoneyRules::ItemId())
	{
		Button->SetToolTip(nullptr);
		Button->SetToolTipText(GameXXKLocalization::Source(TEXT("行旅钱\n背包和仓库中的行旅钱均可用于局内行商。\n每个分解固定返还2500金币。")));
		return true;
	}
	const auto Quality=FGameXXKGemRules::GetItemPresentationQuality(ItemId);
	if(!Tree || !Button || Quality==EGameXXKEquipmentQuality::Invalid)return false;
	if(!Icon)
	{
		TArray<UWidget*> Pending={Button->GetContent()};
		while(!Pending.IsEmpty() && !Icon)
		{
			auto* Current=Pending.Pop();Icon=Cast<UImage>(Current);
			if(auto* Panel=Cast<UPanelWidget>(Current))Pending.Append(Panel->GetAllChildren());
		}
	}
	GameXXKEquipmentQualityStyle::ApplySlot(Tree,Button,Icon,Quality,Size);
	struct FCachedGem { FName ItemId; TWeakObjectPtr<UWidget> Tooltip; uint64 LanguageRevision=0; };
	static TMap<TWeakObjectPtr<UButton>,FCachedGem> Cache;
	for(auto It=Cache.CreateIterator();It;++It)if(!It.Key().IsValid())It.RemoveCurrent();
	auto& Entry=Cache.FindOrAdd(Button);
	if(Entry.ItemId!=ItemId || !Entry.Tooltip.IsValid() || Entry.LanguageRevision!=GameXXKLocalization::GetRevision())
	{
		Entry.ItemId=ItemId;Entry.Tooltip=BuildGem(Tree,ItemId);Entry.LanguageRevision=GameXXKLocalization::GetRevision();
	}
	if(Button->GetToolTip()!=Entry.Tooltip.Get()){Button->SetToolTipText(FText::GetEmpty());Bind(Button,Entry.Tooltip.Get());}
	return true;
}

FString GameXXKEquipmentTooltipPresentation::AffixLine(const FGameXXKEquipmentAffixRoll& Roll)
{
	const auto* Definition=FGameXXKAffixCatalog::FindDefinition(Roll.AffixId);
	if(!Definition || Definition->ModifierKind==EGameXXKEquipmentModifierKind::MaxMana)return {};
	if(Definition->ModifierKind==EGameXXKEquipmentModifierKind::ZhuiFengLateCardDamage)
	{
		return GameXXKLocalization::Localize(NSLOCTEXT("EquipmentAffixUI", "WindMomentum",
			"每回合全队第3张牌起，自身牌伤每张递增1%；同名叠加，重放不计数。")).ToString();
	}
	const auto Label=ModifierLabel(Definition->ModifierKind);
	if(Label.IsEmpty())return Definition->DisplayName.ToString();
	return Roll.Unit==EGameXXKEquipmentMagnitudeUnit::BasisPoints?
		FString::Printf(TEXT("%s +%.2f%%"),*Label.ToString(),Roll.Magnitude/100.0):FString::Printf(TEXT("%s +%d"),*Label.ToString(),Roll.Magnitude);
}

UWidget* GameXXKEquipmentTooltipPresentation::Build(UWidgetTree* Tree,const UGameXXKMVPSubsystem* Subsystem,FName InstanceId,FName CompareCharacterId)
{
	if(!Tree || !Subsystem)return nullptr;
	auto* Frame=Tree->ConstructWidget<UBorder>();Populate(Frame,Tree,Subsystem,InstanceId,CompareCharacterId);return Frame;
}

void GameXXKEquipmentTooltipPresentation::Populate(UBorder* Frame,UWidgetTree* Tree,const UGameXXKMVPSubsystem* Subsystem,FName InstanceId,FName CompareCharacterId)
{
	if(!Frame || !Tree || !Subsystem)return;
	const auto* Item=FGameXXKEquipmentRules::FindInstance(Subsystem->GetRuntimeState().EquipmentCollection,InstanceId);
	const auto* Definition=Item?FGameXXKEquipmentCatalog::FindDefinition(Item->BaseEquipmentId):nullptr;if(!Definition)return;
	if(CompareCharacterId.IsNone())CompareCharacterId=FGameXXKEquipmentRules::HeroCharacterId();
	const auto Detail=BuildDetail(Subsystem,InstanceId).ToString();
	FGameXXKEquipmentTooltipSnapshot Snapshot;const bool HasStats=Subsystem->GetEquipmentTooltipSnapshot(InstanceId,CompareCharacterId,Snapshot);
	static TMap<TWeakObjectPtr<UBorder>,uint32> Cache;
	float AvailableWidth=600,AvailableHeight=720;
	if(FSlateApplication::IsInitialized())
	{
		const auto& App=FSlateApplication::Get();const auto Area=App.GetPreferredWorkArea();const auto Window=App.GetActiveTopLevelWindow();
		const float Scale=FMath::Max(.5f,App.GetApplicationScale()*(Window.IsValid()?Window->GetDPIScaleFactor():1.0f));
		AvailableWidth=FMath::Clamp((Area.Right-Area.Left)/Scale-64.0f,220.0f,600.0f);
		AvailableHeight=FMath::Clamp((Area.Bottom-Area.Top)/Scale-48.0f,140.0f,760.0f);
	}
	uint32 Signature=GetTypeHash(Detail+CompareCharacterId.ToString());
	Signature=HashCombine(Signature,GetTypeHash(GameXXKLocalization::GetRevision()));
	Signature=HashCombine(Signature,GetTypeHash(AvailableWidth));Signature=HashCombine(Signature,GetTypeHash(AvailableHeight));
	if(HasStats){Signature=HashCombine(Signature,GetTypeHash(Snapshot.CharacterStatDeltas.Attack));Signature=HashCombine(Signature,GetTypeHash(Snapshot.CharacterStatDeltas.Defense));Signature=HashCombine(Signature,GetTypeHash(Snapshot.CharacterStatDeltas.MaxHealth));Signature=HashCombine(Signature,GetTypeHash(Snapshot.CharacterStatDeltas.Speed));}
	if(const auto* Existing=Cache.Find(Frame);Existing && *Existing==Signature)return;
	for(auto It=Cache.CreateIterator();It;++It)if(!It.Key().IsValid())It.RemoveCurrent();Cache.Add(Frame,Signature);
	FSlateBrush Paper;Paper.SetResourceObject(LoadObject<UTexture2D>(nullptr,FGameXXKInRunUiStyle::SlotPath));Paper.DrawAs=ESlateBrushDrawType::Box;Paper.Margin=FMargin(.065);Frame->SetBrush(Paper);Frame->SetPadding(FMargin(0));Frame->SetVisibility(ESlateVisibility::HitTestInvisible);
	const float NameWidth=TextWidth(Definition->DisplayName.ToString(),26,EGameXXKFontRole::Title)+12;
	// Do NOT use StaticEnum<>()->GetDisplayNameTextByValue(): UEnum::GetDisplayNameTextByIndex
	// reads the DisplayName metadata inside #if WITH_EDITOR and otherwise falls through to the
	// raw C++ entry name, so a packaged build would render "Common"/"Epic" here. The rule helper
	// resolves through the localization catalog and is correct in every build.
	const FText QualityName=FGameXXKEquipmentQualityRules::GetDisplayName(Item->Quality);
	FString Meta=FString::Printf(TEXT("%s · 等级 %d"),*QualityName.ToString(),Item->ItemLevel);if(Item->EnhancementLevel>0)Meta+=FString::Printf(TEXT(" · 强化 +%d"),Item->EnhancementLevel);
	float NaturalWidth=FMath::Max(NameWidth,TextWidth(Meta,15));
	for(const auto& Roll:Item->RolledAffixes)NaturalWidth=FMath::Max(NaturalWidth,TextWidth(AffixLine(Roll),16)+24);
	bool HasSetBonuses=false;
	for(const auto& Bonus:FGameXXKEquipmentSetCatalog::GetDefinitions())if(Bonus.Set==Definition->Set)
	{
		HasSetBonuses=true;NaturalWidth=FMath::Max(NaturalWidth,TextWidth(Bonus.Description.ToString(),16)+24);
	}
	float GemCellWidth=36;
	for(const auto& Gem:Item->SocketedGems)if(!Gem.IsEmpty())GemCellWidth=FMath::Max(GemCellWidth,43+TextWidth(FGameXXKGemRules::GetSocketText(Gem.Type,Gem.Quality).ToString(),16));
	NaturalWidth=FMath::Max(NaturalWidth,(GemCellWidth+10)*FMath::Min(2,Item->SocketedGems.Num()));
	const float Width=FMath::Min(FMath::Clamp(NaturalWidth,260.0f,460.0f),AvailableWidth-32);
	auto* Layers=Tree->ConstructWidget<UOverlay>();Frame->SetContent(Layers);
	auto* Box=Tree->ConstructWidget<USizeBox>();Box->SetWidthOverride(Width);
	auto* ContentSlot=Layers->AddChildToOverlay(Box);ContentSlot->SetPadding(FMargin(16,12));ContentSlot->SetHorizontalAlignment(HAlign_Left);ContentSlot->SetVerticalAlignment(VAlign_Top);
	auto* Contents=Tree->ConstructWidget<UVerticalBox>();Box->SetContent(Contents);
	auto* Heading=Tree->ConstructWidget<UVerticalBox>();Contents->AddChildToVerticalBox(Heading);
	auto* Name=Text(Tree,Definition->DisplayName,26,FGameXXKInRunUiStyle::Ink(),EGameXXKFontRole::Title);Name->SetWrapTextAt(Width);Heading->AddChildToVerticalBox(Name);GameXXKEquipmentQualityStyle::ApplyName(Name,Item->Quality);
	auto* MetaText=Text(Tree,GameXXKLocalization::Source(Meta),15,FGameXXKInRunUiStyle::MutedInk());MetaText->SetWrapTextAt(Width);Heading->AddChildToVerticalBox(MetaText)->SetPadding(FMargin(0,4,0,0));
	auto* BodyBounds=Tree->ConstructWidget<USizeBox>();Contents->AddChildToVerticalBox(BodyBounds);
	auto* Fit=Tree->ConstructWidget<UScaleBox>();Fit->SetStretch(EStretch::UserSpecified);Fit->SetUserSpecifiedScale(1);BodyBounds->SetContent(Fit);
	auto* NaturalBody=Tree->ConstructWidget<USizeBox>();NaturalBody->SetWidthOverride(Width);
	auto* FitSlot=Cast<UScaleBoxSlot>(Fit->AddChild(NaturalBody));FitSlot->SetHorizontalAlignment(HAlign_Left);FitSlot->SetVerticalAlignment(VAlign_Top);
	auto* Rows=Tree->ConstructWidget<UVerticalBox>();NaturalBody->SetContent(Rows);
	auto Section=[&](const FText& Label)
	{
		auto* Line=Tree->ConstructWidget<UBorder>();Line->SetBrushColor(FLinearColor(.25,.20,.13,.18));auto* Size=Tree->ConstructWidget<USizeBox>();Size->SetHeightOverride(1);Size->SetContent(Line);Rows->AddChildToVerticalBox(Size)->SetPadding(FMargin(0,10,0,5));
		Rows->AddChildToVerticalBox(Text(Tree,Label,17,FGameXXKInRunUiStyle::MutedInk()))->SetPadding(FMargin(0,0,0,3));
	};
	auto Prose=[&](const FString& Line)
	{
		auto* Body=Tree->ConstructWidget<UVerticalBox>();Rows->AddChildToVerticalBox(Body);
		FGameXXKCardTooltipPresentationStyle Style;Style.bDisplayBodyFont=true;Style.BodyFontSize=16;Style.TargetFontSize=16;Style.RowHeight=23;Style.WrapWidth=Width;
		GameXXKCardTooltipPresentation::PopulateBody(Tree,Body,Definition->DisplayName.ToString(),Line,Style);
	};
	if(HasStats)
	{
		Section(NSLOCTEXT("EquipmentUI","BaseStats","基础属性"));
		auto* Stats=Tree->ConstructWidget<UUniformGridPanel>();Stats->SetSlotPadding(FMargin(0,1,12,1));Rows->AddChildToVerticalBox(Stats);
		int32 Index=0;const auto Stat=[&](const TCHAR* Label,int32 Value){if(Value){auto* T=Text(Tree,GameXXKLocalization::Source(FString::Printf(TEXT("%s %+d"),Label,Value)),16,FGameXXKInRunUiStyle::Ink());T->SetAutoWrapText(false);Stats->AddChildToUniformGrid(T,Index/2,Index%2);++Index;}};
		const auto& S=Snapshot.ItemCurrentStats;Stat(TEXT("攻击"),S.Attack);Stat(TEXT("防御"),S.Defense);Stat(TEXT("气血"),S.MaxHealth);Stat(TEXT("速度"),S.Speed);
	}
	TArray<FString> Affixes;for(const auto& Roll:Item->RolledAffixes){const auto Line=AffixLine(Roll);if(!Line.IsEmpty())Affixes.Add(Line);}
	if(!Affixes.IsEmpty()){Section(NSLOCTEXT("EquipmentUI","Affixes","词缀加成"));for(const auto& Line:Affixes)Prose(Line);}
	if(!Item->SocketedGems.IsEmpty())
	{
		Section(NSLOCTEXT("EquipmentUI","Sockets","镶嵌"));auto* Grid=Tree->ConstructWidget<UUniformGridPanel>();Grid->SetSlotPadding(FMargin(0,2,10,2));Rows->AddChildToVerticalBox(Grid);
		for(int32 I=0;I<Item->SocketedGems.Num();++I)
		{
			const auto& Gem=Item->SocketedGems[I];auto* Row=Tree->ConstructWidget<UHorizontalBox>();
			auto* Icon=Tree->ConstructWidget<UImage>();FSlateBrush Brush;Brush.DrawAs=ESlateBrushDrawType::Image;Brush.ImageSize=FVector2D(36,36);
			const auto Path=Gem.IsEmpty()?FString(FGameXXKInRunUiStyle::SlotPath):FGameXXKGemRules::GetIconTexturePath(Gem.Type,Gem.Quality).ToString();Brush.SetResourceObject(LoadObject<UTexture2D>(nullptr,*Path));Icon->SetBrush(Brush);
			Row->AddChildToHorizontalBox(Icon)->SetVerticalAlignment(VAlign_Center);
			if(!Gem.IsEmpty())
			{
				auto* Label=Text(Tree,FGameXXKGemRules::GetSocketText(Gem.Type,Gem.Quality),16,FGameXXKInRunUiStyle::Ink());
				GameXXKEquipmentQualityStyle::ApplyName(Label,FGameXXKGemRules::GetPresentationQuality(Gem.Quality));
				Label->SetAutoWrapText(false);
				auto* Slot=Row->AddChildToHorizontalBox(Label);Slot->SetPadding(FMargin(7,0,0,0));Slot->SetVerticalAlignment(VAlign_Center);
			}
			Grid->AddChildToUniformGrid(Row,I/2,I%2);
		}
	}
	if(HasSetBonuses)
	{
		Section(FText::Format(GameXXKLocalization::Text(TEXT("Equipment.SetTitle")),
			FGameXXKEquipmentSetCatalog::GetSetDisplayName(Definition->Set)));
		for(const auto& Bonus:FGameXXKEquipmentSetCatalog::GetDefinitions())if(Bonus.Set==Definition->Set)
			Prose(FText::Format(GameXXKLocalization::Text(HasStats && Snapshot.CurrentSetPieceCounts.FindRef(Definition->Set)>=Bonus.RequiredPieces
				? TEXT("Equipment.SetBonusActive") : TEXT("Equipment.SetBonus")),
				FText::AsNumber(Bonus.RequiredPieces), GameXXKLocalization::Localize(Bonus.Description)).ToString());
	}
	if(HasStats && Snapshot.EquipError==EGameXXKEquipmentTransactionError::None && (Snapshot.CharacterStatDeltas.Attack || Snapshot.CharacterStatDeltas.Defense || Snapshot.CharacterStatDeltas.MaxHealth || Snapshot.CharacterStatDeltas.Speed))
	{
		Section(NSLOCTEXT("EquipmentUI","Compare","装备后变化"));
		const auto& D=Snapshot.CharacterStatDeltas;
		if(D.Attack)Prose(FString::Printf(TEXT("攻击 %+d"),D.Attack));if(D.Defense)Prose(FString::Printf(TEXT("防御 %+d"),D.Defense));if(D.MaxHealth)Prose(FString::Printf(TEXT("气血 %+d"),D.MaxHealth));if(D.Speed)Prose(FString::Printf(TEXT("速度 %+d"),D.Speed));
	}
	// Fit measured content, then shrink the paper to the same width. A centered
	// ScaleToFit inside a fixed-width paper created the empty side gutters.
	Heading->TakeWidget()->SlatePrepass(1.f);NaturalBody->TakeWidget()->SlatePrepass(1.f);
	const FVector2D BodySize=NaturalBody->GetDesiredSize();
	float HeaderHeight=Heading->GetDesiredSize().Y;
	float Scale=FMath::Min(1.0f,FMath::Max(40.0f,AvailableHeight-24-HeaderHeight)/FMath::Max(1.0,BodySize.Y));
	float FittedWidth=FMath::Min(Width,FMath::Max(BodySize.X*Scale,FMath::Max(NameWidth,TextWidth(Meta,15))));
	Name->SetWrapTextAt(FittedWidth);MetaText->SetWrapTextAt(FittedWidth);Heading->TakeWidget()->SlatePrepass(1.f);HeaderHeight=Heading->GetDesiredSize().Y;
	Scale=FMath::Min(Scale,FMath::Max(40.0f,AvailableHeight-24-HeaderHeight)/FMath::Max(1.0,BodySize.Y));
	FittedWidth=FMath::Min(Width,FMath::Max(BodySize.X*Scale,FMath::Max(NameWidth,TextWidth(Meta,15))));
	Fit->SetUserSpecifiedScale(Scale);BodyBounds->SetHeightOverride(BodySize.Y*Scale);Box->SetWidthOverride(FittedWidth);
	const FVector2D PanelSize(FittedWidth+32,HeaderHeight+BodySize.Y*Scale+24);
	const float WashHeight=HeaderHeight+18;
	auto* PaperSurface=QualityLayer(Tree,Item->Quality,TEXT("Header"),PanelSize);
	if(auto* Material=Cast<UMaterialInstanceDynamic>(PaperSurface->GetBrush().GetResourceObject()))Material->SetScalarParameterValue(TEXT("HeaderHeight"),WashHeight);
	Frame->SetBrush(PaperSurface->GetBrush());Frame->ForceVolatile(Item->Quality>=EGameXXKEquipmentQuality::Ascendant);
	auto* Rim=QualityLayer(Tree,Item->Quality,TEXT("Frame"),PanelSize);
	auto* RimSlot=Layers->AddChildToOverlay(Rim);RimSlot->SetHorizontalAlignment(HAlign_Fill);RimSlot->SetVerticalAlignment(VAlign_Fill);
}
