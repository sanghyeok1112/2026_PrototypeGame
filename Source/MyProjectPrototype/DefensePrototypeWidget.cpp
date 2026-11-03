#include "DefensePrototypeWidget.h"
#include "DefensePrototypeManager.h"
#include "ColonyCore.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Button.h"

void UDefensePrototypeWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    UCanvasPanel* Canvas=WidgetTree->ConstructWidget<UCanvasPanel>();
    WidgetTree->RootWidget=Canvas;
    UBorder* Panel=WidgetTree->ConstructWidget<UBorder>();
    Panel->SetBrushColor(FLinearColor(.015f,.025f,.035f,.86f));
    Panel->SetPadding(FMargin(20));
    UCanvasPanelSlot* Position=Canvas->AddChildToCanvas(Panel);
    Position->SetPosition(FVector2D(28,28));
    Position->SetSize(FVector2D(460,230));
    UVerticalBox* List=WidgetTree->ConstructWidget<UVerticalBox>(); Panel->SetContent(List);
    auto Text=[&](const FString& Value,int32 Size)
    {
        UTextBlock* Block=WidgetTree->ConstructWidget<UTextBlock>();
        FSlateFontInfo Font=Block->GetFont(); Font.Size=Size; Block->SetFont(Font);
        Block->SetText(FText::FromString(Value));
        List->AddChildToVerticalBox(Block)->SetPadding(FMargin(0,0,0,8));
        return Block;
    };
    Status=Text(TEXT("콜로니 방어"),24);
    CoreHealth=Text(TEXT("코어 체력"),18);
    CoreBar=WidgetTree->ConstructWidget<UProgressBar>();
    CoreBar->SetFillColorAndOpacity(FLinearColor(.1f,.8f,.8f));
    List->AddChildToVerticalBox(CoreBar)->SetPadding(FMargin(0,0,0,10));
    Hint=Text(TEXT("광산 앞 코어를 지키세요. 마우스 왼쪽: 공격 / 1·2: 무기 교체"),15);
    Hint->SetAutoWrapText(true);
    Retry=WidgetTree->ConstructWidget<UButton>();
    UTextBlock* RetryLabel=WidgetTree->ConstructWidget<UTextBlock>();
    RetryLabel->SetText(FText::FromString(TEXT("다시 시작")));
    Retry->SetContent(RetryLabel);
    Retry->OnClicked.AddDynamic(this,&ThisClass::OnRetry);
    List->AddChildToVerticalBox(Retry);
    Retry->SetVisibility(ESlateVisibility::Collapsed);
}
void UDefensePrototypeWidget::Refresh()
{
    if(!Manager || !Status) return;
    const EDefensePhase Phase=Manager->GetPhase();
    FString Message;
    switch(Phase)
    {
    case EDefensePhase::Preparation: Message=FString::Printf(TEXT("방어 준비 · %d초 후 시작"),FMath::CeilToInt(Manager->GetCountdown())); break;
    case EDefensePhase::Wave: Message=FString::Printf(TEXT("웨이브 %d / %d · 남은 적 %d"),Manager->GetWaveNumber(),Manager->GetWaveCount(),Manager->GetAliveEnemyCount()+Manager->GetUnspawnedCount()); break;
    case EDefensePhase::Intermission: Message=FString::Printf(TEXT("웨이브 %d 완료 · 다음까지 %d초"),Manager->GetWaveNumber(),FMath::CeilToInt(Manager->GetCountdown())); break;
    case EDefensePhase::Victory: Message=TEXT("방어 성공!"); break;
    case EDefensePhase::Defeat: Message=TEXT("방어 실패"); break;
    default: Message=TEXT("방어 준비 중"); break;
    }
    Status->SetText(FText::FromString(Message));
    Status->SetColorAndOpacity(Phase==EDefensePhase::Defeat?FLinearColor(1,.3f,.25f):FLinearColor::White);
    if(IsValid(Manager->Core))
    {
        CoreHealth->SetText(FText::FromString(FString::Printf(TEXT("콜로니 코어  %.0f / %.0f"),Manager->Core->GetHealth(),Manager->Core->GetMaxHealth())));
        CoreBar->SetPercent(Manager->Core->GetHealth()/FMath::Max(1.f,Manager->Core->GetMaxHealth()));
    }
    if(Manager->IsFinished()) Hint->SetText(FText::FromString(Manager->GetResultReason()));
    Retry->SetVisibility(Manager->IsFinished()?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
}
void UDefensePrototypeWidget::OnRetry() { if(Manager) Manager->RestartRun(); }
