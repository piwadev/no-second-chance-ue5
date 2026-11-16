#include "NSCAIController.h"
#include "NoSecondChance.h"
#include "NSCCharacter.h"
#include "NSCHealthComponent.h"
#include "NSCWeaponComponent.h"
#include "NSCPlayerState.h"
#include "NSCGameState.h"
#include "NSCGameMode.h"
#include "NSCBomb.h"
#include "NSCGrenade.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"

ANSCAIController::ANSCAIController()
{
	bWantsPlayerState = true;
	PrimaryActorTick.bCanEverTick = true;
	bSetControlRotationFromPawnOrientation = false;
}

ANSCCharacter* ANSCAIController::GetBot() const
{
	return Cast<ANSCCharacter>(GetPawn());
}

float ANSCAIController::Skill() const
{
	const ANSCGameMode* GM = GetWorld()->GetAuthGameMode<ANSCGameMode>();
	return GM ? GM->GetBotSkill() : 0.6f;
}

void ANSCAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	Target = nullptr;
	Goal = ENSCBotGoal::Idle;
	PathPoints.Reset();
	PathDest = FVector(MAX_flt);
	bThrewSmoke = false;
	LastSawEnemyTime = -100.f;
	StartDelayUntil = GetWorld()->GetTimeSeconds() + FMath::FRandRange(0.f, 2.5f);
	if (InPawn) { SetControlRotation(InPawn->GetActorRotation()); }
}

// ------------------------------------------------------------------------------------- helpers

ANSCBombSite* ANSCAIController::FindSite(FName Name) const
{
	ANSCBombSite* Any = nullptr;
	for (TActorIterator<ANSCBombSite> It(GetWorld()); It; ++It)
	{
		if (It->SiteName == Name) { return *It; }
		Any = *It;
	}
	return Any;
}

FVector ANSCAIController::RandomReachable(const FVector& Center, float Radius) const
{
	if (UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		FNavLocation Out;
		if (Nav->GetRandomReachablePointInRadius(Center, Radius, Out)) { return Out.Location; }
	}
	const FVector2D R = FMath::RandPointInCircle(Radius);
	return Center + FVector(R.X, R.Y, 0.f);
}

ANSCCharacter* ANSCAIController::FindDownedTeammate(const ANSCCharacter* Me, float MaxDist) const
{
	ANSCCharacter* Best = nullptr;
	float BestD = MaxDist;
	for (TActorIterator<ANSCCharacter> It(GetWorld()); It; ++It)
	{
		if (*It == Me || It->GetTeam() != Me->GetTeam() || !It->GetHealth()->IsDowned()) { continue; }
		const float D = FVector::Dist(It->GetActorLocation(), Me->GetActorLocation());
		if (D < BestD) { BestD = D; Best = *It; }
	}
	return Best;
}

bool ANSCAIController::CanSee(const ANSCCharacter* Me, const ANSCCharacter* Other, FVector& OutAimPoint) const
{
	const FVector Eye = Me->GetEyeLocation();
	const FVector Head = Other->GetActorLocation() + FVector(0.f, 0.f, Other->GetHealth()->IsDowned() ? -40.f : 62.f);
	const FVector Chest = Other->GetActorLocation() + FVector(0.f, 0.f, Other->GetHealth()->IsDowned() ? -60.f : 25.f);
	const FVector ToHead = Head - Eye;
	const float Dist = ToHead.Size();
	if (Dist > 7000.f) { return false; }
	const FVector Fwd = GetControlRotation().Vector();
	const float CosHalfFov = FMath::Cos(FMath::DegreesToRadians(Dist < 400.f ? 85.f : 60.f));
	if (FVector::DotProduct(Fwd, ToHead / FMath::Max(Dist, 1.f)) < CosHalfFov) { return false; }

	FCollisionQueryParams P(SCENE_QUERY_STAT(NSCBotSight), false, Me);
	P.AddIgnoredActor(Other);
	for (const FVector& Point : { Head, Chest })
	{
		FHitResult Hit;
		if (!GetWorld()->LineTraceSingleByChannel(Hit, Eye, Point, ECC_Visibility, P) && !ANSCGrenade::IsLineBlockedBySmoke(GetWorld(), Eye, Point))
		{
			OutAimPoint = Point;
			return true;
		}
	}
	return false;
}

