// Operations mode: 5v5 attack/defend with a demolition charge. Buy 20 s, preparation 15 s, live 120 s,
// charge 40 s. First to 13 (24 regulation rounds, halftime after 12, MR6 overtime with standard money).
// Bots fill empty slots so the game is playable offline; humans can host a listen server for LAN play.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "NSCTypes.h"
#include "NSCWeaponData.h"
#include "NSCGameMode.generated.h"

class ANSCCharacter;
class ANSCPlayerState;
class ANSCGameState;
class ANSCBomb;
class ANSCAIController;
struct FNSCHitInfo;

UCLASS()
class NOSECONDCHANCE_API ANSCGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ANSCGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void StartPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual bool PlayerCanRestart_Implementation(APlayerController* Player) override { return false; }

	// ------------------------------------------------------------ notifications from gameplay
	void NotifyDamage(ANSCCharacter* Victim, const FNSCHitInfo& Hit, float HealthDamage);
	void NotifyDowned(ANSCCharacter* Victim, const FNSCHitInfo& Hit);
	void NotifyKilled(ANSCCharacter* Victim, const FNSCHitInfo& Hit);
	void NotifyRevived(ANSCCharacter* Victim, AController* Rescuer);
	void OnBombPlanted(ANSCCharacter* Planter, FName Site);
	void OnBombDefused(ANSCCharacter* Defuser);
	/** Friendly fire is off in Operations (your own frag still hurts you). */
	bool ShouldTakeDamage(const ANSCCharacter* Victim, const AController* Instigator) const;

	// ------------------------------------------------------------ economy
	bool TryBuyWeapon(AController* Buyer, FName WeaponId);
	bool TryBuyEquipment(AController* Buyer, ENSCEquipment Item);
	bool IsBuyAllowed() const;

	/** Attack plan the attacking bots follow this round ("A" or "B"). */
	FName GetAttackSite() const { return AttackSite; }
	ANSCBomb* GetBomb() const { return Bomb; }
	TArray<FTransform> GetSpawnTransforms(ENSCTeam Team) const;
	ANSCGameState* GetNSCGameState() const;
	float GetBotSkill() const { return BotSkill; }

protected:
	void BeginMatch();
	void StartRound();
	void BeginPreparation();
	void BeginLive();
	void EndRound(ENSCTeam Winner, ENSCRoundEndReason Reason);
	void AfterRoundEnd();
	void BeginHalfTime();
	void EndMatch(int32 WinnerSquad);
	void CheckWinConditions();
	void ApplyRoundEconomy(ENSCTeam Winner, ENSCRoundEndReason Reason);
	void AssignSides();
	void FillBots();
	void SpawnBot(int32 Squad);
	void BotsBuy();
	void EnsureMapActors();
	void SnapshotSurvivorLoadouts();
	int32 CountStanding(ENSCTeam Team) const;
	int32 CountPlayersInSquad(int32 Squad) const;
	bool IsHalfTimeAfterRound(int32 Round) const;
	bool IsOvertimeStartRound(int32 Round) const;
	int32 WinningSquadIfMatchOver() const;
	void SetAllFrozen(bool bAttackers, bool bDefenders);
	ENSCTeam TeamOf(const AController* C) const;
	static FVector WarehouseToWorld(float BrowserX, float BrowserZ, float Height = 100.f);

	UPROPERTY() TObjectPtr<ANSCBomb> Bomb;

	int32 Squad0Score = 0;
	int32 Squad1Score = 0;
	int32 RoundNumber = 0;
	int32 LossStreak[2] = { 0, 0 };
	int32 RoundsToWin = NSCRules::RoundsToWin;
	int32 RegulationRounds = NSCRules::RegulationRounds;
	bool bAllowOvertime = true;
	bool bCasual = false;
	bool bFillWithBots = true;
	bool bMatchStarted = false;
	bool bPlantedThisRound = false;
	int32 BotCounter = 0;
	int32 RoundRescues = 0;
	float RoundStartTime = 0.f;
	float BotSkill = 0.6f;
	float NextCheck = 0.f;
	FName AttackSite = TEXT("A");
	ENSCRoundEndReason PendingReason = ENSCRoundEndReason::None;
	TWeakObjectPtr<ANSCPlayerState> PlanterPS;
	TMap<TWeakObjectPtr<ANSCCharacter>, TWeakObjectPtr<AController>> LastDraggerOf;
};
