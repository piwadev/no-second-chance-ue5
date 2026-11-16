#include "NSCBomb.h"
#include "NSCCharacter.h"
#include "NSCHealthComponent.h"
#include "NSCGameState.h"
#include "NoSecondChance.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

// ---------------------------------------------------------------- site
ANSCBombSite::ANSCBombSite()
{
	Volume = CreateDefaultSubobject<UBoxComponent>(TEXT("Volume"));
	Volume->SetBoxExtent(FVector(700.f, 800.f, 300.f));
	Volume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Volume->ShapeColor = FColor(213, 166, 74);
	RootComponent = Volume;
	bReplicates = false;
}

bool ANSCBombSite::Contains(const FVector& Location) const
{
	const FVector Local = GetActorTransform().InverseTransformPosition(Location);
	const FVector E = Volume->GetUnscaledBoxExtent();
	return FMath::Abs(Local.X) <= E.X && FMath::Abs(Local.Y) <= E.Y && FMath::Abs(Local.Z) <= E.Z;
}

ANSCBombSite* ANSCBombSite::FindSiteAt(const UWorld* World, const FVector& Location)
{
	if (!World) { return nullptr; }
	for (TActorIterator<ANSCBombSite> It(const_cast<UWorld*>(World)); It; ++It)
	{
		if (It->Contains(Location)) { return *It; }
	}
	return nullptr;
}

// ---------------------------------------------------------------- bomb
ANSCBomb::ANSCBomb()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);
	bAlwaysRelevant = true;

	Pickup = CreateDefaultSubobject<USphereComponent>(TEXT("Pickup"));
	Pickup->InitSphereRadius(70.f);
	Pickup->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	Pickup->SetGenerateOverlapEvents(true);
	RootComponent = Pickup;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Pickup);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded()) { Mesh->SetStaticMesh(Cube.Object); }
	Mesh->SetRelativeScale3D(FVector(0.3f, 0.2f, 0.12f));
	Mesh->SetRelativeLocation(FVector(0, 0, -60.f));

	Blinker = CreateDefaultSubobject<UPointLightComponent>(TEXT("Blinker"));
	Blinker->SetupAttachment(Mesh);
	Blinker->SetIntensity(0.f);
	Blinker->SetLightColor(FLinearColor(1.f, 0.1f, 0.05f));
	Blinker->SetAttenuationRadius(250.f);
	Blinker->SetCastShadows(false);
}

void ANSCBomb::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ANSCBomb, State);
	DOREPLIFETIME(ANSCBomb, Carrier);
	DOREPLIFETIME(ANSCBomb, PlantedSite);
}

void ANSCBomb::BeginPlay()
{
	Super::BeginPlay();
	if (UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/NSC/Weapons/SM_Charge.SM_Charge"), nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		Mesh->SetStaticMesh(M);
		Mesh->SetRelativeScale3D(FVector(1.f));
	}
	if (HasAuthority()) { Pickup->OnComponentBeginOverlap.AddDynamic(this, &ANSCBomb::OnPickupOverlap); }
	UpdateVisibility();
}

void ANSCBomb::OnPickupOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (State != ENSCBombState::Dropped) { return; }
	ANSCCharacter* C = Cast<ANSCCharacter>(OtherActor);
	if (C && C->GetTeam() == ENSCTeam::Attackers && C->GetHealth()->IsAliveAndStanding()) { GiveTo(C); }
}

void ANSCBomb::GiveTo(ANSCCharacter* NewCarrier)
{
	if (!NewCarrier) { return; }
	if (Carrier && Carrier != NewCarrier) { Carrier->SetHasBomb(false); }
	Carrier = NewCarrier;
	State = ENSCBombState::Carried;
	Carrier->SetHasBomb(true);
	AttachToActor(Carrier, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	UpdateVisibility();
}

void ANSCBomb::Drop(const FVector& Location)
{
	if (Carrier) { Carrier->SetHasBomb(false); }
	Carrier = nullptr;
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	// put it on the floor below the drop point
	FHitResult Hit;
	FVector Where = Location;
	FCollisionQueryParams P(SCENE_QUERY_STAT(NSCBombDrop), false, this);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Location, Location - FVector(0, 0, 500.f), ECC_Visibility, P)) { Where = Hit.ImpactPoint + FVector(0, 0, 60.f); }
	SetActorLocation(Where);
	SetActorRotation(FRotator::ZeroRotator);
	State = ENSCBombState::Dropped;
	UpdateVisibility();
}

