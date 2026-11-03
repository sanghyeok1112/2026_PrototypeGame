// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "MonsterCharacter.generated.h"

class UStaticMeshComponent;

UCLASS()
class MYPROJECTPROTOTYPE_API AMonsterCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	AMonsterCharacter();

    bool TryStartMeleeAttack(AActor* Target);
    void CancelMeleeAttack();
    bool IsAttackInProgress() const { return bAttackInProgress; }
    bool IsAlive() const { return !bIsDead && CurrentHealth > 0.f; }
    float GetHealth() const { return CurrentHealth; }
    FName GetAttackPhase() const { return AttackPhase; }
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual float TakeDamage(
		float DamageAmount,
		const FDamageEvent& DamageEvent,
		AController* EventInstigator,
		AActor* DamageCauser) override;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Monster")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Monster",
		meta = (ClampMin = "1.0"))
	float MaxHealth = 100.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Monster")
	float CurrentHealth = 0.f;

    UPROPERTY(EditDefaultsOnly, Category="Monster|Attack", meta=(ClampMin="0"))
    float AttackDamage = 10.f;
    UPROPERTY(EditDefaultsOnly, Category="Monster|Attack", meta=(ClampMin="0"))
    float AttackReach = 150.f;
    UPROPERTY(EditDefaultsOnly, Category="Monster|Attack", meta=(ClampMin="0.01"))
    float WindupSeconds = .4f;
    UPROPERTY(EditDefaultsOnly, Category="Monster|Attack", meta=(ClampMin="0.01"))
    float HitWindowSeconds = .15f;
    UPROPERTY(EditDefaultsOnly, Category="Monster|Attack", meta=(ClampMin="0.01"))
    float RecoverySeconds = .45f;
    UPROPERTY(EditDefaultsOnly, Category="Monster|Attack", meta=(ClampMin="0.01"))
    float AttackCooldown = 1.5f;
    UPROPERTY(EditDefaultsOnly, Category="Monster|Attack")
    bool bShowAttackDebug = true;

private:
    bool bIsDead = false;
    bool bAttackInProgress = false;
    bool bDamageApplied = false;
    double NextAttackTime = 0.;
    FName AttackPhase = TEXT("Idle");
    TWeakObjectPtr<AActor> AttackTarget;
    FVector AttackForward = FVector::ForwardVector;
    FTimerHandle PhaseTimer;
    FTimerHandle HitTimer;
    bool IsValidAttackTarget(AActor* Target, bool bCheckDirection) const;
    void BeginHitWindow();
    void CheckMeleeHit();
    void BeginRecovery();
    void FinishMeleeAttack();

};