void ANSCAIController::FaceTowards(const FVector& Point, float DeltaSeconds, float Speed)
{
	const ANSCCharacter* Me = GetBot();
	if (!Me) { return; }
	const FRotator Want = (Point - Me->GetEyeLocation()).Rotation();
	const FRotator Now = GetControlRotation();
	SetControlRotation(FMath::RInterpTo(Now, FRotator(Want.Pitch, Want.Yaw, 0.f), DeltaSeconds, Speed));
}

bool ANSCAIController::RebuildPath(const ANSCCharacter* Me, const FVector& Dest)
{
	PathPoints.Reset();
	PathIndex = 0;
	PathDest = Dest;
	if (UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(), Me->GetActorLocation(), Dest, const_cast<ANSCCharacter*>(Me)))
	{
		if (Path->IsValid() && Path->PathPoints.Num() > 1)
		{
			PathPoints = Path->PathPoints;
			PathIndex = 1;
			return true;
		}
	}
	PathPoints.Add(Dest);   // no navmesh: steer straight
	return false;
}

void ANSCAIController::MoveTowards(ANSCCharacter* Me, const FVector& Dest, float AcceptRadius, bool bAllowSprint)
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (FVector::Dist2D(Me->GetActorLocation(), Dest) <= AcceptRadius) { Me->SetWantsSprint(false); return; }
	if (FVector::Dist(PathDest, Dest) > 150.f || Now >= NextPathTime || !PathPoints.IsValidIndex(PathIndex))
	{
		RebuildPath(Me, Dest);
		NextPathTime = Now + 1.5f;
	}
	if (!PathPoints.IsValidIndex(PathIndex)) { return; }
	FVector Next = PathPoints[PathIndex];
	if (FVector::Dist2D(Me->GetActorLocation(), Next) < 70.f && PathPoints.IsValidIndex(PathIndex + 1))
	{
		++PathIndex;
		Next = PathPoints[PathIndex];
	}
	const FVector Dir = (Next - Me->GetActorLocation()).GetSafeNormal2D();
	Me->AddMovementInput(Dir, 1.f);
	Me->SetWantsSprint(bAllowSprint && FVector::Dist2D(Me->GetActorLocation(), Dest) > 1200.f);

	// stuck detection
	if (Now >= StuckCheckTime)
	{
		if (FVector::Dist2D(StuckCheckLocation, Me->GetActorLocation()) < 40.f)
		{
			Me->Jump();
			NextPathTime = 0.f;
			PathDest = FVector(MAX_flt);
		}
		StuckCheckLocation = Me->GetActorLocation();
		StuckCheckTime = Now + 1.5f;
	}
}

void ANSCAIController::ManageWeapon(ANSCCharacter* Me, bool bInCombat)
{
	UNSCWeaponComponent* W = Me->GetWeapons();
	if (W->GetActiveSlot() == ENSCWeaponSlot::Grenade)
	{
		const float Now = GetWorld()->GetTimeSeconds();
		if (bPendingThrow && !bInCombat && Now < PendingThrowGiveUp)
		{
			const FRotator Aim = (PendingThrowTarget - Me->GetEyeLocation()).Rotation();
			SetControlRotation(Aim);
			if (W->CanFire()) { W->BotFireOnce(Aim); bPendingThrow = false; }
			return;
		}
		bPendingThrow = false;
		W->SelectSlot(W->GetPrimary().IsValid() ? ENSCWeaponSlot::Primary : ENSCWeaponSlot::Secondary);
		return;
	}
	if (W->GetPrimary().IsValid() && W->GetActiveSlot() != ENSCWeaponSlot::Primary && (W->GetPrimary().Rounds() > 0 || W->GetPrimary().ReserveRounds() > 0))
	{
		W->SelectSlot(ENSCWeaponSlot::Primary);
	}
	const FNSCWeaponInstance* Active = W->GetActiveWeapon();
	if (!Active || W->IsReloading()) { return; }
	FNSCWeaponStats S;
	if (!W->GetActiveStats(S)) { return; }
	if (Active->Rounds() == 0)
	{
		if (Active->SpareMags() > 0) { W->Reload(); }
		else if (W->GetActiveSlot() == ENSCWeaponSlot::Primary && W->GetSecondary().IsValid()) { W->SelectSlot(ENSCWeaponSlot::Secondary); }
	}
	else if (!bInCombat && Active->Rounds() < S.MagSize * 0.5f) { W->Reload(); }
}