void ANSCBomb::Plant(ANSCCharacter* Planter, FName Site)
{
	FVector Where = Planter ? Planter->GetActorLocation() : GetActorLocation();
	if (Planter) { Planter->SetHasBomb(false); }
	Carrier = nullptr;
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	FHitResult Hit;
	FCollisionQueryParams P(SCENE_QUERY_STAT(NSCBombPlant), false, this);
	P.AddIgnoredActor(Planter);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Where, Where - FVector(0, 0, 500.f), ECC_Visibility, P)) { Where = Hit.ImpactPoint + FVector(0, 0, 60.f); }
	SetActorLocation(Where);
	SetActorRotation(FRotator(0.f, Planter ? Planter->GetActorRotation().Yaw : 0.f, 0.f));
	PlantedSite = Site;
	State = ENSCBombState::Planted;
	LocalPlantTime = GetWorld()->GetTimeSeconds();
	UpdateVisibility();
}

void ANSCBomb::SetDefused()
{
	State = ENSCBombState::Defused;
	UpdateVisibility();
}

void ANSCBomb::Explode()
{
	State = ENSCBombState::Exploded;
	MulticastExplode();
	// Everyone close to the charge dies, falloff further out.
	const FVector Center = GetActorLocation();
	for (TActorIterator<ANSCCharacter> It(GetWorld()); It; ++It)
	{
		ANSCCharacter* C = *It;
		if (!C || C->GetHealth()->IsDead()) { continue; }
		const float D = FVector::Dist(C->GetActorLocation(), Center);
		if (D > 2200.f) { continue; }
		FNSCHitInfo Hit;
		Hit.BaseDamage = FMath::Lerp(500.f, 20.f, D / 2200.f);
		Hit.ArmorPen = 0.9f;
		Hit.bExplosive = true;
		Hit.Zone = ENSCHitZone::Chest;
		Hit.WeaponId = FName(TEXT("Charge"));
		C->GetHealth()->ApplyHit(Hit);
	}
	UpdateVisibility();
}

void ANSCBomb::MulticastExplode_Implementation()
{
	if (USoundBase* S = LoadObject<USoundBase>(nullptr, TEXT("/Game/NSC/Audio/Grenades/SW_Explosion.SW_Explosion"), nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		UGameplayStatics::PlaySoundAtLocation(this, S, GetActorLocation(), 2.f);
	}
	Blinker->SetIntensity(800000.f);
	Blinker->SetAttenuationRadius(5000.f);
	Blinker->SetLightColor(FLinearColor(1.f, 0.6f, 0.25f));
}

void ANSCBomb::OnRep_State()
{
	if (State == ENSCBombState::Planted) { LocalPlantTime = GetWorld()->GetTimeSeconds(); }
	UpdateVisibility();
}

void ANSCBomb::UpdateVisibility()
{
	const bool bVisible = State == ENSCBombState::Dropped || State == ENSCBombState::Planted || State == ENSCBombState::Defused;
	Mesh->SetVisibility(bVisible, true);
	if (State != ENSCBombState::Planted && State != ENSCBombState::Exploded) { Blinker->SetIntensity(0.f); }
}

void ANSCBomb::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (State != ENSCBombState::Planted) { return; }
	const float Now = GetWorld()->GetTimeSeconds();
	// beeps speed up as the timer runs out
	float Remaining = 40.f;
	if (const ANSCGameState* GS = GetWorld()->GetGameState<ANSCGameState>()) { Remaining = GS->GetBombTimeRemaining(); }
	const float Interval = FMath::GetMappedRangeValueClamped(FVector2D(0.f, 40.f), FVector2D(0.12f, 1.f), Remaining);
	if (Now >= NextBeep)
	{
		NextBeep = Now + Interval;
		Blinker->SetIntensity(3000.f);
		if (GetNetMode() != NM_DedicatedServer)
		{
			if (USoundBase* S = LoadObject<USoundBase>(nullptr, TEXT("/Game/NSC/Audio/UI/SW_BombBeep.SW_BombBeep"), nullptr, LOAD_NoWarn | LOAD_Quiet))
			{
				UGameplayStatics::PlaySoundAtLocation(this, S, GetActorLocation(), 0.7f);
			}
		}
	}
	else if (Now > NextBeep - Interval + 0.06f)
	{
		Blinker->SetIntensity(0.f);
	}
}
