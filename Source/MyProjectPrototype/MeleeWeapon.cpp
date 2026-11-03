// Fill out your copyright notice in the Description page of Project Settings.


#include "MeleeWeapon.h"
#include "Engine/World.h"
#include "CollisionShape.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"

void AMeleeWeapon::Attack()
{
    AActor* WeaponOwner = GetOwner();
    if (!WeaponOwner)
    {
        return;
    }

    const FVector Forward = WeaponOwner->GetActorForwardVector();

    const FVector Start =
        WeaponOwner->GetActorLocation() + Forward * 50.f;

    const FVector End = Start + Forward * 150.f;
    const float Radius = 40.f;

    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);
    QueryParams.AddIgnoredActor(WeaponOwner);

    FHitResult Hit;

    const bool bHit = GetWorld()->SweepSingleByChannel(
        Hit,
        Start,
        End,
        FQuat::Identity,
        ECC_Visibility,
        FCollisionShape::MakeSphere(Radius),
        QueryParams);

    const FVector Center = (Start + End) * 0.5f;
    const float HalfHeight =
        FVector::Distance(Start, End) * 0.5f + Radius;

    const FColor DebugColor = bHit
        ? FColor::Green
        : FColor::Red;

    DrawDebugCapsule(
        GetWorld(),
        Center,
        HalfHeight,
        Radius,
        FQuat::FindBetweenNormals(FVector::UpVector, Forward),
        DebugColor,
        false,
        2.f);

    if (bHit)
    {
        DrawDebugSphere(
            GetWorld(),
            Hit.ImpactPoint,
            8.f,
            12,
            FColor::Yellow,
            false,
            2.f);

        if (AActor* HitActor = Hit.GetActor())
        {
            UGameplayStatics::ApplyDamage(
                HitActor,
                40.f,
                GetInstigatorController(),
                this,
                UDamageType::StaticClass());
        }
    }
}