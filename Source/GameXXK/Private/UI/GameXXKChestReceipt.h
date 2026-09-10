#pragma once
#include "GameXXKTrainingChestRules.h"
#include "GameXXKMVPRules.h"
#include "GameXXKTravelMoneyRules.h"
#include "GameXXKGemRules.h"
#include "GameXXKEquipmentCatalog.h"
#include "UI/GameXXKLocalization.h"

namespace GameXXKChestReceipt
{
    inline FText BuildRecord(const FGameXXKTrainingChestOpenReceipt& Receipt)
    {
        FText Name;
        if(!Receipt.EquipmentBaseId.IsNone())
        {
            const auto* Definition=FGameXXKEquipmentCatalog::FindDefinition(Receipt.EquipmentBaseId);
            Name=Definition?GameXXKLocalization::Localize(Definition->DisplayName):GameXXKLocalization::Text(TEXT("Chest.Report.Gear"));
        }
        else
        {
            bool Found=false;const auto Item=UGameXXKMVPRules::GetItemDef(Receipt.ItemId,Found);
            Name=Found?GameXXKLocalization::Localize(Item.DisplayName):FText::FromName(Receipt.ItemId);
        }
        const FText Drop=FText::Format(GameXXKLocalization::Text(TEXT("Chest.Receipt.Item")),Name,Receipt.Quantity,
            GameXXKLocalization::Text(Receipt.bSentToWarehouse?TEXT("Chest.Receipt.Storage"):TEXT("Chest.Receipt.Bag")));
        const FText Kind=GameXXKLocalization::Text(Receipt.Tier==EGameXXKTrainingRewardTier::NormalChest?TEXT("Chest.Report.Normal"):
            Receipt.Tier==EGameXXKTrainingRewardTier::AdvancedChest?TEXT("Chest.Report.Advanced"):TEXT("Chest.Report.Hunt"));
        FText Detail=FText::Format(GameXXKLocalization::Text(TEXT("Chest.Report.Header")),Receipt.OpenOrdinal,Kind,
            FGameXXKGemRules::GetQualityDisplayName(FGameXXKGemRules::QualityFromRank(Receipt.QualityRank)));
        if(Receipt.ItemLevel>0)Detail=FText::Format(GameXXKLocalization::Text(TEXT("Chest.Report.Level")),Detail,Receipt.ItemLevel);
        return FText::Format(GameXXKLocalization::Text(TEXT("Chest.Receipt.Lines")),Drop,Detail);
    }

    inline TArray<FText> BuildReports(const FGameXXKTrainingChestOpenResult& Result)
    {
        TArray<FText> Reports;
        if(!Result.bSucceeded)return Reports;
        for(const auto& Receipt:Result.Receipts)Reports.Add(BuildRecord(Receipt));
        if(!Reports.IsEmpty()&&Result.Error==EGameXXKTrainingChestOpenError::BackpackFull)
            Reports.Last()=FText::Format(GameXXKLocalization::Text(TEXT("Chest.Receipt.Lines")),Reports.Last(),GameXXKLocalization::Text(TEXT("Chest.Receipt.Full")));
        return Reports;
    }

    inline FText Build(const FGameXXKRuntimeState& State,const FGameXXKTrainingChestOpenResult& Result)
    {
        if(Result.Receipts.Num()==1)return BuildRecord(Result.Receipts[0]);
        auto Join=[](const FText& A,const FText& B){return A.IsEmpty()?B:FText::Format(GameXXKLocalization::Text(TEXT("Chest.Receipt.Lines")),A,B);};
        FText Details;TArray<FName> Ids;Result.ItemDeltas.GetKeys(Ids);Ids.Sort(FNameLexicalLess());
        int64 BagGems=0,StoredGems=0;
        for(FName Id:Ids)
        {
            const bool Stored=Id!=FGameXXKTravelMoneyRules::ItemId()&&State.DesktopInventory.WarehouseItems.FindRef(Id)>0;
            EGameXXKGemType GemType;EGameXXKGemQuality GemQuality;
            if(Result.OpenedCount>1&&FGameXXKGemRules::TryParseItemId(Id,GemType,GemQuality))
            {
                (Stored?StoredGems:BagGems)+=Result.ItemDeltas.FindRef(Id);continue;
            }
            bool Found=false;const auto Item=UGameXXKMVPRules::GetItemDef(Id,Found);
            const FText Name=Found?GameXXKLocalization::Localize(Item.DisplayName):FText::FromName(Id);
            Details=Join(Details,FText::Format(GameXXKLocalization::Text(TEXT("Chest.Receipt.Item")),Name,Result.ItemDeltas.FindRef(Id),
                GameXXKLocalization::Text(Stored?TEXT("Chest.Receipt.Storage"):TEXT("Chest.Receipt.Bag"))));
        }
        for(bool Stored:{false,true})if(const int64 Count=Stored?StoredGems:BagGems)
            Details=Join(Details,FText::Format(GameXXKLocalization::Text(TEXT("Chest.Receipt.Item")),GameXXKLocalization::Text(TEXT("Chest.Receipt.Gems")),Count,
                GameXXKLocalization::Text(Stored?TEXT("Chest.Receipt.Storage"):TEXT("Chest.Receipt.Bag"))));
        if(!Result.EquipmentInstanceIds.IsEmpty())Details=Join(Details,FText::Format(GameXXKLocalization::Text(TEXT("Chest.Receipt.Equipment")),Result.EquipmentInstanceIds.Num()));
        if(Result.Error==EGameXXKTrainingChestOpenError::BackpackFull)Details=Join(Details,GameXXKLocalization::Text(TEXT("Chest.Receipt.Full")));
        if(Result.OpenedCount==1)return Details;
        const FText Summary=FText::Format(GameXXKLocalization::Text(TEXT("Chest.Receipt.Batch")),Result.OpenedCount,
            Result.ItemDeltas.FindRef(UGameXXKMVPRules::ItemEnhancementStone()),Result.ItemDeltas.FindRef(UGameXXKMVPRules::ItemRefinementSand()));
        return Join(Summary,Details);
    }
}
