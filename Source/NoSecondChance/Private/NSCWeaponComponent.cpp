#include "NSCWeaponComponent.h"
#include "NoSecondChance.h"
#include "NSCCharacter.h"
#include "NSCHealthComponent.h"
#include "NSCGrenade.h"
#include "NSCPlayerState.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"
#include "Components/CapsuleComponent.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"
#include "EngineUtils.h"

// ---------------------------------------------------------------- FNSCWeaponInstance
int32 FNSCWeaponInstance::SpareMags() const
{
	int32 N = 0;
	for (int32 i = 0; i < Mags.Num(); ++i) { if (i != CurrentMag && Mags[i] > 0) { ++N; } }
	return N;
}

int32 FNSCWeaponInstance::ReserveRounds() const
{
	int32 N = 0;
	for (int32 i = 0; i < Mags.Num(); ++i) { if (i != CurrentMag) { N += Mags[i]; } }
	return N;
}

void FNSCWeaponInstance::Init(const FNSCWeaponStats& S)
{
	Id = S.Id;
	Mags.Init(S.MagSize, FMath::Max(1, S.MagCount));
	CurrentMag = 0;
}

// ---------------------------------------------------------------- component
UNSCWeaponComponent::UNSCWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
	SpreadStream.Initialize(0x5EC0DC);
}

void UNSCWeaponComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UNSCWeaponComponent, Primary);
	DOREPLIFETIME(UNSCWeaponComponent, Secondary);
	DOREPLIFETIME(UNSCWeaponComponent, ActiveSlot);
	DOREPLIFETIME(UNSCWeaponComponent, SelectedGrenade);
	DOREPLIFETIME(UNSCWeaponComponent, bReloading);
	DOREPLIFETIME(UNSCWeaponComponent, bAiming);
	DOREPLIFETIME(UNSCWeaponComponent, Smokes);
	DOREPLIFETIME(UNSCWeaponComponent, Flashes);
	DOREPLIFETIME(UNSCWeaponComponent, Frags);
	DOREPLIFETIME(UNSCWeaponComponent, Bandages);
	DOREPLIFETIME(UNSCWeaponComponent, Tourniquets);
	DOREPLIFETIME(UNSCWeaponComponent, Medkits);
	DOREPLIFETIME(UNSCWeaponComponent, bDefuseKit);
}

void UNSCWeaponComponent::BeginPlay()
{
	Super::BeginPlay();
	SpreadStream.Initialize(static_cast<int32>(reinterpret_cast<UPTRINT>(this) & 0x7FFFFFFF));
}

ANSCCharacter* UNSCWeaponComponent::GetNSCOwner() const
{
	return Cast<ANSCCharacter>(GetOwner());
}

const FNSCWeaponInstance* UNSCWeaponComponent::GetActiveWeapon() const
{
	switch (ActiveSlot)
	{
	case ENSCWeaponSlot::Primary: return Primary.IsValid() ? &Primary : nullptr;
	case ENSCWeaponSlot::Secondary: return Secondary.IsValid() ? &Secondary : nullptr;
	default: return nullptr;
	}
}

FNSCWeaponInstance* UNSCWeaponComponent::GetActiveWeaponMutable()
{
	return const_cast<FNSCWeaponInstance*>(GetActiveWeapon());
}

bool UNSCWeaponComponent::GetActiveStats(FNSCWeaponStats& Out) const
{
	const FNSCWeaponInstance* W = GetActiveWeapon();
	return W && UNSCWeaponLibrary::GetWeaponStats(W->Id, Out);
}

FName UNSCWeaponComponent::GetActiveWeaponId() const
{
	if (ActiveSlot == ENSCWeaponSlot::Grenade)
	{
		switch (SelectedGrenade)
		{
		case ENSCGrenadeType::Smoke: return FName(TEXT("Smoke"));
		case ENSCGrenadeType::Flash: return FName(TEXT("Flash"));
		case ENSCGrenadeType::Frag: return FName(TEXT("Frag"));
		default: return NAME_None;
		}
	}
	const FNSCWeaponInstance* W = GetActiveWeapon();
	return W ? W->Id : NAME_None;
}

int32 UNSCWeaponComponent::GetGrenadeCount(ENSCGrenadeType T) const
{
	switch (T)
	{
	case ENSCGrenadeType::Smoke: return Smokes;
	case ENSCGrenadeType::Flash: return Flashes;
	case ENSCGrenadeType::Frag: return Frags;
	default: return 0;
	}
}

