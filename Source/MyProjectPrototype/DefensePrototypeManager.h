#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DefensePrototypeManager.generated.h"

class AColonyCore;
class AMonsterCharacter;
class ATargetPoint;
UENUM(BlueprintType)
enum class EDefensePhase : uint8 { Waiting, Preparation, Wave, Intermission, Victory, Defeat };

/** One local prototype run; only enemies spawned by this actor count towards its waves. */
UCLASS()
class MYPROJECTPROTOTYPE_API ADefensePrototypeManager : public AActor
{
    GENERATED_BODY()
public:
    ADefensePrototypeManager();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    UFUNCTION(BlueprintCallable) void StartRun();
    UFUNCTION(BlueprintCallable) void RestartRun();
    EDefensePhase GetPhase() const { return Phase; }
    bool IsFinished() const { return Phase==EDefensePhase::Victory || Phase==EDefensePhase::Defeat; }
    int32 GetWaveNumber() const { return WaveIndex+1; }
    int32 GetWaveCount() const { return WaveSizes.Num(); }
    int32 GetAliveEnemyCount() const;
    int32 GetUnspawnedCount() const { return FMath::Max(0,PendingSpawns); }
    float GetCountdown() const;
    const FString& GetResultReason() const { return ResultReason; }
    UPROPERTY(EditInstanceOnly, Category="Defense") TObjectPtr<AColonyCore> Core;
    UPROPERTY(EditInstanceOnly, Category="Defense") TArray<TObjectPtr<ATargetPoint>> SpawnPoints;
    UPROPERTY(EditAnywhere, Category="Defense") TSubclassOf<AMonsterCharacter> MonsterClass;
    UPROPERTY(EditAnywhere, Category="Defense") TArray<int32> WaveSizes={3,5,7};
    UPROPERTY(EditAnywhere, Category="Defense",meta=(ClampMin="0.1")) float PreparationSeconds=10.f;
    UPROPERTY(EditAnywhere, Category="Defense",meta=(ClampMin="0.1")) float IntermissionSeconds=8.f;
    UPROPERTY(EditAnywhere, Category="Defense",meta=(ClampMin="0.1")) float SpawnInterval=2.f;
    UPROPERTY(EditAnywhere, Category="Defense") bool bAutoStart=true;
private:
    UPROPERTY(VisibleInstanceOnly) EDefensePhase Phase=EDefensePhase::Waiting;
    TArray<TWeakObjectPtr<AMonsterCharacter>> ActiveEnemies;
    int32 WaveIndex=-1;
    int32 PendingSpawns=0;
    int32 SpawnSerial=0;
    int32 FailedSpawnAttempts=0;
    double PhaseEndsAt=0.;
    FString ResultReason;
    FTimerHandle PhaseTimer;
    FTimerHandle SpawnTimer;
    void BeginWave();
    void SpawnNextEnemy();
    void CheckWaveCleared();
    void FinishRun(bool bWon,const FString& Reason);
    UFUNCTION() void OnEnemyDestroyed(AActor* DestroyedActor);
    UFUNCTION() void OnCoreDestroyed();
    void UpdateStatusDisplay();
};
