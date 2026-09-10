#include "UI/GameXXKLocalizationLibrary.h"
#include "UI/GameXXKLocalization.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Serialization/JsonSerializer.h"
#include "Widgets/SWindow.h"
#include "GenericPlatform/GenericWindow.h"

FString UGameXXKLocalizationLibrary::GetLanguage(){return GameXXKLocalization::GetLanguage();}
bool UGameXXKLocalizationLibrary::SetLanguage(const FString& Language,bool bPersist){return GameXXKLocalization::SetLanguage(Language,bPersist);}
bool UGameXXKLocalizationLibrary::ReloadTextCatalog(){return GameXXKLocalization::ReloadTextCatalog();}
TArray<FString> UGameXXKLocalizationLibrary::GetMissingSources(){return GameXXKLocalization::GetMissingSources();}

FString UGameXXKLocalizationLibrary::AuditTextLayout(UUserWidget* Root)
{
    auto Report = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> Rows;
    TSet<UWidget*> Visited;
    int32 Unarranged = 0;
    if (!Root || !FSlateApplication::IsInitialized()) return TEXT("{\"error\":\"No live UI root\"}");
    const auto Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
    TFunction<void(UWidget*, float)> Walk = [&](UWidget* Widget, float ParentWidth)
    {
        if (!Widget || Visited.Contains(Widget) || !Widget->IsVisible()) return;
        Visited.Add(Widget);
        const FGeometry& Geometry = Widget->GetCachedGeometry();
        const FVector2D Local = Geometry.GetLocalSize();
        const FVector2D Start = Geometry.LocalToAbsolute(FVector2D::ZeroVector);
        const FVector2D End = Geometry.LocalToAbsolute(Local);
        const float Width = FMath::Abs(End.X - Start.X);
        const float Scale = Local.X > 0 ? Width / Local.X : 0;
        const float Available = Width > 0 ? (ParentWidth > 0 ? FMath::Min(Width, ParentWidth) : Width) : ParentWidth;
        if (auto* Text = Cast<UTextBlock>(Widget); Text && !Text->GetText().IsEmpty())
        {
            if (Width <= 0 || Local.Y <= 0) ++Unarranged;
            else
            {
                const FString Value = Text->GetText().ToString();
                const FSlateFontInfo Font = Text->GetFont();
                const FVector2D Natural = Measure->Measure(Text->GetText(), Font);
                const bool Wraps = Text->GetAutoWrapText() || Text->GetWrapTextAt() > 0;
                bool Chinese = false;
                for (TCHAR Char : Value) Chinese |= Char >= 0x3400 && Char <= 0x9fff;
                auto Row = MakeShared<FJsonObject>();
                Row->SetStringField(TEXT("widget"), Widget->GetName());
                Row->SetStringField(TEXT("path"), Widget->GetPathName());
                Row->SetStringField(TEXT("text"), Value);
                Row->SetStringField(TEXT("font"), GetPathNameSafe(Font.FontObject));
                Row->SetNumberField(TEXT("fontSize"), Font.Size);
                Row->SetNumberField(TEXT("x"), Start.X); Row->SetNumberField(TEXT("y"), Start.Y);
                Row->SetNumberField(TEXT("width"), Width);
                Row->SetNumberField(TEXT("availableWidth"), Available);
                Row->SetNumberField(TEXT("naturalWidth"), Natural.X * Scale);
                Row->SetBoolField(TEXT("wraps"), Wraps);
                Row->SetBoolField(TEXT("chinese"), Chinese);
                Row->SetBoolField(TEXT("potentialOverflow"), !Wraps && Natural.X * Scale > Available + 1.0f);
                Rows.Add(MakeShared<FJsonValueObject>(Row));
            }
        }
        if (auto* User = Cast<UUserWidget>(Widget); User && User->WidgetTree)
            Walk(User->WidgetTree->RootWidget, Available);
        if (auto* Panel = Cast<UPanelWidget>(Widget))
            for (UWidget* Child : Panel->GetAllChildren()) Walk(Child, Available);
    };
    Walk(Root, 0);
    Report->SetStringField(TEXT("language"), GameXXKLocalization::GetLanguage());
    Report->SetNumberField(TEXT("applicationScale"),FSlateApplication::Get().GetApplicationScale());
    TArray<TSharedPtr<FJsonValue>> WindowScales;
    for(const auto& Window:FSlateApplication::Get().GetTopLevelWindows())
    {
        auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("title"),Window->GetTitle().ToString());
        if(const auto Native=Window->GetNativeWindow())Row->SetNumberField(TEXT("nativeSlateDpi"),Native->GetDPIScaleFactor());
        WindowScales.Add(MakeShared<FJsonValueObject>(Row));
    }
    Report->SetArrayField(TEXT("windowScales"),WindowScales);
    Report->SetNumberField(TEXT("unarrangedTextCount"), Unarranged);
    Report->SetArrayField(TEXT("texts"), Rows);
    FString Output;
    FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Output));
    return Output;
}