float UNSCWeaponComponent::GetReloadFraction() const
{
	if (!bReloading || ReloadDuration <= 0.f || !GetWorld()) { return 0.f; }
	return FMath::Clamp((GetWorld()->GetTimeSeconds() - ReloadStartTime) / ReloadDuration, 0.f, 1.f);
}

float UNSCWeaponComponent::GetMovementMultiplier() const
{
	if (ActiveSlot == ENSCWeaponSlot::Grenade) { return 1.f; }
	FNSCWeaponStats S;
	return GetActiveStats(S) ? S.MoveSpeedMultiplier : 1.f;
}

// ---------------------------------------------------------------- loadout
void UNSCWeaponComponent::GiveWeapon(FName WeaponId)
{
	FNSCWeaponStats S;
	if (!UNSCWeaponLibrary::GetWeaponStats(WeaponId, S)) { return; }
	if (S.Slot == ENSCWeaponSlot::Primary) { Primary.Init(S); ActiveSlot = ENSCWeaponSlot::Primary; }
	else { Secondary.Init(S); if (!Primary.IsValid()) { ActiveSlot = ENSCWeaponSlot::Secondary; } }
	bReloading = false;
	OnRep_Weapons();
}

void UNSCWeaponComponent::GiveDefaultLoadout(ENSCTeam Team)
{
	ClearAll();
	GiveWeapon(UNSCWeaponLibrary::DefaultPistol(Team));
	ActiveSlot = ENSCWeaponSlot::Secondary;
	OnRep_Weapons();
}

void UNSCWeaponComponent::ClearAll()
{
	Primary.Clear();
	Secondary.Clear();
	Smokes = Flashes = Frags = Bandages = Tourniquets = Medkits = 0;
	bDefuseKit = false;
	bReloading = false;
	SelectedGrenade = ENSCGrenadeType::None;
	ActiveSlot = ENSCWeaponSlot::Secondary;
}

void UNSCWeaponComponent::ApplyLoadout(const FNSCLoadout& L)
{
	Primary = L.Primary;
	Secondary = L.Secondary;
	Smokes = L.Smokes; Flashes = L.Flashes; Frags = L.Frags;
	Bandages = L.Bandages; Tourniquets = L.Tourniquets; Medkits = L.Medkits;
	bDefuseKit = L.bDefuseKit;
	ActiveSlot = Primary.IsValid() ? ENSCWeaponSlot::Primary : ENSCWeaponSlot::Secondary;
	if (ANSCCharacter* C = GetNSCOwner())
	{
		C->GetHealth()->SetArmor(L.Armor, L.bHelmet);
	}
	OnRep_Weapons();
}

FNSCLoadout UNSCWeaponComponent::MakeLoadout() const
{
	FNSCLoadout L;
	L.Primary = Primary;
	L.Secondary = Secondary;
	L.Smokes = Smokes; L.Flashes = Flashes; L.Frags = Frags;
	L.Bandages = Bandages; L.Tourniquets = Tourniquets; L.Medkits = Medkits;
	L.bDefuseKit = bDefuseKit;
	if (const ANSCCharacter* C = GetNSCOwner())
	{
		L.Armor = C->GetHealth()->GetArmor();
		L.bHelmet = C->GetHealth()->HasHelmet();
	}
	L.bValid = true;
	return L;
}

void UNSCWeaponComponent::RefillAmmo()
{
	FNSCWeaponStats S;
	if (Primary.IsValid() && UNSCWeaponLibrary::GetWeaponStats(Primary.Id, S)) { const int32 Cur = Primary.Rounds(); Primary.Init(S); Primary.Mags[0] = FMath::Max(Cur, Primary.Mags[0]); }
	if (Secondary.IsValid() && UNSCWeaponLibrary::GetWeaponStats(Secondary.Id, S)) { const int32 Cur = Secondary.Rounds(); Secondary.Init(S); Secondary.Mags[0] = FMath::Max(Cur, Secondary.Mags[0]); }
	OnRep_Weapons();
}

// ---------------------------------------------------------------- selection
void UNSCWeaponComponent::SelectSlot(ENSCWeaponSlot Slot)
{
	if (Slot == ENSCWeaponSlot::Primary && !Primary.IsValid()) { return; }
	if (Slot == ENSCWeaponSlot::Secondary && !Secondary.IsValid()) { return; }
	if (Slot == ActiveSlot && Slot != ENSCWeaponSlot::Grenade) { return; }
	if (Slot == ENSCWeaponSlot::Grenade && SelectedGrenade == ENSCGrenadeType::None)
	{
		if (Flashes > 0) { SelectedGrenade = ENSCGrenadeType::Flash; }
		else if (Smokes > 0) { SelectedGrenade = ENSCGrenadeType::Smoke; }
		else if (Frags > 0) { SelectedGrenade = ENSCGrenadeType::Frag; }
		else { return; }
	}
	ActiveSlot = Slot;
	bWantsFire = false;
	if (GetOwner()->HasAuthority()) { bReloading = false; GetWorld()->GetTimerManager().ClearTimer(ReloadTimer); }
	FNSCWeaponStats S;
	DrawEndTime = GetWorld()->GetTimeSeconds() + (GetActiveStats(S) ? S.DrawTime : 0.4f);
	OnRep_Weapons();
	if (!GetOwner()->HasAuthority()) { ServerSelectSlot(Slot, SelectedGrenade); }
}

