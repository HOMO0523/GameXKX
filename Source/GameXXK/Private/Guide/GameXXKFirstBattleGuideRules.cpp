#include "Guide/GameXXKFirstBattleGuideRules.h"
#include "GameXXKMVPRules.h"
#include "GameXXKCardCatalog.h"
#include "Narrative/GameXXKMainStoryRules.h"

namespace
{
    const FName Attack(TEXT("Hero.Generic.QingFengYiShi"));
    const FName Heal(TEXT("Hero.Generic.GuiYuanShu"));
    const FName Armor(TEXT("Hero.Generic.HengJianShouShi"));
    bool Matches(const FGameXXKCardInstance& C,FName Id) {return C.OwnerUnitId==TEXT("Player") && C.CardId==Id;}
    void EnsureAvailable(FGameXXKBattleDeckState& D,FName Id)
    {
        for(const auto* Zone:{&D.Hand,&D.DrawPile,&D.DiscardPile})
            if(Zone->ContainsByPredicate([Id](const auto& C){return Matches(C,Id);}))return;
        FGameXXKCardInstance C;C.CardId=Id;C.OwnerUnitId=TEXT("Player");C.AcquisitionOrdinal=D.ActiveInstanceIds.Num();
        int32 N=0;
        do {C.InstanceId=FName(*FString::Printf(TEXT("FirstBattle.Borrowed.%s.%d"),*Id.ToString(),N++));}while(D.ActiveInstanceIds.Contains(C.InstanceId));
        C.SourceEntryId=C.InstanceId;
        // These ordinary instances live only in this battle deck, never in a permanent loadout.
        D.ActiveInstanceIds.Add(C.InstanceId);D.DrawPile.Insert(C,0);
    }
    void IntoHand(FGameXXKBattleDeckState& D,FName Id,const TArray<FName>& Keep)
    {
        EnsureAvailable(D,Id);
        if(D.Hand.ContainsByPredicate([Id](const auto& C){return Matches(C,Id);}))return;
        for(auto* Zone:{&D.DrawPile,&D.DiscardPile})
        {
            const int32 I=Zone->IndexOfByPredicate([Id](const auto& C){return Matches(C,Id);});
            if(I==INDEX_NONE)continue;
            const auto Card=(*Zone)[I];Zone->RemoveAt(I,1,EAllowShrinking::No);
            if(D.Hand.Num()>=D.HandLimit)
            {
                const int32 SwapIndex=D.Hand.IndexOfByPredicate([&](const auto& C){return !Keep.Contains(C.CardId);});
                if(SwapIndex!=INDEX_NONE){const auto Other=D.Hand[SwapIndex];D.Hand.RemoveAt(SwapIndex,1,EAllowShrinking::No);D.DrawPile.Insert(Other,0);}
            }
            D.Hand.Add(Card);return;
        }
    }
}

