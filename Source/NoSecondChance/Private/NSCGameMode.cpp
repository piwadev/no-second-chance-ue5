#include "NSCGameMode.h"
#include "NoSecondChance.h"
#include "NSCCharacter.h"
#include "NSCPlayerState.h"
#include "NSCGameState.h"
#include "NSCPlayerController.h"
#include "NSCAIController.h"
#include "NSCHealthComponent.h"
#include "NSCWeaponComponent.h"
#include "NSCBomb.h"
#include "NSCGrenade.h"
#include "GameFramework/PlayerStart.h"
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "TimerManager.h"

namespace
{
	const TCHAR* BotNames[] = {
		TEXT("Volkov"), TEXT("Reyes"), TEXT("Hart"), TEXT("Okafor"), TEXT("Lindqvist"), TEXT("Mercer"), TEXT("Sato"),
		TEXT("Brandt"), TEXT("Kowalski"), TEXT("Duarte"), TEXT("Novak"), TEXT("Ibarra"), TEXT("Fischer"), TEXT("Quinn"),
		TEXT("Haddad"), TEXT("Moreau"), TEXT("Petrov"), TEXT("Kaur"), TEXT("Lund"), TEXT("Abara") };
	constexpr int32 TeamSize = 5;
	constexpr float PointsKill = 2.f, PointsDown = 1.f, PointsAssist = 0.5f, PointsRevive = 1.5f, PointsRescue = 1.f,
		PointsPlant = 1.f, PointsDefuse = 2.f, PointsSurvive = 0.3f, PointsWin = 0.5f;
}

ANSCGameMode::ANSCGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = ANSCCharacter::StaticClass();
	PlayerControllerClass = ANSCPlayerController::StaticClass();
	PlayerStateClass = ANSCPlayerState::StaticClass();
	GameStateClass = ANSCGameState::StaticClass();
	bUseSeamlessTravel = false;
	bStartPlayersAsSpectators = true;
}

ANSCGameState* ANSCGameMode::GetNSCGameState() const
{
	return GetGameState<ANSCGameState>();
}

void ANSCGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	// ?Bots=0 disables bot fill. ?Mode=Casual -> first to 7, no overtime. ?RoundsToWin=N overrides.
	bFillWithBots = UGameplayStatics::GetIntOption(Options, TEXT("Bots"), 1) != 0;
	const FString Mode = UGameplayStatics::ParseOption(Options, TEXT("Mode"));
	bCasual = Mode.Equals(TEXT("Casual"), ESearchCase::IgnoreCase);
	if (bCasual) { RoundsToWin = 7; RegulationRounds = 12; bAllowOvertime = false; }
	RoundsToWin = FMath::Clamp(UGameplayStatics::GetIntOption(Options, TEXT("RoundsToWin"), RoundsToWin), 1, 30);
	RegulationRounds = FMath::Max(2, RoundsToWin * 2 - 2);
	BotSkill = FMath::Clamp(UGameplayStatics::GetIntOption(Options, TEXT("BotSkill"), 60), 10, 100) / 100.f;
	UE_LOG(LogNSC, Log, TEXT("Operations: RoundsToWin=%d Regulation=%d Bots=%d Casual=%d Skill=%.2f"), RoundsToWin, RegulationRounds, bFillWithBots, bCasual, BotSkill);
}

void ANSCGameMode::StartPlay()
{
	Super::StartPlay();
	EnsureMapActors();
	if (ANSCGameState* GS = GetNSCGameState())
	{
		GS->SetRoundsToWin(RoundsToWin);
		GS->SetModeName(bCasual ? TEXT("Casual") : TEXT("Operations"));
		GS->SetPhase(ENSCRoundPhase::WaitingForPlayers, 3.f);
	}
}

// ------------------------------------------------------------------------------------------ players

int32 ANSCGameMode::CountPlayersInSquad(int32 Squad) const
{
	int32 N = 0;
	if (const ANSCGameState* GS = GetNSCGameState())
	{
		for (APlayerState* P : GS->PlayerArray)
		{
			const ANSCPlayerState* PS = Cast<ANSCPlayerState>(P);
			if (PS && PS->GetTeam() != ENSCTeam::None && PS->GetSquad() == Squad) { ++N; }
		}
	}
	return N;
}

void ANSCGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	ANSCPlayerState* PS = NewPlayer ? NewPlayer->GetPlayerState<ANSCPlayerState>() : nullptr;
	ANSCGameState* GS = GetNSCGameState();
	if (!PS || !GS) { return; }

	// Put the human on the side with fewer humans, then replace one of that squad's bots.
	int32 Humans[2] = { 0, 0 };
	for (APlayerState* P : GS->PlayerArray)
	{
		const ANSCPlayerState* O = Cast<ANSCPlayerState>(P);
		if (O && O != PS && !O->bIsBotPlayer && O->GetTeam() != ENSCTeam::None) { Humans[O->GetSquad()]++; }
	}
	const int32 Squad = Humans[0] <= Humans[1] ? 0 : 1;
	PS->SetSquad(Squad);
	PS->SetTeam(GS->GetSideOfSquad(Squad));
	PS->SetMoney(NSCRules::StartMoney);

	if (CountPlayersInSquad(Squad) > TeamSize)
	{
		for (FConstControllerIterator It = GetWorld()->GetControllerIterator(); It; ++It)
		{
			ANSCAIController* AI = Cast<ANSCAIController>(It->Get());
			const ANSCPlayerState* BPS = AI ? AI->GetPlayerState<ANSCPlayerState>() : nullptr;
			if (BPS && BPS->GetSquad() == Squad)
			{
				if (APawn* P = AI->GetPawn()) { P->Destroy(); }
				AI->Destroy();
				break;
			}
		}
	}
	UE_LOG(LogNSC, Log, TEXT("%s joined squad %d (%s)"), *PS->GetPlayerName(), Squad, *NSCText::TeamName(PS->GetTeam()));
}

