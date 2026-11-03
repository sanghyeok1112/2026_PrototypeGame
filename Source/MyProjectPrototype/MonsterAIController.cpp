#include "MonsterAIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BrainComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"
#include "TimerManager.h"
#include "MonsterCharacter.h"
#include "MyProjectPrototypeCharacter.h"
#include "ColonyCore.h"

static bool IsLivingPlayerPawn(APawn* Pawn)
{
    const AMyProjectPrototypeCharacter* Player = Cast<AMyProjectPrototypeCharacter>(Pawn);
    return IsValid(Player) && Player->IsPlayerControlled() && Player->IsAlive();
}

AMonsterAIController::AMonsterAIController()
{
    SightPerception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("MonsterPerception"));
    SetPerceptionComponent(*SightPerception);
    SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
    SightConfig->SightRadius = 1500.f;
    SightConfig->LoseSightRadius = 1800.f;
    SightConfig->PeripheralVisionAngleDegrees = 70.f;
    SightConfig->SetMaxAge(4.f);
    SightConfig->DetectionByAffiliation.bDetectEnemies = true;
    SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
    SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
    SightPerception->ConfigureSense(*SightConfig);
    SightPerception->SetDominantSense(UAISense_Sight::StaticClass());
    SightPerception->OnTargetPerceptionUpdated.AddDynamic(this, &ThisClass::OnSightChanged);
}

void AMonsterAIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    // A soft path avoids loading a newly-created asset in the native class constructor.
    if (!BehaviorTreeAsset)
        BehaviorTreeAsset = LoadObject<UBehaviorTree>(nullptr, TEXT("/Game/Monster/AI/BT_Monster.BT_Monster"));
    if (!BehaviorTreeAsset || !RunBehaviorTree(BehaviorTreeAsset))
    {
        UE_LOG(LogTemp, Error, TEXT("MONSTER_AI %s: missing or invalid BT_Monster"), *GetName());
        return;
    }
    GetWorldTimerManager().SetTimer(ObservationTimer, this, &ThisClass::UpdateObservation, .2f, true);
    UpdateObservation();
    SightPerception->RequestStimuliListenerUpdate();
}

void AMonsterAIController::OnSightChanged(AActor* Actor, FAIStimulus Stimulus)
{
    if(bDefenseFinished) return;
    APawn* Candidate = Cast<APawn>(Actor);
    if (!IsLivingPlayerPawn(Candidate)) return;
    if (Stimulus.WasSuccessfullySensed())
    {
        if (TargetPawn.IsValid() && TargetPawn != Candidate) return;
        TargetPawn = Candidate;
        bHasSight = true;
        LostSightAt = -1.;
        LastSeenLocation = Candidate->GetActorLocation();
    }
    else if (TargetPawn == Candidate)
    {
        if (bHasSight) LostSightAt = GetWorld()->GetTimeSeconds();
        bHasSight = false;
        bNearTarget = false;
        StopMovement();
        ClearFocus(EAIFocusPriority::Gameplay);
    }
    UpdateObservation();
}

void AMonsterAIController::ForgetTarget()
{
    TargetPawn.Reset();
    bHasSight = false;
    bNearTarget = false;
    LostSightAt = -1.;
    StopMovement();
    ClearFocus(EAIFocusPriority::Gameplay);
    if (UBlackboardComponent* BB = GetBlackboardComponent())
    {
        BB->SetValueAsBool(TEXT("HasSight"), false);
        BB->SetValueAsBool(TEXT("HasLastKnown"), false);
        BB->SetValueAsBool(TEXT("IsNearTarget"), false);
        BB->ClearValue(TEXT("TargetActor"));
        BB->ClearValue(TEXT("LastKnownLocation"));
        BB->ClearValue(TEXT("MoveGoal"));
    }
    SetState(TEXT("Idle"));
}