namespace GameXXKFirstBattleGuide
{
    FName Marker(const TCHAR* Topic){return FName(*(FString(TEXT("FirstBattle.V1."))+Topic));}
    bool Has(const FGameXXKRuntimeState& S,const TCHAR* T){return S.GuideProgress.CompletedGuideStepIds.Contains(Marker(T));}
    bool Eligible(const FGameXXKRuntimeState& S)
    {
        return S.GuideProgress.bFirstBattleGuideEnabled && !Has(S,TEXT("Complete"))
            && S.GuideProgress.Preference!=EGameXXKGuidePreference::ExperiencedPlayer
            && S.Training.bChallengeActive && !FGameXXKMainStoryRules::IsDedicatedJourney(S);
    }
    bool IsSupportCard(FName Id)
    {
        const auto* D=FGameXXKCardCatalog::FindCardDefinition(Id);if(!D || D->Owner!=EGameXXKCardOwner::Hero)return false;
        return D->Effects.ContainsByPredicate([](const auto& E)
        {return E.Type==EGameXXKCardEffectType::Heal || E.Type==EGameXXKCardEffectType::HealOrReverseWithMedicine
            || E.Type==EGameXXKCardEffectType::HealOrReverseFlat || E.Type==EGameXXKCardEffectType::AddArmor
            || E.Type==EGameXXKCardEffectType::GainArmorFromCurrentManaPercent;});
    }
    void PrepareOpening(FGameXXKRuntimeState& S,FGameXXKCardBattleRuntime& B)
    {
        if(!Eligible(S))return;
        auto& D=B.Deck;D.bFirstBattleGuidance=true;D.FirstBattleDrawPhase=1;
        S.GuideProgress.CompletedGuideStepIds.Add(Marker(TEXT("Started")));
        for(FName Id:{Attack,Heal,Armor})EnsureAvailable(D,Id);
        for(const auto* Zone:{&D.Hand,&D.DrawPile,&D.DiscardPile})for(const auto& C:*Zone)
            if(C.OwnerUnitId==TEXT("Player")&&IsSupportCard(C.CardId))D.FirstBattleDeferredCardIds.AddUnique(C.CardId);
        const int32 Desired=D.Hand.Num();
        for(int32 I=D.Hand.Num()-1;I>=0;--I)if(IsDeferred(D,D.Hand[I]))
        {const auto C=D.Hand[I];D.Hand.RemoveAt(I,1,EAllowShrinking::No);D.DrawPile.Insert(C,0);}
        IntoHand(D,Attack,{Attack});
        while(D.Hand.Num()<Desired)
        {
            int32 Pick=INDEX_NONE;
            for(int32 J=D.DrawPile.Num()-1;J>=0;--J)if(!IsDeferred(D,D.DrawPile[J])){Pick=J;break;}
            if(Pick==INDEX_NONE)break;
            D.Hand.Add(D.DrawPile[Pick]);D.DrawPile.RemoveAt(Pick,1,EAllowShrinking::No);
        }
    }
    bool IsDeferred(const FGameXXKBattleDeckState& D,const FGameXXKCardInstance& C)
    {return D.bFirstBattleGuidance&&D.FirstBattleDrawPhase==1&&C.OwnerUnitId==TEXT("Player")&&D.FirstBattleDeferredCardIds.Contains(C.CardId);}
    void DeliverSupportHand(FGameXXKBattleDeckState& D)
    {
        if(!D.bFirstBattleGuidance || D.FirstBattleDrawPhase!=1)return;
        D.FirstBattleDrawPhase=2;
        IntoHand(D,Heal,{Heal,Armor});IntoHand(D,Armor,{Heal,Armor});
    }
    bool Observe(FGameXXKRuntimeState& S)
    {
        const auto& B=S.CardRun.ActiveBattle;if(!B.Deck.bFirstBattleGuidance)return false;
        bool Changed=false;
        auto Add=[&](const TCHAR* T){const auto Id=Marker(T);if(!S.GuideProgress.CompletedGuideStepIds.Contains(Id)){S.GuideProgress.CompletedGuideStepIds.Add(Id);Changed=true;}};
        for(const auto& U:B.Units)
        {
            if(U.Side==EGameXXKCardTargetSide::Enemy && U.SettlementHealthLost>0)Add(TEXT("Damage"));
            if(U.Side==EGameXXKCardTargetSide::Party && U.SettlementHealingReceived>0)Add(TEXT("Heal"));
            if(U.Side==EGameXXKCardTargetSide::Party && U.SettlementArmorGenerated>0)Add(TEXT("Armor"));
        }
        if(B.Phase==EGameXXKCardBattlePhase::Enemy || B.RoundNumber>1)Add(TEXT("EndTurn"));
        if(Has(S,TEXT("Damage"))&&Has(S,TEXT("Qi"))&&Has(S,TEXT("EndTurn"))&&Has(S,TEXT("Heal"))
            &&Has(S,TEXT("Armor"))&&Has(S,TEXT("ArmorView"))&&Has(S,TEXT("Auto")))Add(TEXT("Complete"));
        return Changed;
    }
    void ObserveDamage(FGameXXKRuntimeState& S,const TArray<FGameXXKCardDamageResult>& Results)
    {
        if(!S.CardRun.ActiveBattle.Deck.bFirstBattleGuidance)return;
        for(const auto& R:Results)
        {
            const auto* Target=S.CardRun.ActiveBattle.Units.FindByPredicate([&](const auto& U){return U.UnitId==R.ResolvedTargetUnitId;});
            if(Target && Target->Side==EGameXXKCardTargetSide::Enemy)
            {S.GuideProgress.CompletedGuideStepIds.Add(Marker(TEXT("Damage")));break;}
        }
        Observe(S);
    }
    FName NextTopic(const FGameXXKRuntimeState& S)
    {
        if(!Eligible(S)||!S.CardRun.bHasActiveCardBattle||!S.CardRun.ActiveBattle.Deck.bFirstBattleGuidance)return NAME_None;
        const auto& B=S.CardRun.ActiveBattle;
        if(B.Phase!=EGameXXKCardBattlePhase::Player)return NAME_None;
        if(!Has(S,TEXT("Damage")))return Has(S,TEXT("AttackView"))?TEXT("Attack"):TEXT("AttackView");
        if(!Has(S,TEXT("Qi")))return TEXT("Qi");
        // The live round is authoritative even if an older progress marker was missed.
        if(B.RoundNumber<2 && !Has(S,TEXT("EndTurn")))return TEXT("EndTurn");
        if(B.RoundNumber<2)return NAME_None;
        const bool Hurt=B.Units.ContainsByPredicate([](const auto& U){return U.Side==EGameXXKCardTargetSide::Party && U.bLiving && U.HP<U.MaxHP;});
        if(!Has(S,TEXT("Heal"))&&Hurt)return TEXT("Heal");
        if(!Has(S,TEXT("Armor")))return TEXT("Armor");
        if(!Has(S,TEXT("ArmorView")) && B.Units.ContainsByPredicate([](const auto& U){return U.Side==EGameXXKCardTargetSide::Party && U.Armor>0;}))return TEXT("ArmorView");
        if(!Has(S,TEXT("Auto")))return TEXT("Auto");
        return NAME_None;
    }
    FName SuggestedCard(FName T){return T==TEXT("Heal")?Heal:T==TEXT("Armor")?Armor:Attack;}
}
