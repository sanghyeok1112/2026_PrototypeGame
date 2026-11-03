#pragma once
#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_MonsterAttack.generated.h"

/** Requests character combat; the task never applies damage itself. */
UCLASS()
class MYPROJECTPROTOTYPE_API UBTTask_MonsterAttack : public UBTTaskNode
{
    GENERATED_BODY()
public:
    UBTTask_MonsterAttack();
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
    virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
    virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
private:
    bool bStarted = false;
};
