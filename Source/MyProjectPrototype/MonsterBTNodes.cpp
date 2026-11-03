#include "MonsterBTNodes.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType.h"

UBTDecorator_MonsterFlag::UBTDecorator_MonsterFlag()
{
    FlowAbortMode = EBTFlowAbortMode::Both;
    OperationType = static_cast<uint8>(EBasicKeyOperation::Set);
#if WITH_EDITORONLY_DATA
    BasicOperation = EBasicKeyOperation::Set;
#endif
}

void UBTDecorator_MonsterFlag::Configure(FName Key)
{
    BlackboardKey.SelectedKeyName = Key;
    NodeName = Key.ToString();
}

UBTTask_MonsterMove::UBTTask_MonsterMove()
{
    bObserveBlackboardValue = true;
    bTrackMovingGoal = FValueOrBBKey_Bool(false);
    bAllowPartialPath = FValueOrBBKey_Bool(false);
    bReachTestIncludesAgentRadius = FValueOrBBKey_Bool(false);
    bReachTestIncludesGoalRadius = FValueOrBBKey_Bool(false);
    ObservedBlackboardValueTolerance = FValueOrBBKey_Float(50.f);
}

void UBTTask_MonsterMove::Configure(FName Key, float Radius)
{
    BlackboardKey.SelectedKeyName = Key;
    AcceptableRadius = FValueOrBBKey_Float(Radius);
    NodeName = Key == TEXT("LastKnownLocation") ? TEXT("Investigate last seen location") : TEXT("Chase observed location");
}
