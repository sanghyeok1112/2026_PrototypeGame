#if WITH_EDITOR
#include "CoreMinimal.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "Engine/World.h"
#include "Engine/TargetPoint.h"
#include "EngineUtils.h"
#include "ColonyCore.h"
#include "DefensePrototypeManager.h"
#include "MonsterCharacter.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "NavigationData.h"
#include "Containers/Ticker.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"

static void InstallColonyDefense()
{
    const TCHAR* MapPath=TEXT("/Game/Prototype/Maps/Lvl_ApprovedLayout");
    UWorld* W=UEditorLoadingAndSavingUtils::LoadMap(MapPath);
    if(!W || GEditor->PlayWorld) return;
    ADefensePrototypeManager* Manager=nullptr;
    for(TActorIterator<ADefensePrototypeManager> It(W);It;++It) { Manager=*It; break; }
    if(!Manager)
    {
        AColonyCore* Core=W->SpawnActor<AColonyCore>(FVector(0,7100,150),FRotator::ZeroRotator);
        Core->SetActorLabel(TEXT("Defense_ColonyCore")); Core->SetFolderPath(TEXT("PrototypeDefense"));
        Manager=W->SpawnActor<ADefensePrototypeManager>();
        Manager->SetActorLabel(TEXT("Defense_WaveManager")); Manager->SetFolderPath(TEXT("PrototypeDefense"));
        Manager->Core=Core;
        Manager->MonsterClass=LoadClass<AMonsterCharacter>(nullptr,TEXT("/Game/Monster/BP_Monster.BP_Monster_C"));
        if(!Manager->MonsterClass) Manager->MonsterClass=AMonsterCharacter::StaticClass();
        // Front approach and aircraft breach, both outside the village.
        const FVector Points[]={FVector(1500,500,100),FVector(3900,3200,100)};
        for(int32 I=0;I<2;++I)
        {
            ATargetPoint* Point=W->SpawnActor<ATargetPoint>(Points[I],(Core->GetActorLocation()-Points[I]).Rotation());
            Point->SetActorLabel(FString::Printf(TEXT("Defense_Spawn_%s"),I==0?TEXT("Front"):TEXT("CrashSide")));
            Point->SetFolderPath(TEXT("PrototypeDefense"));
            Point->Tags={TEXT("DefenseSpawn")}; Manager->SpawnPoints.Add(Point);
        }
    }
    FlushAsyncLoading();
    UNavigationSystemV1* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(W);
    int32 Routes=0;
    if(Nav && Manager->Core)
    {
        Nav->ReleaseInitialBuildingLock();
        Nav->RemoveNavigationBuildLock(ENavigationBuildLock::AsyncLoadLock);
        Nav->Build();
        if(ANavigationData* Data=Nav->GetDefaultNavDataInstance(FNavigationSystem::Create))
        { Data->RebuildAll(); Data->EnsureBuildCompletion(); }
        for(ATargetPoint* Point:Manager->SpawnPoints)
        {
            UNavigationPath* Path=Point?UNavigationSystemV1::FindPathToLocationSynchronously(W,Point->GetActorLocation(),Manager->Core->GetActorLocation()):nullptr;
            if(Path && Path->IsValid() && !Path->IsPartial()) ++Routes;
        }
    }
    W->MarkPackageDirty();
    const bool Saved=UEditorLoadingAndSavingUtils::SaveMap(W,MapPath);
    UE_LOG(LogTemp,Display,TEXT("DEFENSE_SETUP saved=%d core=%s spawn_routes=%d/%d enemy=%s"),Saved,*GetNameSafe(Manager->Core),Routes,Manager->SpawnPoints.Num(),*GetNameSafe(Manager->MonsterClass));
    if(FParse::Param(FCommandLine::Get(),TEXT("DefenseBatch"))) FPlatformMisc::RequestExit(false);
}
static FAutoConsoleCommand DefenseSetup(TEXT("DefensePrototype.Setup"),TEXT("Install the colony core and three-wave prototype in the approved map."),
    FConsoleCommandDelegate::CreateLambda([]()
    { FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float){ InstallColonyDefense(); return false; }),2.f); }));
#endif
