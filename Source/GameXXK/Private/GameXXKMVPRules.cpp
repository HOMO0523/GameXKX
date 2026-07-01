#include "GameXXKMVPRules.h"

namespace GameXXKMVP
{
	static const FName RegionQingshanName(TEXT("Region.Qingshan"));
	static const FName RegionHuangshanName(TEXT("Region.Huangshan"));
	static const FName RegionTanjiangName(TEXT("Region.Tanjiang"));
	static const FName ItemHealingPowderName(TEXT("Item.HealingPowder"));
	static const FName ItemIronSwordName(TEXT("Item.IronSword"));
	static const FName ItemClothArmorName(TEXT("Item.ClothArmor"));

	static FGameXXKItemDef MakeItem(FName Id, EGameXXKItemKind Kind, int32 Buy, int32 Sell, int32 Heal, int32 Attack, int32 Defense, int32 MaxHP)
	{
		FGameXXKItemDef Def;
		Def.Id = Id;
		Def.Kind = Kind;
		Def.BuyPrice = Buy;
		Def.SellPrice = Sell;
		Def.HealAmount = Heal;
		Def.AttackBonus = Attack;
		Def.DefenseBonus = Defense;
		Def.MaxHPBonus = MaxHP;
		return Def;
	}

	static bool GetItemDef(FName ItemId, FGameXXKItemDef& OutDef)
	{
		if (ItemId == ItemHealingPowderName)
		{
			OutDef = MakeItem(ItemId, EGameXXKItemKind::Consumable, 10, 5, 35, 0, 0, 0);
			return true;
		}
		if (ItemId == ItemIronSwordName)
		{
			OutDef = MakeItem(ItemId, EGameXXKItemKind::Weapon, 24, 12, 0, 6, 0, 0);
			return true;
		}
		if (ItemId == ItemClothArmorName)
		{
			OutDef = MakeItem(ItemId, EGameXXKItemKind::Armor, 18, 9, 0, 0, 3, 20);
			return true;
		}
		return false;
	}

	static void ApplyXP(FGameXXKRuntimeState& State, int32 XP)
	{
		State.PlayerXP += FMath::Max(0, XP);
		while (State.PlayerXP >= State.PlayerLevel * 100)
		{
			State.PlayerXP -= State.PlayerLevel * 100;
			State.PlayerLevel += 1;
			State.PlayerMaxHP += 20;
			State.PlayerAttack += 3;
			State.PlayerDefense += 2;
			State.PlayerSpeed += 1;
			State.PlayerHP = State.PlayerMaxHP;
		}
	}

	static bool IsDungeonNode(const FGameXXKRuntimeState& State, EGameXXKNodeKind ExpectedNode)
	{
		const TArray<EGameXXKNodeKind> Nodes = UGameXXKMVPRules::GetFixedDungeonNodes(0);
		return State.bDungeonActive && Nodes.IsValidIndex(State.DungeonNodeIndex) && Nodes[State.DungeonNodeIndex] == ExpectedNode;
	}

	static FGameXXKBattleUnit MakeBattleUnit(FName Id, int32 HP, int32 Attack, int32 Defense, int32 Speed, FName Weakness, int32 Shield)
	{
		FGameXXKBattleUnit Unit;
		Unit.Id = Id;
		Unit.HP = HP;
		Unit.Attack = Attack;
		Unit.Defense = Defense;
		Unit.Speed = Speed;
		Unit.Weakness = Weakness;
		Unit.Shield = Shield;
		return Unit;
	}
}

FName UGameXXKMVPRules::RegionQingshan()
{
	return GameXXKMVP::RegionQingshanName;
}

FName UGameXXKMVPRules::RegionHuangshan()
{
	return GameXXKMVP::RegionHuangshanName;
}

FName UGameXXKMVPRules::RegionTanjiang()
{
	return GameXXKMVP::RegionTanjiangName;
}

FName UGameXXKMVPRules::ItemHealingPowder()
{
	return GameXXKMVP::ItemHealingPowderName;
}

FName UGameXXKMVPRules::ItemIronSword()
{
	return GameXXKMVP::ItemIronSwordName;
}

