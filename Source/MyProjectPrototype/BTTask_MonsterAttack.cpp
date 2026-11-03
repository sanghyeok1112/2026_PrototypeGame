#include "BTTask_MonsterAttack.h"
#include "AIController.h"
#include "MonsterCharacter.h"
#include "MyProjectPrototypeCharacter.h"
#include "ColonyCore.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"

UBTTask_MonsterAttack::UBTTask_MonsterAttack()
{
    NodeName = TEXT("Melee attack: windup / strike / recovery");
    bCreateNodeInstance = true;
    bNotifyTick = true;
}

EBTNodeResult::Type UBTTask_MonsterAttack::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    bStarted = false;
    AMonsterCharacter* Monster = OwnerComp.GetAIOwner() ? Cast<AMonsterCharacter>(OwnerComp.GetAIOwner()->GetPawn()) : nullptr;
    if (!Monster || !Monster->IsAlive()) return EBTNodeResult::Failed;
    OwnerComp.GetAIOwner()->StopMovement();
    return EBTNodeResult::InProgress;
}

void UBTTask_MonsterAttack::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    AMonsterCharacter* Monster = OwnerComp.GetAIOwner() ? Cast<AMonsterCharacter>(OwnerComp.GetAIOwner()->GetPawn()) : nullptr;
    UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
    AActor* Target = BB ? Cast<AActor>(BB->GetValueAsObject(TEXT("TargetActor"))) : nullptr;
    const AMyProjectPrototypeCharacter* Player=Cast<AMyProjectPrototypeCharacter>(Target);
    const AColonyCore* Core=Cast<AColonyCore>(Target);
    const bool bLivingTarget=(IsValid(Player) && Player->IsAlive()) || (IsValid(Core) && Core->IsAlive());
    if (!IsValid(Monster) || !Monster->IsAlive() || !bLivingTarget)
    {
        if (IsValid(Monster)) Monster->CancelMeleeAttack();
        FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
        return;
    }
    if (bStarted)
    {
        if (!Monster->IsAttackInProgress()) FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
    }
    else
    {
        // Stay in this branch during cooldown instead of falling through into Move To.
        bStarted = Monster->TryStartMeleeAttack(Target);
    }
}

EBTNodeResult::Type UBTTask_MonsterAttack::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    if (OwnerComp.GetAIOwner())
        if (AMonsterCharacter* Monster = Cast<AMonsterCharacter>(OwnerComp.GetAIOwner()->GetPawn())) Monster->CancelMeleeAttack();
    bStarted = false;
    return EBTNodeResult::Aborted;
}
