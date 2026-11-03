#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DefensePrototypeWidget.generated.h"
class ADefensePrototypeManager;
class UTextBlock;
class UProgressBar;
class UButton;
UCLASS()
class MYPROJECTPROTOTYPE_API UDefensePrototypeWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UPROPERTY() TObjectPtr<ADefensePrototypeManager> Manager;
    void Refresh();
protected:
    virtual void NativeOnInitialized() override;
private:
    UPROPERTY() TObjectPtr<UTextBlock> Status;
    UPROPERTY() TObjectPtr<UTextBlock> CoreHealth;
    UPROPERTY() TObjectPtr<UTextBlock> Hint;
    UPROPERTY() TObjectPtr<UProgressBar> CoreBar;
    UPROPERTY() TObjectPtr<UButton> Retry;
    UFUNCTION() void OnRetry();
};
