#include "GameXXKCardCatalog.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKHunterPartnerCatalogTest
{
	struct FExpectedEffect
	{
		EGameXXKCardEffectType Type;
		EGameXXKCardEffectTarget Target;
		int32 Magnitude;
		EGameXXKCardStatus Status = EGameXXKCardStatus::None;
		int32 HitCount = 1;
		int32 SecondaryMagnitude = 0;
		EGameXXKCardEffectConditionType ConditionType = EGameXXKCardEffectConditionType::None;
		EGameXXKCardStatus ConditionStatus = EGameXXKCardStatus::None;
		int32 MinimumStatusStacks = 0;
		float HealthPercentThreshold = 0.0f;
	};

	struct FExpectedCard
	{
		const TCHAR* CardId;
		const TCHAR* DisplayName;
		int32 EnergyCost;
		int32 ManaCost;
		EGameXXKCardTargetMode TargetMode;
		bool bCore;
		TArray<FExpectedEffect> Effects;
		EGameXXKHeavyArrowKind HeavyKind = EGameXXKHeavyArrowKind::None;
		int32 HeavyMagnitude = 0;
		int32 HeavyDrawPerCharge = 0;
		int32 HeavyManaPerCharge = 0;
	};

	const TArray<FExpectedCard>& ExpectedCards()
	{
		static const TArray<FExpectedCard> Cards = {
			{TEXT("Profession.Hunter.YingYan"), TEXT("锐意感知"), 1, 0, EGameXXKCardTargetMode::Self, true, {
				{EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 2},
				{EGameXXKCardEffectType::GainEnergy, EGameXXKCardEffectTarget::CardOwner, 1},
				{EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 2, EGameXXKCardStatus::Agility},
				{EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 3, EGameXXKCardStatus::Charge}}},
			{TEXT("Profession.Hunter.LianZhuJian"), TEXT("连珠箭"), 1, 3, EGameXXKCardTargetMode::SingleEnemy, true, {
				{EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 8, EGameXXKCardStatus::Bleed},
				{EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 6, EGameXXKCardStatus::Poison},
				{EGameXXKCardEffectType::DamagePercentAttack, EGameXXKCardEffectTarget::SelectedTarget, 50},
				{EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 1, EGameXXKCardStatus::Charge}},
				EGameXXKHeavyArrowKind::ExtraAttackPerCharge, 50},

			{TEXT("Profession.Hunter.XunXiJian"), TEXT("寻隙箭"), 1, 2, EGameXXKCardTargetMode::SingleEnemy, false, {
				{EGameXXKCardEffectType::DamagePercentAttack, EGameXXKCardEffectTarget::SelectedTarget, 80},
				{EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 2, EGameXXKCardStatus::Mark}},
				EGameXXKHeavyArrowKind::AddPrimaryAttackPercentPerCharge, 25},
			{TEXT("Profession.Hunter.FuBu"), TEXT("鹰眼"), 1, 0, EGameXXKCardTargetMode::Self, false, {
				{EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 3, EGameXXKCardStatus::Charge}}},
			{TEXT("Profession.Hunter.ZhuiLie"), TEXT("追猎"), 1, 2, EGameXXKCardTargetMode::SingleEnemy, false, {
				{EGameXXKCardEffectType::DamagePercentAttack, EGameXXKCardEffectTarget::SelectedTarget, 75},
				{EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 1, EGameXXKCardStatus::None, 1, 0, EGameXXKCardEffectConditionType::TargetHasStatus, EGameXXKCardStatus::Mark, 1},
				{EGameXXKCardEffectType::GainEnergy, EGameXXKCardEffectTarget::CardOwner, 1, EGameXXKCardStatus::None, 1, 0, EGameXXKCardEffectConditionType::TargetHasStatus, EGameXXKCardStatus::Mark, 1}},
				EGameXXKHeavyArrowKind::AddPrimaryAttackPercentPerCharge, 25, 0, 2},
			{TEXT("Profession.Hunter.LieWang"), TEXT("猎网"), 1, 3, EGameXXKCardTargetMode::SingleEnemy, false, {
				{EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 4, EGameXXKCardStatus::Mark},
				{EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 1, EGameXXKCardStatus::Vulnerability},
				{EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 1, EGameXXKCardStatus::Charge}}},
			{TEXT("Profession.Hunter.ChuanYang"), TEXT("穿杨"), 2, 6, EGameXXKCardTargetMode::SingleEnemy, false, {
				{EGameXXKCardEffectType::DamagePercentAttack, EGameXXKCardEffectTarget::SelectedTarget, 150},
				{EGameXXKCardEffectType::IgnoreDefense, EGameXXKCardEffectTarget::SelectedTarget, 6}},
				EGameXXKHeavyArrowKind::AddPrimaryAttackPercentPerCharge, 50},
			{TEXT("Profession.Hunter.FuZuShi"), TEXT("淬毒矢"), 1, 2, EGameXXKCardTargetMode::SingleEnemy, false, {
				{EGameXXKCardEffectType::DamagePercentAttack, EGameXXKCardEffectTarget::SelectedTarget, 70},
				{EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 6, EGameXXKCardStatus::Poison},
				{EGameXXKCardEffectType::ResolveToxicExplosion, EGameXXKCardEffectTarget::SelectedTarget, 1}},
				EGameXXKHeavyArrowKind::ToxicExplosionPerCharge, 1},
			{TEXT("Profession.Hunter.YinZong"), TEXT("隐踪"), 1, 0, EGameXXKCardTargetMode::Self, false, {
				{EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 2, EGameXXKCardStatus::Agility},
				{EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 1, EGameXXKCardStatus::Charge}}},
			{TEXT("Profession.Hunter.DuanMaiShi"), TEXT("断脉矢"), 1, 4, EGameXXKCardTargetMode::SingleEnemy, false, {
				{EGameXXKCardEffectType::DamagePercentAttack, EGameXXKCardEffectTarget::SelectedTarget, 100},
				{EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 8, EGameXXKCardStatus::Bleed}},
				EGameXXKHeavyArrowKind::AddPrimaryAttackPercentPerCharge, 30},
			{TEXT("Profession.Hunter.ShouHun"), TEXT("狩魂"), 2, 8, EGameXXKCardTargetMode::SingleEnemy, false, {
				{EGameXXKCardEffectType::DamagePercentAttack, EGameXXKCardEffectTarget::SelectedTarget, 150},
				{EGameXXKCardEffectType::BonusDamagePercent, EGameXXKCardEffectTarget::SelectedTarget, 20, EGameXXKCardStatus::None, 1, MAX_int32, EGameXXKCardEffectConditionType::TargetHasStatus, EGameXXKCardStatus::Mark, 1}},
				EGameXXKHeavyArrowKind::AddPrimaryAttackPercentPerCharge, 35},
			{TEXT("Profession.Hunter.BaiBuChuanYang"), TEXT("百步穿杨"), 3, 12, EGameXXKCardTargetMode::SingleEnemy, false, {
				{EGameXXKCardEffectType::DamagePercentAttack, EGameXXKCardEffectTarget::SelectedTarget, 210},
				{EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 2, EGameXXKCardStatus::None, 1, 0, EGameXXKCardEffectConditionType::TargetHasStatus, EGameXXKCardStatus::Mark, 5}},
				EGameXXKHeavyArrowKind::AddPrimaryAttackPercentPerCharge, 40, 1},
			{TEXT("Profession.Hunter.LueYingJian"), TEXT("掠影箭"), 1, 2, EGameXXKCardTargetMode::SingleEnemy, false, {
				{EGameXXKCardEffectType::DamagePercentAttack, EGameXXKCardEffectTarget::SelectedTarget, 65},
				{EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 1, EGameXXKCardStatus::None, 1, 0, EGameXXKCardEffectConditionType::TargetHasStatus, EGameXXKCardStatus::Mark, 1},
				{EGameXXKCardEffectType::GainEnergy, EGameXXKCardEffectTarget::CardOwner, 1, EGameXXKCardStatus::None, 1, 0, EGameXXKCardEffectConditionType::TargetHasStatus, EGameXXKCardStatus::Mark, 1}},
				EGameXXKHeavyArrowKind::AddPrimaryAttackPercentPerCharge, 20},
			{TEXT("Profession.Hunter.LieHunBiao"), TEXT("猎魂标"), 0, 4, EGameXXKCardTargetMode::SingleEnemy, false, {
				{EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 4, EGameXXKCardStatus::Mark},
				{EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 2, EGameXXKCardStatus::Charge}}},
			{TEXT("Profession.Hunter.PoJiaDing"), TEXT("破甲钉"), 1, 2, EGameXXKCardTargetMode::SingleEnemy, false, {
				{EGameXXKCardEffectType::DamagePercentAttack, EGameXXKCardEffectTarget::SelectedTarget, 75},
				{EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 2, EGameXXKCardStatus::Vulnerability},
				{EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 1, EGameXXKCardStatus::Poison}},
				EGameXXKHeavyArrowKind::AddPrimaryAttackPercentPerCharge, 25},
			{TEXT("Profession.Hunter.HuiHuanJian"), TEXT("回环箭"), 1, 2, EGameXXKCardTargetMode::SingleEnemy, false, {
				{EGameXXKCardEffectType::DamagePercentAttack, EGameXXKCardEffectTarget::SelectedTarget, 60},
				{EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 1},
				{EGameXXKCardEffectType::GainEnergy, EGameXXKCardEffectTarget::CardOwner, 1}},
				EGameXXKHeavyArrowKind::AddPrimaryAttackPercentPerCharge, 15, 1, 2},
			{TEXT("Profession.Hunter.FuYeXianJing"), TEXT("腐叶陷阱"), 1, 5, EGameXXKCardTargetMode::SingleEnemy, false, {
				{EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 8, EGameXXKCardStatus::Poison},
				{EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 2, EGameXXKCardStatus::Mark},
				{EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 2, EGameXXKCardStatus::Charge}}},
			{TEXT("Profession.Hunter.YingLuo"), TEXT("鹰落"), 3, 12, EGameXXKCardTargetMode::SingleEnemy, false, {
				{EGameXXKCardEffectType::DamagePercentAttack, EGameXXKCardEffectTarget::SelectedTarget, 200},
				{EGameXXKCardEffectType::BonusDamagePercent, EGameXXKCardEffectTarget::SelectedTarget, 100, EGameXXKCardStatus::None, 1, 0, EGameXXKCardEffectConditionType::TargetHealthBelowPercent, EGameXXKCardStatus::None, 0, 35.0f}},
				EGameXXKHeavyArrowKind::AddPrimaryAttackPercentPerCharge, 60}};
		return Cards;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKHunterPartnerAll18CatalogTest,
	"GameXXK.Data.PartnerCards.Hunter.All18Catalog",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHunterPartnerAll18CatalogTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHunterPartnerCatalogTest;
	TSet<FName> SeenIds;
	for (const FExpectedCard& Expected : ExpectedCards())
	{
		const FName CardId(Expected.CardId);
		TestFalse(FString::Printf(TEXT("Hunter card ID is unique: %s"), Expected.CardId), SeenIds.Contains(CardId));
		SeenIds.Add(CardId);
		const FGameXXKCardDefinition* Actual = FGameXXKCardCatalog::FindCardDefinition(CardId);
		if (!TestNotNull(FString::Printf(TEXT("Hunter card exists: %s"), Expected.CardId), Actual))
		{
			continue;
		}
		TestEqual(FString::Printf(TEXT("%s display name"), Expected.CardId), Actual->DisplayName.ToString(), FString(Expected.DisplayName));
		TestEqual(FString::Printf(TEXT("%s owner"), Expected.CardId), Actual->Owner, EGameXXKCardOwner::Profession);
		TestEqual(FString::Printf(TEXT("%s role"), Expected.CardId), Actual->Role, EGameXXKCharacterRole::Hunter);
		TestEqual(FString::Printf(TEXT("%s owner ID"), Expected.CardId), Actual->OwnerId, FName(TEXT("Profession.Hunter")));
		TestEqual(FString::Printf(TEXT("%s energy"), Expected.CardId), Actual->EnergyCost, Expected.EnergyCost);
		TestEqual(FString::Printf(TEXT("%s mana"), Expected.CardId), Actual->ManaCost, Expected.ManaCost);
		TestEqual(FString::Printf(TEXT("%s target"), Expected.CardId), Actual->TargetSpec.Mode, Expected.TargetMode);
		TestEqual(FString::Printf(TEXT("%s core flag"), Expected.CardId), Actual->bCoreProfessionCard, Expected.bCore);
		TestEqual(FString::Printf(TEXT("%s effect count"), Expected.CardId), Actual->Effects.Num(), Expected.Effects.Num());
		for (int32 EffectIndex = 0; EffectIndex < Actual->Effects.Num() && EffectIndex < Expected.Effects.Num(); ++EffectIndex)
		{
			const FGameXXKCardEffect& ActualEffect = Actual->Effects[EffectIndex];
			const FExpectedEffect& ExpectedEffect = Expected.Effects[EffectIndex];
			const FString Prefix = FString::Printf(TEXT("%s effect %d"), Expected.CardId, EffectIndex);
			TestEqual(Prefix + TEXT(" type"), ActualEffect.Type, ExpectedEffect.Type);
			TestEqual(Prefix + TEXT(" target"), ActualEffect.Target, ExpectedEffect.Target);
			TestEqual(Prefix + TEXT(" magnitude"), ActualEffect.Magnitude, ExpectedEffect.Magnitude);
			TestEqual(Prefix + TEXT(" status"), ActualEffect.Status, ExpectedEffect.Status);
			TestEqual(Prefix + TEXT(" hit count"), ActualEffect.HitCount, ExpectedEffect.HitCount);
			TestEqual(Prefix + TEXT(" secondary"), ActualEffect.SecondaryMagnitude, ExpectedEffect.SecondaryMagnitude);
			TestEqual(Prefix + TEXT(" condition"), ActualEffect.Condition.Type, ExpectedEffect.ConditionType);
			TestEqual(Prefix + TEXT(" condition status"), ActualEffect.Condition.Status, ExpectedEffect.ConditionStatus);
			TestEqual(Prefix + TEXT(" condition minimum"), ActualEffect.Condition.MinimumStatusStacks, ExpectedEffect.MinimumStatusStacks);
			TestEqual(Prefix + TEXT(" condition health"), ActualEffect.Condition.HealthPercentThreshold, ExpectedEffect.HealthPercentThreshold);
		}
		TestEqual(FString::Printf(TEXT("%s Heavy Arrow kind"), Expected.CardId), Actual->HeavyArrow.Kind, Expected.HeavyKind);
		TestEqual(FString::Printf(TEXT("%s Heavy Arrow magnitude"), Expected.CardId), Actual->HeavyArrow.MagnitudePerCharge, Expected.HeavyMagnitude);
		TestEqual(FString::Printf(TEXT("%s Heavy Arrow draw"), Expected.CardId), Actual->HeavyArrow.DrawPerCharge, Expected.HeavyDrawPerCharge);
		TestEqual(FString::Printf(TEXT("%s Heavy Arrow mana"), Expected.CardId), Actual->HeavyArrow.ManaPerCharge, Expected.HeavyManaPerCharge);
	}
	TestEqual(TEXT("the permanent Hunter pool remains exactly eighteen stable cards"), SeenIds.Num(), 18);
	return true;
}

#endif
