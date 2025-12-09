// F12BuilderHUD.h
// HUD Widget for F12 Station Builder
// 
// USAGE: Create a Widget Blueprint that inherits from this class,
// then design the UI visually. Name your widgets to match the
// BindWidget properties below and the C++ logic will drive them.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "F12BuilderHUD.generated.h"

class AF12BuilderController;
class UBorder;
class UTextBlock;

UCLASS()
class UF12BuilderHUD : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

protected:
    // =====================================================================
    // BIND WIDGETS - Create these in your Blueprint with matching names!
    // =====================================================================
    
    // The three mode buttons (Border widgets for colored backgrounds)
    UPROPERTY(meta = (BindWidget))
    UBorder* BuildButton;

    UPROPERTY(meta = (BindWidget))
    UBorder* PaintButton;

    UPROPERTY(meta = (BindWidget))
    UBorder* DeleteButton;

    // Mode label text (optional - use BindWidgetOptional if you might not have it)
    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* ModeLabel;

    // Paint material index display (optional)
    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* PaintIndexLabel;

    // =====================================================================
    // COLORS - Customize these in the Blueprint defaults
    // =====================================================================
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Colors")
    FLinearColor BuildActiveColor = FLinearColor(0.2f, 0.5f, 1.0f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Colors")
    FLinearColor DeleteActiveColor = FLinearColor(0.9f, 0.2f, 0.2f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Colors")
    FLinearColor InactiveColor = FLinearColor(0.15f, 0.15f, 0.15f, 0.85f);

    // =====================================================================
    // INTERNAL
    // =====================================================================
    
    UPROPERTY()
    AF12BuilderController* CachedController;

    // Find and cache the controller reference
    AF12BuilderController* GetBuilderController();

    // Update all displays
    void UpdateDisplay();
};