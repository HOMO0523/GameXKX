#include "UnrealBridgeModule.h"
#include "UnrealBridgeDiscovery.h"
#include "UnrealBridgeServer.h"
#include "Async/Async.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Interfaces/IMainFrameModule.h"
#include "Interfaces/IPluginManager.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "HttpPath.h"
#include "HttpRouteHandle.h"
#include "HttpServerModule.h"
#include "HttpServerRequest.h"
#include "HttpServerResponse.h"
#include "IHttpRouter.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "HAL/PlatformFileManager.h"
#include "Modules/ModuleManager.h"
#include "ShaderCore.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/UnrealType.h"
#include "UnrealBridgeEditorLibrary.h"
#include "IImageWrapperModule.h"

// Forward decls for the debug-hook registration that lives in the blueprint
// library's .cpp (keeps the delegate handle private while still letting the
// module control init / teardown lifetime).
namespace BridgeDebugState
{
	void Register();
	void Unregister();
}

// Always-on perf hook (frame-time histogram + hitch log) — defined in
// UnrealBridgePerfLibrary.cpp, lifetime tied to the module.
namespace BridgePerfFrameHook
{
	void Register();
	void Unregister();
}

// Opt-in periodic perf sampler — defined in UnrealBridgePerfLibrary.cpp.
// No Start at module init (caller-driven) but Shutdown must release the
// FTSTicker handle if a sampling run is still active when the module exits.
namespace BridgePerfSampler
{
	void Shutdown();
}

DEFINE_LOG_CATEGORY_STATIC(LogUnrealBridgeModule, Log, All);

namespace
{
	bool GDesignHttpServerStartedForTdd = false;
	bool GDesignHttpHealthRouteBoundForTdd = false;
	bool GDesignHttpBallSelectRouteBoundForTdd = false;
	bool GDesignHttpBallSelectCommandRouteBoundForTdd = false;
	bool GDesignHttpFlowStateRouteBoundForTdd = false;
	bool GDesignHttpRuntimeBallsRouteBoundForTdd = false;
	bool GDesignHttpSnapshotRouteBoundForTdd = false;
	bool GDesignHttpViewportRouteBoundForTdd = false;
	FTSTicker::FDelegateHandle GDesignHttpTddHandle;
	TSharedPtr<IHttpRouter> GDesignHttpRouter;
	TArray<FHttpRouteHandle> GDesignHttpRouteHandles;
	uint32 GDesignHttpPort = 7878;

	void ResolveStringConfig(const TCHAR* CliKey, const TCHAR* EnvKey, const TCHAR* IniKey, FString& Out);
	void ResolveIntConfig(const TCHAR* CliKey, const TCHAR* EnvKey, const TCHAR* IniKey, int32& Out);

	FString CleanExportedValue(FString Value)
	{
		Value.TrimStartAndEndInline();
		if (Value.StartsWith(TEXT("\"")) && Value.EndsWith(TEXT("\"")) && Value.Len() >= 2)
		{
			Value = Value.Mid(1, Value.Len() - 2);
		}
		FString Left;
		FString Right;
		if (Value.Split(TEXT("::"), &Left, &Right, ESearchCase::CaseSensitive, ESearchDir::FromEnd))
		{
			Value = Right;
		}
		return Value;
	}

	FString ExportFieldValue(const UStruct* Struct, const void* Container, const FName FieldName)
	{
		if (!Struct || !Container)
		{
			return FString();
		}
		const FProperty* Property = Struct->FindPropertyByName(FieldName);
		if (!Property)
		{
			return FString();
		}
		const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Container);
		if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
		{
			const int64 EnumValue = EnumProperty->GetUnderlyingProperty()->GetSignedIntPropertyValue(ValuePtr);
			return EnumProperty->GetEnum() ? EnumProperty->GetEnum()->GetNameStringByValue(EnumValue) : FString::FromInt(static_cast<int32>(EnumValue));
		}
		if (const FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
		{
			if (ByteProperty->Enum)
			{
				return ByteProperty->Enum->GetNameStringByValue(ByteProperty->GetPropertyValue(ValuePtr));
			}
		}
		if (const FNameProperty* NameProperty = CastField<FNameProperty>(Property))
		{
			return NameProperty->GetPropertyValue(ValuePtr).ToString();
		}
		FString Out;
		Property->ExportText_Direct(Out, ValuePtr, nullptr, nullptr, PPF_None);
		return CleanExportedValue(Out);
	}

