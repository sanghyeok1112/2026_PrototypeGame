// Fill out your copyright notice in the Description page of Project Settings.


#include "MonsterCharacter.h"
#include "MonsterAIController.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "MyProjectPrototypeCharacter.h"
#include "ColonyCore.h"
#include "BrainComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h"

// Sets default values
AMonsterCharacter::AMonsterCharacter()
{
    PrimaryActorTick.bCanEverTick = false;
    AIControllerClass = AMonsterAIController::StaticClass();
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

    GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);

    BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(
        TEXT("BodyMesh"));

    BodyMesh->SetupAttachment(RootComponent);
    BodyMesh->SetRelativeScale3D(FVector(0.6f, 0.6f, 1.6f));

    BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    GetCapsuleComponent()->SetCollisionResponseToChannel(
        ECC_Visibility, ECR_Block);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
        TEXT("/Engine/BasicShapes/Cube.Cube"));

    if (CubeMesh.Succeeded())
    {
        BodyMesh->SetStaticMesh(CubeMesh.Object);
    }

    GetCharacterMovement()->MaxWalkSpeed = 350.f;
    GetCharacterMovement()->bOrientRotationToMovement = true;
    bUseControllerRotationYaw = false;
}

// Called when the game starts or when spawned
void AMonsterCharacter::BeginPlay()
{
    Super::BeginPlay();

    CurrentHealth = MaxHealth;
}

// Called every frame
void AMonsterCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void AMonsterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

float AMonsterCharacter::TakeDamage(
    float DamageAmount,
    const FDamageEvent& DamageEvent,
    AController* EventInstigator,
    AActor* DamageCauser)
{
    if (bIsDead || CurrentHealth <= 0.f || !FMath::IsFinite(DamageAmount) || DamageAmount <= 0.f)
    {
        return 0.f;
    }

    const float AppliedDamage = FMath::Min(
        DamageAmount, CurrentHealth);

    CurrentHealth -= AppliedDamage;

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("%s Health: %.0f / %.0f"),
        *GetName(),
        CurrentHealth,
        MaxHealth);

    if (CurrentHealth <= 0.f)
    {
        bIsDead = true;
        CancelMeleeAttack();
        GetCharacterMovement()->StopMovementImmediately();
        GetCharacterMovement()->DisableMovement();
        if (AMonsterAIController* AI = Cast<AMonsterAIController>(GetController()))
        {
            AI->StopMovement();
            if (AI->GetBrainComponent()) AI->GetBrainComponent()->StopLogic(TEXT("Monster dead"));
        }
        SetActorEnableCollision(false);
        Destroy();
    }

    return AppliedDamage;
}

bool AMonsterCharacter::IsValidAttackTarget(AActor* Target, bool bCheckDirection) const
{
    if (!IsAlive() || !IsValid(Target)) return false;
    const AMyProjectPrototypeCharacter* Player = Cast<AMyProjectPrototypeCharacter>(Target);
    const AColonyCore* Core = Cast<AColonyCore>(Target);
    if ((!Player || !Player->IsAlive()) && (!Core || !Core->IsAlive())) return false;
    const float TargetRadius = Player ? Player->GetCapsuleComponent()->GetScaledCapsuleRadius() : Core->GetTargetRadius();
    const FVector Delta = Target->GetActorLocation() - GetActorLocation();
    const float Gap = FMath::Max(0.f, Delta.Size2D()
        - GetCapsuleComponent()->GetScaledCapsuleRadius()
        - TargetRadius);
    if (Gap > AttackReach || FMath::Abs(Delta.Z) > 100.f) return false;
    if (bCheckDirection && FVector::DotProduct(Delta.GetSafeNormal2D(), AttackForward) < .573576f) return false;
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(MonsterAttackVisibility), false, this);
    const bool bBlocked = GetWorld()->LineTraceSingleByChannel(Hit, GetActorLocation(), Target->GetActorLocation(), ECC_Visibility, Params);
    return !bBlocked || Hit.GetActor() == Target;
}