void UNSCWeaponComponent::SelectGrenade(ENSCGrenadeType Type)
{
	if (GetGrenadeCount(Type) <= 0) { return; }
	SelectedGrenade = Type;
	ActiveSlot = ENSCWeaponSlot::Secondary; // force a change so SelectSlot runs
	SelectSlot(ENSCWeaponSlot::Grenade);
}

void UNSCWeaponComponent::CycleWeapon(int32 Direction)
{
	TArray<ENSCWeaponSlot> Order;
	if (Primary.IsValid()) { Order.Add(ENSCWeaponSlot::Primary); }
	if (Secondary.IsValid()) { Order.Add(ENSCWeaponSlot::Secondary); }
	if (Smokes + Flashes + Frags > 0) { Order.Add(ENSCWeaponSlot::Grenade); }
	if (Order.Num() < 2) { return; }
	const int32 Cur = FMath::Max(0, Order.IndexOfByKey(ActiveSlot));
	const int32 Next = (Cur + (Direction >= 0 ? 1 : Order.Num() - 1)) % Order.Num();
	SelectSlot(Order[Next]);
}

void UNSCWeaponComponent::ServerSelectSlot_Implementation(ENSCWeaponSlot Slot, ENSCGrenadeType Grenade)
{
	if (Slot == ENSCWeaponSlot::Grenade && GetGrenadeCount(Grenade) > 0) { SelectedGrenade = Grenade; ActiveSlot = ENSCWeaponSlot::Secondary; }
	SelectSlot(Slot);
}

void UNSCWeaponComponent::SetAiming(bool bAim)
{
	bAiming = bAim;
	if (!GetOwner()->HasAuthority()) { ServerSetAiming(bAim); }
}

void UNSCWeaponComponent::ServerSetAiming_Implementation(bool bAim)
{
	bAiming = bAim;
}

// ---------------------------------------------------------------- firing
bool UNSCWeaponComponent::CanFire() const
{
	const ANSCCharacter* C = GetNSCOwner();
	if (!C || !C->CanUseWeapons()) { return false; }
	if (bReloading) { return false; }
	if (GetWorld()->GetTimeSeconds() < DrawEndTime) { return false; }
	if (ActiveSlot == ENSCWeaponSlot::Grenade) { return GetGrenadeCount(SelectedGrenade) > 0; }
	const FNSCWeaponInstance* W = GetActiveWeapon();
	return W && W->Rounds() > 0;
}

void UNSCWeaponComponent::StartFire()
{
	bWantsFire = true;
	bFiredSinceTrigger = false;
	const FNSCWeaponInstance* W = GetActiveWeapon();
	if (W && W->Rounds() <= 0 && ActiveSlot != ENSCWeaponSlot::Grenade) { Reload(); }
}

void UNSCWeaponComponent::StopFire()
{
	bWantsFire = false;
}

float UNSCWeaponComponent::GetCurrentSpread() const
{
	FNSCWeaponStats S;
	const ANSCCharacter* C = GetNSCOwner();
	if (!C || !GetActiveStats(S)) { return 0.01f; }
	const UCharacterMovementComponent* Move = C->GetCharacterMovement();
	const float MaxRun = 500.f;
	const float Speed = C->GetVelocity().Size2D();
	const float SpeedFrac = FMath::Clamp((Speed - 0.34f * MaxRun) / (0.66f * MaxRun), 0.f, 1.f);
	float Spread = S.BaseSpread + SpeedFrac * S.MoveSpread;
	if (Move && Move->IsFalling()) { Spread += S.JumpSpread; }
	if (C->bIsCrouched) { Spread *= 0.75f; }
	if (S.bScoped)
	{
		if (!bAiming) { Spread = FMath::Max(Spread, S.UnscopedSpread); }
	}
	else if (bAiming)
	{
		Spread *= 0.7f;
	}
	// Spray adds bloom on top of the pattern after the first few shots
	Spread += FMath::Clamp(SprayHeat - 3.f, 0.f, 10.f) * S.BaseSpread * 0.6f;
	Spread *= C->GetHealth()->GetHandlingMultiplier();
	return Spread;
}