void ANSCGameMode::Logout(AController* Exiting)
{
	if (ANSCCharacter* C = Exiting ? Cast<ANSCCharacter>(Exiting->GetPawn()) : nullptr)
	{
		if (C->HasBomb() && Bomb) { Bomb->Drop(C->GetActorLocation()); }
		C->Destroy();
	}
	Super::Logout(Exiting);
	if (bMatchStarted && bFillWithBots && Cast<APlayerController>(Exiting))
	{
		// keep it 5v5 when someone leaves
		GetWorldTimerManager().SetTimerForNextTick(this, &ANSCGameMode::FillBots);
	}
}

void ANSCGameMode::FillBots()
{
	if (!bFillWithBots) { return; }
	for (int32 Squad = 0; Squad < 2; ++Squad)
	{
		int32 Guard = 0;
		while (CountPlayersInSquad(Squad) < TeamSize && Guard++ < TeamSize) { SpawnBot(Squad); }
	}
}

void ANSCGameMode::SpawnBot(int32 Squad)
{
	ANSCGameState* GS = GetNSCGameState();
	if (!GS) { return; }
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ANSCAIController* AI = GetWorld()->SpawnActor<ANSCAIController>(ANSCAIController::StaticClass(), FTransform::Identity, P);
	if (!AI) { return; }
	ANSCPlayerState* PS = AI->GetPlayerState<ANSCPlayerState>();
	if (!PS) { AI->Destroy(); return; }
	const int32 NameIdx = BotCounter++ % UE_ARRAY_COUNT(BotNames);
	PS->SetPlayerName(FString::Printf(TEXT("BOT %s"), BotNames[NameIdx]));
	PS->SetIsABot(true);
	PS->bIsBotPlayer = true;
	PS->SetSquad(Squad);
	PS->SetTeam(GS->GetSideOfSquad(Squad));
	PS->SetMoney(NSCRules::StartMoney);
}

ENSCTeam ANSCGameMode::TeamOf(const AController* C) const
{
	const ANSCPlayerState* PS = C ? C->GetPlayerState<ANSCPlayerState>() : nullptr;
	return PS ? PS->GetTeam() : ENSCTeam::None;
}

void ANSCGameMode::AssignSides()
{
	ANSCGameState* GS = GetNSCGameState();
	if (!GS) { return; }
	for (APlayerState* P : GS->PlayerArray)
	{
		if (ANSCPlayerState* PS = Cast<ANSCPlayerState>(P))
		{
			if (PS->GetTeam() != ENSCTeam::None) { PS->SetTeam(GS->GetSideOfSquad(PS->GetSquad())); }
		}
	}
}

// ------------------------------------------------------------------------------------------ map helpers

FVector ANSCGameMode::WarehouseToWorld(float BrowserX, float BrowserZ, float Height)
{
	// browser (three.js, metres, -z forward) -> Unreal (cm, X forward, Y right, Z up)
	return FVector(-BrowserZ * 100.f, BrowserX * 100.f, Height);
}

void ANSCGameMode::EnsureMapActors()
{
	bool bHasSites = false;
	for (TActorIterator<ANSCBombSite> It(GetWorld()); It; ++It) { bHasSites = true; break; }
	if (!bHasSites)
	{
		// Fallback for maps without placed sites: the warehouse layout of the browser version.
		struct FSiteDef { const TCHAR* Name; float X; float Z; };
		const FSiteDef Defs[] = { { TEXT("A"), -27.f, -26.f }, { TEXT("B"), 27.f, -26.f } };
		for (const FSiteDef& D : Defs)
		{
			FActorSpawnParameters P;
			ANSCBombSite* Site = GetWorld()->SpawnActor<ANSCBombSite>(ANSCBombSite::StaticClass(), WarehouseToWorld(D.X, D.Z, 150.f), FRotator::ZeroRotator, P);
			if (Site)
			{
				Site->SiteName = FName(D.Name);
				Site->Volume->SetBoxExtent(FVector(800.f, 700.f, 300.f));
			}
		}
		UE_LOG(LogNSC, Warning, TEXT("No ANSCBombSite in map - spawned fallback warehouse sites A/B"));
	}
}

TArray<FTransform> ANSCGameMode::GetSpawnTransforms(ENSCTeam Team) const
{
	TArray<FTransform> Out;
	const FName Tag = Team == ENSCTeam::Attackers ? FName(TEXT("Attackers")) : FName(TEXT("Defenders"));
	TArray<APlayerStart*> Starts;
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		if (It->PlayerStartTag == Tag) { Starts.Add(*It); }
	}
	Starts.Sort([](const APlayerStart& A, const APlayerStart& B) { return A.GetName() < B.GetName(); });
	for (const APlayerStart* S : Starts) { Out.Add(S->GetActorTransform()); }

	// Fallback / overflow: the browser spawn rows behind each team's line.
	const bool bAtk = Team == ENSCTeam::Attackers;
	for (int32 i = Out.Num(); i < 12; ++i)
	{
		const int32 Row = i / 5;
		const int32 Col = i % 5;
		const float X = -6.f + 3.f * Col;
		const float Z = bAtk ? (33.f + 2.5f * Row + (Col % 2) * 1.2f) : -(34.f + 2.5f * Row + (Col % 2) * 1.2f);
		Out.Add(FTransform(FRotator(0.f, bAtk ? 0.f : 180.f, 0.f), WarehouseToWorld(X, Z, 100.f)));
	}
	return Out;
}

