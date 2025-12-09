// F12BuilderHUD.cpp
// Implementation of the HUD Widget

#include "F12BuilderHUD.h"
#include "F12BuilderController.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"

void UF12BuilderHUD::NativeConstruct()
{
    Super::NativeConstruct();
    
    // Log which widgets were found
    UE_LOG(LogTemp, Log, TEXT("HUD Construct - BuildButton: %s, PaintButton: %s, DeleteButton: %s, ModeLabel: %s"),
        BuildButton ? TEXT("Found") : TEXT("NULL"),
        PaintButton ? TEXT("Found") : TEXT("NULL"),
        DeleteButton ? TEXT("Found") : TEXT("NULL"),
        ModeLabel ? TEXT("Found") : TEXT("NULL"));
    
    // Initial update
    UpdateDisplay();
}

void UF12BuilderHUD::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    
    // Update every frame to stay in sync with controller
    UpdateDisplay();
}

AF12BuilderController* UF12BuilderHUD::GetBuilderController()
{
    if (!CachedController)
    {
        APlayerController* PC = GetOwningPlayer();
        if (PC)
        {
            CachedController = Cast<AF12BuilderController>(PC);
            if (CachedController)
            {
                UE_LOG(LogTemp, Log, TEXT("HUD: Found controller, PaintColors count: %d"), 
                    CachedController->PaintColors.Num());
            }
        }
    }
    return CachedController;
}

void UF12BuilderHUD::UpdateDisplay()
{
    AF12BuilderController* Controller = GetBuilderController();
    if (!Controller)
        return;

    EF12BuilderMode CurrentMode = Controller->CurrentMode;
    bool bIsBuild = (CurrentMode == EF12BuilderMode::Build);
    bool bIsPaint = (CurrentMode == EF12BuilderMode::Paint);
    bool bIsDelete = (CurrentMode == EF12BuilderMode::Delete);

    // === UPDATE BUILD BUTTON ===
    if (BuildButton)
    {
        FLinearColor BgColor = bIsBuild ? BuildActiveColor : InactiveColor;
        BuildButton->SetBrushColor(BgColor);
        
        // Scale up active button slightly for visual feedback
        BuildButton->SetRenderScale(bIsBuild ? FVector2D(1.1f, 1.1f) : FVector2D(1.0f, 1.0f));
        
        // Make active button content brighter
        BuildButton->SetContentColorAndOpacity(bIsBuild ? FLinearColor::White : FLinearColor(0.7f, 0.7f, 0.7f, 1.0f));
    }

    // === UPDATE PAINT BUTTON ===
    if (PaintButton)
    {
        FLinearColor BgColor;
        if (bIsPaint)
        {
            // In paint mode, show current paint color
            BgColor = Controller->GetCurrentPaintColor();
        }
        else
        {
            BgColor = InactiveColor;
        }
        
        PaintButton->SetBrushColor(BgColor);
        PaintButton->SetRenderScale(bIsPaint ? FVector2D(1.1f, 1.1f) : FVector2D(1.0f, 1.0f));
        PaintButton->SetContentColorAndOpacity(bIsPaint ? FLinearColor::White : FLinearColor(0.7f, 0.7f, 0.7f, 1.0f));
    }

    // === UPDATE DELETE BUTTON ===
    if (DeleteButton)
    {
        FLinearColor BgColor = bIsDelete ? DeleteActiveColor : InactiveColor;
        DeleteButton->SetBrushColor(BgColor);
        DeleteButton->SetRenderScale(bIsDelete ? FVector2D(1.1f, 1.1f) : FVector2D(1.0f, 1.0f));
        DeleteButton->SetContentColorAndOpacity(bIsDelete ? FLinearColor::White : FLinearColor(0.7f, 0.7f, 0.7f, 1.0f));
    }

    // === UPDATE MODE LABEL ===
    if (ModeLabel)
    {
        FString ModeName;
        switch (CurrentMode)
        {
            case EF12BuilderMode::Build:  ModeName = TEXT("BUILD MODE"); break;
            case EF12BuilderMode::Paint:  ModeName = TEXT("PAINT MODE"); break;
            case EF12BuilderMode::Delete: ModeName = TEXT("DELETE MODE"); break;
            default: ModeName = TEXT("UNKNOWN"); break;
        }
        ModeLabel->SetText(FText::FromString(ModeName));
    }

    // === UPDATE PAINT INDEX LABEL ===
    if (PaintIndexLabel)
    {
        if (bIsPaint)
        {
            FString IndexText = FString::Printf(TEXT("Material: %d"), Controller->CurrentPaintMaterialIndex + 1);
            PaintIndexLabel->SetText(FText::FromString(IndexText));
            PaintIndexLabel->SetVisibility(ESlateVisibility::Visible);
        }
        else
        {
            PaintIndexLabel->SetVisibility(ESlateVisibility::Hidden);
        }
    }
}