#include "GameXXKCardBattleAdapter.h"
#include "GameXXKCardCatalog.h"
#include "GameXXKCardRules.h"
#include "GameXXKMVPRules.h"
#include "MVP/GameXXKSaveMigration.h"

#include "Misc/AutomationTest.h"

#include <type_traits>

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKHeroCardUnlockMigrationTest
{
	struct FLegacyCardPair
	{
		const TCHAR* LegacyId;
		const TCHAR* CanonicalId;
	};

	const TArray<FLegacyCardPair>& LegacyPairs()
	{
		static const TArray<FLegacyCardPair> Pairs = {
			{TEXT("Hero.QingFengYiShi"), TEXT("Hero.Generic.QingFengYiShi")},
			{TEXT("Hero.HeYuZhan"), TEXT("Hero.Generic.HeYuZhan")},
			{TEXT("Hero.FengShenBu"), TEXT("Hero.Generic.FengShenBu")},
			{TEXT("Hero.SuiYanJi"), TEXT("Hero.Generic.SuiYanJi")},
			{TEXT("Hero.GuiYuanShu"), TEXT("Hero.Generic.GuiYuanShu")},
			{TEXT("Hero.HengJianShouShi"), TEXT("Hero.Generic.HengJianShouShi")},
			{TEXT("Hero.NingShenTuNa"), TEXT("Hero.Generic.NingShenTuNa")},
			{TEXT("Hero.GuanXi"), TEXT("Hero.Generic.GuanXi")},
			{TEXT("Hero.PoYunYiShan"), TEXT("Hero.Generic.PoYunYiShan")},
			{TEXT("Hero.HuiFengZhuiJian"), TEXT("Hero.Generic.XingQiHuiHuan")},
			{TEXT("Hero.JianYiGuanHong"), TEXT("Hero.Generic.JianYiGuanHong")},
			{TEXT("Hero.GuiYuanFanZhao"), TEXT("Hero.Generic.GuiYuanFanZhao")}
		};
		return Pairs;
	}

	const TArray<FName>& CanonicalHeroIds()
	{
		static const TArray<FName> Ids = {
			TEXT("Hero.Generic.QingFengYiShi"), TEXT("Hero.Generic.HeYuZhan"),
			TEXT("Hero.Generic.FengShenBu"), TEXT("Hero.Generic.SuiYanJi"),
			TEXT("Hero.Generic.GuiYuanShu"), TEXT("Hero.Generic.HengJianShouShi"),
			TEXT("Hero.Generic.NingShenTuNa"), TEXT("Hero.Generic.GuanXi"),
			TEXT("Hero.Generic.PoYunYiShan"), TEXT("Hero.Generic.XingQiHuiHuan"),
			TEXT("Hero.Generic.JianYiGuanHong"), TEXT("Hero.Generic.GuiYuanFanZhao"),
			TEXT("Hero.Blade.TongFengYinShi"), TEXT("Hero.Blade.XueLuXiangCheng"),
			TEXT("Hero.Blade.YingFengHuanBu"), TEXT("Hero.Blade.TongPaoJuShi"),
			TEXT("Hero.Guard.TieBiTongShou"), TEXT("Hero.Guard.JieJiaHuanFeng"),
			TEXT("Hero.Guard.LieZhenChengFeng"), TEXT("Hero.Guard.XuanJiaZhenYue"),
			TEXT("Hero.Healer.YiXueCuiFang"), TEXT("Hero.Healer.HuiChunNiMai"),
			TEXT("Hero.Healer.DuHuoTongLu"), TEXT("Hero.Healer.BaiCaoJiZhen"),
			TEXT("Hero.Hunter.FengYanDingXian"), TEXT("Hero.Hunter.LieYuLianShi"),
			TEXT("Hero.Hunter.CuiDuChuanXin"), TEXT("Hero.Hunter.HuiFengGuanRi"),
			TEXT("Hero.Mage.YanXuLiaoYuan"), TEXT("Hero.Mage.HanXuNingChuan"),
			TEXT("Hero.Mage.LeiXuYinTing"), TEXT("Hero.Mage.GuiXuTongXuan"),
			TEXT("Hero.Formation.GuanShiLuoZi"), TEXT("Hero.Formation.YiZhenHuiXiang"),
			TEXT("Hero.Formation.LianYingBuShi"), TEXT("Hero.Formation.LiuHeGuiYi")
		};
		return Ids;
	}

	FName MapLegacyId(const FName CardId)
	{
		for (const FLegacyCardPair& Pair : LegacyPairs())
		{
			if (CardId == FName(Pair.LegacyId))
			{
				return FName(Pair.CanonicalId);
			}
		}
		return CardId;
	}

	FName MapCanonicalToLegacy(const FName CardId)
	{
		for (const FLegacyCardPair& Pair : LegacyPairs())
		{
			if (CardId == FName(Pair.CanonicalId))
			{
				return FName(Pair.LegacyId);
			}
		}
		return CardId;
	}

	TArray<FName> ExpectedUnlocks(const int32 Level)
	{
		TArray<FName> Result;
		const TArray<FName>& AllIds = CanonicalHeroIds();
		for (int32 Index = 0; Index < AllIds.Num(); ++Index)
		{
			const int32 RequiredLevel = Index == 8 ? 5 : Index == 9 ? 10 : Index == 10 ? 15 : Index == 11 ? 20 : 1;
			if (RequiredLevel <= Level)
			{
				Result.Add(AllIds[Index]);
			}
		}
		return Result;
	}

	bool NamesEqual(const TArray<FName>& Left, const TArray<FName>& Right)
	{
		return Left == Right;
	}

	TArray<FName> FirstEight(const TArray<FName>& Values)
	{
		TArray<FName> Result;
		for (int32 Index = 0; Index < FMath::Min(8, Values.Num()); ++Index)
		{
			Result.Add(Values[Index]);
		}
		return Result;
	}

	template <typename T, typename = void>
	struct THasHeroUnlockQuery : std::false_type
	{
	};

	template <typename T>
	struct THasHeroUnlockQuery<T, std::void_t<decltype(T::GetHeroCardIdsUnlockedAtLevel(1))>> : std::true_type
	{
	};

	template <typename T>
	TArray<FName> QueryHeroUnlocks(const int32 Level)
	{
		if constexpr (THasHeroUnlockQuery<T>::value)
		{
			return T::GetHeroCardIdsUnlockedAtLevel(Level);
		}
		else
		{
			return {};
		}
	}

	template <typename T, typename = void>
	struct THeroCardPoolVersion
	{
		static constexpr int32 Value = INDEX_NONE;
	};

	template <typename T>
	struct THeroCardPoolVersion<T, std::void_t<decltype(T::HeroCardPoolIntroducedSaveVersion)>>
	{
		static constexpr int32 Value = T::HeroCardPoolIntroducedSaveVersion;
	};

	FGameXXKCardCombatUnit MakeUnit(
		const FName UnitId,
		const EGameXXKCardTargetSide Side,
		const EGameXXKCharacterRole Role,
		const int32 StableOrder)
	{
		FGameXXKCardCombatUnit Unit;
		Unit.UnitId = UnitId;
		Unit.Side = Side;
		Unit.Role = Role;
		Unit.bLiving = true;
		Unit.HP = Unit.MaxHP = 100;
		Unit.Mana = Unit.MaxMana = 30;
		Unit.Attack = 15;
		Unit.Defense = 0;
		Unit.Speed = 10;
		Unit.StableSortOrder = StableOrder;
		return Unit;
	}

	FGameXXKCardInstance MakeInstance(const FName CardId, const int32 Ordinal)
	{
		FGameXXKCardInstance Instance;
		Instance.InstanceId = FName(*FString::Printf(TEXT("Legacy.Hero.%02d"), Ordinal));
		Instance.CardId = CardId;
		Instance.CurrentQuality = EGameXXKCardQuality::Common;
		Instance.OwnerUnitId = TEXT("Player");
		Instance.SourceEntryId = FName(*FString::Printf(TEXT("Legacy.Entry.%02d"), Ordinal));
		Instance.AcquisitionOrdinal = Ordinal;
		return Instance;
	}

	FGameXXKResolvedCardSnapshot MakeSnapshot(
		const FName CardId,
		const FName OwnerUnitId,
		const TArray<FName>& TargetUnitIds = {})
	{
		FGameXXKResolvedCardSnapshot Snapshot;
		Snapshot.CardId = CardId;
		Snapshot.Quality = EGameXXKCardQuality::Common;
		Snapshot.OwnerUnitId = OwnerUnitId;
		Snapshot.OriginalTargetUnitIds = TargetUnitIds;
		return Snapshot;
	}

	bool BuildActiveV11Fixture(FGameXXKRuntimeState& OutState, FString& OutError)
	{
		OutState = UGameXXKMVPRules::CreateNewGame();
		OutState.PlayerLevel = 1;
		TArray<FName> LegacyIds;
		TArray<FGameXXKCardInstance> Instances;
		for (int32 Index = 0; Index < LegacyPairs().Num(); ++Index)
		{
			const FName LegacyId(LegacyPairs()[Index].LegacyId);
			const FName CanonicalId(LegacyPairs()[Index].CanonicalId);
			const FName LiveCatalogId = FGameXXKCardCatalog::FindCardDefinition(LegacyId)
				? LegacyId
				: CanonicalId;
			LegacyIds.Add(LegacyId);
			Instances.Add(MakeInstance(LiveCatalogId, Index));
		}
		OutState.CardRun.HeroUnlockedCardIds = LegacyIds;
		OutState.CardRun.HeroSelectedCardIds = {
			TEXT("Hero.PoYunYiShan"), TEXT("Hero.QingFengYiShi"),
			TEXT("Hero.JianYiGuanHong"), TEXT("Hero.HeYuZhan"),
			TEXT("Hero.GuiYuanFanZhao"), TEXT("Hero.FengShenBu"),
			TEXT("Hero.HuiFengZhuiJian"), TEXT("Hero.SuiYanJi")
		};

		TArray<FGameXXKCardCombatUnit> Units = {
			MakeUnit(TEXT("Player"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 0),
			MakeUnit(TEXT("Enemy"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 1)
		};
		FGameXXKCardBattleRuntime Runtime;
		if (!GameXXKCardRules::InitializeCardBattleRuntime(Runtime, Instances, Units, EGameXXKCardTerrain::Plain, 0x1234567, &OutError))
		{
			return false;
		}
		if (Runtime.Deck.Hand.IsEmpty()
			|| !GameXXKCardRules::MoveHandCardToDiscard(Runtime.Deck, Runtime.Deck.Hand[0].InstanceId, &OutError))
		{
			return false;
		}
		for (TArray<FGameXXKCardInstance>* Zone : {&Runtime.Deck.DrawPile, &Runtime.Deck.Hand, &Runtime.Deck.DiscardPile})
		{
			for (FGameXXKCardInstance& Instance : *Zone)
			{
				Instance.CardId = MapCanonicalToLegacy(Instance.CardId);
			}
		}
		FGameXXKCardCombatUnit* Player = Runtime.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
		{
			return Unit.UnitId == TEXT("Player");
		});
		if (!Player)
		{
			OutError = TEXT("Active migration fixture lost the player unit.");
			return false;
		}
		GameXXKCardRules::AddCombatStatus(*Player, EGameXXKCardStatus::Medicine, 5);
		GameXXKCardRules::AddCombatStatus(*Player, EGameXXKCardStatus::NextHealingBonus, 80);

		Runtime.EquippedHeroCardIds = OutState.CardRun.HeroSelectedCardIds;
		Runtime.LastActiveCard.CardId = TEXT("Hero.QingFengYiShi");
		Runtime.LastActiveCard.OwnerUnitId = TEXT("Player");
		Runtime.AutomaticResolutionQueue.bActive = true;
		Runtime.AutomaticResolutionQueue.Origin = EGameXXKCardResolutionOrigin::AutomaticReplay;
		Runtime.AutomaticResolutionQueue.PendingCards.Add(MakeSnapshot(TEXT("Hero.HeYuZhan"), TEXT("Player"), {TEXT("Enemy")}));
		Runtime.HeroSpellTask.bActive = true;
		Runtime.HeroSpellTask.LockedHeroCardIds = OutState.CardRun.HeroSelectedCardIds;
		Runtime.HeroSpellTask.CompletedHeroCardIds = {TEXT("Hero.FengShenBu")};
		Runtime.HeroSpellTask.FirstPlayOrder.Add(MakeSnapshot(TEXT("Hero.FengShenBu"), TEXT("Player"), {TEXT("Player")}));
		Runtime.HeroSpellTask.StarterReward = EGameXXKHeroSpellTaskReward::Fire;
		Runtime.HeroSpellTask.StarterOwnerUnitId = TEXT("Player");
		Runtime.CombatRandomState = 0;
		OutState.CardRun.bLoadoutLockedForRoute = true;
		OutState.CardRun.bHasActiveCardBattle = true;
		OutState.CardRun.ActiveBattle = MoveTemp(Runtime);
		return true;
	}

	int32 StatusStacks(const FGameXXKCardBattleRuntime& Runtime, const EGameXXKCardStatus Status)
	{
		const FGameXXKCardCombatUnit* Player = Runtime.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
		{
			return Unit.UnitId == TEXT("Player");
		});
		return Player ? GameXXKCardRules::GetCombatStatusStacks(*Player, Status) : INDEX_NONE;
	}

	bool CheckMigratedInstances(
		FAutomationTestBase& Test,
		const TCHAR* Label,
		const TArray<FGameXXKCardInstance>& Before,
		const TArray<FGameXXKCardInstance>& After)
	{
		bool bMatches = Test.TestEqual(FString::Printf(TEXT("%s count is preserved"), Label), After.Num(), Before.Num());
		for (int32 Index = 0; Index < FMath::Min(Before.Num(), After.Num()); ++Index)
		{
			bMatches &= Test.TestEqual(
				FString::Printf(TEXT("%s card %d is migrated"), Label, Index),
				After[Index].CardId,
				MapLegacyId(Before[Index].CardId));
		}
		return bMatches;
	}

	bool StartAcceptedRoute(FGameXXKRuntimeState& OutState)
	{
		OutState = UGameXXKMVPRules::CreateNewGame();
		OutState.PlayerLevel = 20;
		if (!UGameXXKMVPRules::OpenWorldMap(OutState)
			|| !UGameXXKMVPRules::EnterWorldRegion(OutState, UGameXXKMVPRules::RegionQingshan())
			|| !UGameXXKMVPRules::AcceptTownQuest(OutState))
		{
			return false;
		}
		OutState.RouteSeed = 0x24681357;
		return UGameXXKMVPRules::EnterDungeon(OutState);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKHeroCardPoolV12Test,
	"GameXXK.MVP.SaveGame.HeroCardPoolV12",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHeroCardPoolV12Test::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroCardUnlockMigrationTest;

	TestEqual(TEXT("the protagonist pool is introduced by save version twelve"), THeroCardPoolVersion<FGameXXKSaveMigration>::Value, 12);
	TestEqual(TEXT("the current save version is sixteen"), FGameXXKSaveMigration::CurrentSaveVersion, 16);
	TestTrue(TEXT("the catalog exposes the deterministic hero unlock query"), THasHeroUnlockQuery<FGameXXKCardCatalog>::value);
	for (const int32 Level : {1, 5, 10, 15, 20})
	{
		TestTrue(
			FString::Printf(TEXT("catalog unlock order is exact at level %d"), Level),
			NamesEqual(QueryHeroUnlocks<FGameXXKCardCatalog>(Level), ExpectedUnlocks(Level)));
	}

	FGameXXKRuntimeState FreshLevelOne = UGameXXKMVPRules::CreateNewGame();
	FreshLevelOne.PlayerLevel = 1;
	FString Error;
	TestTrue(TEXT("level-one card state initializes"), FGameXXKCardBattleAdapter::EnsureCardRunInitialized(FreshLevelOne, &Error));
	TestTrue(TEXT("level one receives exactly thirty-two unlocked cards"), NamesEqual(FreshLevelOne.CardRun.HeroUnlockedCardIds, ExpectedUnlocks(1)));
	TestTrue(TEXT("level one selects the first eight generic cards"), NamesEqual(
		FreshLevelOne.CardRun.HeroSelectedCardIds,
		FirstEight(ExpectedUnlocks(1))));
	const FGameXXKCardRunState FreshLevelOneBeforeSecondPass = FreshLevelOne.CardRun;
	TestTrue(TEXT("level-one initialization is repeatable"), FGameXXKCardBattleAdapter::EnsureCardRunInitialized(FreshLevelOne, &Error));
	TestTrue(TEXT("a second level-one initialization is byte-stable"), FGameXXKCardRunState::StaticStruct()->CompareScriptStruct(
		&FreshLevelOne.CardRun, &FreshLevelOneBeforeSecondPass, PPF_None));

	FGameXXKRuntimeState ActiveState;
	if (!TestTrue(TEXT("the active v11 fixture builds"), BuildActiveV11Fixture(ActiveState, Error)))
	{
		AddError(Error);
		return false;
	}
	FGameXXKSaveState ActiveSource = UGameXXKMVPRules::MakeSaveState(ActiveState);
	ActiveSource.SaveVersion = 11;
	const FGameXXKSaveState ActiveSourceBefore = ActiveSource;
	FGameXXKSaveState ActiveMigratedA;
	FGameXXKSaveMigrationReport ActiveReportA;
	if (!TestTrue(TEXT("the v11 active battle migrates"), FGameXXKSaveMigration::MigrateToCurrent(ActiveSource, ActiveMigratedA, ActiveReportA)))
	{
		AddError(ActiveReportA.Error);
		return false;
	}
	FGameXXKSaveState ActiveMigratedB;
	FGameXXKSaveMigrationReport ActiveReportB;
	TestTrue(TEXT("the same v11 active battle migrates twice"), FGameXXKSaveMigration::MigrateToCurrent(ActiveSource, ActiveMigratedB, ActiveReportB));
	TestTrue(TEXT("migration never mutates its source"), FGameXXKSaveState::StaticStruct()->CompareScriptStruct(&ActiveSource, &ActiveSourceBefore, PPF_None));
	TestTrue(TEXT("identical v11 inputs produce byte-identical v12 saves"), FGameXXKSaveState::StaticStruct()->CompareScriptStruct(&ActiveMigratedA, &ActiveMigratedB, PPF_None));
	TestEqual(TEXT("migration writes version sixteen"), ActiveMigratedA.SaveVersion, 16);
	TestTrue(TEXT("level-one migration rebuilds the exact allowed unlock order"), NamesEqual(
		ActiveMigratedA.RuntimeState.CardRun.HeroUnlockedCardIds, ExpectedUnlocks(1)));
	TestTrue(TEXT("locked legacy selections repair to the first eight legal IDs"), NamesEqual(
		ActiveMigratedA.RuntimeState.CardRun.HeroSelectedCardIds,
		FirstEight(ExpectedUnlocks(1))));

	const FGameXXKCardBattleRuntime& BeforeBattle = ActiveSource.RuntimeState.CardRun.ActiveBattle;
	const FGameXXKCardBattleRuntime& AfterBattle = ActiveMigratedA.RuntimeState.CardRun.ActiveBattle;
	CheckMigratedInstances(*this, TEXT("draw pile"), BeforeBattle.Deck.DrawPile, AfterBattle.Deck.DrawPile);
	CheckMigratedInstances(*this, TEXT("hand"), BeforeBattle.Deck.Hand, AfterBattle.Deck.Hand);
	CheckMigratedInstances(*this, TEXT("discard pile"), BeforeBattle.Deck.DiscardPile, AfterBattle.Deck.DiscardPile);
	TestEqual(TEXT("last active-card snapshot migrates"), AfterBattle.LastActiveCard.CardId, MapLegacyId(BeforeBattle.LastActiveCard.CardId));
	TestEqual(TEXT("automatic replay snapshot migrates"), AfterBattle.AutomaticResolutionQueue.PendingCards[0].CardId, TEXT("Hero.Generic.HeYuZhan"));
	TestEqual(TEXT("spell-task locked IDs migrate"), AfterBattle.HeroSpellTask.LockedHeroCardIds[0], MapLegacyId(BeforeBattle.HeroSpellTask.LockedHeroCardIds[0]));
	TestEqual(TEXT("spell-task completed IDs migrate"), AfterBattle.HeroSpellTask.CompletedHeroCardIds[0], TEXT("Hero.Generic.FengShenBu"));
	TestEqual(TEXT("spell-task first-play snapshot migrates"), AfterBattle.HeroSpellTask.FirstPlayOrder[0].CardId, TEXT("Hero.Generic.FengShenBu"));
	TestEqual(TEXT("spell-task starter reward survives migration"), AfterBattle.HeroSpellTask.StarterReward, EGameXXKHeroSpellTaskReward::Fire);
	TestEqual(TEXT("spell-task starter owner survives migration"), AfterBattle.HeroSpellTask.StarterOwnerUnitId, TEXT("Player"));
	TestEqual(TEXT("retired hidden Medicine is cleared"), StatusStacks(AfterBattle, EGameXXKCardStatus::Medicine), 0);
	TestEqual(TEXT("five legacy Medicine converts to thirty healing bonus and clamps with existing eighty"), StatusStacks(AfterBattle, EGameXXKCardStatus::NextHealingBonus), 99);
	TestNotEqual(TEXT("a pre-v12 battle receives a non-zero independent combat seed"), AfterBattle.CombatRandomState, 0);
	TestEqual(TEXT("combat-seed migration is deterministic"), AfterBattle.CombatRandomState, ActiveMigratedB.RuntimeState.CardRun.ActiveBattle.CombatRandomState);
	FGameXXKSaveState ActiveRoundTrip;
	FGameXXKSaveMigrationReport ActiveRoundTripReport;
	TestTrue(TEXT("the migrated current save roundtrips"), FGameXXKSaveMigration::MigrateToCurrent(ActiveMigratedA, ActiveRoundTrip, ActiveRoundTripReport));
	TestTrue(TEXT("a second migration pass is byte-stable"), FGameXXKSaveState::StaticStruct()->CompareScriptStruct(&ActiveMigratedA, &ActiveRoundTrip, PPF_None));

	FGameXXKRuntimeState RouteState;
	if (!TestTrue(TEXT("the level-twenty legacy route fixture builds"), StartAcceptedRoute(RouteState)))
	{
		return false;
	}
	for (FGameXXKRouteCardEntry& Entry : RouteState.CardRun.RouteCardEntries)
	{
		Entry.CardId = MapCanonicalToLegacy(Entry.CardId);
	}
	RouteState.CardRun.RouteCardIds = {TEXT("Hero.JianYiGuanHong"), TEXT("Hero.GuiYuanFanZhao")};
	RouteState.CardRun.ActiveBattle.Deck.ExhaustPile.Add(MakeInstance(TEXT("Hero.HuiFengZhuiJian"), 90));
	RouteState.CardRun.ActiveBattle.Deck.PendingChoice.Candidates.Add(MakeInstance(TEXT("Hero.PoYunYiShan"), 91));
	RouteState.CardRun.ActiveBattle.LastActiveCard.CardId = TEXT("Hero.GuiYuanFanZhao");
	RouteState.CardRun.ActiveBattle.AutomaticResolutionQueue.PendingCards.Add(MakeSnapshot(TEXT("Hero.JianYiGuanHong"), TEXT("Player")));
	RouteState.CardRun.ActiveBattle.HeroSpellTask.LockedHeroCardIds = {TEXT("Hero.QingFengYiShi")};
	FGameXXKSaveState RouteSource = UGameXXKMVPRules::MakeSaveState(RouteState);
	RouteSource.SaveVersion = 11;
	FGameXXKSaveState RouteMigrated;
	FGameXXKSaveMigrationReport RouteReport;
	if (!TestTrue(TEXT("the v11 level-twenty route migrates"), FGameXXKSaveMigration::MigrateToCurrent(RouteSource, RouteMigrated, RouteReport)))
	{
		AddError(RouteReport.Error);
		return false;
	}
	TestTrue(TEXT("level twenty receives all thirty-six cards in catalog order"), NamesEqual(
		RouteMigrated.RuntimeState.CardRun.HeroUnlockedCardIds, ExpectedUnlocks(20)));
	for (int32 Index = 0; Index < RouteSource.RuntimeState.CardRun.HeroSelectedCardIds.Num(); ++Index)
	{
		TestEqual(TEXT("level-twenty legal selection order is preserved"),
			RouteMigrated.RuntimeState.CardRun.HeroSelectedCardIds[Index],
			MapLegacyId(RouteSource.RuntimeState.CardRun.HeroSelectedCardIds[Index]));
	}
	for (int32 Index = 0; Index < RouteSource.RuntimeState.CardRun.RouteCardEntries.Num(); ++Index)
	{
		TestEqual(TEXT("stable route entry card IDs migrate"),
			RouteMigrated.RuntimeState.CardRun.RouteCardEntries[Index].CardId,
			MapLegacyId(RouteSource.RuntimeState.CardRun.RouteCardEntries[Index].CardId));
	}
	TestEqual(TEXT("legacy route ID zero migrates"), RouteMigrated.RuntimeState.CardRun.RouteCardIds[0], TEXT("Hero.Generic.JianYiGuanHong"));
	TestEqual(TEXT("legacy route ID one migrates"), RouteMigrated.RuntimeState.CardRun.RouteCardIds[1], TEXT("Hero.Generic.GuiYuanFanZhao"));
	TestEqual(TEXT("inactive exhaust card migrates"), RouteMigrated.RuntimeState.CardRun.ActiveBattle.Deck.ExhaustPile[0].CardId, TEXT("Hero.Generic.XingQiHuiHuan"));
	TestEqual(TEXT("inactive pending candidate migrates"), RouteMigrated.RuntimeState.CardRun.ActiveBattle.Deck.PendingChoice.Candidates[0].CardId, TEXT("Hero.Generic.PoYunYiShan"));
	TestEqual(TEXT("inactive last-card snapshot migrates"), RouteMigrated.RuntimeState.CardRun.ActiveBattle.LastActiveCard.CardId, TEXT("Hero.Generic.GuiYuanFanZhao"));
	TestEqual(TEXT("inactive replay snapshot migrates"), RouteMigrated.RuntimeState.CardRun.ActiveBattle.AutomaticResolutionQueue.PendingCards[0].CardId, TEXT("Hero.Generic.JianYiGuanHong"));
	TestTrue(TEXT("pre-v12 inactive stale task progress is cleared"),
		RouteMigrated.RuntimeState.CardRun.ActiveBattle.HeroSpellTask.LockedHeroCardIds.IsEmpty());

	return true;
}

#endif
