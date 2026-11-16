#include "NSCGameState.h"
#include "NSCPlayerState.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"

ANSCGameState::ANSCGameState()
{
}

void ANSCGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ANSCGameState, Phase);
	DOREPLIFETIME(ANSCGameState, PhaseEndServerTime);
	DOREPLIFETIME(ANSCGameState, BombExplodeServerTime);
	DOREPLIFETIME(ANSCGameState, PlantedSite);
	DOREPLIFETIME(ANSCGameState, DefuseProgress);
	DOREPLIFETIME(ANSCGameState, RoundNumber);
	DOREPLIFETIME(ANSCGameState, Squad0Score);
	DOREPLIFETIME(ANSCGameState, Squad1Score);
	DOREPLIFETIME(ANSCGameState, bSidesSwapped);
	DOREPLIFETIME(ANSCGameState, RoundsToWin);
	DOREPLIFETIME(ANSCGameState, OvertimeIndex);
	DOREPLIFETIME(ANSCGameState, MatchWinnerSquad);
	DOREPLIFETIME(ANSCGameState, ModeName);
	DOREPLIFETIME(ANSCGameState, KillFeed);
	DOREPLIFETIME(ANSCGameState, LastRound);
}

float ANSCGameState::GetPhaseTimeRemaining() const
{
	return FMath::Max(0.f, PhaseEndServerTime - static_cast<float>(GetServerWorldTimeSeconds()));
}

float ANSCGameState::GetBombTimeRemaining() const
{
	if (PlantedSite == NAME_None) { return 0.f; }
	return FMath::Max(0.f, BombExplodeServerTime - static_cast<float>(GetServerWorldTimeSeconds()));
}

ENSCTeam ANSCGameState::GetSideOfSquad(int32 Squad) const
{
	const bool bSquad0Attacks = !bSidesSwapped;
	if (Squad == 0) { return bSquad0Attacks ? ENSCTeam::Attackers : ENSCTeam::Defenders; }
	return bSquad0Attacks ? ENSCTeam::Defenders : ENSCTeam::Attackers;
}

int32 ANSCGameState::GetScoreForTeam(ENSCTeam Team) const
{
	return GetSideOfSquad(0) == Team ? Squad0Score : Squad1Score;
}

TArray<ANSCPlayerState*> ANSCGameState::GetPlayersOnTeam(ENSCTeam Team) const
{
	TArray<ANSCPlayerState*> Out;
	for (APlayerState* P : PlayerArray)
	{
		ANSCPlayerState* PS = Cast<ANSCPlayerState>(P);
		if (PS && PS->GetTeam() == Team) { Out.Add(PS); }
	}
	return Out;
}

void ANSCGameState::SetPhase(ENSCRoundPhase InPhase, float Duration)
{
	Phase = InPhase;
	PhaseEndServerTime = static_cast<float>(GetServerWorldTimeSeconds()) + Duration;
}

void ANSCGameState::SetBombPlanted(FName Site, float Duration)
{
	PlantedSite = Site;
	BombExplodeServerTime = static_cast<float>(GetServerWorldTimeSeconds()) + Duration;
	DefuseProgress = 0.f;
}

void ANSCGameState::ClearBomb()
{
	PlantedSite = NAME_None;
	BombExplodeServerTime = 0.f;
	DefuseProgress = 0.f;
}

void ANSCGameState::AddKillFeed(const FNSCKillFeedEntry& E)
{
	KillFeed.Add(E);
	while (KillFeed.Num() > 6) { KillFeed.RemoveAt(0); }
}