FRotator UNSCWeaponComponent::ApplySprayToAim(const FRotator& Aim, const FNSCWeaponStats& S, int32 ShotIndex) const
{
	if (S.SprayPattern.Num() == 0) { return Aim; }
	const FVector2D P = S.SprayPattern[FMath::Clamp(ShotIndex, 0, S.SprayPattern.Num() - 1)];
	const float Mult = 1.f; // arm injury already widens spread; the pattern itself stays learnable
	FRotator R = Aim;
	R.Pitch += P.Y * Mult;
	R.Yaw += P.X * Mult;
	return R;
}

void UNSCWeaponComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const UWorld* World = GetWorld();
	if (!World) { return; }
	const float Now = World->GetTimeSeconds();

	// Spray recovery (both server and owning client)
	FNSCWeaponStats S;
	const bool bHasStats = GetActiveStats(S);
	const float Recovery = bHasStats ? S.SprayRecovery : 10.f;
	if (Now - LastShotTime > (bHasStats ? S.FireInterval * 1.6f : 0.15f))
	{
		SprayHeat = FMath::Max(0.f, SprayHeat - Recovery * DeltaTime);
	}
	// Visual camera kick follows the spray and recovers
	if (bHasStats && S.SprayPattern.Num() > 0)
	{
		const FVector2D P = S.SprayPattern[FMath::Clamp(FMath::FloorToInt(SprayHeat), 0, S.SprayPattern.Num() - 1)];
		const FVector2D Target = SprayHeat > 0.01f ? FVector2D(P.Y, P.X) * S.ViewKick : FVector2D::ZeroVector;
		ViewKick = FMath::Vector2DInterpTo(ViewKick, Target, DeltaTime, 18.f);
	}
	else
	{
		ViewKick = FMath::Vector2DInterpTo(ViewKick, FVector2D::ZeroVector, DeltaTime, 12.f);
	}

	// Trigger handling: only the controlling machine runs it (owning client, or server for its local player).
	const ANSCCharacter* C = GetNSCOwner();
	if (!C || !C->IsLocallyControlled() || C->IsBotControlled()) { return; }
	if (bWantsFire && Now >= NextFireTime && CanFire())
	{
		const bool bAuto = ActiveSlot != ENSCWeaponSlot::Grenade && bHasStats && S.bAutomatic;
		if (bAuto || !bFiredSinceTrigger)
		{
			FireOnceLocal();
			bFiredSinceTrigger = true;
		}
	}
}

void UNSCWeaponComponent::FireOnceLocal()
{
	ANSCCharacter* C = GetNSCOwner();
	if (!C) { return; }
	const float Now = GetWorld()->GetTimeSeconds();
	const FVector Origin = C->GetEyeLocation();
	const FRotator Aim = C->GetControlRotation();

	if (ActiveSlot == ENSCWeaponSlot::Grenade)
	{
		NextFireTime = Now + 1.0f;
		bWantsFire = false;
		const bool bLob = bAiming;
		if (GetOwner()->HasAuthority()) { ThrowGrenadeAuthority(Origin, Aim, bLob); }
		else { ServerThrowGrenade(Origin, Aim, bLob); }
		C->PlayThrowAnimation();
		return;
	}

	FNSCWeaponStats S;
	if (!GetActiveStats(S)) { return; }
	NextFireTime = Now + S.FireInterval;

	if (GetOwner()->HasAuthority())
	{
		FireAuthority(Origin, Aim, Now);
	}
	else
	{
		// Predict locally: ammo, spray, cosmetics. The server's copy is authoritative and replicates back.
		if (FNSCWeaponInstance* W = GetActiveWeaponMutable()) { if (W->Mags.IsValidIndex(W->CurrentMag)) { W->Mags[W->CurrentMag] = FMath::Max(0, W->Mags[W->CurrentMag] - 1); } }
		const float Spread = GetCurrentSpread();
		const FRotator Shot = ApplySprayToAim(Aim, S, FMath::FloorToInt(SprayHeat));
		const FVector Dir = SpreadStream.VRandCone(Shot.Vector(), Spread);
		FHitResult Hit;
		if (TraceShot(Origin, Dir, S.MaxRange, Hit))
		{
			SpawnImpactFX(Hit.ImpactPoint, Hit.ImpactNormal, Cast<ANSCCharacter>(Hit.GetActor()) != nullptr);
		}
		SprayHeat += 1.f;
		LastShotTime = Now;
		PlayFireCosmetics(S);
		ServerFire(Origin, Aim.Vector(), Now);
	}
}

