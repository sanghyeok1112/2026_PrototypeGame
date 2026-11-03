#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "MonsterAIController.generated.h"

class UAISenseConfig_Sight;
class UBehaviorTree;
class AColonyCore;

/** Perception owns observations; the behavior tree owns navigation decisions. */
UCLASS()
class MYPROJECTPROTOTYPE_API AMonsterAIController : public AAIController
{
    GENERATED_BODY()
public:
    AMonsterAIController();
    void SetDefenseObjective(AColonyCore* Objective);
    void StopForDefenseResult();
    virtual void OnPossess(APawn* InPawn) override;
    virtual void OnUnPossess() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Monster AI")
    TObjectPtr<UAIPerceptionComponent> SightPerception;
    UPROPERTY()
    TObjectPtr<UAISenseConfig_Sight> SightConfig;
    UPROPERTY(EditDefaultsOnly, Category="Monster AI")
    TObjectPtr<UBehaviorTree> BehaviorTreeAsset;
    UPROPERTY(EditDefaultsOnly, Category="Monster AI", meta=(ClampMin="0.1"))
    float MemorySeconds = 4.f;
    UPROPERTY(EditDefaultsOnly, Category="Monster AI", meta=(ClampMin="0"))
    float StopSurfaceDistance = 150.f;
    UPROPERTY(EditDefaultsOnly, Category="Monster AI", meta=(ClampMin="0"))
    float ResumeSurfaceDistance = 190.f;

private:
    UFUNCTION()
    void OnSightChanged(AActor* Actor, FAIStimulus Stimulus);
    void UpdateObservation();
    void ForgetTarget();
    void SetState(FName NewState);
    void UpdateDefenseObjective();
    TWeakObjectPtr<AColonyCore> DefenseObjective;
    bool bDefenseFinished=false;
    TWeakObjectPtr<APawn> TargetPawn;
    FVector LastSeenLocation = FVector::ZeroVector;
    double LostSightAt = -1.;
    bool bHasSight = false;
    bool bNearTarget = false;
    FName CurrentState = NAME_None;
    FTimerHandle ObservationTimer;
};
