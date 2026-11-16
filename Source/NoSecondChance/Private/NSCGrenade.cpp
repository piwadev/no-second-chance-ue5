#include "NSCGrenade.h"
#include "NoSecondChance.h"
#include "NSCCharacter.h"
#include "NSCHealthComponent.h"
#include "NSCPlayerController.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

ANSCGrenade::ANSCGrenade()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(5.f);
	Collision->SetCollisionProfileName(TEXT("Projectile"));
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Collision->SetCollisionResponseToChannel(NSC_TRACE_WEAPON, ECR_Ignore);
	RootComponent = Collision;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Collision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetRelativeScale3D(FVector(0.08f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Sphere.Succeeded()) { Mesh->SetStaticMesh(Sphere.Object); }

	FlashLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FlashLight"));
	FlashLight->SetupAttachment(Collision);
	FlashLight->SetIntensity(0.f);
	FlashLight->SetAttenuationRadius(1500.f);
	FlashLight->SetCastShadows(false);

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->UpdatedComponent = Collision;
	Movement->bShouldBounce = true;
	Movement->Bounciness = 0.35f;
	Movement->Friction = 0.4f;
	Movement->ProjectileGravityScale = 1.f;
	Movement->bAutoActivate = false;
}

void ANSCGrenade::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ANSCGrenade, Type);
	DOREPLIFETIME(ANSCGrenade, bDetonated);
}

void ANSCGrenade::BeginPlay()
{
	Super::BeginPlay();
	UpdateMesh();
}

void ANSCGrenade::OnRep_Type()
{
	UpdateMesh();
}

void ANSCGrenade::UpdateMesh()
{
	// Real grenade meshes (imported by Tools/Setup.bat) replace the placeholder sphere.
	const TCHAR* Path = Type == ENSCGrenadeType::Smoke ? TEXT("/Game/NSC/Weapons/SM_Grenade_Smoke.SM_Grenade_Smoke")
		: Type == ENSCGrenadeType::Flash ? TEXT("/Game/NSC/Weapons/SM_Grenade_Flash.SM_Grenade_Flash") : TEXT("/Game/NSC/Weapons/SM_Grenade_Frag.SM_Grenade_Frag");
	if (UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		Mesh->SetStaticMesh(M);
		Mesh->SetRelativeScale3D(FVector(1.f));
	}
}

void ANSCGrenade::Launch(ENSCGrenadeType InType, const FVector& Velocity, AController* InInstigator)
{
	Type = InType;
	UpdateMesh();
	InstigatorController = InInstigator;
	Movement->Velocity = Velocity;
	Movement->Activate(true);
	const float Fuse = Type == ENSCGrenadeType::Smoke ? 1.8f : (Type == ENSCGrenadeType::Flash ? 1.5f : 2.2f);
	GetWorldTimerManager().SetTimer(FuseTimer, this, &ANSCGrenade::Detonate, Fuse, false);
}

void ANSCGrenade::Detonate()
{
	if (bDetonated) { return; }
	bDetonated = true;
	Movement->StopMovementImmediately();
	MulticastDetonateFX(Type, GetActorLocation());
	switch (Type)
	{
	case ENSCGrenadeType::Frag: DoFrag(); SetLifeSpan(2.f); break;
	case ENSCGrenadeType::Flash: DoFlash(); SetLifeSpan(2.f); break;
	case ENSCGrenadeType::Smoke: StartSmoke(); SetLifeSpan(SmokeDuration + 1.f); break;
	default: Destroy(); break;
	}
}

void ANSCGrenade::DoFrag()
{
	const FVector Center = GetActorLocation() + FVector(0, 0, 10.f);
	const float Radius = 450.f;
	for (TActorIterator<ANSCCharacter> It(GetWorld()); It; ++It)
	{
		ANSCCharacter* C = *It;
		if (!C || C->GetHealth()->IsDead()) { continue; }
		const FVector Target = C->GetActorLocation();
		const float Dist = FVector::Dist(Center, Target);
		if (Dist > Radius) { continue; }
		FHitResult Block;
		FCollisionQueryParams P(SCENE_QUERY_STAT(NSCFrag), false, this);
		P.AddIgnoredActor(C);
		if (GetWorld()->LineTraceSingleByChannel(Block, Center, Target, ECC_Visibility, P)) { continue; }
		FNSCHitInfo Hit;
		Hit.BaseDamage = FMath::Lerp(98.f, 8.f, Dist / Radius);
		Hit.ArmorPen = 0.6f;
		Hit.Zone = ENSCHitZone::Chest;
		Hit.bExplosive = true;
		Hit.WeaponId = FName(TEXT("Frag"));
		Hit.HitLocation = Target;
		Hit.HitDirection = (Target - Center).GetSafeNormal();
		Hit.Instigator = InstigatorController.Get();
		C->GetHealth()->ApplyHit(Hit);
	}
}

