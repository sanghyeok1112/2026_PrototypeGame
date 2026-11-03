// Fill out your copyright notice in the Description page of Project Settings.


#include "GunWeapon.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"

void AGunWeapon::Attack()
{
    AActor* WeaponOwner = GetOwner();
    if (!WeaponOwner)
    {
        return;
    }

    UCameraComponent* Camera =
        WeaponOwner->FindComponentByClass<UCameraComponent>();

    if (!Camera)
    {
        return;
    }

    const FVector Start = Camera->GetComponentLocation();
    const FVector End =
        Start + Camera->GetForwardVector() * 10000.f;

    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);
    QueryParams.AddIgnoredActor(WeaponOwner);

    FHitResult Hit;

    const bool bHit = GetWorld()->LineTraceSingleByChannel(
        Hit,
        Start,
        End,
        ECC_Visibility,
        QueryParams);

    const FVector TraceEnd = bHit ? Hit.ImpactPoint : End;

    DrawDebugLine(
        GetWorld(),
        Start,
        TraceEnd,
        FColor::Red,
        false,
        2.f,
        0,
        1.f);

    if (bHit)
    {
        DrawDebugSphere(
            GetWorld(),
            Hit.ImpactPoint,
            8.f,
            12,
            FColor::Green,
            false,
            2.f);

        if (AActor* HitActor = Hit.GetActor())
        {
            UGameplayStatics::ApplyDamage(
                HitActor,
                25.f,
                GetInstigatorController(),
                this,
                UDamageType::StaticClass());
        }
    }
}