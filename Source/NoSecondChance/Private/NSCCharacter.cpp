#include "NSCCharacter.h"
#include "NoSecondChance.h"
#include "NSCHealthComponent.h"
#include "NSCWeaponComponent.h"
#include "NSCMovementComponent.h"
#include "NSCPlayerState.h"
#include "NSCPlayerController.h"
#include "NSCGameMode.h"
#include "NSCGameState.h"
#include "NSCGameInstance.h"
#include "NSCBomb.h"
#include "NSCInputSet.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/InputComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Particles/ParticleSystem.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Misc/EngineVersionComparison.h"

namespace NSCPaths
{
	static const TCHAR* SoldierMesh = TEXT("/Game/NSC/Characters/Soldier/SK_Soldier.SK_Soldier");
	static const TCHAR* ArmsMesh = TEXT("/Game/NSC/Characters/Arms/SK_Arms.SK_Arms");
	static const TCHAR* AnimFolder = TEXT("/Game/NSC/Characters/Anims/");
	static const TCHAR* HelmetMesh = TEXT("/Game/NSC/Characters/Gear/SM_Helmet.SM_Helmet");
}

ANSCCharacter::ANSCCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UNSCMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
#if UE_VERSION_OLDER_THAN(5, 5, 0)
	NetUpdateFrequency = 64.f;
#else
	SetNetUpdateFrequency(64.f);
#endif

	GetCapsuleComponent()->InitCapsuleSize(34.f, 88.f);
	GetCapsuleComponent()->SetCollisionResponseToChannel(NSC_TRACE_WEAPON, ECR_Ignore);
	BaseEyeHeight = 70.f;
	CrouchedEyeHeight = 40.f;

	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	CameraRoot = CreateDefaultSubobject<USceneComponent>(TEXT("CameraRoot"));
	CameraRoot->SetupAttachment(GetCapsuleComponent());
	CameraRoot->SetRelativeLocation(FVector(0.f, 0.f, BaseEyeHeight));

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(CameraRoot);
	FirstPersonCamera->bUsePawnControlRotation = false;
	FirstPersonCamera->SetFieldOfView(90.f);

	Mesh1P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh1P"));
	Mesh1P->SetupAttachment(FirstPersonCamera);
	Mesh1P->SetOnlyOwnerSee(true);
	Mesh1P->bCastDynamicShadow = false;
	Mesh1P->CastShadow = false;
	Mesh1P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh1P->SetRelativeLocation(FVector(-5.f, 0.f, -155.f));

	WeaponMesh1P = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh1P"));
	WeaponMesh1P->SetupAttachment(FirstPersonCamera);
	WeaponMesh1P->SetOnlyOwnerSee(true);
	WeaponMesh1P->SetCastShadow(false);
	WeaponMesh1P->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -88.f));
	GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	GetMesh()->SetCollisionProfileName(TEXT("CharacterMesh"));
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	GetMesh()->SetCollisionResponseToChannel(NSC_TRACE_WEAPON, ECR_Block);
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones; // hit boxes must follow the pose on the server

	WeaponMesh3P = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh3P"));
	WeaponMesh3P->SetupAttachment(GetMesh(), TEXT("hand_r"));
	WeaponMesh3P->SetOwnerNoSee(true);
	WeaponMesh3P->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	HelmetMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HelmetMesh"));
	HelmetMesh->SetupAttachment(GetMesh(), TEXT("Head"));
	HelmetMesh->SetOwnerNoSee(true);
	HelmetMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HelmetMesh->SetVisibility(false);

	MuzzleLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("MuzzleLight"));
	MuzzleLight->SetupAttachment(FirstPersonCamera);
	MuzzleLight->SetRelativeLocation(FVector(90.f, 12.f, -10.f));
	MuzzleLight->SetIntensity(0.f);
	MuzzleLight->SetLightColor(FLinearColor(1.f, 0.75f, 0.4f));
	MuzzleLight->SetAttenuationRadius(600.f);
	MuzzleLight->SetCastShadows(false);

	Health = CreateDefaultSubobject<UNSCHealthComponent>(TEXT("Health"));
	Weapons = CreateDefaultSubobject<UNSCWeaponComponent>(TEXT("Weapons"));
}

void ANSCCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ANSCCharacter, CurrentAction);
	DOREPLIFETIME(ANSCCharacter, ActionProgress);
	DOREPLIFETIME(ANSCCharacter, ActionTarget);
	DOREPLIFETIME(ANSCCharacter, DragTarget);
	DOREPLIFETIME(ANSCCharacter, DraggedBy);
	DOREPLIFETIME_CONDITION(ANSCCharacter, LeanInput, COND_SkipOwner);
	DOREPLIFETIME(ANSCCharacter, bHasBomb);
	DOREPLIFETIME(ANSCCharacter, bFrozen);
	DOREPLIFETIME(ANSCCharacter, ReplicatedTeam);
}

UNSCMovementComponent* ANSCCharacter::GetNSCMovement() const
{
	return Cast<UNSCMovementComponent>(GetCharacterMovement());
}

void ANSCCharacter::BeginPlay()
{
	Super::BeginPlay();
	LoadCharacterMeshes();
	RefreshWeaponVisuals();
	ApplyTeamLook();
}

