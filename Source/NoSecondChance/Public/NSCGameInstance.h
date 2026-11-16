// Game instance: persistent settings (USaveGame) and LAN sessions through OnlineSubsystemNull.
#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "GameFramework/SaveGame.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "NSCGameInstance.generated.h"

class FOnlineSessionSearch;

UCLASS()
class NOSECONDCHANCE_API UNSCSettingsSave : public USaveGame
{
	GENERATED_BODY()
public:
	UPROPERTY() float MouseSensitivity = 1.f;
	UPROPERTY() float FieldOfView = 90.f;
	UPROPERTY() int32 BotSkill = 60;
	UPROPERTY() float MasterVolume = 1.f;
	UPROPERTY() FString PlayerName;
};

USTRUCT()
struct FNSCLanGame
{
	GENERATED_BODY()
	UPROPERTY() FString HostName;
	UPROPERTY() int32 Players = 0;
	UPROPERTY() int32 MaxPlayers = 10;
	UPROPERTY() int32 PingMs = 0;
	int32 ResultIndex = -1;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FNSCOnLanSearchDone, const TArray<FNSCLanGame>& /*Games*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FNSCOnStatus, const FString& /*Message*/);

UCLASS()
class NOSECONDCHANCE_API UNSCGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;
	virtual void Shutdown() override;

	// settings
	float GetMouseSensitivity() const { return Settings ? Settings->MouseSensitivity : 1.f; }
	float GetFieldOfView() const { return Settings ? Settings->FieldOfView : 90.f; }
	int32 GetBotSkill() const { return Settings ? Settings->BotSkill : 60; }
	float GetMasterVolume() const { return Settings ? Settings->MasterVolume : 1.f; }
	FString GetPlayerName() const;
	void SetMouseSensitivity(float V);
	void SetFieldOfView(float V);
	void SetBotSkill(int32 V);
	void SetMasterVolume(float V);
	void SaveSettings();

	// play
	void PlayOffline(bool bCasual);
	void HostLan(bool bFillBots, bool bCasual);
	void FindLanGames();
	void JoinLanGame(int32 ResultIndex);
	void JoinByAddress(const FString& Address);
	void ReturnToMenu(const FString& Reason);
	FString ConsumeMenuMessage() { FString M = MenuMessage; MenuMessage.Reset(); return M; }

	FNSCOnLanSearchDone OnLanSearchDone;
	FNSCOnStatus OnStatus;

	static const TCHAR* WarehouseMap;
	static const TCHAR* MenuMap;

protected:
	IOnlineSessionPtr GetSessions() const;
	FString BuildOptions(bool bFillBots, bool bCasual) const;
	void OnCreateSessionComplete(FName SessionName, bool bOk);
	void OnFindSessionsComplete(bool bOk);
	void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void OnDestroySessionComplete(FName SessionName, bool bOk);
	void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& Error);
	void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& Error);

	UPROPERTY() TObjectPtr<UNSCSettingsSave> Settings;
	TSharedPtr<FOnlineSessionSearch> Search;
	FString PendingHostOptions;
	bool bPendingHostAfterDestroy = false;
	FString MenuMessage;
	FDelegateHandle CreateHandle, FindHandle, JoinHandle, DestroyHandle;
};