void ANSCGameMode::SetAllFrozen(bool bAttackers, bool bDefenders)
{
	for (TActorIterator<ANSCCharacter> It(GetWorld()); It; ++It)
	{
		const ENSCTeam T = It->GetTeam();
		It->SetFrozen(T == ENSCTeam::Attackers ? bAttackers : (T == ENSCTeam::Defenders ? bDefenders : false));
	}
}

// ------------------------------------------------------------------------------------------ round flow

void ANSCGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ANSCGameState* GS = GetNSCGameState();
	if (!GS) { return; }
	const float Remaining = GS->GetPhaseTimeRemaining();

	switch (GS->GetPhase())
	{
	case ENSCRoundPhase::WaitingForPlayers:
		if (Remaining <= 0.f) { BeginMatch(); }
		break;
	case ENSCRoundPhase::Buy:
		if (Remaining <= 0.f) { BeginPreparation(); }
		break;
	case ENSCRoundPhase::Preparation:
		if (Remaining <= 0.f) { BeginLive(); }
		break;
	case ENSCRoundPhase::Live:
		CheckWinConditions();
		if (GS->GetPhase() == ENSCRoundPhase::Live && Remaining <= 0.f) { EndRound(ENSCTeam::Defenders, ENSCRoundEndReason::TimeExpired); }
		break;
	case ENSCRoundPhase::BombPlanted:
		CheckWinConditions();
		if (GS->GetPhase() == ENSCRoundPhase::BombPlanted && GS->GetBombTimeRemaining() <= 0.f)
		{
			if (Bomb) { Bomb->Explode(); }
			EndRound(ENSCTeam::Attackers, ENSCRoundEndReason::BombDetonated);
		}
		break;
	case ENSCRoundPhase::RoundEnd:
		if (Remaining <= 0.f) { AfterRoundEnd(); }
		break;
	case ENSCRoundPhase::HalfTime:
		if (Remaining <= 0.f) { StartRound(); }
		break;
	case ENSCRoundPhase::MatchEnd:
		if (Remaining <= 0.f) { BeginMatch(); }
		break;
	default: break;
	}
}

void ANSCGameMode::BeginMatch()
{
	ANSCGameState* GS = GetNSCGameState();
	if (!GS) { return; }
	bMatchStarted = true;
	FillBots();
	Squad0Score = Squad1Score = 0;
	RoundNumber = 0;
	LossStreak[0] = LossStreak[1] = 0;
	GS->SetScores(0, 0);
	GS->SetSidesSwapped(false);
	GS->SetMatchWinner(-1);
	GS->SetOvertimeIndex(0);
	for (APlayerState* P : GS->PlayerArray)
	{
		if (ANSCPlayerState* PS = Cast<ANSCPlayerState>(P)) { PS->ResetStats(); }
	}
	AssignSides();
	StartRound();
}

