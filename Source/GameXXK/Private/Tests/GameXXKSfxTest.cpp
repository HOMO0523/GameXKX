#include "Audio/GameXXKSfxPolicy.h"
#include "Audio/GameXXKSfx.h"
#include "Misc/AutomationTest.h"
#include "Sound/SoundWave.h"
#include "UI/GameXXKInRunUiStyle.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKSfxImpactTest, "GameXXK.Audio.Policy.ImpactRouting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKSfxImpactTest::RunTest(const FString& Parameters)
{
	FGameXXKBattlePresentationEvent E;
	TestEqual(TEXT("no real impact stays silent"), FGameXXKSfxPolicy::ResolveImpact(E), EGameXXKSfxCue::None);
	E.HealthDamage = 5; E.TargetHealthBefore = 100; E.TargetHealthAfter = 95;
	TestEqual(TEXT("ordinary physical hit"), FGameXXKSfxPolicy::ResolveImpact(E), EGameXXKSfxCue::HitLight);
	E.HealthDamage = 40; E.TargetHealthAfter = 60;
	TestEqual(TEXT("existing heavy feedback tier"), FGameXXKSfxPolicy::ResolveImpact(E), EGameXXKSfxCue::HitHeavy);
	E.HealthDamage = 0; E.ArmorAbsorbed = 40; E.TargetHealthAfter = 100;
	TestEqual(TEXT("armor absorption sounds like a block"), FGameXXKSfxPolicy::ResolveImpact(E), EGameXXKSfxCue::Block);
	E.HealthDamage = 5;
	TestEqual(TEXT("partial armor absorption still has shield feedback"), FGameXXKSfxPolicy::ResolveImpact(E), EGameXXKSfxCue::Block);
	E.ArmorAbsorbed = 0; E.DamageCause = EGameXXKCardDamageCause::Block;
	TestEqual(TEXT("actual block reaction has shield feedback"), FGameXXKSfxPolicy::ResolveImpact(E), EGameXXKSfxCue::Block);
	E.DamageCause = EGameXXKCardDamageCause::Invalid; E.HealthDamage = 0; E.ArmorAbsorbed = 40;
	E.Element = EGameXXKCardDamageElement::Frost;
	TestEqual(TEXT("fully absorbed spell keeps element identity"), FGameXXKSfxPolicy::ResolveImpact(E), EGameXXKSfxCue::Frost);
	E.Element = EGameXXKCardDamageElement::Fire;
	TestEqual(TEXT("fire packet"), FGameXXKSfxPolicy::ResolveImpact(E), EGameXXKSfxCue::Fire);
	E.Element = EGameXXKCardDamageElement::Lightning;
	TestEqual(TEXT("lightning packet"), FGameXXKSfxPolicy::ResolveImpact(E), EGameXXKSfxCue::Lightning);
	E.bAvoided = true;
	TestEqual(TEXT("avoided element never sounds like a hit"), FGameXXKSfxPolicy::ResolveImpact(E), EGameXXKSfxCue::None);
	E.bAvoided = false; E.ArmorAbsorbed = 0; E.HealthDamage = 0;
	TestEqual(TEXT("zero-damage spell has no successful hit"), FGameXXKSfxPolicy::ResolveImpact(E), EGameXXKSfxCue::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKSfxHealingTest, "GameXXK.Audio.Policy.EffectiveHealing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKSfxHealingTest::RunTest(const FString& Parameters)
{
	FGameXXKCardBattleRuntime Before;
	FGameXXKCardCombatUnit Unit;
	Unit.UnitId = TEXT("HealingTest"); Unit.HP = 50; Unit.MaxHP = 100;
	Unit.SettlementHealingReceived = 8;
	Before.Units.Add(Unit);
	FGameXXKCardBattleRuntime After = Before;
	TestFalse(TEXT("refresh and ineffective heal stay silent"), FGameXXKSfxPolicy::HasActualHealing(Before, After));
	After.Units[0].SettlementHealingReceived += 10;
	TestTrue(TEXT("actual heal remains audible when damage offsets the net HP gain"), FGameXXKSfxPolicy::HasActualHealing(Before, After));
	After = Before; After.Units[0].MaxHP += 10; After.Units[0].HP += 10;
	TestFalse(TEXT("changing max health is not proof of healing"), FGameXXKSfxPolicy::HasActualHealing(Before, After));
	After = Before; After.Units[0].SettlementHealingReceived = 0;
	TestFalse(TEXT("new session/reset does not replay old healing"), FGameXXKSfxPolicy::HasActualHealing(Before, After));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKSfxGateTest, "GameXXK.Audio.Policy.MuteAndBurst",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKSfxGateTest::RunTest(const FString& Parameters)
{
	FGameXXKSfxPolicy Gate;
	TestFalse(TEXT("muted request is suppressed"), Gate.Accept(EGameXXKSfxCue::Button, 1.0, true));
	TestTrue(TEXT("muted request does not consume the first audible click"), Gate.Accept(EGameXXKSfxCue::Button, 1.0, false));
	TestFalse(TEXT("rapid duplicate click is limited"), Gate.Accept(EGameXXKSfxCue::Button, 1.01, false));
	TestTrue(TEXT("next deliberate click works"), Gate.Accept(EGameXXKSfxCue::Button, 1.2, false));
	TestTrue(TEXT("unrelated impact is not blocked by UI gate"), Gate.Accept(EGameXXKSfxCue::HitLight, 1.2, false));
	TestFalse(TEXT("invalid timestamp is rejected"), Gate.Accept(EGameXXKSfxCue::HitHeavy, std::numeric_limits<double>::quiet_NaN(), false));
	TestFalse(TEXT("no cue is never played"), Gate.Accept(EGameXXKSfxCue::None, 1.3, false));
	Gate.Reset();
	TestTrue(TEXT("new session may start at a new time origin"), Gate.Accept(EGameXXKSfxCue::Button, 0.0, false));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKSfxAssetsTest, "GameXXK.Audio.Assets.EssentialLibrary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKSfxAssetsTest::RunTest(const FString& Parameters)
{
	int32 WaveCount = 0;
	for (int32 Index = static_cast<int32>(EGameXXKSfxCue::HitLight); Index <= static_cast<int32>(EGameXXKSfxCue::Tool); ++Index)
	{
		const auto Cue = static_cast<EGameXXKSfxCue>(Index);
		TestEqual(TEXT("cue names round-trip"), FGameXXKSfx::FindCue(FGameXXKSfx::CueName(Cue)), Cue);
		for (int32 Variant = 0; Variant < FGameXXKSfx::VariantCount(Cue); ++Variant)
		{
			const FString Path = FGameXXKSfx::AssetPath(Cue, Variant);
			const auto* Sound = LoadObject<USoundWave>(nullptr, *Path, nullptr, LOAD_NoWarn);
			TestNotNull(*Path, Sound);
			if (Sound)
			{
				TestTrue(TEXT("short nonempty effect"), Sound->Duration > 0.005f && Sound->Duration < 1.6f);
				TestEqual(TEXT("positionable mono source"), Sound->NumChannels, 1);
				TestFalse(TEXT("effects do not loop"), Sound->bLooping);
			}
			++WaveCount;
		}
	}
	TestEqual(TEXT("approved 14 families contain 22 waves"), WaveCount, 22);
	TestEqual(TEXT("unrecognized cue is silent"), FGameXXKSfx::FindCue(TEXT("NotACue")), EGameXXKSfxCue::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKSfxButtonTest, "GameXXK.Audio.Assets.SharedButtonSound",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKSfxButtonTest::RunTest(const FString& Parameters)
{
	const FButtonStyle Action = FGameXXKInRunUiStyle::Action(FVector2D(180, 48));
	const FButtonStyle Choice = FGameXXKInRunUiStyle::Choice(FVector2D(180, 240), false);
	TestNotNull(TEXT("action has a click sound"), Action.PressedSlateSound.GetResourceObject());
	TestTrue(TEXT("different common buttons share exactly one sound"), Action.PressedSlateSound.GetResourceObject() == Choice.PressedSlateSound.GetResourceObject());
	TestNull(TEXT("hover remains quiet"), Action.HoveredSlateSound.GetResourceObject());
	TestFalse(TEXT("null world cannot emit an audio component"), UGameXXKSfxLibrary::PlayNamed(nullptr, TEXT("Button")));
	return true;
}
#endif