void UNSCWeaponComponent::ServerFire_Implementation(FVector_NetQuantize Origin, FVector_NetQuantizeNormal Direction, float ClientTime)
{
	ANSCCharacter* C = GetNSCOwner();
	if (!C) { return; }
	FNSCWeaponStats S;
	if (!GetActiveStats(S)) { return; }
	const float Now = GetWorld()->GetTimeSeconds();
	// Rate limit with a little tolerance for network jitter.
	if (Now < NextFireTime - S.FireInterval * 0.35f) { return; }
	// Origin must be close to where the server thinks the eyes are.
	const FVector ServerEye = C->GetEyeLocation();
	FVector UseOrigin = Origin;
	if (FVector::DistSquared(Origin, ServerEye) > FMath::Square(120.f)) { UseOrigin = ServerEye; }
	// Aim must be roughly where the server-side control rotation points.
	FVector Dir = FVector(Direction).GetSafeNormal();
	const FVector ServerDir = C->GetControlRotation().Vector();
	if (Dir.IsNearlyZero() || FVector::DotProduct(Dir, ServerDir) < FMath::Cos(FMath::DegreesToRadians(20.f))) { Dir = ServerDir; }
	NextFireTime = Now + S.FireInterval;
	FireAuthority(UseOrigin, Dir.Rotation(), ClientTime);
}

void UNSCWeaponComponent::BotFireOnce(const FRotator& AimRotation)
{
	ANSCCharacter* C = GetNSCOwner();
	if (!C || !GetOwner()->HasAuthority()) { return; }
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now < NextFireTime || !CanFire()) { return; }
	if (ActiveSlot == ENSCWeaponSlot::Grenade)
	{
		NextFireTime = Now + 1.f;
		ThrowGrenadeAuthority(C->GetEyeLocation(), AimRotation, false);
		return;
	}
	FNSCWeaponStats S;
	if (!GetActiveStats(S)) { return; }
	NextFireTime = Now + S.FireInterval;
	FireAuthority(C->GetEyeLocation(), AimRotation, Now);
}

bool UNSCWeaponComponent::TraceShot(const FVector& Start, const FVector& Dir, float Range, FHitResult& OutHit) const
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(NSCWeaponTrace), true, GetOwner());
	Params.bReturnPhysicalMaterial = true;
	return GetWorld()->LineTraceSingleByChannel(OutHit, Start, Start + Dir * Range, NSC_TRACE_WEAPON, Params);
}

ANSCCharacter* UNSCWeaponComponent::RewindTest(const FVector& Start, const FVector& Dir, float Range, float RewindSeconds, FVector& OutPoint, float& OutZ01, float& OutSide) const
{
	const float Now = GetWorld()->GetTimeSeconds();
	ANSCCharacter* Best = nullptr;
	float BestDist = Range;
	for (TActorIterator<ANSCCharacter> It(GetWorld()); It; ++It)
	{
		ANSCCharacter* Other = *It;
		if (!Other || Other == GetOwner() || Other->GetHealth()->IsDead()) { continue; }
		const FVector Past = Other->GetLocationAtTime(Now - RewindSeconds);
		const float HalfHeight = Other->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		const float Radius = Other->GetCapsuleComponent()->GetScaledCapsuleRadius() * 0.8f;
		// closest approach between the ray and the capsule's vertical axis
		const FVector Bottom = Past - FVector(0, 0, HalfHeight - Radius);
		const FVector Top = Past + FVector(0, 0, HalfHeight - Radius);
		FVector OnRay, OnAxis;
		FMath::SegmentDistToSegmentSafe(Start, Start + Dir * Range, Bottom, Top, OnRay, OnAxis);
		if (FVector::Dist(OnRay, OnAxis) > Radius) { continue; }
		const float D = FVector::Dist(Start, OnRay);
		if (D >= BestDist) { continue; }
		// make sure the world (not other pawns) does not block the line to that point
		FHitResult Block;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(NSCRewind), false, GetOwner());
		Params.AddIgnoredActor(Other);
		if (GetWorld()->LineTraceSingleByChannel(Block, Start, OnRay, ECC_Visibility, Params)) { continue; }
		BestDist = D;
		Best = Other;
		OutPoint = OnRay;
		OutZ01 = FMath::Clamp((OnRay.Z - (Past.Z - HalfHeight)) / (2.f * HalfHeight), 0.f, 1.f);
		const FVector Right = FVector::CrossProduct(FVector::UpVector, Dir).GetSafeNormal();
		OutSide = FMath::Clamp(FVector::DotProduct(OnRay - Past, Right) / FMath::Max(Radius, 1.f), -1.f, 1.f);
	}
	return Best;
}

