#include "SGameXXKDevWorkbench.h"
#include "Dev/GameXXKDevToolsSubsystem.h"
#include "UI/GameXXKInRunUiStyle.h"
#include "UI/GameXXKDesktopPaperStyle.h"
#include "Engine/Texture2D.h"
#include "Engine/Font.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SScrollBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Styling/CoreStyle.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformProcess.h"

namespace
{
	using Object=TSharedPtr<FJsonObject>;
	Object Obj() { return MakeShared<FJsonObject>(); }
	FString Json(const Object& O) { FString S;auto W=TJsonWriterFactory<>::Create(&S);FJsonSerializer::Serialize(O.ToSharedRef(),W);return S; }
	Object Parse(const FString& S) { Object O;auto R=TJsonReaderFactory<>::Create(S);FJsonSerializer::Deserialize(R,O);return O; }
	Object Data(UGameXXKDevToolsSubsystem* T,const FString& Command,const Object& Args=nullptr)
	{
		if (!T) return Obj();Object R=Obj();R->SetStringField(TEXT("command"),Command);if (Args) R->SetObjectField(TEXT("args"),Args);
		auto Result=Parse(T->ExecuteJson(Json(R)));const Object* D=nullptr;return Result && Result->TryGetObjectField(TEXT("data"),D)?*D:Obj();
	}
	FString Str(const Object& O,const TCHAR* Key) { FString V;if (O) O->TryGetStringField(Key,V);return V; }
	int32 Num(const Object& O,const TCHAR* Key) { double V=0;if(O) O->TryGetNumberField(Key,V);return static_cast<int32>(V); }
	TArray<TPair<FString,FString>> Sets()
	{ return {{TEXT("XuanJia"),TEXT("玄甲")},{TEXT("PoJun"),TEXT("破军")},{TEXT("QingNang"),TEXT("青囊")},{TEXT("ZhuiFeng"),TEXT("追风")},{TEXT("ShiGu"),TEXT("蚀骨")},{TEXT("ShanHe"),TEXT("山河")}}; }
}

void SGameXXKDevWorkbench::AddReferencedObjects(FReferenceCollector& Collector)
{ for (auto& R:Resources) Collector.AddReferencedObject(R); }

TSharedRef<SWidget> SGameXXKDevWorkbench::Scroller(TSharedRef<SWidget> Content)
{
	TSharedPtr<SScrollBox> Box;TSharedPtr<SScrollBar> Bar;
	const auto Pair=MakeShared<FInkScrollPair>();
	auto Result=SNew(SHorizontalBox)
		+SHorizontalBox::Slot().FillWidth(1)[SAssignNew(Box,SScrollBox).ScrollBarVisibility(EVisibility::Collapsed)+SScrollBox::Slot()[Content]]
		+SHorizontalBox::Slot().AutoWidth().Padding(5,0,0,0)[SNew(SBox).WidthOverride(12)
		[SAssignNew(Bar,SScrollBar).Style(&ScrollStyle).Thickness(FVector2D(10,10))
		.OnUserScrolled_Lambda([Pair](float Offset){if(auto Scroll=Pair->Box.Pin())Scroll->SetScrollOffset(Offset/FMath::Max(0.001f,1.0f-Pair->Fraction)*Scroll->GetScrollOffsetOfEnd());})]];
	Pair->Box=Box;Pair->Bar=Bar;ScrollPairs.Add(Pair);return Result;
}