void ANSCCharacter::LoadCharacterMeshes()
{
	if (USkeletalMesh* Body = LoadObject<USkeletalMesh>(nullptr, NSCPaths::SoldierMesh, nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		GetMesh()->SetSkeletalMesh(Body);
	}
	if (!GetMesh()->GetPhysicsAsset())
	{
		// No physics asset: hit the capsule instead and resolve the zone from hit height.
		GetCapsuleComponent()->SetCollisionResponseToChannel(NSC_TRACE_WEAPON, ECR_Block);
	}
	if (USkeletalMesh* Arms = LoadObject<USkeletalMesh>(nullptr, NSCPaths::ArmsMesh, nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		Mesh1P->SetSkeletalMesh(Arms);
	}
	if (UStaticMesh* Helmet = LoadObject<UStaticMesh>(nullptr, NSCPaths::HelmetMesh, nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		HelmetMesh->SetStaticMesh(Helmet);
	}
	PlayAnim(TEXT("A_Idle"), true);
}

void ANSCCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	if (const ANSCPlayerState* PS = GetPlayerState<ANSCPlayerState>()) { ReplicatedTeam = PS->GetTeam(); }
	ApplyTeamLook();
}

void ANSCCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	ApplyTeamLook();
}

void ANSCCharacter::OnRep_Team()
{
	ApplyTeamLook();
}

void ANSCCharacter::ApplyTeamLook()
{
	const ENSCTeam Team = GetTeam();
	if (Team == ENSCTeam::None || !GetMesh()) { return; }
	const FLinearColor Col = NSCColors::Team(Team);
	for (int32 i = 0; i < GetMesh()->GetNumMaterials(); ++i)
	{
		if (UMaterialInstanceDynamic* MID = GetMesh()->CreateAndSetMaterialInstanceDynamic(i))
		{
			MID->SetVectorParameterValue(TEXT("TeamColor"), Col);
			MID->SetScalarParameterValue(TEXT("TeamIndex"), Team == ENSCTeam::Attackers ? 0.f : 1.f);
		}
	}
}

ENSCTeam ANSCCharacter::GetTeam() const
{
	if (const ANSCPlayerState* PS = GetPlayerState<ANSCPlayerState>()) { return PS->GetTeam(); }
	return ReplicatedTeam;
}

FString ANSCCharacter::GetDisplayName() const
{
	if (const APlayerState* PS = GetPlayerState()) { return PS->GetPlayerName(); }
	return TEXT("Soldier");
}

FVector ANSCCharacter::GetEyeLocation() const
{
	return GetPawnViewLocation();
}

FVector ANSCCharacter::GetPawnViewLocation() const
{
	FVector Eye = GetActorLocation() + FVector(0.f, 0.f, BaseEyeHeight);
	if (bIsCrouched) { Eye.Z = GetActorLocation().Z + CrouchedEyeHeight; }
	if (Health && Health->IsDowned()) { Eye.Z = GetActorLocation().Z - 20.f; }
	// Lean shifts the eye sideways (server knows LeanInput, owner uses its smoothed value).
	const float L = IsLocallyControlled() ? Lean : static_cast<float>(LeanInput);
	if (FMath::Abs(L) > 0.01f)
	{
		Eye += GetActorRightVector() * (L * 22.f);
	}
	return Eye;
}

bool ANSCCharacter::CanUseWeapons() const
{
	if (!Health || !Health->IsAliveAndStanding()) { return false; }
	if (IsDragging() || IsBusyWithAction()) { return false; }
	if (const UNSCMovementComponent* M = GetNSCMovement()) { if (M->bWantsToSprint && GetVelocity().Size2D() > 300.f && CanSprint()) { return false; } }
	if (const ANSCGameState* GS = GetWorld() ? GetWorld()->GetGameState<ANSCGameState>() : nullptr)
	{
		const ENSCRoundPhase P = GS->GetPhase();
		if (P == ENSCRoundPhase::Buy || P == ENSCRoundPhase::HalfTime || P == ENSCRoundPhase::MatchEnd) { return false; }
		if (P == ENSCRoundPhase::Preparation && GetTeam() == ENSCTeam::Attackers) { return false; }
	}
	return true;
}

bool ANSCCharacter::CanSprint() const
{
	if (!Health || !Health->IsAliveAndStanding() || Health->HasLegInjury()) { return false; }
	if (bIsCrouched || IsDragging() || IsAimingWeapon()) { return false; }
	return true;
}

bool ANSCCharacter::IsFrozen() const
{
	return bFrozen;
}

bool ANSCCharacter::IsAimingWeapon() const
{
	return Weapons && Weapons->IsAiming();
}

float ANSCCharacter::GetFlashAmount() const
{
	if (!GetWorld()) { return 0.f; }
	const float Left = FlashEnd - GetWorld()->GetTimeSeconds();
	if (Left <= 0.f) { return 0.f; }
	// full white for the first part, then fade over the last 1.5 s
	return FMath::Clamp(Left / FMath::Min(1.5f, FlashDuration), 0.f, 1.f);
}

bool ANSCCharacter::IsFlashed() const
{
	return GetFlashAmount() > 0.5f;
}

FVector ANSCCharacter::GetLocationAtTime(float WorldTime) const
{
	if (PositionHistory.Num() == 0) { return GetActorLocation(); }
	for (int32 i = PositionHistory.Num() - 1; i > 0; --i)
	{
		const FNSCPositionSample& B = PositionHistory[i];
		const FNSCPositionSample& A = PositionHistory[i - 1];
		if (A.Time <= WorldTime && B.Time >= WorldTime)
		{
			const float T = (B.Time - A.Time) > KINDA_SMALL_NUMBER ? (WorldTime - A.Time) / (B.Time - A.Time) : 0.f;
			return FMath::Lerp(A.Location, B.Location, T);
		}
	}
	return WorldTime < PositionHistory[0].Time ? PositionHistory[0].Location : GetActorLocation();
}

// ---------------------------------------------------------------- input
void ANSCCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	ANSCPlayerController* PC = Cast<ANSCPlayerController>(GetController());
	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!PC || !EIC) { UE_LOG(LogNSC, Warning, TEXT("Enhanced Input not available - check DefaultInput.ini")); return; }
	const UNSCInputSet* In = PC->GetInputSet();
	if (!In) { return; }

	EIC->BindAction(In->Move, ETriggerEvent::Triggered, this, &ANSCCharacter::InputMove);
	EIC->BindAction(In->Look, ETriggerEvent::Triggered, this, &ANSCCharacter::InputLook);
	EIC->BindAction(In->Jump, ETriggerEvent::Started, this, &ANSCCharacter::InputJump);
	EIC->BindAction(In->Sprint, ETriggerEvent::Started, this, &ANSCCharacter::InputSprintStart);
	EIC->BindAction(In->Sprint, ETriggerEvent::Completed, this, &ANSCCharacter::InputSprintStop);
	EIC->BindAction(In->Walk, ETriggerEvent::Started, this, &ANSCCharacter::InputWalkStart);
	EIC->BindAction(In->Walk, ETriggerEvent::Completed, this, &ANSCCharacter::InputWalkStop);
	EIC->BindAction(In->Crouch, ETriggerEvent::Started, this, &ANSCCharacter::InputCrouchStart);
	EIC->BindAction(In->Crouch, ETriggerEvent::Completed, this, &ANSCCharacter::InputCrouchStop);
	EIC->BindAction(In->Fire, ETriggerEvent::Started, this, &ANSCCharacter::InputFireStart);
	EIC->BindAction(In->Fire, ETriggerEvent::Completed, this, &ANSCCharacter::InputFireStop);
	EIC->BindAction(In->Aim, ETriggerEvent::Started, this, &ANSCCharacter::InputAimStart);
	EIC->BindAction(In->Aim, ETriggerEvent::Completed, this, &ANSCCharacter::InputAimStop);
	EIC->BindAction(In->Reload, ETriggerEvent::Started, this, &ANSCCharacter::InputReload);
	EIC->BindAction(In->SlotPrimary, ETriggerEvent::Started, this, &ANSCCharacter::InputPrimary);
	EIC->BindAction(In->SlotSecondary, ETriggerEvent::Started, this, &ANSCCharacter::InputSecondary);
	EIC->BindAction(In->SlotSmoke, ETriggerEvent::Started, this, &ANSCCharacter::InputSmoke);
	EIC->BindAction(In->SlotFlash, ETriggerEvent::Started, this, &ANSCCharacter::InputFlash);
	EIC->BindAction(In->SlotFrag, ETriggerEvent::Started, this, &ANSCCharacter::InputFrag);
	EIC->BindAction(In->CycleWeapon, ETriggerEvent::Started, this, &ANSCCharacter::InputCycle);
	EIC->BindAction(In->LeanLeft, ETriggerEvent::Started, this, &ANSCCharacter::InputLeanLeftStart);
	EIC->BindAction(In->LeanLeft, ETriggerEvent::Completed, this, &ANSCCharacter::InputLeanLeftStop);
	EIC->BindAction(In->LeanRight, ETriggerEvent::Started, this, &ANSCCharacter::InputLeanRightStart);
	EIC->BindAction(In->LeanRight, ETriggerEvent::Completed, this, &ANSCCharacter::InputLeanRightStop);
	EIC->BindAction(In->Interact, ETriggerEvent::Started, this, &ANSCCharacter::InputInteractStart);
	EIC->BindAction(In->Interact, ETriggerEvent::Completed, this, &ANSCCharacter::InputInteractStop);
	EIC->BindAction(In->Drag, ETriggerEvent::Started, this, &ANSCCharacter::InputDragStart);
	EIC->BindAction(In->Drag, ETriggerEvent::Completed, this, &ANSCCharacter::InputDragStop);
	EIC->BindAction(In->Bandage, ETriggerEvent::Started, this, &ANSCCharacter::InputBandage);
	EIC->BindAction(In->Tourniquet, ETriggerEvent::Started, this, &ANSCCharacter::InputTourniquet);
}