void ANSCGameMode::StartRound()
{
	ANSCGameState* GS = GetNSCGameState();
	if (!GS) { return; }
	++RoundNumber;
	GS->SetRoundNumber(RoundNumber);
	GS->ClearBomb();
	bPlantedThisRound = false;
	PlanterPS.Reset();
	RoundRescues = 0;
	LastDraggerOf.Reset();
	AttackSite = FMath::RandBool() ? FName(TEXT("A")) : FName(TEXT("B"));

	// Overtime: standardized money at the start of every overtime.
	if (bAllowOvertime && RoundNumber > RegulationRounds)
	{
		const int32 Rel = RoundNumber - RegulationRounds - 1;
		GS->SetOvertimeIndex(Rel / (2 * NSCRules::OvertimeHalf) + 1);
		if (Rel % (2 * NSCRules::OvertimeHalf) == 0)
		{
			for (APlayerState* P : GS->PlayerArray)
			{
				if (ANSCPlayerState* PS = Cast<ANSCPlayerState>(P)) { PS->SetMoney(NSCRules::OvertimeMoney); PS->SavedLoadout = FNSCLoadout(); }
			}
			LossStreak[0] = LossStreak[1] = 0;
		}
	}

	// Clear the world
	for (TActorIterator<ANSCGrenade> It(GetWorld()); It; ++It) { It->Destroy(); }
	if (Bomb) { Bomb->Destroy(); Bomb = nullptr; }
	for (TActorIterator<ANSCCharacter> It(GetWorld()); It; ++It)
	{
		if (AController* C = It->GetController()) { C->UnPossess(); }
		It->Destroy();
	}

	// Spawn everyone fresh
	TArray<FTransform> AtkSpawns = GetSpawnTransforms(ENSCTeam::Attackers);
	TArray<FTransform> DefSpawns = GetSpawnTransforms(ENSCTeam::Defenders);
	int32 AtkIdx = 0, DefIdx = 0;
	TArray<ANSCCharacter*> Attackers;
	for (FConstControllerIterator It = GetWorld()->GetControllerIterator(); It; ++It)
	{
		AController* C = It->Get();
		ANSCPlayerState* PS = C ? C->GetPlayerState<ANSCPlayerState>() : nullptr;
		if (!PS || PS->GetTeam() == ENSCTeam::None) { continue; }
		const bool bAtk = PS->GetTeam() == ENSCTeam::Attackers;
		TArray<FTransform>& List = bAtk ? AtkSpawns : DefSpawns;
		int32& Idx = bAtk ? AtkIdx : DefIdx;
		const FTransform T = List[Idx % List.Num()];
		++Idx;
		RestartPlayerAtTransform(C, T);
		ANSCCharacter* Ch = Cast<ANSCCharacter>(C->GetPawn());
		if (!Ch) { continue; }
		if (PS->SavedLoadout.bValid)
		{
			Ch->GetWeapons()->ApplyLoadout(PS->SavedLoadout);
			Ch->GetWeapons()->RefillAmmo();
			if (!Ch->GetWeapons()->GetSecondary().IsValid()) { Ch->GetWeapons()->GiveWeapon(UNSCWeaponLibrary::DefaultPistol(PS->GetTeam())); }
		}
		else
		{
			Ch->GetWeapons()->GiveDefaultLoadout(PS->GetTeam());
			Ch->GetWeapons()->Bandages = 1;
		}
		PS->SavedLoadout = FNSCLoadout();
		PS->SetLifeState(ENSCInjuryState::Healthy);
		PS->RoundPoints = 0.f;
		PS->RoundKills = 0;
		if (bAtk) { Attackers.Add(Ch); }
	}

	// The charge goes to a random attacker
	FActorSpawnParameters BP;
	BP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Bomb = GetWorld()->SpawnActor<ANSCBomb>(ANSCBomb::StaticClass(), FTransform::Identity, BP);
	if (Bomb && Attackers.Num() > 0) { Bomb->GiveTo(Attackers[FMath::RandRange(0, Attackers.Num() - 1)]); }

	SetAllFrozen(true, true);
	GS->SetPhase(ENSCRoundPhase::Buy, NSCRules::BuyTime);
	RoundStartTime = GetWorld()->GetTimeSeconds();
	BotsBuy();
	UE_LOG(LogNSC, Log, TEXT("Round %d started (%d-%d)"), RoundNumber, Squad0Score, Squad1Score);
}

void ANSCGameMode::BeginPreparation()
{
	if (ANSCGameState* GS = GetNSCGameState()) { GS->SetPhase(ENSCRoundPhase::Preparation, NSCRules::PrepTime); }
	// Defenders set up; attackers stay behind their barrier.
	SetAllFrozen(true, false);
}

void ANSCGameMode::BeginLive()
{
	if (ANSCGameState* GS = GetNSCGameState()) { GS->SetPhase(ENSCRoundPhase::Live, NSCRules::RoundTime); }
	SetAllFrozen(false, false);
}

int32 ANSCGameMode::CountStanding(ENSCTeam Team) const
{
	int32 N = 0;
	for (TActorIterator<ANSCCharacter> It(GetWorld()); It; ++It)
	{
		if (It->GetTeam() == Team && It->GetHealth()->IsAliveAndStanding()) { ++N; }
	}
	return N;
}

void ANSCGameMode::CheckWinConditions()
{
	ANSCGameState* GS = GetNSCGameState();
	if (!GS) { return; }
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now < NextCheck) { return; }
	NextCheck = Now + 0.2f;

	const bool bHasAtk = CountPlayersInSquad(0) > 0 && CountPlayersInSquad(1) > 0;
	if (!bHasAtk) { return; }   // solo practice: no elimination win
	// A team whose remaining players are all downed is eliminated: nobody is left to revive them.
	const int32 Atk = CountStanding(ENSCTeam::Attackers);
	const int32 Def = CountStanding(ENSCTeam::Defenders);
	if (GS->GetPhase() == ENSCRoundPhase::Live)
	{
		if (Atk == 0) { EndRound(ENSCTeam::Defenders, ENSCRoundEndReason::AttackersEliminated); }
		else if (Def == 0) { EndRound(ENSCTeam::Attackers, ENSCRoundEndReason::DefendersEliminated); }
	}
	else if (GS->GetPhase() == ENSCRoundPhase::BombPlanted)
	{
		if (Def == 0) { EndRound(ENSCTeam::Attackers, ENSCRoundEndReason::DefendersEliminated); }
	}
}

