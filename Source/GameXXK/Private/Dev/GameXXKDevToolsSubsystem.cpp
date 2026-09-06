#include "Dev/GameXXKDevToolsSubsystem.h"
#include "SGameXXKDevWorkbench.h"
#include "GameXXKDevFixtures.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "MVP/GameXXKMVPPlayerController.h"
#include "MVP/GameXXKSaveMigration.h"
#include "GameXXKEquipmentCatalog.h"
#include "GameXXKEquipmentEconomyRules.h"
#include "GameXXKEquipmentSetCatalog.h"
#include "GameXXKAffixCatalog.h"
#include "GameXXKCompanionCatalog.h"
#include "GameXXKCompanionRules.h"
#include "GameXXKCardCatalog.h"
#include "GameXXKCardBattleAdapter.h"
#include "GameXXKCombatSimulationRules.h"
#include "GameXXKRelicCatalog.h"
#include "GameXXKRelicRules.h"
#include "GameXXKDesktopInventoryRules.h"
#include "UI/GameXXKCharacterUiPresentation.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/IInputProcessor.h"
#include "Widgets/SWindow.h"
#include "Widgets/SViewport.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/EngineVersion.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "GenericPlatform/GenericWindow.h"
#include "Modules/ModuleManager.h"
#include "Misc/SecureHash.h"

namespace DevTools
{
	using Object = TSharedPtr<FJsonObject>;
	FString LastInputTrace;
	FString BinaryFingerprint()
	{
		static const FString Fingerprint=[]()
		{
#if IS_MONOLITHIC
			FString Path=FPlatformProcess::ExecutablePath();
#else
			FString Path=FModuleManager::Get().GetModuleFilename(TEXT("GameXXK"));
			if(Path.IsEmpty() || !IFileManager::Get().FileExists(*Path))Path=FPlatformProcess::ExecutablePath();
#endif
			const FMD5Hash Hash=FMD5Hash::HashFile(*Path);
			return Hash.IsValid()?LexToString(Hash):FString(TEXT("unavailable"));
		}();
		return Fingerprint;
	}
	Object NewObject() { return MakeShared<FJsonObject>(); }
	FString Encode(const Object& Value)
	{
		FString Text; auto Writer = TJsonWriterFactory<>::Create(&Text);
		FJsonSerializer::Serialize(Value.ToSharedRef(), Writer); return Text;
	}
	Object Decode(const FString& Text)
	{
		Object Value; auto Reader = TJsonReaderFactory<>::Create(Text);
		return FJsonSerializer::Deserialize(Reader, Value) ? Value : nullptr;
	}
	template<typename T> Object Struct(const T& Value)
	{
		Object Result = NewObject();
		FJsonObjectConverter::UStructToJsonObject(T::StaticStruct(), &Value, Result.ToSharedRef(), 0, 0);
		return Result;
	}
	FString String(const Object& Args, const TCHAR* Key, const FString& Default=FString())
	{ FString Value; return Args.IsValid() && Args->TryGetStringField(Key, Value) ? Value : Default; }
	bool Flag(const Object& Args, const TCHAR* Key, bool Default=false)
	{ bool Value; return Args.IsValid() && Args->TryGetBoolField(Key, Value) ? Value : Default; }
	bool Integer(const Object& Args, const TCHAR* Key, int32 Default, int32 Min, int32 Max, int32& Out, FString& Error)
	{
		double Value = Default;
		if (Args->HasField(Key) && !Args->TryGetNumberField(Key, Value))
		{ Error = FString::Printf(TEXT("%s 必须是整数。"), Key); return false; }
		if (!FMath::IsFinite(Value) || Value < Min || Value > Max || Value != FMath::FloorToDouble(Value))
		{ Error = FString::Printf(TEXT("%s 应在 %d～%d 之间。"), Key, Min, Max); return false; }
		Out = static_cast<int32>(Value); return true;
	}
	template<typename T> bool Enum(const FString& Name, T& Out)
	{
		const UEnum* Type = StaticEnum<T>();
		const int64 Value = Type->GetValueByNameString(Name);
		if (Value == INDEX_NONE || Value == 0 || Name.EndsWith(TEXT("_MAX"))) return false;
		Out = static_cast<T>(Value); return true;
	}
	bool SafeName(const FString& Name)
	{
		if (Name.IsEmpty() || Name.Len() > 64 || Name.Contains(TEXT(".."))) return false;
		for (TCHAR C : Name) if (C < 32 || FString(TEXT("/\\:*?\"<>|")).Contains(FString::Chr(C))) return false;
		return true;
	}
	Object Stats(const FGameXXKCharacterStats& S)
	{
		Object O=NewObject(); O->SetNumberField(TEXT("health"),S.MaxHealth); O->SetNumberField(TEXT("mana"),S.MaxMana);
		O->SetNumberField(TEXT("attack"),S.Attack); O->SetNumberField(TEXT("defense"),S.Defense); O->SetNumberField(TEXT("speed"),S.Speed); return O;
	}
	bool IsCharacter(const FGameXXKRuntimeState& S, FName Id)
	{
		return Id == TEXT("Player") || FGameXXKCompanionCatalog::FindQuestNpcDefinition(Id)
			|| S.CardRun.CompanionRoster.PermanentCompanions.ContainsByPredicate([Id](const auto& C){return C.InstanceId==Id;});
	}
	using GameXXKDevFixtures::SetLevel;
	void RefreshPresentation(UGameXXKMVPSubsystem* MVP)
	{
		if (MVP && MVP->GetWorld())
			if (auto* PC=Cast<AGameXXKMVPPlayerController>(MVP->GetWorld()->GetFirstPlayerController())) PC->RefreshPlayerFlowWidgetsForTest();
	}
	Object Snapshot(const FGameXXKRuntimeState& S, const FGameXXKTrainingTravelRuntime& Travel)
	{
		Object O=NewObject(); O->SetNumberField(TEXT("schema"),1);
		O->SetStringField(TEXT("created_at"),FDateTime::UtcNow().ToIso8601());
		O->SetStringField(TEXT("engine"),FEngineVersion::Current().ToString());
		O->SetStringField(TEXT("build"),TEXT(__DATE__ " " __TIME__));
		O->SetStringField(TEXT("binary_md5"),BinaryFingerprint());
		O->SetObjectField(TEXT("state"),Struct(S)); O->SetObjectField(TEXT("travel"),Struct(Travel)); return O;
	}
	bool ReadSnapshot(const Object& O, FGameXXKRuntimeState& S, FGameXXKTrainingTravelRuntime& Travel, FString& Error)
	{
		int32 Schema;
		if(!O || !Integer(O,TEXT("schema"),1,1,1,Schema,Error))return false;
		const Object* StateObject=nullptr; const Object* TravelObject=nullptr;
		if (!O.IsValid() || !O->TryGetObjectField(TEXT("state"),StateObject)
			|| !O->TryGetObjectField(TEXT("travel"),TravelObject)
			|| !FJsonObjectConverter::JsonObjectToUStruct(StateObject->ToSharedRef(), &S, 0, 0)
			|| !FJsonObjectConverter::JsonObjectToUStruct(TravelObject->ToSharedRef(), &Travel, 0, 0))
		{ Error=TEXT("快照格式无效。"); return false; }
		return FGameXXKSaveMigration::ValidateRuntimeState(S,Error);
	}
}

struct FGameXXKDevToolsImpl
{
	TOptional<FGameXXKRuntimeState> Original, BattleStart, BeforeBattle;
	FGameXXKTrainingTravelRuntime OriginalTravel, BeforeBattleTravel;
	TSharedPtr<SWindow> Window;
	TSharedPtr<SGameXXKDevWorkbench> Panel;
	TSharedPtr<IInputProcessor> Input;
	FTSTicker::FDelegateHandle TickHandle;
	FKey Hotkey = EKeys::F10;
	FString Message=TEXT("选择物品或配装开始试验。F10收起面板，Esc关闭面板。");
	bool bLastCommandSucceeded=true;
	TArray<DevTools::Object> History;
	TMap<FString,FString> PendingResponses;
	float InboxElapsed=0;
	struct FBatch
	{
		FString Id;
		FGameXXKRuntimeState Source;
		FName Stage=TEXT("Training.Normal.1-1");
		int32 Encounter=0, FirstSeed=20260906, Total=1, Done=0, MaxRounds=100;
		bool bContinue=false, bCancelled=false;
		TArray<DevTools::Object> Results;
	};
	TOptional<FBatch> Batch;
	DevTools::Object LastReport;
};

