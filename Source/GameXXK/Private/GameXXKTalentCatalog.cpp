#include "GameXXKTalentCatalog.h"

namespace
{
	constexpr float EntryDistance = 245.0f;
	constexpr float LayerDistance = 145.0f;
	constexpr float ApprovedGridPitch = 180.0f;

	void SetError(FString* OutError, const FString& Error)
	{
		if (OutError)
		{
			*OutError = Error;
		}
	}

	FGameXXKTalentNodeDefinition MakeNode(
		const TCHAR* Id,
		const TCHAR* Name,
		const TCHAR* Description,
		const EGameXXKTalentBranch Branch,
		const EGameXXKTalentEffect Effect,
		const EGameXXKTalentIcon Icon,
		const int32 EffectPerRank,
		const int32 MaxRank,
		const int32 CostTier,
		const int32 Layer,
		const FVector2D& Position,
		const TArray<FName>& Prerequisites = {})
	{
		FGameXXKTalentNodeDefinition Node;
		Node.Id = FName(Id);
		Node.DisplayName = FText::FromString(Name);
		Node.Description = FText::FromString(Description);
		Node.Branch = Branch;
		Node.Effect = Effect;
		Node.Icon = Icon;
		Node.EffectPerRank = EffectPerRank;
		Node.MaxRank = MaxRank;
		Node.CostTier = CostTier;
		Node.GraphLayer = Layer;
		Node.GraphPosition = Position;
		Node.PrerequisiteIds = Prerequisites;
		return Node;
	}

	FVector2D DirectionForBranch(const EGameXXKTalentBranch Branch)
	{
		switch (Branch)
		{
		case EGameXXKTalentBranch::Combat: return FVector2D(-1.0f, -1.0f).GetSafeNormal();
		case EGameXXKTalentBranch::CapacityChest: return FVector2D(-1.0f, 1.0f).GetSafeNormal();
		case EGameXXKTalentBranch::IdleOffline: return FVector2D(1.0f, -1.0f).GetSafeNormal();
		case EGameXXKTalentBranch::Tools: return FVector2D(1.0f, 1.0f).GetSafeNormal();
		default: return FVector2D::ZeroVector;
		}
	}

	FName EntryIdForBranch(const EGameXXKTalentBranch Branch)
	{
		switch (Branch)
		{
		case EGameXXKTalentBranch::Combat: return TEXT("Talent.Entry.Combat");
		case EGameXXKTalentBranch::CapacityChest: return TEXT("Talent.Entry.CapacityChest");
		case EGameXXKTalentBranch::IdleOffline: return TEXT("Talent.Entry.IdleOffline");
		case EGameXXKTalentBranch::Tools: return TEXT("Talent.Entry.Tools");
		default: return NAME_None;
		}
	}

	void AddTrack(
		TArray<FGameXXKTalentNodeDefinition>& Nodes,
		const TCHAR* Prefix,
		const TCHAR* DisplayPrefix,
		const TCHAR* Description,
		const EGameXXKTalentBranch Branch,
		const EGameXXKTalentEffect Effect,
		const EGameXXKTalentIcon Icon,
		const int32 EffectPerRank,
		const int32 LayerCount,
		const float LateralOffset)
	{
		const FVector2D Direction = DirectionForBranch(Branch);
		const FVector2D Perpendicular(-Direction.Y, Direction.X);
		FName PreviousId = EntryIdForBranch(Branch);
		for (int32 Layer = 1; Layer <= LayerCount; ++Layer)
		{
			const FString NodeId = FString::Printf(TEXT("%s.%02d"), Prefix, Layer);
			const FVector2D Position = Direction * (EntryDistance + LayerDistance * Layer)
				+ Perpendicular * LateralOffset;
			FGameXXKTalentNodeDefinition Node = MakeNode(
				*NodeId,
				*FString::Printf(TEXT("%s %02d"), DisplayPrefix, Layer),
				Description,
				Branch,
				Effect,
				Icon,
				EffectPerRank,
				5,
				Layer,
				Layer,
				Position,
				{PreviousId});
			Nodes.Add(MoveTemp(Node));
			PreviousId = FName(*NodeId);
		}
	}

