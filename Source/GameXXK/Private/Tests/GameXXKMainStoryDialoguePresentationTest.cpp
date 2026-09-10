#include "Misc/AutomationTest.h"
#include "UI/GameXXKMainStoryDialoguePresentation.h"
#include "UI/GameXXKDialoguePanelWidget.h"
#include "GameXXKMVPRules.h"
#include "Blueprint/WidgetTree.h"
#include "UObject/StrongObjectPtr.h"
#include "Engine/Texture2D.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKMainStoryDialogueViewTest,"GameXXK.MainStory.ExistingDialoguePresentation",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKMainStoryDialogueViewTest::RunTest(const FString&)
{
	const auto* Node=FGameXXKMainStoryCatalog::FindNode(TEXT("S00-02"));
	if(!TestNotNull(TEXT("authored dialogue exists"),Node))return false;
	const int32 Index=Node->Lines.IndexOfByPredicate([](const auto& Line){return Line.SpeakerId==TEXT("you_bai");});
	FGameXXKRuntimeState State;
	State.NarrativeProgress.MainStory.ActiveNodeId=Node->Id;
	State.NarrativeProgress.MainStory.Phase=EGameXXKMainStoryActivityPhase::Dialogue;
	State.NarrativeProgress.MainStory.LineIndex=Index;
	const auto View=GameXXKMainStoryDialoguePresentation::Build(State,FText::GetEmpty());
	TestEqual(TEXT("speaker uses the approved display name"),View.SpeakerDisplayName.ToString(),FString(TEXT("幽白")));
	TestTrue(TEXT("speaker uses the project's existing character art"),View.PortraitPath.ToString().Contains(TEXT("T_Npc_YueBai_IdleFirst")));
	TestFalse(TEXT("dialogue portraits do not borrow generated story illustrations"),View.PortraitPath.ToString().Contains(TEXT("StoryNodes")));
	TestFalse(TEXT("view adapter creates no second dialogue save session"),State.DialogueSession.bActive);
	TestEqual(TEXT("view creation does not advance progress"),State.NarrativeProgress.MainStory.LineIndex,Index);
	TestEqual(TEXT("unknown option IDs cannot become choice zero"),GameXXKMainStoryDialoguePresentation::ChoiceIndex(TEXT("unrelated")),INDEX_NONE);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKMainStoryDialogueStableControlsTest,"GameXXK.MainStory.DialogueUpdatesPreserveControls",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKMainStoryDialogueStableControlsTest::RunTest(const FString&)
{
	TStrongObjectPtr<UGameXXKDialoguePanelWidget> Panel(NewObject<UGameXXKDialoguePanelWidget>());
	Panel->SetCompactLayout(true);
	UWidget* const Root=Panel->WidgetTree->RootWidget;
	int32 Advances=0,Pauses=0;
	Panel->SetAdvanceRequested(FGameXXKDialogueAdvanceRequested::CreateLambda([&Advances](){++Advances;}));
	Panel->SetPauseRequested(FGameXXKDialogueAdvanceRequested::CreateLambda([&Pauses](){++Pauses;}));
	FGameXXKDialoguePresentationView View;View.NodeId=TEXT("Test.Line.1");View.Text=FText::FromString(TEXT("先把话说清。"));
	Panel->Present(View);
	View.NodeId=TEXT("Test.Line.2");View.Text=FText::FromString(TEXT("再把路走稳。"));Panel->Present(View);
	TestEqual(TEXT("advancing changes text without reconstructing the UI root"),Panel->WidgetTree->RootWidget.Get(),Root);
	TestEqual(TEXT("the existing presenter shows the next line"),Panel->GetBodyTextForTest().ToString(),View.Text.ToString());
	Panel->RequestOptionForTest(-2);
	TestEqual(TEXT("close dispatches only pause"),Pauses,1);
	TestEqual(TEXT("close does not accidentally advance"),Advances,0);
	Panel->ClearPresentation();
	TestEqual(TEXT("closing hides only the dialogue presenter"),Panel->GetVisibility(),ESlateVisibility::Collapsed);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKFirstChapterSpeakerPortraitTest,"GameXXK.MainStory.FirstChapterSpeakerPortraits",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKFirstChapterSpeakerPortraitTest::RunTest(const FString&)
{
	const auto* Chapter=FGameXXKMainStoryCatalog::FindChapter(TEXT("S00"));
	if(!TestNotNull(TEXT("first chapter exists"),Chapter))return false;
	TSet<FName> Speakers;TSet<FString> Portraits;
	for(FName Id:Chapter->Nodes)
	{
		const auto* Node=FGameXXKMainStoryCatalog::FindNode(Id);
		if(!Node)continue;
		for(int32 I=0;I<Node->ReplayLineCount();++I)
		{
			const auto& Line=*Node->ReplayLine(I);
			const auto View=GameXXKMainStoryDialoguePresentation::LineView(*Node,I);
			if(Line.SpeakerId==TEXT("narrator"))
			{
				TestTrue(TEXT("narration stays portrait-free"),View.PortraitPath.IsNull());
				continue;
			}
			if(Speakers.Contains(Line.SpeakerId))continue;
			Speakers.Add(Line.SpeakerId);
			TestFalse(*FString::Printf(TEXT("speaking character %s has their own portrait"),*Line.SpeakerId.ToString()),View.PortraitPath.IsNull());
			Portraits.Add(View.PortraitPath.ToString());
			if(View.PortraitPath.ToString().Contains(TEXT("StoryPortraits")))
			{
				auto* Texture=LoadObject<UTexture2D>(nullptr,*View.PortraitPath.ToString());
				if(TestNotNull(TEXT("dedicated dialogue bust resolves to a real imported texture"),Texture))
				{
					// NullRHI may not allocate platform texture data. Check the
					// authored size here; the import/PIE audit checks BC7 resources.
#if WITH_EDITORONLY_DATA
					TestEqual(TEXT("dedicated bust source uses the prepared square width"),Texture->Source.GetSizeX(),int64(512));
					TestEqual(TEXT("dedicated bust source uses the prepared square height"),Texture->Source.GetSizeY(),int64(512));
#endif
				}
			}
		}
	}
	TestEqual(TEXT("chapter has hero, spirit and four distinct supporting speakers"),Speakers.Num(),6);
	TestEqual(TEXT("no supporting speaker borrows another character's face"),Portraits.Num(),Speakers.Num());
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKAllChapterSpeakerPortraitTest,"GameXXK.MainStory.AllChapterSpeakerPortraits",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKAllChapterSpeakerPortraitTest::RunTest(const FString&)
{
	TSet<FName> Speakers;TSet<FString> Portraits;int32 DedicatedBusts=0;
	for(int32 ChapterIndex=0;ChapterIndex<6;++ChapterIndex)
	{
		const auto* Chapter=FGameXXKMainStoryCatalog::FindChapter(FName(*FString::Printf(TEXT("S%02d"),ChapterIndex)));
		if(!TestNotNull(TEXT("authored chapter exists"),Chapter))continue;
		for(FName Id:Chapter->Nodes)
		{
			const auto* Node=FGameXXKMainStoryCatalog::FindNode(Id);
			if(!TestNotNull(TEXT("authored node exists"),Node))continue;
			for(int32 I=0;I<Node->ReplayLineCount();++I)
			{
				const FName Speaker=Node->ReplayLine(I)->SpeakerId;
				const auto View=GameXXKMainStoryDialoguePresentation::LineView(*Node,I);
				if(Speaker==TEXT("narrator"))
				{
					TestTrue(TEXT("narration never borrows a character portrait"),View.PortraitPath.IsNull());
					continue;
				}
				if(Speakers.Contains(Speaker))continue;
				Speakers.Add(Speaker);
				TestFalse(*FString::Printf(TEXT("all-chapter speaker %s has a portrait"),*Speaker.ToString()),View.PortraitPath.IsNull());
				Portraits.Add(View.PortraitPath.ToString());
				auto* Texture=Cast<UTexture2D>(View.PortraitPath.TryLoad());
				if(!TestNotNull(*FString::Printf(TEXT("speaker %s portrait loads"),*Speaker.ToString()),Texture))continue;
				if(View.PortraitPath.ToString().Contains(TEXT("StoryPortraits/")))
				{
					++DedicatedBusts;
					TestTrue(TEXT("supporting portrait belongs to the actual speaking role"),Texture->GetName().EndsWith(TEXT("_")+Speaker.ToString()));
#if WITH_EDITORONLY_DATA
					TestEqual(TEXT("supporting portrait source width"),Texture->Source.GetSizeX(),int64(512));
					TestEqual(TEXT("supporting portrait source height"),Texture->Source.GetSizeY(),int64(512));
#endif
				}
			}
		}
	}
	TestEqual(TEXT("all nineteen named speaking roles are covered"),Speakers.Num(),19);
	TestEqual(TEXT("each named role has its own distinct portrait"),Portraits.Num(),Speakers.Num());
	TestEqual(TEXT("all twelve supporting roles use dedicated cropped busts"),DedicatedBusts,12);
	return true;
}
#endif
