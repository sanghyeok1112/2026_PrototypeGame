#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "MonsterCharacter.h"
#include "MyProjectPrototypeCharacter.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMonsterCombatTest,
    "Prototype.MonsterCombat.DamageEvadeWallAndDeath",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMonsterCombatTest::RunTest(const FString& Parameters)
{
    const UWorld::InitializationValues IVS = UWorld::InitializationValues()
        .AllowAudioPlayback(false).RequiresHitProxies(false).CreatePhysicsScene(true)
        .CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false)
        .EnableTraceCollision(true).SetTransactional(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &IVS);
    if (!TestNotNull(TEXT("Test physics world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->EndPlay(EEndPlayReason::Quit); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
    World->InitializeActorsForPlay(FURL());
    World->GetWorldSettings()->NotifyBeginPlay();

    UClass* PlayerClass = LoadClass<AMyProjectPrototypeCharacter>(nullptr,
        TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter_C"));
    if (!TestNotNull(TEXT("Existing player blueprint"), PlayerClass)) return false;
    AMyProjectPrototypeCharacter* Player = World->SpawnActorDeferred<AMyProjectPrototypeCharacter>(PlayerClass,
        FTransform(FRotator::ZeroRotator, FVector(200,0,1000)), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!TestNotNull(TEXT("Player"), Player)) return false;
    Player->AutoPossessAI = EAutoPossessAI::Disabled;
    Player->AutoPossessPlayer = EAutoReceiveInput::Disabled;
    Player->FinishSpawning(FTransform(FRotator::ZeroRotator,FVector(200,0,1000)));
    Player->GetCharacterMovement()->DisableMovement();

    AMonsterCharacter* Monster = World->SpawnActorDeferred<AMonsterCharacter>(AMonsterCharacter::StaticClass(),
        FTransform(FRotator::ZeroRotator,FVector(0,0,1000)),nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!TestNotNull(TEXT("Monster"), Monster)) return false;
    Monster->AutoPossessAI = EAutoPossessAI::Disabled;
    Monster->FinishSpawning(FTransform(FRotator::ZeroRotator,FVector(0,0,1000)));
    Monster->GetCharacterMovement()->DisableMovement();
    auto Advance = [World](float Seconds)
    {
        for (int32 I=0; I<FMath::CeilToInt(Seconds/.025f); ++I)
        {
            ++GFrameCounter;
            World->Tick(LEVELTICK_All,.025f);
        }
    };
    Advance(.05f);
    TestEqual(TEXT("Initial player HP"),Player->GetHealth(),100.f);
    TestTrue(TEXT("Start in-range attack"),Monster->TryStartMeleeAttack(Player));
    Advance(.3f);
    TestEqual(TEXT("Windup causes no damage"),Player->GetHealth(),100.f);
    Advance(.3f);
    TestEqual(TEXT("Hit window applies one hit"),Player->GetHealth(),90.f);
    Advance(.5f);
    TestEqual(TEXT("No repeated damage during hit/recovery"),Player->GetHealth(),90.f);
    TestFalse(TEXT("Cooldown blocks early restart"),Monster->TryStartMeleeAttack(Player));
    Advance(.5f);
    TestTrue(TEXT("Cooldown eventually allows attack"),Monster->TryStartMeleeAttack(Player));
    Player->SetActorLocation(FVector(1000,0,1000),false,nullptr,ETeleportType::TeleportPhysics);
    Advance(1.1f);
    TestEqual(TEXT("Moving out during windup evades"),Player->GetHealth(),90.f);

    Advance(.5f);
    Player->SetActorLocation(FVector(200,0,1000),false,nullptr,ETeleportType::TeleportPhysics);
    TestTrue(TEXT("Start attack before cover appears"),Monster->TryStartMeleeAttack(Player));
    AStaticMeshActor* Wall = World->SpawnActor<AStaticMeshActor>();
    Wall->SetMobility(EComponentMobility::Movable);
    Wall->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
    Wall->SetActorLocation(FVector(100,0,1000));
    Wall->SetActorScale3D(FVector(.2f,3.f,4.f));
    Wall->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
    Advance(1.1f);
    TestEqual(TEXT("Cover blocks damage at impact time"),Player->GetHealth(),90.f);
    Wall->Destroy();
    Advance(.5f);

    TestTrue(TEXT("Start attack before lethal damage"),Monster->TryStartMeleeAttack(Player));
    UGameplayStatics::ApplyDamage(Monster,1000.f,nullptr,Player,UDamageType::StaticClass());
    Advance(.7f);
    TestEqual(TEXT("Attacker death cancels pending damage"),Player->GetHealth(),90.f);
    UGameplayStatics::ApplyDamage(Player,1000.f,nullptr,nullptr,UDamageType::StaticClass());
    TestEqual(TEXT("Player HP clamps to zero"),Player->GetHealth(),0.f);
    TestFalse(TEXT("Player death is exposed to targeting"),Player->IsAlive());
    TestEqual(TEXT("Dead player ignores further damage"),UGameplayStatics::ApplyDamage(Player,10.f,nullptr,nullptr,UDamageType::StaticClass()),0.f);
    return true;
}
#endif