FName UGameXXKMVPRules::ItemClothArmor()
{
	return GameXXKMVP::ItemClothArmorName;
}

FGameXXKRuntimeState UGameXXKMVPRules::CreateNewGame()
{
	FGameXXKRuntimeState State;
	State.Screen = EGameXXKScreen::MainMenu;
	State.CurrentRegion = NAME_None;
	State.UnlockedRegions.Add(RegionQingshan());
	AddItem(State, ItemHealingPowder(), 1);
	return State;
}

bool UGameXXKMVPRules::OpenWorldMap(FGameXXKRuntimeState& State)
{
	State.Screen = EGameXXKScreen::WorldMap;
	State.CurrentRegion = NAME_None;
	return true;
}

bool UGameXXKMVPRules::EnterWorldRegion(FGameXXKRuntimeState& State, FName RegionId)
{
	if (!State.UnlockedRegions.Contains(RegionId))
	{
		return false;
	}
	State.CurrentRegion = RegionId;
	State.Screen = EGameXXKScreen::Town;
	return true;
}

bool UGameXXKMVPRules::AcceptTownQuest(FGameXXKRuntimeState& State)
{
	if (State.Screen != EGameXXKScreen::Town || State.QuestState != EGameXXKQuestState::NotAccepted)
	{
		return false;
	}
	State.QuestState = EGameXXKQuestState::Accepted;
	State.bFollowerJoined = true;
	return true;
}

bool UGameXXKMVPRules::CanEnterDungeon(const FGameXXKRuntimeState& State)
{
	return State.QuestState == EGameXXKQuestState::Accepted;
}

bool UGameXXKMVPRules::EnterDungeon(FGameXXKRuntimeState& State)
{
	if (State.Screen != EGameXXKScreen::Town || State.CurrentRegion != RegionQingshan() || !CanEnterDungeon(State))
	{
		return false;
	}
	State.Screen = EGameXXKScreen::DungeonMap;
	State.CurrentRegion = RegionHuangshan();
	State.bDungeonActive = true;
	State.DungeonNodeIndex = 0;
	return true;
}

bool UGameXXKMVPRules::AdvanceDungeonNode(FGameXXKRuntimeState& State, EGameXXKNodeKind ExpectedNode)
{
	if (!GameXXKMVP::IsDungeonNode(State, ExpectedNode))
	{
		return false;
	}
	if (ExpectedNode == EGameXXKNodeKind::Battle || ExpectedNode == EGameXXKNodeKind::Boss)
	{
		State.Screen = EGameXXKScreen::Battle;
		return true;
	}
	State.DungeonNodeIndex += 1;
	State.Screen = EGameXXKScreen::DungeonMap;
	return true;
}

bool UGameXXKMVPRules::ResolveBattleVictory(FGameXXKRuntimeState& State, bool bBossBattle)
{
	if (State.Screen != EGameXXKScreen::Battle)
	{
		return false;
	}
	if (bBossBattle)
	{
		if (!GameXXKMVP::IsDungeonNode(State, EGameXXKNodeKind::Boss))
		{
			return false;
		}
		GameXXKMVP::ApplyXP(State, 150);
		State.PlayerGold += 35;
		AddItem(State, ItemClothArmor(), 1);
		return ResolveBossClear(State);
	}
	if (!GameXXKMVP::IsDungeonNode(State, EGameXXKNodeKind::Battle))
	{
		return false;
	}
	GameXXKMVP::ApplyXP(State, 80);
	State.PlayerGold += 18;
	AddItem(State, ItemHealingPowder(), 1);
	State.DungeonNodeIndex += 1;
	State.Screen = EGameXXKScreen::DungeonMap;
	return true;
}

bool UGameXXKMVPRules::ResolveEventReward(FGameXXKRuntimeState& State, bool bTakeGold)
{
	if (!GameXXKMVP::IsDungeonNode(State, EGameXXKNodeKind::Event))
	{
		return false;
	}
	if (bTakeGold)
	{
		State.PlayerGold += 12;
	}
	else
	{
		AddItem(State, ItemHealingPowder(), 1);
	}
	State.DungeonNodeIndex += 1;
	return true;
}