void AMonsterAIController::UpdateObservation()
{
    if(bDefenseFinished) return;
    UBlackboardComponent* BB = GetBlackboardComponent();
    APawn* Self = GetPawn();
    if (!BB || !Self) return;

    // Keep the chosen target fixed until this attack finishes, even if a player
    // becomes visible while the monster is striking the objective.
    if(AMonsterCharacter* Attacker=Cast<AMonsterCharacter>(Self); Attacker && Attacker->IsAttackInProgress())
    {
        BB->SetValueAsBool(TEXT("IsNearTarget"),true);
        SetState(Attacker->GetAttackPhase());
        return;
    }

    if (!TargetPawn.IsValid())
    {
        // Do not clear/rewrite the core BB branch every observation tick:
        // that would repeatedly abort the running Move To task.
        if(!DefenseObjective.IsValid() || Cast<AMyProjectPrototypeCharacter>(BB->GetValueAsObject(TEXT("TargetActor")))) ForgetTarget();
        // Perception does not send a second "seen" event for an already visible candidate.
        TArray<AActor*> Visible;
        SightPerception->GetCurrentlyPerceivedActors(UAISense_Sight::StaticClass(), Visible);
        Visible.Sort([Self](const AActor& A, const AActor& B) {
            const double DA = FVector::DistSquared(Self->GetActorLocation(), A.GetActorLocation());
            const double DB = FVector::DistSquared(Self->GetActorLocation(), B.GetActorLocation());
            return DA == DB ? A.GetUniqueID() < B.GetUniqueID() : DA < DB;
        });
        for (AActor* Actor : Visible)
            if (APawn* Candidate = Cast<APawn>(Actor); IsLivingPlayerPawn(Candidate))
            {
                TargetPawn = Candidate;
                bHasSight = true;
                break;
            }
        if (!TargetPawn.IsValid()) { UpdateDefenseObjective(); return; }
    }
    APawn* Target = TargetPawn.Get();
    if (!IsLivingPlayerPawn(Target))
    {
        if (AMonsterCharacter* Monster = Cast<AMonsterCharacter>(Self)) Monster->CancelMeleeAttack();
        ForgetTarget();
        UpdateDefenseObjective();
        return;
    }
    BB->SetValueAsObject(TEXT("TargetActor"), Target);

    // Preserve the attack branch through windup/strike/recovery. Ordinary sight
    // and distance changes are acted on after the locked animation interval.
    if (AMonsterCharacter* Monster = Cast<AMonsterCharacter>(Self); Monster && Monster->IsAttackInProgress())
    {
        if (bHasSight) LastSeenLocation = Target->GetActorLocation();
        BB->SetValueAsBool(TEXT("IsNearTarget"), true);
        SetState(Monster->GetAttackPhase());
        return;
    }

    if (bHasSight)
    {
        // No target transform is read in the lost-sight branch below.
        LastSeenLocation = Target->GetActorLocation();
        float SelfRadius = 0.f, TargetRadius = 0.f;
        if (const ACharacter* C = Cast<ACharacter>(Self)) SelfRadius = C->GetCapsuleComponent()->GetScaledCapsuleRadius();
        if (const ACharacter* C = Cast<ACharacter>(Target)) TargetRadius = C->GetCapsuleComponent()->GetScaledCapsuleRadius();
        const float Gap = FMath::Max(0.f, FVector::Dist2D(Self->GetActorLocation(), LastSeenLocation) - SelfRadius - TargetRadius);
        const bool bWasNear = bNearTarget;
        bNearTarget = Gap <= (bNearTarget ? ResumeSurfaceDistance : StopSurfaceDistance)
            && FMath::Abs(Self->GetActorLocation().Z - LastSeenLocation.Z) <= 100.f;
        if (bNearTarget && !bWasNear) StopMovement();
        BB->SetValueAsVector(TEXT("MoveGoal"), LastSeenLocation);
        BB->SetValueAsVector(TEXT("LastKnownLocation"), LastSeenLocation);
        BB->SetValueAsBool(TEXT("HasLastKnown"), true);
        BB->SetValueAsBool(TEXT("IsNearTarget"), bNearTarget);
        BB->SetValueAsBool(TEXT("HasSight"), true);
        SetState(bNearTarget ? TEXT("NearTarget") : TEXT("Chasing"));
    }
    else
    {
        if (LostSightAt < 0. || GetWorld()->GetTimeSeconds() - LostSightAt >= MemorySeconds
            || FVector::Dist2D(Self->GetActorLocation(), LastSeenLocation) > 3000.f)
        {
            ForgetTarget();
            UpdateDefenseObjective();
            return;
        }
        BB->SetValueAsBool(TEXT("HasSight"), false);
        BB->SetValueAsBool(TEXT("IsNearTarget"), false);
        BB->SetValueAsBool(TEXT("HasLastKnown"), true);
        BB->SetValueAsVector(TEXT("MoveGoal"), LastSeenLocation);
        SetState(TEXT("Investigating"));
    }
}