void ANSCGameMode::EndRound(ENSCTeam Winner, ENSCRoundEndReason Reason)
{
	ANSCGameState* GS = GetNSCGameState();
	if (!GS) { return; }
	const ENSCRoundPhase P = GS->GetPhase();
	if (P == ENSCRoundPhase::RoundEnd || P == ENSCRoundPhase::MatchEnd || P == ENSCRoundPhase::HalfTime) { return; }

	const int32 WinnerSquad = GS->GetSideOfSquad(0) == Winner ? 0 : 1;
	if (WinnerSquad == 0) { ++Squad0Score; } else { ++Squad1Score; }
	GS->SetScores(Squad0Score, Squad1Score);

	ApplyRoundEconomy(Winner, Reason);

	// Stats + MVP
	ANSCPlayerState* MVP = nullptr;
	for (APlayerState* PSBase : GS->PlayerArray)
	{
		ANSCPlayerState* PS = Cast<ANSCPlayerState>(PSBase);
		if (!PS || PS->GetTeam() == ENSCTeam::None) { continue; }
		PS->RoundsPlayed++;
		const ANSCCharacter* Ch = Cast<ANSCCharacter>(PS->GetPawn());
		if (Ch && !Ch->GetHealth()->IsDead()) { PS->RoundPoints += PointsSurvive; }
		if (PS->GetTeam() == Winner)
		{
			PS->RoundPoints += PointsWin;
			if (!MVP || PS->RoundPoints > MVP->RoundPoints) { MVP = PS; }
		}
		PS->AddTacticalPoints(PS->RoundPoints);
	}
	if (MVP) { MVP->MVPs++; }

	FNSCRoundSummary S;
	S.Round = RoundNumber;
	S.Winner = Winner;
	S.Reason = Reason;
	S.MVPName = MVP ? MVP->GetPlayerName() : FString();
	S.Rescues = RoundRescues;
	GS->SetLastRound(S);
	GS->SetDefuseProgress(0.f);
	GS->SetPhase(ENSCRoundPhase::RoundEnd, NSCRules::RoundEndTime);
	UE_LOG(LogNSC, Log, TEXT("Round %d: %s win (%s)"), RoundNumber, *NSCText::TeamName(Winner), *NSCText::ReasonText(Reason));
}

void ANSCGameMode::ApplyRoundEconomy(ENSCTeam Winner, ENSCRoundEndReason Reason)
{
	ANSCGameState* GS = GetNSCGameState();
	const int32 WinnerSquad = GS->GetSideOfSquad(0) == Winner ? 0 : 1;
	const int32 LoserSquad = 1 - WinnerSquad;
	const int32 LossBonus = FMath::Min(NSCRules::LossBase + NSCRules::LossStep * LossStreak[LoserSquad], NSCRules::LossMax);
	for (APlayerState* P : GS->PlayerArray)
	{
		ANSCPlayerState* PS = Cast<ANSCPlayerState>(P);
		if (!PS || PS->GetTeam() == ENSCTeam::None) { continue; }
		if (PS->GetSquad() == WinnerSquad)
		{
			PS->AddMoney(NSCRules::WinReward);
		}
		else
		{
			int32 Amount = LossBonus;
			if (PS->GetTeam() == ENSCTeam::Attackers && bPlantedThisRound) { Amount += NSCRules::PlantTeamBonusOnLoss; }
			PS->AddMoney(Amount);
		}
	}
	LossStreak[WinnerSquad] = FMath::Max(0, LossStreak[WinnerSquad] - 1);
	LossStreak[LoserSquad] = FMath::Min(4, LossStreak[LoserSquad] + 1);
}

void ANSCGameMode::SnapshotSurvivorLoadouts()
{
	for (FConstControllerIterator It = GetWorld()->GetControllerIterator(); It; ++It)
	{
		AController* C = It->Get();
		ANSCPlayerState* PS = C ? C->GetPlayerState<ANSCPlayerState>() : nullptr;
		if (!PS) { continue; }
		const ANSCCharacter* Ch = Cast<ANSCCharacter>(C->GetPawn());
		if (Ch && !Ch->GetHealth()->IsDead()) { PS->SavedLoadout = Ch->GetWeapons()->MakeLoadout(); }
		else { PS->SavedLoadout = FNSCLoadout(); }
	}
}

bool ANSCGameMode::IsHalfTimeAfterRound(int32 Round) const
{
	if (Round == RegulationRounds / 2) { return true; }
	if (bAllowOvertime && Round > RegulationRounds) { return (Round - RegulationRounds) % NSCRules::OvertimeHalf == 0; }
	return false;
}

int32 ANSCGameMode::WinningSquadIfMatchOver() const
{
	const int32 Hi = FMath::Max(Squad0Score, Squad1Score);
	const int32 Leader = Squad0Score > Squad1Score ? 0 : 1;
	if (RoundNumber <= RegulationRounds)
	{
		if (Hi >= RoundsToWin) { return Leader; }
		if (RoundNumber == RegulationRounds && !bAllowOvertime)
		{
			return Squad0Score == Squad1Score ? 2 : Leader;   // 2 = draw
		}
		return -1;
	}
	// MR6 overtime: win 4 of 6. Tied 3-3 -> another overtime.
	const int32 Rel = RoundNumber - RegulationRounds - 1;
	const int32 OT = Rel / (2 * NSCRules::OvertimeHalf);
	const int32 Need = (RoundsToWin - 1) + NSCRules::OvertimeHalf * OT + NSCRules::OvertimeHalf + 1;
	return Hi >= Need ? Leader : -1;
}

void ANSCGameMode::AfterRoundEnd()
{
	SnapshotSurvivorLoadouts();
	const int32 W = WinningSquadIfMatchOver();
	if (W >= 0) { EndMatch(W); return; }
	if (IsHalfTimeAfterRound(RoundNumber)) { BeginHalfTime(); return; }
	StartRound();
}