bool UGameXXKMVPRules::ResolveCampReward(FGameXXKRuntimeState& State, bool bHealNow)
{
	if (!GameXXKMVP::IsDungeonNode(State, EGameXXKNodeKind::Camp))
	{
		return false;
	}
	if (bHealNow)
	{
		State.PlayerHP = State.PlayerMaxHP;
	}
	else
	{
		AddItem(State, ItemHealingPowder(), 1);
	}
	State.DungeonNodeIndex += 1;
	return true;
}

bool UGameXXKMVPRules::FailDungeonToTown(FGameXXKRuntimeState& State)
{
	if (!State.bDungeonActive)
	{
		return false;
	}
	State.Screen = EGameXXKScreen::Town;
	State.CurrentRegion = RegionQingshan();
	State.bDungeonActive = false;
	State.DungeonNodeIndex = 0;
	State.bFollowerJoined = State.QuestState == EGameXXKQuestState::Accepted;
	return true;
}

bool UGameXXKMVPRules::ResolveBossClear(FGameXXKRuntimeState& State)
{
	if (!State.bDungeonActive || State.QuestState != EGameXXKQuestState::Accepted)
	{
		return false;
	}
	State.QuestState = EGameXXKQuestState::Completed;
	State.bFollowerJoined = false;
	State.bDungeonActive = false;
	State.DungeonNodeIndex = 0;
	State.Screen = EGameXXKScreen::WorldMap;
	State.CurrentRegion = NAME_None;
	State.UnlockedRegions.Add(RegionTanjiang());
	return true;
}

TArray<EGameXXKNodeKind> UGameXXKMVPRules::GetFixedDungeonNodes(int32 Seed)
{
	const bool bEventBeforeCamp = (Seed % 2) == 0;
	return bEventBeforeCamp
		? TArray<EGameXXKNodeKind>{EGameXXKNodeKind::Start, EGameXXKNodeKind::Battle, EGameXXKNodeKind::Event, EGameXXKNodeKind::Camp, EGameXXKNodeKind::Boss}
		: TArray<EGameXXKNodeKind>{EGameXXKNodeKind::Start, EGameXXKNodeKind::Battle, EGameXXKNodeKind::Camp, EGameXXKNodeKind::Event, EGameXXKNodeKind::Boss};
}

bool UGameXXKMVPRules::AddItem(FGameXXKRuntimeState& State, FName ItemId, int32 Quantity)
{
	FGameXXKItemDef Def;
	if (Quantity <= 0 || !GameXXKMVP::GetItemDef(ItemId, Def))
	{
		return false;
	}
	State.Inventory.FindOrAdd(ItemId) += Quantity;
	return true;
}

bool UGameXXKMVPRules::RemoveItem(FGameXXKRuntimeState& State, FName ItemId, int32 Quantity)
{
	if (Quantity <= 0)
	{
		return false;
	}
	int32* Count = State.Inventory.Find(ItemId);
	if (!Count || *Count < Quantity)
	{
		return false;
	}
	*Count -= Quantity;
	if (*Count <= 0)
	{
		State.Inventory.Remove(ItemId);
	}
	return true;
}

int32 UGameXXKMVPRules::GetItemCount(const FGameXXKRuntimeState& State, FName ItemId)
{
	return State.Inventory.FindRef(ItemId);
}

bool UGameXXKMVPRules::BuyItem(FGameXXKRuntimeState& State, FName ItemId, int32 Quantity)
{
	FGameXXKItemDef Def;
	if (Quantity <= 0 || !GameXXKMVP::GetItemDef(ItemId, Def))
	{
		return false;
	}
	const int32 Total = Def.BuyPrice * Quantity;
	if (State.PlayerGold < Total)
	{
		return false;
	}
	State.PlayerGold -= Total;
	return AddItem(State, ItemId, Quantity);
}

bool UGameXXKMVPRules::SellItem(FGameXXKRuntimeState& State, FName ItemId, int32 Quantity)
{
	FGameXXKItemDef Def;
	if (Quantity <= 0 || !GameXXKMVP::GetItemDef(ItemId, Def) || !RemoveItem(State, ItemId, Quantity))
	{
		return false;
	}
	State.PlayerGold += Def.SellPrice * Quantity;
	return true;
}

