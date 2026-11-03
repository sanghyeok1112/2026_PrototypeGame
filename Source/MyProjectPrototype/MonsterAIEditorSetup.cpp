// Editor-only setup helper. Uses Unreal asset/graph APIs; never runs in packaged games.
#if WITH_EDITOR
#include "MonsterAIController.h"
#include "MonsterCharacter.h"
#include "MonsterBTNodes.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Name.h"
#include "BehaviorTree/Composites/BTComposite_Selector.h"
#include "BehaviorTree/Tasks/BTTask_Wait.h"
#include "BehaviorTreeGraph.h"
#include "BehaviorTreeGraphNode_Root.h"
#include "EdGraphSchema_BehaviorTree.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Engine/StaticMeshActor.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavigationSystem.h"
#include "Builders/CubeBuilder.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "HAL/IConsoleManager.h"

namespace MonsterAISetup
{
static void SaveAsset(UObject* Asset)
{
    Asset->MarkPackageDirty();
    const FString Filename = FPackageName::LongPackageNameToFilename(
        Asset->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
    FSavePackageArgs Args;
    Args.TopLevelFlags = RF_Public | RF_Standalone;
    Args.SaveFlags = SAVE_NoError;
    const bool bSaved = UPackage::SavePackage(Asset->GetOutermost(), Asset, *Filename, Args);
    UE_LOG(LogTemp, Display, TEXT("MONSTER_SETUP Save %s %s"), *Filename, bSaved ? TEXT("OK") : TEXT("FAILED"));
}

template<class T> static void AddKey(UBlackboardData* BB, FName Name)
{
    FBlackboardEntry Entry;
    Entry.EntryName = Name;
    Entry.KeyType = NewObject<T>(BB);
    BB->Keys.Add(Entry);
}

static void Setup(const TArray<FString>& Args, UWorld* World)
{
    if (!GEditor || !World || World->IsGameWorld()) return;
    UBlackboardData* BB = LoadObject<UBlackboardData>(nullptr, TEXT("/Game/Monster/AI/BB_Monster.BB_Monster"));
    UBehaviorTree* BT = LoadObject<UBehaviorTree>(nullptr, TEXT("/Game/Monster/AI/BT_Monster.BT_Monster"));
    if (!BB && !BT)
    {
        BB = NewObject<UBlackboardData>(CreatePackage(TEXT("/Game/Monster/AI/BB_Monster")), TEXT("BB_Monster"), RF_Public | RF_Standalone);
        AddKey<UBlackboardKeyType_Object>(BB, TEXT("TargetActor"));
        CastChecked<UBlackboardKeyType_Object>(BB->Keys.Last().KeyType)->BaseClass = APawn::StaticClass();
        AddKey<UBlackboardKeyType_Vector>(BB, TEXT("LastKnownLocation"));
        AddKey<UBlackboardKeyType_Vector>(BB, TEXT("MoveGoal"));
        AddKey<UBlackboardKeyType_Bool>(BB, TEXT("HasSight"));
        AddKey<UBlackboardKeyType_Bool>(BB, TEXT("HasLastKnown"));
        AddKey<UBlackboardKeyType_Bool>(BB, TEXT("IsNearTarget"));
        AddKey<UBlackboardKeyType_Name>(BB, TEXT("AIState"));
        BB->UpdatePersistentKey<UBlackboardKeyType_Object>(FBlackboard::KeySelf);
        BB->PostLoad();
        FAssetRegistryModule::AssetCreated(BB);

        BT = NewObject<UBehaviorTree>(CreatePackage(TEXT("/Game/Monster/AI/BT_Monster")), TEXT("BT_Monster"), RF_Public | RF_Standalone);
        BT->BlackboardAsset = BB;
        UBTComposite_Selector* Root = NewObject<UBTComposite_Selector>(BT, TEXT("MonsterDecision"));
        Root->NodeName = TEXT("Monster: stop / chase / investigate / idle");
        BT->RootNode = Root;

        auto AddBranch = [BT, Root](UBTTaskNode* Task, FName Condition)
        {
            FBTCompositeChild Child;
            Child.ChildTask = Task;
            if (!Condition.IsNone())
            {
                UBTDecorator_MonsterFlag* Flag = NewObject<UBTDecorator_MonsterFlag>(BT);
                Flag->Configure(Condition);
                Child.Decorators.Add(Flag);
            }
            Root->Children.Add(Child);
        };
        UBTTask_Wait* Near = NewObject<UBTTask_Wait>(BT);
        Near->NodeName = TEXT("Hold position (attack comes next lesson)");
        Near->WaitTime = FValueOrBBKey_Float(.2f);
        Near->RandomDeviation = FValueOrBBKey_Float(0.f);
        AddBranch(Near, TEXT("IsNearTarget"));
        UBTTask_MonsterMove* Chase = NewObject<UBTTask_MonsterMove>(BT);
        Chase->Configure(TEXT("MoveGoal"), 80.f);
        AddBranch(Chase, TEXT("HasSight"));
        UBTTask_MonsterMove* Investigate = NewObject<UBTTask_MonsterMove>(BT);
        Investigate->Configure(TEXT("LastKnownLocation"), 50.f);
        AddBranch(Investigate, TEXT("HasLastKnown"));
        UBTTask_Wait* Idle = NewObject<UBTTask_Wait>(BT);
        Idle->NodeName = TEXT("Wait for a visible player");
        Idle->WaitTime = FValueOrBBKey_Float(.3f);
        Idle->RandomDeviation = FValueOrBBKey_Float(0.f);
        AddBranch(Idle, NAME_None);

        UBehaviorTreeGraph* Graph = NewObject<UBehaviorTreeGraph>(BT, TEXT("BehaviorTreeGraph"), RF_Transactional);
        BT->BTGraph = Graph;
        Graph->Schema = UEdGraphSchema_BehaviorTree::StaticClass();
        Graph->GetSchema()->CreateDefaultNodesForGraph(*Graph);
        for (UEdGraphNode* Node : Graph->Nodes)
            if (UBehaviorTreeGraphNode_Root* R = Cast<UBehaviorTreeGraphNode_Root>(Node)) R->BlackboardAsset = BB;
        Graph->OnCreated();
        // AutoArrange needs an open graph widget. Use persisted coordinates instead.
        int32 LayoutIndex = 0;
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            Node->NodePosX = LayoutIndex < 2 ? 400 : (LayoutIndex - 2) * 320;
            Node->NodePosY = LayoutIndex == 0 ? 0 : (LayoutIndex == 1 ? 140 : 360);
            ++LayoutIndex;
        }
        Graph->UpdateAsset();
        FAssetRegistryModule::AssetCreated(BT);
        SaveAsset(BB);
        SaveAsset(BT);
    }
    else if (!BB || !BT)
    {
        UE_LOG(LogTemp, Error, TEXT("MONSTER_SETUP partial asset set: refusing to overwrite"));
        return;
    }

    UBlueprint* MonsterBP = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Monster/BP_Monster.BP_Monster"));
    if (MonsterBP && MonsterBP->GeneratedClass)
    {
        APawn* CDO = CastChecked<APawn>(MonsterBP->GeneratedClass->GetDefaultObject());
        CDO->Modify();
        CDO->AIControllerClass = AMonsterAIController::StaticClass();
        CDO->AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
        CDO->PostEditChange();
        FKismetEditorUtilities::CompileBlueprint(MonsterBP);
        SaveAsset(MonsterBP);
    }
    int32 MonsterCount = 0;
    for (TActorIterator<AMonsterCharacter> It(World); It; ++It)
    {
        It->Modify();
        It->AIControllerClass = AMonsterAIController::StaticClass();
        It->AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
        It->MarkPackageDirty();
        ++MonsterCount;
    }

    // Add only if the user has not already placed a navigation bounds volume.
    bool bHasNav = false;
    for (TActorIterator<ANavMeshBoundsVolume> It(World); It; ++It) bHasNav = true;
    if (!bHasNav)
    {
        FBox Bounds(ForceInit);
        for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
            Bounds += It->GetComponentsBoundingBox(true);
        if (!Bounds.IsValid) Bounds = FBox(FVector(-3000,-3000,-500), FVector(3000,3000,1500));
        Bounds = Bounds.ExpandBy(FVector(200,200,500));
        ANavMeshBoundsVolume* Volume = World->SpawnActor<ANavMeshBoundsVolume>();
        Volume->SetActorLabel(TEXT("MonsterAI_NavBounds"));
        UCubeBuilder* Builder = NewObject<UCubeBuilder>(Volume);
        Builder->X = Bounds.GetSize().X;
        Builder->Y = Bounds.GetSize().Y;
        Builder->Z = Bounds.GetSize().Z;
        Builder->Build(World, Volume);
        Volume->SetActorLocation(Bounds.GetCenter());
        Volume->PostEditChange();
        Volume->MarkPackageDirty();
        if (UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
            Nav->OnNavigationBoundsUpdated(Volume);
    }
    FNavigationSystem::Build(*World);
    UE_LOG(LogTemp, Display, TEXT("MONSTER_SETUP COMPLETE: %d placed monsters. Save All to persist level changes."), MonsterCount);
}

static FAutoConsoleCommandWithWorldAndArgs SetupCommand(
    TEXT("MonsterAI.Setup"), TEXT("Create missing monster AI assets and configure the current editor map."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Setup));
}
#endif