	FVector2D GridOffset(
		const EGameXXKTalentBranch Branch,
		const int32 X,
		const int32 Y)
	{
		const float SignX = Branch == EGameXXKTalentBranch::Combat
			|| Branch == EGameXXKTalentBranch::CapacityChest ? -1.0f : 1.0f;
		const float SignY = Branch == EGameXXKTalentBranch::Combat
			|| Branch == EGameXXKTalentBranch::IdleOffline ? -1.0f : 1.0f;
		return FVector2D(SignX * X * ApprovedGridPitch, SignY * Y * ApprovedGridPitch);
	}

	FGameXXKTalentNodeDefinition& RequireNode(
		TArray<FGameXXKTalentNodeDefinition>& Nodes,
		const FName NodeId)
	{
		FGameXXKTalentNodeDefinition* Node = Nodes.FindByPredicate(
			[NodeId](const FGameXXKTalentNodeDefinition& Candidate)
			{
				return Candidate.Id == NodeId;
			});
		checkf(Node, TEXT("Missing permanent talent node %s"), *NodeId.ToString());
		return *Node;
	}

	TArray<FName> NumberedIds(
		const TCHAR* Prefix,
		const int32 First,
		const int32 Last)
	{
		TArray<FName> Result;
		for (int32 Index = First; Index <= Last; ++Index)
		{
			Result.Add(FName(*FString::Printf(TEXT("%s.%02d"), Prefix, Index)));
		}
		return Result;
	}

	struct FApprovedTrackPair
	{
		TArray<FName> Horizontal;
		TArray<FName> Vertical;
		bool bPreserveVerticalCostTier = false;
	};

	void PlaceStraightTrack(
		TArray<FGameXXKTalentNodeDefinition>& Nodes,
		const TArray<FName>& NodeIds,
		const FVector2D Start,
		const FVector2D Step,
		const FName RootPrerequisite,
		const int32 FirstCostTier,
		const bool bPreserveAuthoredCostTier)
	{
		FName PreviousId = RootPrerequisite;
		for (int32 Index = 0; Index < NodeIds.Num(); ++Index)
		{
			FGameXXKTalentNodeDefinition& Node = RequireNode(Nodes, NodeIds[Index]);
			Node.GraphPosition = Start + Step * Index;
			Node.PrerequisiteIds = {PreviousId};
			Node.VisualConnectionIds = {PreviousId};
			if (!bPreserveAuthoredCostTier)
			{
				Node.CostTier = FMath::Clamp(FirstCostTier + Index, 1, 35);
				Node.GraphLayer = Node.CostTier;
			}
			PreviousId = Node.Id;
		}
	}

	void ApplyBranchGrid(
		TArray<FGameXXKTalentNodeDefinition>& Nodes,
		const EGameXXKTalentBranch Branch,
		const TArray<FName>& MainIds,
		const TArray<FApprovedTrackPair>& SidePairs,
		const TCHAR* MainDisplayPrefix,
		const EGameXXKTalentIcon MainIcon)
	{
		check(MainIds.Num() == SidePairs.Num());
		for (int32 CycleIndex = 0; CycleIndex < MainIds.Num(); ++CycleIndex)
		{
			FGameXXKTalentNodeDefinition& Main = RequireNode(Nodes, MainIds[CycleIndex]);
			const FVector2D MainPosition = GridOffset(Branch, CycleIndex + 1, CycleIndex + 1);
			Main.GraphPosition = MainPosition;
			Main.CostTier = CycleIndex == 0 ? 0 : CycleIndex;
			Main.GraphLayer = Main.CostTier;
			Main.DisplayName = FText::FromString(FString::Printf(
				TEXT("%s %02d"),
				MainDisplayPrefix,
				CycleIndex + 1));
			Main.Icon = MainIcon;
			if (CycleIndex == 0)
			{
				Main.PrerequisiteIds = {TEXT("Talent.Root")};
				Main.VisualConnectionIds = {TEXT("Talent.Root")};
			}
			else
			{
				Main.PrerequisiteIds = {MainIds[CycleIndex - 1]};
				Main.VisualConnectionIds = {MainIds[CycleIndex - 1]};
			}

			const FApprovedTrackPair& Pair = SidePairs[CycleIndex];
			PlaceStraightTrack(
				Nodes,
				Pair.Horizontal,
				MainPosition + GridOffset(Branch, 1, 0),
				GridOffset(Branch, 1, 0),
				Main.Id,
				CycleIndex + 1,
				false);
			PlaceStraightTrack(
				Nodes,
				Pair.Vertical,
				MainPosition + GridOffset(Branch, 0, 1),
				GridOffset(Branch, 0, 1),
				Main.Id,
				CycleIndex + 1,
				Pair.bPreserveVerticalCostTier);
		}
	}

