// Per-player match data: squad/side, money, stats (K/D/A, ADR, rescues, revives) and the
// tactical performance score.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "NSCTypes.h"
#include "NSCWeaponComponent.h"
#include "NSCPlayerState.generated.h"

UCLASS()
class NOSECONDCHANCE_API ANSCPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	ANSCPlayerState();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void CopyProperties(APlayerState* PlayerState) override;

	UFUNCTION(BlueprintPure, Category = "NSC") ENSCTeam GetTeam() const { return Team; }
	UFUNCTION(BlueprintPure, Category = "NSC") int32 GetSquad() const { return Squad; }
	UFUNCTION(BlueprintPure, Category = "NSC") int32 GetMoney() const { return Money; }
	UFUNCTION(BlueprintPure, Category = "NSC") float GetADR() const { return RoundsPlayed > 0 ? DamageDealt / RoundsPlayed : 0.f; }
	UFUNCTION(BlueprintPure, Category = "NSC") float GetTPS() const;
	UFUNCTION(BlueprintPure, Category = "NSC") ENSCInjuryState GetLifeState() const { return LifeState; }

	// server
	void SetSquad(int32 InSquad) { Squad = InSquad; }
	void SetTeam(ENSCTeam InTeam) { Team = InTeam; }
	void SetMoney(int32 InMoney) { Money = FMath::Clamp(InMoney, 0, NSCRules::MaxMoney); }
	void AddMoney(int32 Delta) { SetMoney(Money + Delta); }
	bool SpendMoney(int32 Amount) { if (Money < Amount) { return false; } Money -= Amount; return true; }
	void SetLifeState(ENSCInjuryState S) { LifeState = S; }
	void AddTacticalPoints(float P) { TacticalPoints += P; }
	void ResetStats();

	UPROPERTY(Replicated, BlueprintReadOnly) int32 Kills = 0;
	UPROPERTY(Replicated, BlueprintReadOnly) int32 Deaths = 0;
	UPROPERTY(Replicated, BlueprintReadOnly) int32 Assists = 0;
	UPROPERTY(Replicated, BlueprintReadOnly) int32 Downs = 0;
	UPROPERTY(Replicated, BlueprintReadOnly) float DamageDealt = 0.f;
	UPROPERTY(Replicated, BlueprintReadOnly) int32 Revives = 0;   // teammates you revived with a medkit
	UPROPERTY(Replicated, BlueprintReadOnly) int32 Rescues = 0;   // teammates you dragged out who were then revived
	UPROPERTY(Replicated, BlueprintReadOnly) int32 TimesRevived = 0;
	UPROPERTY(Replicated, BlueprintReadOnly) int32 Plants = 0;
	UPROPERTY(Replicated, BlueprintReadOnly) int32 Defuses = 0;
	UPROPERTY(Replicated, BlueprintReadOnly) int32 MVPs = 0;
	UPROPERTY(Replicated, BlueprintReadOnly) int32 RoundsPlayed = 0;
	UPROPERTY(Replicated, BlueprintReadOnly) float TacticalPoints = 0.f;
	UPROPERTY(Replicated, BlueprintReadOnly) int32 Headshots = 0;
	UPROPERTY(Replicated, BlueprintReadOnly) bool bIsBotPlayer = false;
	UPROPERTY(BlueprintReadOnly) int32 ShotsFired = 0;
	UPROPERTY(BlueprintReadOnly) int32 ShotsHit = 0;

	/** Gear carried into the next round if this player survives (server only). */
	FNSCLoadout SavedLoadout;
	/** This round, for MVP / TPS */
	float RoundPoints = 0.f;
	int32 RoundKills = 0;

protected:
	UPROPERTY(Replicated) ENSCTeam Team = ENSCTeam::None;
	UPROPERTY(Replicated) int32 Squad = 0;
	UPROPERTY(Replicated) int32 Money = NSCRules::StartMoney;
	UPROPERTY(Replicated) ENSCInjuryState LifeState = ENSCInjuryState::Dead;
};