TSharedRef<SWidget> SGameXXKDevWorkbench::Text(const FString& Value,int32 Size,bool bDisplay,FLinearColor Color)
{
	const auto Font=FGameXXKInRunUiStyle::Font(Size,bDisplay);
	if (Font.FontObject) Resources.AddUnique(const_cast<UObject*>(Font.FontObject.Get()));
	return SNew(STextBlock).Text(FText::FromString(Value)).Font(Font)
		.ColorAndOpacity(Color==FLinearColor::Black ? FGameXXKInRunUiStyle::Ink() : Color).AutoWrapText(true);
}
TSharedRef<SWidget> SGameXXKDevWorkbench::Button(const FString& Label,TFunction<void()> Action,bool bPrimary)
{
	return SNew(SButton).ButtonStyle(bPrimary?&ActionStyle:&QuietStyle)
		.HAlign(HAlign_Center).VAlign(VAlign_Center)
		.OnClicked_Lambda([Action=MoveTemp(Action)](){Action();return FReply::Handled();})
		[SNew(STextBlock).Text(FText::FromString(Label)).Font(FGameXXKInRunUiStyle::Font(16,true)).AutoWrapText(false)
			.ColorAndOpacity(bPrimary?FLinearColor(0.98f,0.94f,0.83f):FGameXXKInRunUiStyle::Ink())];
}
TSharedRef<SWidget> SGameXXKDevWorkbench::Field(const FString& Key,const FString& Label,const FString& Default,float Width)
{
	FString Initial=Fields.Contains(Key)&&Fields[Key] ? Fields[Key]->GetText().ToString():Default;
	TSharedPtr<SEditableTextBox> Edit;
	auto W=SNew(SVerticalBox)
		+SVerticalBox::Slot().AutoHeight().Padding(0,0,0,5)[Text(Label,13,false,FGameXXKInRunUiStyle::MutedInk())]
		+SVerticalBox::Slot().AutoHeight()[SNew(SBox).WidthOverride(Width).HeightOverride(36)
			[SAssignNew(Edit,SEditableTextBox).Style(&EditStyle).Font(FGameXXKInRunUiStyle::Font(17)).Text(FText::FromString(Initial)).SelectAllTextWhenFocused(true)]];
	Fields.Add(Key,Edit);return W;
}
TSharedRef<SWidget> SGameXXKDevWorkbench::Choice(const FString& Key,const FString& Label,const TArray<TPair<FString,FString>>& Options,const FString& Default)
{
	if (!Choices.Contains(Key)) Choices.Add(Key,Default);
	TSharedPtr<TArray<TSharedPtr<FString>>> Values=MakeShared<TArray<TSharedPtr<FString>>>();TSharedPtr<FString> Selected;
	for (const auto& Pair:Options) { auto V=MakeShared<FString>(Pair.Key);Values->Add(V);if (Pair.Key==Choices[Key]) Selected=V; }
	if (!Selected && !Values->IsEmpty()) { Selected=(*Values)[0];Choices[Key]=*Selected; }
	auto Name=[this,Options,Key](const FString& Id)
	{
		const TArray<TSharedPtr<FJsonValue>>* Rows=nullptr;
		if(Key==TEXT("character") && InspectorState && InspectorState->TryGetArrayField(TEXT("characters"),Rows))
			for(const auto& Row:*Rows){auto O=Row->AsObject();if(Str(O,TEXT("id"))==Id)return Str(O,TEXT("name"))+FString::Printf(TEXT(" · %d级"),Num(O,TEXT("level")));}
		for(const auto& P:Options)if(P.Key==Id)return P.Value;return Id;
	};
	return SNew(SVerticalBox)
		+SVerticalBox::Slot().AutoHeight().Padding(0,0,0,5)[Text(Label,13,false,FGameXXKInRunUiStyle::MutedInk())]
		+SVerticalBox::Slot().AutoHeight()[SNew(SBox).HeightOverride(36)
		[SNew(SComboBox<TSharedPtr<FString>>).ComboBoxStyle(&ComboStyle).ItemStyle(&ComboRowStyle).ScrollBarStyle(&ScrollStyle).MaxListHeight(540)
		.OptionsSource(Values.Get()).InitiallySelectedItem(Selected)
		.OnGenerateWidget_Lambda([this,Name,Values](TSharedPtr<FString> V){return SNew(SBox).MinDesiredWidth(230).Padding(FMargin(5,2))[SNew(STextBlock).Text(FText::FromString(Name(*V))).Font(FGameXXKInRunUiStyle::Font(14)).ColorAndOpacity(FGameXXKInRunUiStyle::Ink()).AutoWrapText(false)];})
		.OnSelectionChanged_Lambda([this,Key,Values](TSharedPtr<FString> V,ESelectInfo::Type How)
		{
			if(!V)return;Choices[Key]=*V;
			if(Key==TEXT("character")&&How!=ESelectInfo::Direct){CharacterId=*V;Object A=Obj();A->SetStringField(TEXT("character"),CharacterId);A->SetBoolField(TEXT("compact"),true);InspectorState=Data(Tools.Get(),TEXT("inspect"),A);}
		})
		[SNew(STextBlock).Font(FGameXXKInRunUiStyle::Font(14)).ColorAndOpacity(FGameXXKInRunUiStyle::Ink())
		.Text_Lambda([this,Name,Key](){return FText::FromString(Name(Choices.FindRef(Key)));})]]];
}
FString SGameXXKDevWorkbench::Value(const FString& Key) const
{ const auto* F=Fields.Find(Key);return F&&F->IsValid()?(*F)->GetText().ToString():Choices.FindRef(Key); }
TSharedRef<SWidget> SGameXXKDevWorkbench::Section(const FString& Title,TSharedRef<SWidget> Content)
{
	return SNew(SVerticalBox)
		+SVerticalBox::Slot().AutoHeight().Padding(0,0,0,10)[Text(Title,20,true)]
		+SVerticalBox::Slot().FillHeight(1)[Content];
}
void SGameXXKDevWorkbench::Construct(const FArguments& Args)
{
	Tools=Args._Tools;
	PaperBrush=GameXXKDesktopPaperStyle::MakeBrush(FVector2D(1180,700));
	ShadeBrush=*FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));ShadeBrush.TintColor=FSlateColor(FLinearColor(0.13f,0.105f,0.07f,0.055f));
	DividerBrush=ShadeBrush;DividerBrush.TintColor=FSlateColor(FLinearColor(0.13f,0.105f,0.07f,0.18f));
	ActionStyle=FGameXXKInRunUiStyle::Action(FVector2D(180,42),true);
	QuietStyle=FGameXXKInRunUiStyle::Action(FVector2D(150,38),false);
	ComboStyle=FCoreStyle::Get().GetWidgetStyle<FComboBoxStyle>(TEXT("ComboBox"));
	const FSlateBrush MenuPaper=QuietStyle.Normal;
	if(MenuPaper.GetResourceObject())Resources.AddUnique(MenuPaper.GetResourceObject());
	ComboStyle.ComboButtonStyle.SetButtonStyle(QuietStyle).SetMenuBorderBrush(MenuPaper).SetMenuBorderPadding(FMargin(8));
	ComboStyle.MenuRowPadding=FMargin(2);
	ComboStyle.ComboButtonStyle.DownArrowImage.TintColor=FSlateColor(FGameXXKInRunUiStyle::Ink());
	ComboRowStyle=FCoreStyle::Get().GetWidgetStyle<FTableRowStyle>(TEXT("TableView.Row"));
	FSlateBrush Selected=ShadeBrush;Selected.TintColor=FSlateColor(FLinearColor(0.12f,0.30f,0.23f,0.16f));
	ComboRowStyle.SetSelectorFocusedBrush(FSlateNoResource()).SetActiveBrush(Selected).SetActiveHoveredBrush(Selected).SetInactiveBrush(Selected).SetInactiveHoveredBrush(Selected);
	ComboRowStyle.SetEvenRowBackgroundBrush(FSlateNoResource()).SetOddRowBackgroundBrush(FSlateNoResource());
	for (const FSlateBrush* B:{&PaperBrush,&ActionStyle.Normal,&ActionStyle.Hovered,&ActionStyle.Pressed,&ActionStyle.Disabled,&QuietStyle.Normal,&QuietStyle.Hovered,&QuietStyle.Pressed,&QuietStyle.Disabled})
		if (B->GetResourceObject())Resources.AddUnique(B->GetResourceObject());
	EditStyle=FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>(TEXT("NormalEditableTextBox"));
	EditStyle.SetBackgroundImageNormal(ShadeBrush).SetBackgroundImageHovered(DividerBrush).SetBackgroundImageFocused(DividerBrush)
		.SetForegroundColor(FGameXXKInRunUiStyle::Ink()).SetPadding(FMargin(9,4));
	ScrollStyle=FCoreStyle::Get().GetWidgetStyle<FScrollBarStyle>(TEXT("ScrollBar"));
	FSlateBrush Thumb;Thumb.SetResourceObject(LoadObject<UTexture2D>(nullptr,TEXT("/Game/GameXXK/UI/MasterV2/Approved/inventory_scrollbar_Button.inventory_scrollbar_Button")));
	Thumb.ImageSize=FVector2D(12,58);Thumb.DrawAs=ESlateBrushDrawType::Image;
	if(Thumb.GetResourceObject())Resources.AddUnique(Thumb.GetResourceObject());
	ScrollStyle.SetNormalThumbImage(Thumb).SetHoveredThumbImage(Thumb).SetDraggedThumbImage(Thumb);
	ScrollStyle.SetVerticalBackgroundImage(FSlateNoResource()).SetHorizontalBackgroundImage(FSlateNoResource());
	TSharedRef<SVerticalBox> Navigation=SNew(SVerticalBox);
	for (const auto& Tab:TArray<TPair<FString,FString>>{{TEXT("home"),TEXT("一键整备")},{TEXT("items"),TEXT("万物匣")},{TEXT("equipment"),TEXT("配装台")},{TEXT("battle"),TEXT("试武场")},{TEXT("records"),TEXT("试验记录")}})
	{
		Navigation->AddSlot().AutoHeight().Padding(0,0,0,12)
		[Button(Tab.Value,[this,Id=Tab.Key](){ActiveTab=Id;Rebuild();})];
	}
	Navigation->AddSlot().FillHeight(1)[SNew(SSpacer)];
	Navigation->AddSlot().AutoHeight().Padding(0,0,0,14)[Text(TEXT("试验进度独立\n可随时恢复"),13,false,FGameXXKInRunUiStyle::MutedInk())];
	Navigation->AddSlot().AutoHeight()[Button(TEXT("收起 · F10"),[this](){if(Tools.IsValid())Tools->ClosePanel();})];
	ChildSlot
	[SNew(SOverlay)
		+SOverlay::Slot().Padding(5).HAlign(HAlign_Fill).VAlign(VAlign_Fill)
		[SNew(SScaleBox).Stretch(EStretch::UserSpecified).HAlign(HAlign_Fill).VAlign(VAlign_Fill)
			.UserSpecifiedScale_Lambda([this](){const auto Size=GetCachedGeometry().GetLocalSize();return GameXXKDesktopPaperStyle::GetBackpackScale(Size.X>0?FVector2D(Size):FVector2D(1180,700));})
			[SNew(SBorder).BorderImage(&PaperBrush).Padding(0).Visibility(EVisibility::HitTestInvisible)]]
		+SOverlay::Slot().Padding(FMargin(30,26))
	[SNew(SVerticalBox)
		+SVerticalBox::Slot().AutoHeight().Padding(0,0,0,12)
		[SNew(SHorizontalBox)
			+SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Text(TEXT("试炼手札"),30,true)]
			+SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(16,0)[Text(TEXT("DEV"),15,false,FGameXXKInRunUiStyle::Vermilion())]
			+SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SNew(STextBlock).Font(FGameXXKInRunUiStyle::Font(14)).ColorAndOpacity(FGameXXKInRunUiStyle::Jade())
				.Text_Lambda([this](){return FText::FromString(Tools.IsValid()?Tools->GetStatusText():FString());})]
			+SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Text(TEXT("F10 开关  ·  Esc 关闭"),13,false,FGameXXKInRunUiStyle::MutedInk())]]
		+SVerticalBox::Slot().AutoHeight().Padding(0,0,0,15)[SNew(SBox).HeightOverride(1)[SNew(SBorder).BorderImage(&DividerBrush)]]
		+SVerticalBox::Slot().FillHeight(1)
		[SNew(SHorizontalBox)
			+SHorizontalBox::Slot().AutoWidth().Padding(0,0,18,0)[SNew(SBox).WidthOverride(150)[Navigation]]
			+SHorizontalBox::Slot().FillWidth(1).Padding(0,0,20,0)[SAssignNew(Body,SVerticalBox)]
			+SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(252)
				[SNew(SBorder).BorderImage(&ShadeBrush).Padding(16)[SAssignNew(Inspector,SVerticalBox)]]]]
		+SVerticalBox::Slot().AutoHeight().Padding(0,15,0,0)
		[SNew(SBorder).BorderImage(&ShadeBrush).Padding(FMargin(12,9))
		[SNew(STextBlock).Font(FGameXXKInRunUiStyle::Font(14)).ColorAndOpacity_Lambda([this](){return Tools.IsValid()&&Tools->WasLastCommandSuccessful()?FGameXXKInRunUiStyle::Jade():FGameXXKInRunUiStyle::Vermilion();}).AutoWrapText(true)
			.Text_Lambda([this](){return FText::FromString(Tools.IsValid()?Tools->GetLastMessage():FString());})]]]];
	Rebuild();RebuildInspector();
}
void SGameXXKDevWorkbench::Tick(const FGeometry& Geometry,double CurrentTime,float DeltaTime)
{
	SCompoundWidget::Tick(Geometry,CurrentTime,DeltaTime);
	ScrollPairs.RemoveAll([](const auto& P){return !P->Box.IsValid() || !P->Bar.IsValid();});
	for(const auto& Pair:ScrollPairs)
	{
		auto Box=Pair->Box.Pin();auto Bar=Pair->Bar.Pin();const float End=Box->GetScrollOffsetOfEnd();
		Pair->Fraction=FMath::Clamp(64.0f/FMath::Max(72.0f,static_cast<float>(Bar->GetCachedGeometry().GetLocalSize().Y)),0.05f,0.9f);
		Bar->SetVisibility(End>0.5f?EVisibility::Visible:EVisibility::Hidden);
		Bar->SetState(End>0?FMath::Clamp(Box->GetScrollOffset()/End,0.0f,1.0f)*(1-Pair->Fraction):0,Pair->Fraction);
	}
	if (CurrentTime-LastInspectionTime<0.5 || !Tools.IsValid()) return;
	LastInspectionTime=CurrentTime;Object A=Obj();A->SetStringField(TEXT("character"),CharacterId);A->SetBoolField(TEXT("compact"),true);InspectorState=Data(Tools.Get(),TEXT("inspect"),A);
}
void SGameXXKDevWorkbench::Run(const FString& Command,const Object& Args)
{
	if(!Tools.IsValid())return;Object Request=Obj();Request->SetStringField(TEXT("command"),Command);if(Args)Request->SetObjectField(TEXT("args"),Args);
	LastResponse=Tools->ExecuteJson(Json(Request));RebuildInspector();
}
void SGameXXKDevWorkbench::Rebuild()
{
	if(!Body)return;Body->ClearChildren();
	Body->AddSlot().FillHeight(1)[ActiveTab==TEXT("home")?BuildHome():ActiveTab==TEXT("items")?BuildItems():ActiveTab==TEXT("equipment")?BuildEquipment():ActiveTab==TEXT("battle")?BuildBattle():BuildRecords()];
}
void SGameXXKDevWorkbench::RebuildInspector()
{
	if(!Inspector)return;Inspector->ClearChildren();Object A=Obj();A->SetStringField(TEXT("character"),CharacterId);
	A->SetBoolField(TEXT("compact"),true);Object D=Data(Tools.Get(),TEXT("inspect"),A);InspectorState=D;TArray<TPair<FString,FString>> Characters;
	const TArray<TSharedPtr<FJsonValue>>* Rows=nullptr;
	if(D->TryGetArrayField(TEXT("characters"),Rows))for(const auto& V:*Rows){auto R=V->AsObject();Characters.Add({Str(R,TEXT("id")),Str(R,TEXT("name"))+FString::Printf(TEXT(" · %d级"),Num(R,TEXT("level")))});}
	if(!Characters.IsEmpty() && !Characters.ContainsByPredicate([this](const auto& P){return P.Key==CharacterId;}))
	{ CharacterId=Characters[0].Key;Choices.Add(TEXT("character"),CharacterId);A->SetStringField(TEXT("character"),CharacterId);D=Data(Tools.Get(),TEXT("inspect"),A);InspectorState=D; }
	Inspector->AddSlot().AutoHeight().Padding(0,0,0,14)[Choice(TEXT("character"),TEXT("当前操作角色"),Characters,CharacterId)];
	Inspector->AddSlot().AutoHeight().Padding(0,0,0,6)[Text(TEXT("角色面板"),19,true)];
	Inspector->AddSlot().AutoHeight().Padding(0,0,0,12)[SNew(STextBlock).Font(FGameXXKInRunUiStyle::Font(16,false,true)).ColorAndOpacity(FGameXXKInRunUiStyle::Ink())
		.Text_Lambda([this](){const Object* O=nullptr;if(!InspectorState||!InspectorState->TryGetObjectField(TEXT("final"),O))return FText::GetEmpty();return FText::FromString(FString::Printf(TEXT("血 %d   内 %d\n攻 %d  防 %d  速 %d"),Num(*O,TEXT("health")),Num(*O,TEXT("mana")),Num(*O,TEXT("attack")),Num(*O,TEXT("defense")),Num(*O,TEXT("speed"))));})];
	Inspector->AddSlot().AutoHeight().Padding(0,0,0,13)[SNew(STextBlock).Font(FGameXXKInRunUiStyle::Font(13)).ColorAndOpacity(FGameXXKInRunUiStyle::Jade()).AutoWrapText(true)
		.Text_Lambda([this](){const Object* O=nullptr;if(!InspectorState||!InspectorState->TryGetObjectField(TEXT("live"),O)||Str(*O,TEXT("context")).IsEmpty())return FText::GetEmpty();return FText::FromString(Str(*O,TEXT("context"))+FString::Printf(TEXT("\n现有气血 %d  ·  护甲 %d"),Num(*O,TEXT("health")),Num(*O,TEXT("armor"))));})];
	Inspector->AddSlot().AutoHeight().Padding(0,0,0,8)[Text(TEXT("来源拆分"),16,true)];
	const TCHAR* Keys[]={TEXT("bare"),TEXT("equipment"),TEXT("gems"),TEXT("modifiers")};
	const TCHAR* Labels[]={TEXT("基础 / 天赋"),TEXT("装备强化"),TEXT("镶嵌宝石"),TEXT("词缀修正")};
	TSharedRef<SVerticalBox> StatsList=SNew(SVerticalBox);
	for(int32 I=0;I<4;++I)
	{
		const Object* S=nullptr;if(!D->TryGetObjectField(Keys[I],S))continue;
		StatsList->AddSlot().AutoHeight().Padding(0,0,8,10)
		[SNew(SVerticalBox)
			+SVerticalBox::Slot().AutoHeight()[Text(Labels[I],12,false,FGameXXKInRunUiStyle::MutedInk())]
			+SVerticalBox::Slot().AutoHeight().Padding(0,3,0,0)[SNew(STextBlock).Font(FGameXXKInRunUiStyle::Font(13)).ColorAndOpacity(FGameXXKInRunUiStyle::Ink())
			.Text_Lambda([this,Key=FString(Keys[I])](){const Object* O=nullptr;if(!InspectorState||!InspectorState->TryGetObjectField(Key,O))return FText::GetEmpty();return FText::FromString(FString::Printf(TEXT("血 %d   内 %d\n攻 %d   防 %d   速 %d"),Num(*O,TEXT("health")),Num(*O,TEXT("mana")),Num(*O,TEXT("attack")),Num(*O,TEXT("defense")),Num(*O,TEXT("speed"))));})]];
	}
	Inspector->AddSlot().FillHeight(1).Padding(0,0,0,10)[Scroller(StatsList)];
	Inspector->AddSlot().AutoHeight().Padding(0,0,0,9)[Button(TEXT("全队回满"),[this](){Run(TEXT("heal"));})];
	Inspector->AddSlot().AutoHeight()[Button(TEXT("返回原进度"),[this](){Run(TEXT("session.restore"));},true)];
}