	void ApplyApprovedGridLayout(TArray<FGameXXKTalentNodeDefinition>& Nodes)
	{
		for (FGameXXKTalentNodeDefinition& Node : Nodes)
		{
			Node.VisualConnectionIds.Reset();
		}
		FGameXXKTalentNodeDefinition& Root = RequireNode(Nodes, TEXT("Talent.Root"));
		Root.GraphPosition = FVector2D::ZeroVector;

		ApplyBranchGrid(
			Nodes,
			EGameXXKTalentBranch::Combat,
			{TEXT("Talent.Entry.Combat"), TEXT("Talent.Combat.FlatAttack.08"),
			 TEXT("Talent.Combat.FlatHealth.08"), TEXT("Talent.Combat.FlatDefense.08"),
			 TEXT("Talent.Combat.CriticalDamage.05")},
			{
				{NumberedIds(TEXT("Talent.Combat.FlatAttack"), 1, 7), NumberedIds(TEXT("Talent.Combat.FlatHealth"), 1, 7)},
				{NumberedIds(TEXT("Talent.Combat.FlatDefense"), 1, 7), {TEXT("Talent.Combat.Movement.01")}},
				{NumberedIds(TEXT("Talent.Combat.AttackPercent"), 1, 10), NumberedIds(TEXT("Talent.Combat.FinalDamage"), 1, 10)},
				{NumberedIds(TEXT("Talent.Combat.DefensePercent"), 1, 10), NumberedIds(TEXT("Talent.Combat.HealthPercent"), 1, 10)},
				{NumberedIds(TEXT("Talent.Combat.CriticalChance"), 1, 4), NumberedIds(TEXT("Talent.Combat.CriticalDamage"), 1, 4)}
			},
			TEXT("战斗根基"),
			EGameXXKTalentIcon::Attack);

		ApplyBranchGrid(
			Nodes,
			EGameXXKTalentBranch::CapacityChest,
			{TEXT("Talent.Entry.CapacityChest"), TEXT("Talent.Capacity.Backpack.34"), TEXT("Talent.Capacity.Backpack.35")},
			{
				{NumberedIds(TEXT("Talent.Capacity.Backpack"), 1, 33),
				 {TEXT("Talent.Capacity.WarehousePage.03"), TEXT("Talent.Capacity.WarehousePage.04"),
				  TEXT("Talent.Capacity.WarehousePage.05"), TEXT("Talent.Capacity.WarehousePage.06")}, true},
				{NumberedIds(TEXT("Talent.Chest.NormalDrop"), 1, 35), NumberedIds(TEXT("Talent.Chest.AdvancedDrop"), 1, 35)},
				{NumberedIds(TEXT("Talent.Chest.OfflineTime"), 1, 35), {}}
			},
			TEXT("行囊根基"),
			EGameXXKTalentIcon::Backpack);

		ApplyBranchGrid(
			Nodes,
			EGameXXKTalentBranch::IdleOffline,
			{TEXT("Talent.Entry.IdleOffline"), TEXT("Talent.Idle.OnlineGold.35"), TEXT("Talent.Idle.OnlineExperience.35")},
			{
				{NumberedIds(TEXT("Talent.Idle.OnlineGold"), 1, 34), NumberedIds(TEXT("Talent.Idle.OnlineExperience"), 1, 34)},
				{NumberedIds(TEXT("Talent.Idle.OfflineGold"), 1, 35), NumberedIds(TEXT("Talent.Idle.OfflineExperience"), 1, 35)},
				{NumberedIds(TEXT("Talent.Idle.OfflineGoldTime"), 1, 35), NumberedIds(TEXT("Talent.Idle.OfflineExperienceTime"), 1, 35)}
			},
			TEXT("行旅根基"),
			EGameXXKTalentIcon::Offline);

		ApplyBranchGrid(
			Nodes,
			EGameXXKTalentBranch::Tools,
			{TEXT("Talent.Entry.Tools")},
			{
				{NumberedIds(TEXT("Talent.Tools.Experience"), 1, 10), NumberedIds(TEXT("Talent.Tools.Gold"), 1, 10)}
			},
			TEXT("百工根基"),
			EGameXXKTalentIcon::Tools);
	}

