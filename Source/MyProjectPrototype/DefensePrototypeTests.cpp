#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "ColonyCore.h"
#include "DefensePrototypeManager.h"
#include "MonsterCharacter.h"
#include "MonsterAIController.h"
#include "MyProjectPrototypeCharacter.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TargetPoint.h"
#include "EngineUtils.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "NavigationPath.h"
#include "Builders/CubeBuilder.h"
#include "ActorFactories/ActorFactory.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefensePrototypeTest,"Prototype.ColonyDefense.CoreAIWavesAndResults",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDefensePrototypeTest::RunTest(const FString& Parameters)
{
    const UWorld::InitializationValues IVS=UWorld::InitializationValues().AllowAudioPlayback(false)
        .RequiresHitProxies(false).CreatePhysicsScene(true).CreateNavigation(true).CreateAISystem(true)
        .ShouldSimulatePhysics(false).EnableTraceCollision(true).SetTransactional(false);
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&IVS);
    if(!TestNotNull(TEXT("Defense test world"),World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->EndPlay(EEndPlayReason::Quit); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
    World->InitializeActorsForPlay(FURL()); World->GetWorldSettings()->NotifyBeginPlay();
    auto Advance=[World](float Seconds)
    {
        for(int32 I=0;I<FMath::CeilToInt(Seconds/.025f);++I)
        { ++GFrameCounter; World->Tick(LEVELTICK_All,.025f); }
    };
    AStaticMeshActor* Floor=World->SpawnActor<AStaticMeshActor>();
    Floor->SetMobility(EComponentMobility::Movable);
    Floor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
    Floor->SetActorLocation(FVector(0,0,-25)); Floor->SetActorScale3D(FVector(80,80,.5f));
    Floor->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
    Floor->SetMobility(EComponentMobility::Static);
    FNavigationSystem::AddNavigationSystemToWorld(*World,FNavigationSystemRunMode::GameMode);
    ANavMeshBoundsVolume* Bounds=World->SpawnActor<ANavMeshBoundsVolume>();
    UCubeBuilder* Builder=NewObject<UCubeBuilder>(Bounds); Builder->X=7500; Builder->Y=7500; Builder->Z=1000;
    UActorFactory::CreateBrushForVolumeActor(Bounds,Builder); Bounds->PostEditChange();
    UNavigationSystemV1* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
    if(!TestNotNull(TEXT("Navigation system"),Nav)) return false;
    // A runtime world with static NavMesh cannot generate new tiles. Build this
    // isolated fixture as an editor world, then use those tiles in game ticks.
    World->WorldType=EWorldType::Editor;
    Nav->InitializeForWorld(*World,FNavigationSystemRunMode::EditorMode);
    Nav->OnNavigationBoundsUpdated(Bounds); Nav->ReleaseInitialBuildingLock();
    Nav->RemoveNavigationBuildLock(ENavigationBuildLock::AsyncLoadLock); Nav->Build();
    ANavigationData* Data=Nav->GetDefaultNavDataInstance(FNavigationSystem::Create);
    if(!TestNotNull(TEXT("Nav data"),Data)) return false;
    Data->RebuildAll(); Data->EnsureBuildCompletion();
    World->WorldType=EWorldType::Game;
    UNavigationPath* TestRoute=UNavigationSystemV1::FindPathToLocationSynchronously(World,FVector(2000,0,0),FVector(0,0,0));
    if(!TestTrue(TEXT("Test fixture has complete ground navigation"),TestRoute && TestRoute->IsValid() && !TestRoute->IsPartial())) return false;
    AColonyCore* Core=World->SpawnActor<AColonyCore>(FVector(0,0,150),FRotator::ZeroRotator);
    auto SpawnMonster=[World](FVector Pos,bool bAI)
    {
        AMonsterCharacter* M=World->SpawnActorDeferred<AMonsterCharacter>(AMonsterCharacter::StaticClass(),FTransform(FRotator::ZeroRotator,Pos),nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        M->AutoPossessAI=bAI?EAutoPossessAI::PlacedInWorldOrSpawned:EAutoPossessAI::Disabled;
        M->FinishSpawning(FTransform(FRotator::ZeroRotator,Pos)); return M;
    };
    AMonsterCharacter* Attacker=SpawnMonster(FVector(280,0,100),false);
    TestEqual(TEXT("Core starts with 500 HP"),Core->GetHealth(),500.f);
    TestEqual(TEXT("Player/environment cannot damage core"),UGameplayStatics::ApplyDamage(Core,25,nullptr,nullptr,UDamageType::StaticClass()),0.f);
    TestTrue(TEXT("Monster can wind up against core"),Attacker->TryStartMeleeAttack(Core));
    Advance(.3f); TestEqual(TEXT("Core windup has no damage"),Core->GetHealth(),500.f);
    Advance(.4f); TestEqual(TEXT("One core hit per strike"),Core->GetHealth(),490.f);
    Attacker->CancelMeleeAttack(); Attacker->Destroy();

    // Real BT + navigation: no player, so the monster must advance to the core.
    AMonsterCharacter* Walker=SpawnMonster(FVector(2000,0,101),true);
    AMonsterAIController* AI=Cast<AMonsterAIController>(Walker->GetController());
    if(!TestNotNull(TEXT("Native AI controller"),AI)) return false;
    AI->SetDefenseObjective(Core);
    TestEqual(TEXT("Objective is selected in blackboard"),AI->GetBlackboardComponent()->GetValueAsObject(TEXT("TargetActor")),static_cast<UObject*>(Core));
    const float Before=Core->GetHealth();
    Advance(14.f);
    TestTrue(TEXT("Monster navigates to core and attacks it"),Core->GetHealth()<Before);
    Walker->Destroy();
    Core->Destroy();

    Core=World->SpawnActor<AColonyCore>(FVector(0,0,150),FRotator::ZeroRotator);
    ATargetPoint* Spawn=World->SpawnActor<ATargetPoint>(FVector(2000,0,100),FRotator::ZeroRotator);
    auto NewManager=[World,Core,Spawn]()
    {
        ADefensePrototypeManager* M=World->SpawnActorDeferred<ADefensePrototypeManager>(ADefensePrototypeManager::StaticClass(),FTransform::Identity);
        M->bAutoStart=false; M->Core=Core; M->SpawnPoints={Spawn}; M->MonsterClass=AMonsterCharacter::StaticClass();
        M->WaveSizes={2,1,1}; M->PreparationSeconds=.1f; M->IntermissionSeconds=.2f; M->SpawnInterval=.4f;
        M->FinishSpawning(FTransform::Identity); return M;
    };
    auto KillWave=[World](ADefensePrototypeManager* M)
    {
        TArray<AMonsterCharacter*> Enemies;
        // Possession makes the controller the pawn's Owner, so ownership is not
        // an identifier for wave membership. This isolated fixture contains
        // only this manager's enemies at each kill step.
        for(TActorIterator<AMonsterCharacter> It(World);It;++It) if(It->IsAlive()) Enemies.Add(*It);
        for(AMonsterCharacter* Enemy:Enemies) UGameplayStatics::ApplyDamage(Enemy,1000,nullptr,nullptr,UDamageType::StaticClass());
    };
    ADefensePrototypeManager* Manager=NewManager(); Manager->StartRun();
    Advance(.2f); TestEqual(TEXT("First wave begins"),Manager->GetWaveNumber(),1);
    TestEqual(TEXT("First enemy spawned"),Manager->GetAliveEnemyCount(),1);
    KillWave(Manager);
    TestTrue(TEXT("Killing early enemy cannot skip pending spawn"),Manager->GetPhase()==EDefensePhase::Wave);
    Advance(.5f); KillWave(Manager);
    TestTrue(TEXT("Cleared wave enters intermission"),Manager->GetPhase()==EDefensePhase::Intermission);
    Advance(.3f); TestEqual(TEXT("Second wave"),Manager->GetWaveNumber(),2); KillWave(Manager);
    Advance(.3f); TestEqual(TEXT("Third wave"),Manager->GetWaveNumber(),3); KillWave(Manager);
    TestTrue(TEXT("Third cleared wave wins"),Manager->GetPhase()==EDefensePhase::Victory);
    Advance(1.f); TestEqual(TEXT("Victory stops spawning"),Manager->GetAliveEnemyCount(),0);
    Manager->Destroy();

    Manager=NewManager(); Manager->StartRun(); Advance(.2f);
    AMonsterCharacter* Cause=SpawnMonster(FVector(3000,2000,100),false);
    UGameplayStatics::ApplyDamage(Core,10000,nullptr,Cause,UDamageType::StaticClass());
    TestEqual(TEXT("Core health clamps to zero"),Core->GetHealth(),0.f);
    TestTrue(TEXT("Core destruction loses immediately"),Manager->GetPhase()==EDefensePhase::Defeat);
    const int32 AliveAtDefeat=Manager->GetAliveEnemyCount();
    Advance(1.f); TestEqual(TEXT("Defeat cancels remaining spawns"),Manager->GetAliveEnemyCount(),AliveAtDefeat);
    KillWave(Manager); TestTrue(TEXT("No victory after defeat"),Manager->GetPhase()==EDefensePhase::Defeat);
    Cause->Destroy(); Manager->Destroy(); Core->Destroy();

    // A controlled living player overrides the stationary defense objective;
    // after that player dies the solo run must end and pending spawns must stop.
    Core=World->SpawnActor<AColonyCore>(FVector(0,0,150),FRotator::ZeroRotator);
    AMonsterCharacter* Watcher=SpawnMonster(FVector(1000,0,101),true);
    AMonsterAIController* WatchAI=Cast<AMonsterAIController>(Watcher->GetController());
    if(!TestNotNull(TEXT("Player-priority AI"),WatchAI)) return false;
    WatchAI->SetDefenseObjective(Core);
    UClass* PlayerClass=LoadClass<AMyProjectPrototypeCharacter>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter_C"));
    if(!TestNotNull(TEXT("Existing player BP"),PlayerClass)) return false;
    AMyProjectPrototypeCharacter* Player=World->SpawnActorDeferred<AMyProjectPrototypeCharacter>(PlayerClass,FTransform(FRotator::ZeroRotator,FVector(800,80,101)),nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    Player->AutoPossessAI=EAutoPossessAI::Disabled; Player->AutoPossessPlayer=EAutoReceiveInput::Disabled;
    Player->FinishSpawning(FTransform(FRotator::ZeroRotator,FVector(800,80,101)));
    APlayerController* PC=World->SpawnActor<APlayerController>();
    // This fixture has no GameMode to create a PlayerState automatically.
    PC->PlayerState=World->SpawnActor<APlayerState>(); PC->Possess(Player);
    TestTrue(TEXT("Priority fixture is a real player-controlled pawn"),Player->IsPlayerControlled());
    Watcher->SetActorRotation((Player->GetActorLocation()-Watcher->GetActorLocation()).Rotation());
    Advance(2.f);
    TestEqual(TEXT("Visible player has priority over core"),WatchAI->GetBlackboardComponent()->GetValueAsObject(TEXT("TargetActor")),static_cast<UObject*>(Player));
    TestTrue(TEXT("Priority target receives melee damage"),Player->GetHealth()<100.f);
    Watcher->Destroy();
    // NewManager captures the previous core, so configure this one explicitly.
    ADefensePrototypeManager* DeathRun=World->SpawnActorDeferred<ADefensePrototypeManager>(ADefensePrototypeManager::StaticClass(),FTransform::Identity);
    DeathRun->bAutoStart=false; DeathRun->Core=Core; DeathRun->SpawnPoints={Spawn}; DeathRun->MonsterClass=AMonsterCharacter::StaticClass();
    DeathRun->WaveSizes={2}; DeathRun->PreparationSeconds=.1f; DeathRun->SpawnInterval=.5f;
    DeathRun->FinishSpawning(FTransform::Identity); DeathRun->StartRun(); Advance(.2f);
    UGameplayStatics::ApplyDamage(Player,1000,nullptr,nullptr,UDamageType::StaticClass()); Advance(.2f);
    TestTrue(TEXT("Solo player death ends run"),DeathRun->GetPhase()==EDefensePhase::Defeat);
    const int32 EnemiesAtPlayerDeath=DeathRun->GetAliveEnemyCount(); Advance(1.f);
    TestEqual(TEXT("Player death cancels pending spawns"),DeathRun->GetAliveEnemyCount(),EnemiesAtPlayerDeath);
    return true;
}
#endif
