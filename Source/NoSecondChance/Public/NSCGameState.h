// Replicated match state: phase + timers, squad scores, sides, bomb timer, kill feed, round summary.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "NSCTypes.h"
#include "NSCGameState.generated.h"

class ANSCPlayerState;

UCLASS()
class NOSECONDCHANCE_API ANSCGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ANSCGameState();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "NSC") ENSCRoundPhase GetPhase() const { return Phase; }
	UFUNCTION(BlueprintPure, Category = "NSC") float GetPhaseTimeRemaining() const;
	UFUNCTION(BlueprintPure, Category = "NSC") float GetBombTimeRemaining() const;
	UFUNCTION(BlueprintPure, Category = "NSC") int32 GetRoundNumber() const { return RoundNumber; }
	UFUNCTION(BlueprintPure, Category = "NSC") int32 GetScoreForTeam(ENSCTeam Team) const;
	UFUNCTION(BlueprintPure, Category = "NSC") int32 GetSquadScore(int32 Squad) const { return Squad == 0 ? Squad0Score : Squad1Score; }
	UFUNCTION(BlueprintPure, Category = "NSC") ENSCTeam GetSideOfSquad(int32 Squad) const;
	UFUNCTION(BlueprintPure, Category = "NSC") float GetDefuseProgress() const { return DefuseProgress; }
	UFUNCTION(BlueprintPure, Category = "NSC") FName GetPlantedSite() const { return PlantedSite; }
	UFUNCTION(BlueprintPure, Category = "NSC") int32 GetRoundsToWin() const { return RoundsToWin; }
	UFUNCTION(BlueprintPure, Category = "NSC") bool IsOvertime() const { return OvertimeIndex > 0; }
	const TArray<FNSCKillFeedEntry>& GetKillFeed() const { return KillFeed; }
	const FNSCRoundSummary& GetLastRound() const { return LastRound; }
	int32 GetMatchWinnerSquad() const { return MatchWinnerSquad; }
	FString GetModeName() const { return ModeName; }
	TArray<ANSCPlayerState*> GetPlayersOnTeam(ENSCTeam Team) const;

	// server
	void SetPhase(ENSCRoundPhase InPhase, float Duration);
	void SetRoundNumber(int32 N) { RoundNumber = N; }
	void SetScores(int32 S0, int32 S1) { Squad0Score = S0; Squad1Score = S1; }
	void SetSidesSwapped(bool b) { bSidesSwapped = b; }
	bool AreSidesSwapped() const { return bSidesSwapped; }
	void SetBombPlanted(FName Site, float Duration);
	void ClearBomb();
	void SetDefuseProgress(float P) { DefuseProgress = P; }
	void AddKillFeed(const FNSCKillFeedEntry& E);
	void SetLastRound(const FNSCRoundSummary& S) { LastRound = S; }
	void SetMatchWinner(int32 Squad) { MatchWinnerSquad = Squad; }
	void SetRoundsToWin(int32 N) { RoundsToWin = N; }
	void SetOvertimeIndex(int32 N) { OvertimeIndex = N; }
	void SetModeName(const FString& N) { ModeName = N; }

protected:
	UPROPERTY(Replicated) ENSCRoundPhase Phase = ENSCRoundPhase::WaitingForPlayers;
	UPROPERTY(Replicated) float PhaseEndServerTime = 0.f;
	UPROPERTY(Replicated) float BombExplodeServerTime = 0.f;
	UPROPERTY(Replicated) FName PlantedSite = NAME_None;
	UPROPERTY(Replicated) float DefuseProgress = 0.f;
	UPROPERTY(Replicated) int32 RoundNumber = 0;
	UPROPERTY(Replicated) int32 Squad0Score = 0;
	UPROPERTY(Replicated) int32 Squad1Score = 0;
	UPROPERTY(Replicated) bool bSidesSwapped = false;
	UPROPERTY(Replicated) int32 RoundsToWin = NSCRules::RoundsToWin;
	UPROPERTY(Replicated) int32 OvertimeIndex = 0;
	UPROPERTY(Replicated) int32 MatchWinnerSquad = -1;
	UPROPERTY(Replicated) FString ModeName = TEXT("Operations");
	UPROPERTY(Replicated) TArray<FNSCKillFeedEntry> KillFeed;
	UPROPERTY(Replicated) FNSCRoundSummary LastRound;
};
