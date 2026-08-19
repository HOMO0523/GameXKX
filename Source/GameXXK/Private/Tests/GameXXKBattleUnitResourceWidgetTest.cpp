#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"
#include "UI/GameXXKBattleUnitResourceWidget.h"

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
	TestTrue(TEXT("resource mask clips the complete Full texture horizontally instead of only its center color channel"),
		UGameXXKBattleUnitResourceWidget::UsesWholeFullBarMaskForTest());

	TestTrue(TEXT("resource widget prepares a native runtime tree for screen-space embedding"), ResourceWidget->PrepareForScreenSpaceEmbedding());
	TestTrue(TEXT("resource widget retains its native runtime tree"), ResourceWidget->HasRuntimeWidgetTreeForTest());

	const auto ReadStringGetter = [this, ResourceWidget](const FName FunctionName)
	{
		UFunction* const Function = ResourceWidget->FindFunction(FunctionName);
		TestNotNull(*FString::Printf(TEXT("%s is exposed for PSD resource-style inspection"), *FunctionName.ToString()), Function);
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
		TEXT("health track uses the derived PSD track texture"),
		ReadStringGetter(TEXT("GetHealthTrackResourcePathForTest")),
		FString(TEXT("/Game/GameXXK/UI/Battle/ResourceBars/T_BattlePsd_HealthTrack.T_BattlePsd_HealthTrack")));
	TestEqual(
		TEXT("health full layer uses the complete PSD red bar before right-side masking"),
		ReadStringGetter(TEXT("GetHealthFullResourcePathForTest")),
		FString(TEXT("/Game/GameXXK/UI/Battle/ResourceBars/T_BattlePsd_HealthFull.T_BattlePsd_HealthFull")));
	TestEqual(
		TEXT("mana track uses the derived PSD track texture"),
		ReadStringGetter(TEXT("GetManaTrackResourcePathForTest")),
		FString(TEXT("/Game/GameXXK/UI/Battle/ResourceBars/T_BattlePsd_ManaTrack.T_BattlePsd_ManaTrack")));
	TestEqual(
		TEXT("mana full layer uses the complete PSD green bar before right-side masking"),
		ReadStringGetter(TEXT("GetManaFullResourcePathForTest")),
		FString(TEXT("/Game/GameXXK/UI/Battle/ResourceBars/T_BattlePsd_ManaFull.T_BattlePsd_ManaFull")));
	TestEqual(
		TEXT("resource rows use the dedicated percentage-mask UI material"),
		ReadStringGetter(TEXT("GetResourceMaskMaterialPathForTest")),
		FString(TEXT("/Game/GameXXK/UI/Battle/ResourceBars/M_BattlePsdResourceMask.M_BattlePsdResourceMask")));
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

#endif