TSharedRef<SWidget> SGameXXKDevWorkbench::BuildHome()
{
	auto Content=SNew(SVerticalBox);

	Content->AddSlot().AutoHeight().Padding(0,0,0,22)[Text(TEXT("为主角、所有已拥有伙伴与六位NPC，配置同等级的完整装备。"),16,false,FGameXXKInRunUiStyle::MutedInk())];
	Content->AddSlot().AutoHeight().Padding(0,0,0,22)
	[SNew(SHorizontalBox)
		+SHorizontalBox::Slot().FillWidth(1).Padding(0,0,18,0)[Field(TEXT("recommend_level"),TEXT("统一角色 / 装备等级"),TEXT("100"),220)]
		+SHorizontalBox::Slot().FillWidth(1)[Choice(TEXT("recommend_hero_set"),TEXT("主角套装"),Sets(),TEXT("PoJun"))]];
	Content->AddSlot().AutoHeight().Padding(0,0,0,18)
	[SNew(SBorder).BorderImage(&ShadeBrush).Padding(18)
	[SNew(SVerticalBox)
		+SVerticalBox::Slot().AutoHeight().Padding(0,0,0,10)[Text(TEXT("至宝 · 强化 +10 · 满孔同品质宝石"),20,true)]
		+SVerticalBox::Slot().AutoHeight().Padding(0,0,0,10)[Text(TEXT("刀客 破军　守卫 玄甲　药师 青囊\n弓手 追风　法师 追风　阵师 山河"),16)]
		+SVerticalBox::Slot().AutoHeight()[Text(TEXT("中位词缀 · 4攻 / 4防 / 4生命\n蚀骨六件放入背包，随时自行替换。"),14,false,FGameXXKInRunUiStyle::MutedInk())]]];
	auto Layout=SNew(SVerticalBox);Layout->AddSlot().FillHeight(1)[Scroller(Content)];
	Layout->AddSlot().AutoHeight().Padding(0,12,0,12)[Button(TEXT("一键推荐配装"),[this](){Object A=Obj();A->SetNumberField(TEXT("level"),FCString::Atoi(*Value(TEXT("recommend_level"))));A->SetStringField(TEXT("hero_set"),Value(TEXT("recommend_hero_set")));Run(TEXT("equipment.recommend_all"),A);},true)];
	Layout->AddSlot().AutoHeight()[Text(TEXT("NPC按第一关联职业配套；重复整备复用匹配装备。完成后全队回满，可到试武场选择关卡。"),13,false,FGameXXKInRunUiStyle::MutedInk())];
	return Section(TEXT("一键整备 · 百战起手"),Layout);
}