bool UGameXXKMVPRules::EquipItem(FGameXXKRuntimeState& State, FName ItemId)
{
	FGameXXKItemDef Def;
	if (!GameXXKMVP::GetItemDef(ItemId, Def) || GetItemCount(State, ItemId) <= 0)
	{
		return false;
	}
	if (Def.Kind == EGameXXKItemKind::Weapon)
	{
		State.EquippedWeapon = ItemId;
		State.PlayerAttack += Def.AttackBonus;
		return true;
	}
	if (Def.Kind == EGameXXKItemKind::Armor)
	{
		State.EquippedArmor = ItemId;
		State.PlayerDefense += Def.DefenseBonus;
		State.PlayerMaxHP += Def.MaxHPBonus;
		State.PlayerHP += Def.MaxHPBonus;
		return true;
	}
	return false;
}

bool UGameXXKMVPRules::UseHealingItem(FGameXXKRuntimeState& State)
{
	FGameXXKItemDef Def;
	if (State.PlayerHP >= State.PlayerMaxHP || !GameXXKMVP::GetItemDef(ItemHealingPowder(), Def) || !RemoveItem(State, ItemHealingPowder(), 1))
	{
		return false;
	}
	State.PlayerHP = FMath::Min(State.PlayerMaxHP, State.PlayerHP + Def.HealAmount);
	return true;
}

TArray<FName> UGameXXKMVPRules::BuildTurnOrder(const FGameXXKRuntimeState& State, bool bBossBattle)
{
	TArray<FGameXXKBattleUnit> Units;
	Units.Add(GameXXKMVP::MakeBattleUnit(TEXT("Player"), State.PlayerHP, State.PlayerAttack, State.PlayerDefense, State.PlayerSpeed, TEXT("Sword"), 1));
	if (State.bFollowerJoined)
	{
		Units.Add(GameXXKMVP::MakeBattleUnit(TEXT("Follower"), 80, 9, 4, 8, TEXT("Sword"), 1));
	}
	if (bBossBattle)
	{
		Units.Add(GameXXKMVP::MakeBattleUnit(TEXT("Boss"), 180, 18, 8, 9, TEXT("Sword"), 3));
	}
	else
	{
		Units.Add(GameXXKMVP::MakeBattleUnit(TEXT("Bandit"), 60, 10, 3, 7, TEXT("Sword"), 1));
		Units.Add(GameXXKMVP::MakeBattleUnit(TEXT("Wolf"), 45, 8, 2, 12, TEXT("Bow"), 1));
	}

	Units.Sort([](const FGameXXKBattleUnit& A, const FGameXXKBattleUnit& B)
	{
		if (A.Speed == B.Speed)
		{
			return A.Id.LexicalLess(B.Id);
		}
		return A.Speed > B.Speed;
	});

	TArray<FName> Order;
	for (const FGameXXKBattleUnit& Unit : Units)
	{
		Order.Add(Unit.Id);
	}
	return Order;
}

FGameXXKSaveState UGameXXKMVPRules::MakeSaveState(const FGameXXKRuntimeState& State)
{
	FGameXXKSaveState SaveState;
	SaveState.QuestState = State.QuestState;
	SaveState.PlayerLevel = State.PlayerLevel;
	SaveState.PlayerXP = State.PlayerXP;
	SaveState.PlayerGold = State.PlayerGold;
	SaveState.UnlockedRegions = State.UnlockedRegions;
	return SaveState;
}

FGameXXKRuntimeState UGameXXKMVPRules::RestoreFromSaveState(const FGameXXKSaveState& SaveState)
{
	FGameXXKRuntimeState State = CreateNewGame();
	State.Screen = EGameXXKScreen::MainMenu;
	State.QuestState = SaveState.QuestState;
	State.PlayerLevel = SaveState.PlayerLevel;
	State.PlayerXP = SaveState.PlayerXP;
	State.PlayerGold = SaveState.PlayerGold;
	State.UnlockedRegions = SaveState.UnlockedRegions;
	State.Inventory.Reset();
	State.EquippedWeapon = NAME_None;
	State.EquippedArmor = NAME_None;
	State.bFollowerJoined = SaveState.QuestState == EGameXXKQuestState::Accepted;
	return State;
}