void AMonsterAIController::SetDefenseObjective(AColonyCore* Objective)
{
    DefenseObjective=Objective;
    UpdateObservation();
}
void AMonsterAIController::UpdateDefenseObjective()
{
    AColonyCore* Objective=DefenseObjective.Get();
    AMonsterCharacter* Monster=Cast<AMonsterCharacter>(GetPawn());
    UBlackboardComponent* BB=GetBlackboardComponent();
    if(!BB || !IsValid(Objective) || !Objective->IsAlive() || !Monster || !Monster->IsAlive()) return;
    const FVector Goal=Objective->GetActorLocation();
    const float Gap=FMath::Max(0.f,FVector::Dist2D(Monster->GetActorLocation(),Goal)
        -Monster->GetCapsuleComponent()->GetScaledCapsuleRadius()-Objective->GetTargetRadius());
    const bool bClose=Gap<=StopSurfaceDistance && FMath::Abs(Monster->GetActorLocation().Z-Goal.Z)<=100.f;
    FHitResult Hit;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(CoreAttackVisibility),false,Monster);
    const bool bBlocked=GetWorld()->LineTraceSingleByChannel(Hit,Monster->GetActorLocation(),Goal,ECC_Visibility,Query);
    const bool bCanAttack=bClose && (!bBlocked || Hit.GetActor()==Objective);
    BB->SetValueAsObject(TEXT("TargetActor"),Objective);
    BB->SetValueAsVector(TEXT("MoveGoal"),Goal);
    // Reuse the existing attack/chase BT branches. This known defense objective
    // does not come from sight and never replaces last-seen player information.
    BB->SetValueAsBool(TEXT("HasSight"),true);
    BB->SetValueAsBool(TEXT("HasLastKnown"),false);
    BB->SetValueAsBool(TEXT("IsNearTarget"),bCanAttack);
    SetState(bCanAttack?TEXT("AttackingCore"):TEXT("AdvancingToCore"));
}
void AMonsterAIController::StopForDefenseResult()
{
    bDefenseFinished=true;
    GetWorldTimerManager().ClearTimer(ObservationTimer);
    if(AMonsterCharacter* Monster=Cast<AMonsterCharacter>(GetPawn())) Monster->CancelMeleeAttack();
    StopMovement();
    ClearFocus(EAIFocusPriority::Gameplay);
    if(BrainComponent) BrainComponent->StopLogic(TEXT("Defense result"));
}

void AMonsterAIController::SetState(FName NewState)
{
    if (CurrentState == NewState) return;
    CurrentState = NewState;
    if (UBlackboardComponent* BB = GetBlackboardComponent()) BB->SetValueAsName(TEXT("AIState"), NewState);
    UE_LOG(LogTemp, Display, TEXT("MONSTER_AI %s State=%s"), *GetName(), *NewState.ToString());
}

void AMonsterAIController::OnUnPossess()
{
    GetWorldTimerManager().ClearTimer(ObservationTimer);
    ForgetTarget();
    if (BrainComponent) BrainComponent->StopLogic(TEXT("Monster unpossessed"));
    Super::OnUnPossess();
}

void AMonsterAIController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(ObservationTimer);
    StopMovement();
    if (BrainComponent) BrainComponent->StopLogic(TEXT("Monster end play"));
    Super::EndPlay(EndPlayReason);
}
