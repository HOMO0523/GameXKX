#include "GameXXKMVPRules.h"

namespace GameXXKMVP
{
	static const FName RegionQingshanName(TEXT("Region.Qingshan"));
	static const FName RegionHuangshanName(TEXT("Region.Huangshan"));
	static const FName RegionTanjiangName(TEXT("Region.Tanjiang"));
	static const FName ItemHealingPowderName(TEXT("Item.HealingPowder"));
	static const FName ItemLingzhiPowderName(TEXT("Item.LingzhiPowder"));
	static const FName ItemQingxinTeaName(TEXT("Item.QingxinTea"));
	static const FName ItemCraneSachetName(TEXT("Item.CraneSachet"));
	static const FName ItemIronSwordName(TEXT("Item.IronSword"));
	static const FName ItemClothArmorName(TEXT("Item.ClothArmor"));
	static const FName ItemCranePatternTalismanName(TEXT("Item.CranePatternTalisman"));
	static const FName ItemInkstonePendantName(TEXT("Item.InkstonePendant"));
	static const FName ItemWoodenSwordName(TEXT("Item.WoodenSword"));
	static const FName ItemStarterClothArmorName(TEXT("Item.StarterClothArmor"));
	static const FName ItemClothTalismanName(TEXT("Item.ClothTalisman"));

	static FGameXXKItemDef MakeItem(FName Id, const TCHAR* DisplayName, EGameXXKItemKind Kind, int32 Buy, int32 Sell, int32 Heal, int32 MPHeal, int32 Attack, int32 Defense, int32 MaxHP, int32 MaxMP)
	{
		FGameXXKItemDef Def;
		Def.Id = Id;
		Def.DisplayName = FText::FromString(DisplayName);
		Def.Kind = Kind;
		Def.BuyPrice = Buy;
		Def.SellPrice = Sell;
		Def.HealAmount = Heal;
		Def.MPHealAmount = MPHeal;
		Def.AttackBonus = Attack;
		Def.DefenseBonus = Defense;
		Def.MaxHPBonus = MaxHP;
		Def.MaxMPBonus = MaxMP;
		return Def;
	}

	static TArray<FName> GetKnownItemIds()
	{
		return {
			ItemHealingPowderName,
			ItemLingzhiPowderName,
			ItemQingxinTeaName,
			ItemCraneSachetName,
			ItemIronSwordName,
			ItemClothArmorName,
			ItemCranePatternTalismanName,
			ItemInkstonePendantName,
			ItemWoodenSwordName,
			ItemStarterClothArmorName,
			ItemClothTalismanName,
		};
	}

	static TArray<FName> GetShopItemIds()
	{
		return {
			ItemHealingPowderName,
			ItemLingzhiPowderName,
			ItemQingxinTeaName,
			ItemCraneSachetName,
			ItemIronSwordName,
			ItemClothArmorName,
			ItemCranePatternTalismanName,
			ItemInkstonePendantName,
		};
	}

	static bool GetItemDef(FName ItemId, FGameXXKItemDef& OutDef)
	{
		if (ItemId == ItemHealingPowderName)
		{
			OutDef = MakeItem(ItemId, TEXT("金疮药"), EGameXXKItemKind::Consumable, 10, 5, 30, 0, 0, 0, 0, 0);
			return true;
		}
		if (ItemId == ItemLingzhiPowderName)
		{
			OutDef = MakeItem(ItemId, TEXT("灵芝散"), EGameXXKItemKind::Consumable, 28, 14, 80, 0, 0, 0, 0, 0);
			return true;
		}
		if (ItemId == ItemQingxinTeaName)
		{
			OutDef = MakeItem(ItemId, TEXT("清心茶"), EGameXXKItemKind::Consumable, 16, 8, 0, 20, 0, 0, 0, 0);
			return true;
		}
		if (ItemId == ItemCraneSachetName)
		{
			OutDef = MakeItem(ItemId, TEXT("鹤羽香囊"), EGameXXKItemKind::Consumable, 22, 11, 0, 0, 0, 0, 0, 0);
			return true;
		}
		if (ItemId == ItemIronSwordName)
		{
			OutDef = MakeItem(ItemId, TEXT("青锋短剑"), EGameXXKItemKind::Weapon, 48, 24, 0, 0, 8, 0, 0, 0);
			return true;
		}
		if (ItemId == ItemClothArmorName)
		{
			OutDef = MakeItem(ItemId, TEXT("竹编轻甲"), EGameXXKItemKind::Armor, 36, 18, 0, 0, 0, 6, 0, 0);
			return true;
		}
		if (ItemId == ItemCranePatternTalismanName)
		{
			OutDef = MakeItem(ItemId, TEXT("鹤纹护符"), EGameXXKItemKind::Accessory, 32, 16, 0, 0, 0, 0, 30, 0);
			return true;
		}
		if (ItemId == ItemInkstonePendantName)
		{
			OutDef = MakeItem(ItemId, TEXT("墨砚坠饰"), EGameXXKItemKind::Accessory, 32, 16, 0, 0, 0, 0, 0, 20);
			return true;
		}
		if (ItemId == ItemWoodenSwordName)
		{
			OutDef = MakeItem(ItemId, TEXT("木剑"), EGameXXKItemKind::Weapon, 18, 9, 0, 0, 3, 0, 0, 0);
			return true;
		}
		if (ItemId == ItemStarterClothArmorName)
		{
			OutDef = MakeItem(ItemId, TEXT("布甲"), EGameXXKItemKind::Armor, 16, 8, 0, 0, 0, 3, 0, 0);
			return true;
		}
		if (ItemId == ItemClothTalismanName)
		{
			OutDef = MakeItem(ItemId, TEXT("布护符"), EGameXXKItemKind::Accessory, 14, 7, 0, 0, 0, 0, 10, 0);
			return true;
		}
		return false;
	}

	static int32 GetBaseMaxHP(int32 Level)
	{
		return 100 + FMath::Max(0, Level - 1) * 15;
	}

	static int32 GetBaseMaxMP(int32 Level)
	{
		return 30 + FMath::Max(0, Level - 1) * 5;
	}

	static int32 GetBaseAttack(int32 Level)
	{
		return 15 + FMath::Max(0, Level - 1) * 3;
	}

	static int32 GetBaseDefense(int32 Level)
	{
		return 8 + FMath::Max(0, Level - 1) * 2;
	}

	static int32 GetBaseSpeed(int32 Level)
	{
		return 10 + FMath::Max(0, Level - 1);
	}

	static void AddEquipmentBonuses(const FGameXXKRuntimeState& State, FName ItemId, int32& Attack, int32& Defense, int32& MaxHP, int32& MaxMP)
	{
		if (ItemId.IsNone() || State.Inventory.FindRef(ItemId) <= 0)
		{
			return;
		}
		FGameXXKItemDef Def;
		if (!GetItemDef(ItemId, Def))
		{
			return;
		}
		Attack += Def.AttackBonus;
		Defense += Def.DefenseBonus;
		MaxHP += Def.MaxHPBonus;
		MaxMP += Def.MaxMPBonus;
	}

