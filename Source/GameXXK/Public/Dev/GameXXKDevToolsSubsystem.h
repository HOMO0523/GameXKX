#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameXXKDevToolsSubsystem.generated.h"

class UGameXXKMVPSubsystem;
struct FGameXXKDevToolsImpl;

/** One runtime command surface shared by the ink workbench, MCP and local JSON jobs. */
UCLASS()
class GAMEXXK_API UGameXXKDevToolsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	UGameXXKDevToolsSubsystem();
	virtual ~UGameXXKDevToolsSubsystem() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category="GameXXK|Development", meta=(DevelopmentOnly))
	FString ExecuteJson(const FString& RequestJson);
	UFUNCTION(BlueprintCallable, Category="GameXXK|Development", meta=(DevelopmentOnly))
	void TogglePanel();
	UFUNCTION(BlueprintPure, Category="GameXXK|Development", meta=(DevelopmentOnly))
	bool IsPanelOpen() const;
	UFUNCTION(BlueprintPure, Category="GameXXK|Development", meta=(DevelopmentOnly))
	bool IsSessionActive() const;

	void ClosePanel();
	UGameXXKMVPSubsystem* ResolveMVP() const;
	FString GetStatusText() const;
	FString GetLastMessage() const;
	bool WasLastCommandSuccessful() const;
	FString GetStorageDirectory() const;
	bool TickDevelopment(float DeltaSeconds);
#if WITH_DEV_AUTOMATION_TESTS
	void SetMVPForTest(UGameXXKMVPSubsystem* InMVP) { MVPOverride = InMVP; }
#endif
private:
	UPROPERTY(Transient)
	TObjectPtr<UGameXXKMVPSubsystem> MVPOverride;
	TSharedPtr<FGameXXKDevToolsImpl> Impl;
};