class FGameXXKDevInput final : public IInputProcessor
{
	TWeakObjectPtr<UGameXXKDevToolsSubsystem> Tools;
	FKey Key;
public:
	FGameXXKDevInput(UGameXXKDevToolsSubsystem* In, FKey InKey):Tools(In),Key(InKey) {}
	virtual void Tick(float, FSlateApplication&, TSharedRef<ICursor>) override {}
	virtual bool HandleKeyDownEvent(FSlateApplication& App,const FKeyEvent& Event) override
	{
		auto* T=Tools.Get(); if (!T || !T->GetWorld() || !T->GetWorld()->IsGameWorld()) return false;
		auto Window=App.GetActiveTopLevelWindow();
		const FString SlateWindow=Window?Window->GetTitle().ToString():TEXT("none");
		// Composited desktop HUD activation can leave Slate's active-window cache
		// behind the native foreground HWND. Scope the shortcut to the real window.
		for(const auto& Candidate:App.GetTopLevelWindows())
			if(Candidate->GetNativeWindow() && Candidate->GetNativeWindow()->IsForegroundWindow()) {Window=Candidate;break;}
		if (!Window) return false;
		bool bGameWindow=Window->GetTitle().ToString()==TEXT("GameXXKDesktopOverlay") || Window->GetTitle().ToString()==TEXT("GameXXKDevTools");
		if (auto* Viewport=T->GetWorld()->GetGameViewport())
			if (auto Widget=Viewport->GetGameViewportWidget()) bGameWindow |= App.FindWidgetWindow(Widget.ToSharedRef())==Window;
		if (!bGameWindow) return false;
		if(Event.GetKey()==Key || Event.GetKey()==EKeys::Escape)
			DevTools::LastInputTrace=FString::Printf(TEXT("%s | Slate=%s | foreground=%s | repeat=%d"),*Event.GetKey().ToString(),*SlateWindow,*Window->GetTitle().ToString(),Event.IsRepeat());
		if (Event.GetKey()==Key)
		{ if (!Event.IsRepeat()) T->TogglePanel(); return true; }
		if (Event.GetKey()==EKeys::Escape && T->IsPanelOpen()) { T->ClosePanel(); return true; }
		return false;
	}
};

UGameXXKDevToolsSubsystem::UGameXXKDevToolsSubsystem():Impl(MakeShared<FGameXXKDevToolsImpl>()) {}
UGameXXKDevToolsSubsystem::~UGameXXKDevToolsSubsystem() = default;
bool UGameXXKDevToolsSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return Super::ShouldCreateSubsystem(Outer);
#endif
}
void UGameXXKDevToolsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
#if !UE_BUILD_SHIPPING
	Collection.InitializeDependency<UGameXXKMVPSubsystem>();
	IFileManager::Get().MakeDirectory(*(GetStorageDirectory()/TEXT("inbox")),true);
	IFileManager::Get().MakeDirectory(*(GetStorageDirectory()/TEXT("outbox")),true);
	IFileManager::Get().MakeDirectory(*(GetStorageDirectory()/TEXT("snapshots")),true);
	IFileManager::Get().MakeDirectory(*(GetStorageDirectory()/TEXT("reports")),true);
	FString Config;
	if (FFileHelper::LoadFileToString(Config,*(GetStorageDirectory()/TEXT("settings.json"))))
		if (auto O=DevTools::Decode(Config)) { FKey K(*DevTools::String(O,TEXT("hotkey"),TEXT("F10"))); if (K.IsValid()) Impl->Hotkey=K; }
	if (FSlateApplication::IsInitialized() && !GIsAutomationTesting)
	{
		Impl->Input=MakeShared<FGameXXKDevInput>(this,Impl->Hotkey);
		FSlateApplication::Get().RegisterInputPreProcessor(Impl->Input,0);
	}
	Impl->TickHandle=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this,&UGameXXKDevToolsSubsystem::TickDevelopment),0.1f);
#endif
}
void UGameXXKDevToolsSubsystem::Deinitialize()
{
	ClosePanel();
	Impl->Panel.Reset();
	if (Impl->TickHandle.IsValid()) FTSTicker::GetCoreTicker().RemoveTicker(Impl->TickHandle);
	if (Impl->Input && FSlateApplication::IsInitialized()) FSlateApplication::Get().UnregisterInputPreProcessor(Impl->Input);
	Impl->Input.Reset();
#if !UE_BUILD_SHIPPING
	if (auto* MVP=ResolveMVP(); MVP && Impl->Original.IsSet())
	{
		FString Error;
		if (MVP->ApplyDevelopmentState(Impl->Original.GetValue(),Error,&Impl->OriginalTravel)) MVP->SetDevelopmentWritesSuppressed(false);
	}
#endif
	Super::Deinitialize();
}
UGameXXKMVPSubsystem* UGameXXKDevToolsSubsystem::ResolveMVP() const
{ return MVPOverride ? MVPOverride.Get() : GetGameInstance() ? GetGameInstance()->GetSubsystem<UGameXXKMVPSubsystem>() : nullptr; }
bool UGameXXKDevToolsSubsystem::IsPanelOpen() const { return Impl->Window.IsValid(); }
bool UGameXXKDevToolsSubsystem::IsSessionActive() const { return Impl->Original.IsSet(); }
FString UGameXXKDevToolsSubsystem::GetLastMessage() const { return Impl->Message; }
bool UGameXXKDevToolsSubsystem::WasLastCommandSuccessful() const { return Impl->bLastCommandSucceeded; }
FString UGameXXKDevToolsSubsystem::GetStorageDirectory() const { return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("DevTools")); }
FString UGameXXKDevToolsSubsystem::GetStatusText() const
{
	FString Text=IsSessionActive() ? TEXT("临时试验中 · 玩家原档不写入") : TEXT("查看模式 · 尚未改变游戏");
	if (Impl->Batch.IsSet()) Text+=FString::Printf(TEXT("   |   批测 %d / %d%s"),Impl->Batch->Done,Impl->Batch->Total,Impl->Batch->bCancelled ? TEXT(" 已取消") : TEXT(""));
	return Text;
}
void UGameXXKDevToolsSubsystem::TogglePanel()
{
#if !UE_BUILD_SHIPPING
	if (IsPanelOpen()) { ClosePanel(); return; }
	if (!FSlateApplication::IsInitialized() || !ResolveMVP()) return;
	if(!Impl->Panel) Impl->Panel=SNew(SGameXXKDevWorkbench).Tools(this);
	else Impl->Panel->RefreshFromGame();
	Impl->Window=SNew(SWindow).Title(FText::FromString(TEXT("GameXXKDevTools")))
		.ClientSize(FVector2D(1180,700)).SizingRule(ESizingRule::UserSized)
		.SupportsMaximize(false).SupportsMinimize(false).IsTopmostWindow(true)
		[Impl->Panel.ToSharedRef()];
	Impl->Window->SetOnWindowClosed(FOnWindowClosed::CreateWeakLambda(this,[this](const TSharedRef<SWindow>& Closed){if(Impl->Window==Closed){Closed->SetContent(SNullWidget::NullWidget);Impl->Window.Reset();}}));
	FSlateApplication::Get().AddWindow(Impl->Window.ToSharedRef(),true);
#endif
}
void UGameXXKDevToolsSubsystem::ClosePanel()
{
	if (Impl->Window && FSlateApplication::IsInitialized())
	{
		auto Window=Impl->Window; Window->SetContent(SNullWidget::NullWidget);Impl->Window.Reset(); Window->RequestDestroyWindow();
		FSlateApplication::Get().SetAllUserFocusToGameViewport();
	}
}