	static void RecalculatePlayerStats(FGameXXKRuntimeState& State, bool bPreserveMissingResources = true)
	{
		const int32 OldMaxHP = FMath::Max(1, State.PlayerMaxHP);
		const int32 OldMaxMP = FMath::Max(1, State.PlayerMaxMP);
		const int32 MissingHP = bPreserveMissingResources ? FMath::Max(0, OldMaxHP - State.PlayerHP) : 0;
		const int32 MissingMP = bPreserveMissingResources ? FMath::Max(0, OldMaxMP - State.PlayerMP) : 0;

		State.PlayerMaxHP = GetBaseMaxHP(State.PlayerLevel);
		State.PlayerMaxMP = GetBaseMaxMP(State.PlayerLevel);
		State.PlayerAttack = GetBaseAttack(State.PlayerLevel);
		State.PlayerDefense = GetBaseDefense(State.PlayerLevel);
		State.PlayerSpeed = GetBaseSpeed(State.PlayerLevel);

		if (State.Inventory.FindRef(State.EquippedWeapon) <= 0)
		{
			State.EquippedWeapon = NAME_None;
		}
		if (State.Inventory.FindRef(State.EquippedArmor) <= 0)
		{
			State.EquippedArmor = NAME_None;
		}
		if (State.Inventory.FindRef(State.EquippedAccessory) <= 0)
		{
			State.EquippedAccessory = NAME_None;
		}

		AddEquipmentBonuses(State, State.EquippedWeapon, State.PlayerAttack, State.PlayerDefense, State.PlayerMaxHP, State.PlayerMaxMP);
		AddEquipmentBonuses(State, State.EquippedArmor, State.PlayerAttack, State.PlayerDefense, State.PlayerMaxHP, State.PlayerMaxMP);
		AddEquipmentBonuses(State, State.EquippedAccessory, State.PlayerAttack, State.PlayerDefense, State.PlayerMaxHP, State.PlayerMaxMP);

		State.PlayerHP = FMath::Clamp(State.PlayerMaxHP - MissingHP, 0, State.PlayerMaxHP);
		State.PlayerMP = FMath::Clamp(State.PlayerMaxMP - MissingMP, 0, State.PlayerMaxMP);
	}

	static void ApplyXP(FGameXXKRuntimeState& State, int32 XP)
	{
		State.PlayerXP += FMath::Max(0, XP);
		bool bLeveled = false;
		while (State.PlayerXP >= State.PlayerLevel * 100)
		{
			State.PlayerXP -= State.PlayerLevel * 100;
			State.PlayerLevel += 1;
			bLeveled = true;
		}
		if (bLeveled)
		{
			RecalculatePlayerStats(State, false);
			State.PlayerHP = State.PlayerMaxHP;
			State.PlayerMP = State.PlayerMaxMP;
		}
	}

	static const FGameXXKRouteMapNode* FindRouteNode(const FGameXXKRuntimeState& State, int32 NodeId)
	{
		return State.RouteMapNodes.FindByPredicate([NodeId](const FGameXXKRouteMapNode& Node)
		{
			return Node.NodeId == NodeId;
		});
	}

	static void AddUniqueInt(TArray<int32>& Values, int32 Value)
	{
		if (!Values.Contains(Value))
		{
			Values.Add(Value);
		}
	}

	static void RemoveInt(TArray<int32>& Values, int32 Value)
	{
		Values.Remove(Value);
	}

	static int32 NormalizeRouteSeed(int32 Seed)
	{
		if (Seed == 0 || Seed == MIN_int32)
		{
			return 1;
		}
		return FMath::Abs(Seed);
	}

	static int32 MakeNewRouteSeed()
	{
		const int32 Seed = FMath::Rand();
		return NormalizeRouteSeed(Seed);
	}

	static void AddRouteNode(FGameXXKRuntimeState& State, int32 NodeId, int32 LayerIndex, int32 ColumnIndex, EGameXXKNodeKind NodeKind, float X, float Y, TArray<int32> OutgoingNodeIds)
	{
		State.RouteMapNodes.Emplace(NodeId, LayerIndex, ColumnIndex, NodeKind, FVector2D(X, Y), OutgoingNodeIds);
		for (int32 ToNodeId : OutgoingNodeIds)
		{
			State.RouteMapEdges.Emplace(NodeId, ToNodeId);
		}
	}

	static FGameXXKRouteMapNode* FindMutableRouteNode(FGameXXKRuntimeState& State, int32 NodeId)
	{
		return State.RouteMapNodes.FindByPredicate([NodeId](const FGameXXKRouteMapNode& Node)
		{
			return Node.NodeId == NodeId;
		});
	}

	static int32 GetGeneratedLayerNodeCount(FRandomStream& Stream, int32 LayerIndex)
	{
		switch (LayerIndex)
		{
		case 1:
			return 2 + Stream.RandRange(0, 1);
		case 2:
		case 3:
		case 4:
			return 3 + Stream.RandRange(0, 1);
		case 5:
			return 2 + Stream.RandRange(0, 1);
		default:
			return 1;
		}
	}

	static EGameXXKNodeKind PickGeneratedRouteNodeKind(FRandomStream& Stream, int32 LayerIndex, int32 ColumnIndex)
	{
		const int32 Roll = Stream.RandRange(0, 99);
		if (LayerIndex == 1)
		{
			return EGameXXKNodeKind::Battle;
		}
		if (LayerIndex == 5)
		{
			if (ColumnIndex == 0)
			{
				return EGameXXKNodeKind::Camp;
			}
			return Roll < 45 ? EGameXXKNodeKind::Battle : (Roll < 72 ? EGameXXKNodeKind::Merchant : EGameXXKNodeKind::Elite);
		}
		if (Roll < 44)
		{
			return EGameXXKNodeKind::Battle;
		}
		if (Roll < 58)
		{
			return EGameXXKNodeKind::Elite;
		}
		if (Roll < 72)
		{
			return EGameXXKNodeKind::Event;
		}
		if (Roll < 84)
		{
			return EGameXXKNodeKind::Chest;
		}
		if (Roll < 93)
		{
			return EGameXXKNodeKind::Merchant;
		}
		return EGameXXKNodeKind::Camp;
	}

	static TArray<int32> GetRouteNodeIdsInLayer(const FGameXXKRuntimeState& State, int32 LayerIndex)
	{
		TArray<int32> NodeIds;
		for (const FGameXXKRouteMapNode& Node : State.RouteMapNodes)
		{
			if (Node.LayerIndex == LayerIndex)
			{
				NodeIds.Add(Node.NodeId);
			}
		}
		NodeIds.Sort([&State](int32 LeftNodeId, int32 RightNodeId)
		{
			const FGameXXKRouteMapNode* LeftNode = FindRouteNode(State, LeftNodeId);
			const FGameXXKRouteMapNode* RightNode = FindRouteNode(State, RightNodeId);
			const int32 LeftColumn = LeftNode ? LeftNode->ColumnIndex : 0;
			const int32 RightColumn = RightNode ? RightNode->ColumnIndex : 0;
			return LeftColumn < RightColumn;
		});
		return NodeIds;
	}

	static int32 CountIncomingRouteEdges(const FGameXXKRuntimeState& State, int32 NodeId)
	{
		int32 Count = 0;
		for (const FGameXXKRouteMapEdge& Edge : State.RouteMapEdges)
		{
			if (Edge.ToNodeId == NodeId)
			{
				++Count;
			}
		}
		return Count;
	}