TSharedRef<SWidget> SGameXXKDevWorkbench::BuildItems()
{
	if(!Choices.Contains(TEXT("item_kind")))Choices.Add(TEXT("item_kind"),TEXT("item"));
	Object A=Obj();A->SetStringField(TEXT("category"),Choices[TEXT("item_kind")]);A->SetStringField(TEXT("query"),SearchText);
	Object D=Data(Tools.Get(),TEXT("catalog"),A);const TArray<TSharedPtr<FJsonValue>>* Rows=nullptr;D->TryGetArrayField(TEXT("entries"),Rows);
	TSharedRef<SVerticalBox> List=SNew(SVerticalBox);Object Selected;
	if(Rows && !Rows->IsEmpty())
	{
		for(const auto& V:*Rows)if(Str(V->AsObject(),TEXT("id"))==ActiveItem)Selected=V->AsObject();
		if(!Selected){Selected=(*Rows)[0]->AsObject();ActiveItem=Str(Selected,TEXT("id"));}
		for(const auto& V:*Rows)
		{
			auto R=V->AsObject();FString Id=Str(R,TEXT("id"));
			List->AddSlot().AutoHeight().Padding(0,0,8,6)[Button(Str(R,TEXT("name")),[this,Id](){ActiveItem=Id;Rebuild();},Id==ActiveItem)];
		}
	}
	else List->AddSlot().AutoHeight()[Text(TEXT("没有找到匹配物品"),16)];
	TSharedRef<SVerticalBox> Detail=SNew(SVerticalBox);
	Detail->AddSlot().AutoHeight().Padding(0,0,0,12)[Text(Selected?Str(Selected,TEXT("name")):TEXT("选择物品"),27,true)];
	Detail->AddSlot().AutoHeight().Padding(0,0,0,16)[Text(Selected?Str(Selected,TEXT("description")):FString(),15,false,FGameXXKInRunUiStyle::MutedInk())];
	Detail->AddSlot().AutoHeight().Padding(0,0,0,12)[Field(TEXT("quantity"),TEXT("获得数量"),TEXT("1"),180)];
	if(Choices[TEXT("item_kind")]==TEXT("equipment"))
	{
		Detail->AddSlot().AutoHeight().Padding(0,0,0,12)[Field(TEXT("item_level"),TEXT("装备等级 1～100"),TEXT("100"),180)];
		Detail->AddSlot().AutoHeight().Padding(0,0,0,12)[Choice(TEXT("item_quality"),TEXT("装备品质"),{{TEXT("1"),TEXT("普通")},{TEXT("2"),TEXT("稀有")},{TEXT("3"),TEXT("珍稀")},{TEXT("4"),TEXT("传奇")},{TEXT("5"),TEXT("不朽")},{TEXT("6"),TEXT("至宝")},{TEXT("7"),TEXT("超凡")},{TEXT("8"),TEXT("天界")},{TEXT("9"),TEXT("登神")},{TEXT("10"),TEXT("宇宙")}},TEXT("6"))];
	}
	Detail->AddSlot().FillHeight(1)[SNew(SSpacer)];
	Detail->AddSlot().AutoHeight().Padding(0,0,0,10)[Button(TEXT("获得物品"),[this]()
	{
		Object P=Obj();P->SetStringField(TEXT("id"),ActiveItem);P->SetStringField(TEXT("character"),CharacterId);P->SetNumberField(TEXT("quantity"),FCString::Atoi(*Value(TEXT("quantity"))));
		if(Choices[TEXT("item_kind")]==TEXT("equipment")){P->SetNumberField(TEXT("level"),FCString::Atoi(*Value(TEXT("item_level"))));P->SetNumberField(TEXT("quality"),FCString::Atoi(*Value(TEXT("item_quality"))));Run(TEXT("equipment.create"),P);}
		else Run(TEXT("item.give"),P);
	},true)];
	Detail->AddSlot().AutoHeight()[Text(Selected?Str(Selected,TEXT("id")):FString(),11,false,FGameXXKInRunUiStyle::MutedInk())];
	TSharedRef<SVerticalBox> Content=SNew(SVerticalBox);
	TSharedRef<SHorizontalBox> Categories=SNew(SHorizontalBox);
	for(const auto& C:TArray<TPair<FString,FString>>{{TEXT("item"),TEXT("道具与材料")},{TEXT("equipment"),TEXT("装备")},{TEXT("relic"),TEXT("局内遗物")}})
		Categories->AddSlot().FillWidth(1).Padding(0,0,8,0)[Button(C.Value,[this,Id=C.Key](){Choices[TEXT("item_kind")]=Id;ActiveItem.Empty();Rebuild();},Choices[TEXT("item_kind")]==C.Key)];
	Content->AddSlot().AutoHeight().Padding(0,0,0,12)[Categories];
	Content->AddSlot().AutoHeight().Padding(0,0,0,15)
	[SAssignNew(Search,SEditableTextBox).Style(&EditStyle).Font(FGameXXKInRunUiStyle::Font(16)).HintText(FText::FromString(TEXT("输入中文名称检索 · 回车搜索")))
		.Text(FText::FromString(SearchText)).OnTextCommitted_Lambda([this](const FText& V,ETextCommit::Type How){if(How==ETextCommit::OnEnter){SearchText=V.ToString();Rebuild();}})];
	Content->AddSlot().FillHeight(1)
	[SNew(SHorizontalBox)
		+SHorizontalBox::Slot().FillWidth(1).Padding(0,0,17,0)[Scroller(List)]
		+SHorizontalBox::Slot().FillWidth(1)[SNew(SBorder).BorderImage(&ShadeBrush).Padding(16)[Detail]]];
	return Section(TEXT("万物匣 · 即取即用"),Content);
}

