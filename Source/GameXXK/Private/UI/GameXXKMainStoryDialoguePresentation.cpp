#include "UI/GameXXKMainStoryDialoguePresentation.h"
#include "GameXXKMVPRules.h"

namespace
{
	FSoftObjectPath Portrait(FName Speaker)
	{
		if(Speaker==TEXT("hero")) return FSoftObjectPath(TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_HeroFullBody.T_MasterV2_HeroFullBody"));
		// Dedicated cropped busts use the whole image; original playable/NPC
		// textures below retain their existing view-only portrait regions.
		static const TSet<FName> StoryBusts={TEXT("driver"),TEXT("mountain_man"),TEXT("abbot"),TEXT("innkeeper"),
			TEXT("woodcutter"),TEXT("hunter"),TEXT("willow_scholar"),TEXT("boatman"),TEXT("villager"),
			TEXT("elder"),TEXT("poor_traveler"),TEXT("porter")};
		if(StoryBusts.Contains(Speaker))
		{
			const FString Asset=TEXT("T_StoryPortrait_")+Speaker.ToString();
			return FSoftObjectPath(TEXT("/Game/GameXXK/UI/StoryPortraits/")+Asset+TEXT(".")+Asset);
		}
		const TMap<FName,FString> Names={{TEXT("you_bai"),TEXT("YueBai")},{TEXT("jin_gui"),TEXT("JinGui")},
			{TEXT("zhou_guang_zu"),TEXT("ZhouGuangZu")},{TEXT("tusi"),TEXT("TusiChief")},
			{TEXT("song_jin_bao"),TEXT("SongJinBao")},{TEXT("qiong_yao_er"),TEXT("QiongMeiEr")}};
		if(const FString* Name=Names.Find(Speaker))
		{
			const FString Asset=TEXT("T_Npc_")+*Name+TEXT("_IdleFirst");
			return FSoftObjectPath(TEXT("/Game/GameXXK/Characters/Follower/Textures/")+Asset+TEXT(".")+Asset);
		}
		// An off-screen speaker keeps their own name; never impersonate a different NPC.
		return FSoftObjectPath();
	}
}

bool GameXXKMainStoryDialoguePresentation::IsActive(const FGameXXKRuntimeState& State)
{
	const auto Phase=State.NarrativeProgress.MainStory.Phase;
	return !State.NarrativeProgress.MainStory.ActiveNodeId.IsNone() &&
		(Phase==EGameXXKMainStoryActivityPhase::Dialogue || Phase==EGameXXKMainStoryActivityPhase::Choice ||
		 Phase==EGameXXKMainStoryActivityPhase::ReadyToBattle);
}

FGameXXKDialoguePresentationView GameXXKMainStoryDialoguePresentation::LineView(const FGameXXKMainStoryNode& Node,int32 Index)
{
	FGameXXKDialoguePresentationView View;
	if(!Node.Lines.IsValidIndex(Index)) return View;
	const auto& Line=Node.Lines[Index];
	View.NodeId=FName(*FString::Printf(TEXT("%s.Line.%d"),*Node.Id.ToString(),Index));
	View.SpeakerDisplayName=Line.SpeakerName;
	View.Text=Line.Text;
	View.PortraitPath=Portrait(Line.SpeakerId);
	return View;
}

FGameXXKDialoguePresentationView GameXXKMainStoryDialoguePresentation::Build(const FGameXXKRuntimeState& State,const FText& Feedback)
{
	FGameXXKDialoguePresentationView View;
	const auto& Session=State.NarrativeProgress.MainStory;
	const auto* Node=FGameXXKMainStoryCatalog::FindNode(Session.ActiveNodeId);
	if(!Node || !IsActive(State))return View;
	if(Session.Phase==EGameXXKMainStoryActivityPhase::Dialogue)
		View=LineView(*Node,FMath::Clamp(Session.LineIndex,0,Node->Lines.Num()-1));
	else
	{
		View.NodeId=FName(*(Node->Id.ToString()+(Session.Phase==EGameXXKMainStoryActivityPhase::Choice?TEXT(".Choice"):TEXT(".Battle"))));
		View.SpeakerDisplayName=FGameXXKMainStoryCatalog::CharacterName(TEXT("hero"));
		View.PortraitPath=Portrait(TEXT("hero"));
		View.Text=Node->Objective;
		if(Session.Phase==EGameXXKMainStoryActivityPhase::ReadyToBattle)
		{
			FGameXXKDialogueVisibleOption Option;Option.OptionId=TEXT("MainStory.Battle");Option.Text=FText::FromString(TEXT("迎战"));
			View.Options.Add(Option);
		}
		else for(int32 I=0;I<Node->Options.Num();++I)
		{
			FGameXXKDialogueVisibleOption Option;Option.OptionId=FName(*FString::Printf(TEXT("MainStory.Choice.%d"),I));
			Option.Text=Node->Options[I].Text;View.Options.Add(Option);
		}
	}
	if(!Feedback.IsEmpty())View.Text=FText::FromString(View.Text.ToString()+TEXT("\n")+Feedback.ToString());
	return View;
}

bool GameXXKMainStoryDialoguePresentation::SameView(const FGameXXKDialoguePresentationView& A,const FGameXXKDialoguePresentationView& B)
{
	if(A.NodeId!=B.NodeId || A.PortraitPath!=B.PortraitPath || !A.SpeakerDisplayName.EqualTo(B.SpeakerDisplayName) ||
		!A.Text.EqualTo(B.Text) || A.Options.Num()!=B.Options.Num()) return false;
	for(int32 I=0;I<A.Options.Num();++I)
		if(A.Options[I].OptionId!=B.Options[I].OptionId || !A.Options[I].Text.EqualTo(B.Options[I].Text) ||
			A.Options[I].bEnabled!=B.Options[I].bEnabled || !A.Options[I].DisabledReason.EqualTo(B.Options[I].DisabledReason))return false;
	return true;
}

int32 GameXXKMainStoryDialoguePresentation::ChoiceIndex(FName OptionId)
{
	const FString Id=OptionId.ToString();const FString Prefix=TEXT("MainStory.Choice.");
	if(!Id.StartsWith(Prefix))return INDEX_NONE;
	const FString Index=Id.Mid(Prefix.Len());
	return Index.Len()==1 && Index[0]>=TEXT('0') && Index[0]<=TEXT('3') ? Index[0]-TEXT('0') : INDEX_NONE;
}