	static bool HasRouteEdge(const FGameXXKRuntimeState& State, int32 FromNodeId, int32 ToNodeId)
	{
		return State.RouteMapEdges.ContainsByPredicate([FromNodeId, ToNodeId](const FGameXXKRouteMapEdge& Edge)
		{
			return Edge.FromNodeId == FromNodeId && Edge.ToNodeId == ToNodeId;
		});
	}

	static void AddUniqueRouteEdge(FGameXXKRuntimeState& State, int32 FromNodeId, int32 ToNodeId)
	{
		FGameXXKRouteMapNode* FromNode = FindMutableRouteNode(State, FromNodeId);
		if (!FromNode || !FindRouteNode(State, ToNodeId) || HasRouteEdge(State, FromNodeId, ToNodeId))
		{
			return;
		}
		FromNode->OutgoingNodeIds.Add(ToNodeId);
		State.RouteMapEdges.Emplace(FromNodeId, ToNodeId);
	}

	static int32 PickClosestTargetNode(
		const FGameXXKRuntimeState& State,
		int32 FromNodeId,
		const TArray<int32>& CandidateNodeIds,
		int32 MaxIncomingEdges,
		bool bRespectIncomingLimit)
	{
		const FGameXXKRouteMapNode* FromNode = FindRouteNode(State, FromNodeId);
		if (!FromNode)
		{
			return INDEX_NONE;
		}

		int32 BestNodeId = INDEX_NONE;
		float BestScore = TNumericLimits<float>::Max();
		for (int32 CandidateNodeId : CandidateNodeIds)
		{
			if (HasRouteEdge(State, FromNodeId, CandidateNodeId))
			{
				continue;
			}
			if (bRespectIncomingLimit && CountIncomingRouteEdges(State, CandidateNodeId) >= MaxIncomingEdges)
			{
				continue;
			}
			const FGameXXKRouteMapNode* CandidateNode = FindRouteNode(State, CandidateNodeId);
			if (!CandidateNode)
			{
				continue;
			}
			const float Score = FMath::Abs(FromNode->NormalizedPosition.X - CandidateNode->NormalizedPosition.X);
			if (Score < BestScore)
			{
				BestScore = Score;
				BestNodeId = CandidateNodeId;
			}
		}
		return BestNodeId;
	}

	static int32 PickClosestSourceNode(
		const FGameXXKRuntimeState& State,
		int32 ToNodeId,
		const TArray<int32>& CandidateNodeIds,
		int32 MaxOutgoingEdges)
	{
		const FGameXXKRouteMapNode* ToNode = FindRouteNode(State, ToNodeId);
		if (!ToNode)
		{
			return INDEX_NONE;
		}

		int32 BestNodeId = INDEX_NONE;
		float BestScore = TNumericLimits<float>::Max();
		for (int32 CandidateNodeId : CandidateNodeIds)
		{
			const FGameXXKRouteMapNode* CandidateNode = FindRouteNode(State, CandidateNodeId);
			if (!CandidateNode || CandidateNode->OutgoingNodeIds.Num() >= MaxOutgoingEdges || HasRouteEdge(State, CandidateNodeId, ToNodeId))
			{
				continue;
			}
			const float Score = FMath::Abs(CandidateNode->NormalizedPosition.X - ToNode->NormalizedPosition.X);
			if (Score < BestScore)
			{
				BestScore = Score;
				BestNodeId = CandidateNodeId;
			}
		}
		return BestNodeId;
	}

	static void ConnectGeneratedRouteLayer(FGameXXKRuntimeState& State, FRandomStream& Stream, int32 LayerIndex)
	{
		static constexpr int32 MaxOutgoingEdges = 3;
		static constexpr int32 MaxIncomingEdges = 2;

		const TArray<int32> SourceNodeIds = GetRouteNodeIdsInLayer(State, LayerIndex);
		const TArray<int32> TargetNodeIds = GetRouteNodeIdsInLayer(State, LayerIndex + 1);
		if (SourceNodeIds.IsEmpty() || TargetNodeIds.IsEmpty())
		{
			return;
		}

		const bool bTargetIsBossLayer = TargetNodeIds.Num() == 1;
		for (int32 SourceNodeId : SourceNodeIds)
		{
			int32 TargetNodeId = PickClosestTargetNode(State, SourceNodeId, TargetNodeIds, MaxIncomingEdges, !bTargetIsBossLayer);
			if (TargetNodeId == INDEX_NONE)
			{
				TargetNodeId = PickClosestTargetNode(State, SourceNodeId, TargetNodeIds, MaxIncomingEdges, false);
			}
			AddUniqueRouteEdge(State, SourceNodeId, TargetNodeId);
		}

		for (int32 TargetNodeId : TargetNodeIds)
		{
			if (CountIncomingRouteEdges(State, TargetNodeId) > 0)
			{
				continue;
			}
			int32 SourceNodeId = PickClosestSourceNode(State, TargetNodeId, SourceNodeIds, MaxOutgoingEdges);
			if (SourceNodeId == INDEX_NONE)
			{
				SourceNodeId = SourceNodeIds[Stream.RandRange(0, SourceNodeIds.Num() - 1)];
			}
			AddUniqueRouteEdge(State, SourceNodeId, TargetNodeId);
		}

		for (int32 SourceNodeId : SourceNodeIds)
		{
			FGameXXKRouteMapNode* SourceNode = FindMutableRouteNode(State, SourceNodeId);
			if (!SourceNode || SourceNode->OutgoingNodeIds.Num() >= MaxOutgoingEdges || Stream.RandRange(0, 99) >= 45)
			{
				continue;
			}
			const bool bRespectIncomingLimit = !bTargetIsBossLayer;
			const int32 TargetNodeId = PickClosestTargetNode(State, SourceNodeId, TargetNodeIds, MaxIncomingEdges, bRespectIncomingLimit);
			AddUniqueRouteEdge(State, SourceNodeId, TargetNodeId);
		}
	}

	static void GenerateRouteMap(FGameXXKRuntimeState& State)
	{
		const int32 Seed = State.RouteSeed != 0 ? State.RouteSeed : MakeNewRouteSeed();
		UGameXXKMVPRules::GenerateRouteMapForSeed(State, Seed);
	}

	static bool CompleteRouteNode(FGameXXKRuntimeState& State, const FGameXXKRouteMapNode& Node)
	{
		AddUniqueInt(State.VisitedRouteNodeIds, Node.NodeId);
		State.ReachableRouteNodeIds.Reset();
		for (int32 OutgoingNodeId : Node.OutgoingNodeIds)
		{
			if (!State.VisitedRouteNodeIds.Contains(OutgoingNodeId))
			{
				AddUniqueInt(State.ReachableRouteNodeIds, OutgoingNodeId);
			}
		}
		State.PendingRouteNodeId = INDEX_NONE;
		State.CurrentRouteNodeId = State.ReachableRouteNodeIds.IsEmpty() ? INDEX_NONE : State.ReachableRouteNodeIds[0];
		State.DungeonNodeIndex = State.VisitedRouteNodeIds.Num();
		State.Screen = EGameXXKScreen::DungeonMap;
		State.CurrentMapId = TEXT("HuangshanRoute");
		State.TownPanelMode = EGameXXKTownPanelMode::None;
		return true;
	}