void ANSCGameMode::BeginHalfTime()
{
	ANSCGameState* GS = GetNSCGameState();
	if (!GS) { return; }
	GS->SetSidesSwapped(!GS->AreSidesSwapped());
	AssignSides();
	const bool bOT = RoundNumber > RegulationRounds;
	for (APlayerState* P : GS->PlayerArray)
	{
		if (ANSCPlayerState* PS = Cast<ANSCPlayerState>(P))
		{
			PS->SetMoney(bOT ? NSCRules::OvertimeMoney : NSCRules::StartMoney);
			PS->SavedLoadout = FNSCLoadout();
		}
	}
	LossStreak[0] = LossStreak[1] = 0;
	SetAllFrozen(true, true);
	GS->SetPhase(ENSCRoundPhase::HalfTime, bOT ? 6.f : NSCRules::HalfTimeDuration);
	UE_LOG(LogNSC, Log, TEXT("Half time after round %d - sides swapped"), RoundNumber);
}

void ANSCGameMode::EndMatch(int32 WinnerSquad)
{
	ANSCGameState* GS = GetNSCGameState();
	if (!GS) { return; }
	GS->SetMatchWinner(WinnerSquad);
	SetAllFrozen(true, true);
	GS->SetPhase(ENSCRoundPhase::MatchEnd, NSCRules::MatchEndTime);
	UE_LOG(LogNSC, Log, TEXT("Match over: squad %d (%d-%d)"), WinnerSquad, Squad0Score, Squad1Score);
}

// ------------------------------------------------------------------------------------------ notifications

bool ANSCGameMode::ShouldTakeDamage(const ANSCCharacter* Victim, const AController* InstigatorController) const
{
	if (!Victim || !InstigatorController) { return true; }
	if (Victim->GetController() == InstigatorController) { return true; }   // your own frag
	return TeamOf(InstigatorController) != Victim->GetTeam();
}

void ANSCGameMode::NotifyDamage(ANSCCharacter* Victim, const FNSCHitInfo& Hit, float HealthDamage)
{
	ANSCPlayerState* PS = Hit.Instigator ? Hit.Instigator->GetPlayerState<ANSCPlayerState>() : nullptr;
	if (!PS || !Victim || PS->GetTeam() == Victim->GetTeam()) { return; }
	const float D = FMath::Clamp(HealthDamage, 0.f, 100.f);
	PS->DamageDealt += D;
	PS->RoundPoints += D / 100.f;
}

void ANSCGameMode::NotifyDowned(ANSCCharacter* Victim, const FNSCHitInfo& Hit)
{
	ANSCGameState* GS = GetNSCGameState();
	if (!Victim || !GS) { return; }
	ANSCPlayerState* VPS = Victim->GetPlayerState<ANSCPlayerState>();
	ANSCPlayerState* KPS = Hit.Instigator ? Hit.Instigator->GetPlayerState<ANSCPlayerState>() : nullptr;
	if (VPS) { VPS->SetLifeState(ENSCInjuryState::Downed); }
	if (KPS && KPS != VPS && KPS->GetTeam() != Victim->GetTeam()) { KPS->Downs++; KPS->RoundPoints += PointsDown; }

	FNSCKillFeedEntry E;
	E.Killer = KPS ? KPS->GetPlayerName() : FString();
	E.Victim = VPS ? VPS->GetPlayerName() : Victim->GetDisplayName();
	E.Weapon = Hit.WeaponId.ToString();
	E.KillerTeam = KPS ? KPS->GetTeam() : ENSCTeam::None;
	E.VictimTeam = Victim->GetTeam();
	E.bDowned = true;
	E.Time = GetWorld()->GetTimeSeconds();
	GS->AddKillFeed(E);
	NextCheck = 0.f;
}

void ANSCGameMode::NotifyKilled(ANSCCharacter* Victim, const FNSCHitInfo& Hit)
{
	ANSCGameState* GS = GetNSCGameState();
	if (!Victim || !GS) { return; }
	ANSCPlayerState* VPS = Victim->GetPlayerState<ANSCPlayerState>();
	AController* KillerC = Hit.Instigator.Get();
	ANSCPlayerState* KPS = KillerC ? KillerC->GetPlayerState<ANSCPlayerState>() : nullptr;
	if (VPS) { VPS->Deaths++; VPS->SetLifeState(ENSCInjuryState::Dead); }

	if (KPS && KPS != VPS)
	{
		if (KPS->GetTeam() != Victim->GetTeam())
		{
			KPS->Kills++;
			KPS->RoundKills++;
			KPS->RoundPoints += PointsKill;
			FNSCWeaponStats S;
			KPS->AddMoney(UNSCWeaponLibrary::GetWeaponStats(Hit.WeaponId, S) ? S.KillReward : 300);
		}
		else
		{
			KPS->Kills--;
			KPS->AddMoney(-300);
		}
	}
	// Assists: 40+ damage this life from other enemies
	for (const TPair<TWeakObjectPtr<AController>, float>& Pair : Victim->GetHealth()->GetDamageBy())
	{
		AController* C = Pair.Key.Get();
		ANSCPlayerState* APS = C ? C->GetPlayerState<ANSCPlayerState>() : nullptr;
		if (APS && APS != KPS && APS->GetTeam() != Victim->GetTeam() && Pair.Value >= 40.f)
		{
			APS->Assists++;
			APS->RoundPoints += PointsAssist;
		}
	}

	FNSCKillFeedEntry E;
	E.Killer = KPS ? KPS->GetPlayerName() : FString();
	E.Victim = VPS ? VPS->GetPlayerName() : Victim->GetDisplayName();
	E.Weapon = Hit.WeaponId.ToString();
	E.KillerTeam = KPS ? KPS->GetTeam() : ENSCTeam::None;
	E.VictimTeam = Victim->GetTeam();
	E.bHeadshot = Hit.Zone == ENSCHitZone::Head && !Hit.bExplosive;
	E.Time = GetWorld()->GetTimeSeconds();
	GS->AddKillFeed(E);
	NextCheck = 0.f;
}