static bool NSCUIBlocksInput(const ANSCCharacter* C)
{
	const ANSCPlayerController* PC = C ? Cast<ANSCPlayerController>(C->GetController()) : nullptr;
	return PC && PC->IsMenuOpen();
}

void ANSCCharacter::InputMove(const FInputActionValue& V)
{
	if (NSCUIBlocksInput(this)) { return; }
	const FVector2D Axis = V.Get<FVector2D>();
	const FRotator Yaw(0.f, GetControlRotation().Yaw, 0.f);
	AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::X), Axis.Y);
	AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), Axis.X);
}

void ANSCCharacter::InputLook(const FInputActionValue& V)
{
	if (NSCUIBlocksInput(this)) { return; }
	FVector2D Axis = V.Get<FVector2D>();
	float Sens = 1.f;
	if (const UNSCGameInstance* GI = GetGameInstance<UNSCGameInstance>()) { Sens = GI->GetMouseSensitivity(); }
	if (IsAimingWeapon()) { Sens *= CurrentFOV / 90.f; }
	LastLookInput = Axis;
	AddControllerYawInput(Axis.X * Sens * 0.5f);
	AddControllerPitchInput(-Axis.Y * Sens * 0.5f);
}

void ANSCCharacter::InputJump() { if (!NSCUIBlocksInput(this)) { Jump(); } }

void ANSCCharacter::InputSprintStart() { SetWantsSprint(true); }
void ANSCCharacter::InputSprintStop() { SetWantsSprint(false); }
void ANSCCharacter::InputWalkStart() { SetWantsWalk(true); }
void ANSCCharacter::InputWalkStop() { SetWantsWalk(false); }

void ANSCCharacter::SetWantsSprint(bool b)
{
	if (UNSCMovementComponent* M = GetNSCMovement()) { M->bWantsToSprint = b; if (b) { M->bWantsToWalk = false; } }
}

void ANSCCharacter::SetWantsWalk(bool b)
{
	if (UNSCMovementComponent* M = GetNSCMovement()) { M->bWantsToWalk = b; if (b) { M->bWantsToSprint = false; } }
}

void ANSCCharacter::ServerSetMoveFlags_Implementation(bool bSprint, bool bWalk)
{
	if (UNSCMovementComponent* M = GetNSCMovement()) { M->bWantsToSprint = bSprint; M->bWantsToWalk = bWalk; }
}

void ANSCCharacter::InputCrouchStart() { if (Health->IsAliveAndStanding()) { Crouch(); } }
void ANSCCharacter::InputCrouchStop() { if (Health->IsAliveAndStanding()) { UnCrouch(); } }