// ------------------------------------------------------------------------------------- tick

void ANSCAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ANSCCharacter* Me = GetBot();
	if (!Me || Me->GetHealth()->IsDead()) { return; }
	const ANSCGameState* GS = GetWorld()->GetGameState<ANSCGameState>();
	if (!GS) { return; }
	const float Now = GetWorld()->GetTimeSeconds();

	if (RoundSeen != GS->GetRoundNumber())
	{
		RoundSeen = GS->GetRoundNumber();
		Goal = ENSCBotGoal::Idle;
		NextGoalTime = 0.f;
	}

	if (Me->GetHealth()->IsDowned()) { DownedBehaviour(Me); return; }
	if (Me->IsFrozen())
	{
		// buy / prep: look toward the middle of the map
		FaceTowards(FVector(0.f, 0.f, Me->GetActorLocation().Z), DeltaSeconds, 2.f);
		return;
	}

	UpdatePerception(Me, Now);
	const bool bCombat = Target && !Target->GetHealth()->IsDead() && Now - LastSawEnemyTime < 0.35f;
	ManageWeapon(Me, bCombat);

	if (bCombat)
	{
		Combat(Me, DeltaSeconds, Now);
		return;
	}
	Me->GetWeapons()->SetAiming(false);

	// patch yourself up when safe
	if (Now - LastSawEnemyTime > 2.f && !Me->IsBusyWithAction())
	{
		if (Me->GetHealth()->GetBleed() == ENSCBleed::Heavy) { Me->ServerUseBandage_Internal(true); }
		if (Me->GetHealth()->GetBleed() != ENSCBleed::None && !Me->IsBusyWithAction()) { Me->ServerUseBandage_Internal(false); }
	}
	if (Me->IsBusyWithAction()) { return; }   // planting / defusing / reviving / bandaging: hold still

	if (Now >= NextGoalTime) { ChooseGoal(Me, Now); }
	PursueGoal(Me, DeltaSeconds, Now);
}

void ANSCAIController::UpdatePerception(ANSCCharacter* Me, float Now)
{
	if (Now < NextPerception) { return; }
	NextPerception = Now + 0.1f;
	if (Me->BotFlashedUntil > Now) { Target = nullptr; return; }

	ANSCCharacter* Best = nullptr;
	float BestD = MAX_flt;
	FVector BestAim = FVector::ZeroVector;
	for (TActorIterator<ANSCCharacter> It(GetWorld()); It; ++It)
	{
		ANSCCharacter* O = *It;
		if (O == Me || O->GetTeam() == Me->GetTeam() || O->GetTeam() == ENSCTeam::None || O->GetHealth()->IsDead()) { continue; }
		FVector Aim;
		if (!CanSee(Me, O, Aim)) { continue; }
		float D = FVector::Dist(O->GetActorLocation(), Me->GetActorLocation());
		if (O->GetHealth()->IsDowned()) { D += 1500.f; }   // standing threats first
		if (D < BestD) { BestD = D; Best = O; BestAim = Aim; }
	}
	if (Best)
	{
		if (Best != Target || Now - LastSawEnemyTime > 1.f)
		{
			// new contact: reaction time 0.18 s (skill 100) .. 0.6 s (skill 10)
			ReactionReadyTime = Now + FMath::Lerp(0.6f, 0.18f, Skill()) + FMath::FRandRange(0.f, 0.12f);
			const float Err = FMath::Lerp(90.f, 18.f, Skill());
			AimError = FVector(FMath::FRandRange(-Err, Err), FMath::FRandRange(-Err, Err), FMath::FRandRange(-Err * 0.5f, Err));
			BurstShots = 0;
		}
		Target = Best;
		LastSawEnemyTime = Now;
		LastKnownEnemy = Best->GetActorLocation();
	}
}

