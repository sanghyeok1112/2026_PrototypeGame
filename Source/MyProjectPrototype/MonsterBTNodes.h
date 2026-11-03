#pragma once
#include "CoreMinimal.h"
#include "BehaviorTree/Decorators/BTDecorator_Blackboard.h"
#include "BehaviorTree/Tasks/BTTask_MoveTo.h"
#include "MonsterBTNodes.generated.h"

UCLASS()
class MYPROJECTPROTOTYPE_API UBTDecorator_MonsterFlag : public UBTDecorator_Blackboard
{
    GENERATED_BODY()
public:
    UBTDecorator_MonsterFlag();
    void Configure(FName Key);
};

UCLASS()
class MYPROJECTPROTOTYPE_API UBTTask_MonsterMove : public UBTTask_MoveTo
{
    GENERATED_BODY()
public:
    UBTTask_MonsterMove();
    void Configure(FName Key, float Radius);
};