TSharedRef<SWidget> SGameXXKDevWorkbench::BuildEquipment()
{
	auto Content=SNew(SVerticalBox);
	Content->AddSlot().AutoHeight().Padding(0,0,0,15)[Text(TEXT("按当前角色生成完整配装，装备等级、词缀与镶嵌都会进入真实战斗属性。"),15,false,FGameXXKInRunUiStyle::MutedInk())];
	Content->AddSlot().AutoHeight().Padding(0,0,0,15)
	[SNew(SHorizontalBox)
		+SHorizontalBox::Slot().FillWidth(1).Padding(0,0,10,0)[Field(TEXT("level"),TEXT("角色 / 装备等级"),TEXT("100"),145)]
		+SHorizontalBox::Slot().FillWidth(1).Padding(0,0,10,0)[Field(TEXT("enhance"),TEXT("强化 +0～10"),TEXT("10"),125)]
		+SHorizontalBox::Slot().FillWidth(1)[Choice(TEXT("quality"),TEXT("装备品质"),{{TEXT("1"),TEXT("普通")},{TEXT("2"),TEXT("稀有")},{TEXT("3"),TEXT("珍稀")},{TEXT("4"),TEXT("传奇")},{TEXT("5"),TEXT("不朽")},{TEXT("6"),TEXT("至宝")},{TEXT("7"),TEXT("超凡")},{TEXT("8"),TEXT("天界")},{TEXT("9"),TEXT("登神")},{TEXT("10"),TEXT("宇宙")}},TEXT("6"))]];
	Content->AddSlot().AutoHeight().Padding(0,0,0,15)
	[SNew(SHorizontalBox)
		+SHorizontalBox::Slot().FillWidth(1).Padding(0,0,10,0)[Choice(TEXT("layout"),TEXT("套装结构"),{{TEXT("6"),TEXT("六件同套")},{TEXT("42"),TEXT("四件 + 两件")},{TEXT("222"),TEXT("两件 + 两件 + 两件")}},TEXT("6"))]
		+SHorizontalBox::Slot().FillWidth(1)[Choice(TEXT("affix"),TEXT("词缀取值"),{{TEXT("random"),TEXT("实际随机")},{TEXT("low"),TEXT("合法最低值")},{TEXT("mid"),TEXT("区间中位值")},{TEXT("high"),TEXT("合法最高值")}},TEXT("mid"))]];
	Content->AddSlot().AutoHeight().Padding(0,0,0,15)
	[SNew(SHorizontalBox)
		+SHorizontalBox::Slot().FillWidth(1).Padding(0,0,10,0)[Choice(TEXT("set_a"),TEXT("第一套"),Sets(),TEXT("XuanJia"))]
		+SHorizontalBox::Slot().FillWidth(1).Padding(0,0,10,0)[Choice(TEXT("set_b"),TEXT("第二套"),Sets(),TEXT("PoJun"))]
		+SHorizontalBox::Slot().FillWidth(1)[Choice(TEXT("set_c"),TEXT("第三套"),Sets(),TEXT("ShanHe"))]];
	Content->AddSlot().AutoHeight().Padding(0,0,0,15)[Choice(TEXT("gem"),TEXT("按品质对应孔位镶嵌"),{{TEXT("none"),TEXT("保留空孔")},{TEXT("balanced"),TEXT("攻击 / 防御 / 生命均衡")},{TEXT("Attack"),TEXT("全部攻击")},{TEXT("Defense"),TEXT("全部防御")},{TEXT("MaxHealth"),TEXT("全部生命")}},TEXT("balanced"))];
	Content->AddSlot().AutoHeight().Padding(0,0,0,12)
	[SNew(SHorizontalBox)
		+SHorizontalBox::Slot().FillWidth(1).Padding(0,0,10,0)[Button(TEXT("设置角色等级"),[this](){Object A=Obj();A->SetStringField(TEXT("character"),CharacterId);A->SetNumberField(TEXT("level"),FCString::Atoi(*Value(TEXT("level"))));Run(TEXT("character.level"),A);})]
		+SHorizontalBox::Slot().FillWidth(1)[Button(TEXT("编入出战队伍"),[this](){Object A=Obj();A->SetStringField(TEXT("character"),CharacterId);Run(TEXT("party.select"),A);})]];
	Content->AddSlot().FillHeight(1)[SNew(SSpacer)];
	auto Create=[this](bool Equip)
	{
		Object A=Obj();A->SetStringField(TEXT("character"),CharacterId);A->SetNumberField(TEXT("level"),FCString::Atoi(*Value(TEXT("level"))));A->SetNumberField(TEXT("enhance"),FCString::Atoi(*Value(TEXT("enhance"))));A->SetNumberField(TEXT("quality"),FCString::Atoi(*Value(TEXT("quality"))));A->SetStringField(TEXT("affix"),Value(TEXT("affix")));A->SetStringField(TEXT("gem"),Value(TEXT("gem")));A->SetBoolField(TEXT("equip"),Equip);
		TArray<TSharedPtr<FJsonValue>> Six;
		for(int32 I=0;I<6;++I){FString Key=Value(TEXT("layout"))==TEXT("6")?TEXT("set_a"):Value(TEXT("layout"))==TEXT("42")?(I<4?TEXT("set_a"):TEXT("set_b")):(I<2?TEXT("set_a"):I<4?TEXT("set_b"):TEXT("set_c"));Six.Add(MakeShared<FJsonValueString>(Value(Key)));}
		A->SetArrayField(TEXT("sets"),Six);A->SetNumberField(TEXT("character_level"),FCString::Atoi(*Value(TEXT("level"))));Run(TEXT("equipment.loadout"),A);
	};
	Content->AddSlot().AutoHeight()
	[SNew(SHorizontalBox)
		+SHorizontalBox::Slot().FillWidth(1).Padding(0,0,10,0)[Button(TEXT("生成至仓库"),[Create](){Create(false);})]
		+SHorizontalBox::Slot().FillWidth(1)[Button(TEXT("生成并穿戴"),[Create](){Create(true);},true)]];
	return Section(TEXT("配装台 · 六槽成套"),Scroller(Content));
}