void UNSCWeaponComponent::FireAuthority(const FVector& Origin, const FRotator& AimRotation, float ClientTime)
{
	ANSCCharacter* C = GetNSCOwner();
	FNSCWeaponInstance* W = GetActiveWeaponMutable();
	FNSCWeaponStats S;
	if (!C || !W || !GetActiveStats(S) || !C->CanUseWeapons() || bReloading) { return; }
	if (W->Rounds() <= 0) { return; }

	W->Mags[W->CurrentMag] -= 1;
	const int32 ShotIndex = FMath::FloorToInt(SprayHeat);
	const float Spread = GetCurrentSpread();
	SprayHeat += 1.f;
	LastShotTime = GetWorld()->GetTimeSeconds();
	++ServerShotCounter;

	if (ANSCPlayerState* PS = C->GetPlayerState<ANSCPlayerState>()) { PS->ShotsFired++; }

	// Remote client latency for the lag-compensation rewind (capped at 200 ms).
	float Rewind = 0.f;
	if (!C->IsLocallyControlled() && C->GetPlayerState())
	{
		Rewind = FMath::Clamp(C->GetPlayerState()->GetPingInMilliseconds() * 0.001f * 0.5f + 0.05f, 0.f, 0.2f);
	}

	const FRotator Shot = ApplySprayToAim(AimRotation, S, ShotIndex);
	FVector FirstEnd = Origin + Shot.Vector() * S.MaxRange;
	FVector FirstNormal = -Shot.Vector();
	bool bFirstHitPawn = false;
	bool bFirstHasImpact = false;

	for (int32 Pellet = 0; Pellet < FMath::Max(1, S.Pellets); ++Pellet)
	{
		FVector Dir = SpreadStream.VRandCone(Shot.Vector(), Spread);
		if (S.Pellets > 1) { Dir = SpreadStream.VRandCone(Dir, S.PelletSpread); }

		FHitResult Hit;
		const bool bHit = TraceShot(Origin, Dir, S.MaxRange, Hit);
		ANSCCharacter* Victim = bHit ? Cast<ANSCCharacter>(Hit.GetActor()) : nullptr;
		FVector ImpactPoint = bHit ? FVector(Hit.ImpactPoint) : Origin + Dir * S.MaxRange;
		ENSCHitZone Zone = ENSCHitZone::None;

		if (Victim)
		{
			Zone = UNSCHealthComponent::ZoneFromBone(Hit.BoneName);
			if (Zone == ENSCHitZone::None)
			{
				const float HalfHeight = Victim->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
				const float Z01 = FMath::Clamp((Hit.ImpactPoint.Z - (Victim->GetActorLocation().Z - HalfHeight)) / (2.f * HalfHeight), 0.f, 1.f);
				const FVector Right = FVector::CrossProduct(FVector::UpVector, Dir).GetSafeNormal();
				const float Side = FVector::DotProduct(FVector(Hit.ImpactPoint) - Victim->GetActorLocation(), Right) / 30.f;
				Zone = UNSCHealthComponent::ZoneFromHeight(Z01, Side);
			}
		}
		else if (Rewind > 0.f)
		{
			// Lag compensation: the client saw the target where it was ~half a round trip ago.
			FVector P; float Z01 = 0.5f, Side = 0.f;
			const float WorldDist = bHit ? FVector::Dist(Origin, Hit.ImpactPoint) : S.MaxRange;
			if (ANSCCharacter* Past = RewindTest(Origin, Dir, WorldDist, Rewind, P, Z01, Side))
			{
				Victim = Past;
				ImpactPoint = P;
				Zone = UNSCHealthComponent::ZoneFromHeight(Z01, Side);
			}
		}

		if (Victim && !Victim->GetHealth()->IsDead())
		{
			const float Dist = FVector::Dist(Origin, ImpactPoint);
			FNSCHitInfo Info;
			Info.BaseDamage = S.Damage * FMath::Pow(S.RangeModifier, Dist / 1000.f);
			Info.ArmorPen = S.ArmorPen;
			Info.HeadMultiplier = S.HeadMultiplier;
			Info.Zone = Zone == ENSCHitZone::None ? ENSCHitZone::Chest : Zone;
			Info.WeaponId = S.Id;
			Info.HitLocation = ImpactPoint;
			Info.HitDirection = Dir;
			Info.Instigator = C->GetController();
			Victim->GetHealth()->ApplyHit(Info);
			if (ANSCPlayerState* PS = C->GetPlayerState<ANSCPlayerState>()) { PS->ShotsHit++; if (Info.Zone == ENSCHitZone::Head) { PS->Headshots++; } }
		}

		if (Pellet == 0)
		{
			FirstEnd = ImpactPoint;
			FirstNormal = bHit ? FVector(Hit.ImpactNormal) : -Dir;
			bFirstHitPawn = Victim != nullptr;
			bFirstHasImpact = bHit || Victim != nullptr;
		}
	}

	// Cosmetics for everyone else (the owning client already played its own).
	MulticastShotFX(FirstEnd, FirstNormal, bFirstHitPawn, bFirstHasImpact);
	if (C->IsLocallyControlled() && !C->IsBotControlled()) { PlayFireCosmetics(S); }
	C->MakeNoise(1.f, C, C->GetActorLocation(), 4000.f);
	OnRep_Weapons();
}