void ANSCCharacter::InputFireStart()
{
	if (NSCUIBlocksInput(this)) { return; }
	SetWantsSprint(false);
	Weapons->StartFire();
}
void ANSCCharacter::InputFireStop() { Weapons->StopFire(); }
void ANSCCharacter::InputAimStart() { if (!NSCUIBlocksInput(this)) { SetWantsSprint(false); Weapons->SetAiming(true); } }
void ANSCCharacter::InputAimStop() { Weapons->SetAiming(false); }
void ANSCCharacter::InputReload() { Weapons->Reload(); }
void ANSCCharacter::InputPrimary() { Weapons->SelectSlot(ENSCWeaponSlot::Primary); }
void ANSCCharacter::InputSecondary() { Weapons->SelectSlot(ENSCWeaponSlot::Secondary); }
void ANSCCharacter::InputSmoke() { Weapons->SelectGrenade(ENSCGrenadeType::Smoke); }
void ANSCCharacter::InputFlash() { Weapons->SelectGrenade(ENSCGrenadeType::Flash); }
void ANSCCharacter::InputFrag() { Weapons->SelectGrenade(ENSCGrenadeType::Frag); }
void ANSCCharacter::InputCycle(const FInputActionValue& V) { Weapons->CycleWeapon(V.Get<float>() >= 0.f ? 1 : -1); }

void ANSCCharacter::InputLeanLeftStart() { LeanInput = -1; ServerSetLean(-1); }
void ANSCCharacter::InputLeanLeftStop() { if (LeanInput < 0) { LeanInput = 0; ServerSetLean(0); } }
void ANSCCharacter::InputLeanRightStart() { LeanInput = 1; ServerSetLean(1); }
void ANSCCharacter::InputLeanRightStop() { if (LeanInput > 0) { LeanInput = 0; ServerSetLean(0); } }
void ANSCCharacter::ServerSetLean_Implementation(int8 NewLean) { LeanInput = FMath::Clamp<int8>(NewLean, -1, 1); }

void ANSCCharacter::InputInteractStart() { bWantsInteract = true; ServerStartInteract(); }
void ANSCCharacter::InputInteractStop() { bWantsInteract = false; ServerStopInteract(); }
void ANSCCharacter::InputDragStart() { ServerStartDrag(); }
void ANSCCharacter::InputDragStop() { ServerStopDrag(); }
void ANSCCharacter::InputBandage() { ServerUseBandage(false); }
void ANSCCharacter::InputTourniquet() { ServerUseBandage(true); }

void ANSCCharacter::ServerStartInteract_Implementation() { ServerStartInteract_Internal(); }
void ANSCCharacter::ServerStopInteract_Implementation() { ServerStopInteract_Internal(); }
void ANSCCharacter::ServerStartDrag_Implementation() { ServerStartDrag_Internal(); }
void ANSCCharacter::ServerStopDrag_Implementation() { StopDragging(); }
void ANSCCharacter::ServerUseBandage_Implementation(bool bTourniquet) { ServerUseBandage_Internal(bTourniquet); }

// ---------------------------------------------------------------- actions (server)
ANSCCharacter* ANSCCharacter::FindDownedTeammate(float Range) const
{
	ANSCCharacter* Best = nullptr;
	float BestD = Range;
	const FVector Eye = GetEyeLocation();
	const FVector Fwd = GetControlRotation().Vector();
	for (TActorIterator<ANSCCharacter> It(GetWorld()); It; ++It)
	{
		ANSCCharacter* O = *It;
		if (!O || O == this || O->GetTeam() != GetTeam() || !O->GetHealth()->IsDowned()) { continue; }
		const float D = FVector::Dist2D(GetActorLocation(), O->GetActorLocation());
		if (D > BestD) { continue; }
		// roughly in front of us (or very close)
		if (D > 80.f && FVector::DotProduct((O->GetActorLocation() - Eye).GetSafeNormal2D(), Fwd.GetSafeNormal2D()) < 0.3f) { continue; }
		Best = O;
		BestD = D;
	}
	return Best;
}

ANSCBomb* ANSCCharacter::FindBomb() const
{
	for (TActorIterator<ANSCBomb> It(GetWorld()); It; ++It) { return *It; }
	return nullptr;
}

bool ANSCCharacter::IsInBombSite() const
{
	return ANSCBombSite::FindSiteAt(GetWorld(), GetActorLocation()) != nullptr;
}

void ANSCCharacter::ServerStartInteract_Internal()
{
	if (!Health->IsAliveAndStanding() || IsDragging() || IsBusyWithAction()) { return; }
	const ANSCGameState* GS = GetWorld()->GetGameState<ANSCGameState>();
	const ENSCRoundPhase Phase = GS ? GS->GetPhase() : ENSCRoundPhase::Live;

	// 1) revive a downed teammate (needs a medical kit)
	if (ANSCCharacter* Downed = FindDownedTeammate(NSCRules::InteractRange))
	{
		if (Weapons->Medkits > 0)
		{
			CurrentAction = ENSCActionType::Reviving;
			ActionTarget = Downed;
			ActionProgress = 0.f;
			return;
		}
	}
	// 2) plant
	if (GetTeam() == ENSCTeam::Attackers && bHasBomb && Phase == ENSCRoundPhase::Live && IsInBombSite() && !GetCharacterMovement()->IsFalling())
	{
		CurrentAction = ENSCActionType::Planting;
		ActionProgress = 0.f;
		return;
	}
	// 3) defuse
	if (GetTeam() == ENSCTeam::Defenders && Phase == ENSCRoundPhase::BombPlanted)
	{
		if (ANSCBomb* Bomb = FindBomb())
		{
			if (Bomb->IsPlanted() && FVector::Dist(Bomb->GetActorLocation(), GetActorLocation()) < 150.f)
			{
				CurrentAction = ENSCActionType::Defusing;
				ActionProgress = 0.f;
			}
		}
	}
}

