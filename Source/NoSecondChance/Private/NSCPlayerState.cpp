#include "NSCPlayerState.h"
#include "Net/UnrealNetwork.h"

ANSCPlayerState::ANSCPlayerState()
{
}

void ANSCPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ANSCPlayerState, Team);
	DOREPLIFETIME(ANSCPlayerState, Squad);
	DOREPLIFETIME(ANSCPlayerState, Money);
	DOREPLIFETIME(ANSCPlayerState, LifeState);
	DOREPLIFETIME(ANSCPlayerState, Kills);
	DOREPLIFETIME(ANSCPlayerState, Deaths);
	DOREPLIFETIME(ANSCPlayerState, Assists);
	DOREPLIFETIME(ANSCPlayerState, Downs);
	DOREPLIFETIME(ANSCPlayerState, DamageDealt);
	DOREPLIFETIME(ANSCPlayerState, Revives);
	DOREPLIFETIME(ANSCPlayerState, Rescues);
	DOREPLIFETIME(ANSCPlayerState, TimesRevived);
	DOREPLIFETIME(ANSCPlayerState, Plants);
	DOREPLIFETIME(ANSCPlayerState, Defuses);
	DOREPLIFETIME(ANSCPlayerState, MVPs);
	DOREPLIFETIME(ANSCPlayerState, RoundsPlayed);
	DOREPLIFETIME(ANSCPlayerState, TacticalPoints);
	DOREPLIFETIME(ANSCPlayerState, Headshots);
	DOREPLIFETIME(ANSCPlayerState, bIsBotPlayer);
}

void ANSCPlayerState::CopyProperties(APlayerState* PlayerState)
{
	Super::CopyProperties(PlayerState);
	if (ANSCPlayerState* PS = Cast<ANSCPlayerState>(PlayerState))
	{
		PS->Team = Team;
		PS->Squad = Squad;
		PS->Money = Money;
	}
}

float ANSCPlayerState::GetTPS() const
{
	// Tactical Performance Score: ~1.00 is an average round. Kills, downs, assists, revives, rescues, objective
	// play and damage all count, so a medic who keeps the team alive scores as well as a fragger.
	if (RoundsPlayed <= 0) { return 0.f; }
	return (TacticalPoints / static_cast<float>(RoundsPlayed)) / 1.6f;
}

void ANSCPlayerState::ResetStats()
{
	Kills = Deaths = Assists = Downs = Revives = Rescues = TimesRevived = Plants = Defuses = MVPs = RoundsPlayed = Headshots = 0;
	DamageDealt = 0.f;
	TacticalPoints = 0.f;
	ShotsFired = ShotsHit = 0;
	Money = NSCRules::StartMoney;
	SavedLoadout = FNSCLoadout();
}
