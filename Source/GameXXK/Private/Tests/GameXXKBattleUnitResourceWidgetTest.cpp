#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"
#include "UI/GameXXKBattleUnitResourceWidget.h"
#include "UI/GameXXKInkResourceBarStyle.h"
#include "UI/GameXXKInRunUiStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Materials/MaterialInstanceDynamic.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBattleUnitResourceWidgetTest,
	"GameXXK.UI.Battle.UnitResourceWidget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBattleUnitResourceWidgetTest::RunTest(const FString& Parameters)
{
	UGameXXKBattleUnitResourceWidget* ResourceWidget = NewObject<UGameXXKBattleUnitResourceWidget>();
	TestNotNull(TEXT("resource widget is created"), ResourceWidget);
	if (!ResourceWidget)
	{
		return false;
	}

	const auto VerifyReflectedRenderedValueGetter = [this, ResourceWidget](const FName FunctionName)
	{
		const UFunction* const Function = ResourceWidget->FindFunction(FunctionName);
		TestNotNull(*FString::Printf(TEXT("%s is exposed for real-PIE inspection"), *FunctionName.ToString()), Function);
		if (Function)
		{
		TestTrue(*FString::Printf(TEXT("%s is BlueprintPure"), *FunctionName.ToString()), Function->HasAnyFunctionFlags(FUNC_BlueprintPure));
#if WITH_METADATA
		TestTrue(*FString::Printf(TEXT("%s is development-only"), *FunctionName.ToString()), Function->HasMetaData(TEXT("DevelopmentOnly")));
#else
		TestTrue(*FString::Printf(TEXT("%s development-only metadata is unavailable in this target"), *FunctionName.ToString()), true);
#endif
		}
	};
	VerifyReflectedRenderedValueGetter(GET_FUNCTION_NAME_CHECKED(UGameXXKBattleUnitResourceWidget, GetHealthDisplayTextForTest));
	VerifyReflectedRenderedValueGetter(GET_FUNCTION_NAME_CHECKED(UGameXXKBattleUnitResourceWidget, GetManaDisplayTextForTest));
	VerifyReflectedRenderedValueGetter(GET_FUNCTION_NAME_CHECKED(UGameXXKBattleUnitResourceWidget, GetHealthPercentForTest));
	VerifyReflectedRenderedValueGetter(GET_FUNCTION_NAME_CHECKED(UGameXXKBattleUnitResourceWidget, GetManaPercentForTest));
	VerifyReflectedRenderedValueGetter(GET_FUNCTION_NAME_CHECKED(UGameXXKBattleUnitResourceWidget, IsHealthFillLeftToRightForTest));
	VerifyReflectedRenderedValueGetter(GET_FUNCTION_NAME_CHECKED(UGameXXKBattleUnitResourceWidget, IsManaFillLeftToRightForTest));

	TestTrue(TEXT("resource widget prepares a native runtime tree for screen-space embedding"), ResourceWidget->PrepareForScreenSpaceEmbedding());
	TestTrue(TEXT("resource widget retains its native runtime tree"), ResourceWidget->HasRuntimeWidgetTreeForTest());

	const auto ReadStringGetter = [this, ResourceWidget](const FName FunctionName)
	{
		UFunction* const Function = ResourceWidget->FindFunction(FunctionName);
		TestNotNull(*FString::Printf(TEXT("%s is exposed for resource-style inspection"), *FunctionName.ToString()), Function);
		if (!Function)
		{
			return FString();
		}

		TArray<uint8> ParametersBuffer;
		ParametersBuffer.SetNumZeroed(Function->ParmsSize);
		ResourceWidget->ProcessEvent(Function, ParametersBuffer.GetData());
		const FStrProperty* const ReturnProperty = FindFProperty<FStrProperty>(Function, TEXT("ReturnValue"));
		TestNotNull(*FString::Printf(TEXT("%s returns a string resource path"), *FunctionName.ToString()), ReturnProperty);
		return ReturnProperty
			? ReturnProperty->GetPropertyValue_InContainer(ParametersBuffer.GetData())
			: FString();
	};

	TestEqual(
		TEXT("health track uses the common ink master"),
		ReadStringGetter(TEXT("GetHealthTrackResourcePathForTest")),
		FString(GameXXKInkResourceBarStyle::TexturePath));
	TestEqual(
		TEXT("health fill uses the same ink silhouette"),
		ReadStringGetter(TEXT("GetHealthFullResourcePathForTest")),
		FString(GameXXKInkResourceBarStyle::TexturePath));
	TestEqual(
		TEXT("mana track uses the common ink master"),
		ReadStringGetter(TEXT("GetManaTrackResourcePathForTest")),
		FString(GameXXKInkResourceBarStyle::TexturePath));
	TestEqual(
		TEXT("mana fill uses the same ink silhouette"),
		ReadStringGetter(TEXT("GetManaFullResourcePathForTest")),
		FString(GameXXKInkResourceBarStyle::TexturePath));
	TestEqual(
		TEXT("resource rows use the dedicated percentage-mask UI material"),
		ReadStringGetter(TEXT("GetResourceMaskMaterialPathForTest")),
		FString(GameXXKInkResourceBarStyle::MaterialPath));
	TestEqual(TEXT("resource widget wrapper itself is input-transparent"), ResourceWidget->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
	ResourceWidget->SetVisibility(ESlateVisibility::Visible);
	TestTrue(TEXT("resource widget can reprepare its native runtime tree"), ResourceWidget->PrepareForScreenSpaceEmbedding());
	TestEqual(TEXT("repreparing restores wrapper input transparency"), ResourceWidget->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);

	ResourceWidget->SetUnitVitals(TEXT("我 1P"), FText::FromString(TEXT("主角")), 0, 0, 0, 0, true);
	TestEqual(TEXT("zero health snapshot retains its supplied maximum label"), ResourceWidget->GetHealthDisplayTextForTest(), FString(TEXT("气血 0 / 0")));
	TestEqual(TEXT("zero mana snapshot retains its supplied maximum label"), ResourceWidget->GetManaDisplayTextForTest(), FString(TEXT("内力 0 / 0")));
	TestEqual(TEXT("zero health snapshot uses an empty safe fill"), ResourceWidget->GetHealthPercentForTest(), 0.0f);
	TestEqual(TEXT("zero mana snapshot uses an empty safe fill"), ResourceWidget->GetManaPercentForTest(), 0.0f);
	TestTrue(TEXT("zero mana snapshot remains visible when mana is enabled"), ResourceWidget->IsManaRowVisibleForTest());
	TestEqual(TEXT("zero mana snapshot uses self-hit-test-invisible row visibility"), ResourceWidget->GetManaRowVisibilityForTest(), ESlateVisibility::SelfHitTestInvisible);
	ResourceWidget->SetUnitVitals(TEXT("我 1P"), FText::FromString(TEXT("主角")), 0, 0, 1, 0, true);
	TestEqual(TEXT("zero maximum mana always uses an empty safe fill"), ResourceWidget->GetManaPercentForTest(), 0.0f);

	ResourceWidget->SetUnitVitals(TEXT("我 1P"), FText::FromString(TEXT("主角")), 72, 100, 18, 30, true);
	TestEqual(TEXT("hero health row uses the required readable label"), ResourceWidget->GetHealthDisplayTextForTest(), FString(TEXT("气血 72 / 100")));
	TestEqual(TEXT("hero mana row uses the required readable label"), ResourceWidget->GetManaDisplayTextForTest(), FString(TEXT("内力 18 / 30")));
	TestEqual(TEXT("hero health fill follows current and maximum health"), ResourceWidget->GetHealthPercentForTest(), 0.72f);
	TestEqual(TEXT("hero mana fill follows current and maximum mana"), ResourceWidget->GetManaPercentForTest(), 0.60f);
	UImage* HealthImage = Cast<UImage>(ResourceWidget->WidgetTree->FindWidget(TEXT("HealthBarLegacy")));
	UImage* ManaImage = Cast<UImage>(ResourceWidget->WidgetTree->FindWidget(TEXT("ManaBarLegacy")));
	UMaterialInstanceDynamic* HealthMaterial = HealthImage ? Cast<UMaterialInstanceDynamic>(HealthImage->GetBrush().GetResourceObject()) : nullptr;
	UMaterialInstanceDynamic* ManaMaterial = ManaImage ? Cast<UMaterialInstanceDynamic>(ManaImage->GetBrush().GetResourceObject()) : nullptr;
	TestNotNull(TEXT("the visible health image owns its ink material"), HealthMaterial);
	TestNotNull(TEXT("the visible mana image owns its ink material"), ManaMaterial);
	if (HealthMaterial && ManaMaterial)
	{
		TestEqual(TEXT("rendered health fill agrees with the number"), HealthMaterial->K2_GetScalarParameterValue(TEXT("FillPercent")), 0.72f);
		TestEqual(TEXT("rendered mana fill agrees with the number"), ManaMaterial->K2_GetScalarParameterValue(TEXT("FillPercent")), 0.60f);
		TestTrue(TEXT("mana and health use distinct colors on the same silhouette"),
			!HealthMaterial->K2_GetVectorParameterValue(TEXT("FillColor")).Equals(ManaMaterial->K2_GetVectorParameterValue(TEXT("FillColor"))));
		ResourceWidget->SetUnitVitals(TEXT("我 1P"), FText::FromString(TEXT("主角")), 0,100,30,30,true);
		TestEqual(TEXT("death empties the actual material immediately"), HealthMaterial->K2_GetScalarParameterValue(TEXT("FillPercent")), 0.0f);
		ResourceWidget->SetUnitVitals(TEXT("我 1P"), FText::FromString(TEXT("主角")), 100,100,0,30,true);
		TestEqual(TEXT("a reused health row returns to a full fill"), HealthMaterial->K2_GetScalarParameterValue(TEXT("FillPercent")), 1.0f);
		TestEqual(TEXT("empty mana does not retain the previous full fill"), ManaMaterial->K2_GetScalarParameterValue(TEXT("FillPercent")), 0.0f);
	}
	for (const FName TextName : {FName(TEXT("HealthText")), FName(TEXT("ManaText"))})
	{
		const UTextBlock* Text = Cast<UTextBlock>(ResourceWidget->WidgetTree->FindWidget(TextName));
		TestTrue(TEXT("resource numbers use JiangHu font"), Text && Text->GetFont().FontObject == FGameXXKInRunUiStyle::Font(18,true).FontObject);
	}
	TestTrue(TEXT("hero mana row is visible when mana is enabled"), ResourceWidget->IsManaRowVisibleForTest());
	TestTrue(TEXT("hero resource content never blocks screen-space targeting"), ResourceWidget->AreContentWidgetsHitTestTransparentForTest());
	TestTrue(TEXT("health fill consumes the PSD bar from left to right"), ResourceWidget->IsHealthFillLeftToRightForTest());
	TestTrue(TEXT("mana fill consumes the PSD bar from left to right"), ResourceWidget->IsManaFillLeftToRightForTest());

	ResourceWidget->SetUnitVitals(TEXT("敌 1P"), FText::FromString(TEXT("黑熊")), 240, 240, 99, 100, false);
	TestEqual(TEXT("enemy health row uses the required readable label"), ResourceWidget->GetHealthDisplayTextForTest(), FString(TEXT("气血 240 / 240")));
	TestEqual(TEXT("enemy mana row collapses despite a mana value"), ResourceWidget->GetManaRowVisibilityForTest(), ESlateVisibility::Collapsed);
	TestFalse(TEXT("enemy mana row is not visible despite a mana value"), ResourceWidget->IsManaRowVisibleForTest());
	TestTrue(TEXT("enemy resource content never blocks screen-space targeting"), ResourceWidget->AreContentWidgetsHitTestTransparentForTest());
	TestEqual(TEXT("resource root leaves screen-space hit testing to its owner"), UGameXXKBattleUnitResourceWidget::GetRootHitTestVisibilityForTest(), ESlateVisibility::SelfHitTestInvisible);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKTravelInkResourceFillTest,
	"GameXXK.UI.Battle.InkResourceBar.TravelFillSynchronization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTravelInkResourceFillTest::RunTest(const FString& Parameters)
{
	UProgressBar* Bar = NewObject<UProgressBar>();
	Bar->SetPercent(0.75f);
	GameXXKInkResourceBarStyle::Apply(Bar, FVector2D(124,18));
	UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(Bar->GetWidgetStyle().BackgroundImage.GetResourceObject());
	if (!TestNotNull(TEXT("travel bar renders its shared ink material"), Material)) return false;
	TestEqual(TEXT("initial visible fill follows the existing HP snapshot"), Material->K2_GetScalarParameterValue(TEXT("FillPercent")), 0.75f);
	for (const float Percent : {0.37f, 0.0f, 1.0f, -1.0f, 2.0f})
	{
		GameXXKInkResourceBarStyle::Update(Bar, Percent);
		TestEqual(TEXT("travel bar and shader use the same clamped live value"),
			Material->K2_GetScalarParameterValue(TEXT("FillPercent")), Bar->GetPercent());
		TestEqual(TEXT("fill stays inside the rail"), Bar->GetPercent(), FMath::Clamp(Percent,0.0f,1.0f));
	}
	return true;
}

#endif
