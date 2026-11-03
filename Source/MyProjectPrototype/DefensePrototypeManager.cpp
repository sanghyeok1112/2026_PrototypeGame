#include "DefensePrototypeManager.h"
#include "ColonyCore.h"
#include "Components/InputComponent.h"
#include "InputCoreTypes.h"
#include "MonsterCharacter.h"
#include "MonsterAIController.h"
#include "MyProjectPrototypeCharacter.h"
#include "MyProjectPrototypePlayerController.h"
#include "BrainComponent.h"
#include "Engine/TargetPoint.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "TimerManager.h"

ADefensePrototypeManager::ADefensePrototypeManager()
{
    PrimaryActorTick.bCanEverTick=true;
    PrimaryActorTick.TickInterval=.1f;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("DefenseRoot")));
}
void ADefensePrototypeManager::BeginPlay()
{
    Super::BeginPlay();
    if(IsValid(Core)) Core->OnCoreDestroyed.AddDynamic(this,&ThisClass::OnCoreDestroyed);
    UpdateStatusDisplay();
    if(bAutoStart) StartRun();
}
void ADefensePrototypeManager::StartRun()
{
    if(Phase!=EDefensePhase::Waiting) return;
    if(!IsValid(Core) || !Core->IsAlive() || WaveSizes.IsEmpty() || SpawnPoints.IsEmpty() || !MonsterClass)
    { FinishRun(false,TEXT("방어 목표 또는 스폰 설정을 확인해 주세요.")); return; }
    for(const int32 Size:WaveSizes) if(Size<=0)
    { FinishRun(false,TEXT("웨이브 수는 1 이상이어야 합니다.")); return; }
    Core->SetDamageEnabled(true);
    Phase=EDefensePhase::Preparation;
    PhaseEndsAt=GetWorld()->GetTimeSeconds()+FMath::Max(.1f,PreparationSeconds);
    GetWorldTimerManager().SetTimer(PhaseTimer,this,&ThisClass::BeginWave,FMath::Max(.1f,PreparationSeconds),false);
    UE_LOG(LogTemp,Display,TEXT("DEFENSE_RUN Preparation waves=%d core=%.0f"),WaveSizes.Num(),Core->GetHealth());
}
void ADefensePrototypeManager::BeginWave()
{
    if(IsFinished()) return;
    if(!IsValid(Core) || !Core->IsAlive()) { OnCoreDestroyed(); return; }
    ++WaveIndex;
    if(!WaveSizes.IsValidIndex(WaveIndex)) { FinishRun(true,TEXT("모든 웨이브를 막았습니다!")); return; }
    Phase=EDefensePhase::Wave;
    PendingSpawns=WaveSizes[WaveIndex];
    FailedSpawnAttempts=0;
    UE_LOG(LogTemp,Display,TEXT("DEFENSE_WAVE Start=%d quota=%d"),WaveIndex+1,PendingSpawns);
    GetWorldTimerManager().SetTimer(SpawnTimer,this,&ThisClass::SpawnNextEnemy,FMath::Max(.1f,SpawnInterval),true);
    SpawnNextEnemy();
}
void ADefensePrototypeManager::SpawnNextEnemy()
{
    if(Phase!=EDefensePhase::Wave || PendingSpawns<=0) return;
    UNavigationSystemV1* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    bool bSpawned=false;
    for(int32 Attempt=0;Attempt<SpawnPoints.Num() && !bSpawned;++Attempt)
    {
        ATargetPoint* Point=SpawnPoints[(SpawnSerial+Attempt)%SpawnPoints.Num()];
        if(!IsValid(Point) || !Nav || !IsValid(Core)) continue;
        FNavLocation Ground;
        if(!Nav->ProjectPointToNavigation(Point->GetActorLocation(),Ground,FVector(250,250,500))) continue;
        UNavigationPath* Path=UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(),Ground.Location,Core->GetActorLocation());
        if(!Path || !Path->IsValid() || Path->IsPartial()) continue;
        FActorSpawnParameters Params;
        Params.Owner=this;
        Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
        const float Height=MonsterClass->GetDefaultObject<AMonsterCharacter>()->GetSimpleCollisionHalfHeight();
        AMonsterCharacter* Enemy=GetWorld()->SpawnActor<AMonsterCharacter>(MonsterClass,Ground.Location+FVector(0,0,Height+5),
            (Core->GetActorLocation()-Ground.Location).Rotation(),Params);
        if(!Enemy) continue;
        if(!Enemy->GetController()) Enemy->SpawnDefaultController();
        AMonsterAIController* AI=Cast<AMonsterAIController>(Enemy->GetController());
        if(!AI || !AI->GetBlackboardComponent()) { Enemy->Destroy(); continue; }
        AI->SetDefenseObjective(Core);
        Enemy->OnDestroyed.AddDynamic(this,&ThisClass::OnEnemyDestroyed);
        ActiveEnemies.Add(Enemy);
        --PendingSpawns;
        ++SpawnSerial;
        bSpawned=true;
        FailedSpawnAttempts=0;
        UE_LOG(LogTemp,Display,TEXT("DEFENSE_SPAWN wave=%d enemy=%s point=%s pending=%d"),WaveIndex+1,*Enemy->GetName(),*Point->GetName(),PendingSpawns);
    }
    if(!bSpawned && ++FailedSpawnAttempts>=5)
    { FinishRun(false,TEXT("몬스터 생성 실패: 스폰 위치와 이동 경로를 확인해 주세요.")); return; }
    if(PendingSpawns==0) GetWorldTimerManager().ClearTimer(SpawnTimer);
}
int32 ADefensePrototypeManager::GetAliveEnemyCount() const
{
    int32 Count=0;
    for(const TWeakObjectPtr<AMonsterCharacter>& Enemy:ActiveEnemies) if(Enemy.IsValid() && Enemy->IsAlive()) ++Count;
    return Count;
}
float ADefensePrototypeManager::GetCountdown() const
{
    return (Phase==EDefensePhase::Preparation || Phase==EDefensePhase::Intermission)
        ? FMath::Max(0.f,static_cast<float>(PhaseEndsAt-GetWorld()->GetTimeSeconds())) : 0.f;
}
void ADefensePrototypeManager::OnEnemyDestroyed(AActor* DestroyedActor)
{
    ActiveEnemies.RemoveAll([DestroyedActor](const TWeakObjectPtr<AMonsterCharacter>& Enemy)
        { return !Enemy.IsValid() || Enemy.Get()==DestroyedActor; });
    CheckWaveCleared();
}
void ADefensePrototypeManager::CheckWaveCleared()
{
    if(Phase!=EDefensePhase::Wave || PendingSpawns>0 || GetAliveEnemyCount()>0) return;
    if(!IsValid(Core) || !Core->IsAlive()) { OnCoreDestroyed(); return; }
    if(WaveIndex+1==WaveSizes.Num()) { FinishRun(true,TEXT("모든 웨이브를 막았습니다!")); return; }
    Phase=EDefensePhase::Intermission;
    PhaseEndsAt=GetWorld()->GetTimeSeconds()+FMath::Max(.1f,IntermissionSeconds);
    GetWorldTimerManager().SetTimer(PhaseTimer,this,&ThisClass::BeginWave,FMath::Max(.1f,IntermissionSeconds),false);
    UE_LOG(LogTemp,Display,TEXT("DEFENSE_WAVE Cleared=%d"),WaveIndex+1);
}
void ADefensePrototypeManager::OnCoreDestroyed() { FinishRun(false,TEXT("콜로니 코어가 파괴되었습니다.")); }
void ADefensePrototypeManager::FinishRun(bool bWon,const FString& Reason)
{
    if(IsFinished()) return;
    Phase=bWon?EDefensePhase::Victory:EDefensePhase::Defeat;
    ResultReason=Reason;
    GetWorldTimerManager().ClearTimer(PhaseTimer);
    GetWorldTimerManager().ClearTimer(SpawnTimer);
    if(IsValid(Core)) Core->SetDamageEnabled(false);
    for(const TWeakObjectPtr<AMonsterCharacter>& Enemy:ActiveEnemies) if(Enemy.IsValid())
    {
        Enemy->CancelMeleeAttack();
        Enemy->GetCharacterMovement()->StopMovementImmediately();
        Enemy->GetCharacterMovement()->DisableMovement();
        if(AMonsterAIController* AI=Cast<AMonsterAIController>(Enemy->GetController())) AI->StopForDefenseResult();
    }
    if(APlayerController* PC=UGameplayStatics::GetPlayerController(this,0))
    {
        if(APawn* Pawn=PC->GetPawn()) Pawn->DisableInput(PC);
        PC->SetIgnoreMoveInput(true);
        PC->bShowMouseCursor=false;
        PC->SetInputMode(FInputModeGameOnly());
        EnableInput(PC);
        if(InputComponent) InputComponent->BindKey(EKeys::R,IE_Pressed,this,&ThisClass::RestartRun);
        if(AMyProjectPrototypePlayerController* TPS=Cast<AMyProjectPrototypePlayerController>(PC)) TPS->SetCrosshairVisible(false);
    }
    UE_LOG(LogTemp,Display,TEXT("DEFENSE_RESULT %s reason=%s"),bWon?TEXT("VICTORY"):TEXT("DEFEAT"),*Reason);
}
void ADefensePrototypeManager::UpdateStatusDisplay()
{
    APlayerController* PC=UGameplayStatics::GetPlayerController(this,0);
    if(!PC || !PC->IsLocalController() || !GEngine || !GEngine->GameViewport || IsRunningCommandlet() || GetWorld()->GetNetMode()==NM_DedicatedServer) return;
    FString Status=FString::Printf(TEXT("CORE %.0f / %.0f | WAVE %d / %d | ENEMIES %d"),
        IsValid(Core)?Core->GetHealth():0.f,IsValid(Core)?Core->GetMaxHealth():0.f,
        FMath::Max(1,GetWaveNumber()),GetWaveCount(),GetAliveEnemyCount()+GetUnspawnedCount());
    if(Phase==EDefensePhase::Preparation) Status+=FString::Printf(TEXT(" | START %ds"),FMath::CeilToInt(GetCountdown()));
    else if(Phase==EDefensePhase::Intermission) Status+=FString::Printf(TEXT(" | NEXT %ds"),FMath::CeilToInt(GetCountdown()));
    else if(IsFinished()) Status+=FString::Printf(TEXT("\n%s - %s | R: Restart"),
        Phase==EDefensePhase::Victory?TEXT("VICTORY"):TEXT("DEFEAT"),*ResultReason);
    GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()),1.f,
        Phase==EDefensePhase::Defeat?FColor::Red:FColor::White,Status,false);
}
void ADefensePrototypeManager::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if(Phase!=EDefensePhase::Waiting && !IsFinished())
    {
        if(!IsValid(Core) || !Core->IsAlive()) OnCoreDestroyed();
        bool bFoundPlayer=false,bAnyAlive=false;
        for(FConstPlayerControllerIterator It=GetWorld()->GetPlayerControllerIterator();It;++It)
            if(APlayerController* PC=It->Get()) if(AMyProjectPrototypeCharacter* Player=Cast<AMyProjectPrototypeCharacter>(PC->GetPawn()))
            { bFoundPlayer=true; bAnyAlive|=Player->IsAlive(); }
        if(bFoundPlayer && !bAnyAlive) FinishRun(false,TEXT("플레이어가 쓰러졌습니다."));
        CheckWaveCleared();
    }
    UpdateStatusDisplay();
}
void ADefensePrototypeManager::RestartRun()
{
    if(!IsFinished()) return;
    if(APlayerController* PC=UGameplayStatics::GetPlayerController(this,0))
    {
        PC->ResetIgnoreMoveInput(); PC->ResetIgnoreLookInput();
        PC->bShowMouseCursor=false; PC->SetInputMode(FInputModeGameOnly());
    }
    UGameplayStatics::OpenLevel(this,FName(*UGameplayStatics::GetCurrentLevelName(this,true)));
}
void ADefensePrototypeManager::EndPlay(const EEndPlayReason::Type Reason)
{
    GetWorldTimerManager().ClearTimer(PhaseTimer);
    GetWorldTimerManager().ClearTimer(SpawnTimer);
    if(IsValid(Core)) Core->OnCoreDestroyed.RemoveDynamic(this,&ThisClass::OnCoreDestroyed);
    for(const TWeakObjectPtr<AMonsterCharacter>& Enemy:ActiveEnemies) if(Enemy.IsValid())
        Enemy->OnDestroyed.RemoveDynamic(this,&ThisClass::OnEnemyDestroyed);
    if(GEngine) GEngine->RemoveOnScreenDebugMessage(GetUniqueID());
    Super::EndPlay(Reason);
}
