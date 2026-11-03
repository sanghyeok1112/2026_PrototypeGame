#include "ColonyCore.h"
#include "MonsterCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "UObject/ConstructorHelpers.h"

AColonyCore::AColonyCore()
{
    PrimaryActorTick.bCanEverTick=false;
    Collision=CreateDefaultSubobject<UCapsuleComponent>(TEXT("CoreCollision"));
    SetRootComponent(Collision);
    Collision->InitCapsuleSize(120.f,150.f);
    Collision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Collision->SetCollisionResponseToChannel(ECC_Camera,ECR_Ignore);
    Collision->SetCanEverAffectNavigation(false);
    CoreMesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CoreMesh"));
    CoreMesh->SetupAttachment(Collision);
    CoreMesh->SetRelativeScale3D(FVector(2.1f,2.1f,2.8f));
    CoreMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if(Cube.Succeeded()) CoreMesh->SetStaticMesh(Cube.Object);
    HealthLabel=CreateDefaultSubobject<UTextRenderComponent>(TEXT("CoreHealthLabel"));
    HealthLabel->SetupAttachment(Collision);
    HealthLabel->SetRelativeLocation(FVector(0,0,240));
    HealthLabel->SetRelativeRotation(FRotator(0,-90,0));
    HealthLabel->SetHorizontalAlignment(EHTA_Center);
    HealthLabel->SetWorldSize(55);
    HealthLabel->SetTextRenderColor(FColor::Cyan);
    HealthLabel->SetText(FText::FromString(TEXT("COLONY CORE")));
}
void AColonyCore::BeginPlay()
{
    Super::BeginPlay();
    CurrentHealth=FMath::Max(1.f,MaxHealth);
    RefreshLabel();
}
float AColonyCore::GetTargetRadius() const { return Collision->GetScaledCapsuleRadius(); }
void AColonyCore::RefreshLabel()
{
    HealthLabel->SetText(FText::FromString(FString::Printf(TEXT("COLONY CORE\n%.0f / %.0f"),CurrentHealth,MaxHealth)));
    HealthLabel->SetTextRenderColor(IsAlive()?FColor::Cyan:FColor::Red);
}
float AColonyCore::TakeDamage(float DamageAmount,const FDamageEvent& DamageEvent,AController* EventInstigator,AActor* DamageCauser)
{
    // The player's weapon traces can hit the core, but cannot damage it.
    if(!bDamageEnabled || !IsAlive() || !Cast<AMonsterCharacter>(DamageCauser)
        || !FMath::IsFinite(DamageAmount) || DamageAmount<=0.f) return 0.f;
    const float Applied=FMath::Min(CurrentHealth,DamageAmount);
    CurrentHealth-=Applied;
    RefreshLabel();
    UE_LOG(LogTemp,Display,TEXT("DEFENSE_CORE HP=%.0f/%.0f Damage=%.0f"),CurrentHealth,MaxHealth,Applied);
    if(!IsAlive()) OnCoreDestroyed.Broadcast();
    return Applied;
}