TSharedRef<SWidget> SGameXXKDevWorkbench::BuildBattle()
{
	Object Q=Obj();Q->SetStringField(TEXT("category"),TEXT("stage"));auto D=Data(Tools.Get(),TEXT("catalog"),Q);
	TArray<TPair<FString,FString>> Stages;const TArray<TSharedPtr<FJsonValue>>* Rows=nullptr;
	if(D->TryGetArrayField(TEXT("entries"),Rows))for(const auto& R:*Rows){auto V=R->AsObject();Stages.Add({Str(V,TEXT("id")),Str(V,TEXT("name"))+TEXT(" · ")+Str(V,TEXT("description"))});}
	auto Content=SNew(SVerticalBox);
	if(!Choices.Contains(TEXT("battle_mode")))Choices.Add(TEXT("battle_mode"),TEXT("manual"));
	const bool bManual=Choices[TEXT("battle_mode")]==TEXT("manual");
	Content->AddSlot().AutoHeight().Padding(0,0,0,13)[SNew(SHorizontalBox)
		+SHorizontalBox::Slot().FillWidth(1).Padding(0,0,10,0)[Button(TEXT("手动试武"),[this](){Choices[TEXT("battle_mode")]=TEXT("manual");Rebuild();},bManual)]
		+SHorizontalBox::Slot().FillWidth(1)[Button(TEXT("批量推演"),[this](){Choices[TEXT("battle_mode")]=TEXT("batch");Rebuild();},!bManual)]];
	Content->AddSlot().AutoHeight().Padding(0,0,0,13)[Text(TEXT("相同种子保留相同开场。怪物、阶段和伤害读取局内实际规则。"),13,false,FGameXXKInRunUiStyle::MutedInk())];
	Content->AddSlot().AutoHeight().Padding(0,0,0,16)[Choice(TEXT("stage"),TEXT("挑战关卡"),Stages,TEXT("Training.Normal.1-1"))];
	Content->AddSlot().AutoHeight().Padding(0,0,0,16)
	[SNew(SHorizontalBox)
		+SHorizontalBox::Slot().FillWidth(1).Padding(0,0,12,0)[Choice(TEXT("encounter"),TEXT("关内场次"),{{TEXT("1"),TEXT("第1场 · 普通")},{TEXT("2"),TEXT("第2场 · 普通")},{TEXT("3"),TEXT("第3场 · 普通")},{TEXT("4"),TEXT("第4场 · 普通")},{TEXT("5"),TEXT("第5场 · 精英")},{TEXT("6"),TEXT("第6场 · 精英")},{TEXT("7"),TEXT("第7场 · Boss")}},TEXT("7"))]
		+SHorizontalBox::Slot().FillWidth(1)[Field(TEXT("seed"),TEXT("固定随机种子"),TEXT("20260906"),225)]];
	auto Scenario=[this](){Object A=Obj();A->SetStringField(TEXT("stage"),Value(TEXT("stage")));A->SetNumberField(TEXT("encounter"),FCString::Atoi(*Value(TEXT("encounter"))));A->SetNumberField(TEXT("seed"),FCString::Atoi(*Value(TEXT("seed"))));return A;};
	if(bManual)
	{
	Content->AddSlot().AutoHeight().Padding(0,0,0,13)[Button(TEXT("进入这场战斗"),[this,Scenario](){Run(TEXT("battle.start"),Scenario());},true)];
	Content->AddSlot().AutoHeight().Padding(0,0,0,22)
	[SNew(SHorizontalBox)
		+SHorizontalBox::Slot().FillWidth(1).Padding(0,0,9,0)[Button(TEXT("同种子重开"),[this](){Run(TEXT("battle.restart"));})]
		+SHorizontalBox::Slot().FillWidth(1).Padding(0,0,9,0)[Button(TEXT("返回战前"),[this](){Run(TEXT("battle.return"));})]
		+SHorizontalBox::Slot().FillWidth(1)[Button(TEXT("自动开关"),[this](){Object A=Obj();auto D=Data(Tools.Get(),TEXT("inspect"));bool Enabled=false;D->TryGetBoolField(TEXT("auto_play"),Enabled);A->SetBoolField(TEXT("enabled"),!Enabled);Run(TEXT("battle.auto"),A);})]];
	Content->AddSlot().FillHeight(1)[SNew(SSpacer)];
	Content->AddSlot().AutoHeight()[Text(TEXT("进入后按F10收起手札，即可在战斗界面正常出牌。"),13,false,FGameXXKInRunUiStyle::MutedInk())];
	}
	else
	{
	Content->AddSlot().AutoHeight().Padding(0,0,0,15)
	[SNew(SHorizontalBox)
		+SHorizontalBox::Slot().FillWidth(1).Padding(0,0,12,0)[Field(TEXT("runs"),TEXT("样本数 1～1000"),TEXT("100"),190)]
		+SHorizontalBox::Slot().FillWidth(1)[Field(TEXT("rounds"),TEXT("单场回合上限"),TEXT("100"),190)]];
	Content->AddSlot().AutoHeight().Padding(0,0,0,12)
	[SNew(SHorizontalBox)
		+SHorizontalBox::Slot().FillWidth(1).Padding(0,0,10,0)[Button(TEXT("开始批测"),[this,Scenario](){auto A=Scenario();A->SetNumberField(TEXT("runs"),FCString::Atoi(*Value(TEXT("runs"))));A->SetNumberField(TEXT("max_rounds"),FCString::Atoi(*Value(TEXT("rounds"))));Run(TEXT("simulate.start"),A);},true)]
		+SHorizontalBox::Slot().FillWidth(1)[Button(TEXT("取消批测"),[this](){Run(TEXT("simulate.cancel"));})]];
	Content->AddSlot().AutoHeight().Padding(0,0,0,12)[Button(TEXT("只续算当前战斗"),[this](){Object A=Obj();A->SetBoolField(TEXT("continue_current"),true);A->SetNumberField(TEXT("runs"),1);Run(TEXT("simulate.start"),A);})];
	Content->AddSlot().FillHeight(1)[SNew(SSpacer)];
	Content->AddSlot().AutoHeight()[Text(TEXT("推演使用独立状态副本。结果与逐步记录保存在试验记录中。"),13,false,FGameXXKInRunUiStyle::MutedInk())];
	}
	return Section(TEXT("试武场 · 同局复验"),Content);
}