	static const FGameXXKRouteMapNode* FindFirstReachableRouteNodeOfKind(const FGameXXKRuntimeState& State, EGameXXKNodeKind NodeKind)
	{
		for (int32 NodeId : State.ReachableRouteNodeIds)
		{
			const FGameXXKRouteMapNode* Node = FindRouteNode(State, NodeId);
			if (Node && Node->NodeKind == NodeKind)
			{
				return Node;
			}
		}
		return nullptr;
	}

	static const FGameXXKRouteMapNode* FindPendingRouteNode(const FGameXXKRuntimeState& State)
	{
		return FindRouteNode(State, State.PendingRouteNodeId);
	}

	static bool IsDungeonNode(const FGameXXKRuntimeState& State, EGameXXKNodeKind ExpectedNode)
	{
		if (State.bHasGeneratedRouteMap)
		{
			const FGameXXKRouteMapNode* PendingNode = FindPendingRouteNode(State);
			if (PendingNode)
			{
				return State.bDungeonActive && PendingNode->NodeKind == ExpectedNode;
			}
			return State.bDungeonActive && FindFirstReachableRouteNodeOfKind(State, ExpectedNode) != nullptr;
		}
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

	static FGameXXKBattleRuntimeUnit MakeBattleRuntimeUnit(FName Id, const TCHAR* DisplayName, int32 HP, int32 Attack, int32 Defense, int32 Speed, int32 Shield, bool bEnemy, int32 MP = 0, int32 MaxMP = 0)
	{
		FGameXXKBattleRuntimeUnit Unit;
		Unit.Id = Id;
		Unit.DisplayName = FText::FromString(DisplayName);
		Unit.HP = HP;
		Unit.MaxHP = HP;
		Unit.MP = FMath::Clamp(MP, 0, FMath::Max(0, MaxMP));
		Unit.MaxMP = FMath::Max(0, MaxMP);
		Unit.Attack = Attack;
		Unit.Defense = Defense;
		Unit.Speed = Speed;
		Unit.Shield = Shield;
		Unit.bEnemy = bEnemy;
		Unit.bDefeated = HP <= 0;
		return Unit;
	}

	static void ClearActiveBattle(FGameXXKRuntimeState& State)
	{
		State.bHasActiveBattle = false;
		State.ActiveBattleNodeId = INDEX_NONE;
		State.ActiveBattleEnemies.Reset();
		State.ActiveBattleParty.Reset();
	}

	static void BuildPartySnapshot(FGameXXKRuntimeState& State)
	{
		State.ActiveBattleParty.Reset();
		if (State.PlayerMaxMP <= 0)
		{
			RecalculatePlayerStats(State, true);
		}
		if (State.PlayerMP <= 0)
		{
			State.PlayerMP = State.PlayerMaxMP;
		}
		State.PlayerMP = FMath::Clamp(State.PlayerMP, 0, State.PlayerMaxMP);
		State.ActiveBattleParty.Add(MakeBattleRuntimeUnit(TEXT("Player"), TEXT("Hero"), State.PlayerHP, State.PlayerAttack, State.PlayerDefense, State.PlayerSpeed, 1, false, State.PlayerMP, State.PlayerMaxMP));
		if (State.bFollowerJoined)
		{
			State.ActiveBattleParty.Add(MakeBattleRuntimeUnit(TEXT("Follower"), TEXT("Follower"), 80, 9, 4, 8, 1, false, 20, 20));
		}
	}

	static bool BeginBattle(FGameXXKRuntimeState& State, EGameXXKNodeKind NodeKind, int32 NodeId)
	{
		if (NodeKind != EGameXXKNodeKind::Battle && NodeKind != EGameXXKNodeKind::Elite && NodeKind != EGameXXKNodeKind::Boss)
		{
			return false;
		}

		ClearActiveBattle(State);
		State.bHasActiveBattle = true;
		State.ActiveBattleNodeId = NodeId;
		BuildPartySnapshot(State);

		if (NodeKind == EGameXXKNodeKind::Boss)
		{
			State.ActiveBattleEnemies.Add(MakeBattleRuntimeUnit(TEXT("Boss"), TEXT("Boss"), 180, 18, 8, 9, 3, true));
		}
		else if (NodeKind == EGameXXKNodeKind::Elite)
		{
			State.ActiveBattleEnemies.Add(MakeBattleRuntimeUnit(TEXT("EliteBandit"), TEXT("Elite Bandit"), 96, 15, 5, 9, 2, true));
			State.ActiveBattleEnemies.Add(MakeBattleRuntimeUnit(TEXT("Wolf"), TEXT("Wolf"), 54, 10, 3, 12, 1, true));
		}
		else
		{
			State.ActiveBattleEnemies.Add(MakeBattleRuntimeUnit(TEXT("Bandit"), TEXT("Bandit"), 60, 10, 3, 7, 1, true));
			State.ActiveBattleEnemies.Add(MakeBattleRuntimeUnit(TEXT("Wolf"), TEXT("Wolf"), 45, 8, 2, 12, 1, true));
		}

		State.Screen = EGameXXKScreen::Battle;
		State.CurrentMapId = TEXT("Battle");
		State.TownPanelMode = EGameXXKTownPanelMode::None;
		return true;
	}

	static bool AreAllEnemiesDefeated(const FGameXXKRuntimeState& State)
	{
		return !State.ActiveBattleEnemies.IsEmpty()
			&& !State.ActiveBattleEnemies.ContainsByPredicate([](const FGameXXKBattleRuntimeUnit& Enemy)
			{
				return Enemy.bEnemy && !Enemy.bDefeated;
			});
	}

	static bool AreAllPartyMembersDefeated(const FGameXXKRuntimeState& State)
	{
		return !State.ActiveBattleParty.IsEmpty()
			&& !State.ActiveBattleParty.ContainsByPredicate([](const FGameXXKBattleRuntimeUnit& PartyMember)
			{
				return !PartyMember.bEnemy && !PartyMember.bDefeated;
			});
	}

	static bool IsHeroDefeated(const FGameXXKRuntimeState& State)
	{
		return State.ActiveBattleParty.IsValidIndex(0)
			&& (State.ActiveBattleParty[0].bDefeated || State.ActiveBattleParty[0].HP <= 0);
	}

	static bool FailBattleToTownWithRestoredHero(FGameXXKRuntimeState& State)
	{
		State.PlayerHP = State.PlayerMaxHP;
		if (State.ActiveBattleParty.IsValidIndex(0))
		{
			State.ActiveBattleParty[0].HP = State.ActiveBattleParty[0].MaxHP;
			State.ActiveBattleParty[0].bDefeated = false;
		}
		return UGameXXKMVPRules::FailDungeonToTown(State);
	}

	static void SyncPlayerFromBattle(FGameXXKRuntimeState& State)
	{
		if (!State.ActiveBattleParty.IsValidIndex(0))
		{
			return;
		}

		const FGameXXKBattleRuntimeUnit& Hero = State.ActiveBattleParty[0];
		State.PlayerHP = FMath::Clamp(Hero.HP, 0, State.PlayerMaxHP);
		State.PlayerMP = FMath::Clamp(Hero.MP, 0, State.PlayerMaxMP);
	}

	static int32 FindWeakestLivingPartyIndex(const FGameXXKRuntimeState& State)
	{
		int32 BestIndex = INDEX_NONE;
		float BestRatio = TNumericLimits<float>::Max();
		for (int32 PartyIndex = 0; PartyIndex < State.ActiveBattleParty.Num(); ++PartyIndex)
		{
			const FGameXXKBattleRuntimeUnit& PartyMember = State.ActiveBattleParty[PartyIndex];
			if (PartyMember.bEnemy || PartyMember.bDefeated || PartyMember.HP <= 0)
			{
				continue;
			}

			const float Ratio = PartyMember.MaxHP > 0
				? static_cast<float>(PartyMember.HP) / static_cast<float>(PartyMember.MaxHP)
				: 0.0f;
			if (Ratio < BestRatio)
			{
				BestRatio = Ratio;
				BestIndex = PartyIndex;
			}
		}
		return BestIndex;
	}

	static void MarkDefeatedIfNeeded(FGameXXKBattleRuntimeUnit& Unit)
	{
		if (Unit.HP <= 0)
		{
			Unit.HP = 0;
			Unit.bDefeated = true;
			Unit.bDefending = false;
		}
	}

	static void ClearPartyDefense(FGameXXKRuntimeState& State)
	{
		for (FGameXXKBattleRuntimeUnit& PartyMember : State.ActiveBattleParty)
		{
			PartyMember.bDefending = false;
		}
	}

	static void RunEnemyAI(FGameXXKRuntimeState& State)
	{
		for (FGameXXKBattleRuntimeUnit& Enemy : State.ActiveBattleEnemies)
		{
			if (!Enemy.bEnemy || Enemy.bDefeated || Enemy.HP <= 0)
			{
				continue;
			}

			const int32 TargetIndex = FindWeakestLivingPartyIndex(State);
			if (!State.ActiveBattleParty.IsValidIndex(TargetIndex))
			{
				break;
			}

			FGameXXKBattleRuntimeUnit& Target = State.ActiveBattleParty[TargetIndex];
			int32 Damage = FMath::Max(1, Enemy.Attack - Target.Defense);
			if (Target.bDefending)
			{
				Damage = FMath::Max(1, FMath::CeilToInt(static_cast<float>(Damage) * 0.5f));
			}
			Target.HP = FMath::Max(0, Target.HP - Damage);
			MarkDefeatedIfNeeded(Target);
		}

		ClearPartyDefense(State);
		SyncPlayerFromBattle(State);
	}

	static bool FinishPlayerBattleAction(FGameXXKRuntimeState& State)
	{
		SyncPlayerFromBattle(State);
		if (AreAllEnemiesDefeated(State))
		{
			const bool bBossBattle = IsDungeonNode(State, EGameXXKNodeKind::Boss);
			return UGameXXKMVPRules::ResolveBattleVictory(State, bBossBattle);
		}

		RunEnemyAI(State);
		if (IsHeroDefeated(State) || AreAllPartyMembersDefeated(State))
		{
			return FailBattleToTownWithRestoredHero(State);
		}
		return true;
	}

	static bool ValidatePartyAction(FGameXXKRuntimeState& State, int32 PartyIndex)
	{
		if (State.Screen != EGameXXKScreen::Battle || !State.bHasActiveBattle || !State.ActiveBattleParty.IsValidIndex(PartyIndex))
		{
			return false;
		}
		const FGameXXKBattleRuntimeUnit& Actor = State.ActiveBattleParty[PartyIndex];
		return !Actor.bEnemy && !Actor.bDefeated && Actor.HP > 0;
	}

	static bool ValidateEnemyTarget(FGameXXKRuntimeState& State, int32 EnemyIndex)
	{
		if (!State.ActiveBattleEnemies.IsValidIndex(EnemyIndex))
		{
			return false;
		}
		const FGameXXKBattleRuntimeUnit& Target = State.ActiveBattleEnemies[EnemyIndex];
		return Target.bEnemy && !Target.bDefeated && Target.HP > 0;
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

FName UGameXXKMVPRules::ItemWoodenSword()
{
	return GameXXKMVP::ItemWoodenSwordName;
}

FName UGameXXKMVPRules::ItemStarterClothArmor()
{
	return GameXXKMVP::ItemStarterClothArmorName;
}

FName UGameXXKMVPRules::ItemClothTalisman()
{
	return GameXXKMVP::ItemClothTalismanName;
}

FGameXXKItemDef UGameXXKMVPRules::GetItemDef(FName ItemId, bool& bFound)
{
	FGameXXKItemDef Def;
	bFound = GameXXKMVP::GetItemDef(ItemId, Def);
	return Def;
}

TArray<FName> UGameXXKMVPRules::GetKnownItemIds()
{
	return GameXXKMVP::GetKnownItemIds();
}

TArray<FName> UGameXXKMVPRules::GetShopItemIds()
{
	return GameXXKMVP::GetShopItemIds();
}

FGameXXKRuntimeState UGameXXKMVPRules::CreateNewGame()
{
	FGameXXKRuntimeState State;
	State.Screen = EGameXXKScreen::MainMenu;
	State.CurrentRegion = NAME_None;
	State.CurrentMapId = TEXT("MainMenu");
	GameXXKMVP::RecalculatePlayerStats(State, false);
	State.UnlockedRegions.Add(RegionQingshan());
	AddItem(State, ItemHealingPowder(), 1);
	AddItem(State, ItemWoodenSword(), 1);
	AddItem(State, ItemStarterClothArmor(), 1);
	AddItem(State, ItemClothTalisman(), 1);
	return State;
}

bool UGameXXKMVPRules::OpenWorldMap(FGameXXKRuntimeState& State)
{
	State.Screen = EGameXXKScreen::WorldMap;
	State.CurrentRegion = NAME_None;
	State.CurrentMapId = TEXT("WorldMap");
	State.TownPanelMode = EGameXXKTownPanelMode::None;
	return true;
}

bool UGameXXKMVPRules::EnterWorldRegion(FGameXXKRuntimeState& State, FName RegionId)
{
	if (!State.UnlockedRegions.Contains(RegionId))
	{
		return false;
	}
	State.CurrentRegion = RegionId;
	State.CurrentMapId = RegionId;
	State.Screen = EGameXXKScreen::Town;
	State.TownPanelMode = EGameXXKTownPanelMode::None;
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

void UGameXXKMVPRules::GenerateRouteMapForSeed(FGameXXKRuntimeState& State, int32 Seed)
{
	const int32 NormalizedSeed = GameXXKMVP::NormalizeRouteSeed(Seed);
	FRandomStream Stream(NormalizedSeed);
	static constexpr int32 FinalLayerIndex = 6;

	State.bHasGeneratedRouteMap = true;
	State.RouteSeed = NormalizedSeed;
	State.CurrentRouteNodeId = 0;
	State.PendingRouteNodeId = INDEX_NONE;
	State.RouteMapNodes.Reset();
	State.RouteMapEdges.Reset();
	State.VisitedRouteNodeIds.Reset();
	State.ReachableRouteNodeIds.Reset();
	State.ReachableRouteNodeIds.Add(0);

	int32 NextNodeId = 0;
	GameXXKMVP::AddRouteNode(State, NextNodeId++, 0, 0, EGameXXKNodeKind::Start, 0.50f, 0.00f, {});
	for (int32 LayerIndex = 1; LayerIndex < FinalLayerIndex; ++LayerIndex)
	{
		const int32 LayerNodeCount = GameXXKMVP::GetGeneratedLayerNodeCount(Stream, LayerIndex);
		for (int32 ColumnIndex = 0; ColumnIndex < LayerNodeCount; ++ColumnIndex)
		{
			const float BaseX = static_cast<float>(ColumnIndex + 1) / static_cast<float>(LayerNodeCount + 1);
			const float Jitter = LayerNodeCount > 1 ? Stream.FRandRange(-0.035f, 0.035f) : 0.0f;
			const float X = FMath::Clamp(BaseX + Jitter, 0.12f, 0.88f);
			const float Y = static_cast<float>(LayerIndex) / static_cast<float>(FinalLayerIndex);
			const EGameXXKNodeKind NodeKind = GameXXKMVP::PickGeneratedRouteNodeKind(Stream, LayerIndex, ColumnIndex);
			GameXXKMVP::AddRouteNode(State, NextNodeId++, LayerIndex, ColumnIndex, NodeKind, X, Y, {});
		}
	}
	GameXXKMVP::AddRouteNode(State, NextNodeId++, FinalLayerIndex, 0, EGameXXKNodeKind::Boss, 0.50f, 1.00f, {});

	for (int32 LayerIndex = 0; LayerIndex < FinalLayerIndex; ++LayerIndex)
	{
		GameXXKMVP::ConnectGeneratedRouteLayer(State, Stream, LayerIndex);
	}
}

bool UGameXXKMVPRules::EnterDungeon(FGameXXKRuntimeState& State)
{
	if (State.Screen != EGameXXKScreen::Town || State.CurrentRegion != RegionQingshan() || !CanEnterDungeon(State))
	{
		return false;
	}
	State.Screen = EGameXXKScreen::DungeonMap;
	State.CurrentRegion = RegionHuangshan();
	State.CurrentMapId = TEXT("HuangshanRoute");
	State.bDungeonActive = true;
	State.DungeonNodeIndex = 0;
	State.TownPanelMode = EGameXXKTownPanelMode::None;
	GameXXKMVP::GenerateRouteMap(State);
	return true;
}

bool UGameXXKMVPRules::AdvanceDungeonNode(FGameXXKRuntimeState& State, EGameXXKNodeKind ExpectedNode)
{
	if (State.bHasGeneratedRouteMap)
	{
		const FGameXXKRouteMapNode* Node = GameXXKMVP::FindFirstReachableRouteNodeOfKind(State, ExpectedNode);
		return Node ? SelectRouteNodeById(State, Node->NodeId) : false;
	}
	if (!GameXXKMVP::IsDungeonNode(State, ExpectedNode))
	{
		return false;
	}
	if (ExpectedNode == EGameXXKNodeKind::Battle || ExpectedNode == EGameXXKNodeKind::Boss)
	{
		State.TownPanelMode = EGameXXKTownPanelMode::None;
		return GameXXKMVP::BeginBattle(State, ExpectedNode, INDEX_NONE);
	}
	State.DungeonNodeIndex += 1;
	State.Screen = EGameXXKScreen::DungeonMap;
	State.CurrentMapId = TEXT("HuangshanRoute");
	State.TownPanelMode = EGameXXKTownPanelMode::None;
	return true;
}

bool UGameXXKMVPRules::SelectRouteNodeById(FGameXXKRuntimeState& State, int32 NodeId)
{
	if (!State.bDungeonActive || !State.bHasGeneratedRouteMap || State.Screen != EGameXXKScreen::DungeonMap || !State.ReachableRouteNodeIds.Contains(NodeId))
	{
		return false;
	}

	const FGameXXKRouteMapNode* Node = GameXXKMVP::FindRouteNode(State, NodeId);
	if (!Node)
	{
		return false;
	}

	State.CurrentRouteNodeId = NodeId;
	State.TownPanelMode = EGameXXKTownPanelMode::None;
	if (Node->NodeKind == EGameXXKNodeKind::Battle || Node->NodeKind == EGameXXKNodeKind::Elite || Node->NodeKind == EGameXXKNodeKind::Boss)
	{
		State.PendingRouteNodeId = NodeId;
		return GameXXKMVP::BeginBattle(State, Node->NodeKind, NodeId);
	}
	if (Node->NodeKind == EGameXXKNodeKind::Event || Node->NodeKind == EGameXXKNodeKind::Chest)
	{
		State.PendingRouteNodeId = NodeId;
		State.Screen = EGameXXKScreen::RouteEvent;
		State.CurrentMapId = TEXT("RouteEvent");
		return true;
	}
	if (Node->NodeKind == EGameXXKNodeKind::Camp)
	{
		State.PendingRouteNodeId = NodeId;
		State.Screen = EGameXXKScreen::RouteCamp;
		State.CurrentMapId = TEXT("RouteCamp");
		return true;
	}
	if (Node->NodeKind == EGameXXKNodeKind::Merchant)
	{
		State.PendingRouteNodeId = NodeId;
		State.Screen = EGameXXKScreen::RouteMerchant;
		State.CurrentMapId = TEXT("RouteMerchant");
		return true;
	}

	return GameXXKMVP::CompleteRouteNode(State, *Node);
}

bool UGameXXKMVPRules::ResolveBattleVictory(FGameXXKRuntimeState& State, bool bBossBattle)
{
	if (State.Screen != EGameXXKScreen::Battle)
	{
		return false;
	}
	if (State.bHasGeneratedRouteMap)
	{
		const FGameXXKRouteMapNode* PendingNode = GameXXKMVP::FindPendingRouteNode(State);
		if (!PendingNode || (PendingNode->NodeKind != EGameXXKNodeKind::Battle && PendingNode->NodeKind != EGameXXKNodeKind::Elite && PendingNode->NodeKind != EGameXXKNodeKind::Boss))
		{
			return false;
		}
		if (PendingNode->NodeKind == EGameXXKNodeKind::Boss)
		{
			GameXXKMVP::ApplyXP(State, 150);
			State.PlayerGold += 35;
			AddItem(State, ItemClothArmor(), 1);
			GameXXKMVP::ClearActiveBattle(State);
			return ResolveBossClear(State);
		}
		GameXXKMVP::ApplyXP(State, PendingNode->NodeKind == EGameXXKNodeKind::Elite ? 110 : 80);
		State.PlayerGold += PendingNode->NodeKind == EGameXXKNodeKind::Elite ? 24 : 18;
		AddItem(State, ItemHealingPowder(), 1);
		GameXXKMVP::ClearActiveBattle(State);
		return GameXXKMVP::CompleteRouteNode(State, *PendingNode);
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
		GameXXKMVP::ClearActiveBattle(State);
		return ResolveBossClear(State);
	}
	if (!GameXXKMVP::IsDungeonNode(State, EGameXXKNodeKind::Battle))
	{
		return false;
	}
	GameXXKMVP::ApplyXP(State, 80);
	State.PlayerGold += 18;
	AddItem(State, ItemHealingPowder(), 1);
	GameXXKMVP::ClearActiveBattle(State);
	State.DungeonNodeIndex += 1;
	State.Screen = EGameXXKScreen::DungeonMap;
	State.CurrentMapId = TEXT("HuangshanRoute");
	return true;
}

bool UGameXXKMVPRules::ExecuteBattleBasicAttack(FGameXXKRuntimeState& State, int32 PartyIndex, int32 EnemyIndex)
{
	if (!GameXXKMVP::ValidatePartyAction(State, PartyIndex) || !GameXXKMVP::ValidateEnemyTarget(State, EnemyIndex))
	{
		return false;
	}

	FGameXXKBattleRuntimeUnit& Attacker = State.ActiveBattleParty[PartyIndex];
	FGameXXKBattleRuntimeUnit& Target = State.ActiveBattleEnemies[EnemyIndex];

	const int32 Damage = FMath::Max(1, Attacker.Attack - Target.Defense);
	Target.HP = FMath::Max(0, Target.HP - Damage);
	GameXXKMVP::MarkDefeatedIfNeeded(Target);
	Attacker.MP = FMath::Min(Attacker.MaxMP, Attacker.MP + 2);
	return GameXXKMVP::FinishPlayerBattleAction(State);
}

bool UGameXXKMVPRules::ExecuteBattleCraneWingSlash(FGameXXKRuntimeState& State, int32 PartyIndex, int32 EnemyIndex)
{
	static constexpr int32 MPCost = 8;
	if (!GameXXKMVP::ValidatePartyAction(State, PartyIndex) || !GameXXKMVP::ValidateEnemyTarget(State, EnemyIndex))
	{
		return false;
	}

	FGameXXKBattleRuntimeUnit& Attacker = State.ActiveBattleParty[PartyIndex];
	if (Attacker.MP < MPCost)
	{
		return false;
	}

	FGameXXKBattleRuntimeUnit& Target = State.ActiveBattleEnemies[EnemyIndex];
	Attacker.MP -= MPCost;
	const int32 Damage = FMath::Max(1, FMath::RoundToInt(static_cast<float>(Attacker.Attack) * 1.6f) + 6 - Target.Defense);
	Target.HP = FMath::Max(0, Target.HP - Damage);
	GameXXKMVP::MarkDefeatedIfNeeded(Target);
	return GameXXKMVP::FinishPlayerBattleAction(State);
}

bool UGameXXKMVPRules::ExecuteBattleGuiyuanArt(FGameXXKRuntimeState& State, int32 PartyIndex)
{
	static constexpr int32 MPCost = 10;
	static constexpr int32 HealAmount = 36;
	if (!GameXXKMVP::ValidatePartyAction(State, PartyIndex))
	{
		return false;
	}

	FGameXXKBattleRuntimeUnit& Caster = State.ActiveBattleParty[PartyIndex];
	if (Caster.MP < MPCost || Caster.HP >= Caster.MaxHP)
	{
		return false;
	}

	Caster.MP -= MPCost;
	Caster.HP = FMath::Min(Caster.MaxHP, Caster.HP + HealAmount);
	return GameXXKMVP::FinishPlayerBattleAction(State);
}

bool UGameXXKMVPRules::ExecuteBattleDefend(FGameXXKRuntimeState& State, int32 PartyIndex)
{
	if (!GameXXKMVP::ValidatePartyAction(State, PartyIndex))
	{
		return false;
	}

	FGameXXKBattleRuntimeUnit& Defender = State.ActiveBattleParty[PartyIndex];
	Defender.bDefending = true;
	Defender.MP = FMath::Min(Defender.MaxMP, Defender.MP + 6);
	return GameXXKMVP::FinishPlayerBattleAction(State);
}

bool UGameXXKMVPRules::ExecuteBattleHealingPowder(FGameXXKRuntimeState& State, int32 PartyIndex)
{
	if (!GameXXKMVP::ValidatePartyAction(State, PartyIndex))
	{
		return false;
	}

	FGameXXKBattleRuntimeUnit& Target = State.ActiveBattleParty[PartyIndex];
	if (Target.HP >= Target.MaxHP || !RemoveItem(State, ItemHealingPowder(), 1))
	{
		return false;
	}

	FGameXXKItemDef HealingPowderDef;
	const int32 HealAmount = GameXXKMVP::GetItemDef(ItemHealingPowder(), HealingPowderDef)
		? HealingPowderDef.HealAmount
		: 35;
	Target.HP = FMath::Min(Target.MaxHP, Target.HP + HealAmount);
	return GameXXKMVP::FinishPlayerBattleAction(State);
}

bool UGameXXKMVPRules::ResolveEventReward(FGameXXKRuntimeState& State, bool bTakeGold)
{
	if (State.bHasGeneratedRouteMap && State.Screen == EGameXXKScreen::RouteEvent)
	{
		const FGameXXKRouteMapNode* PendingNode = GameXXKMVP::FindPendingRouteNode(State);
		if (!PendingNode || (PendingNode->NodeKind != EGameXXKNodeKind::Event && PendingNode->NodeKind != EGameXXKNodeKind::Chest))
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
		return GameXXKMVP::CompleteRouteNode(State, *PendingNode);
	}
	if (State.bHasGeneratedRouteMap && State.Screen == EGameXXKScreen::DungeonMap)
	{
		const FGameXXKRouteMapNode* Node = GameXXKMVP::FindFirstReachableRouteNodeOfKind(State, EGameXXKNodeKind::Event);
		if (!Node)
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
		return GameXXKMVP::CompleteRouteNode(State, *Node);
	}
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
	State.Screen = EGameXXKScreen::DungeonMap;
	State.CurrentMapId = TEXT("HuangshanRoute");
	return true;
}

bool UGameXXKMVPRules::ResolveCampReward(FGameXXKRuntimeState& State, bool bHealNow)
{
	if (State.bHasGeneratedRouteMap && State.Screen == EGameXXKScreen::RouteCamp)
	{
		const FGameXXKRouteMapNode* PendingNode = GameXXKMVP::FindPendingRouteNode(State);
		if (!PendingNode || PendingNode->NodeKind != EGameXXKNodeKind::Camp)
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
		return GameXXKMVP::CompleteRouteNode(State, *PendingNode);
	}
	if (State.bHasGeneratedRouteMap && State.Screen == EGameXXKScreen::DungeonMap)
	{
		const FGameXXKRouteMapNode* Node = GameXXKMVP::FindFirstReachableRouteNodeOfKind(State, EGameXXKNodeKind::Camp);
		if (!Node)
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
		return GameXXKMVP::CompleteRouteNode(State, *Node);
	}
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
	State.Screen = EGameXXKScreen::DungeonMap;
	State.CurrentMapId = TEXT("HuangshanRoute");
	return true;
}

bool UGameXXKMVPRules::ResolveMerchantRouteNode(FGameXXKRuntimeState& State)
{
	if (State.bHasGeneratedRouteMap && State.Screen == EGameXXKScreen::RouteMerchant)
	{
		const FGameXXKRouteMapNode* PendingNode = GameXXKMVP::FindPendingRouteNode(State);
		if (!PendingNode || PendingNode->NodeKind != EGameXXKNodeKind::Merchant)
		{
			return false;
		}
		State.PlayerGold = FMath::Max(0, State.PlayerGold);
		return GameXXKMVP::CompleteRouteNode(State, *PendingNode);
	}
	return false;
}

bool UGameXXKMVPRules::FailDungeonToTown(FGameXXKRuntimeState& State)
{
	if (!State.bDungeonActive)
	{
		return false;
	}
	State.Screen = EGameXXKScreen::Town;
	State.CurrentRegion = RegionQingshan();
	State.CurrentMapId = RegionQingshan();
	State.bDungeonActive = false;
	State.DungeonNodeIndex = 0;
	State.TownPanelMode = EGameXXKTownPanelMode::None;
	State.PlayerHP = State.PlayerMaxHP;
	State.PlayerMP = State.PlayerMaxMP;
	State.bFollowerJoined = State.QuestState == EGameXXKQuestState::Accepted;
	GameXXKMVP::ClearActiveBattle(State);
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
	State.CurrentMapId = TEXT("WorldMap");
	State.TownPanelMode = EGameXXKTownPanelMode::None;
	State.UnlockedRegions.Add(RegionTanjiang());
	GameXXKMVP::ClearActiveBattle(State);
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
	if (State.EquippedWeapon == ItemId || State.EquippedArmor == ItemId || State.EquippedAccessory == ItemId)
	{
		GameXXKMVP::RecalculatePlayerStats(State, true);
	}
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
	}
	else if (Def.Kind == EGameXXKItemKind::Armor)
	{
		State.EquippedArmor = ItemId;
	}
	else if (Def.Kind == EGameXXKItemKind::Accessory)
	{
		State.EquippedAccessory = ItemId;
	}
	else
	{
		return false;
	}
	GameXXKMVP::RecalculatePlayerStats(State, true);
	return true;
}

bool UGameXXKMVPRules::UnequipItem(FGameXXKRuntimeState& State, FName ItemId)
{
	if (ItemId.IsNone())
	{
		return false;
	}
	bool bChanged = false;
	if (State.EquippedWeapon == ItemId)
	{
		State.EquippedWeapon = NAME_None;
		bChanged = true;
	}
	else if (State.EquippedArmor == ItemId)
	{
		State.EquippedArmor = NAME_None;
		bChanged = true;
	}
	else if (State.EquippedAccessory == ItemId)
	{
		State.EquippedAccessory = NAME_None;
		bChanged = true;
	}
	if (!bChanged)
	{
		return false;
	}
	GameXXKMVP::RecalculatePlayerStats(State, true);
	return true;
}

bool UGameXXKMVPRules::UseHealingItem(FGameXXKRuntimeState& State)
{
	return UseItem(State, ItemHealingPowder());
}

bool UGameXXKMVPRules::UseItem(FGameXXKRuntimeState& State, FName ItemId)
{
	FGameXXKItemDef Def;
	if (!GameXXKMVP::GetItemDef(ItemId, Def) || Def.Kind != EGameXXKItemKind::Consumable)
	{
		return false;
	}
	const bool bCanHealHP = Def.HealAmount > 0 && State.PlayerHP < State.PlayerMaxHP;
	const bool bCanHealMP = Def.MPHealAmount > 0 && State.PlayerMP < State.PlayerMaxMP;
	if (!bCanHealHP && !bCanHealMP)
	{
		return false;
	}
	if (!RemoveItem(State, ItemId, 1))
	{
		return false;
	}
	State.PlayerHP = FMath::Min(State.PlayerMaxHP, State.PlayerHP + Def.HealAmount);
	State.PlayerMP = FMath::Min(State.PlayerMaxMP, State.PlayerMP + Def.MPHealAmount);
	return true;
}

bool UGameXXKMVPRules::OpenTownPanel(FGameXXKRuntimeState& State, EGameXXKTownPanelMode PanelMode)
{
	if (State.Screen != EGameXXKScreen::Town)
	{
		return false;
	}
	State.TownPanelMode = PanelMode;
	return true;
}

bool UGameXXKMVPRules::CloseTownPanel(FGameXXKRuntimeState& State)
{
	State.TownPanelMode = EGameXXKTownPanelMode::None;
	return true;
}

void UGameXXKMVPRules::RecalculatePlayerStatsFromEquipment(FGameXXKRuntimeState& State)
{
	GameXXKMVP::RecalculatePlayerStats(State, true);
}

TArray<FName> UGameXXKMVPRules::BuildTurnOrder(const FGameXXKRuntimeState& State, bool bBossBattle)
{
	if (State.bHasActiveBattle)
	{
		TArray<FGameXXKBattleRuntimeUnit> Units = State.ActiveBattleParty;
		Units.Append(State.ActiveBattleEnemies);
		Units.Sort([](const FGameXXKBattleRuntimeUnit& A, const FGameXXKBattleRuntimeUnit& B)
		{
			if (A.Speed == B.Speed)
			{
				return A.Id.LexicalLess(B.Id);
			}
			return A.Speed > B.Speed;
		});

		TArray<FName> Order;
		for (const FGameXXKBattleRuntimeUnit& Unit : Units)
		{
			if (!Unit.bDefeated)
			{
				Order.Add(Unit.Id);
			}
		}
		return Order;
	}

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
	SaveState.SaveVersion = 2;
	SaveState.RuntimeState = State;
	SaveState.bHasPlayerLocation = State.bHasPlayerLocation;
	SaveState.PlayerLocation = State.PlayerLocation;
	SaveState.QuestState = State.QuestState;
	SaveState.bFollowerJoined = State.bFollowerJoined;
	SaveState.bHasQuestNpcLocation = State.bHasQuestNpcLocation;
	SaveState.QuestNpcLocation = State.QuestNpcLocation;
	SaveState.PlayerLevel = State.PlayerLevel;
	SaveState.PlayerXP = State.PlayerXP;
	SaveState.PlayerGold = State.PlayerGold;
	SaveState.UnlockedRegions = State.UnlockedRegions;
	return SaveState;
}

FGameXXKRuntimeState UGameXXKMVPRules::RestoreFromSaveState(const FGameXXKSaveState& SaveState)
{
	if (SaveState.SaveVersion >= 2)
	{
		FGameXXKRuntimeState State = SaveState.RuntimeState;
		if (SaveState.bHasPlayerLocation)
		{
			State.bHasPlayerLocation = true;
			State.PlayerLocation = SaveState.PlayerLocation;
		}
		return State;
	}

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
	State.EquippedAccessory = NAME_None;
	State.TownPanelMode = EGameXXKTownPanelMode::None;
	State.bHasPlayerLocation = SaveState.bHasPlayerLocation;
	State.PlayerLocation = SaveState.PlayerLocation;
	State.bFollowerJoined = SaveState.bFollowerJoined;
	State.bHasQuestNpcLocation = SaveState.bHasQuestNpcLocation;
	State.QuestNpcLocation = SaveState.QuestNpcLocation;
	return State;
}
