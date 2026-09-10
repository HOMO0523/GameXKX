#include "Narrative/GameXXKMainStoryCatalog.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "GameXXKTrainingRules.h"
#include "GameXXKEnemyCatalog.h"

namespace
{
#include "GameXXKMainStoryGenerated.inl"

	struct FStoryContent
	{
		TArray<FGameXXKMainStoryChapter> Chapters;
		TArray<FGameXXKMainStoryNode> Nodes;
		TMap<FName, FText> CharacterNames;
		FString Error;
	};

	void ReadIds(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, TArray<FName>& Out)
	{
		for (const TSharedPtr<FJsonValue>& Value : Object->GetArrayField(Key)) Out.Add(FName(*Value->AsString()));
	}
	bool ReadLines(const TArray<TSharedPtr<FJsonValue>>& Values,const TMap<FName,FText>& Names,TArray<FGameXXKMainStoryLine>& Out)
	{
		for(const auto& Value:Values)
		{
			const auto& Pair=Value->AsArray();if(Pair.Num()!=2)return false;
			FGameXXKMainStoryLine Line;Line.SpeakerId=FName(*Pair[0]->AsString());
			Line.SpeakerName=Names.FindRef(Line.SpeakerId);Line.Text=FText::FromString(Pair[1]->AsString());Out.Add(MoveTemp(Line));
		}
		return true;
	}

	const FStoryContent& Content()
	{
		static const FStoryContent Data = []
		{
			FStoryContent Out;
			FString Json;
			for (const TCHAR* Chunk : MainStoryJsonChunks) Json += Chunk;
			TSharedPtr<FJsonObject> Root;
			const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
			if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
			{
				Out.Error = TEXT("Embedded main-story JSON could not be read.");
				return Out;
			}
			for (const auto& Pair : Root->GetObjectField(TEXT("characters"))->Values)
			{
				Out.CharacterNames.Add(FName(*Pair.Key), FText::FromString(Pair.Value->AsObject()->GetStringField(TEXT("name"))));
			}
			for (const auto& ChapterValue : Root->GetArrayField(TEXT("chapters")))
			{
				const TSharedPtr<FJsonObject> Object = ChapterValue->AsObject();
				FGameXXKMainStoryChapter Chapter;
				Chapter.Id = FName(*Object->GetStringField(TEXT("id")));
				Chapter.StoryId = FName(*(TEXT("Story.Main.Cartography.") + Chapter.Id.ToString()));
				Chapter.MainlineEnd = FName(*Object->GetStringField(TEXT("mainline_end")));
				Chapter.Title = FText::FromString(Object->GetStringField(TEXT("title")));
				Chapter.Summary = FText::FromString(Object->GetStringField(TEXT("summary")));
				Chapter.StageNumber = static_cast<int32>(Object->GetNumberField(TEXT("stage_number")));
				for (const auto& NodeValue : Object->GetArrayField(TEXT("nodes")))
				{
					const TSharedPtr<FJsonObject> Item = NodeValue->AsObject();
					FGameXXKMainStoryNode Node;
					Node.Id = FName(*Item->GetStringField(TEXT("id")));
					Node.ChapterId = Chapter.Id;
					Node.StoryId = Chapter.StoryId;
					Node.StepId = FName(*(Node.Id.ToString() + TEXT(".Goal")));
					Node.Title = FText::FromString(Item->GetStringField(TEXT("title")));
					Node.Summary = FText::FromString(Item->GetStringField(TEXT("summary")));
					Node.Objective = FText::FromString(Item->GetStringField(TEXT("objective")));
					Node.Result = FText::FromString(Item->GetStringField(TEXT("result")));
					const FString Kind = Item->GetStringField(TEXT("kind"));
					Node.Kind = Kind == TEXT("JourneyBattle") ? EGameXXKMainStoryNodeKind::JourneyBattle
						: Kind == TEXT("JourneyInvestigation") ? EGameXXKMainStoryNodeKind::JourneyInvestigation
						: Kind == TEXT("Investigation") ? EGameXXKMainStoryNodeKind::Investigation : EGameXXKMainStoryNodeKind::Dialogue;
					Node.StageNumber = Chapter.StageNumber;
					Node.bOptional = Item->GetBoolField(TEXT("optional"));
					Node.bInsideJourney = Item->GetBoolField(TEXT("inside_journey"));
					ReadIds(Item, TEXT("requires_all"), Node.RequiresAll);
					ReadIds(Item, TEXT("requires_any"), Node.RequiresAny);
					for (const auto& LineValue : Item->GetArrayField(TEXT("lines")))
					{
						const auto& Pair = LineValue->AsArray();
						if (Pair.Num() != 2) { Out.Error = TEXT("Main-story line must have speaker and text."); return Out; }
						FGameXXKMainStoryLine Line;
						Line.SpeakerId = FName(*Pair[0]->AsString());
						Line.SpeakerName = Out.CharacterNames.FindRef(Line.SpeakerId);
						Line.Text = FText::FromString(Pair[1]->AsString());
						Node.Lines.Add(MoveTemp(Line));
					}
					const TArray<TSharedPtr<FJsonValue>>* AfterLines=nullptr;
					if(Item->TryGetArrayField(TEXT("after_battle_lines"),AfterLines)
						&&!ReadLines(*AfterLines,Out.CharacterNames,Node.AfterBattleLines))
					{Out.Error=TEXT("Post-battle line must have speaker and text.");return Out;}
					if(Item->HasField(TEXT("enemy_ids")))ReadIds(Item,TEXT("enemy_ids"),Node.EnemyDefinitionIds);
					for (const auto& OptionValue : Item->GetArrayField(TEXT("options")))
					{
						const auto Choice = OptionValue->AsObject();
						FGameXXKMainStoryOption Option;
						Option.Text = FText::FromString(Choice->GetStringField(TEXT("text")));
						Option.Feedback = FText::FromString(Choice->GetStringField(TEXT("feedback")));
						Option.bCorrect = Choice->GetBoolField(TEXT("correct"));
						Node.Options.Add(MoveTemp(Option));
					}
					for (const auto& Hint : Item->GetArrayField(TEXT("hints"))) Node.Hints.Add(FText::FromString(Hint->AsString()));
					const auto Reward = Item->GetObjectField(TEXT("reward"));
					Node.Gold = static_cast<int32>(Reward->GetNumberField(TEXT("gold")));
					Node.AdvancedBoxes = static_cast<int32>(Reward->GetNumberField(TEXT("advanced_boxes")));
					Node.NormalBoxes = static_cast<int32>(Reward->GetNumberField(TEXT("normal_boxes")));
					Node.BoxLevel = static_cast<int32>(Reward->GetNumberField(TEXT("box_level")));
					Node.Illustration = FSoftObjectPath(Item->GetObjectField(TEXT("art"))->GetStringField(TEXT("texture")));
					Chapter.Nodes.Add(Node.Id);
					Out.Nodes.Add(MoveTemp(Node));
				}
				Out.Chapters.Add(MoveTemp(Chapter));
			}
			return Out;
		}();
		return Data;
	}
}