	TArray<FGameXXKTalentNodeDefinition> BuildDefinitions()
	{
		TArray<FGameXXKTalentNodeDefinition> Nodes;
		Nodes.Reserve(430);

		FGameXXKTalentNodeDefinition Root = MakeNode(
			TEXT("Talent.Root"),
			TEXT("行旅根基"),
			TEXT("解锁四条永久天赋分支，并开启仓库第2页。"),
			EGameXXKTalentBranch::None,
			EGameXXKTalentEffect::UnlockWarehousePage,
			EGameXXKTalentIcon::Root,
			1,
			1,
			0,
			0,
			FVector2D::ZeroVector);
		Root.bRoot = true;
		Nodes.Add(MoveTemp(Root));

		auto AddEntry = [&Nodes](
			const TCHAR* Id,
			const TCHAR* Name,
			const TCHAR* Description,
			const EGameXXKTalentBranch Branch,
			const EGameXXKTalentEffect Effect,
			const EGameXXKTalentIcon Icon,
			const int32 EffectValue)
		{
			FGameXXKTalentNodeDefinition Entry = MakeNode(
				Id,
				Name,
				Description,
				Branch,
				Effect,
				Icon,
				EffectValue,
				1,
				0,
				0,
				DirectionForBranch(Branch) * EntryDistance,
				{TEXT("Talent.Root")});
			Entry.bBranchEntry = true;
			Nodes.Add(MoveTemp(Entry));
		};

		AddEntry(
			TEXT("Talent.Entry.Combat"), TEXT("强身砺刃"),
			TEXT("全队攻击、生命、防御各+5，并展开战斗分支。"),
			EGameXXKTalentBranch::Combat, EGameXXKTalentEffect::CombatFoundation,
			EGameXXKTalentIcon::Attack, 5);
		AddEntry(
			TEXT("Talent.Entry.CapacityChest"), TEXT("行囊纳宝"),
			TEXT("背包容量+5，并展开容量与宝箱分支。"),
			EGameXXKTalentBranch::CapacityChest, EGameXXKTalentEffect::BackpackSlots,
			EGameXXKTalentIcon::Backpack, 5);
		AddEntry(
			TEXT("Talent.Entry.IdleOffline"), TEXT("昼夜行旅"),
			TEXT("解锁离线奖励，并展开在线/离线收益分支。"),
			EGameXXKTalentBranch::IdleOffline, EGameXXKTalentEffect::UnlockOfflineRewards,
			EGameXXKTalentIcon::Offline, 1);
		AddEntry(
			TEXT("Talent.Entry.Tools"), TEXT("百工开物"),
			TEXT("一次解锁分解、合成、强化、洗炼与镶嵌。"),
			EGameXXKTalentBranch::Tools, EGameXXKTalentEffect::UnlockTools,
			EGameXXKTalentIcon::Tools, 1);

		// Short combat branch: all foundational tracks can be finished near the center.
		AddTrack(Nodes, TEXT("Talent.Combat.FlatAttack"), TEXT("攻击锤炼"), TEXT("全队攻击+5。"),
			EGameXXKTalentBranch::Combat, EGameXXKTalentEffect::FlatAttack, EGameXXKTalentIcon::Attack, 5, 8, -520.0f);
		AddTrack(Nodes, TEXT("Talent.Combat.FlatHealth"), TEXT("气血锤炼"), TEXT("全队最大生命+5。"),
			EGameXXKTalentBranch::Combat, EGameXXKTalentEffect::FlatMaxHP, EGameXXKTalentIcon::Health, 5, 8, -390.0f);
		AddTrack(Nodes, TEXT("Talent.Combat.FlatDefense"), TEXT("护体锤炼"), TEXT("全队防御+5。"),
			EGameXXKTalentBranch::Combat, EGameXXKTalentEffect::FlatDefense, EGameXXKTalentIcon::Defense, 5, 8, -260.0f);
		AddTrack(Nodes, TEXT("Talent.Combat.AttackPercent"), TEXT("攻势增幅"), TEXT("局内攻击+2%。"),
			EGameXXKTalentBranch::Combat, EGameXXKTalentEffect::RouteAttackPercent, EGameXXKTalentIcon::Attack, 2, 10, -130.0f);
		AddTrack(Nodes, TEXT("Talent.Combat.FinalDamage"), TEXT("终伤增幅"), TEXT("局内最终伤害+2%。"),
			EGameXXKTalentBranch::Combat, EGameXXKTalentEffect::RouteFinalDamagePercent, EGameXXKTalentIcon::Attack, 2, 10, 0.0f);
		AddTrack(Nodes, TEXT("Talent.Combat.DefensePercent"), TEXT("守势增幅"), TEXT("局内防御+2%。"),
			EGameXXKTalentBranch::Combat, EGameXXKTalentEffect::RouteDefensePercent, EGameXXKTalentIcon::Defense, 2, 10, 130.0f);
		AddTrack(Nodes, TEXT("Talent.Combat.HealthPercent"), TEXT("气血增幅"), TEXT("局内最大生命+2%。"),
			EGameXXKTalentBranch::Combat, EGameXXKTalentEffect::RouteMaxHPPercent, EGameXXKTalentIcon::Health, 2, 10, 260.0f);
		AddTrack(Nodes, TEXT("Talent.Combat.CriticalChance"), TEXT("会心"), TEXT("暴击率+1%。"),
			EGameXXKTalentBranch::Combat, EGameXXKTalentEffect::CriticalChancePercent, EGameXXKTalentIcon::Critical, 1, 4, 390.0f);
		AddTrack(Nodes, TEXT("Talent.Combat.CriticalDamage"), TEXT("会心伤害"), TEXT("暴击伤害+2%。"),
			EGameXXKTalentBranch::Combat, EGameXXKTalentEffect::CriticalDamagePercent, EGameXXKTalentIcon::Critical, 2, 5, 520.0f);
		AddTrack(Nodes, TEXT("Talent.Combat.Movement"), TEXT("疾行"), TEXT("每级将波次间走路时间缩短0.5秒。"),
			EGameXXKTalentBranch::Combat, EGameXXKTalentEffect::TravelMovementRank, EGameXXKTalentIcon::Movement, 1, 1, 650.0f);

		// Capacity/chest long branch.
		AddTrack(Nodes, TEXT("Talent.Capacity.Backpack"), TEXT("背包扩容"), TEXT("背包容量+1格。"),
			EGameXXKTalentBranch::CapacityChest, EGameXXKTalentEffect::BackpackSlots, EGameXXKTalentIcon::Backpack, 1, 35, -330.0f);
		AddTrack(Nodes, TEXT("Talent.Chest.NormalDrop"), TEXT("普通宝箱掉率"), TEXT("普通宝箱相对掉率+2%。"),
			EGameXXKTalentBranch::CapacityChest, EGameXXKTalentEffect::NormalChestDropPercent, EGameXXKTalentIcon::Chest, 2, 35, -110.0f);
		AddTrack(Nodes, TEXT("Talent.Chest.AdvancedDrop"), TEXT("高级宝箱掉率"), TEXT("高级宝箱相对掉率+2%。"),
			EGameXXKTalentBranch::CapacityChest, EGameXXKTalentEffect::AdvancedChestDropPercent, EGameXXKTalentIcon::Chest, 2, 35, 110.0f);
		AddTrack(Nodes, TEXT("Talent.Chest.OfflineTime"), TEXT("离线宝箱时间"), TEXT("离线宝箱累计上限+3分钟。"),
			EGameXXKTalentBranch::CapacityChest, EGameXXKTalentEffect::OfflineChestMinutes, EGameXXKTalentIcon::ChestTime, 3, 35, 330.0f);

		const int32 MilestoneLayers[] = {5, 15, 25, 35};
		const int32 RequiredCapacities[] = {50, 100, 150, 200};
		for (int32 Index = 0; Index < 4; ++Index)
		{
			const int32 Layer = MilestoneLayers[Index];
			const FString NodeId = FString::Printf(TEXT("Talent.Capacity.WarehousePage.%02d"), Index + 3);
			FGameXXKTalentNodeDefinition Node = MakeNode(
				*NodeId,
				*FString::Printf(TEXT("仓库第%d页"), Index + 3),
				TEXT("永久解锁一页仓库。"),
				EGameXXKTalentBranch::CapacityChest,
				EGameXXKTalentEffect::UnlockWarehousePage,
				EGameXXKTalentIcon::Warehouse,
				1,
				1,
				Layer,
				Layer,
				DirectionForBranch(EGameXXKTalentBranch::CapacityChest)
					* (EntryDistance + LayerDistance * Layer)
					+ FVector2D(-DirectionForBranch(EGameXXKTalentBranch::CapacityChest).Y,
						DirectionForBranch(EGameXXKTalentBranch::CapacityChest).X) * -550.0f,
				{FName(*FString::Printf(TEXT("Talent.Capacity.Backpack.%02d"), Layer))});
			Node.bMilestone = true;
			Node.RequiredBackpackCapacity = RequiredCapacities[Index];
			Nodes.Add(MoveTemp(Node));
		}

		// Six independent long reward/time tracks.
		AddTrack(Nodes, TEXT("Talent.Idle.OnlineGold"), TEXT("在线金币"), TEXT("在线挂机金币+2%。"),
			EGameXXKTalentBranch::IdleOffline, EGameXXKTalentEffect::OnlineGoldPercent, EGameXXKTalentIcon::Gold, 2, 35, -550.0f);
		AddTrack(Nodes, TEXT("Talent.Idle.OnlineExperience"), TEXT("在线经验"), TEXT("在线挂机经验+2%。"),
			EGameXXKTalentBranch::IdleOffline, EGameXXKTalentEffect::OnlineExperiencePercent, EGameXXKTalentIcon::Experience, 2, 35, -330.0f);
		AddTrack(Nodes, TEXT("Talent.Idle.OfflineGold"), TEXT("离线金币"), TEXT("离线挂机金币+2%。"),
			EGameXXKTalentBranch::IdleOffline, EGameXXKTalentEffect::OfflineGoldPercent, EGameXXKTalentIcon::Gold, 2, 35, -110.0f);
		AddTrack(Nodes, TEXT("Talent.Idle.OfflineExperience"), TEXT("离线经验"), TEXT("离线挂机经验+2%。"),
			EGameXXKTalentBranch::IdleOffline, EGameXXKTalentEffect::OfflineExperiencePercent, EGameXXKTalentIcon::Experience, 2, 35, 110.0f);
		AddTrack(Nodes, TEXT("Talent.Idle.OfflineGoldTime"), TEXT("离线金币时间"), TEXT("离线金币累计时间+2%。"),
			EGameXXKTalentBranch::IdleOffline, EGameXXKTalentEffect::OfflineGoldTimePercent, EGameXXKTalentIcon::Time, 2, 35, 330.0f);
		AddTrack(Nodes, TEXT("Talent.Idle.OfflineExperienceTime"), TEXT("离线经验时间"), TEXT("离线经验累计时间+2%。"),
			EGameXXKTalentBranch::IdleOffline, EGameXXKTalentEffect::OfflineExperienceTimePercent, EGameXXKTalentIcon::Time, 2, 35, 550.0f);

		// Short tools branch.
		AddTrack(Nodes, TEXT("Talent.Tools.Experience"), TEXT("工具经验"), TEXT("工具经验+5%。"),
			EGameXXKTalentBranch::Tools, EGameXXKTalentEffect::ToolExperiencePercent, EGameXXKTalentIcon::ToolReward, 5, 10, -130.0f);
		AddTrack(Nodes, TEXT("Talent.Tools.Gold"), TEXT("工具金币"), TEXT("工具产出金币+5%。"),
			EGameXXKTalentBranch::Tools, EGameXXKTalentEffect::ToolGoldPercent, EGameXXKTalentIcon::ToolReward, 5, 10, 130.0f);

		ApplyApprovedGridLayout(Nodes);
		return Nodes;
	}

