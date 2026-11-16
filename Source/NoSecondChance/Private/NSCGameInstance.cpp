#include "NSCGameInstance.h"
#include "NoSecondChance.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "OnlineSessionSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"

const TCHAR* UNSCGameInstance::WarehouseMap = TEXT("/Game/NSC/Maps/L_Warehouse");
const TCHAR* UNSCGameInstance::MenuMap = TEXT("/Game/NSC/Maps/L_MainMenu");
static const TCHAR* SettingsSlot = TEXT("NSCSettings");
static const FName NSC_HOST_KEY(TEXT("NSC_HOST"));

void UNSCGameInstance::Init()
{
	Super::Init();
	Settings = Cast<UNSCSettingsSave>(UGameplayStatics::LoadGameFromSlot(SettingsSlot, 0));
	if (!Settings) { Settings = Cast<UNSCSettingsSave>(UGameplayStatics::CreateSaveGameObject(UNSCSettingsSave::StaticClass())); }
	if (GEngine)
	{
		GEngine->OnNetworkFailure().AddUObject(this, &UNSCGameInstance::HandleNetworkFailure);
		GEngine->OnTravelFailure().AddUObject(this, &UNSCGameInstance::HandleTravelFailure);
	}
}

void UNSCGameInstance::Shutdown()
{
	if (GEngine)
	{
		GEngine->OnNetworkFailure().RemoveAll(this);
		GEngine->OnTravelFailure().RemoveAll(this);
	}
	Super::Shutdown();
}

FString UNSCGameInstance::GetPlayerName() const
{
	if (Settings && !Settings->PlayerName.IsEmpty()) { return Settings->PlayerName; }
	return FPlatformProcess::UserName();
}

void UNSCGameInstance::SetMouseSensitivity(float V) { if (Settings) { Settings->MouseSensitivity = FMath::Clamp(V, 0.1f, 5.f); } }
void UNSCGameInstance::SetFieldOfView(float V) { if (Settings) { Settings->FieldOfView = FMath::Clamp(V, 70.f, 110.f); } }
void UNSCGameInstance::SetBotSkill(int32 V) { if (Settings) { Settings->BotSkill = FMath::Clamp(V, 10, 100); } }
void UNSCGameInstance::SetMasterVolume(float V) { if (Settings) { Settings->MasterVolume = FMath::Clamp(V, 0.f, 1.f); } }

void UNSCGameInstance::SaveSettings()
{
	if (Settings) { UGameplayStatics::SaveGameToSlot(Settings, SettingsSlot, 0); }
}

FString UNSCGameInstance::BuildOptions(bool bFillBots, bool bCasual) const
{
	return FString::Printf(TEXT("Bots=%d?BotSkill=%d%s"), bFillBots ? 1 : 0, GetBotSkill(), bCasual ? TEXT("?Mode=Casual") : TEXT(""));
}

void UNSCGameInstance::PlayOffline(bool bCasual)
{
	UGameplayStatics::OpenLevel(this, FName(WarehouseMap), true, BuildOptions(true, bCasual));
}

IOnlineSessionPtr UNSCGameInstance::GetSessions() const
{
	return Online::GetSessionInterface(GetWorld());
}

void UNSCGameInstance::HostLan(bool bFillBots, bool bCasual)
{
	PendingHostOptions = TEXT("listen?") + BuildOptions(bFillBots, bCasual);
	IOnlineSessionPtr Sessions = GetSessions();
	if (!Sessions.IsValid())
	{
		// No online subsystem: still host a plain listen server (join by IP works).
		UGameplayStatics::OpenLevel(this, FName(WarehouseMap), true, PendingHostOptions);
		return;
	}
	if (Sessions->GetNamedSession(NAME_GameSession))
	{
		bPendingHostAfterDestroy = true;
		DestroyHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(FOnDestroySessionCompleteDelegate::CreateUObject(this, &UNSCGameInstance::OnDestroySessionComplete));
		Sessions->DestroySession(NAME_GameSession);
		return;
	}
	FOnlineSessionSettings S;
	S.bIsLANMatch = true;
	S.NumPublicConnections = 10;
	S.bShouldAdvertise = true;
	S.bAllowJoinInProgress = true;
	S.bUsesPresence = false;
	S.bAllowJoinViaPresence = false;
	S.Set(NSC_HOST_KEY, GetPlayerName(), EOnlineDataAdvertisementType::ViaOnlineService);
	CreateHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(FOnCreateSessionCompleteDelegate::CreateUObject(this, &UNSCGameInstance::OnCreateSessionComplete));
	OnStatus.Broadcast(TEXT("Creating LAN game..."));
	if (!Sessions->CreateSession(0, NAME_GameSession, S))
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
		UGameplayStatics::OpenLevel(this, FName(WarehouseMap), true, PendingHostOptions);
	}
}