const TArray<FGameXXKMainStoryChapter>& FGameXXKMainStoryCatalog::Chapters() { return Content().Chapters; }
const TArray<FGameXXKMainStoryNode>& FGameXXKMainStoryCatalog::Nodes() { return Content().Nodes; }
const FGameXXKMainStoryNode* FGameXXKMainStoryCatalog::FindNode(FName Id) { return Nodes().FindByPredicate([Id](const auto& N) { return N.Id == Id; }); }
const FGameXXKMainStoryChapter* FGameXXKMainStoryCatalog::FindChapter(FName Id) { return Chapters().FindByPredicate([Id](const auto& C) { return C.Id == Id; }); }
FText FGameXXKMainStoryCatalog::CharacterName(FName Id) { return Content().CharacterNames.FindRef(Id); }
FName FGameXXKMainStoryCatalog::StageId(const FGameXXKMainStoryChapter& Chapter) { return FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, Chapter.StageNumber); }

bool FGameXXKMainStoryCatalog::Validate(FString* OutError)
{
	FString Error = Content().Error;
	if (Error.IsEmpty() && (Chapters().Num() != 6 || Nodes().Num() != 61)) Error = TEXT("Main story must contain six chapters and sixty-one nodes.");
	TSet<FName> Seen;
	for (const auto& Node : Nodes())
	{
		if (Node.Id.IsNone() || Seen.Contains(Node.Id) || Node.Lines.Num() < 4 || Node.Hints.Num() != 3 || Node.Illustration.IsNull()) Error = TEXT("Main-story node identity or content is invalid.");
		Seen.Add(Node.Id);
		if(Node.Kind==EGameXXKMainStoryNodeKind::JourneyBattle)
		{
			if(Node.EnemyDefinitionIds.IsEmpty()||Node.EnemyDefinitionIds.Num()>3||Node.AfterBattleLines.IsEmpty())Error=TEXT("Story battle needs its own enemy roster and aftermath.");
			for(FName Id:Node.EnemyDefinitionIds)if(!FGameXXKEnemyCatalog::Find(Id))Error=TEXT("Story battle enemy does not exist.");
		}
		else if(!Node.EnemyDefinitionIds.IsEmpty()||!Node.AfterBattleLines.IsEmpty())Error=TEXT("Only battle tasks may define an enemy roster or aftermath.");
		for(const auto& Line:Node.AfterBattleLines)if(Line.SpeakerName.IsEmpty()||Line.Text.IsEmpty())Error=TEXT("Post-battle speaker or text is missing.");
		for (FName Id : Node.RequiresAll) if (!FindNode(Id) || FindNode(Id)->ChapterId != Node.ChapterId) Error = TEXT("Main-story prerequisite is invalid.");
		for (FName Id : Node.RequiresAny) if (!FindNode(Id) || FindNode(Id)->ChapterId != Node.ChapterId) Error = TEXT("Main-story branch prerequisite is invalid.");
		if (Node.IsInvestigation())
		{
			int32 Correct = 0;
			for (const auto& Option : Node.Options) Correct += Option.bCorrect ? 1 : 0;
			if (Node.Options.Num() < 2 || Correct != 1) Error = TEXT("Investigation must have exactly one evidence-supported answer.");
		}
	}
	for (const auto& Chapter : Chapters()) if (!Chapter.Nodes.Contains(Chapter.MainlineEnd)) Error = TEXT("Mainline ending is not a chapter node.");
	if (OutError) *OutError = Error;
	return Error.IsEmpty();
}
