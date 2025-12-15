// F12StationGeneratorWidget.h
// UI Widget for Station Generator Controls
// Exposes all spine+branch generation parameters

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "F12StationGenerator.h"
#include "F12StationGeneratorWidget.generated.h"

class UButton;
class UTextBlock;
class UComboBoxString;
class USpinBox;
class UCheckBox;
class USlider;
class UBorder;
class UEditableTextBox;

/**
 * Widget for controlling the station generator
 * Create a Widget Blueprint with this as the parent class
 */
UCLASS()
class UF12StationGeneratorWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    // === BIND WIDGETS ===
    // Create widgets with these names in your Blueprint
    
    // Seed input
    UPROPERTY(meta = (BindWidgetOptional))
    UEditableTextBox* SeedInput;

    UPROPERTY(meta = (BindWidgetOptional))
    UButton* RandomSeedButton;

    // Primary axis selector
    UPROPERTY(meta = (BindWidgetOptional))
    UComboBoxString* AxisCombo;

    // Symmetry mode selector
    UPROPERTY(meta = (BindWidgetOptional))
    UComboBoxString* SymmetryCombo;

    // Main spine length
    UPROPERTY(meta = (BindWidgetOptional))
    USpinBox* SpineLengthSpinBox;

    // Branch count
    UPROPERTY(meta = (BindWidgetOptional))
    USpinBox* BranchCountSpinBox;

    // Branch length range
    UPROPERTY(meta = (BindWidgetOptional))
    USpinBox* BranchLengthMinSpinBox;

    UPROPERTY(meta = (BindWidgetOptional))
    USpinBox* BranchLengthMaxSpinBox;

    // Hub settings
    UPROPERTY(meta = (BindWidgetOptional))
    USpinBox* HubFrequencySpinBox;

    UPROPERTY(meta = (BindWidgetOptional))
    USpinBox* HubSizeSpinBox;

    // Offset controls
    UPROPERTY(meta = (BindWidgetOptional))
    USpinBox* OffsetXSpinBox;

    UPROPERTY(meta = (BindWidgetOptional))
    USpinBox* OffsetYSpinBox;

    UPROPERTY(meta = (BindWidgetOptional))
    USpinBox* OffsetZSpinBox;

    // Options
    UPROPERTY(meta = (BindWidgetOptional))
    UCheckBox* ClearExistingCheckBox;

    UPROPERTY(meta = (BindWidgetOptional))
    UCheckBox* PreserveCoreCheckBox;

    UPROPERTY(meta = (BindWidgetOptional))
    UCheckBox* AutoMaterialsCheckBox;

    // Buttons
    UPROPERTY(meta = (BindWidgetOptional))
    UButton* GenerateButton;

    UPROPERTY(meta = (BindWidgetOptional))
    UButton* PreviewButton;

    UPROPERTY(meta = (BindWidgetOptional))
    UButton* ClearAllButton;

    UPROPERTY(meta = (BindWidgetOptional))
    UButton* CloseButton;

    // Info display
    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* EstimateText;

    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* StatusText;

    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* LastResultText;

    // Main panel (for showing/hiding)
    UPROPERTY(meta = (BindWidgetOptional))
    UBorder* MainPanel;

    // === FUNCTIONS ===
    
    UFUNCTION(BlueprintCallable, Category = "StationGenerator")
    void ShowPanel();

    UFUNCTION(BlueprintCallable, Category = "StationGenerator")
    void HidePanel();

    UFUNCTION(BlueprintCallable, Category = "StationGenerator")
    void TogglePanel();

    UFUNCTION(BlueprintCallable, Category = "StationGenerator")
    bool IsPanelVisible() const;

    // Get current parameters from UI
    UFUNCTION(BlueprintCallable, Category = "StationGenerator")
    FF12StationParams GetCurrentParams() const;

    // Set UI from parameters
    UFUNCTION(BlueprintCallable, Category = "StationGenerator")
    void SetParamsToUI(const FF12StationParams& Params);

    // Update estimate display
    UFUNCTION(BlueprintCallable, Category = "StationGenerator")
    void UpdateEstimate();

    // Get the station generator
    UFUNCTION(BlueprintCallable, Category = "StationGenerator")
    UF12StationGenerator* GetGenerator();

protected:
    UPROPERTY()
    class AF12BuilderController* CachedController;

    UPROPERTY()
    UF12StationGenerator* Generator;

    bool bPanelVisible = false;
    float EstimateUpdateTimer = 0.0f;
    bool bEstimateNeedsUpdate = true;

    // Last generation result for display
    FF12StationResult LastResult;

    // Get controller reference
    class AF12BuilderController* GetBuilderController();

    // Initialize combo box options
    void InitializeComboBoxes();

    // Setup spin box ranges
    void SetupSpinBoxes();

    // Bind all button events
    void BindButtonEvents();

    // === BUTTON HANDLERS ===
    
    UFUNCTION()
    void OnGenerateClicked();

    UFUNCTION()
    void OnPreviewClicked();

    UFUNCTION()
    void OnClearAllClicked();

    UFUNCTION()
    void OnCloseClicked();

    UFUNCTION()
    void OnRandomSeedClicked();

    // === PARAM CHANGE HANDLERS ===
    
    UFUNCTION()
    void OnParamsChanged();

    UFUNCTION()
    void OnAxisChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

    UFUNCTION()
    void OnSymmetryChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

    UFUNCTION()
    void OnSpinBoxChanged(float Value);

    UFUNCTION()
    void OnSeedTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);
};