FString UGameXXKDevToolsSubsystem::ExecuteJson(const FString& RequestJson)
{
	using namespace DevTools;
	Object Response=NewObject(); Response->SetNumberField(TEXT("schema"),1);
	auto Finish=[&](bool bOK,const FString& Message,const Object& Data=nullptr)
	{
		Response->SetBoolField(TEXT("ok"),bOK); Response->SetStringField(TEXT("message"),Message);
		Response->SetBoolField(TEXT("session_active"),IsSessionActive());
		if (Data) Response->SetObjectField(TEXT("data"),Data);
		const FString Op=DevTools::String(Response,TEXT("command"));
		if (!bOK || (Op!=TEXT("inspect") && Op!=TEXT("catalog") && Op!=TEXT("snapshot.list") && Op!=TEXT("simulate.status"))) { Impl->Message=Message;Impl->bLastCommandSucceeded=bOK; }
		return Encode(Response);
	};
#if UE_BUILD_SHIPPING
	return Finish(false,TEXT("此版本不包含开发工具。"));
#else
	if (RequestJson.Len()>16*1024*1024) return Finish(false,TEXT("命令超过16MB限制。"));
	Object Request=Decode(RequestJson);
	if (!Request) return Finish(false,TEXT("命令必须是有效JSON。"));
	const FString Command=String(Request,TEXT("command"));
	Response->SetStringField(TEXT("command"),Command);
	Response->SetStringField(TEXT("request_id"),String(Request,TEXT("request_id")));
	const Object* ArgsPtr=nullptr; Object Args=Request->TryGetObjectField(TEXT("args"),ArgsPtr) ? *ArgsPtr : NewObject();
	if (Request->HasField(TEXT("args")) && !ArgsPtr) return Finish(false,TEXT("args必须是JSON对象。"));
	for (const auto& Field:Request->Values)
		if (Field.Key!=TEXT("schema") && Field.Key!=TEXT("request_id") && Field.Key!=TEXT("command") && Field.Key!=TEXT("args")) return Finish(false,FString::Printf(TEXT("未知请求字段：%s"),*Field.Key));
	FString SchemaError;int32 Schema;
	if (!Integer(Request,TEXT("schema"),1,1,1,Schema,SchemaError)) return Finish(false,SchemaError);
	static const TMap<FString,FString> Schemas={
		{TEXT("help"),TEXT("")},{TEXT("catalog"),TEXT("query,category")},{TEXT("inspect"),TEXT("character,compact")},
		{TEXT("session.begin"),TEXT("")},{TEXT("session.restore"),TEXT("")},
		{TEXT("snapshot.save"),TEXT("name")},{TEXT("snapshot.load"),TEXT("name")},{TEXT("snapshot.list"),TEXT("")},{TEXT("snapshot.export"),TEXT("")},{TEXT("snapshot.import"),TEXT("scene")},
		{TEXT("item.give"),TEXT("id,quantity,character")},{TEXT("character.level"),TEXT("level,character")},{TEXT("party.select"),TEXT("character")},{TEXT("cards.set"),TEXT("character,cards")},
		{TEXT("equipment.create"),TEXT("id,character,level,quality,enhance,quantity,affix,gem,equip,character_level")},{TEXT("equipment.loadout"),TEXT("sets,character,level,quality,enhance,quantity,affix,gem,equip,character_level")},
		{TEXT("equipment.recommend_all"),TEXT("level,hero_set")},{TEXT("benchmark.prepare"),TEXT("role,npc,hero_direction,npc_omit,hero_cards,partner_cards,npc_cards,enhance,gems")},{TEXT("simulate.run"),TEXT("stage,encounter,seed,max_rounds,continue_current,scene")},
		{TEXT("heal"),TEXT("")},{TEXT("battle.start"),TEXT("stage,encounter,seed")},{TEXT("battle.restart"),TEXT("")},{TEXT("battle.return"),TEXT("")},{TEXT("battle.auto"),TEXT("enabled")},
		{TEXT("simulate.start"),TEXT("stage,encounter,seed,runs,max_rounds,continue_current,scene")},{TEXT("simulate.status"),TEXT("")},{TEXT("simulate.cancel"),TEXT("")},{TEXT("settings.key"),TEXT("key")}};
	const FString* Spec=Schemas.Find(Command);
	if (!Spec) return Finish(false,TEXT("未知指令，请使用help查看可用功能。"));
	TArray<FString> Allowed;Spec->ParseIntoArray(Allowed,TEXT(","));
	for (const auto& Field:Args->Values)
	{
		const FString FieldName(*Field.Key);
		if (!Allowed.Contains(FieldName)) return Finish(false,FString::Printf(TEXT("未知参数：%s"),*Field.Key));
		const bool bNumber=TArray<FString>{TEXT("npc_omit"),TEXT("quantity"),TEXT("level"),TEXT("character_level"),TEXT("quality"),TEXT("enhance"),TEXT("runs"),TEXT("max_rounds"),TEXT("encounter"),TEXT("seed")}.Contains(FieldName);
		const bool bBool=TArray<FString>{TEXT("gems"),TEXT("equip"),TEXT("enabled"),TEXT("compact"),TEXT("continue_current")}.Contains(FieldName);
		const EJson Expected=bNumber?EJson::Number:bBool?EJson::Boolean:Field.Key==TEXT("scene")?EJson::Object:(Field.Key==TEXT("sets")||Field.Key==TEXT("cards")||FieldName.EndsWith(TEXT("_cards")))?EJson::Array:EJson::String;
		if (!Field.Value || Field.Value->Type!=Expected) return Finish(false,FString::Printf(TEXT("参数类型不正确：%s"),*Field.Key));
	}
	auto* MVP=ResolveMVP(); if (!MVP) return Finish(false,TEXT("请先进入游戏。"));
	const FGameXXKRuntimeState& Current=MVP->GetRuntimeState();
	FString Error;
	auto BeginSession=[&]()
	{
		if (!Impl->Original.IsSet())
		{
			Impl->Original=Current; Impl->OriginalTravel=MVP->GetTrainingTravelRuntimeCopy();
			MVP->SetDevelopmentWritesSuppressed(true);
		}
	};
	if (Command==TEXT("help"))
	{
		Object D=NewObject(); TArray<TSharedPtr<FJsonValue>> Names;
		TArray<FString> Commands;Schemas.GetKeys(Commands);Commands.Sort();Object Arguments=NewObject();
		for(const auto& N:Commands){Names.Add(MakeShared<FJsonValueString>(N));Arguments->SetStringField(N,Schemas[N]);}
		D->SetObjectField(TEXT("arguments"),Arguments);
		D->SetArrayField(TEXT("commands"),Names); D->SetStringField(TEXT("directory"),GetStorageDirectory()); return Finish(true,TEXT("开发指令目录"),D);
	}
	if (Command==TEXT("catalog"))
	{
		Object D=NewObject(); TArray<TSharedPtr<FJsonValue>> Rows;
		const FString Query=String(Args,TEXT("query")), Category=String(Args,TEXT("category"));
		auto Add=[&](const FString& Id,const FString& Name,const FString& Kind,const FString& Description)
		{
			if ((!Category.IsEmpty() && Category!=Kind) || (!Query.IsEmpty() && !Id.Contains(Query) && !Name.Contains(Query))) return;
			Object R=NewObject(); R->SetStringField(TEXT("id"),Id);R->SetStringField(TEXT("name"),Name);R->SetStringField(TEXT("category"),Kind);R->SetStringField(TEXT("description"),Description); Rows.Add(MakeShared<FJsonValueObject>(R));
		};
		Add(TEXT("Currency.Gold"),TEXT("金币"),TEXT("item"),TEXT("永久金币 · 测试会话内使用"));
		for (FName Id:UGameXXKMVPRules::GetKnownItemIds())
		{
			if (Id==UGameXXKMVPRules::ItemHealingPowder() || FGameXXKEquipmentCatalog::FindDefinition(Id)) continue;
			bool bFound=false;auto Def=UGameXXKMVPRules::GetItemDef(Id,bFound);
			if (bFound) Add(Id.ToString(),Def.DisplayName.ToString(),TEXT("item"),TEXT("游戏内道具 / 材料"));
		}
		for (const auto& E:FGameXXKEquipmentCatalog::GetPackageDefinitions())
			if (E.Set!=EGameXXKEquipmentSet::Legacy) Add(E.Id.ToString(),E.DisplayName.ToString(),TEXT("equipment"),FGameXXKEquipmentSetCatalog::GetSetDisplayName(E.Set).ToString());
		for (const auto& R:FGameXXKRelicCatalog::GetAllDefinitions()) Add(R.Id.ToString(),R.DisplayName.ToString(),TEXT("relic"),R.Description.ToString());
		for (const auto& S:FGameXXKTrainingRules::GetStageDefinitions()) Add(S.StageId.ToString(),S.DisplayName.ToString(),TEXT("stage"),FString::Printf(TEXT("敌方等级 %d"),S.CombatLevel));
		D->SetArrayField(TEXT("entries"),Rows);return Finish(true,FString::Printf(TEXT("找到 %d 条记录"),Rows.Num()),D);
	}
	if (Command==TEXT("inspect"))
	{
		Object D=NewObject();TArray<TSharedPtr<FJsonValue>> Characters;
		auto Add=[&](FName Id,int32 Level)
		{
			Object O=NewObject();O->SetStringField(TEXT("id"),Id.ToString());O->SetStringField(TEXT("name"),GameXXKCharacterUiPresentation::GetDisplayName(MVP,Id));O->SetNumberField(TEXT("level"),Level);Characters.Add(MakeShared<FJsonValueObject>(O));
		};
		Add(TEXT("Player"),Current.PlayerLevel);
		for (const auto& C:Current.CardRun.CompanionRoster.PermanentCompanions) Add(C.InstanceId,C.Level);
		for (const auto& N:FGameXXKCompanionCatalog::GetQuestNpcDefinitions())
		{
			const auto* P=Current.CardRun.PartySelection.QuestNpcProgressions.Find(N.NpcId);Add(N.NpcId,P ? P->Level : 1);
		}
		D->SetArrayField(TEXT("characters"),Characters);
		FName Character(*String(Args,TEXT("character"),TEXT("Player")));
		FGameXXKEquipmentLoadoutSnapshot S;
		if (MVP->GetEquipmentLoadoutSnapshot(Character,S))
		{
			D->SetObjectField(TEXT("bare"),Stats(S.BareStats));D->SetObjectField(TEXT("equipment"),Stats(S.EnhancedEquipmentBaseStats));
			D->SetObjectField(TEXT("gems"),Stats(S.SocketGemFlatStats));D->SetObjectField(TEXT("final"),Stats(S.AttributesBeforeRoute));
			D->SetObjectField(TEXT("loadout"),Struct(S));
			FGameXXKCharacterStats Delta=S.AttributesBeforeRoute;
			Delta.MaxHealth-=S.BareStats.MaxHealth+S.EnhancedEquipmentBaseStats.MaxHealth+S.SocketGemFlatStats.MaxHealth;
			Delta.MaxMana-=S.BareStats.MaxMana+S.EnhancedEquipmentBaseStats.MaxMana+S.SocketGemFlatStats.MaxMana;
			Delta.Attack-=S.BareStats.Attack+S.EnhancedEquipmentBaseStats.Attack+S.SocketGemFlatStats.Attack;
			Delta.Defense-=S.BareStats.Defense+S.EnhancedEquipmentBaseStats.Defense+S.SocketGemFlatStats.Defense;
			Delta.Speed-=S.BareStats.Speed+S.EnhancedEquipmentBaseStats.Speed+S.SocketGemFlatStats.Speed;
			D->SetObjectField(TEXT("modifiers"),Stats(Delta));
		}
		D->SetNumberField(TEXT("gold"),Current.PlayerGold);D->SetNumberField(TEXT("health"),Current.PlayerHP);D->SetNumberField(TEXT("max_health"),Current.PlayerMaxHP);D->SetStringField(TEXT("input_trace"),LastInputTrace);
		D->SetBoolField(TEXT("battle_active"),Current.CardRun.bHasActiveCardBattle);D->SetBoolField(TEXT("auto_play"),MVP->IsBattleAutoPlayEnabled());
		if (!Flag(Args,TEXT("compact"))) { D->SetObjectField(TEXT("battle"),Struct(Current.CardRun.ActiveBattle));D->SetObjectField(TEXT("travel"),Struct(MVP->GetTrainingTravelRuntimeCopy())); }
		Object Live=NewObject();
		if (Current.CardRun.bHasActiveCardBattle)
		{
			for (const auto& U:Current.CardRun.ActiveBattle.Units) if (U.UnitId==Character)
			{ Live->SetNumberField(TEXT("health"),U.HP);Live->SetNumberField(TEXT("mana"),U.Mana);Live->SetNumberField(TEXT("armor"),U.Armor);Live->SetStringField(TEXT("context"),FString::Printf(TEXT("战斗 · 第%d回合"),Current.CardRun.ActiveBattle.RoundNumber)); }
		}
		else for (const auto& U:MVP->GetTrainingTravelRuntimeCopy().PartyUnits) if (U.UnitId==Character)
		{ Live->SetNumberField(TEXT("health"),U.HP);Live->SetStringField(TEXT("context"),TEXT("当前游历")); }
		D->SetObjectField(TEXT("live"),Live);D->SetStringField(TEXT("storage"),GetStorageDirectory());
		D->SetStringField(TEXT("character_name"),GameXXKCharacterUiPresentation::GetDisplayName(MVP,Character));
		return Finish(true,TEXT("实时属性与战斗状态"),D);
	}
	if (Command==TEXT("session.begin")) { BeginSession();return Finish(true,TEXT("已开始临时试验；原进度保留。")); }
	if (Command==TEXT("session.restore"))
	{
		if (!Impl->Original.IsSet()) return Finish(false,TEXT("没有需要恢复的试验会话。"));
		if (!MVP->ApplyDevelopmentState(Impl->Original.GetValue(),Error,&Impl->OriginalTravel)) return Finish(false,Error);
		Impl->Original.Reset();Impl->BeforeBattle.Reset();Impl->BattleStart.Reset();MVP->SetDevelopmentWritesSuppressed(false);RefreshPresentation(MVP);
		return Finish(true,TEXT("已返回进入试验前的原进度。"));
	}
	if (Command==TEXT("snapshot.export")) return Finish(true,TEXT("已导出当前场景。"),Snapshot(Current,MVP->GetTrainingTravelRuntimeCopy()));
	if (Command==TEXT("snapshot.import"))
	{
		const Object* Scene=nullptr;FGameXXKRuntimeState S;FGameXXKTrainingTravelRuntime Travel;
		if (!Args->TryGetObjectField(TEXT("scene"),Scene) || !ReadSnapshot(*Scene,S,Travel,Error)) return Finish(false,Error.IsEmpty()?TEXT("请提供有效的scene对象。"):Error);
		BeginSession();if (!MVP->ApplyDevelopmentState(S,Error,&Travel)) return Finish(false,Error);RefreshPresentation(MVP);return Finish(true,TEXT("已导入测试场景。"));
	}
	if (Command==TEXT("snapshot.list"))
	{
		TArray<FString> Files;IFileManager::Get().FindFiles(Files,*(GetStorageDirectory()/TEXT("snapshots/*.json")),true,false);Files.Sort();
		Object D=NewObject();TArray<TSharedPtr<FJsonValue>> Names;for (const auto& F:Files) Names.Add(MakeShared<FJsonValueString>(FPaths::GetBaseFilename(F)));D->SetArrayField(TEXT("names"),Names);return Finish(true,TEXT("保存的试验快照"),D);
	}
	if (Command==TEXT("snapshot.save") || Command==TEXT("snapshot.load"))
	{
		const FString Name=String(Args,TEXT("name"));if (!SafeName(Name)) return Finish(false,TEXT("请输入1～64字的快照名称，不含路径符号。"));
		const FString Path=GetStorageDirectory()/TEXT("snapshots")/(Name+TEXT(".json"));
		if (Command==TEXT("snapshot.save"))
		{
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path),true);
			if (!FFileHelper::SaveStringToFile(Encode(Snapshot(Current,MVP->GetTrainingTravelRuntimeCopy())),*Path,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)) return Finish(false,TEXT("快照写入失败。"));
			return Finish(true,TEXT("已保存试验快照：")+Name);
		}
		FString File;FGameXXKRuntimeState S;FGameXXKTrainingTravelRuntime Travel;
		if (!FFileHelper::LoadFileToString(File,*Path) || !ReadSnapshot(Decode(File),S,Travel,Error)) return Finish(false,Error.IsEmpty()?TEXT("找不到快照。"):Error);
		BeginSession();if (!MVP->ApplyDevelopmentState(S,Error,&Travel)) return Finish(false,Error);RefreshPresentation(MVP);return Finish(true,TEXT("已载入试验快照：")+Name);
	}
	if (Command==TEXT("settings.key"))
	{
		const FString Name=String(Args,TEXT("key"));FKey Key(*Name);
		int32 Index=0;
		if (!Key.IsValid() || !Name.StartsWith(TEXT("F")) || !LexTryParseString(Index,*Name.Mid(1)) || Index<1 || Index>12) return Finish(false,TEXT("请选择F1～F12中的一个功能键。"));
		Object D=NewObject();D->SetStringField(TEXT("hotkey"),Name);
		if (!FFileHelper::SaveStringToFile(Encode(D),*(GetStorageDirectory()/TEXT("settings.json")))) return Finish(false,TEXT("设置保存失败。"));
		Impl->Hotkey=Key;
		if (Impl->Input && FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().UnregisterInputPreProcessor(Impl->Input);Impl->Input=MakeShared<FGameXXKDevInput>(this,Key);FSlateApplication::Get().RegisterInputPreProcessor(Impl->Input,0);
		}
		return Finish(true,TEXT("开关键已改为 ")+Name);
	}
	if (Command==TEXT("benchmark.prepare"))
	{
		EGameXXKCharacterRole Role=EGameXXKCharacterRole::Invalid; int32 Omit=3;
		if(!Enum(String(Args,TEXT("role"),TEXT("Blade")),Role)||!Integer(Args,TEXT("npc_omit"),3,0,3,Omit,Error))return Finish(false,TEXT("职业或NPC省略卡序号无效。"));
		FGameXXKRuntimeState State;
		if(!GameXXKDevFixtures::BuildBenchmark(Role,FName(*String(Args,TEXT("npc"),TEXT("Npc.TusiChief"))),String(Args,TEXT("hero_direction"),TEXT("Blade")),Omit,State,Error))return Finish(false,Error);
		int32 Enhancement=10;if(!Integer(Args,TEXT("enhance"),10,0,10,Enhancement,Error))return Finish(false,Error);
		for(auto& Item:State.EquipmentCollection.EquipmentInstances){Item.EnhancementLevel=Enhancement;if(!Flag(Args,TEXT("gems"),true))for(auto& Socket:Item.SocketedGems)Socket=FGameXXKSocketedGem();}
		if(!FGameXXKEquipmentEconomyRules::SynchronizeRuntimeMirrors(State))return Finish(false,TEXT("标准场景属性同步失败。"));State.PlayerHP=State.PlayerMaxHP;State.PlayerMP=State.PlayerMaxMP;
		for(const TCHAR* Key:{TEXT("hero_cards"),TEXT("partner_cards"),TEXT("npc_cards")})
		{
			const TArray<TSharedPtr<FJsonValue>>* Values=nullptr;if(!Args->TryGetArrayField(Key,Values))continue;
			TArray<FName> Cards;for(const auto& V:*Values){FString Id;if(!V->TryGetString(Id))return Finish(false,TEXT("卡牌ID必须是文本。"));Cards.Add(FName(*Id));}
			if(FString(Key)==TEXT("hero_cards")){if(!FGameXXKCardBattleAdapter::SetHeroSelectedCards(State,Cards,&Error))return Finish(false,Error);}
			else if(FString(Key)==TEXT("partner_cards"))
			{auto* C=State.CardRun.CompanionRoster.PermanentCompanions.FindByPredicate([](const auto& V){return V.bIsActive;});if(!C||!FGameXXKCompanionRules::SetSelectedPersonalCards(*C,Cards,&Error))return Finish(false,Error);}
			else if(!FGameXXKCardBattleAdapter::SetQuestNpcForCurrentRun(State,State.CardRun.PartySelection.QuestNpc.NpcId,Cards,&Error))return Finish(false,Error);
		}
		if(!FGameXXKSaveMigration::ValidateRuntimeState(State,Error))return Finish(false,Error);
		Object D=Snapshot(State,FGameXXKTrainingTravelRuntime());TArray<TSharedPtr<FJsonValue>> Cards;
		for(const auto& C:FGameXXKCardCatalog::GetAllCardDefinitions())Cards.Add(MakeShared<FJsonValueObject>(Struct(C)));
		D->SetArrayField(TEXT("card_catalog"),Cards);D->SetStringField(TEXT("fixture"),TEXT("level100-treasure-v2"));D->SetObjectField(TEXT("fixture_parameters"),Args);
		return Finish(true,TEXT("已构造独立的标准测试阵容，未修改当前游戏。"),D);
	}
	if(Command==TEXT("simulate.run"))
	{
		FGameXXKRuntimeState State=Current;FGameXXKTrainingTravelRuntime Ignored;const Object* Scene=nullptr;
		if(Args->TryGetObjectField(TEXT("scene"),Scene)&&!ReadSnapshot(*Scene,State,Ignored,Error))return Finish(false,Error);
		int32 Seed,Encounter,MaxRounds;
		if(!Integer(Args,TEXT("seed"),20260906,1,MAX_int32,Seed,Error)||!Integer(Args,TEXT("encounter"),7,1,7,Encounter,Error)||!Integer(Args,TEXT("max_rounds"),60,1,200,MaxRounds,Error))return Finish(false,Error);
		bool OK=Flag(Args,TEXT("continue_current"))||UGameXXKMVPSubsystem::BuildDevelopmentTrainingBattle(State,FName(*String(Args,TEXT("stage"),TEXT("Training.Hell.3-1"))),Encounter-1,Seed,Error);
		Object D=NewObject();D->SetObjectField(TEXT("opening"),Struct(State.CardRun.ActiveBattle));D->SetStringField(TEXT("binary_md5"),BinaryFingerprint());D->SetNumberField(TEXT("seed"),Seed);
		FGameXXKSimulationMetrics M;TArray<FGameXXKSimulationTraceEntry> Trace;
		if(OK){FGameXXKSimulationScenario Scenario;Scenario.InitialRuntimeState=State;Scenario.bResumeActiveBattle=true;Scenario.Seed=Seed;Scenario.MaxRounds=MaxRounds;Scenario.Terrain=State.CardRun.ActiveBattle.Terrain;OK=FGameXXKCombatSimulationRules::RunScenario(Scenario,M,Trace,&Error);}
		D->SetBoolField(TEXT("ok"),OK);D->SetStringField(TEXT("error"),Error);D->SetStringField(TEXT("outcome"),!OK?TEXT("error"):M.bStalemateResolved?TEXT("stalemate"):M.bVictory?TEXT("victory"):TEXT("defeat"));D->SetObjectField(TEXT("metrics"),Struct(M));
		TArray<TSharedPtr<FJsonValue>> Entries;for(const auto& T:Trace)Entries.Add(MakeShared<FJsonValueObject>(Struct(T)));D->SetArrayField(TEXT("trace"),Entries);
		return Finish(true,TEXT("真实战斗运行完成；请检查data.ok与outcome。"),D);
	}
	if (Command==TEXT("simulate.status"))
	{
		Object D=NewObject();if (Impl->Batch.IsSet()) { D->SetStringField(TEXT("id"),Impl->Batch->Id);D->SetNumberField(TEXT("done"),Impl->Batch->Done);D->SetNumberField(TEXT("total"),Impl->Batch->Total);D->SetBoolField(TEXT("cancelled"),Impl->Batch->bCancelled);D->SetBoolField(TEXT("running"),Impl->Batch->Done<Impl->Batch->Total && !Impl->Batch->bCancelled); }
		if (Impl->LastReport) D->SetObjectField(TEXT("report"),Impl->LastReport);return Finish(true,TEXT("批量测试状态"),D);
	}
	if (Command==TEXT("simulate.cancel")) { if (Impl->Batch.IsSet()) Impl->Batch->bCancelled=true;return Finish(true,TEXT("已停止继续提交模拟场次，完成记录保留。")); }
	if (Command==TEXT("simulate.start"))
	{
		if (Impl->Batch.IsSet() && Impl->Batch->Done<Impl->Batch->Total && !Impl->Batch->bCancelled) return Finish(false,TEXT("已有批测进行中，请等待完成或取消。"));
		FGameXXKDevToolsImpl::FBatch B;B.Id=FGuid::NewGuid().ToString(EGuidFormats::Digits);B.Source=Current;
		const Object* InputScene=nullptr;FGameXXKTrainingTravelRuntime IgnoredTravel;
		if (Args->TryGetObjectField(TEXT("scene"),InputScene) && !ReadSnapshot(*InputScene,B.Source,IgnoredTravel,Error)) return Finish(false,Error);
		if (!Integer(Args,TEXT("runs"),100,1,1000,B.Total,Error) || !Integer(Args,TEXT("seed"),20260906,1,MAX_int32-1000,B.FirstSeed,Error)
			|| !Integer(Args,TEXT("encounter"),1,1,7,B.Encounter,Error) || !Integer(Args,TEXT("max_rounds"),100,1,200,B.MaxRounds,Error)) return Finish(false,Error);
		--B.Encounter;B.Stage=FName(*String(Args,TEXT("stage"),TEXT("Training.Normal.1-1")));B.bContinue=Flag(Args,TEXT("continue_current"));
		FGameXXKTrainingStageDefinition Stage;if (!FGameXXKTrainingRules::TryGetStageDefinition(B.Stage,Stage)) return Finish(false,TEXT("关卡不存在。"));
		if (B.bContinue && (!B.Source.CardRun.bHasActiveCardBattle || B.Total!=1)) return Finish(false,TEXT("继续当前战斗只运行一次，请设置runs=1。"));
		Impl->Batch=MoveTemp(B);Impl->LastReport.Reset();Object D=NewObject();D->SetStringField(TEXT("id"),Impl->Batch->Id);
		return Finish(true,TEXT("已开始后台批测；正在游玩的状态保持独立。"),D);
	}
	if (Command==TEXT("battle.auto"))
	{
		if (!Current.CardRun.bHasActiveCardBattle) return Finish(false,TEXT("请先进入测试战斗。"));
		BeginSession();const bool Enabled=Flag(Args,TEXT("enabled"),true);MVP->SetBattleAutoPlayEnabled(Enabled);return Finish(true,Enabled?TEXT("自动出牌已开启。"):TEXT("自动出牌已关闭。"));
	}
	if (Command==TEXT("battle.return") || Command==TEXT("battle.restart"))
	{
		const auto& Target=Command==TEXT("battle.return") ? Impl->BeforeBattle : Impl->BattleStart;
		if (!Target.IsSet()) return Finish(false,TEXT("尚未建立测试战斗。"));
		BeginSession();if (!MVP->ApplyDevelopmentState(Target.GetValue(),Error,Command==TEXT("battle.return")?&Impl->BeforeBattleTravel:nullptr)) return Finish(false,Error);
		MVP->SetBattleAutoPlayEnabled(false);RefreshPresentation(MVP);return Finish(true,Command==TEXT("battle.return")?TEXT("已返回战前配装状态。"):TEXT("已使用相同种子重开本场。"));
	}
	FGameXXKRuntimeState Candidate=Current;
	const FName Character(*String(Args,TEXT("character"),TEXT("Player")));
	TArray<FName> Created;
	if(Command==TEXT("equipment.recommend_all"))
	{
		int32 Level=100;EGameXXKEquipmentSet HeroSet=EGameXXKEquipmentSet::PoJun;
		if(!Integer(Args,TEXT("level"),100,1,100,Level,Error)||!Enum(String(Args,TEXT("hero_set"),TEXT("PoJun")),HeroSet))return Finish(false,TEXT("等级或主角套装无效。"));
		if(!GameXXKDevFixtures::RecommendAll(Candidate,Level,HeroSet,Created,Error))return Finish(false,Error);
	}
	else if (Command==TEXT("item.give"))
	{
		int32 Quantity;if (!Integer(Args,TEXT("quantity"),1,1,1000000,Quantity,Error)) return Finish(false,Error);
		FName Id(*String(Args,TEXT("id")));
		if (Id==TEXT("Currency.Gold"))
		{ if (Candidate.PlayerGold>MAX_int32-Quantity) return Finish(false,TEXT("金币数量超出范围。"));Candidate.PlayerGold+=Quantity; }
		else if (Id==UGameXXKMVPRules::ItemHealingPowder()) return Finish(false,TEXT("该药品已从当前玩法退役。"));
		else if (FGameXXKRelicCatalog::FindDefinition(Id))
		{
			if (!Candidate.CardRun.bLoadoutLockedForRoute) return Finish(false,TEXT("遗物属于局内，请先进入测试战斗。"));
			if (Quantity>32) return Finish(false,TEXT("一次最多添加32件遗物。"));
			for (int32 I=0;I<Quantity;++I) if (!FGameXXKRelicRules::AcquireRelic(Candidate,Id,&Error)) return Finish(false,Error);
		}
		else if (!UGameXXKMVPRules::AddItem(Candidate,Id,Quantity)) return Finish(false,TEXT("物品不存在、容器已满或该类型需要专用生成器。"));
	}
	else if (Command==TEXT("character.level"))
	{
		if (Candidate.CardRun.bLoadoutLockedForRoute) return Finish(false,TEXT("请先返回战前整备，再调整角色等级。"));
		int32 Level;if (!Integer(Args,TEXT("level"),100,1,FGameXXKCharacterStatRules::MaxCharacterLevel,Level,Error)) return Finish(false,Error);
		if (!SetLevel(Candidate,Character,Level,Error)) return Finish(false,Error);
	}
	else if (Command==TEXT("party.select"))
	{
		if (Candidate.CardRun.bLoadoutLockedForRoute) return Finish(false,TEXT("请先返回战前整备。"));
		if (FGameXXKCompanionCatalog::FindQuestNpcDefinition(Character))
		{ if (!FGameXXKCardBattleAdapter::SetQuestNpcForCurrentRun(Candidate,Character,{},&Error)) return Finish(false,Error); }
		else
		{
			// Use the public formation transaction on a temporary subsystem state only after validation.
			if (!IsCharacter(Candidate,Character) || Character==TEXT("Player")) return Finish(false,TEXT("请选择一个伙伴或NPC。"));
			BeginSession();if (!MVP->SetActivePermanentCompanion(Character)) return Finish(false,TEXT("无法更换出战伙伴。"));RefreshPresentation(MVP);return Finish(true,TEXT("出战伙伴已更新。"));
		}
	}
	else if (Command==TEXT("cards.set"))
	{
		const TArray<TSharedPtr<FJsonValue>>* Values=nullptr;TArray<FName> Ids;
		if (!Args->TryGetArrayField(TEXT("cards"),Values)) return Finish(false,TEXT("请提供cards数组。"));
		for (const auto& V:*Values) { FString Id;if (!V->TryGetString(Id)) return Finish(false,TEXT("卡牌ID必须是文本。"));Ids.Add(FName(*Id)); }
		if (Character==TEXT("Player")) { if (!FGameXXKCardBattleAdapter::SetHeroSelectedCards(Candidate,Ids,&Error)) return Finish(false,Error); }
		else if (FGameXXKCompanionCatalog::FindQuestNpcDefinition(Character))
		{
			if (Candidate.CardRun.bLoadoutLockedForRoute || !FGameXXKCompanionRules::ValidateQuestNpcCardSelection(Character,Ids,&Error)) return Finish(false,Error.IsEmpty()?TEXT("请先返回战前整备。"):Error);
			Candidate.CardRun.PartySelection.QuestNpcCardLoadouts.FindOrAdd(Character).SelectedCardIds=Ids;
			if (Candidate.CardRun.PartySelection.QuestNpc.NpcId==Character) Candidate.CardRun.PartySelection.QuestNpc.SelectedCardIds=Ids;
		}
		else
		{
			auto* C=Candidate.CardRun.CompanionRoster.PermanentCompanions.FindByPredicate([Character](const auto& V){return V.InstanceId==Character;});
			if (!C || Candidate.CardRun.bLoadoutLockedForRoute || !FGameXXKCompanionRules::SetSelectedPersonalCards(*C,Ids,&Error)) return Finish(false,Error.IsEmpty()?TEXT("角色或卡组状态不适用。"):Error);
		}
	}
	else if (Command==TEXT("equipment.create") || Command==TEXT("equipment.loadout"))
	{
		if (Candidate.CardRun.bLoadoutLockedForRoute) return Finish(false,TEXT("请先返回战前整备，再生成配装。"));
		if (!IsCharacter(Candidate,Character)) return Finish(false,TEXT("装备所有者不存在。"));
		int32 Level,Quality,Enhance,Quantity;
		if (!Integer(Args,TEXT("level"),100,1,100,Level,Error) || !Integer(Args,TEXT("quality"),6,1,10,Quality,Error)
			|| !Integer(Args,TEXT("enhance"),0,0,10,Enhance,Error) || !Integer(Args,TEXT("quantity"),1,1,12,Quantity,Error)) return Finish(false,Error);
		if (Args->HasField(TEXT("character_level")))
		{
			int32 CharacterLevel;
			if (!Integer(Args,TEXT("character_level"),1,1,FGameXXKCharacterStatRules::MaxCharacterLevel,CharacterLevel,Error) || !SetLevel(Candidate,Character,CharacterLevel,Error)) return Finish(false,Error);
		}
		const bool bLoadout=Command==TEXT("equipment.loadout"), bEquip=Flag(Args,TEXT("equip"),bLoadout);
		TArray<EGameXXKEquipmentSet> Sets;TArray<EGameXXKEquipmentSlot> Slots;
		if (bLoadout)
		{
			const TArray<TSharedPtr<FJsonValue>>* Values=nullptr;
			if (!Args->TryGetArrayField(TEXT("sets"),Values) || Values->Num()!=6) return Finish(false,TEXT("整套配装需要按六个装备槽提供6个套装名。"));
			for (int32 I=0;I<6;++I) { EGameXXKEquipmentSet Set;if (!Enum((*Values)[I]->AsString(),Set) || Set==EGameXXKEquipmentSet::Legacy) return Finish(false,TEXT("套装名无效。"));Sets.Add(Set);Slots.Add(static_cast<EGameXXKEquipmentSlot>(I+1)); }
		}
		else
		{
			const auto* Def=FGameXXKEquipmentCatalog::FindDefinition(FName(*String(Args,TEXT("id"))));
			if (!Def || Def->Set==EGameXXKEquipmentSet::Legacy) return Finish(false,TEXT("请选择有效的当前装备。"));
			for (int32 I=0;I<Quantity;++I) { Sets.Add(Def->Set);Slots.Add(Def->Slot); }
		}
		const FString AffixMode=String(Args,TEXT("affix"),TEXT("random"));
		if (AffixMode!=TEXT("random") && AffixMode!=TEXT("low") && AffixMode!=TEXT("mid") && AffixMode!=TEXT("high")) return Finish(false,TEXT("词缀档位无效。"));
		const FString Gem=String(Args,TEXT("gem"),TEXT("none"));EGameXXKGemType GemType=EGameXXKGemType::Invalid;
		if (Gem!=TEXT("none") && Gem!=TEXT("balanced") && !Enum(Gem,GemType)) return Finish(false,TEXT("宝石类型无效。"));
		int32 GemIndex=0;
		for (int32 I=0;I<Sets.Num();++I)
		{
			FGameXXKEquipmentCreateRequest Create;Create.Set=Sets[I];Create.Quality=static_cast<EGameXXKEquipmentQuality>(Quality);Create.ItemLevel=Level;Create.bForceSlot=true;Create.ForcedSlot=Slots[I];
			FName Id;if (!FGameXXKEquipmentRules::CreateRolledInstance(Candidate.EquipmentCollection,Create,Id,&Error)) return Finish(false,Error);
			auto* Item=Candidate.EquipmentCollection.EquipmentInstances.FindByPredicate([Id](const auto& E){return E.InstanceId==Id;});
			if (!Item) return Finish(false,TEXT("装备生成后无法找到实例。"));Item->EnhancementLevel=Enhance;
			for (auto& A:Item->RolledAffixes)
				if (AffixMode!=TEXT("random")) { const auto Range=FGameXXKAffixCatalog::GetMagnitudeRange(A.Unit,A.Tier);A.Magnitude=AffixMode==TEXT("low")?Range.Minimum:AffixMode==TEXT("high")?Range.Maximum:Range.Minimum+(Range.Maximum-Range.Minimum)/2; }
			for (auto& Socket:Item->SocketedGems)
				if (Gem!=TEXT("none")) { Socket.Type=Gem==TEXT("balanced")?static_cast<EGameXXKGemType>(1+(GemIndex++%3)):GemType;Socket.Quality=static_cast<EGameXXKGemQuality>(Quality); }
			if (bEquip) { FGameXXKEquipmentTransactionResult Result;if (!FGameXXKEquipmentEconomyRules::Equip(Candidate,Character,Slots[I],Id,Result)) return Finish(false,Result.Message.ToString()); }
			Created.Add(Id);
		}
	}
	else if (Command==TEXT("heal"))
	{
		Candidate.PlayerHP=Candidate.PlayerMaxHP;Candidate.PlayerMP=Candidate.PlayerMaxMP;
		if (Candidate.CardRun.bHasActiveCardBattle)
		{
			for (auto& U:Candidate.CardRun.ActiveBattle.Units) if (U.Side==EGameXXKCardTargetSide::Party) { U.HP=U.MaxHP;U.Mana=U.MaxMana;U.bLiving=true; }
			if (!FGameXXKCardBattleAdapter::SyncCardBattleToLegacyProjection(Candidate,&Error)) return Finish(false,Error);
		}
	}
	else if (Command==TEXT("battle.start"))
	{
		if (Current.CardRun.bHasActiveCardBattle) return Finish(false,TEXT("请先返回战前，再选择新的场次；当前战斗可直接重开。"));
		int32 Encounter,Seed;if (!Integer(Args,TEXT("encounter"),1,1,7,Encounter,Error) || !Integer(Args,TEXT("seed"),20260906,1,MAX_int32,Seed,Error)) return Finish(false,Error);
		if (!UGameXXKMVPSubsystem::BuildDevelopmentTrainingBattle(Candidate,FName(*String(Args,TEXT("stage"),TEXT("Training.Normal.1-1"))),Encounter-1,Seed,Error)) return Finish(false,Error);
	}
	else return Finish(false,TEXT("未知指令，请使用help查看可用功能。"));
	if (!Candidate.CardRun.bHasActiveCardBattle && !FGameXXKEquipmentEconomyRules::SynchronizeRuntimeMirrors(Candidate)) return Finish(false,TEXT("角色属性投影失败。"));
	if (!FGameXXKDesktopInventoryRules::Normalize(Candidate,&Error) || !FGameXXKSaveMigration::ValidateRuntimeState(Candidate,Error)) return Finish(false,Error);
	const FGameXXKRuntimeState Before=Current;const auto BeforeTravel=MVP->GetTrainingTravelRuntimeCopy();
	FGameXXKTrainingTravelRuntime AppliedTravel=BeforeTravel;
	const bool bPreserveTravel=Command==TEXT("item.give") || Command==TEXT("cards.set") || Command==TEXT("heal");
	if (Command==TEXT("heal"))
	{
		for(auto& U:AppliedTravel.PartyUnits) U.HP=U.MaxHP;
		AppliedTravel.PlayerHP=AppliedTravel.PlayerMaxHP;
	}
	BeginSession();if (!MVP->ApplyDevelopmentState(Candidate,Error,bPreserveTravel?&AppliedTravel:nullptr)) return Finish(false,Error);
	if(Command==TEXT("equipment.recommend_all"))
	{
		auto FullTravel=MVP->GetTrainingTravelRuntimeCopy();for(auto& U:FullTravel.PartyUnits)U.HP=U.MaxHP;FullTravel.PlayerHP=FullTravel.PlayerMaxHP;
		if(!MVP->ApplyDevelopmentState(MVP->GetRuntimeState(),Error,&FullTravel))return Finish(false,Error);
	}
	if (Command==TEXT("battle.start")) { Impl->BeforeBattle=Before;Impl->BeforeBattleTravel=BeforeTravel;Impl->BattleStart=Candidate;MVP->SetBattleAutoPlayEnabled(false); }
	Object D=NewObject();TArray<TSharedPtr<FJsonValue>> Ids;for (FName Id:Created) Ids.Add(MakeShared<FJsonValueString>(Id.ToString()));D->SetArrayField(TEXT("created_ids"),Ids);
	Object Log=NewObject();Log->SetStringField(TEXT("command"),Command);Log->SetObjectField(TEXT("args"),Args);Log->SetStringField(TEXT("at"),FDateTime::UtcNow().ToIso8601());Impl->History.Add(Log);if (Impl->History.Num()>200) Impl->History.RemoveAt(0);
	RefreshPresentation(MVP);
	return Finish(true,Command==TEXT("battle.start")?TEXT("测试战斗已就绪，可收起面板开始游玩。"):TEXT("已应用到临时试验；可在记录页保存或恢复。"),D);