TSharedRef<SWidget> SGameXXKDevWorkbench::BuildRecords()
{
	auto Content=SNew(SVerticalBox);
	Content->AddSlot().AutoHeight().Padding(0,0,0,12)[Text(TEXT("保存整队配装、牌组和当前战斗，方便下次继续或交给AI复现。"),15,false,FGameXXKInRunUiStyle::MutedInk())];
	Content->AddSlot().AutoHeight().Padding(0,0,0,12)[Field(TEXT("snapshot"),TEXT("快照名称"),TEXT("我的配装"),480)];
	Content->AddSlot().AutoHeight().Padding(0,0,0,18)
	[SNew(SHorizontalBox)
		+SHorizontalBox::Slot().FillWidth(1).Padding(0,0,10,0)[Button(TEXT("保存当前快照"),[this](){Object A=Obj();A->SetStringField(TEXT("name"),Value(TEXT("snapshot")));Run(TEXT("snapshot.save"),A);Rebuild();},true)]
		+SHorizontalBox::Slot().FillWidth(1)[Button(TEXT("载入这个快照"),[this](){Object A=Obj();A->SetStringField(TEXT("name"),Value(TEXT("snapshot")));Run(TEXT("snapshot.load"),A);})]];
	Object Saved=Data(Tools.Get(),TEXT("snapshot.list"));const TArray<TSharedPtr<FJsonValue>>* Names=nullptr;FString NamesText;
	if(Saved->TryGetArrayField(TEXT("names"),Names))for(const auto& V:*Names){if(!NamesText.IsEmpty())NamesText+=TEXT(" · ");NamesText+=V->AsString();}
	Content->AddSlot().AutoHeight().Padding(0,0,0,18)[Text(NamesText.IsEmpty()?TEXT("还没有保存的快照"):TEXT("已有快照：")+NamesText,13,false,FGameXXKInRunUiStyle::MutedInk())];
	Object Status=Data(Tools.Get(),TEXT("simulate.status"));const Object* Report=nullptr;
	FString Summary=TEXT("完成批测后，这里显示胜负、回合与剩余生命。");
	if(Status->TryGetObjectField(TEXT("report"),Report))
		Summary=FString::Printf(TEXT("已完成 %d / %d 场\n胜 %d  ·  负 %d  ·  僵局 %d  ·  错误 %d\n平均回合 %.1f   平均剩余生命 %.1f"),Num(*Report,TEXT("completed")),Num(*Report,TEXT("total")),Num(*Report,TEXT("wins")),Num(*Report,TEXT("defeats")),Num(*Report,TEXT("stalemates")),Num(*Report,TEXT("errors")),(*Report)->GetNumberField(TEXT("mean_rounds")),(*Report)->GetNumberField(TEXT("mean_remaining_health")));
	Content->AddSlot().AutoHeight().Padding(0,0,0,13)[SNew(SBorder).BorderImage(&ShadeBrush).Padding(13)[Text(Summary,16)]];
	Content->AddSlot().AutoHeight().Padding(0,0,0,18)
	[SNew(SHorizontalBox)
		+SHorizontalBox::Slot().FillWidth(1).Padding(0,0,10,0)[Button(TEXT("刷新结果"),[this](){Rebuild();})]
		+SHorizontalBox::Slot().FillWidth(1)[Button(TEXT("打开记录目录"),[this](){if(Tools.IsValid())FPlatformProcess::ExploreFolder(*Tools->GetStorageDirectory());})]];
	Content->AddSlot().FillHeight(1)[SNew(SSpacer)];
	Content->AddSlot().AutoHeight().Padding(0,0,0,8)[Button(Choices.FindRef(TEXT("advanced"))==TEXT("open")?TEXT("收起高级指令"):TEXT("AI / 高级指令"),[this](){Choices.Add(TEXT("advanced"),Choices.FindRef(TEXT("advanced"))==TEXT("open")?TEXT("closed"):TEXT("open"));Rebuild();})];
	if(Choices.FindRef(TEXT("advanced"))==TEXT("open"))
	{
	Content->AddSlot().AutoHeight().Padding(0,0,0,9)
	[SAssignNew(CommandInput,SEditableTextBox).Style(&EditStyle).Font(FGameXXKInRunUiStyle::Font(13)).Text(FText::FromString(TEXT("{\"command\":\"help\"}")))];
	Content->AddSlot().AutoHeight()
	[SNew(SHorizontalBox)
		+SHorizontalBox::Slot().FillWidth(1).Padding(0,0,10,0)[Button(TEXT("执行指令"),[this](){if(Tools.IsValid()&&CommandInput){LastResponse=Tools->ExecuteJson(CommandInput->GetText().ToString());RebuildInspector();}})]
		+SHorizontalBox::Slot().FillWidth(1)[Button(TEXT("复制结果"),[this](){FPlatformApplicationMisc::ClipboardCopy(*LastResponse);})]];
	}
	return Section(TEXT("试验记录 · 留样复现"),Scroller(Content));
}