void UNSCGameInstance::OnDestroySessionComplete(FName SessionName, bool bOk)
{
	if (IOnlineSessionPtr Sessions = GetSessions()) { Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle); }
	if (bPendingHostAfterDestroy)
	{
		bPendingHostAfterDestroy = false;
		const bool bCasual = PendingHostOptions.Contains(TEXT("Mode=Casual"));
		const bool bBots = PendingHostOptions.Contains(TEXT("Bots=1"));
		HostLan(bBots, bCasual);
	}
}

void UNSCGameInstance::OnCreateSessionComplete(FName SessionName, bool bOk)
{
	if (IOnlineSessionPtr Sessions = GetSessions()) { Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle); }
	UE_LOG(LogNSC, Log, TEXT("CreateSession %s: %d"), *SessionName.ToString(), bOk);
	// Even if advertising failed, start the listen server so join-by-IP still works.
	UGameplayStatics::OpenLevel(this, FName(WarehouseMap), true, PendingHostOptions);
}

void UNSCGameInstance::FindLanGames()
{
	IOnlineSessionPtr Sessions = GetSessions();
	if (!Sessions.IsValid()) { OnLanSearchDone.Broadcast(TArray<FNSCLanGame>()); return; }
	Search = MakeShared<FOnlineSessionSearch>();
	Search->bIsLanQuery = true;
	Search->MaxSearchResults = 50;
	Search->PingBucketSize = 50;
	FindHandle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(FOnFindSessionsCompleteDelegate::CreateUObject(this, &UNSCGameInstance::OnFindSessionsComplete));
	OnStatus.Broadcast(TEXT("Searching the local network..."));
	if (!Sessions->FindSessions(0, Search.ToSharedRef()))
	{
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
		OnLanSearchDone.Broadcast(TArray<FNSCLanGame>());
	}
}

void UNSCGameInstance::OnFindSessionsComplete(bool bOk)
{
	if (IOnlineSessionPtr Sessions = GetSessions()) { Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle); }
	TArray<FNSCLanGame> Games;
	if (Search.IsValid())
	{
		for (int32 i = 0; i < Search->SearchResults.Num(); ++i)
		{
			const FOnlineSessionSearchResult& R = Search->SearchResults[i];
			FNSCLanGame G;
			G.ResultIndex = i;
			R.Session.SessionSettings.Get(NSC_HOST_KEY, G.HostName);
			if (G.HostName.IsEmpty()) { G.HostName = R.Session.OwningUserName; }
			G.MaxPlayers = R.Session.SessionSettings.NumPublicConnections;
			G.Players = G.MaxPlayers - R.Session.NumOpenPublicConnections;
			G.PingMs = R.PingInMs;
			Games.Add(G);
		}
	}
	OnLanSearchDone.Broadcast(Games);
}

void UNSCGameInstance::JoinLanGame(int32 ResultIndex)
{
	IOnlineSessionPtr Sessions = GetSessions();
	if (!Sessions.IsValid() || !Search.IsValid() || !Search->SearchResults.IsValidIndex(ResultIndex)) { return; }
	if (Sessions->GetNamedSession(NAME_GameSession)) { Sessions->DestroySession(NAME_GameSession); }
	JoinHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(FOnJoinSessionCompleteDelegate::CreateUObject(this, &UNSCGameInstance::OnJoinSessionComplete));
	OnStatus.Broadcast(TEXT("Joining..."));
	Sessions->JoinSession(0, NAME_GameSession, Search->SearchResults[ResultIndex]);
}

void UNSCGameInstance::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	IOnlineSessionPtr Sessions = GetSessions();
	if (!Sessions.IsValid()) { return; }
	Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
	FString Url;
	if (Result != EOnJoinSessionCompleteResult::Success || !Sessions->GetResolvedConnectString(SessionName, Url))
	{
		OnStatus.Broadcast(TEXT("Could not join that game."));
		return;
	}
	JoinByAddress(Url);
}

void UNSCGameInstance::JoinByAddress(const FString& Address)
{
	FString Addr = Address.TrimStartAndEnd();
	if (Addr.IsEmpty()) { return; }
	if (APlayerController* PC = GetFirstLocalPlayerController())
	{
		OnStatus.Broadcast(FString::Printf(TEXT("Connecting to %s..."), *Addr));
		PC->ClientTravel(Addr, TRAVEL_Absolute);
	}
}

void UNSCGameInstance::ReturnToMenu(const FString& Reason)
{
	MenuMessage = Reason;
	if (IOnlineSessionPtr Sessions = GetSessions())
	{
		if (Sessions->GetNamedSession(NAME_GameSession)) { Sessions->DestroySession(NAME_GameSession); }
	}
	UGameplayStatics::OpenLevel(this, FName(MenuMap));
}

void UNSCGameInstance::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& Error)
{
	UE_LOG(LogNSC, Warning, TEXT("Network failure: %s"), *Error);
	MenuMessage = FString::Printf(TEXT("Disconnected: %s"), *Error);
}

void UNSCGameInstance::HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& Error)
{
	UE_LOG(LogNSC, Warning, TEXT("Travel failure: %s"), *Error);
	MenuMessage = FString::Printf(TEXT("Could not connect: %s"), *Error);
}
