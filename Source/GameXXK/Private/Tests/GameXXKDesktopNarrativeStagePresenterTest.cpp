#include "Misc/AutomationTest.h"

#include "Components/TextBlock.h"
#include "UI/GameXXKDesktopNarrativeStagePresenterWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKDesktopNarrativeStagePresenterTestPrivate
{
	UGameXXKDesktopNarrativeStagePresenterWidget* MakePresenter(
		const EGameXXKDesktopNarrativeSlot Slot)
	{
		UGameXXKDesktopNarrativeStagePresenterWidget* const Presenter =
			NewObject<UGameXXKDesktopNarrativeStagePresenterWidget>();
		Presenter->ConfigureSlot(Slot);
		Presenter->TakeWidget();
		return Presenter;
	}

	bool IsVisible(const UWidget* Widget)
	{
		return Widget
			&& Widget->GetVisibility() != ESlateVisibility::Collapsed
			&& Widget->GetVisibility() != ESlateVisibility::Hidden;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopNarrativeStagePresenterOwnedNodesTest,
	"GameXXK.DesktopNarrative.Presenter.OwnedSemanticNodes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopNarrativeStagePresenterOwnedNodesTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKDesktopNarrativeStagePresenterTestPrivate;
	UGameXXKDesktopNarrativeStagePresenterWidget* const Role =
		MakePresenter(EGameXXKDesktopNarrativeSlot::Left);
	TestTrue(TEXT("role presenter builds real owned nodes"), Role->IsPresentationReady());
	TestNotNull(TEXT("role resource owns a real content node"), Role->GetRoleContentNode());
	TestNotNull(TEXT("role action owns a real content node"), Role->GetRoleActionNode());
	TestTrue(TEXT("role content node is parented"),
		Role->GetRoleContentNode() && Role->GetRoleContentNode()->GetParent());
	Role->PresentRole(
		TEXT("Hero"),
		TEXT("Character.Hero"),
		EGameXXKDesktopNarrativeFacing::Left,
		EGameXXKDesktopNarrativeRoleActionState::Pending,
		TEXT("Action.Hero.Bow"));
	TestTrue(TEXT("shown role mutates the real role node"),
		IsVisible(Role->GetRoleContentNode()));
	TestEqual(TEXT("role node content reflects semantic resource"),
		Role->GetRoleContentNode()->GetText(), FText::FromName(TEXT("Character.Hero")));
	TestEqual(TEXT("role presenter retains semantic role id"),
		Role->GetPresentedRoleId(), FName(TEXT("Hero")));
	TestTrue(TEXT("left facing mutates the real presenter transform"),
		Role->GetRoleContentNode()->GetRenderTransform().Scale.X < 0.0f);
	TestTrue(TEXT("pending action mutates the real action node"),
		IsVisible(Role->GetRoleActionNode()));
	TestEqual(TEXT("action node content reflects semantic action"),
		Role->GetRoleActionNode()->GetText(), FText::FromName(TEXT("Action.Hero.Bow")));
	TestEqual(TEXT("role presenter exposes pending state"),
		Role->GetRoleActionState(), EGameXXKDesktopNarrativeRoleActionState::Pending);
	Role->PresentRole(
		TEXT("Hero"),
		TEXT("Character.Hero"),
		static_cast<EGameXXKDesktopNarrativeFacing>(255),
		EGameXXKDesktopNarrativeRoleActionState::Pending,
		TEXT("Action.Hero.Bow"));
	TestEqual(TEXT("forged facing is rejected instead of coerced to Right"),
		Role->GetRoleFacing(), EGameXXKDesktopNarrativeFacing::Left);
	TestTrue(TEXT("forged facing leaves the real transform unchanged"),
		Role->GetRoleContentNode()->GetRenderTransform().Scale.X < 0.0f);
	Role->PresentRole(
		TEXT("Hero"),
		TEXT("Character.Hero"),
		EGameXXKDesktopNarrativeFacing::Right,
		EGameXXKDesktopNarrativeRoleActionState::Idle,
		NAME_None);
	TestTrue(TEXT("right facing restores positive real transform"),
		Role->GetRoleContentNode()->GetRenderTransform().Scale.X > 0.0f);
	TestFalse(TEXT("idle action clears the real action node"),
		IsVisible(Role->GetRoleActionNode()));

	UGameXXKDesktopNarrativeStagePresenterWidget* const Prop =
		MakePresenter(EGameXXKDesktopNarrativeSlot::Prop);
	Prop->PresentProp(TEXT("Prop.MapScroll"));
	TestTrue(TEXT("prop uses a dedicated real node"), IsVisible(Prop->GetPropContentNode()));
	TestEqual(TEXT("prop node content reflects semantic resource"),
		Prop->GetPropContentNode()->GetText(), FText::FromName(TEXT("Prop.MapScroll")));

	UGameXXKDesktopNarrativeStagePresenterWidget* const Vfx =
		MakePresenter(EGameXXKDesktopNarrativeSlot::Vfx);
	Vfx->PresentVfx(TEXT("Vfx.Wind"));
	Vfx->PresentFlash(TEXT("Vfx.Flash.White"));
	Vfx->PresentToast(TEXT("Toast.Ready"));
	TestTrue(TEXT("VFX uses a dedicated real node"), IsVisible(Vfx->GetVfxContentNode()));
	TestTrue(TEXT("flash uses a dedicated real node"), IsVisible(Vfx->GetFlashContentNode()));
	TestTrue(TEXT("toast uses a dedicated real node"), IsVisible(Vfx->GetToastContentNode()));
	TestEqual(TEXT("VFX node content reflects semantic resource"),
		Vfx->GetVfxContentNode()->GetText(), FText::FromName(TEXT("Vfx.Wind")));
	TestEqual(TEXT("flash node content reflects semantic resource"),
		Vfx->GetFlashContentNode()->GetText(), FText::FromName(TEXT("Vfx.Flash.White")));
	TestEqual(TEXT("toast node content reflects semantic resource"),
		Vfx->GetToastContentNode()->GetText(), FText::FromName(TEXT("Toast.Ready")));

	Role->ResetPresentation();
	Prop->ResetPresentation();
	Vfx->ResetPresentation();
	TestFalse(TEXT("role reset clears real nodes"), Role->HasAnyPresentation());
	TestFalse(TEXT("prop reset clears real nodes"), Prop->HasAnyPresentation());
	TestFalse(TEXT("VFX reset clears every dedicated node"), Vfx->HasAnyPresentation());
	return true;
}

#endif
