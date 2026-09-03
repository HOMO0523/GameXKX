#include "GameXXKSorcererPartnerRuntimeTestUtils.h"
#include "GameXXKCardQualityRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKPartnerSorcererRebalanceTest
{
	using namespace GameXXKSorcererPartnerRuntimeTestUtils;
	const FName NpcId(TEXT("Npc.TusiChief"));
	const FName Flow(TEXT("Profession.Sorcerer.SheLingHuo"));
	const FName Capacity(TEXT("Profession.Sorcerer.FenMaiFu"));
	const FName Mirror(TEXT("Profession.Sorcerer.LingYanLianDan"));
	const FName Search(TEXT("Profession.Sorcerer.HuLingMu"));
	const FName Echo(TEXT("Profession.Sorcerer.JuLing"));
	const FName Group(TEXT("Profession.Sorcerer.XingHuoHuiShou"));
	const FName Draw(TEXT("Profession.Sorcerer.LieFu"));
	const FName Replay(TEXT("Profession.Sorcerer.ChiYanFengJie"));

	bool Fixture(FAutomationTestBase& Test, const TArray<FName>& CardIds, FGameXXKCardBattleRuntime& Runtime,
		const int32 Defense = 100, const int32 Level = 100)
	{
		TArray<FGameXXKCardInstance> Cards;
		for (int32 Index = 0; Index < CardIds.Num(); ++Index)
		{
			FGameXXKCardInstance Card = MakeCard(CardIds[Index], Index);
			const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(CardIds[Index]);
			if (!Definition) return false;
			Card.CurrentQuality = Definition->BaseQuality;
			Cards.Add(Card);
		}
		TArray<FGameXXKCardCombatUnit> Units = {
			MakeUnit(SorcererId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Sorcerer, 1, 100),
			MakeUnit(AllyId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 2, 100),
			MakeUnit(NpcId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::QuestNpc, 3, 100),
			MakeUnit(EnemyAId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 10),
			MakeUnit(EnemyBId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 11)};
		for (FGameXXKCardCombatUnit& Unit : Units)
		{
			Unit.CombatLevel = Level;
			if (Unit.Side == EGameXXKCardTargetSide::Enemy) Unit.HP = Unit.MaxHP = 100000;
		}
		Units[0].Mana = Units[0].MaxMana = 34;
		Units[0].Defense = Defense;
		Units[1].Mana = 7;
		Units[1].MaxMana = 30;
		Units[2].Mana = 2;
		Units[2].MaxMana = 20;
		if (!BuildRuntime(Test, Cards, Units, 609041, Runtime)) return false;
		return InstallAllCardsInHand(Test, Runtime, Cards);
	}

	bool Play(FAutomationTestBase& Test, FGameXXKCardBattleRuntime& Runtime, const FName CardId,
		FGameXXKCardPlayResult* OutResult = nullptr)
	{
		const FGameXXKCardInstance* Card = Runtime.Deck.Hand.FindByPredicate([CardId](const FGameXXKCardInstance& C) { return C.CardId == CardId; });
		if (!Card) { Test.AddError(TEXT("The requested test card is not in hand: ") + CardId.ToString()); return false; }
		const FName InstanceId = Card->InstanceId;
		const int32 ManaBefore = FindUnit(Runtime, SorcererId)->Mana;
		const int32 ArmorBefore = FindUnit(Runtime, SorcererId)->Armor;
		const int32 EnergyBefore = Runtime.Deck.SharedEnergy;
		FGameXXKCardPlayPreview Preview;
		FString Error;
		if (!GameXXKCardRules::BuildCardPlayPreview(Runtime, InstanceId, Preview, &Error) || !Preview.bCanPlay)
		{
			Test.AddError(TEXT("Card preview rejected: ") + CardId.ToString() + TEXT(" ") + Error + Preview.FailureReason);
			return false;
		}
		Test.TestEqual(TEXT("preview leaves Mana unchanged"), FindUnit(Runtime, SorcererId)->Mana, ManaBefore);
		Test.TestEqual(TEXT("preview leaves Armor unchanged"), FindUnit(Runtime, SorcererId)->Armor, ArmorBefore);
		Test.TestEqual(TEXT("preview leaves Energy unchanged"), Runtime.Deck.SharedEnergy, EnergyBefore);
		FGameXXKCardPlayResult Result;
		if (!ResolveActive(Test, Runtime, InstanceId, Result, *CardId.ToString())) return false;
		if (OutResult) *OutResult = MoveTemp(Result);
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKPartnerSorcererFeesTest,
	"GameXXK.Data.PartnerCards.Sorcerer.Rebalance.EnergyCosts", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKPartnerSorcererFeesTest::RunTest(const FString& Parameters)
{
	int32 Count = 0;
	for (const FGameXXKCardDefinition& Card : FGameXXKCardCatalog::GetAllCardDefinitions())
	{
		if (Card.Owner != EGameXXKCardOwner::Profession || Card.Role != EGameXXKCharacterRole::Sorcerer) continue;
		++Count;
		const int32 Expected = Card.SorcererRule.SequenceRule == EGameXXKSorcererSequenceRule::CoreManaEcho
			|| Card.SorcererRule.SequenceRule == EGameXXKSorcererSequenceRule::UniversalDraw ? 0 : 1;
		for (const EGameXXKCardQuality Quality : {EGameXXKCardQuality::Common, EGameXXKCardQuality::Rare, EGameXXKCardQuality::Epic})
		{
			const FGameXXKCardDefinition Effective = FGameXXKCardQualityRules::BuildEffectiveDefinition(Card, Quality);
			TestEqual(*Card.Id.ToString(), Effective.EnergyCost, Expected);
			TestEqual(TEXT("quality does not change Mage Mana costs"), Effective.ManaCost, Card.ManaCost);
		}
	}
	TestEqual(TEXT("all eighteen partner Mage cards are covered"), Count, 18);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKPartnerSorcererIceRecoveryTest,
	"GameXXK.Data.PartnerCards.Sorcerer.Rebalance.IceRecovery", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKPartnerSorcererIceRecoveryTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKPartnerSorcererRebalanceTest;
	struct FCase { FName Card; int32 Mana; int32 Armor; int32 ExpectedMana; int32 ExpectedMax; int32 ExpectedArmor; };
	const TArray<FCase> Cases = {
		{Flow, 34, 0, 34, 34, 24}, {Capacity, 34, 0, 38, 38, 0},
		{Mirror, 34, 0, 34, 34, 24}, {Mirror, 34, 100, 34, 34, 200},
		{Search, 34, 0, 34, 34, 48}, {Search, 31, 0, 34, 34, 12},
		{Search, 30, 0, 33, 34, 0}, {Search, 0, 0, 0, 34, 0},
		{Mirror, 0, 0, 0, 34, 0}, {Capacity, 0, 0, 0, 38, 0}};
	for (const int32 Defense : {5, 257, 1000})
	{
		for (const FCase& Row : Cases)
		{
			FGameXXKCardBattleRuntime Runtime;
			if (!Fixture(*this, {Flow, Capacity, Mirror, Search, Echo}, Runtime, Defense)) return false;
			FindUnit(Runtime, SorcererId)->Mana = Row.Mana;
			FindUnit(Runtime, SorcererId)->Armor = Row.Armor;
			if (!Play(*this, Runtime, Row.Card)) return false;
			const FGameXXKCardCombatUnit* Owner = FindUnit(Runtime, SorcererId);
			TestEqual(*FString::Printf(TEXT("%s Mana=%d Armor=%d Defense=%d: current Mana"), *Row.Card.ToString(), Row.Mana, Row.Armor, Defense), Owner->Mana, Row.ExpectedMana);
			TestEqual(TEXT("only explicit capacity effects change maximum Mana"), Owner->MaxMana, Row.ExpectedMax);
			TestEqual(TEXT("only actual overflow generates Armor, with no Defense multiplier or second recovery"), Owner->Armor, Row.ExpectedArmor);
			TestEqual(TEXT("zero output still records the active card"), Runtime.SorcererPartnerTasks[0].CompletedCardIds.Num(), 1);
		}
	}
	FGameXXKCardBattleRuntime SearchRuntime;
	if (!Fixture(*this, {Flow, Capacity, Mirror, Search, Echo}, SearchRuntime)) return false;
	const FGameXXKCardInstance Candidate = SearchRuntime.Deck.Hand.Pop();
	SearchRuntime.Deck.DrawPile.Add(Candidate);
	if (!Play(*this, SearchRuntime, Search)) return false;
	TestEqual(TEXT("a legal search grants only its initial overflow Armor"), FindUnit(SearchRuntime, SorcererId)->Armor, 24);
	TestEqual(TEXT("the real search pauses for the legal candidate"), SearchRuntime.Deck.PendingChoice.Candidates.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKPartnerSorcererGroupOverflowTest,
	"GameXXK.Data.PartnerCards.Sorcerer.Rebalance.GroupOverflow", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKPartnerSorcererGroupOverflowTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKPartnerSorcererRebalanceTest;
	for (const bool bNonDamagePredecessor : {false, true})
	{
		FGameXXKCardBattleRuntime Runtime;
		if (!Fixture(*this, {Group, Echo, Flow, Mirror, Search}, Runtime)) return false;
		if (bNonDamagePredecessor && !Play(*this, Runtime, Echo)) return false;
		if (!Play(*this, Runtime, Group)) return false;
		const int32 ExpectedArmor = bNonDamagePredecessor ? 72 : 24;
		for (const FName Id : {SorcererId, AllyId, NpcId}) TestEqual(TEXT("each ally receives one full overflow grant"), FindUnit(Runtime, Id)->Armor, ExpectedArmor);
		TestEqual(TEXT("the caster restores fixed Mana after paying four"), FindUnit(Runtime, SorcererId)->Mana, 34);
		TestEqual(TEXT("group Armor does not refill the hero"), FindUnit(Runtime, AllyId)->Mana, 7);
		TestEqual(TEXT("group Armor does not refill the NPC"), FindUnit(Runtime, NpcId)->Mana, 2);
	}
	FGameXXKCardBattleRuntime Empty;
	if (!Fixture(*this, {Group, Echo, Flow, Mirror, Search}, Empty)) return false;
	FindUnit(Empty, SorcererId)->Mana = 4;
	if (!Play(*this, Empty, Group)) return false;
	TestEqual(TEXT("fixed recovery with no overflow still succeeds"), FindUnit(Empty, SorcererId)->Mana, 8);
	for (const FName Id : {SorcererId, AllyId, NpcId}) TestEqual(TEXT("there is no flat group Armor without overflow"), FindUnit(Empty, Id)->Armor, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKPartnerSorcererIceTaskChainsTest,
	"GameXXK.Data.PartnerCards.Sorcerer.Rebalance.IceTaskChains", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKPartnerSorcererIceTaskChainsTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKPartnerSorcererRebalanceTest;
	struct FCase { TArray<FName> Cards; int32 CasterArmor; int32 OtherArmor; int32 MaxMana; int32 Energy; };
	const TArray<FCase> Cases = {
		{{Group, Flow, Echo, Search, Mirror}, 193, 265, 34, 16},
		{{Draw, Flow, Group, Search, Mirror}, 228, 168, 34, 17},
		{{Replay, Flow, Group, Search, Mirror}, 0, 168, 34, 15},
		{{Draw, Flow, Capacity, Search, Mirror}, 114, 0, 42, 17},
		{{Replay, Flow, Capacity, Search, Mirror}, 0, 0, 42, 15},
		{{Mirror, Flow, Capacity, Search, Draw}, 87, 87, 42, 16}};
	for (const FCase& Row : Cases)
	{
		FGameXXKCardBattleRuntime Runtime;
		if (!Fixture(*this, Row.Cards, Runtime)) return false;
		for (const FName Card : Row.Cards) if (!Play(*this, Runtime, Card)) return false;
		TestEqual(*FString::Printf(TEXT("%s starter: final caster Armor"), *Row.Cards[0].ToString()), FindUnit(Runtime, SorcererId)->Armor, Row.CasterArmor);
		TestEqual(TEXT("hero retains prior Armor plus only the appropriate reward"), FindUnit(Runtime, AllyId)->Armor, Row.OtherArmor);
		TestEqual(TEXT("NPC receives the same full party reward"), FindUnit(Runtime, NpcId)->Armor, Row.OtherArmor);
		TestEqual(TEXT("capacity persists through free replay"), FindUnit(Runtime, SorcererId)->MaxMana, Row.MaxMana);
		TestEqual(TEXT("free replays do not pay again; refund comes after the fifth paid card"), Runtime.Deck.SharedEnergy, Row.Energy);
		TestFalse(TEXT("the entire task finishes without a pending continuation"), Runtime.AutomaticResolutionQueue.bActive);
	}
	return true;
}

namespace GameXXKPartnerSorcererRebalanceTest
{
	bool ReplayAt(FAutomationTestBase& Test, FGameXXKCardBattleRuntime& Runtime, const FName CardId,
		const EGameXXKCardQuality Quality, const int32 Position, FGameXXKCardPlayResult& Result)
	{
		FGameXXKResolvedCardSnapshot Snapshot;
		Snapshot.CardId = CardId;
		Snapshot.OwnerUnitId = SorcererId;
		Snapshot.Quality = Quality;
		Snapshot.SorcererSequencePosition = Position;
		Snapshot.PreviousSorcererFamily = Position > 1 ? EGameXXKSorcererCardFamily::Core : EGameXXKSorcererCardFamily::None;
		Snapshot.SorcererTaskBranch = Position > 0 ? EGameXXKSorcererTaskBranch::Normal : EGameXXKSorcererTaskBranch::None;
		Runtime.AutomaticResolutionQueue.bActive = true;
		Runtime.AutomaticResolutionQueue.Origin = EGameXXKCardResolutionOrigin::AutomaticReplay;
		Runtime.AutomaticResolutionQueue.PendingCards = {Snapshot};
		Runtime.AutomaticResolutionQueue.NextCardIndex = 0;
		return ResolveCompletedReward(Test, Runtime, Result);
	}

	bool RewardReady(FAutomationTestBase& Test, const FName Starter, const EGameXXKCardQuality Quality,
		const EGameXXKSorcererTaskBranch Branch, FGameXXKCardBattleRuntime& Runtime)
	{
		TArray<FName> Ids = {Starter};
		const FGameXXKCardDefinition* StarterDefinition = FGameXXKCardCatalog::FindCardDefinition(Starter);
		if (!StarterDefinition) return false;
		if (StarterDefinition->SorcererRule.Family == EGameXXKSorcererCardFamily::Universal)
		{
			Ids.Add(Branch == EGameXXKSorcererTaskBranch::Ice ? Capacity
				: Branch == EGameXXKSorcererTaskBranch::Fire ? FName(TEXT("Profession.Sorcerer.LiHuoYin"))
				: Branch == EGameXXKSorcererTaskBranch::Lightning ? FName(TEXT("Profession.Sorcerer.ChiXiaoFenXing")) : Echo);
		}
		for (const FName Id : {Echo, Capacity, Flow, Mirror, Search, Group})
		{
			if (Ids.Num() < 5) Ids.AddUnique(Id);
		}
		if (!Fixture(Test, Ids, Runtime)) return false;
		Runtime.Deck.Hand[0].CurrentQuality = Quality;
		Runtime.Deck.DiscardPile = MoveTemp(Runtime.Deck.Hand);
		FGameXXKSorcererPartnerTaskRuntime& Task = Runtime.SorcererPartnerTasks.AddDefaulted_GetRef();
		Task.bActive = true;
		Task.OwnerUnitId = SorcererId;
		Task.LockedCardIds = Task.CompletedCardIds = Ids;
		Task.StarterReward = StarterDefinition->SorcererRule.RewardRule;
		Task.LockedBranch = StarterDefinition->SorcererRule.Family == EGameXXKSorcererCardFamily::Universal
			? Branch : BranchForFamily(StarterDefinition->SorcererRule.Family);
		for (int32 Index = 0; Index < Ids.Num(); ++Index)
		{
			FGameXXKResolvedCardSnapshot& Snapshot = Task.FirstPlayOrder.AddDefaulted_GetRef();
			Snapshot.CardId = Ids[Index];
			Snapshot.OwnerUnitId = SorcererId;
			Snapshot.Quality = Runtime.Deck.DiscardPile[Index].CurrentQuality;
			Snapshot.SorcererSequencePosition = Index + 1;
			Snapshot.PreviousSorcererFamily = Index == 0 ? EGameXXKSorcererCardFamily::None
				: FGameXXKCardCatalog::FindCardDefinition(Ids[Index - 1])->SorcererRule.Family;
			Snapshot.SorcererTaskBranch = Task.LockedBranch;
		}
		Runtime.AutomaticResolutionQueue.bActive = true;
		Runtime.AutomaticResolutionQueue.Origin = EGameXXKCardResolutionOrigin::PartnerSorcererTaskReplay;
		Runtime.AutomaticResolutionQueue.PendingCards = Task.FirstPlayOrder;
		Runtime.AutomaticResolutionQueue.NextCardIndex = 5;
		Runtime.AutomaticResolutionQueue.PendingSorcererReward = Task.StarterReward;
		Runtime.AutomaticResolutionQueue.RewardOwnerUnitId = SorcererId;
		FString Error;
		const bool bValid = GameXXKCardRules::ValidateCardBattleRuntime(Runtime, &Error);
		Test.TestTrue(TEXT("completed reward fixture validates: ") + Error, bValid);
		return bValid;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKPartnerSorcererScalingTest,
	"GameXXK.Data.PartnerCards.Sorcerer.Rebalance.Scaling", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKPartnerSorcererScalingTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKPartnerSorcererRebalanceTest;
	const FName Lamp(TEXT("Profession.Sorcerer.LiHuoYin"));
	const FName Burst(TEXT("Profession.Sorcerer.BaoYanShu"));
	const FName FireSearch(TEXT("Profession.Sorcerer.XingHuoLiaoYuan"));
	const FName Lightning(TEXT("Profession.Sorcerer.NingYanChengRen"));
	FGameXXKCardPlayResult Result;
	const TArray<EGameXXKCardQuality> Qualities = {EGameXXKCardQuality::Common, EGameXXKCardQuality::Rare, EGameXXKCardQuality::Epic};
	const TArray<int32> BurnExpected = {20, 24, 28};
	for (int32 Index = 0; Index < Qualities.Num(); ++Index)
	{
		FGameXXKCardBattleRuntime Runtime;
		if (!Fixture(*this, {Lamp, Burst, Echo, Flow, Capacity}, Runtime)
			|| !ReplayAt(*this, Runtime, Lamp, Qualities[Index], 1, Result)) return false;
		TestEqual(TEXT("early Fire coefficient four receives level and quality once"),
			GameXXKCardRules::GetCombatStatusStacks(*FindUnit(Runtime, EnemyAId), EGameXXKCardStatus::Burn), BurnExpected[Index]);
	}
	FGameXXKCardBattleRuntime BurstRuntime;
	if (!Fixture(*this, {Burst, Lamp, Echo, Flow, Capacity}, BurstRuntime)) return false;
	GameXXKCardRules::AddCombatStatus(*FindUnit(BurstRuntime, EnemyAId), EGameXXKCardStatus::Burn, 50);
	if (!ReplayAt(*this, BurstRuntime, Burst, EGameXXKCardQuality::Rare, 3, Result)) return false;
	TestEqual(TEXT("Rare Fire burst uses 96 plus two points per stored Burn"), FindUnit(BurstRuntime, EnemyAId)->HP, 99804);
	TestEqual(TEXT("the burst does not consume or regenerate stored Burn"),
		GameXXKCardRules::GetCombatStatusStacks(*FindUnit(BurstRuntime, EnemyAId), EGameXXKCardStatus::Burn), 50);
	for (const auto& Row : TArray<TPair<FName, int32>>{{FireSearch, 196}, {Replay, 252}, {TEXT("Profession.Sorcerer.YanMuHuTi"), 224}})
	{
		FGameXXKCardBattleRuntime Runtime;
		if (!Fixture(*this, {Row.Key, Echo, Flow, Capacity, Mirror}, Runtime)
			|| !ReplayAt(*this, Runtime, Row.Key, EGameXXKCardQuality::Epic,
				Row.Key == FName(TEXT("Profession.Sorcerer.YanMuHuTi")) ? 5 : 4, Result)) return false;
		TestEqual(TEXT("sequence override and failed-search repeat retain the card's own Epic multiplier"), FindUnit(Runtime, EnemyAId)->HP, 100000 - Row.Value);
	}
	FGameXXKCardBattleRuntime LightningRuntime;
	if (!Fixture(*this, {Lightning, Echo, Flow, Capacity, Mirror}, LightningRuntime)) return false;
	GameXXKCardRules::AddCombatStatus(*FindUnit(LightningRuntime, EnemyAId), EGameXXKCardStatus::Mark, 2);
	if (!ReplayAt(*this, LightningRuntime, Lightning, EGameXXKCardQuality::Epic, 4, Result)) return false;
	TestEqual(TEXT("late Epic lightning hits twice at 98 percent, preserving the existing Mark rounding"), FindUnit(LightningRuntime, EnemyAId)->HP, 99776);
	TestEqual(TEXT("each successful hit consumes one Mark"), GameXXKCardRules::GetCombatStatusStacks(*FindUnit(LightningRuntime, EnemyAId), EGameXXKCardStatus::Mark), 0);
	FGameXXKCardBattleRuntime ManaRuntime;
	if (!Fixture(*this, {Draw, Echo, Flow, Capacity, Mirror}, ManaRuntime)) return false;
	FindUnit(ManaRuntime, SorcererId)->Mana = 20;
	if (!ReplayAt(*this, ManaRuntime, Draw, EGameXXKCardQuality::Epic, 3, Result)) return false;
	TestEqual(TEXT("Epic draw sequence restores its explicit seven Mana"), FindUnit(ManaRuntime, SorcererId)->Mana, 27);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKPartnerSorcererRewardScalingTest,
	"GameXXK.Data.PartnerCards.Sorcerer.Rebalance.RewardScaling", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKPartnerSorcererRewardScalingTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKPartnerSorcererRebalanceTest;
	FGameXXKCardPlayResult Result;
	FGameXXKCardBattleRuntime Fire;
	if (!RewardReady(*this, TEXT("Profession.Sorcerer.XingHuoLiaoYuan"), EGameXXKCardQuality::Epic, EGameXXKSorcererTaskBranch::Fire, Fire)
		|| !ResolveCompletedReward(*this, Fire, Result)) return false;
	TestEqual(TEXT("Epic Fire reward coefficient six generates forty-two Burn at level one hundred"),
		GameXXKCardRules::GetCombatStatusStacks(*FindUnit(Fire, EnemyAId), EGameXXKCardStatus::Burn), 42);
	FGameXXKCardBattleRuntime Spread;
	if (!RewardReady(*this, TEXT("Profession.Sorcerer.YanQiang"), EGameXXKCardQuality::Rare, EGameXXKSorcererTaskBranch::Fire, Spread)) return false;
	GameXXKCardRules::AddCombatStatus(*FindUnit(Spread, EnemyAId), EGameXXKCardStatus::Burn, 31);
	GameXXKCardRules::AddCombatStatus(*FindUnit(Spread, EnemyBId), EGameXXKCardStatus::Burn, 7);
	if (!ResolveCompletedReward(*this, Spread, Result)) return false;
	for (const FName Id : {EnemyAId, EnemyBId}) TestEqual(TEXT("equalization is unscaled; only the new coefficient three is amplified"),
		GameXXKCardRules::GetCombatStatusStacks(*FindUnit(Spread, Id), EGameXXKCardStatus::Burn), 49);
	FGameXXKCardBattleRuntime Lightning;
	if (!RewardReady(*this, TEXT("Profession.Sorcerer.NingYanChengRen"), EGameXXKCardQuality::Epic, EGameXXKSorcererTaskBranch::Lightning, Lightning)) return false;
	GameXXKCardRules::AddCombatStatus(*FindUnit(Lightning, EnemyAId), EGameXXKCardStatus::Mark, 3);
	if (!ResolveCompletedReward(*this, Lightning, Result)) return false;
	TestEqual(TEXT("the lightning starter reward performs five Epic hits after refilling to the five-Mark cap"), FindUnit(Lightning, EnemyAId)->HP, 99440);
	TestEqual(TEXT("five successful reward hits consume the capped five Marks"), GameXXKCardRules::GetCombatStatusStacks(*FindUnit(Lightning, EnemyAId), EGameXXKCardStatus::Mark), 0);
	TestEqual(TEXT("the fixed reward emits five hits for each of two enemies"), Result.DamageResults.Num(), 10);
	for (const auto& Row : TArray<TPair<EGameXXKSorcererTaskBranch, int32>>{
		{EGameXXKSorcererTaskBranch::Normal, 96}, {EGameXXKSorcererTaskBranch::Fire, 72}, {EGameXXKSorcererTaskBranch::Lightning, 48}})
	{
		FGameXXKCardBattleRuntime Runtime;
		if (!RewardReady(*this, Group, EGameXXKCardQuality::Rare, Row.Key, Runtime) || !ResolveCompletedReward(*this, Runtime, Result)) return false;
		for (const FName Id : {SorcererId, AllyId, NpcId}) TestEqual(TEXT("group reward uses caster Defense and starter quality once for each recipient"), FindUnit(Runtime, Id)->Armor, Row.Value);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKPartnerSorcererEnergyBoundaryTest,
	"GameXXK.Data.PartnerCards.Sorcerer.Rebalance.PaymentBoundary", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKPartnerSorcererEnergyBoundaryTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKPartnerSorcererRebalanceTest;
	FGameXXKCardBattleRuntime Runtime;
	if (!Fixture(*this, {Replay, Flow, Group, Search, Mirror}, Runtime)) return false;
	Runtime.Deck.SharedEnergy = 4;
	for (const FName Id : {Replay, Flow, Group, Search}) if (!Play(*this, Runtime, Id)) return false;
	const FName LastInstance = Runtime.Deck.Hand.FindByPredicate([](const FGameXXKCardInstance& Card) { return Card.CardId == Mirror; })->InstanceId;
	const int32 ArmorBefore = FindUnit(Runtime, SorcererId)->Armor;
	const int32 ManaBefore = FindUnit(Runtime, SorcererId)->Mana;
	const int32 HandBefore = Runtime.Deck.Hand.Num();
	FGameXXKCardPlayResult Result;
	FString Error;
	TestFalse(TEXT("a future reward cannot pay the fifth active card's fee"), GameXXKCardRules::ResolveCardPlay(Runtime, LastInstance, NAME_None, Result, &Error));
	TestEqual(TEXT("rejection preserves four completed records"), Runtime.SorcererPartnerTasks[0].CompletedCardIds.Num(), 4);
	TestEqual(TEXT("rejection does not double or consume Armor"), FindUnit(Runtime, SorcererId)->Armor, ArmorBefore);
	TestEqual(TEXT("rejection does not change Mana"), FindUnit(Runtime, SorcererId)->Mana, ManaBefore);
	TestEqual(TEXT("rejection leaves the card in hand"), Runtime.Deck.Hand.Num(), HandBefore);
	Runtime.Deck.SharedEnergy = 1;
	if (!Play(*this, Runtime, Mirror)) return false;
	TestEqual(TEXT("with the actual fee available the free replay finishes at zero Energy"), Runtime.Deck.SharedEnergy, 0);
	TestFalse(TEXT("the completed task queue is drained"), Runtime.AutomaticResolutionQueue.bActive);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKPartnerSorcererArmorBoundariesTest,
	"GameXXK.Data.PartnerCards.Sorcerer.Rebalance.ResolvedArmorBoundaries", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKPartnerSorcererArmorBoundariesTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKPartnerSorcererRebalanceTest;
	FGameXXKCardPlayResult Result;
	for (const EGameXXKCardQuality Quality : {EGameXXKCardQuality::Common, EGameXXKCardQuality::Rare, EGameXXKCardQuality::Epic})
	{
		FGameXXKCardBattleRuntime Runtime;
		if (!Fixture(*this, {Capacity, Flow, Mirror, Search, Echo}, Runtime)) return false;
		Runtime.Deck.Hand[0].CurrentQuality = Quality;
		FindUnit(Runtime, SorcererId)->Mana = FindUnit(Runtime, SorcererId)->MaxMana = 42;
		if (!Play(*this, Runtime, Capacity)) return false;
		const int32 Expected = Quality == EGameXXKCardQuality::Common ? 5 : Quality == EGameXXKCardQuality::Rare ? 6 : 7;
		TestEqual(TEXT("capacity increases before recovery, so one Mana overflows"), FindUnit(Runtime, SorcererId)->Mana, 46);
		TestEqual(TEXT("quality changes Armor but not the five recovered Mana"), FindUnit(Runtime, SorcererId)->Armor, Expected);
		if (!ReplayAt(*this, Runtime, Capacity, Quality, 0, Result)) return false;
		TestEqual(TEXT("unsequenced replay still runs capacity and current-Mana recovery"), FindUnit(Runtime, SorcererId)->MaxMana, 50);
		TestEqual(TEXT("replay recomputes ceiling ten percent from the new current Mana"), FindUnit(Runtime, SorcererId)->Mana, 50);
		TestEqual(TEXT("one more overflow uses exactly one quality and level multiplier"), FindUnit(Runtime, SorcererId)->Armor, 2 * Expected);
	}
	for (const EGameXXKCardQuality Quality : {EGameXXKCardQuality::Rare, EGameXXKCardQuality::Epic})
	{
		for (const int32 Defense : {5, 1000})
		{
			for (const int32 Consumed : {0, 1003})
			{
				for (const bool bNpcLiving : {false, true})
				{
					FGameXXKCardBattleRuntime Runtime;
					if (!RewardReady(*this, Mirror, Quality, EGameXXKSorcererTaskBranch::Ice, Runtime)) return false;
					Runtime.TeamMaxLevelSnapshot = Defense == 5 ? 1 : 100;
					for (FGameXXKCardCombatUnit& Unit : Runtime.Units) Unit.CombatLevel = Runtime.TeamMaxLevelSnapshot;
					FindUnit(Runtime, SorcererId)->Defense = Defense;
					FindUnit(Runtime, SorcererId)->Armor = Consumed;
					FindUnit(Runtime, AllyId)->Armor = 11;
					FindUnit(Runtime, NpcId)->Armor = bNpcLiving ? 23 : 0;
					FindUnit(Runtime, NpcId)->bLiving = bNpcLiving;
					if (!bNpcLiving) FindUnit(Runtime, NpcId)->HP = 0;
					// Armor absorption eliminates HP damage without changing the source's consumed Armor.
					FindUnit(Runtime, EnemyAId)->Armor = 100000;
					FindUnit(Runtime, EnemyBId)->Armor = 100000;
					if (!ResolveCompletedReward(*this, Runtime, Result)) return false;
					const int32 Refund = Consumed == 0 ? 0 : 250;
					TestEqual(TEXT("refund is floor consumed Armor divided by four, independent of level/Defense/quality"), FindUnit(Runtime, SorcererId)->Armor, Refund);
					TestEqual(TEXT("other living allies preserve their old Armor and receive the entire refund"), FindUnit(Runtime, AllyId)->Armor, 11 + Refund);
					TestEqual(TEXT("defeated NPC is excluded, living NPC receives a full grant"), FindUnit(Runtime, NpcId)->Armor, bNpcLiving ? 23 + Refund : 0);
					TestEqual(TEXT("refund does not depend on actual enemy HP loss"), FindUnit(Runtime, EnemyAId)->HP, 100000);
				}
			}
		}
	}
	return true;
}

#endif