	bool HasCycleFrom(
		const FName NodeId,
		const TMap<FName, const FGameXXKTalentNodeDefinition*>& ById,
		TSet<FName>& Visiting,
		TSet<FName>& Visited)
	{
		if (Visiting.Contains(NodeId))
		{
			return true;
		}
		if (Visited.Contains(NodeId))
		{
			return false;
		}
		Visiting.Add(NodeId);
		if (const FGameXXKTalentNodeDefinition* const* Node = ById.Find(NodeId))
		{
			for (const FName PrerequisiteId : (*Node)->PrerequisiteIds)
			{
				if (HasCycleFrom(PrerequisiteId, ById, Visiting, Visited))
				{
					return true;
				}
			}
		}
		Visiting.Remove(NodeId);
		Visited.Add(NodeId);
		return false;
	}
}

const TArray<FGameXXKTalentNodeDefinition>& FGameXXKTalentCatalog::GetDefinitions()
{
	static const TArray<FGameXXKTalentNodeDefinition> Definitions = BuildDefinitions();
	return Definitions;
}

const FGameXXKTalentNodeDefinition* FGameXXKTalentCatalog::Find(const FName NodeId)
{
	return GetDefinitions().FindByPredicate([NodeId](const FGameXXKTalentNodeDefinition& Node)
	{
		return Node.Id == NodeId;
	});
}