void ANSCCharacter::ServerStopInteract_Internal()
{
	if (CurrentAction == ENSCActionType::Reviving || CurrentAction == ENSCActionType::Planting || CurrentAction == ENSCActionType::Defusing)
	{
		CancelAction();
	}
}

void ANSCCharacter::ServerUseBandage_Internal(bool bTourniquet)
{
	if (!Health->IsAliveAndStanding() || IsBusyWithAction() || IsDragging()) { return; }
	if (bTourniquet)
	{
		if (Weapons->Tourniquets <= 0 || Health->GetBleed() != ENSCBleed::Heavy) { return; }
		CurrentAction = ENSCActionType::Tourniquet;
	}
	else
	{
		if (Weapons->Bandages <= 0 || Health->GetBleed() == ENSCBleed::None) { return; }
		CurrentAction = ENSCActionType::Bandaging;
	}
	ActionProgress = 0.f;
}

void ANSCCharacter::CancelAction()
{
	if (ActionTarget && CurrentAction == ENSCActionType::Reviving) { ActionTarget->GetHealth()->SetReviveProgress(0.f); }
	if (CurrentAction == ENSCActionType::Defusing)
	{
		if (ANSCGameState* GS = GetWorld()->GetGameState<ANSCGameState>()) { GS->SetDefuseProgress(0.f); }
	}
	CurrentAction = IsDragging() ? ENSCActionType::Dragging : ENSCActionType::None;
	ActionProgress = 0.f;
	ActionTarget = nullptr;
}

void ANSCCharacter::TickAction(float DeltaSeconds)
{
	if (!HasAuthority() || CurrentAction == ENSCActionType::None || CurrentAction == ENSCActionType::Dragging) { return; }
	if (!Health->IsAliveAndStanding()) { CancelAction(); return; }

	float Duration = 1.f;
	switch (CurrentAction)
	{
	case ENSCActionType::Reviving:
		if (!ActionTarget || !ActionTarget->GetHealth()->IsDowned() || FVector::Dist2D(ActionTarget->GetActorLocation(), GetActorLocation()) > NSCRules::InteractRange + 40.f || Weapons->Medkits <= 0) { CancelAction(); return; }
		Duration = NSCRules::ReviveTime;
		break;
	case ENSCActionType::Planting:
		if (!bHasBomb || !IsInBombSite()) { CancelAction(); return; }
		Duration = NSCRules::PlantTime;
		break;
	case ENSCActionType::Defusing:
	{
		const ANSCBomb* Bomb = FindBomb();
		if (!Bomb || !Bomb->IsPlanted() || FVector::Dist(Bomb->GetActorLocation(), GetActorLocation()) > 180.f) { CancelAction(); return; }
		Duration = Weapons->bDefuseKit ? NSCRules::DefuseTimeKit : NSCRules::DefuseTime;
		break;
	}
	case ENSCActionType::Bandaging: Duration = NSCRules::BandageTime; break;
	case ENSCActionType::Tourniquet: Duration = NSCRules::TourniquetTime; break;
	default: break;
	}

	ActionProgress = FMath::Min(1.f, ActionProgress + DeltaSeconds / Duration);
	if (CurrentAction == ENSCActionType::Reviving && ActionTarget) { ActionTarget->GetHealth()->SetReviveProgress(FMath::Max(ActionProgress, 0.01f)); }
	if (CurrentAction == ENSCActionType::Defusing) { if (ANSCGameState* GS = GetWorld()->GetGameState<ANSCGameState>()) { GS->SetDefuseProgress(ActionProgress); } }
	if (ActionProgress < 1.f) { return; }

	// completed
	ANSCGameMode* GM = GetWorld()->GetAuthGameMode<ANSCGameMode>();
	switch (CurrentAction)
	{
	case ENSCActionType::Reviving:
		Weapons->Medkits = FMath::Max(0, Weapons->Medkits - 1);
		ActionTarget->GetHealth()->Revive(GetController());
		break;
	case ENSCActionType::Planting:
		if (GM) { if (ANSCBombSite* Site = ANSCBombSite::FindSiteAt(GetWorld(), GetActorLocation())) { GM->OnBombPlanted(this, Site->SiteName); } }
		break;
	case ENSCActionType::Defusing:
		if (GM) { GM->OnBombDefused(this); }
		break;
	case ENSCActionType::Bandaging:
		if (Health->ApplyBandage()) { Weapons->Bandages = FMath::Max(0, Weapons->Bandages - 1); }
		break;
	case ENSCActionType::Tourniquet:
		if (Health->ApplyTourniquet()) { Weapons->Tourniquets = FMath::Max(0, Weapons->Tourniquets - 1); }
		break;
	default: break;
	}
	CurrentAction = ENSCActionType::None;
	ActionProgress = 0.f;
	ActionTarget = nullptr;
}

// ---------------------------------------------------------------- dragging (server)
void ANSCCharacter::ServerStartDrag_Internal()
{
	if (!Health->IsAliveAndStanding() || IsDragging() || IsBusyWithAction()) { return; }
	ANSCCharacter* Downed = FindDownedTeammate(NSCRules::InteractRange);
	if (!Downed || Downed->IsBeingDragged()) { return; }
	DragTarget = Downed;
	Downed->DraggedBy = this;
	Downed->LastDraggedBy = GetController();
	CurrentAction = ENSCActionType::Dragging;
	Weapons->StopFire();
	if (bIsCrouched) { UnCrouch(); }
	Downed->GetCharacterMovement()->DisableMovement();
	Downed->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Downed->OnRep_DraggedBy();
}

void ANSCCharacter::StopDragging()
{
	if (!DragTarget) { return; }
	ANSCCharacter* T = DragTarget;
	DragTarget = nullptr;
	if (CurrentAction == ENSCActionType::Dragging) { CurrentAction = ENSCActionType::None; }
	if (T)
	{
		T->DraggedBy = nullptr;
		T->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		if (!T->GetHealth()->IsDead()) { T->GetCharacterMovement()->SetMovementMode(MOVE_Walking); }
		T->OnRep_DraggedBy();
	}
}

