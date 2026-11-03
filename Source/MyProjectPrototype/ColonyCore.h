#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ColonyCore.generated.h"

class UCapsuleComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FColonyDestroyed);

/** Temporary damageable objective representing the mine's survival condition. */
UCLASS()
class MYPROJECTPROTOTYPE_API AColonyCore : public AActor
{
    GENERATED_BODY()
public:
    AColonyCore();
    virtual void BeginPlay() override;
    virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
        AController* EventInstigator, AActor* DamageCauser) override;
    UFUNCTION(BlueprintPure) bool IsAlive() const { return CurrentHealth > 0.f; }
    float GetHealth() const { return CurrentHealth; }
    float GetMaxHealth() const { return MaxHealth; }
    float GetTargetRadius() const;
    void SetDamageEnabled(bool bEnabled) { bDamageEnabled=bEnabled; }
    UPROPERTY(BlueprintAssignable) FColonyDestroyed OnCoreDestroyed;
    UPROPERTY(EditAnywhere, Category="Defense", meta=(ClampMin="1")) float MaxHealth=500.f;
private:
    UPROPERTY(VisibleAnywhere) TObjectPtr<UCapsuleComponent> Collision;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> CoreMesh;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> HealthLabel;
    UPROPERTY(VisibleInstanceOnly) float CurrentHealth=0.f;
    bool bDamageEnabled=true;
    void RefreshLabel();
};