const FGameXXKTalentNodeDefinition* FGameXXKTalentCatalog::FindBranchEntry(const EGameXXKTalentBranch Branch)
{
	return GetDefinitions().FindByPredicate([Branch](const FGameXXKTalentNodeDefinition& Node)
	{
		return Node.bBranchEntry && Node.Branch == Branch;
	});
}

int32 FGameXXKTalentCatalog::GetMaxCostTier()
{
	int32 MaxTier = 0;
	for (const FGameXXKTalentNodeDefinition& Node : GetDefinitions())
	{
		MaxTier = FMath::Max(MaxTier, Node.CostTier);
	}
	return MaxTier;
}

bool FGameXXKTalentCatalog::Validate(FString* OutError)
{
	const TArray<FGameXXKTalentNodeDefinition>& Definitions = GetDefinitions();
	if (Definitions.IsEmpty())
	{
		SetError(OutError, TEXT("Talent catalog is empty."));
		return false;
	}
	TMap<FName, const FGameXXKTalentNodeDefinition*> ById;
	int32 RootCount = 0;
	TMap<EGameXXKTalentBranch, int32> EntryCounts;
	for (const FGameXXKTalentNodeDefinition& Node : Definitions)
	{
		if (Node.Id.IsNone() || ById.Contains(Node.Id))
		{
			SetError(OutError, TEXT("Talent catalog contains an empty or duplicate ID."));
			return false;
		}
		if ((Node.MaxRank != 1 && Node.MaxRank != 5)
			|| Node.CostTier < 0 || Node.CostTier > 35
			|| Node.GraphLayer < 0 || Node.GraphLayer > 35
			|| static_cast<int32>(Node.Icon) < 0 || static_cast<int32>(Node.Icon) >= 16)
		{
			SetError(OutError, FString::Printf(TEXT("Talent %s has invalid rank, tier, layer, or icon."), *Node.Id.ToString()));
			return false;
		}
		RootCount += Node.bRoot ? 1 : 0;
		if (Node.bBranchEntry)
		{
			EntryCounts.FindOrAdd(Node.Branch) += 1;
		}
		ById.Add(Node.Id, &Node);
	}
	if (RootCount != 1)
	{
		SetError(OutError, TEXT("Talent catalog must contain exactly one root."));
		return false;
	}
	for (const EGameXXKTalentBranch Branch : {
		EGameXXKTalentBranch::Combat,
		EGameXXKTalentBranch::CapacityChest,
		EGameXXKTalentBranch::IdleOffline,
		EGameXXKTalentBranch::Tools})
	{
		if (EntryCounts.FindRef(Branch) != 1)
		{
			SetError(OutError, TEXT("Talent catalog must contain exactly one entry per branch."));
			return false;
		}
	}
	for (const FGameXXKTalentNodeDefinition& Node : Definitions)
	{
		for (const FName PrerequisiteId : Node.PrerequisiteIds)
		{
			if (!ById.Contains(PrerequisiteId))
			{
				SetError(OutError, FString::Printf(TEXT("Talent %s has a missing prerequisite."), *Node.Id.ToString()));
				return false;
			}
		}
		for (const FName VisualSourceId : Node.VisualConnectionIds)
		{
			if (!ById.Contains(VisualSourceId))
			{
				SetError(OutError, FString::Printf(TEXT("Talent %s has a missing visual connection source."), *Node.Id.ToString()));
				return false;
			}
		}
	}
	TSet<FName> Visiting;
	TSet<FName> Visited;
	for (const FGameXXKTalentNodeDefinition& Node : Definitions)
	{
		if (HasCycleFrom(Node.Id, ById, Visiting, Visited))
		{
			SetError(OutError, TEXT("Talent catalog contains a prerequisite cycle."));
			return false;
		}
	}
	SetError(OutError, FString());
	return true;
}

int32 FGameXXKTalentCatalog::GetIconAtlasIndex(const EGameXXKTalentIcon Icon)
{
	return static_cast<int32>(Icon);
}