void ANSCCharacter::OnRep_DraggedBy()
{
	if (DraggedBy) { GetCharacterMovement()->DisableMovement(); }
	else if (Health && !Health->IsDead()) { GetCharacterMovement()->SetMovementMode(MOVE_Walking); }
}

void ANSCCharacter::TickDrag(float DeltaSeconds)
{
	if (!HasAuthority() || !DragTarget) { return; }
	if (!Health->IsAliveAndStanding() || !DragTarget->GetHealth()->IsDowned())
	{
		StopDragging();
		return;
	}
	// The downed teammate is pulled along behind us (we walk backwards slowly while dragging).
	const FVector Behind = GetActorLocation() - GetActorForwardVector() * 95.f;
	FVector Target = FVector(Behind.X, Behind.Y, DragTarget->GetActorLocation().Z);
	const FVector Smoothed = FMath::VInterpTo(DragTarget->GetActorLocation(), Target, DeltaSeconds, 10.f);
	DragTarget->SetActorLocation(Smoothed, true);
	DragTarget->SetActorRotation(FRotator(0.f, GetActorRotation().Yaw + 180.f, 0.f));
}

// ---------------------------------------------------------------- state changes (all machines)
void ANSCCharacter::OnInjuryChanged()
{
	// HUD reads the health component directly; nothing else to do yet.
}

void ANSCCharacter::OnDownedStateChanged(bool bNowDowned)
{
	if (bNowDowned)
	{
		if (HasAuthority())
		{
			CancelAction();
			StopDragging();
			if (bHasBomb) { if (ANSCBomb* B = FindBomb()) { B->Drop(GetActorLocation()); } }
		}
		Weapons->StopFire();
		Crouch();
		PlayAnim(TEXT("A_Downed"), true);
	}
	else
	{
		if (HasAuthority())
		{
			for (TActorIterator<ANSCCharacter> It(GetWorld()); It; ++It) { if (It->GetDragTarget() == this) { It->StopDragging(); } }
		}
		UnCrouch();
		CurrentAnimName = NAME_None;
	}
}

void ANSCCharacter::OnDeath()
{
	if (HasAuthority())
	{
		CancelAction();
		StopDragging();
		for (TActorIterator<ANSCCharacter> It(GetWorld()); It; ++It) { if (It->GetDragTarget() == this) { It->StopDragging(); } }
		if (bHasBomb) { if (ANSCBomb* B = FindBomb()) { B->Drop(GetActorLocation()); } }
	}
	Weapons->StopFire();
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh1P->SetVisibility(false);
	Mesh1P->SetVisibility(false);
	// Ragdoll if the mesh has a physics asset, otherwise play the death animation.
	if (GetMesh()->GetPhysicsAsset() && GetNetMode() != NM_DedicatedServer)
	{
		GetMesh()->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
		GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
		GetMesh()->SetAllBodiesSimulatePhysics(true);
		GetMesh()->WakeAllRigidBodies();
		GetMesh()->SetOwnerNoSee(false);
	}
	else
	{
		PlayAnim(TEXT("A_Death"), false);
		GetMesh()->SetOwnerNoSee(false);
	}
	WeaponMesh3P->SetVisibility(false);
	SetLifeSpan(0.f);
}

void ANSCCharacter::SetHasBomb(bool bNew)
{
	bHasBomb = bNew;
}

void ANSCCharacter::ApplyFlash(float Duration)
{
	if (IsBotControlled())
	{
		BotFlashedUntil = GetWorld()->GetTimeSeconds() + Duration;
	}
	ClientFlashed(Duration);
}

void ANSCCharacter::ClientFlashed_Implementation(float Duration)
{
	FlashDuration = Duration;
	FlashEnd = GetWorld()->GetTimeSeconds() + Duration;
}

// ---------------------------------------------------------------- weapon presentation
void ANSCCharacter::RefreshWeaponVisuals()
{
	if (!Weapons) { return; }
	const FName Id = Weapons->GetActiveWeaponId();
	VisualWeaponId = Id;
	FNSCWeaponStats S;
	UStaticMesh* GunMesh = nullptr;
	if (Weapons->GetActiveSlot() == ENSCWeaponSlot::Grenade)
	{
		const FString Path = FString::Printf(TEXT("/Game/NSC/Weapons/SM_Grenade_%s.SM_Grenade_%s"), *Id.ToString(), *Id.ToString());
		GunMesh = LoadObject<UStaticMesh>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!GunMesh) { GunMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")); }
		WeaponMesh1P->SetRelativeLocation(FVector(40.f, 18.f, -22.f));
		WeaponMesh1P->SetRelativeRotation(FRotator::ZeroRotator);
		WeaponMesh1P->SetRelativeScale3D(GunMesh && GunMesh->GetPathName().StartsWith(TEXT("/Engine")) ? FVector(0.08f) : FVector(1.f));
	}
	else if (UNSCWeaponLibrary::GetWeaponStats(Id, S))
	{
		GunMesh = S.Mesh.LoadSynchronous();
		const bool bPlaceholder = GunMesh == nullptr;
		if (bPlaceholder) { GunMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")); }
		WeaponMesh1P->SetRelativeLocation(S.MeshOffset1P);
		WeaponMesh1P->SetRelativeRotation(S.MeshRotation);
		const float Len = S.Slot == ENSCWeaponSlot::Primary ? 0.8f : 0.25f;
		WeaponMesh1P->SetRelativeScale3D(bPlaceholder ? FVector(Len, 0.05f, 0.1f) : FVector(S.MeshScale));
	}
	WeaponMesh1P->SetStaticMesh(GunMesh);
	WeaponMesh3P->SetStaticMesh(GunMesh);
	WeaponMesh3P->SetRelativeScale3D(WeaponMesh1P->GetRelativeScale3D());
	const bool bAlive = Health && !Health->IsDead();
	WeaponMesh1P->SetVisibility(bAlive && GunMesh != nullptr);
	WeaponMesh3P->SetVisibility(bAlive && GunMesh != nullptr);
	if (Health) { HelmetMesh->SetVisibility(Health->HasHelmet() && HelmetMesh->GetStaticMesh() != nullptr); }
}

void ANSCCharacter::PlayMuzzleFlash()
{
	MuzzleLight->SetIntensity(9000.f);
	MuzzleLightOff = GetWorld()->GetTimeSeconds() + 0.05f;
}

void ANSCCharacter::AddWeaponKick(const FNSCWeaponStats& Stats)
{
	// small weapon model kick back; camera kick comes from the spray pattern (UNSCWeaponComponent::GetViewKick)
	KickRecover = FMath::Min(KickRecover + 1.f, 3.f);
}

void ANSCCharacter::PlayReloadAnimation()
{
	PlayAnim(TEXT("A_Reload"), false);
	OneShotAnimUntil = GetWorld()->GetTimeSeconds() + 1.2f;
}

void ANSCCharacter::PlayThrowAnimation()
{
	PlayAnim(TEXT("A_Throw"), false);
	OneShotAnimUntil = GetWorld()->GetTimeSeconds() + 0.9f;
}

void ANSCCharacter::SpawnImpactParticles(const FVector& Point, const FVector& Normal, bool bFlesh) const
{
	const TCHAR* Path = bFlesh ? TEXT("/Game/NSC/FX/P_ImpactBlood.P_ImpactBlood") : TEXT("/Game/NSC/FX/P_ImpactSparks.P_ImpactSparks");
	if (UParticleSystem* PS = LoadObject<UParticleSystem>(nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), PS, Point, Normal.Rotation());
	}
}