	int64 ReadIntFieldValue(const UStruct* Struct, const void* Container, const FName FieldName, const int64 DefaultValue = 0)
	{
		if (!Struct || !Container)
		{
			return DefaultValue;
		}
		const FProperty* Property = Struct->FindPropertyByName(FieldName);
		if (!Property)
		{
			return DefaultValue;
		}
		const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Container);
		if (const FNumericProperty* NumericProperty = CastField<FNumericProperty>(Property))
		{
			return NumericProperty->IsInteger()
				? NumericProperty->GetSignedIntPropertyValue(ValuePtr)
				: FMath::RoundToInt64(NumericProperty->GetFloatingPointPropertyValue(ValuePtr));
		}
		return DefaultValue;
	}

	UWorld* ResolveDesignHttpWorld()
	{
		if (GEditor && GEditor->PlayWorld)
		{
			return GEditor->PlayWorld;
		}
		if (GEngine)
		{
			UWorld* EditorWorld = nullptr;
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				UWorld* World = Context.World();
				if (!World)
				{
					continue;
				}
				if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
				{
					return World;
				}
				if (Context.WorldType == EWorldType::Editor)
				{
					EditorWorld = World;
				}
			}
			if (EditorWorld)
			{
				return EditorWorld;
			}
		}
		return GWorld;
	}

	APlayerState* ResolveDesignHttpPlayerState(AGameStateBase* GameState)
	{
		if (!GameState)
		{
			return nullptr;
		}
		APlayerState* Fallback = nullptr;
		for (APlayerState* PlayerState : GameState->PlayerArray)
		{
			if (!PlayerState)
			{
				continue;
			}
			if (!Fallback)
			{
				Fallback = PlayerState;
			}
			if (const FArrayProperty* PoolProperty = FindFProperty<FArrayProperty>(PlayerState->GetClass(), TEXT("RunBallPool")))
			{
				const void* PoolPtr = PoolProperty->ContainerPtrToValuePtr<void>(PlayerState);
				FScriptArrayHelper PoolHelper(PoolProperty, PoolPtr);
				if (PoolHelper.Num() > 0)
				{
					return PlayerState;
				}
			}
		}
		return Fallback;
	}

	TSharedPtr<FJsonObject> BuildRunBallJson(const FStructProperty* RunBallStructProperty, const void* RunBallPtr, const TArray<int32>& CarriedRunBallIds)
	{
		TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
		if (!RunBallStructProperty || !RunBallPtr)
		{
			return Row;
		}
		const UScriptStruct* Struct = RunBallStructProperty->Struct;
		const int32 RunBallId = static_cast<int32>(ReadIntFieldValue(Struct, RunBallPtr, TEXT("RunBallId"), INDEX_NONE));
		Row->SetNumberField(TEXT("runBallId"), RunBallId);
		Row->SetNumberField(TEXT("forgeLevel"), ReadIntFieldValue(Struct, RunBallPtr, TEXT("ForgeLevel"), 0));
		Row->SetStringField(TEXT("rarity"), ExportFieldValue(Struct, RunBallPtr, TEXT("Rarity")));
		Row->SetNumberField(TEXT("baseRadius"), ReadIntFieldValue(Struct, RunBallPtr, TEXT("BaseRadius"), 0));
		Row->SetNumberField(TEXT("currentRadius"), ReadIntFieldValue(Struct, RunBallPtr, TEXT("CurrentRadius"), 0));

		int32 CarrySlotIndex = INDEX_NONE;
		for (int32 Index = 0; Index < CarriedRunBallIds.Num(); ++Index)
		{
			if (CarriedRunBallIds[Index] == RunBallId)
			{
				CarrySlotIndex = Index;
				break;
			}
		}
		Row->SetNumberField(TEXT("carrySlotIndex"), CarrySlotIndex);
		Row->SetBoolField(TEXT("carried"), CarrySlotIndex != INDEX_NONE);

		if (const FStructProperty* SlotProperty = CastField<FStructProperty>(Struct->FindPropertyByName(TEXT("Slot"))))
		{
			const void* SlotPtr = SlotProperty->ContainerPtrToValuePtr<void>(RunBallPtr);
			Row->SetStringField(TEXT("ballId"), ExportFieldValue(SlotProperty->Struct, SlotPtr, TEXT("BallId")));
			Row->SetStringField(TEXT("beadId"), ExportFieldValue(SlotProperty->Struct, SlotPtr, TEXT("BeadId")));
		}
		return Row;
	}

	bool AddControllerRunStateToJson(TSharedPtr<FJsonObject> Root, UWorld* World)
	{
		APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
		UFunction* Function = PlayerController ? PlayerController->FindFunction(TEXT("BuildWebRunStateJson")) : nullptr;
		if (!Root || !PlayerController || !Function)
		{
			return false;
		}

		struct FWebRunStateParams
		{
			FString ReturnValue;
		};

		FWebRunStateParams Params;
		PlayerController->ProcessEvent(Function, &Params);
		if (Params.ReturnValue.IsEmpty())
		{
			return false;
		}

		TSharedPtr<FJsonObject> RunState;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Params.ReturnValue);
		if (!FJsonSerializer::Deserialize(Reader, RunState) || !RunState)
		{
			return false;
		}

		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : RunState->Values)
		{
			Root->SetField(Pair.Key, Pair.Value);
		}
		return true;
	}

	void AddRunStateToJson(TSharedPtr<FJsonObject> Root, UWorld* World, APlayerState* PlayerState)
	{
		if (!Root)
		{
			return;
		}
		if (AddControllerRunStateToJson(Root, World))
		{
			return;
		}
		TArray<int32> CarriedRunBallIds;
		if (PlayerState)
		{
			if (const FArrayProperty* CarryProperty = FindFProperty<FArrayProperty>(PlayerState->GetClass(), TEXT("CarriedRunBallIds")))
			{
				const void* CarryPtr = CarryProperty->ContainerPtrToValuePtr<void>(PlayerState);
				FScriptArrayHelper CarryHelper(CarryProperty, CarryPtr);
				for (int32 Index = 0; Index < CarryHelper.Num(); ++Index)
				{
					int32 Value = INDEX_NONE;
					if (const FNumericProperty* InnerNumeric = CastField<FNumericProperty>(CarryProperty->Inner))
					{
						Value = static_cast<int32>(InnerNumeric->GetSignedIntPropertyValue(CarryHelper.GetRawPtr(Index)));
					}
					CarriedRunBallIds.Add(Value);
				}
			}
		}

		TArray<TSharedPtr<FJsonValue>> CarryJson;
		for (int32 Index = 0; Index < CarriedRunBallIds.Num(); ++Index)
		{
			TSharedPtr<FJsonObject> Slot = MakeShared<FJsonObject>();
			Slot->SetNumberField(TEXT("slotIndex"), Index);
			Slot->SetNumberField(TEXT("runBallId"), CarriedRunBallIds[Index]);
			CarryJson.Add(MakeShared<FJsonValueObject>(Slot));
		}
		Root->SetArrayField(TEXT("loadoutSlots"), CarryJson);

		TArray<TSharedPtr<FJsonValue>> PoolJson;
		if (PlayerState)
		{
			if (const FArrayProperty* PoolProperty = FindFProperty<FArrayProperty>(PlayerState->GetClass(), TEXT("RunBallPool")))
			{
				if (const FStructProperty* RunBallStructProperty = CastField<FStructProperty>(PoolProperty->Inner))
				{
					const void* PoolPtr = PoolProperty->ContainerPtrToValuePtr<void>(PlayerState);
					FScriptArrayHelper PoolHelper(PoolProperty, PoolPtr);
					for (int32 Index = 0; Index < PoolHelper.Num(); ++Index)
					{
						TSharedPtr<FJsonObject> Row = BuildRunBallJson(RunBallStructProperty, PoolHelper.GetRawPtr(Index), CarriedRunBallIds);
						Row->SetNumberField(TEXT("poolIndex"), Index);
						PoolJson.Add(MakeShared<FJsonValueObject>(Row));
					}
				}
			}
		}
		Root->SetArrayField(TEXT("ballPack"), PoolJson);
		Root->SetNumberField(TEXT("ballPackCount"), PoolJson.Num());
		Root->SetNumberField(TEXT("loadoutSlotCount"), CarryJson.Num());
	}

	void AddSynthesisStateToJson(TSharedPtr<FJsonObject> Root, UWorld* World)
	{
		if (!Root)
		{
			return;
		}

		Root->SetBoolField(TEXT("synthesisMode"), false);
		Root->SetArrayField(TEXT("synthesisMaterialRunBallIds"), TArray<TSharedPtr<FJsonValue>>());
		Root->SetNumberField(TEXT("selectedSynthesisSlotIndex"), INDEX_NONE);
		Root->SetNumberField(TEXT("lastSynthesisOutputRunBallId"), INDEX_NONE);
		Root->SetStringField(TEXT("synthesisProbabilityPreview"), TEXT(""));

		APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
		UFunction* Function = PlayerController ? PlayerController->FindFunction(TEXT("BuildWebBallSelectSynthesisSnapshotJson")) : nullptr;
		if (!Function)
		{
			return;
		}

		struct FWebBallSelectSynthesisSnapshotParams
		{
			FString ReturnValue;
		};

		FWebBallSelectSynthesisSnapshotParams Params;
		PlayerController->ProcessEvent(Function, &Params);
		if (Params.ReturnValue.IsEmpty())
		{
			return;
		}

		TSharedPtr<FJsonObject> Snapshot;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Params.ReturnValue);
		if (!FJsonSerializer::Deserialize(Reader, Snapshot) || !Snapshot)
		{
			return;
		}

		bool bSynthesisMode = false;
		if (Snapshot->TryGetBoolField(TEXT("synthesisMode"), bSynthesisMode))
		{
			Root->SetBoolField(TEXT("synthesisMode"), bSynthesisMode);
		}

		const TArray<TSharedPtr<FJsonValue>>* MaterialIds = nullptr;
		if (Snapshot->TryGetArrayField(TEXT("synthesisMaterialRunBallIds"), MaterialIds) && MaterialIds)
		{
			Root->SetArrayField(TEXT("synthesisMaterialRunBallIds"), *MaterialIds);
		}

		double SelectedSynthesisSlotIndex = INDEX_NONE;
		if (Snapshot->TryGetNumberField(TEXT("selectedSynthesisSlotIndex"), SelectedSynthesisSlotIndex))
		{
			Root->SetNumberField(TEXT("selectedSynthesisSlotIndex"), SelectedSynthesisSlotIndex);
		}

		double LastOutputRunBallId = INDEX_NONE;
		if (Snapshot->TryGetNumberField(TEXT("lastSynthesisOutputRunBallId"), LastOutputRunBallId))
		{
			Root->SetNumberField(TEXT("lastSynthesisOutputRunBallId"), LastOutputRunBallId);
		}

		FString ProbabilityPreview;
		if (Snapshot->TryGetStringField(TEXT("synthesisProbabilityPreview"), ProbabilityPreview))
		{
			Root->SetStringField(TEXT("synthesisProbabilityPreview"), ProbabilityPreview);
		}
	}

	void AddFlowStateToJson(TSharedPtr<FJsonObject> Root, UWorld* World)
	{
		if (!Root)
		{
			return;
		}

		Root->SetStringField(TEXT("flowSchema"), TEXT(""));
		Root->SetStringField(TEXT("flowSurface"), TEXT("webui"));
		Root->SetStringField(TEXT("phaseKind"), TEXT(""));
		Root->SetObjectField(TEXT("phasePayload"), MakeShared<FJsonObject>());
		Root->SetArrayField(TEXT("allowedActions"), TArray<TSharedPtr<FJsonValue>>());

		APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
		UFunction* Function = PlayerController ? PlayerController->FindFunction(TEXT("BuildWebFlowStateJson")) : nullptr;
		if (!Function)
		{
			return;
		}

		struct FWebFlowStateParams
		{
			FString ReturnValue;
		};

		FWebFlowStateParams Params;
		PlayerController->ProcessEvent(Function, &Params);
		if (Params.ReturnValue.IsEmpty())
		{
			return;
		}

		TSharedPtr<FJsonObject> FlowState;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Params.ReturnValue);
		if (!FJsonSerializer::Deserialize(Reader, FlowState) || !FlowState)
		{
			return;
		}

		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : FlowState->Values)
		{
			Root->SetField(Pair.Key, Pair.Value);
		}
	}

	FString JsonToString(const TSharedRef<FJsonObject>& Root)
	{
		FString Output;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
		FJsonSerializer::Serialize(Root, Writer);
		return Output;
	}

	FString BuildDesignHttpStateJson(const bool bIncludeBallSelect)
	{
		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		UWorld* World = ResolveDesignHttpWorld();
		AGameStateBase* GameState = World ? World->GetGameState<AGameStateBase>() : nullptr;
		APlayerState* PlayerState = ResolveDesignHttpPlayerState(GameState);
		Root->SetStringField(TEXT("server"), TEXT("UnrealBridgeDesignHttp"));
		Root->SetNumberField(TEXT("port"), static_cast<double>(GDesignHttpPort));
		Root->SetBoolField(TEXT("worldAvailable"), World != nullptr);
		Root->SetBoolField(TEXT("gameStateAvailable"), GameState != nullptr);
		Root->SetStringField(TEXT("worldName"), World ? World->GetName() : TEXT(""));
		Root->SetStringField(TEXT("phase"), GameState ? ExportFieldValue(GameState->GetClass(), GameState, TEXT("CurrentPhase")) : TEXT("NoWorld"));
		Root->SetNumberField(TEXT("wave"), GameState ? ReadIntFieldValue(GameState->GetClass(), GameState, TEXT("CurrentWave"), 0) : 0);
		Root->SetNumberField(TEXT("score"), GameState ? ReadIntFieldValue(GameState->GetClass(), GameState, TEXT("TotalScore"), 0) : 0);
		Root->SetNumberField(TEXT("shopGold"), GameState ? ReadIntFieldValue(GameState->GetClass(), GameState, TEXT("ShopGold"), 0) : 0);
		Root->SetStringField(TEXT("monsterName"), GameState ? ExportFieldValue(GameState->GetClass(), GameState, TEXT("NextMonsterName")) : TEXT(""));
		Root->SetStringField(TEXT("monsterTypeId"), GameState ? ExportFieldValue(GameState->GetClass(), GameState, TEXT("NextMonsterTypeId")) : TEXT(""));
		if (bIncludeBallSelect)
		{
			AddRunStateToJson(Root, World, PlayerState);
			AddSynthesisStateToJson(Root, World);
			AddFlowStateToJson(Root, World);
		}
		return JsonToString(Root);
	}

	FString GetDesignHttpQueryParam(const FHttpServerRequest& Request, const FString& Key, const FString& DefaultValue = FString())
	{
		if (const FString* Value = Request.QueryParams.Find(Key))
		{
			return *Value;
		}
		return DefaultValue;
	}

	int32 GetDesignHttpQueryInt(const FHttpServerRequest& Request, const FString& Key, const int32 DefaultValue = INDEX_NONE)
	{
		const FString Value = GetDesignHttpQueryParam(Request, Key);
		if (Value.IsEmpty())
		{
			return DefaultValue;
		}
		return FCString::Atoi(*Value);
	}

	bool InvokeDesignHttpBallSelectCommand(const FHttpServerRequest& Request, FString& OutMessage)
	{
		UWorld* World = ResolveDesignHttpWorld();
		APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
		if (!PlayerController)
		{
			OutMessage = TEXT("no_player_controller");
			return false;
		}

		UFunction* Function = PlayerController->FindFunction(TEXT("HandleWebBallSelectCommand"));
		if (!Function)
		{
			OutMessage = TEXT("missing_HandleWebBallSelectCommand");
			return false;
		}

		struct FWebBallSelectCommandParams
		{
			FString Action;
			int32 RunBallId = INDEX_NONE;
			int32 CarrySlotIndex = INDEX_NONE;
			int32 VisibleIndex = INDEX_NONE;
			int32 OfferIndex = INDEX_NONE;
			int32 RunBallIndex = INDEX_NONE;
			bool ReturnValue = false;
		};

		FWebBallSelectCommandParams Params;
		Params.Action = GetDesignHttpQueryParam(Request, TEXT("action"));
		Params.RunBallId = GetDesignHttpQueryInt(Request, TEXT("runBallId"), INDEX_NONE);
		Params.CarrySlotIndex = GetDesignHttpQueryInt(Request, TEXT("carrySlotIndex"), GetDesignHttpQueryInt(Request, TEXT("slotIndex"), INDEX_NONE));
		Params.VisibleIndex = GetDesignHttpQueryInt(Request, TEXT("visibleIndex"), INDEX_NONE);
		Params.OfferIndex = GetDesignHttpQueryInt(Request, TEXT("offerIndex"), GetDesignHttpQueryInt(Request, TEXT("choiceIndex"), INDEX_NONE));
		Params.RunBallIndex = GetDesignHttpQueryInt(Request, TEXT("runBallIndex"), INDEX_NONE);

		PlayerController->ProcessEvent(Function, &Params);
		OutMessage = Params.ReturnValue ? TEXT("handled") : TEXT("not_handled");
		return Params.ReturnValue;
	}

	FString BuildDesignHttpCommandResultJson(const FHttpServerRequest& Request)
	{
		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		FString Message;
		const bool bHandled = InvokeDesignHttpBallSelectCommand(Request, Message);
		Root->SetBoolField(TEXT("ok"), bHandled);
		Root->SetStringField(TEXT("action"), GetDesignHttpQueryParam(Request, TEXT("action")));
		Root->SetStringField(TEXT("message"), Message);
		Root->SetStringField(TEXT("state"), BuildDesignHttpStateJson(true));
		return JsonToString(Root);
	}

	FString BuildDesignHttpRuntimeBallsJson()
	{
		UWorld* World = ResolveDesignHttpWorld();
		APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
		if (!PlayerController)
		{
			TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
			Root->SetStringField(TEXT("server"), TEXT("UnrealBridgeDesignHttp"));
			Root->SetStringField(TEXT("surface"), TEXT("runtime-balls"));
			Root->SetBoolField(TEXT("ok"), false);
			Root->SetStringField(TEXT("message"), TEXT("no_player_controller"));
			Root->SetNumberField(TEXT("ballCount"), 0);
			Root->SetArrayField(TEXT("balls"), TArray<TSharedPtr<FJsonValue>>());
			return JsonToString(Root);
		}

		UFunction* Function = PlayerController->FindFunction(TEXT("BuildWebRuntimeBallsSnapshotJson"));
		if (!Function)
		{
			for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
			{
				APlayerController* Candidate = It->Get();
				UFunction* CandidateFunction = Candidate ? Candidate->FindFunction(TEXT("BuildWebRuntimeBallsSnapshotJson")) : nullptr;
				if (CandidateFunction)
				{
					PlayerController = Candidate;
					Function = CandidateFunction;
					break;
				}
			}
		}
		if (!Function)
		{
			TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
			Root->SetStringField(TEXT("server"), TEXT("UnrealBridgeDesignHttp"));
			Root->SetStringField(TEXT("surface"), TEXT("runtime-balls"));
			Root->SetBoolField(TEXT("ok"), false);
			Root->SetStringField(TEXT("message"), TEXT("missing_BuildWebRuntimeBallsSnapshotJson"));
			Root->SetNumberField(TEXT("ballCount"), 0);
			Root->SetArrayField(TEXT("balls"), TArray<TSharedPtr<FJsonValue>>());
			return JsonToString(Root);
		}

		struct FRuntimeBallsSnapshotParams
		{
			FString ReturnValue;
		};

		FRuntimeBallsSnapshotParams Params;
		PlayerController->ProcessEvent(Function, &Params);
		return Params.ReturnValue.IsEmpty() ? TEXT("{\"ok\":false,\"surface\":\"runtime-balls\",\"ballCount\":0,\"balls\":[]}") : Params.ReturnValue;
	}

	TUniquePtr<FHttpServerResponse> MakeDesignHttpJsonResponse(const FString& Body)
	{
		TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(Body, TEXT("application/json; charset=utf-8"));
		Response->Headers.Add(TEXT("Access-Control-Allow-Origin"), TArray<FString>{TEXT("*")});
		Response->Headers.Add(TEXT("Access-Control-Allow-Methods"), TArray<FString>{TEXT("GET, OPTIONS")});
		Response->Headers.Add(TEXT("Access-Control-Allow-Headers"), TArray<FString>{TEXT("Content-Type")});
		Response->Headers.Add(TEXT("Cache-Control"), TArray<FString>{TEXT("no-store")});
		return Response;
	}

	FString BuildDesignHttpSnapshotJson()
	{
		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("server"), TEXT("UnrealBridgeDesignHttp"));
		Root->SetStringField(TEXT("surface"), TEXT("snapshot"));

		const FVector2D ViewportSize = GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport
			? FVector2D(GEngine->GameViewport->Viewport->GetSizeXY())
			: FVector2D::ZeroVector;
		Root->SetNumberField(TEXT("viewportWidth"), ViewportSize.X);
		Root->SetNumberField(TEXT("viewportHeight"), ViewportSize.Y);
		Root->SetNumberField(TEXT("viewportSourceX"), 0.0);
		Root->SetNumberField(TEXT("viewportSourceY"), 0.0);

		const FBridgeScreenshotResult Snapshot = UUnrealBridgeEditorLibrary::CaptureActiveViewport(TEXT(""), true);
		if (!Snapshot.bSuccess)
		{
			Root->SetBoolField(TEXT("ok"), false);
			Root->SetStringField(TEXT("message"), Snapshot.Error.IsEmpty() ? TEXT("snapshot_failed") : Snapshot.Error);
			Root->SetArrayField(TEXT("balls"), TArray<TSharedPtr<FJsonValue>>());
			Root->SetStringField(TEXT("base64"), TEXT(""));
			Root->SetNumberField(TEXT("base64Length"), 0);
			return JsonToString(Root);
		}

		Root->SetBoolField(TEXT("ok"), true);
		Root->SetStringField(TEXT("message"), TEXT("snapshot_ok"));
		Root->SetStringField(TEXT("source"), Snapshot.Source);
		Root->SetStringField(TEXT("base64"), Snapshot.Base64);
		Root->SetNumberField(TEXT("screenshotWidth"), Snapshot.Width);
		Root->SetNumberField(TEXT("screenshotHeight"), Snapshot.Height);
		Root->SetNumberField(TEXT("base64Length"), Snapshot.Base64.Len());
		Root->SetArrayField(TEXT("balls"), TArray<TSharedPtr<FJsonValue>>());
		return JsonToString(Root);
	}

	TUniquePtr<FHttpServerResponse> BuildDesignHttpSnapshotPngResponse()
	{
		const FString TempDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealBridge"), TEXT("WebUI"));
		if (!FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*TempDir))
		{
			return MakeDesignHttpJsonResponse(TEXT("{\"ok\":false,\"message\":\"snapshot_dir_failed\"}"));
		}

		const FString TempFilename = FPaths::Combine(TempDir, FString::Printf(TEXT("snapshot_%s.png"), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
		const FBridgeScreenshotResult Snapshot = UUnrealBridgeEditorLibrary::CaptureActiveViewport(TempFilename, false);
		if (!Snapshot.bSuccess)
		{
			TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
			Root->SetStringField(TEXT("server"), TEXT("UnrealBridgeDesignHttp"));
			Root->SetStringField(TEXT("surface"), TEXT("snapshot"));
			Root->SetBoolField(TEXT("ok"), false);
			Root->SetStringField(TEXT("message"), Snapshot.Error.IsEmpty() ? TEXT("snapshot_failed") : Snapshot.Error);
			return MakeDesignHttpJsonResponse(JsonToString(Root));
		}

		TArray<uint8> Bytes;
		if (!FFileHelper::LoadFileToArray(Bytes, *TempFilename))
		{
			return MakeDesignHttpJsonResponse(TEXT("{\"ok\":false,\"message\":\"snapshot_file_not_found\"}"));
		}

		IFileManager& FileManager = IFileManager::Get();
		FileManager.Delete(*TempFilename);

		TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(MoveTemp(Bytes), TEXT("image/png"));
		Response->Headers.Add(TEXT("Access-Control-Allow-Origin"), TArray<FString>{TEXT("*")});
		Response->Headers.Add(TEXT("Cache-Control"), TArray<FString>{TEXT("no-store")});
		return Response;
	}


	FString BuildDesignHttpViewportHtml()
	{
		FString Html;
		Html += TEXT("<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n");
		Html += TEXT("<meta charset=\"utf-8\">\n");
		Html += TEXT("<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n");
		Html += TEXT("<title>UE Viewport — BallGameChaos</title>\n");
		Html += TEXT("<style>\n");
		Html += TEXT("*{margin:0;padding:0;box-sizing:border-box}\n");
		Html += TEXT("body{background:#1a1a1a;color:#f5f5f5;font-family:'Segoe UI',Arial,sans-serif;display:flex;flex-direction:column;height:100vh;overflow:hidden}\n");
		Html += TEXT(".bar{background:#222;padding:8px 16px;display:flex;align-items:center;gap:12px;border-bottom:2px solid #e67e22;flex-shrink:0}\n");
		Html += TEXT(".bar .title{font-weight:700;font-size:14px;color:#e67e22}\n");
		Html += TEXT(".bar .status{font-size:12px;color:#999;margin-left:auto}\n");
		Html += TEXT(".bar .fps{font-size:12px;color:#666;font-family:monospace}\n");
		Html += TEXT(".view{flex:1;display:flex;align-items:center;justify-content:center;background:#0d0d0d;overflow:hidden;position:relative}\n");
		Html += TEXT(".view img{max-width:100%;max-height:100%;object-fit:contain;image-rendering:auto}\n");
		Html += TEXT(".view .placeholder{color:#444;font-size:18px}\n");
		Html += TEXT("</style>\n</head>\n<body>\n");
		Html += TEXT("<div class=\"bar\">\n");
		Html += TEXT("<span class=\"title\">BallGameChaos — Live Viewport</span>\n");
		Html += TEXT("<span class=\"fps\" id=\"fps\">— fps</span>\n");
		Html += TEXT("<span class=\"status\" id=\"status\">Connecting...</span>\n");
		Html += TEXT("</div>\n");
		Html += TEXT("<div class=\"view\">\n");
		Html += TEXT("<span class=\"placeholder\" id=\"placeholder\">Waiting for viewport...</span>\n");
		Html += TEXT("<img id=\"snap\" alt=\"Viewport\" style=\"display:none\">\n");
		Html += TEXT("</div>\n");
		Html += TEXT("<script>\n");
		Html += TEXT("const img=document.getElementById('snap');\n");
		Html += TEXT("const status=document.getElementById('status');\n");
		Html += TEXT("const fps=document.getElementById('fps');\n");
		Html += TEXT("const placeholder=document.getElementById('placeholder');\n");
		Html += TEXT("let frameCount=0,fpsTimer=0,loading=false,lastFrame=0;\n");
		Html += TEXT("const TARGET_MS=33,MIN_GAP=16;\n");
		Html += TEXT("function refresh(){\n");
		Html += TEXT("  if(loading)return;\n");
		Html += TEXT("  const now=performance.now();\n");
		Html += TEXT("  if(now-lastFrame<TARGET_MS){requestAnimationFrame(refresh);return;}\n");
		Html += TEXT("  if(!fpsTimer)fpsTimer=now;\n");
		Html += TEXT("  loading=true;\n");
		Html += TEXT("  lastFrame=now;\n");
		Html += TEXT("  const ts=Date.now();\n");
		Html += TEXT("  const snap=new Image();\n");
		Html += TEXT("  snap.onload=function(){\n");
		Html += TEXT("    img.src=snap.src;img.style.display='';placeholder.style.display='none';\n");
		Html += TEXT("    status.textContent='Live';status.style.color='#4caf50';\n");
		Html += TEXT("    frameCount++;\n");
		Html += TEXT("    if(now-fpsTimer>=1000){fps.textContent=(frameCount*1000/(now-fpsTimer)).toFixed(0)+' fps';fpsTimer=now;frameCount=0;}\n");
		Html += TEXT("    loading=false;\n");
		Html += TEXT("    requestAnimationFrame(refresh);\n");
		Html += TEXT("  };\n");
		Html += TEXT("  snap.onerror=function(){status.textContent='Snapshot failed';status.style.color='#e74c3c';loading=false;setTimeout(refresh,500);};\n");
		Html += TEXT("  snap.src='/webui/snapshot/jpg?q=60&t='+ts;\n");
		Html += TEXT("}\n");
		Html += TEXT("refresh();\n");
		Html += TEXT("</script>\n</body>\n</html>");
		return Html;
	}


	TUniquePtr<FHttpServerResponse> BuildDesignHttpSnapshotJpgResponse(int32 Quality)
	{
		Quality = FMath::Clamp(Quality, 10, 100);
		const FString TempDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealBridge"), TEXT("WebUI"));
		if (!FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*TempDir))
		{
			return MakeDesignHttpJsonResponse(TEXT("{\"ok\":false,\"message\":\"snapshot_dir_failed\"}"));
		}

		const FString TempFilename = FPaths::Combine(TempDir, FString::Printf(TEXT("snapshot_%s.png"), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
		const FBridgeScreenshotResult Snapshot = UUnrealBridgeEditorLibrary::CaptureActiveViewport(TempFilename, false);
		if (!Snapshot.bSuccess)
		{
			TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
			Root->SetStringField(TEXT("server"), TEXT("UnrealBridgeDesignHttp"));
			Root->SetStringField(TEXT("surface"), TEXT("snapshot"));
			Root->SetBoolField(TEXT("ok"), false);
			Root->SetStringField(TEXT("message"), Snapshot.Error.IsEmpty() ? TEXT("snapshot_failed") : Snapshot.Error);
			return MakeDesignHttpJsonResponse(JsonToString(Root));
		}

		TArray<uint8> PngBytes;
		if (!FFileHelper::LoadFileToArray(PngBytes, *TempFilename))
		{
			IFileManager::Get().Delete(*TempFilename);
			return MakeDesignHttpJsonResponse(TEXT("{\"ok\":false,\"message\":\"snapshot_file_not_found\"}"));
		}
		IFileManager::Get().Delete(*TempFilename);

		IImageWrapperModule& IWM = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
		FImage RawImage;
		if (!IWM.DecompressImage(PngBytes.GetData(), PngBytes.Num(), RawImage))
		{
			return MakeDesignHttpJsonResponse(TEXT("{\"ok\":false,\"message\":\"png_decompress_failed\"}"));
		}

		TArray64<uint8> JpgBytes;
		if (!IWM.CompressImage(JpgBytes, EImageFormat::JPEG, RawImage, Quality))
		{
			return MakeDesignHttpJsonResponse(TEXT("{\"ok\":false,\"message\":\"jpg_compress_failed\"}"));
		}

		TArray<uint8> OutBytes;
		OutBytes.Append(JpgBytes.GetData(), static_cast<int32>(JpgBytes.Num()));
		TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(MoveTemp(OutBytes), TEXT("image/jpeg"));
		Response->Headers.Add(TEXT("Access-Control-Allow-Origin"), TArray<FString>{TEXT("*")});
		Response->Headers.Add(TEXT("Cache-Control"), TArray<FString>{TEXT("no-store")});
		return Response;
	}
	bool HandleDesignHttpJson(const FString& Body, const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
	{
		if (Request.Verb == EHttpServerRequestVerbs::VERB_OPTIONS)
		{
			OnComplete(MakeDesignHttpJsonResponse(TEXT("{}")));
			return true;
		}
		OnComplete(MakeDesignHttpJsonResponse(Body));
		return true;
	}

	void StopDesignHttpServer()
	{
		if (GDesignHttpRouter.IsValid())
		{
			for (const FHttpRouteHandle& Handle : GDesignHttpRouteHandles)
			{
				if (Handle.IsValid())
				{
					GDesignHttpRouter->UnbindRoute(Handle);
				}
			}
		}
		GDesignHttpRouteHandles.Reset();
		GDesignHttpRouter.Reset();
		GDesignHttpServerStartedForTdd = false;
		GDesignHttpHealthRouteBoundForTdd = false;
		GDesignHttpBallSelectRouteBoundForTdd = false;
		GDesignHttpBallSelectCommandRouteBoundForTdd = false;
		GDesignHttpFlowStateRouteBoundForTdd = false;
		GDesignHttpRuntimeBallsRouteBoundForTdd = false;
		GDesignHttpSnapshotRouteBoundForTdd = false;
		GDesignHttpViewportRouteBoundForTdd = false;
	}

	void StartDesignHttpServer()
	{
		int32 Enabled = 1;
		int32 Port = 7878;
		ResolveIntConfig(TEXT("UnrealBridgeDesignHttp="), TEXT("UNREAL_BRIDGE_DESIGN_HTTP"), TEXT("DesignHttp"), Enabled);
		ResolveIntConfig(TEXT("UnrealBridgeDesignHttpPort="), TEXT("UNREAL_BRIDGE_DESIGN_HTTP_PORT"), TEXT("DesignHttpPort"), Port);
		if (Enabled == 0)
		{
			UE_LOG(LogUnrealBridgeModule, Log, TEXT("Design HTTP server disabled."));
			return;
		}
		Port = FMath::Clamp(Port, 1024, 65535);
		GDesignHttpPort = static_cast<uint32>(Port);

		FHttpServerModule& HttpServerModule = FHttpServerModule::Get();
		GDesignHttpRouter = HttpServerModule.GetHttpRouter(GDesignHttpPort, true);
		if (!GDesignHttpRouter.IsValid())
		{
			UE_LOG(LogUnrealBridgeModule, Warning, TEXT("Design HTTP server could not create router on localhost:%u"), GDesignHttpPort);
			return;
		}

		const EHttpServerRequestVerbs ReadVerbs = EHttpServerRequestVerbs::VERB_GET | EHttpServerRequestVerbs::VERB_OPTIONS;
		FHttpRouteHandle HealthHandle = GDesignHttpRouter->BindRoute(FHttpPath(TEXT("/health")), ReadVerbs,
			FHttpRequestHandler::CreateLambda([](const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
			{
				return HandleDesignHttpJson(TEXT("{\"ok\":true,\"server\":\"UnrealBridgeDesignHttp\"}"), Request, OnComplete);
			}));
		FHttpRouteHandle StateHandle = GDesignHttpRouter->BindRoute(FHttpPath(TEXT("/state")), ReadVerbs,
			FHttpRequestHandler::CreateLambda([](const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
			{
				return HandleDesignHttpJson(BuildDesignHttpStateJson(false), Request, OnComplete);
			}));
		FHttpRouteHandle BallSelectHandle = GDesignHttpRouter->BindRoute(FHttpPath(TEXT("/ui/ball-select")), ReadVerbs,
			FHttpRequestHandler::CreateLambda([](const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
			{
				return HandleDesignHttpJson(BuildDesignHttpStateJson(true), Request, OnComplete);
			}));
		FHttpRouteHandle BallSelectCommandHandle = GDesignHttpRouter->BindRoute(FHttpPath(TEXT("/ui/ball-select/command")), ReadVerbs,
			FHttpRequestHandler::CreateLambda([](const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
			{
				if (Request.Verb == EHttpServerRequestVerbs::VERB_OPTIONS)
				{
					OnComplete(MakeDesignHttpJsonResponse(TEXT("{}")));
					return true;
				}
				const FHttpServerRequest RequestCopy = Request;
				const FHttpResultCallback Complete = OnComplete;
				AsyncTask(ENamedThreads::GameThread, [RequestCopy, Complete]()
				{
					Complete(MakeDesignHttpJsonResponse(BuildDesignHttpCommandResultJson(RequestCopy)));
				});
				return true;
			}));
		FHttpRouteHandle WebHealthHandle = GDesignHttpRouter->BindRoute(FHttpPath(TEXT("/webui/health")), ReadVerbs,
			FHttpRequestHandler::CreateLambda([](const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
			{
				return HandleDesignHttpJson(TEXT("{\"ok\":true,\"server\":\"UnrealBridgeDesignHttp\",\"surface\":\"webui\"}"), Request, OnComplete);
			}));
		FHttpRouteHandle WebStateHandle = GDesignHttpRouter->BindRoute(FHttpPath(TEXT("/webui/state")), ReadVerbs,
			FHttpRequestHandler::CreateLambda([](const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
			{
				return HandleDesignHttpJson(BuildDesignHttpStateJson(true), Request, OnComplete);
			}));
		FHttpRouteHandle FlowStateHandle = GDesignHttpRouter->BindRoute(FHttpPath(TEXT("/webui/flow-state")), ReadVerbs,
			FHttpRequestHandler::CreateLambda([](const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
			{
				return HandleDesignHttpJson(BuildDesignHttpStateJson(true), Request, OnComplete);
			}));
		FHttpRouteHandle FlowStateSlashHandle = GDesignHttpRouter->BindRoute(FHttpPath(TEXT("/webui/flow-state/")), ReadVerbs,
			FHttpRequestHandler::CreateLambda([](const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
			{
				return HandleDesignHttpJson(BuildDesignHttpStateJson(true), Request, OnComplete);
			}));
		FHttpRouteHandle RuntimeBallsHandle = GDesignHttpRouter->BindRoute(FHttpPath(TEXT("/webui/runtime-balls")), ReadVerbs,
			FHttpRequestHandler::CreateLambda([](const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
			{
				if (Request.Verb == EHttpServerRequestVerbs::VERB_OPTIONS)
				{
					OnComplete(MakeDesignHttpJsonResponse(TEXT("{}")));
					return true;
				}
				const FHttpResultCallback Complete = OnComplete;
				AsyncTask(ENamedThreads::GameThread, [Complete]()
				{
					Complete(MakeDesignHttpJsonResponse(BuildDesignHttpRuntimeBallsJson()));
				});
				return true;
			}));

		if (HealthHandle.IsValid())
		{
			GDesignHttpRouteHandles.Add(HealthHandle);
		}
		if (StateHandle.IsValid())
		{
			GDesignHttpRouteHandles.Add(StateHandle);
		}
		if (BallSelectHandle.IsValid())
		{
			GDesignHttpRouteHandles.Add(BallSelectHandle);
		}
		if (BallSelectCommandHandle.IsValid())
		{
			GDesignHttpRouteHandles.Add(BallSelectCommandHandle);
		}
		if (WebHealthHandle.IsValid())
		{
			GDesignHttpRouteHandles.Add(WebHealthHandle);
		}
		if (WebStateHandle.IsValid())
		{
			GDesignHttpRouteHandles.Add(WebStateHandle);
		}
		if (FlowStateHandle.IsValid())
		{
			GDesignHttpRouteHandles.Add(FlowStateHandle);
		}
		if (FlowStateSlashHandle.IsValid())
		{
			GDesignHttpRouteHandles.Add(FlowStateSlashHandle);
		}
		if (RuntimeBallsHandle.IsValid())
		{
			GDesignHttpRouteHandles.Add(RuntimeBallsHandle);
		}
		const FHttpRequestHandler SnapshotHandler = FHttpRequestHandler::CreateLambda([](const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
		{
			if (Request.Verb == EHttpServerRequestVerbs::VERB_OPTIONS)
			{
				OnComplete(MakeDesignHttpJsonResponse(TEXT("{}")));
				return true;
			}
			const FHttpResultCallback Complete = OnComplete;
			AsyncTask(ENamedThreads::GameThread, [Complete]()
			{
				Complete(MakeDesignHttpJsonResponse(BuildDesignHttpSnapshotJson()));
			});
			return true;
		});
		const FHttpRequestHandler SnapshotPngHandler = FHttpRequestHandler::CreateLambda([](const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
		{
			if (Request.Verb == EHttpServerRequestVerbs::VERB_OPTIONS)
			{
				OnComplete(MakeDesignHttpJsonResponse(TEXT("{}")));
				return true;
			}
			const FHttpResultCallback Complete = OnComplete;
			AsyncTask(ENamedThreads::GameThread, [Complete]()
			{
				Complete(BuildDesignHttpSnapshotPngResponse());
			});
			return true;
		});
		FHttpRouteHandle SnapshotHandle = GDesignHttpRouter->BindRoute(FHttpPath(TEXT("/webui/snapshot")), ReadVerbs, SnapshotHandler);
		FHttpRouteHandle SnapshotSlashHandle = GDesignHttpRouter->BindRoute(FHttpPath(TEXT("/webui/snapshot/")), ReadVerbs, SnapshotHandler);
		FHttpRouteHandle SnapshotShortHandle = GDesignHttpRouter->BindRoute(FHttpPath(TEXT("/snapshot")), ReadVerbs, SnapshotHandler);
		FHttpRouteHandle SnapshotShortSlashHandle = GDesignHttpRouter->BindRoute(FHttpPath(TEXT("/snapshot/")), ReadVerbs, SnapshotHandler);
		FHttpRouteHandle SnapshotPngHandle = GDesignHttpRouter->BindRoute(FHttpPath(TEXT("/webui/snapshot/png")), ReadVerbs, SnapshotPngHandler);
		FHttpRouteHandle SnapshotPngHandleAlt = GDesignHttpRouter->BindRoute(FHttpPath(TEXT("/webui/snapshot.png")), ReadVerbs, SnapshotPngHandler);
		if (SnapshotHandle.IsValid())
		{
			GDesignHttpRouteHandles.Add(SnapshotHandle);
		}
		if (SnapshotSlashHandle.IsValid())
		{
			GDesignHttpRouteHandles.Add(SnapshotSlashHandle);
		}
		if (SnapshotShortHandle.IsValid())
		{
			GDesignHttpRouteHandles.Add(SnapshotShortHandle);
		}
		if (SnapshotShortSlashHandle.IsValid())
		{
			GDesignHttpRouteHandles.Add(SnapshotShortSlashHandle);
		}
		if (SnapshotPngHandle.IsValid())
		{
			GDesignHttpRouteHandles.Add(SnapshotPngHandle);
		}
		if (SnapshotPngHandleAlt.IsValid())
		{
			GDesignHttpRouteHandles.Add(SnapshotPngHandleAlt);
		}
		GDesignHttpHealthRouteBoundForTdd = HealthHandle.IsValid();
		GDesignHttpBallSelectRouteBoundForTdd = BallSelectHandle.IsValid();
		GDesignHttpBallSelectCommandRouteBoundForTdd = BallSelectCommandHandle.IsValid();
		GDesignHttpFlowStateRouteBoundForTdd = FlowStateHandle.IsValid() || FlowStateSlashHandle.IsValid();
		GDesignHttpRuntimeBallsRouteBoundForTdd = RuntimeBallsHandle.IsValid();

		const FHttpRequestHandler ViewportHandler = FHttpRequestHandler::CreateLambda([](const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
		{
			if (Request.Verb == EHttpServerRequestVerbs::VERB_OPTIONS)
			{
				OnComplete(MakeDesignHttpJsonResponse(TEXT("{}")));
				return true;
			}
			const FString Html = BuildDesignHttpViewportHtml();
			TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(Html, TEXT("text/html; charset=utf-8"));
			Response->Headers.Add(TEXT("Access-Control-Allow-Origin"), TArray<FString>{TEXT("*")});
			Response->Headers.Add(TEXT("Cache-Control"), TArray<FString>{TEXT("no-store")});
			OnComplete(MoveTemp(Response));
			return true;
		});
		FHttpRouteHandle ViewportHandle = GDesignHttpRouter->BindRoute(FHttpPath(TEXT("/webui/viewport")), ReadVerbs, ViewportHandler);
		FHttpRouteHandle ViewportSlashHandle = GDesignHttpRouter->BindRoute(FHttpPath(TEXT("/webui/viewport/")), ReadVerbs, ViewportHandler);
		if (ViewportHandle.IsValid())
		{
			GDesignHttpRouteHandles.Add(ViewportHandle);
		}
		if (ViewportSlashHandle.IsValid())
		{
			GDesignHttpRouteHandles.Add(ViewportSlashHandle);
		}

		const FHttpRequestHandler SnapshotJpgHandler = FHttpRequestHandler::CreateLambda([](const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
		{
			if (Request.Verb == EHttpServerRequestVerbs::VERB_OPTIONS)
			{
				OnComplete(MakeDesignHttpJsonResponse(TEXT("{}")));
				return true;
			}
			const FHttpResultCallback Complete = OnComplete;
			const int32 Quality = GetDesignHttpQueryInt(Request, TEXT("q"), 60);
			AsyncTask(ENamedThreads::GameThread, [Complete, Quality]()
			{
				Complete(BuildDesignHttpSnapshotJpgResponse(Quality));
			});
			return true;
		});
		FHttpRouteHandle SnapshotJpgHandle = GDesignHttpRouter->BindRoute(FHttpPath(TEXT("/webui/snapshot/jpg")), ReadVerbs, SnapshotJpgHandler);
		if (SnapshotJpgHandle.IsValid())
		{
			GDesignHttpRouteHandles.Add(SnapshotJpgHandle);
		}
		GDesignHttpViewportRouteBoundForTdd = ViewportHandle.IsValid();

		GDesignHttpSnapshotRouteBoundForTdd = SnapshotHandle.IsValid() || SnapshotSlashHandle.IsValid() || SnapshotShortHandle.IsValid() || SnapshotShortSlashHandle.IsValid();
		GDesignHttpServerStartedForTdd =
			GDesignHttpRouter.IsValid() &&
			GDesignHttpHealthRouteBoundForTdd &&
			StateHandle.IsValid() &&
			GDesignHttpBallSelectRouteBoundForTdd &&
			GDesignHttpBallSelectCommandRouteBoundForTdd &&
			GDesignHttpFlowStateRouteBoundForTdd &&
			GDesignHttpRuntimeBallsRouteBoundForTdd &&
			GDesignHttpSnapshotRouteBoundForTdd &&
			GDesignHttpViewportRouteBoundForTdd;

		HttpServerModule.StartAllListeners();
		UE_LOG(LogUnrealBridgeModule, Log, TEXT("Design HTTP server listening on http://127.0.0.1:%u"), GDesignHttpPort);
	}

	void EmitDesignHttpServerTdd()
	{
		UE_LOG(LogUnrealBridgeModule, Log, TEXT("[TDD] DesignHttp_LocalServerStarted: actual=%d expected=1 result=%s"),
			GDesignHttpServerStartedForTdd ? 1 : 0,
			GDesignHttpServerStartedForTdd ? TEXT("PASS") : TEXT("FAIL"));
		UE_LOG(LogUnrealBridgeModule, Log, TEXT("[TDD] DesignHttp_HealthEndpointBound: actual=%d expected=1 result=%s"),
			GDesignHttpHealthRouteBoundForTdd ? 1 : 0,
			GDesignHttpHealthRouteBoundForTdd ? TEXT("PASS") : TEXT("FAIL"));
		UE_LOG(LogUnrealBridgeModule, Log, TEXT("[TDD] DesignHttp_BallSelectEndpointBound: actual=%d expected=1 result=%s"),
			GDesignHttpBallSelectRouteBoundForTdd ? 1 : 0,
			GDesignHttpBallSelectRouteBoundForTdd ? TEXT("PASS") : TEXT("FAIL"));
		UE_LOG(LogUnrealBridgeModule, Log, TEXT("[TDD] DesignHttp_BallSelectCommandEndpointBound: actual=%d expected=1 result=%s"),
			GDesignHttpBallSelectCommandRouteBoundForTdd ? 1 : 0,
			GDesignHttpBallSelectCommandRouteBoundForTdd ? TEXT("PASS") : TEXT("FAIL"));
		UE_LOG(LogUnrealBridgeModule, Log, TEXT("[TDD] DesignHttp_FlowStateEndpointBound: actual=%d expected=1 result=%s"),
			GDesignHttpFlowStateRouteBoundForTdd ? 1 : 0,
			GDesignHttpFlowStateRouteBoundForTdd ? TEXT("PASS") : TEXT("FAIL"));
		UE_LOG(LogUnrealBridgeModule, Log, TEXT("[TDD] DesignHttp_RuntimeBallsEndpointBound: actual=%d expected=1 result=%s"),
			GDesignHttpRuntimeBallsRouteBoundForTdd ? 1 : 0,
			GDesignHttpRuntimeBallsRouteBoundForTdd ? TEXT("PASS") : TEXT("FAIL"));
		UE_LOG(LogUnrealBridgeModule, Log, TEXT("[TDD] DesignHttp_SnapshotEndpointBound: actual=%d expected=1 result=%s"),
			GDesignHttpSnapshotRouteBoundForTdd ? 1 : 0,
			GDesignHttpSnapshotRouteBoundForTdd ? TEXT("PASS") : TEXT("FAIL"));
		UE_LOG(LogUnrealBridgeModule, Log, TEXT("[TDD] DesignHttp_ViewportEndpointBound: actual=%d expected=1 result=%s"),
			GDesignHttpViewportRouteBoundForTdd ? 1 : 0,
			GDesignHttpViewportRouteBoundForTdd ? TEXT("PASS") : TEXT("FAIL"));
	}

	void RegisterDesignHttpServerTdd()
	{
		if (GDesignHttpTddHandle.IsValid())
		{
			return;
		}
		GDesignHttpTddHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateLambda([](float)
			{
				EmitDesignHttpServerTdd();
				return true;
			}),
			1.0f);
	}

	void UnregisterDesignHttpServerTdd()
	{
		if (GDesignHttpTddHandle.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(GDesignHttpTddHandle);
			GDesignHttpTddHandle.Reset();
		}
	}

	/**
	 * Layered config resolution: CLI > env > EditorPerProjectUserSettings.ini > hardcoded default.
	 * Returns `Out` on success, leaves it untouched on miss.
	 */
	void ResolveStringConfig(const TCHAR* CliKey, const TCHAR* EnvKey, const TCHAR* IniKey, FString& Out)
	{
		FString Value;
		if (FParse::Value(FCommandLine::Get(), CliKey, Value) && !Value.IsEmpty())
		{
			Out = Value;
			return;
		}
		Value = FPlatformMisc::GetEnvironmentVariable(EnvKey);
		if (!Value.IsEmpty())
		{
			Out = Value;
			return;
		}
		if (GConfig)
		{
			FString Tmp;
			if (GConfig->GetString(TEXT("UnrealBridge"), IniKey, Tmp, GEditorPerProjectIni) && !Tmp.IsEmpty())
			{
				Out = Tmp;
				return;
			}
		}
	}

	void ResolveIntConfig(const TCHAR* CliKey, const TCHAR* EnvKey, const TCHAR* IniKey, int32& Out)
	{
		int32 Value = 0;
		if (FParse::Value(FCommandLine::Get(), CliKey, Value))
		{
			Out = Value;
			return;
		}
		const FString EnvStr = FPlatformMisc::GetEnvironmentVariable(EnvKey);
		if (!EnvStr.IsEmpty() && EnvStr.IsNumeric())
		{
			Out = FCString::Atoi(*EnvStr);
			return;
		}
		if (GConfig)
		{
			int32 Tmp = 0;
			if (GConfig->GetInt(TEXT("UnrealBridge"), IniKey, Tmp, GEditorPerProjectIni))
			{
				Out = Tmp;
				return;
			}
		}
	}

	/** sha1(token) first 8 bytes, lowercase hex. Empty input → empty output.
	 *  Not cryptographic — only used so clients can verify "I'm talking to the
	 *  editor that holds this token" without leaking the token itself. */
	FString TokenFingerprint(const FString& Token)
	{
		if (Token.IsEmpty())
		{
			return FString();
		}
		const FTCHARToUTF8 Utf8(*Token);
		FSHA1 Hasher;
		Hasher.Update(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
		Hasher.Final();
		uint8 Digest[20];
		Hasher.GetHash(Digest);
		return BytesToHex(Digest, 8).ToLower();
	}

	/**
	 * Find the editor's .uproject path for the running session. Used by the
	 * discovery responder so clients can match by path as well as by name.
	 */
	FString ResolveProjectPath()
	{
		FString Path = FPaths::ConvertRelativePathToFull(
			FPaths::Combine(FPaths::ProjectDir(), FString::Printf(TEXT("%s.uproject"), FApp::GetProjectName())));
		Path.ReplaceInline(TEXT("\\"), TEXT("/"));
		return Path;
	}
}

void FUnrealBridgeModule::StartupModule()
{
	BridgeDebugState::Register();
	BridgePerfFrameHook::Register();
	StartDesignHttpServer();
	RegisterDesignHttpServerTdd();

	// Map /Plugin/UnrealBridge/ -> this plugin's Shaders/ dir so UMaterialExpressionCustom
	// nodes can #include "/Plugin/UnrealBridge/BridgeSnippets.ush" and friends.
	{
		TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("UnrealBridge"));
		if (Plugin.IsValid())
		{
			const FString ShaderDir = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Shaders"), TEXT("Private"));
			if (FPaths::DirectoryExists(ShaderDir))
			{
				AddShaderSourceDirectoryMapping(TEXT("/Plugin/UnrealBridge"), ShaderDir);
				UE_LOG(LogUnrealBridgeModule, Log,
					TEXT("registered shader dir '%s' under /Plugin/UnrealBridge"), *ShaderDir);
			}
		}
	}

	// ---- resolve config (CLI > env > ini > default) ---------------------
	FString BindStr = TEXT("127.0.0.1");
	int32 Port = 0;                   // 0 = OS-assigned ephemeral (discovery tells clients)
	FString Token;
	FString DiscoveryGroup = TEXT("239.255.42.99:9876");
	int32 DiscoveryEnabled = 1;

	ResolveStringConfig(TEXT("UnrealBridgeBind="), TEXT("UNREAL_BRIDGE_BIND"), TEXT("Bind"), BindStr);
	ResolveIntConfig(TEXT("UnrealBridgePort="), TEXT("UNREAL_BRIDGE_PORT"), TEXT("Port"), Port);
	ResolveStringConfig(TEXT("UnrealBridgeToken="), TEXT("UNREAL_BRIDGE_TOKEN"), TEXT("Token"), Token);
	ResolveStringConfig(TEXT("UnrealBridgeDiscoveryGroup="), TEXT("UNREAL_BRIDGE_DISCOVERY_GROUP"),
		TEXT("DiscoveryGroup"), DiscoveryGroup);
	ResolveIntConfig(TEXT("UnrealBridgeDiscoveryEnabled="), TEXT("UNREAL_BRIDGE_DISCOVERY"),
		TEXT("DiscoveryEnabled"), DiscoveryEnabled);

	// Accept `-UnrealBridgeNoDiscovery` as a convenient shorthand toggle.
	if (FParse::Param(FCommandLine::Get(), TEXT("UnrealBridgeNoDiscovery")))
	{
		DiscoveryEnabled = 0;
	}

	// ---- parse bind + discovery group ---------------------------------
	FIPv4Address BindAddress = FIPv4Address(127, 0, 0, 1);
	if (!FIPv4Address::Parse(BindStr, BindAddress))
	{
		UE_LOG(LogUnrealBridgeModule, Warning,
			TEXT("invalid -UnrealBridgeBind='%s' — falling back to 127.0.0.1"), *BindStr);
		BindAddress = FIPv4Address(127, 0, 0, 1);
		BindStr = TEXT("127.0.0.1");
	}

	FIPv4Endpoint DiscoveryEndpoint(FIPv4Address(239, 255, 42, 99), 9876);
	if (!FIPv4Endpoint::Parse(DiscoveryGroup, DiscoveryEndpoint))
	{
		UE_LOG(LogUnrealBridgeModule, Warning,
			TEXT("invalid -UnrealBridgeDiscoveryGroup='%s' — using 239.255.42.99:9876"), *DiscoveryGroup);
		DiscoveryEndpoint = FIPv4Endpoint(FIPv4Address(239, 255, 42, 99), 9876);
	}

	// ---- start the TCP server -----------------------------------------
	Server = MakeShared<FUnrealBridgeServer>();
	FUnrealBridgeServer::FStartConfig StartCfg;
	StartCfg.BindAddress = BindAddress;
	StartCfg.Port = Port;
	StartCfg.Token = Token;

	if (!Server->Start(StartCfg))
	{
		UE_LOG(LogUnrealBridgeModule, Error,
			TEXT("server failed to start (bind=%s port=%d) — bridge disabled this session"),
			*BindStr, Port);
		Server.Reset();
		return;
	}

	UE_LOG(LogUnrealBridgeModule, Log, TEXT("Server up on %s:%d%s"),
		*Server->GetBoundAddress(), Server->GetBoundPort(),
		Server->HasToken() ? TEXT(" (token enforced)") : TEXT(""));

	if (Server->HasToken())
	{
		// Write the token to a predictable place so the client can consume it
		// without copy-pasting from the log. 0 on the mode because UE's
		// FFileHelper doesn't expose ACL control — the Saved/ folder is
		// already user-scoped.
		const FString TokenPath = FPaths::Combine(FPaths::ProjectSavedDir(),
			TEXT("UnrealBridge"), TEXT("token.txt"));
		FFileHelper::SaveStringToFile(Token, *TokenPath,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		UE_LOG(LogUnrealBridgeModule, Log, TEXT("token written to %s"), *TokenPath);
	}

	// ---- start the discovery responder --------------------------------
	if (DiscoveryEnabled != 0)
	{
		FBridgeDiscoveryService::FConfig DiscCfg;
		DiscCfg.GroupAddress = DiscoveryEndpoint.Address;
		DiscCfg.GroupPort = DiscoveryEndpoint.Port;
		DiscCfg.TcpBindAddress = Server->GetBoundAddress();
		DiscCfg.TcpPort = Server->GetBoundPort();
		DiscCfg.ProjectName = FApp::GetProjectName();
		DiscCfg.ProjectPath = ResolveProjectPath();
		DiscCfg.EngineVersion = FEngineVersion::Current().ToString();
		DiscCfg.TokenFingerprint = TokenFingerprint(Token);

		Discovery = MakeUnique<FBridgeDiscoveryService>(DiscCfg);
		if (!Discovery->StartService())
		{
			UE_LOG(LogUnrealBridgeModule, Warning,
				TEXT("discovery failed to start — clients will need an explicit --endpoint=%s:%d"),
				*Server->GetBoundAddress(), Server->GetBoundPort());
			Discovery.Reset();
		}
	}
	else
	{
		UE_LOG(LogUnrealBridgeModule, Log, TEXT("discovery disabled (opt-out) — clients need --endpoint="));
	}

	// ---- editor-ready gate --------------------------------------------
	TWeakPtr<FUnrealBridgeServer> WeakServer = Server;
	auto OnMainFrameReady = [WeakServer](TSharedPtr<SWindow>, bool)
	{
		if (TSharedPtr<FUnrealBridgeServer> Pinned = WeakServer.Pin())
		{
			Pinned->SetEditorReady(true);
		}
	};

	const bool bForceReady = FParse::Param(FCommandLine::Get(), TEXT("UnrealBridgeForceReady"));
	if (bForceReady)
	{
		UE_LOG(LogUnrealBridgeModule, Warning, TEXT("UnrealBridgeForceReady enabled; bridge exec is available before MainFrame readiness."));
		Server->SetEditorReady(true);
	}
	else
	{
		IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
		if (MainFrame.IsWindowInitialized())
		{
			Server->SetEditorReady(true);
		}
		else
		{
			MainFrame.OnMainFrameCreationFinished().AddLambda(OnMainFrameReady);
			FTSTicker::GetCoreTicker().AddTicker(
				FTickerDelegate::CreateLambda([WeakServer](float)
				{
					if (TSharedPtr<FUnrealBridgeServer> Pinned = WeakServer.Pin())
					{
						if (!Pinned->IsEditorReady())
						{
							UE_LOG(LogUnrealBridgeModule, Warning, TEXT("MainFrame ready delegate did not fire; enabling bridge exec after startup fallback."));
							Pinned->SetEditorReady(true);
						}
					}
					return false;
				}),
				10.0f);
		}
	}
}

void FUnrealBridgeModule::ShutdownModule()
{
	UnregisterDesignHttpServerTdd();
	StopDesignHttpServer();
	BridgePerfSampler::Shutdown();
	BridgePerfFrameHook::Unregister();
	BridgeDebugState::Unregister();

	if (Discovery.IsValid())
	{
		Discovery->StopService();
		Discovery.Reset();
	}

	if (Server.IsValid())
	{
		Server->Stop();
		Server.Reset();
		UE_LOG(LogUnrealBridgeModule, Log, TEXT("Server stopped."));
	}
}

IMPLEMENT_MODULE(FUnrealBridgeModule, UnrealBridge)
