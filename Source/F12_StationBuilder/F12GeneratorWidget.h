// F12GeneratorWidget.h
// UI Widget for Procedural Generation Controls

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "F12ProceduralGenerator.h"
#include "F12GeneratorWidget.generated.h"

class UButton;
class UTextBlock;
class UComboBoxString;
class USpinBox;
class UCheckBox;
class USlider;
class UBorder;

/**
 * Widget for controlling procedural generation
 * Create a Widget Blueprint with this as the parent class and add the required widgets
 */
UCLASS()
class UF12GeneratorWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    // === BIND WIDGETS ===
    // Create widgets with these names in your Blueprint
    
    // Shape selector
    UPROPERTY(meta = (BindWidgetOptional))
    UComboBoxString* ShapeCombo;

    // Size controls
    UPROPERTY(meta = (BindWidgetOptional))
    USpinBox* SizeXSpinBox;

    UPROPERTY(meta = (BindWidgetOptional))
    USpinBox* SizeYSpinBox;

    UPROPERTY(meta = (BindWidgetOptional))
    USpinBox* SizeZSpinBox;

    // Wall thickness
    UPROPERTY(meta = (BindWidgetOptional))
    USpinBox* ThicknessSpinBox;

    // Offset controls
    UPROPERTY(meta = (BindWidgetOptional))
    USpinBox* OffsetXSpinBox;

    UPROPERTY(meta = (BindWidgetOptional))
    USpinBox* OffsetYSpinBox;

    UPROPERTY(meta = (BindWidgetOptional))
    USpinBox* OffsetZSpinBox;

    // Options
    UPROPERTY(meta = (BindWidgetOptional))
    UCheckBox* CenterCheckBox;

    UPROPERTY(meta = (BindWidgetOptional))
    UCheckBox* ClearExistingCheckBox;

    UPROPERTY(meta = (BindWidgetOptional))
    UCheckBox* PreserveCoreCheckBox;

    // Buttons
    UPROPERTY(meta = (BindWidgetOptional))
    UButton* GenerateButton;

    UPROPERTY(meta = (BindWidgetOptional))
    UButton* ClearAllButton;

    UPROPERTY(meta = (BindWidgetOptional))
    UButton* CloseButton;

    // Info display
    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* EstimateText;

    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* StatusText;

    // Main panel (for showing/hiding)
    UPROPERTY(meta = (BindWidgetOptional))
    UBorder* MainPanel;

    // === FUNCTIONS ===
    
    UFUNCTION(BlueprintCallable, Category = "Generator")
    void ShowPanel();

    UFUNCTION(BlueprintCallable, Category = "Generator")
    void HidePanel();

    UFUNCTION(BlueprintCallable, Category = "Generator")
    void TogglePanel();

    UFUNCTION(BlueprintCallable, Category = "Generator")
    bool IsPanelVisible() const;

    // Get current parameters from UI
    UFUNCTION(BlueprintCallable, Category = "Generator")
    FF12GenerationParams GetCurrentParams() const;

    // Update estimate display
    UFUNCTION(BlueprintCallable, Category = "Generator")
    void UpdateEstimate();

protected:
    UPROPERTY()
    class AF12BuilderController* CachedController;

    UPROPERTY()
    UF12ProceduralGenerator* Generator;

    bool bPanelVisible = false;
    float EstimateUpdateTimer = 0.0f;

    // Cached params for estimate throttling
    FF12GenerationParams LastEstimateParams;
    int32 LastEstimate = 0;

    // Get controller reference
    class AF12BuilderController* GetBuilderController();

    // Button handlers
    UFUNCTION()
    void OnGenerateClicked();

    UFUNCTION()
    void OnClearAllClicked();

    UFUNCTION()
    void OnCloseClicked();

    // Param change handlers (different signatures for different widget types)
    UFUNCTION()
    void OnParamsChanged();

    UFUNCTION()
    void OnShapeChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

    UFUNCTION()
    void OnSpinBoxChanged(float Value);

    // Initialize combo box options
    void InitializeComboBox();
};