void ANSCAIController::Combat(ANSCCharacter* Me, float DeltaSeconds, float Now)
{
	UNSCWeaponComponent* W = Me->GetWeapons();
	if (W->GetActiveSlot() == ENSCWeaponSlot::Grenade) { W->SelectSlot(W->GetPrimary().IsValid() ? ENSCWeaponSlot::Primary : ENSCWeaponSlot::Secondary); }
	if (Me->IsDragging()) { Me->StopDragging(); }
	Me->SetWantsSprint(false);

	const FVector TargetLoc = Target->GetActorLocation();
	const float Dist = FVector::Dist(TargetLoc, Me->GetActorLocation());
	const bool bHeadshots = FMath::FRand() < Skill();
	const FVector AimPoint = TargetLoc + FVector(0.f, 0.f, Target->GetHealth()->IsDowned() ? -50.f : (bHeadshots ? 60.f : 25.f));

	// error shrinks while tracking
	AimError = FMath::VInterpTo(AimError, FVector::ZeroVector, DeltaSeconds, 1.5f + Skill() * 3.f);
	FVector Desired = AimPoint + AimError * FMath::Clamp(Dist / 1500.f, 0.3f, 1.5f);

	// spray compensation: pull down against accumulated heat
	Desired.Z -= W->GetSprayHeat() * FMath::Lerp(2.f, 9.f, Skill()) * (Dist / 1000.f);
	FaceTowards(Desired, DeltaSeconds, FMath::Lerp(7.f, 22.f, Skill()));

	// movement: counter-strafe at range, strafe-spray up close
	if (Now >= NextStrafeFlip) { StrafeSign = FMath::RandBool() ? 1.f : -1.f; NextStrafeFlip = Now + FMath::FRandRange(0.4f, 1.1f); }
	if (Dist < 900.f && Skill() > 0.35f && W->GetActiveWeapon() && W->GetActiveWeapon()->Rounds() > 0)
	{
		const FVector Right = FRotationMatrix(GetControlRotation()).GetUnitAxis(EAxis::Y);
		if (BurstShots == 0 || Now < BurstPauseUntil) { Me->AddMovementInput(Right * StrafeSign, 0.6f); }
	}
	W->SetAiming(Dist > 2500.f && W->GetActiveWeapon() != nullptr);

	if (Now < ReactionReadyTime || Now < BurstPauseUntil || W->IsReloading()) { return; }
	const FRotator Ctrl = GetControlRotation();
	const FRotator ToTarget = (Desired - Me->GetEyeLocation()).Rotation();
	const float Off = FMath::Abs(FRotator::NormalizeAxis(Ctrl.Yaw - ToTarget.Yaw)) + FMath::Abs(FRotator::NormalizeAxis(Ctrl.Pitch - ToTarget.Pitch));
	const float Tolerance = FMath::Clamp(400.f / FMath::Max(Dist, 100.f), 0.6f, 6.f);
	if (Off > Tolerance) { return; }

	FNSCWeaponStats S;
	if (!W->GetActiveStats(S)) { return; }
	W->BotFireOnce(Ctrl);
	++BurstShots;
	// burst control: full auto up close, bursts at range, taps with pistols/snipers
	const int32 MaxBurst = !S.bAutomatic ? 1 : (Dist < 800.f ? 12 : (Dist < 1800.f ? 5 : 3));
	if (BurstShots >= MaxBurst)
	{
		BurstShots = 0;
		BurstPauseUntil = Now + (S.bAutomatic ? FMath::Lerp(0.45f, 0.22f, Skill()) : FMath::Max(S.FireInterval, FMath::Lerp(0.5f, 0.2f, Skill())));
	}
}

void ANSCAIController::DownedBehaviour(ANSCCharacter* Me)
{
	// crawl toward the closest standing teammate
	ANSCCharacter* Best = nullptr;
	float BestD = MAX_flt;
	for (TActorIterator<ANSCCharacter> It(GetWorld()); It; ++It)
	{
		if (*It == Me || It->GetTeam() != Me->GetTeam() || !It->GetHealth()->IsAliveAndStanding()) { continue; }
		const float D = FVector::Dist(It->GetActorLocation(), Me->GetActorLocation());
		if (D < BestD) { BestD = D; Best = *It; }
	}
	if (Best && BestD > 150.f && !Me->IsBeingDragged())
	{
		MoveTowards(Me, Best->GetActorLocation(), 120.f, false);
	}
}