void ANSCGameMode::NotifyRevived(ANSCCharacter* Victim, AController* Rescuer)
{
	ANSCGameState* GS = GetNSCGameState();
	if (!Victim || !GS) { return; }
	ANSCPlayerState* VPS = Victim->GetPlayerState<ANSCPlayerState>();
	ANSCPlayerState* RPS = Rescuer ? Rescuer->GetPlayerState<ANSCPlayerState>() : nullptr;
	if (VPS) { VPS->TimesRevived++; VPS->SetLifeState(ENSCInjuryState::Critical); }
	if (RPS)
	{
		RPS->Revives++;
		RPS->RoundPoints += PointsRevive;
		RPS->AddMoney(NSCRules::RescueReward);
	}
	if (AController* Dragger = Victim->LastDraggedBy.Get())
	{
		if (ANSCPlayerState* DPS = Dragger->GetPlayerState<ANSCPlayerState>())
		{
			DPS->Rescues++;
			DPS->RoundPoints += PointsRescue;
		}
	}
	Victim->LastDraggedBy.Reset();
	++RoundRescues;

	FNSCKillFeedEntry E;
	E.Killer = RPS ? RPS->GetPlayerName() : FString();
	E.Victim = VPS ? VPS->GetPlayerName() : Victim->GetDisplayName();
	E.Weapon = TEXT("Medkit");
	E.KillerTeam = Victim->GetTeam();
	E.VictimTeam = Victim->GetTeam();
	E.bRescue = true;
	E.Time = GetWorld()->GetTimeSeconds();
	GS->AddKillFeed(E);
}

void ANSCGameMode::OnBombPlanted(ANSCCharacter* Planter, FName Site)
{
	ANSCGameState* GS = GetNSCGameState();
	if (!GS || GS->GetPhase() != ENSCRoundPhase::Live || !Bomb) { return; }
	Bomb->Plant(Planter, Site);
	bPlantedThisRound = true;
	if (ANSCPlayerState* PS = Planter ? Planter->GetPlayerState<ANSCPlayerState>() : nullptr)
	{
		PS->Plants++;
		PS->RoundPoints += PointsPlant;
		PS->AddMoney(NSCRules::PlantBonus);
		PlanterPS = PS;
	}
	GS->SetBombPlanted(Site, NSCRules::BombTime);
	GS->SetPhase(ENSCRoundPhase::BombPlanted, NSCRules::BombTime);
	UE_LOG(LogNSC, Log, TEXT("Charge planted at %s"), *Site.ToString());
}

void ANSCGameMode::OnBombDefused(ANSCCharacter* Defuser)
{
	ANSCGameState* GS = GetNSCGameState();
	if (!GS || GS->GetPhase() != ENSCRoundPhase::BombPlanted || !Bomb) { return; }
	Bomb->SetDefused();
	if (ANSCPlayerState* PS = Defuser ? Defuser->GetPlayerState<ANSCPlayerState>() : nullptr)
	{
		PS->Defuses++;
		PS->RoundPoints += PointsDefuse;
		PS->AddMoney(NSCRules::DefuseBonus);
	}
	EndRound(ENSCTeam::Defenders, ENSCRoundEndReason::BombDefused);
}

// ------------------------------------------------------------------------------------------ buying

bool ANSCGameMode::IsBuyAllowed() const
{
	const ANSCGameState* GS = GetNSCGameState();
	if (!GS) { return false; }
	const ENSCRoundPhase P = GS->GetPhase();
	return P == ENSCRoundPhase::Buy || P == ENSCRoundPhase::Preparation;
}

bool ANSCGameMode::TryBuyWeapon(AController* Buyer, FName WeaponId)
{
	ANSCPlayerState* PS = Buyer ? Buyer->GetPlayerState<ANSCPlayerState>() : nullptr;
	ANSCCharacter* Ch = Buyer ? Cast<ANSCCharacter>(Buyer->GetPawn()) : nullptr;
	if (!PS || !Ch || !IsBuyAllowed() || !Ch->GetHealth()->IsAliveAndStanding()) { return false; }
	FNSCWeaponStats S;
	if (!UNSCWeaponLibrary::GetWeaponStats(WeaponId, S)) { return false; }
	if (S.TeamOnly != ENSCTeam::None && S.TeamOnly != PS->GetTeam()) { return false; }
	const FNSCWeaponInstance& Current = S.Slot == ENSCWeaponSlot::Primary ? Ch->GetWeapons()->GetPrimary() : Ch->GetWeapons()->GetSecondary();
	if (Current.Id == WeaponId) { return false; }
	if (!PS->SpendMoney(S.Price)) { return false; }
	Ch->GetWeapons()->GiveWeapon(WeaponId);
	return true;
}