// ---------------------------------------------------------------- animation (single-node, no AnimBP needed)
UAnimSequence* ANSCCharacter::FindAnim(const TCHAR* Name) const
{
	const FName Key(Name);
	if (const TObjectPtr<UAnimSequence>* Found = AnimCache.Find(Key)) { return Found->Get(); }
	const FString Path = FString::Printf(TEXT("%s%s.%s"), NSCPaths::AnimFolder, Name, Name);
	UAnimSequence* Seq = LoadObject<UAnimSequence>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	const_cast<ANSCCharacter*>(this)->AnimCache.Add(Key, Seq);
	return Seq;
}

void ANSCCharacter::PlayAnim(const TCHAR* Name, bool bLoop, float Rate)
{
	if (GetNetMode() == NM_DedicatedServer || !GetMesh() || !GetMesh()->GetSkeletalMeshAsset()) { return; }
	const FName Key(Name);
	if (Key == CurrentAnimName && bLoop) { return; }
	UAnimSequence* Seq = FindAnim(Name);
	if (!Seq) { return; }
	if (Seq->GetSkeleton() != GetMesh()->GetSkeletalMeshAsset()->GetSkeleton()) { return; }
	CurrentAnimName = Key;
	GetMesh()->PlayAnimation(Seq, bLoop);
	GetMesh()->SetPlayRate(Rate);
}

void ANSCCharacter::TickAnimation(float DeltaSeconds)
{
	if (!Health || Health->IsDead()) { return; }
	if (GetWorld()->GetTimeSeconds() < OneShotAnimUntil) { return; }
	const float Speed = GetVelocity().Size2D();
	if (Health->IsDowned())
	{
		PlayAnim(Speed > 10.f ? TEXT("A_Crawl") : TEXT("A_Downed"), true);
		return;
	}
	if (CurrentAction == ENSCActionType::Reviving || CurrentAction == ENSCActionType::Planting || CurrentAction == ENSCActionType::Defusing)
	{
		PlayAnim(TEXT("A_Revive"), true);
		return;
	}
	if (IsDragging()) { PlayAnim(TEXT("A_Drag"), true, 0.8f); return; }
	if (GetCharacterMovement()->IsFalling()) { PlayAnim(TEXT("A_Jump"), true); return; }
	if (bIsCrouched) { PlayAnim(Speed > 20.f ? TEXT("A_CrouchWalk") : TEXT("A_CrouchIdle"), true); return; }
	if (Speed < 20.f) { PlayAnim(Weapons->GetActiveSlot() == ENSCWeaponSlot::Secondary ? TEXT("A_PistolIdle") : TEXT("A_Idle"), true); return; }
	if (Speed < 320.f) { PlayAnim(TEXT("A_Walk"), true); return; }
	if (Speed < 560.f) { PlayAnim(TEXT("A_Jog"), true); return; }
	PlayAnim(TEXT("A_Sprint"), true);
}

// ---------------------------------------------------------------- camera / feel (owning client)
void ANSCCharacter::TickCamera(float DeltaSeconds)
{
	// Lean (smoothed)
	Lean = FMath::FInterpTo(Lean, static_cast<float>(LeanInput), DeltaSeconds, 10.f);
	float EyeZ = bIsCrouched ? CrouchedEyeHeight : BaseEyeHeight;
	if (Health->IsDowned()) { EyeZ = -20.f; }
	// crouch offset: when crouched the capsule shrinks, CameraRoot is relative to the capsule centre
	const FVector Target(0.f, Lean * 22.f, EyeZ);
	CameraRoot->SetRelativeLocation(FMath::VInterpTo(CameraRoot->GetRelativeLocation(), Target, DeltaSeconds, 14.f));

	if (!IsLocallyControlled()) { return; }
	const FVector2D Kick = Weapons->GetViewKick();
	FRotator View = GetControlRotation();
	View.Pitch += Kick.X;
	View.Yaw += Kick.Y;
	View.Roll = Lean * 9.f;
	FirstPersonCamera->SetWorldRotation(View);

	// FOV: aim-down-sights / scopes
	float WantFOV = 90.f;
	if (const UNSCGameInstance* GI = GetGameInstance<UNSCGameInstance>()) { WantFOV = GI->GetFieldOfView(); }
	FNSCWeaponStats S;
	if (Weapons->IsAiming() && Weapons->GetActiveStats(S)) { WantFOV = S.bScoped ? S.AimFOV : FMath::Min(WantFOV, S.AimFOV); }
	CurrentFOV = FMath::FInterpTo(CurrentFOV, WantFOV, DeltaSeconds, 16.f);
	FirstPersonCamera->SetFieldOfView(CurrentFOV);
	WeaponMesh1P->SetVisibility(!(Weapons->IsAiming() && S.bScoped) && Health->IsAliveAndStanding() && WeaponMesh1P->GetStaticMesh() != nullptr);

	// weapon sway from mouse movement + kick recovery
	WeaponSway = FMath::RInterpTo(WeaponSway, FRotator(-LastLookInput.Y * 1.2f, LastLookInput.X * 1.2f, 0.f), DeltaSeconds, 8.f);
	LastLookInput = FVector2D::ZeroVector;
	KickRecover = FMath::FInterpTo(KickRecover, 0.f, DeltaSeconds, 14.f);
	if (Weapons->GetActiveSlot() != ENSCWeaponSlot::Grenade && Weapons->GetActiveStats(S))
	{
		FVector Offset = S.MeshOffset1P;
		if (Weapons->IsAiming() && !S.bScoped) { Offset = FVector(Offset.X - 4.f, 0.f, Offset.Z + 6.f); }
		Offset.X -= KickRecover * 2.5f;
		WeaponMesh1P->SetRelativeLocation(FMath::VInterpTo(WeaponMesh1P->GetRelativeLocation(), Offset, DeltaSeconds, 20.f));
		WeaponMesh1P->SetRelativeRotation(S.MeshRotation + WeaponSway);
	}
}