// ------------------------------------------------------------------------------------- objectives

void ANSCAIController::ChooseGoal(ANSCCharacter* Me, float Now)
{
	NextGoalTime = Now + 0.5f;
	const ANSCGameState* GS = GetWorld()->GetGameState<ANSCGameState>();
	const ANSCGameMode* GM = GetWorld()->GetAuthGameMode<ANSCGameMode>();
	ANSCBomb* Bomb = GM ? GM->GetBomb() : nullptr;
	const ENSCRoundPhase Phase = GS->GetPhase();
	const bool bAtk = Me->GetTeam() == ENSCTeam::Attackers;

	// 1) revive: medkit + downed teammate near + quiet
	if (Me->GetWeapons()->Medkits > 0 && Now - LastSawEnemyTime > 1.5f)
	{
		if (ANSCCharacter* Downed = FindDownedTeammate(Me, 2500.f))
		{
			Goal = ENSCBotGoal::Revive;
			GoalLocation = Downed->GetActorLocation();
			return;
		}
	}

	// 2) investigate the last place we saw an enemy
	if (Now - LastSawEnemyTime < 4.f && Now - LastSawEnemyTime > 0.4f)
	{
		Goal = ENSCBotGoal::Investigate;
		GoalLocation = LastKnownEnemy;
		return;
	}

	if (Phase == ENSCRoundPhase::BombPlanted && Bomb)
	{
		if (bAtk)
		{
			if (Goal != ENSCBotGoal::GuardCharge) { Goal = ENSCBotGoal::GuardCharge; GoalLocation = RandomReachable(Bomb->GetActorLocation(), 900.f); }
		}
		else
		{
			Goal = ENSCBotGoal::Defuse;
			GoalLocation = Bomb->GetActorLocation();
		}
		return;
	}

	if (bAtk && Bomb)
	{
		if (Me->HasBomb())
		{
			if (ANSCBombSite* Site = FindSite(GM->GetAttackSite()))
			{
				if (Goal != ENSCBotGoal::PlantCharge) { GoalLocation = RandomReachable(Site->GetActorLocation(), 250.f); }
				Goal = ENSCBotGoal::PlantCharge;
			}
			return;
		}
		if (Bomb->GetState() == ENSCBombState::Dropped)
		{
			// the closest attacker fetches it
			float MyD = FVector::Dist(Bomb->GetActorLocation(), Me->GetActorLocation());
			bool bClosest = true;
			for (TActorIterator<ANSCCharacter> It(GetWorld()); It; ++It)
			{
				if (*It != Me && It->GetTeam() == ENSCTeam::Attackers && It->GetHealth()->IsAliveAndStanding() && FVector::Dist(Bomb->GetActorLocation(), It->GetActorLocation()) < MyD) { bClosest = false; break; }
			}
			if (bClosest) { Goal = ENSCBotGoal::PickUpCharge; GoalLocation = Bomb->GetActorLocation(); return; }
		}
		if (Goal != ENSCBotGoal::MoveToSite)
		{
			if (ANSCBombSite* Site = FindSite(GM->GetAttackSite()))
			{
				Goal = ENSCBotGoal::MoveToSite;
				GoalLocation = RandomReachable(Site->GetActorLocation(), 700.f);
			}
		}
		return;
	}

	if (!bAtk && Goal != ENSCBotGoal::Hold)
	{
		// spread defenders across both sites
		const ANSCPlayerState* PS = GetPlayerState<ANSCPlayerState>();
		const int32 Slot = PS ? (PS->GetPlayerId() % 5) : FMath::RandRange(0, 4);
		const FName SiteName = Slot < 2 ? FName(TEXT("A")) : (Slot < 4 ? FName(TEXT("B")) : (FMath::RandBool() ? FName(TEXT("A")) : FName(TEXT("B"))));
		if (ANSCBombSite* Site = FindSite(SiteName))
		{
			Goal = ENSCBotGoal::Hold;
			const FVector SiteLoc = Site->GetActorLocation();
			// hold toward the attackers' approach
			GoalLocation = RandomReachable(SiteLoc + (FVector::ZeroVector - SiteLoc).GetSafeNormal2D() * (Slot == 4 ? 1400.f : 450.f), 500.f);
			LookAtWhenIdle = FVector(0.f, SiteLoc.Y * 0.4f, SiteLoc.Z);
		}
	}
}

