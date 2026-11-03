// Copyright Epic Games, Inc. All Rights Reserved.

#include "MyProjectPrototypeCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "MyProjectPrototype.h"

#include "WeaponBase.h"
#include "GunWeapon.h"
#include "MeleeWeapon.h"

#include "MyProjectPrototypePlayerController.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"

void AMyProjectPrototypeCharacter::BeginPlay()
{
	Super::BeginPlay();
    CurrentHealth = MaxHealth;
    bIsDead = false;
    UpdateHealthDisplay();

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = this;
	SpawnParams.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	PrimaryWeapon = GetWorld()->SpawnActor<AGunWeapon>(
		AGunWeapon::StaticClass(),
		GetActorLocation(),
		GetActorRotation(),
		SpawnParams);

	SecondaryWeapon = GetWorld()->SpawnActor<AMeleeWeapon>(
		AMeleeWeapon::StaticClass(),
		GetActorLocation(),
		GetActorRotation(),
		SpawnParams);

	EquipPrimaryWeapon();
}
AMyProjectPrototypeCharacter::AMyProjectPrototypeCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)

	PrimaryActorTick.bCanEverTick = true;
}

void AMyProjectPrototypeCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AMyProjectPrototypeCharacter::Move);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AMyProjectPrototypeCharacter::Look);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AMyProjectPrototypeCharacter::Look);

		// Sprinting
		EnhancedInputComponent->BindAction(
			SprintAction, ETriggerEvent::Started,
			this, &AMyProjectPrototypeCharacter::StartSprint);

		EnhancedInputComponent->BindAction(
			SprintAction, ETriggerEvent::Completed,
			this, &AMyProjectPrototypeCharacter::StopSprint);

		EnhancedInputComponent->BindAction(
			SprintAction, ETriggerEvent::Canceled,
			this, &AMyProjectPrototypeCharacter::StopSprint);

		// Aiming
		EnhancedInputComponent->BindAction(
			AimAction, ETriggerEvent::Started,
			this, &AMyProjectPrototypeCharacter::StartAim);

		EnhancedInputComponent->BindAction(
			AimAction, ETriggerEvent::Completed,
			this, &AMyProjectPrototypeCharacter::StopAim);

		EnhancedInputComponent->BindAction(
			AimAction, ETriggerEvent::Canceled,
			this, &AMyProjectPrototypeCharacter::StopAim);

		// Firing
		EnhancedInputComponent->BindAction(
			FireAction, ETriggerEvent::Started,
			this, &AMyProjectPrototypeCharacter::Fire);

		// Weapon switching
		EnhancedInputComponent->BindAction(
			EquipPrimaryAction, ETriggerEvent::Started,
			this, &AMyProjectPrototypeCharacter::EquipPrimaryWeapon);

		EnhancedInputComponent->BindAction(
			EquipSecondaryAction, ETriggerEvent::Started,
			this, &AMyProjectPrototypeCharacter::EquipSecondaryWeapon);
	}
	else
	{
		UE_LOG(LogMyProjectPrototype, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void AMyProjectPrototypeCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	// route the input
	DoMove(MovementVector.X, MovementVector.Y);
}

void AMyProjectPrototypeCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// route the input
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void AMyProjectPrototypeCharacter::DoMove(float Right, float Forward)
{
    if (bIsDead) return;
	if (GetController() != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = GetController()->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, Forward);
		AddMovementInput(RightDirection, Right);
	}
}