void ANSCGrenade::DoFlash()
{
	const FVector Center = GetActorLocation() + FVector(0, 0, 10.f);
	for (TActorIterator<ANSCCharacter> It(GetWorld()); It; ++It)
	{
		ANSCCharacter* C = *It;
		if (!C || !C->GetHealth()->IsAliveAndStanding()) { continue; }
		const FVector Eye = C->GetEyeLocation();
		const float Dist = FVector::Dist(Center, Eye);
		if (Dist > 2000.f) { continue; }
		FHitResult Block;
		FCollisionQueryParams P(SCENE_QUERY_STAT(NSCFlash), false, this);
		P.AddIgnoredActor(C);
		if (GetWorld()->LineTraceSingleByChannel(Block, Center, Eye, ECC_Visibility, P)) { continue; }
		const FVector ToFlash = (Center - Eye).GetSafeNormal();
		const float Facing = FVector::DotProduct(C->GetControlRotation().Vector(), ToFlash); // 1 = looking straight at it
		const float FacingMul = Facing > 0.5f ? 1.f : (Facing > -0.2f ? 0.55f : 0.2f);
		const float DistMul = FMath::Clamp(1.f - Dist / 2000.f, 0.f, 1.f);
		const float Duration = 4.5f * FacingMul * (0.35f + 0.65f * DistMul);
		if (Duration > 0.25f) { C->ApplyFlash(Duration); }
	}
}

void ANSCGrenade::StartSmoke()
{
	SmokeEndTime = GetWorld()->GetTimeSeconds() + SmokeDuration;
	BuildSmokeVisual();
}

void ANSCGrenade::OnRep_Detonated()
{
	if (bDetonated && Type == ENSCGrenadeType::Smoke)
	{
		SmokeEndTime = GetWorld()->GetTimeSeconds() + SmokeDuration;
		BuildSmokeVisual();
	}
}

void ANSCGrenade::BuildSmokeVisual()
{
	if (GetNetMode() == NM_DedicatedServer || SmokePuffs.Num() > 0) { return; }
	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	UMaterialInterface* SmokeMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/NSC/Materials/M_Smoke.M_Smoke"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Sphere) { return; }
	FRandomStream R(GetUniqueID());
	for (int32 i = 0; i < 14; ++i)
	{
		UStaticMeshComponent* Puff = NewObject<UStaticMeshComponent>(this);
		Puff->SetStaticMesh(Sphere);
		Puff->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Puff->SetCastShadow(false);
		Puff->SetupAttachment(RootComponent);
		Puff->SetUsingAbsoluteRotation(true);
		const FVector Offset(R.FRandRange(-1.f, 1.f) * 200.f, R.FRandRange(-1.f, 1.f) * 200.f, R.FRandRange(0.f, 1.f) * 180.f + 60.f);
		Puff->SetRelativeLocation(Offset);
		Puff->SetWorldScale3D(FVector(R.FRandRange(3.2f, 4.6f)));
		if (SmokeMat) { Puff->SetMaterial(0, SmokeMat); }
		Puff->RegisterComponent();
		SmokePuffs.Add(Puff);
	}
	Mesh->SetVisibility(false);
}

void ANSCGrenade::MulticastDetonateFX_Implementation(ENSCGrenadeType InType, FVector_NetQuantize Where)
{
	const TCHAR* SoundPath = InType == ENSCGrenadeType::Frag ? TEXT("/Game/NSC/Audio/Grenades/SW_Explosion.SW_Explosion")
		: InType == ENSCGrenadeType::Flash ? TEXT("/Game/NSC/Audio/Grenades/SW_Flashbang.SW_Flashbang") : TEXT("/Game/NSC/Audio/Grenades/SW_SmokeHiss.SW_SmokeHiss");
	if (USoundBase* S = LoadObject<USoundBase>(nullptr, SoundPath, nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		UGameplayStatics::PlaySoundAtLocation(this, S, Where);
	}
	if (InType != ENSCGrenadeType::Smoke)
	{
		FlashLight->SetIntensity(InType == ENSCGrenadeType::Flash ? 400000.f : 120000.f);
		FlashLight->SetLightColor(InType == ENSCGrenadeType::Flash ? FLinearColor::White : FLinearColor(1.f, 0.55f, 0.2f));
		FlashLightOff = GetWorld()->GetTimeSeconds() + 0.12f;
		Mesh->SetVisibility(false);
	}
}

void ANSCGrenade::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (FlashLightOff > 0.f && GetWorld()->GetTimeSeconds() > FlashLightOff)
	{
		FlashLight->SetIntensity(0.f);
		FlashLightOff = 0.f;
	}
	// Smoke puffs slowly drift and thin out near the end.
	if (SmokePuffs.Num() > 0 && SmokeEndTime > 0.f)
	{
		const float Left = SmokeEndTime - GetWorld()->GetTimeSeconds();
		const float Scale = FMath::Clamp(Left / 3.f, 0.f, 1.f);
		for (UStaticMeshComponent* P : SmokePuffs)
		{
			if (P) { P->SetVisibility(Left > 0.f); P->AddWorldRotation(FRotator(0.f, 4.f * DeltaSeconds, 0.f)); if (Scale < 1.f) { P->SetWorldScale3D(P->GetComponentScale() * FMath::Lerp(0.98f, 1.f, Scale)); } }
		}
	}
}

bool ANSCGrenade::IsLineBlockedBySmoke(const UWorld* World, const FVector& From, const FVector& To)
{
	if (!World) { return false; }
	const float Now = World->GetTimeSeconds();
	for (TActorIterator<ANSCGrenade> It(const_cast<UWorld*>(World)); It; ++It)
	{
		const ANSCGrenade* G = *It;
		if (!G || G->Type != ENSCGrenadeType::Smoke || !G->bDetonated || Now > G->SmokeEndTime) { continue; }
		const FVector C = G->GetActorLocation() + FVector(0, 0, 120.f);
		const FVector Closest = FMath::ClosestPointOnSegment(C, From, To);
		if (FVector::DistSquared(Closest, C) < FMath::Square(SmokeRadius)) { return true; }
	}
	return false;
}