void ANSCAIController::PursueGoal(ANSCCharacter* Me, float DeltaSeconds, float Now)
{
	if (Now < StartDelayUntil) { return; }
	const bool bAtk = Me->GetTeam() == ENSCTeam::Attackers;
	switch (Goal)
	{
	case ENSCBotGoal::Revive:
		if (ANSCCharacter* Downed = FindDownedTeammate(Me, 2600.f))
		{
			if (FVector::Dist2D(Downed->GetActorLocation(), Me->GetActorLocation()) < NSCRules::InteractRange - 30.f)
			{
				FaceTowards(Downed->GetActorLocation(), DeltaSeconds, 6.f);
				Me->ServerStartInteract_Internal();
			}
			else
			{
				MoveTowards(Me, Downed->GetActorLocation(), NSCRules::InteractRange - 40.f, true);
				FaceTowards(Me->GetActorLocation() + Me->GetVelocity() * 2.f + FVector(0.f, 0.f, 60.f), DeltaSeconds, 5.f);
			}
		}
		else { Goal = ENSCBotGoal::Idle; }
		return;
	case ENSCBotGoal::Defuse:
		if (FVector::Dist(GoalLocation, Me->GetActorLocation()) < 110.f && Now - LastSawEnemyTime > 0.8f)
		{
			Me->ServerStartInteract_Internal();
		}
		else
		{
			MoveTowards(Me, GoalLocation, 80.f, true);
		}
		break;
	case ENSCBotGoal::PlantCharge:
		if (ANSCBombSite::FindSiteAt(GetWorld(), Me->GetActorLocation()) && FVector::Dist2D(GoalLocation, Me->GetActorLocation()) < 300.f)
		{
			Me->ServerStartInteract_Internal();
		}
		else
		{
			MoveTowards(Me, GoalLocation, 100.f, true);
		}
		break;
	case ENSCBotGoal::MoveToSite:
	case ENSCBotGoal::GuardCharge:
	case ENSCBotGoal::Hold:
	case ENSCBotGoal::Investigate:
	case ENSCBotGoal::PickUpCharge:
	case ENSCBotGoal::Retake:
	{
		const bool bArrived = FVector::Dist2D(GoalLocation, Me->GetActorLocation()) < 120.f;
		if (!bArrived)
		{
			MoveTowards(Me, GoalLocation, 100.f, Goal != ENSCBotGoal::Investigate);
			Me->SetWantsWalk(Goal == ENSCBotGoal::Investigate || (Goal == ENSCBotGoal::MoveToSite && FVector::Dist2D(GoalLocation, Me->GetActorLocation()) < 1500.f && Skill() > 0.5f));
		}
		else
		{
			Me->SetWantsWalk(false);
			if (Goal == ENSCBotGoal::Investigate) { Goal = ENSCBotGoal::Idle; }
		}
		// attackers smoke the site once on the approach
		if (bAtk && Goal == ENSCBotGoal::MoveToSite && !bThrewSmoke && Me->GetWeapons()->Smokes > 0 && FVector::Dist2D(GoalLocation, Me->GetActorLocation()) < 2200.f)
		{
			bThrewSmoke = true;
			Me->GetWeapons()->SelectGrenade(ENSCGrenadeType::Smoke);
			bPendingThrow = true;
			PendingThrowTarget = GoalLocation + FVector(0.f, 0.f, 600.f);
			PendingThrowGiveUp = Now + 2.f;
		}
		break;
	}
	default:
		break;
	}

	// look where we are going, or where enemies come from when holding
	if (Goal == ENSCBotGoal::Hold && FVector::Dist2D(GoalLocation, Me->GetActorLocation()) < 200.f)
	{
		FaceTowards(LookAtWhenIdle + FVector(0.f, 0.f, 60.f), DeltaSeconds, 3.f);
	}
	else if (Me->GetVelocity().SizeSquared2D() > 100.f)
	{
		FaceTowards(Me->GetEyeLocation() + Me->GetVelocity().GetSafeNormal2D() * 500.f, DeltaSeconds, 6.f);
	}
}