void AMyProjectPrototypeCharacter::DoLook(float Yaw, float Pitch)
{
	if (GetController() != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void AMyProjectPrototypeCharacter::DoJumpStart()
{
    if (bIsDead) return;
	// signal the character to jump
	Jump();
}

void AMyProjectPrototypeCharacter::DoJumpEnd()
{
	// signal the character to stop jumping
	StopJumping();
}

void AMyProjectPrototypeCharacter::StartSprint()
{
    if (bIsDead) return;
	bIsSprinting = true;
	UpdateMovementSpeed();
}

void AMyProjectPrototypeCharacter::StopSprint()
{
	bIsSprinting = false;
	UpdateMovementSpeed();
}

void AMyProjectPrototypeCharacter::UpdateMovementSpeed()
{
    if (bIsDead) return;
	if (bIsAiming)
	{
		GetCharacterMovement()->MaxWalkSpeed = 250.f;
	}
	else if (bIsSprinting)
	{
		GetCharacterMovement()->MaxWalkSpeed = 900.f;
	}
	else
	{
		GetCharacterMovement()->MaxWalkSpeed = 500.f;
	}
}

void AMyProjectPrototypeCharacter::StartAim()
{
    if (bIsDead) return;
	bIsAiming = true;

	bUseControllerRotationYaw = true;
	GetCharacterMovement()->bOrientRotationToMovement = false;

	if (AMyProjectPrototypePlayerController* PC =
		Cast<AMyProjectPrototypePlayerController>(GetController()))
	{
		PC->SetCrosshairVisible(true);
	}

	UpdateMovementSpeed();
}

void AMyProjectPrototypeCharacter::StopAim()
{
	bIsAiming = false;

	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;

	if (AMyProjectPrototypePlayerController* PC =
		Cast<AMyProjectPrototypePlayerController>(GetController()))
	{
		PC->SetCrosshairVisible(false);
	}

	UpdateMovementSpeed();
}

void AMyProjectPrototypeCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	const float TargetLength = bIsAiming ? 200.f : 400.f;

	const FVector TargetOffset = bIsAiming
		? FVector(0.f, 70.f, 20.f)
		: FVector(0.f, 0.f, 60.f);

	CameraBoom->TargetArmLength = FMath::FInterpTo(
		CameraBoom->TargetArmLength,
		TargetLength,
		DeltaTime,
		10.f);

	CameraBoom->SocketOffset = FMath::VInterpTo(
		CameraBoom->SocketOffset,
		TargetOffset,
		DeltaTime,
		10.f);
}

void AMyProjectPrototypeCharacter::Fire()
{
    if (bIsDead) return;
	if (IsValid(CurrentWeapon))
	{
		CurrentWeapon->Attack();
	}
}

void AMyProjectPrototypeCharacter::EquipPrimaryWeapon()
{
    if (bIsDead) return;
	if (IsValid(PrimaryWeapon))
	{
		CurrentWeapon = PrimaryWeapon;
		UE_LOG(LogTemp, Warning, TEXT("Equipped Gun"));
	}
}

void AMyProjectPrototypeCharacter::EquipSecondaryWeapon()
{
    if (bIsDead) return;
	if (IsValid(SecondaryWeapon))
	{
		CurrentWeapon = SecondaryWeapon;
		UE_LOG(LogTemp, Warning, TEXT("Equipped Melee"));
	}
}

float AMyProjectPrototypeCharacter::TakeDamage(float DamageAmount,
    const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    if (bIsDead || !FMath::IsFinite(DamageAmount) || DamageAmount <= 0.f) return 0.f;
    const float AppliedDamage = FMath::Min(CurrentHealth, DamageAmount);
    CurrentHealth = FMath::Max(0.f, CurrentHealth - AppliedDamage);
    UE_LOG(LogTemp, Display, TEXT("PLAYER_HEALTH HP=%.0f/%.0f Damage=%.0f"), CurrentHealth, MaxHealth, AppliedDamage);
    if (CurrentHealth <= 0.f)
    {
        bIsDead = true;
        bIsSprinting = false;
        StopAim();
        StopJumping();
        GetCharacterMovement()->StopMovementImmediately();
        GetCharacterMovement()->DisableMovement();
        if (APlayerController* PC = Cast<APlayerController>(GetController())) DisableInput(PC);
        UE_LOG(LogTemp, Display, TEXT("PLAYER_HEALTH Dead; stop and restart Play to retry"));
    }
    UpdateHealthDisplay();
    return AppliedDamage;
}

void AMyProjectPrototypeCharacter::UpdateHealthDisplay()
{
    if (GEngine && IsPlayerControlled())
        GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()), 3600.f,
            bIsDead ? FColor::Red : FColor::White,
            bIsDead ? TEXT("HP 0 - DEAD (restart Play)") : FString::Printf(TEXT("HP %.0f / %.0f"), CurrentHealth, MaxHealth));
}

void AMyProjectPrototypeCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);
    UpdateHealthDisplay();
}

void AMyProjectPrototypeCharacter::EndPlay(const EEndPlayReason::Type Reason)
{
    if (GEngine) GEngine->RemoveOnScreenDebugMessage(GetUniqueID());
    if (IsValid(PrimaryWeapon)) PrimaryWeapon->Destroy();
    if (IsValid(SecondaryWeapon) && SecondaryWeapon != PrimaryWeapon) SecondaryWeapon->Destroy();
    Super::EndPlay(Reason);
}
