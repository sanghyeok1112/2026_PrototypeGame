#if WITH_EDITOR
#include "BTTask_MonsterAttack.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BTCompositeNode.h"
#include "BehaviorTree/Tasks/BTTask_Wait.h"
#include "BehaviorTreeGraph.h"
#include "BehaviorTreeGraphNode_Task.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"

static void SetupMonsterCombat(UWorld* World)
{
    if (!World || World->IsGameWorld()) return;
    UBehaviorTree* Tree = LoadObject<UBehaviorTree>(nullptr, TEXT("/Game/Monster/AI/BT_Monster.BT_Monster"));
    UBehaviorTreeGraph* Graph = Tree ? Cast<UBehaviorTreeGraph>(Tree->BTGraph) : nullptr;
    if (!Graph || !Tree->RootNode || Tree->RootNode->Children.Num() != 4)
    {
        UE_LOG(LogTemp, Error, TEXT("MONSTER_COMBAT expected the verified four-branch tree; no changes made"));
        return;
    }
    UBTTaskNode* NearTask = Tree->RootNode->Children[0].ChildTask;
    if (NearTask && NearTask->IsA<UBTTask_MonsterAttack>())
    {
        UE_LOG(LogTemp, Display, TEXT("MONSTER_COMBAT attack task already installed"));
        return;
    }
    if (!NearTask || !NearTask->IsA<UBTTask_Wait>())
    {
        UE_LOG(LogTemp, Error, TEXT("MONSTER_COMBAT first branch is not Wait; no changes made"));
        return;
    }
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        UBehaviorTreeGraphNode_Task* TaskNode = Cast<UBehaviorTreeGraphNode_Task>(Node);
        if (!TaskNode || TaskNode->NodeInstance != NearTask) continue;
        Tree->Modify(); Graph->Modify(); TaskNode->Modify();
        TaskNode->NodeInstance = NewObject<UBTTask_MonsterAttack>(Tree);
        // Persist ClassData as well as NodeInstance so reopening the editor keeps the node valid.
        TaskNode->UpdateNodeClassData();
        Graph->UpdateAsset();
        Tree->MarkPackageDirty();
        FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone;
        const FString File = FPackageName::LongPackageNameToFilename(Tree->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
        const bool Saved = UPackage::SavePackage(Tree->GetOutermost(), Tree, *File, Args);
        UE_LOG(LogTemp, Display, TEXT("MONSTER_COMBAT attack branch installed saved=%d; movement branches preserved"), Saved);
        return;
    }
    UE_LOG(LogTemp, Error, TEXT("MONSTER_COMBAT could not find near branch graph node"));
}
static FAutoConsoleCommandWithWorld MonsterCombatSetupCommand(TEXT("MonsterCombat.Setup"),
    TEXT("Replace only the verified near-target Wait node with the melee attack task."),
    FConsoleCommandWithWorldDelegate::CreateStatic(&SetupMonsterCombat));
#endif