void ANSCCharacter::TickFootsteps(float DeltaSeconds)
{
	if (GetNetMode() == NM_DedicatedServer || !Health->IsAliveAndStanding() || GetCharacterMovement()->IsFalling()) { return; }
	const float Speed = GetVelocity().Size2D();
	if (Speed < 150.f || bIsCrouched) { return; } // walking and crouching are silent (CS rules)
	const UNSCMovementComponent* M = GetNSCMovement();
	if (M && M->bWantsToWalk) { return; }
	StepDistance += Speed * DeltaSeconds;
	const float Stride = Speed > 560.f ? 190.f : 160.f;
	if (StepDistance < Stride) { return; }
	StepDistance = 0.f;
	const int32 Index = FMath::RandRange(1, 4);
	const FString Path = FString::Printf(TEXT("/Game/NSC/Audio/Footsteps/SW_Step_Concrete_%02d.SW_Step_Concrete_%02d"), Index, Index);
	if (USoundBase* S = LoadObject<USoundBase>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		UGameplayStatics::PlaySoundAtLocation(this, S, GetActorLocation() - FVector(0, 0, 80.f), IsLocallyControlled() ? 0.45f : 0.9f);
	}
}

void ANSCCharacter::UpdateInteractPrompt()
{
	InteractPrompt.Reset();
	if (!Health->IsAliveAndStanding())
	{
		if (Health->IsDowned()) { InteractPrompt = IsBeingDragged() ? TEXT("Being dragged to cover") : TEXT("DOWNED - crawl to cover, wait for a teammate"); }
		return;
	}
	if (CurrentAction != ENSCActionType::None && CurrentAction != ENSCActionType::Dragging) { return; }
	if (IsDragging()) { InteractPrompt = TEXT("Release G to let go"); return; }
	if (const ANSCCharacter* Downed = FindDownedTeammate(NSCRules::InteractRange))
	{
		InteractPrompt = Weapons->Medkits > 0
			? FString::Printf(TEXT("Hold F to revive %s  |  Hold G to drag"), *Downed->GetDisplayName())
			: FString::Printf(TEXT("No medical kit  |  Hold G to drag %s"), *Downed->GetDisplayName());
		return;
	}
	const ANSCGameState* GS = GetWorld()->GetGameState<ANSCGameState>();
	if (bHasBomb && IsInBombSite() && GS && GS->GetPhase() == ENSCRoundPhase::Live) { InteractPrompt = TEXT("Hold F to plant the charge"); return; }
	if (GetTeam() == ENSCTeam::Defenders && GS && GS->GetPhase() == ENSCRoundPhase::BombPlanted)
	{
		if (const ANSCBomb* B = FindBomb()) { if (FVector::Dist(B->GetActorLocation(), GetActorLocation()) < 150.f) { InteractPrompt = TEXT("Hold F to defuse"); return; } }
	}
	if (Health->GetBleed() == ENSCBleed::Heavy && Weapons->Tourniquets > 0) { InteractPrompt = TEXT("Heavy bleeding - press V for a tourniquet"); return; }
	if (Health->GetBleed() != ENSCBleed::None && Weapons->Bandages > 0) { InteractPrompt = TEXT("Bleeding - press X to bandage"); return; }
}

// ---------------------------------------------------------------- tick
void ANSCCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Now = GetWorld()->GetTimeSeconds();

	if (HasAuthority())
	{
		FNSCPositionSample Sample;
		Sample.Time = Now;
		Sample.Location = GetActorLocation();
		PositionHistory.Add(Sample);
		while (PositionHistory.Num() > 0 && Now - PositionHistory[0].Time > 1.f) { PositionHistory.RemoveAt(0); }
		TickAction(DeltaSeconds);
		TickDrag(DeltaSeconds);
		// keep downed players crouched (smaller capsule)
		if (Health->IsDowned() && !bIsCrouched) { Crouch(); }
	}

	if (MuzzleLightOff > 0.f && Now > MuzzleLightOff) { MuzzleLight->SetIntensity(0.f); MuzzleLightOff = 0.f; }

	TickCamera(DeltaSeconds);
	TickAnimation(DeltaSeconds);
	TickFootsteps(DeltaSeconds);
	if (IsLocallyControlled()) { UpdateInteractPrompt(); }
}

void ANSCCharacter::Destroyed()
{
	if (HasAuthority())
	{
		StopDragging();
		if (DraggedBy) { DraggedBy->StopDragging(); }
	}
	Super::Destroyed();
}