bool ANSCGameMode::TryBuyEquipment(AController* Buyer, ENSCEquipment Item)
{
	ANSCPlayerState* PS = Buyer ? Buyer->GetPlayerState<ANSCPlayerState>() : nullptr;
	ANSCCharacter* Ch = Buyer ? Cast<ANSCCharacter>(Buyer->GetPawn()) : nullptr;
	if (!PS || !Ch || !IsBuyAllowed() || !Ch->GetHealth()->IsAliveAndStanding()) { return false; }
	const FNSCEquipmentInfo* Info = UNSCWeaponLibrary::FindEquipment(Item);
	if (!Info) { return false; }
	if (Info->TeamOnly != ENSCTeam::None && Info->TeamOnly != PS->GetTeam()) { return false; }
	UNSCWeaponComponent* W = Ch->GetWeapons();
	UNSCHealthComponent* H = Ch->GetHealth();
	int32 Price = Info->Price;
	const int32 Grenades = W->Smokes + W->Flashes + W->Frags;

	switch (Item)
	{
	case ENSCEquipment::Armor:
		if (H->GetArmor() >= 100.f) { return false; }
		if (!PS->SpendMoney(Price)) { return false; }
		H->SetArmor(100.f, H->HasHelmet());
		return true;
	case ENSCEquipment::ArmorHelmet:
		if (H->GetArmor() >= 100.f && H->HasHelmet()) { return false; }
		if (H->GetArmor() >= 100.f) { Price = 350; }
		if (!PS->SpendMoney(Price)) { return false; }
		H->SetArmor(100.f, true);
		return true;
	case ENSCEquipment::Medkit:
		if (W->Medkits >= Info->MaxCarry || !PS->SpendMoney(Price)) { return false; }
		W->Medkits++;
		return true;
	case ENSCEquipment::Bandage:
		if (W->Bandages >= Info->MaxCarry || !PS->SpendMoney(Price)) { return false; }
		W->Bandages++;
		return true;
	case ENSCEquipment::Tourniquet:
		if (W->Tourniquets >= Info->MaxCarry || !PS->SpendMoney(Price)) { return false; }
		W->Tourniquets++;
		return true;
	case ENSCEquipment::Smoke:
		if (W->Smokes >= Info->MaxCarry || Grenades >= 4 || !PS->SpendMoney(Price)) { return false; }
		W->Smokes++;
		return true;
	case ENSCEquipment::Flash:
		if (W->Flashes >= Info->MaxCarry || Grenades >= 4 || !PS->SpendMoney(Price)) { return false; }
		W->Flashes++;
		return true;
	case ENSCEquipment::Frag:
		if (W->Frags >= Info->MaxCarry || Grenades >= 4 || !PS->SpendMoney(Price)) { return false; }
		W->Frags++;
		return true;
	case ENSCEquipment::DefuseKit:
		if (W->bDefuseKit || !PS->SpendMoney(Price)) { return false; }
		W->bDefuseKit = true;
		return true;
	case ENSCEquipment::Resupply:
		if (!PS->SpendMoney(Price)) { return false; }
		W->RefillAmmo();
		return true;
	default:
		return false;
	}
}

void ANSCGameMode::BotsBuy()
{
	for (FConstControllerIterator It = GetWorld()->GetControllerIterator(); It; ++It)
	{
		ANSCAIController* AI = Cast<ANSCAIController>(It->Get());
		ANSCPlayerState* PS = AI ? AI->GetPlayerState<ANSCPlayerState>() : nullptr;
		ANSCCharacter* Ch = AI ? Cast<ANSCCharacter>(AI->GetPawn()) : nullptr;
		if (!PS || !Ch) { continue; }
		const bool bAtk = PS->GetTeam() == ENSCTeam::Attackers;
		const bool bHasPrimary = Ch->GetWeapons()->GetPrimary().IsValid();
		const int32 Money = PS->GetMoney();

		if (!bHasPrimary)
		{
			FName Rifle = bAtk ? FName(TEXT("AK")) : FName(TEXT("M4"));
			const float Roll = FMath::FRand();
			if (Roll < 0.12f) { Rifle = TEXT("HK416"); }
			else if (Roll < 0.18f && Money >= 5800) { Rifle = TEXT("Bolt"); }
			FNSCWeaponStats S;
			UNSCWeaponLibrary::GetWeaponStats(Rifle, S);
			if (Money >= S.Price + 1000) { TryBuyWeapon(AI, Rifle); }
			else if (Money >= 2300 && RoundNumber != 1)
			{
				const FName Smg = FMath::RandBool() ? FName(TEXT("MP5")) : FName(TEXT("UMP"));
				TryBuyWeapon(AI, Smg);
			}
		}
		const bool bArmed = Ch->GetWeapons()->GetPrimary().IsValid();
		if (bArmed || PS->GetMoney() >= 1650) { TryBuyEquipment(AI, PS->GetMoney() >= 1000 ? ENSCEquipment::ArmorHelmet : ENSCEquipment::Armor); }
		else if (PS->GetMoney() >= 650 && RoundNumber == 1) { TryBuyEquipment(AI, ENSCEquipment::Armor); }
		if (bArmed)
		{
			if (FMath::FRand() < 0.4f) { TryBuyEquipment(AI, ENSCEquipment::Medkit); }
			if (!bAtk && FMath::FRand() < 0.5f) { TryBuyEquipment(AI, ENSCEquipment::DefuseKit); }
			TryBuyEquipment(AI, ENSCEquipment::Flash);
			if (FMath::FRand() < 0.6f) { TryBuyEquipment(AI, ENSCEquipment::Smoke); }
			if (FMath::FRand() < 0.4f) { TryBuyEquipment(AI, ENSCEquipment::Frag); }
			TryBuyEquipment(AI, ENSCEquipment::Bandage);
		}
	}
}