void UNSCWeaponComponent::MulticastShotFX_Implementation(FVector_NetQuantize End, FVector_NetQuantizeNormal Normal, bool bHitPawn, bool bHasImpact)
{
	ANSCCharacter* C = GetNSCOwner();
	if (!C) { return; }
	// The owning human player already played these locally.
	if (C->IsLocallyControlled() && !C->IsBotControlled()) { return; }
	FNSCWeaponStats S;
	if (GetActiveStats(S))
	{
		if (USoundBase* Snd = S.FireSound.LoadSynchronous()) { UGameplayStatics::PlaySoundAtLocation(this, Snd, C->GetActorLocation()); }
	}
	C->PlayMuzzleFlash();
	if (bHasImpact) { SpawnImpactFX(End, Normal, bHitPawn); }
}

void UNSCWeaponComponent::PlayFireCosmetics(const FNSCWeaponStats& S)
{
	ANSCCharacter* C = GetNSCOwner();
	if (!C) { return; }
	if (USoundBase* Snd = S.FireSound.LoadSynchronous()) { UGameplayStatics::PlaySound2D(this, Snd); }
	C->PlayMuzzleFlash();
	C->AddWeaponKick(S);
}

void UNSCWeaponComponent::SpawnImpactFX(const FVector& Point, const FVector& Normal, bool bHitPawn) const
{
	static const TCHAR* BulletHole = TEXT("/Game/NSC/Materials/MD_BulletHole.MD_BulletHole");
	static const TCHAR* Blood = TEXT("/Game/NSC/Materials/MD_Blood.MD_Blood");
	if (UMaterialInterface* Decal = LoadObject<UMaterialInterface>(nullptr, bHitPawn ? Blood : BulletHole, nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		const FRotator Rot = (-Normal).Rotation();
		const float Size = bHitPawn ? 22.f : 5.f;
		UGameplayStatics::SpawnDecalAtLocation(GetWorld(), Decal, FVector(4.f, Size, Size), Point, Rot, bHitPawn ? 12.f : 20.f);
	}
	static const TCHAR* ImpactSound = TEXT("/Game/NSC/Audio/Impacts/SW_Impact_Concrete.SW_Impact_Concrete");
	static const TCHAR* FleshSound = TEXT("/Game/NSC/Audio/Impacts/SW_Impact_Flesh.SW_Impact_Flesh");
	if (USoundBase* Snd = LoadObject<USoundBase>(nullptr, bHitPawn ? FleshSound : ImpactSound, nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		UGameplayStatics::PlaySoundAtLocation(GetWorld(), Snd, Point, 0.6f);
	}
	if (const ANSCCharacter* C = GetNSCOwner())
	{
		C->SpawnImpactParticles(Point, Normal, bHitPawn);
	}
}

// ---------------------------------------------------------------- reload
void UNSCWeaponComponent::Reload()
{
	const FNSCWeaponInstance* W = GetActiveWeapon();
	FNSCWeaponStats S;
	if (!W || bReloading || !GetActiveStats(S)) { return; }
	if (W->Rounds() >= S.MagSize || W->SpareMags() == 0) { return; }
	if (GetOwner()->HasAuthority()) { ServerReload_Implementation(); }
	else { ServerReload(); }
}

void UNSCWeaponComponent::ServerReload_Implementation()
{
	ANSCCharacter* C = GetNSCOwner();
	const FNSCWeaponInstance* W = GetActiveWeapon();
	FNSCWeaponStats S;
	if (!C || !W || bReloading || !GetActiveStats(S) || !C->CanUseWeapons()) { return; }
	if (W->Rounds() >= S.MagSize || W->SpareMags() == 0) { return; }
	bReloading = true;
	bWantsFire = false;
	ReloadStartTime = GetWorld()->GetTimeSeconds();
	ReloadDuration = (W->Rounds() > 0 ? S.ReloadTime : S.EmptyReloadTime) * C->GetHealth()->GetReloadMultiplier();
	GetWorld()->GetTimerManager().SetTimer(ReloadTimer, this, &UNSCWeaponComponent::FinishReload, ReloadDuration, false);
	OnRep_Reloading();
}

void UNSCWeaponComponent::FinishReload()
{
	bReloading = false;
	FNSCWeaponInstance* W = GetActiveWeaponMutable();
	if (W)
	{
		// Magazine memory: swap to the fullest other magazine. A partial magazine goes back in the pouch,
		// an empty one is dropped.
		int32 Best = INDEX_NONE;
		for (int32 i = 0; i < W->Mags.Num(); ++i)
		{
			if (i == W->CurrentMag || W->Mags[i] <= 0) { continue; }
			if (Best == INDEX_NONE || W->Mags[i] > W->Mags[Best]) { Best = i; }
		}
		if (Best != INDEX_NONE)
		{
			const int32 Old = W->CurrentMag;
			W->CurrentMag = Best;
			if (W->Mags[Old] <= 0)
			{
				W->Mags.RemoveAt(Old);
				if (W->CurrentMag > Old) { --W->CurrentMag; }
			}
		}
	}
	OnRep_Reloading();
	OnRep_Weapons();
}

// ---------------------------------------------------------------- grenades
void UNSCWeaponComponent::ServerThrowGrenade_Implementation(FVector_NetQuantize Origin, FRotator Aim, bool bLob)
{
	ANSCCharacter* C = GetNSCOwner();
	if (!C) { return; }
	const FVector ServerEye = C->GetEyeLocation();
	const FVector UseOrigin = FVector::DistSquared(Origin, ServerEye) > FMath::Square(120.f) ? ServerEye : FVector(Origin);
	ThrowGrenadeAuthority(UseOrigin, Aim, bLob);
}

void UNSCWeaponComponent::ThrowGrenadeAuthority(const FVector& Origin, const FRotator& Aim, bool bLob)
{
	ANSCCharacter* C = GetNSCOwner();
	if (!C || !C->CanUseWeapons() || ActiveSlot != ENSCWeaponSlot::Grenade) { return; }
	int32* Count = nullptr;
	switch (SelectedGrenade)
	{
	case ENSCGrenadeType::Smoke: Count = &Smokes; break;
	case ENSCGrenadeType::Flash: Count = &Flashes; break;
	case ENSCGrenadeType::Frag: Count = &Frags; break;
	default: return;
	}
	if (*Count <= 0) { return; }
	--(*Count);

	FActorSpawnParameters P;
	P.Owner = C;
	P.Instigator = C;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector Fwd = Aim.Vector();
	const FVector Start = Origin + Fwd * 40.f;
	if (ANSCGrenade* G = GetWorld()->SpawnActor<ANSCGrenade>(ANSCGrenade::StaticClass(), Start, Aim, P))
	{
		const float Speed = bLob ? 700.f : 1500.f;
		G->Launch(SelectedGrenade, Fwd * Speed + FVector(0, 0, bLob ? 250.f : 150.f) + C->GetVelocity() * 0.5f, C->GetController());
	}
	// Out of this grenade type: pick another or fall back to a gun.
	if (*Count <= 0)
	{
		if (Flashes > 0) { SelectedGrenade = ENSCGrenadeType::Flash; }
		else if (Smokes > 0) { SelectedGrenade = ENSCGrenadeType::Smoke; }
		else if (Frags > 0) { SelectedGrenade = ENSCGrenadeType::Frag; }
		else { SelectedGrenade = ENSCGrenadeType::None; ActiveSlot = Primary.IsValid() ? ENSCWeaponSlot::Primary : ENSCWeaponSlot::Secondary; }
	}
	OnRep_Weapons();
}

// ---------------------------------------------------------------- reps
void UNSCWeaponComponent::OnRep_Weapons()
{
	if (ANSCCharacter* C = GetNSCOwner()) { C->RefreshWeaponVisuals(); }
}

void UNSCWeaponComponent::OnRep_Reloading()
{
	if (bReloading)
	{
		ReloadStartTime = GetWorld()->GetTimeSeconds();
		FNSCWeaponStats S;
		if (GetActiveStats(S) && ReloadDuration <= 0.f) { ReloadDuration = S.ReloadTime; }
		if (ANSCCharacter* C = GetNSCOwner())
		{
			if (GetActiveStats(S)) { if (USoundBase* Snd = S.ReloadSound.LoadSynchronous()) { UGameplayStatics::PlaySoundAtLocation(this, Snd, C->GetActorLocation()); } }
			C->PlayReloadAnimation();
		}
	}
}