#endif
}

bool UGameXXKDevToolsSubsystem::TickDevelopment(float DeltaSeconds)
{
#if !UE_BUILD_SHIPPING
	using namespace DevTools;
	if (!ResolveMVP()) return true;
	if (!Impl->Input && !GIsAutomationTesting && FSlateApplication::IsInitialized() && GetWorld() && GetWorld()->IsGameWorld())
	{
		Impl->Input=MakeShared<FGameXXKDevInput>(this,Impl->Hotkey);FSlateApplication::Get().RegisterInputPreProcessor(Impl->Input,0);
	}
	if (Impl->Batch.IsSet() && !Impl->Batch->bCancelled && Impl->Batch->Done<Impl->Batch->Total)
	{
		auto& B=Impl->Batch.GetValue();FGameXXKRuntimeState State=B.Source;FString Error;
		const int32 Seed=B.FirstSeed+B.Done;
		bool OK=B.bContinue || UGameXXKMVPSubsystem::BuildDevelopmentTrainingBattle(State,B.Stage,B.Encounter,Seed,Error);
		FGameXXKSimulationMetrics Metrics;TArray<FGameXXKSimulationTraceEntry> Trace;
		if (OK)
		{
			FGameXXKSimulationScenario Scenario;Scenario.InitialRuntimeState=State;Scenario.Seed=Seed;Scenario.MaxRounds=B.MaxRounds;Scenario.bResumeActiveBattle=true;Scenario.Terrain=State.CardRun.ActiveBattle.Terrain;
			OK=FGameXXKCombatSimulationRules::RunScenario(Scenario,Metrics,Trace,&Error);
		}
		Object R=NewObject();R->SetNumberField(TEXT("seed"),Seed);R->SetBoolField(TEXT("ok"),OK);
		R->SetStringField(TEXT("outcome"),!OK?TEXT("error"):Metrics.bStalemateResolved?TEXT("stalemate"):Metrics.bVictory?TEXT("victory"):TEXT("defeat"));
		R->SetStringField(TEXT("error"),Error);R->SetObjectField(TEXT("metrics"),Struct(Metrics));
		TArray<TSharedPtr<FJsonValue>> Entries;for (const auto& T:Trace) Entries.Add(MakeShared<FJsonValueObject>(Struct(T)));R->SetArrayField(TEXT("trace"),Entries);
		B.Results.Add(R);++B.Done;
		const FString Dir=GetStorageDirectory()/TEXT("reports")/B.Id;IFileManager::Get().MakeDirectory(*Dir,true);
		FFileHelper::SaveStringToFile(Encode(R),*(Dir/FString::Printf(TEXT("seed-%d.json"),Seed)),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		if (B.Done==1)
			FFileHelper::SaveStringToFile(Encode(Snapshot(B.Source,FGameXXKTrainingTravelRuntime())),*(Dir/TEXT("source.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		if (B.Done==1 || (!Metrics.bVictory && B.Done<=3))
			FFileHelper::SaveStringToFile(Encode(Snapshot(State,FGameXXKTrainingTravelRuntime())),*(Dir/FString::Printf(TEXT("scene-%d.json"),Seed)),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		R->RemoveField(TEXT("trace")); // Trace is on disk; aggregation stays bounded in memory.
		Object Report=NewObject();Report->SetStringField(TEXT("id"),B.Id);Report->SetStringField(TEXT("stage"),B.Stage.ToString());Report->SetNumberField(TEXT("encounter"),B.Encounter+1);Report->SetNumberField(TEXT("completed"),B.Done);Report->SetNumberField(TEXT("total"),B.Total);Report->SetStringField(TEXT("policy"),TEXT("Skilled"));Report->SetStringField(TEXT("build"),TEXT(__DATE__ " " __TIME__));Report->SetStringField(TEXT("directory"),Dir);
		Report->SetStringField(TEXT("binary_md5"),BinaryFingerprint());
		int32 Wins=0,Defeats=0,Stalemates=0,Errors=0;double Rounds=0,Health=0;TArray<TSharedPtr<FJsonValue>> Seeds;
		for (const auto& Result:B.Results)
		{
			const auto Outcome=String(Result,TEXT("outcome"));Wins+=Outcome==TEXT("victory");Defeats+=Outcome==TEXT("defeat");Stalemates+=Outcome==TEXT("stalemate");Errors+=Outcome==TEXT("error");
			if (Outcome!=TEXT("error")) {const Object M=Result->GetObjectField(TEXT("metrics"));double V=0;M->TryGetNumberField(TEXT("rounds"),V);Rounds+=V;V=0;M->TryGetNumberField(TEXT("remainingPartyHealth"),V);Health+=V;}
			Seeds.Add(MakeShared<FJsonValueNumber>(Result->GetNumberField(TEXT("seed"))));
		}
		const int32 Resolved=Wins+Defeats+Stalemates;
		Report->SetNumberField(TEXT("resolved"),Resolved);Report->SetNumberField(TEXT("wins"),Wins);Report->SetNumberField(TEXT("defeats"),Defeats);Report->SetNumberField(TEXT("stalemates"),Stalemates);Report->SetNumberField(TEXT("errors"),Errors);Report->SetNumberField(TEXT("win_rate"),Resolved?static_cast<double>(Wins)/Resolved:0);
		Report->SetNumberField(TEXT("mean_rounds"),Resolved?Rounds/Resolved:0);Report->SetNumberField(TEXT("mean_remaining_health"),Resolved?Health/Resolved:0);Report->SetArrayField(TEXT("seeds"),Seeds);
		Impl->LastReport=Report;FFileHelper::SaveStringToFile(Encode(Report),*(Dir/TEXT("summary.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		Impl->Message=FString::Printf(TEXT("批测 %d/%d · 胜 %d / 负 %d / 僵局 %d / 错误 %d"),B.Done,B.Total,Wins,Defeats,Stalemates,Errors);
		Impl->bLastCommandSucceeded=Errors==0;
	}
	Impl->InboxElapsed+=DeltaSeconds;
	if (!GIsAutomationTesting && Impl->InboxElapsed>=0.5f && GetWorld() && GetWorld()->IsGameWorld())
	{
		Impl->InboxElapsed=0;TArray<FString> Files;IFileManager::Get().FindFiles(Files,*(GetStorageDirectory()/TEXT("inbox/*.json")),true,false);Files.Sort();
		if (!Files.IsEmpty())
		{
			const FString File=Files[0],Path=GetStorageDirectory()/TEXT("inbox")/File,Out=GetStorageDirectory()/TEXT("outbox")/File;
			FString Request;
			if (IFileManager::Get().FileSize(*Path)<=16*1024*1024 && FFileHelper::LoadFileToString(Request,*Path))
			{
				if (IFileManager::Get().FileExists(*Out)) IFileManager::Get().Delete(*Path,false,true);
				else
				{
					if (!Impl->PendingResponses.Contains(File)) Impl->PendingResponses.Add(File,ExecuteJson(Request));
					if (FFileHelper::SaveStringToFile(Impl->PendingResponses[File],*Out,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
					{ Impl->PendingResponses.Remove(File);IFileManager::Get().Delete(*Path,false,true); }
				}
			}
		}
	}
#endif
	return true;
}