bool AMonsterCharacter::TryStartMeleeAttack(AActor* Target)
{
    if (bAttackInProgress || GetWorld()->GetTimeSeconds() < NextAttackTime || !IsValidAttackTarget(Target, false)) return false;
    AttackTarget = Target;
    AttackForward = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
    if (AttackForward.IsNearlyZero()) AttackForward = GetActorForwardVector();
    bAttackInProgress = true;
    bDamageApplied = false;
    AttackPhase = TEXT("Windup");
    NextAttackTime = GetWorld()->GetTimeSeconds() + FMath::Max(.01f, AttackCooldown);
    GetCharacterMovement()->StopMovementImmediately();
    if (AAIController* AI = Cast<AAIController>(GetController())) AI->StopMovement();
    GetCharacterMovement()->bOrientRotationToMovement = false;
    SetActorRotation(AttackForward.Rotation());
    if (bShowAttackDebug)
    {
        const FVector Origin = GetActorLocation() + FVector(0,0,110);
        DrawDebugDirectionalArrow(GetWorld(), Origin, Origin + AttackForward * 220.f, 30.f, FColor::Yellow, false, WindupSeconds, 0, 5.f);
        DrawDebugString(GetWorld(), Origin + FVector(0,0,25), TEXT("ATTACK!"), nullptr, FColor::Yellow, WindupSeconds, true);
    }
    UE_LOG(LogTemp, Display, TEXT("MONSTER_ATTACK %s Windup"), *GetName());
    GetWorldTimerManager().SetTimer(PhaseTimer, this, &ThisClass::BeginHitWindow, FMath::Max(.01f, WindupSeconds), false);
    return true;
}

void AMonsterCharacter::BeginHitWindow()
{
    if (!IsAlive() || !bAttackInProgress) return;
    AttackPhase = TEXT("Strike");
    CheckMeleeHit();
    GetWorldTimerManager().SetTimer(HitTimer, this, &ThisClass::CheckMeleeHit, .03f, true);
    GetWorldTimerManager().SetTimer(PhaseTimer, this, &ThisClass::BeginRecovery, FMath::Max(.01f, HitWindowSeconds), false);
}

void AMonsterCharacter::CheckMeleeHit()
{
    if (!IsAlive() || !bAttackInProgress || AttackPhase != TEXT("Strike") || bDamageApplied) return;
    AActor* Target = AttackTarget.Get();
    if (!IsValidAttackTarget(Target, true)) return;
    const float Radius = GetCapsuleComponent()->GetScaledCapsuleRadius();
    const FVector Start = GetActorLocation() + AttackForward * (Radius + 5.f);
    const FVector End = GetActorLocation() + AttackForward * (Radius + AttackReach);
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(MonsterMeleeSweep), false, this);
    const bool bHit = GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity,
        ECC_Pawn, FCollisionShape::MakeSphere(35.f), Params);
    if (bShowAttackDebug) DrawDebugLine(GetWorld(), Start, End, FColor::Red, false, .2f, 0, 5.f);
    if (bHit && Hit.GetActor() == Target)
    {
        // Set before dispatching damage: callbacks must never deliver a second hit.
        bDamageApplied = true;
        const float Applied = UGameplayStatics::ApplyDamage(Target, AttackDamage, GetController(), this, UDamageType::StaticClass());
        UE_LOG(LogTemp, Display, TEXT("MONSTER_ATTACK %s Hit Damage=%.0f"), *GetName(), Applied);
    }
}

void AMonsterCharacter::BeginRecovery()
{
    GetWorldTimerManager().ClearTimer(HitTimer);
    if (!IsAlive() || !bAttackInProgress) return;
    AttackPhase = TEXT("Recovery");
    UE_LOG(LogTemp, Display, TEXT("MONSTER_ATTACK %s Recovery"), *GetName());
    GetWorldTimerManager().SetTimer(PhaseTimer, this, &ThisClass::FinishMeleeAttack, FMath::Max(.01f, RecoverySeconds), false);
}

void AMonsterCharacter::FinishMeleeAttack()
{
    bAttackInProgress = false;
    AttackPhase = TEXT("Idle");
    AttackTarget.Reset();
    GetCharacterMovement()->bOrientRotationToMovement = true;
}

void AMonsterCharacter::CancelMeleeAttack()
{
    GetWorldTimerManager().ClearTimer(PhaseTimer);
    GetWorldTimerManager().ClearTimer(HitTimer);
    FinishMeleeAttack();
}

void AMonsterCharacter::EndPlay(const EEndPlayReason::Type Reason)
{
    CancelMeleeAttack();
    Super::EndPlay(Reason);
}

