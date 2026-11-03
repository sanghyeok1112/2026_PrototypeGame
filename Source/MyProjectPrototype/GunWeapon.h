// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WeaponBase.h"
#include "GunWeapon.generated.h"

/**
 * 
 */
UCLASS()
class MYPROJECTPROTOTYPE_API AGunWeapon : public AWeaponBase
{
	GENERATED_BODY()
	
public:
	virtual void Attack() override;
};
