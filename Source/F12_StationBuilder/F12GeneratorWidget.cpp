// F12GeneratorWidget.cpp
// Implementation of the Generation UI Widget

#include "F12GeneratorWidget.h"
#include "F12BuilderController.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/ComboBoxString.h"
#include "Components/SpinBox.h"
#include "Components/CheckBox.h"
#include "Components/Border.h"
#include "Kismet/GameplayStatics.h"

void UF12GeneratorWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // Initialize combo box with shape options
    InitializeComboBox();

    // Set default values for spin boxes
    if (SizeXSpinBox)
    {
        SizeXSpinBox->SetMinValue(1);
        SizeXSpinBox->SetMaxValue(50);
        SizeXSpinBox->SetValue(10);
        SizeXSpinBox->OnValueChanged.AddDynamic(this, &UF12GeneratorWidget::OnSpinBoxChanged);
    }
    if (SizeYSpinBox)
    {
        SizeYSpinBox->SetMinValue(1);
        SizeYSpinBox->SetMaxValue(50);
        SizeYSpinBox->SetValue(10);
        SizeYSpinBox->OnValueChanged.AddDynamic(this, &UF12GeneratorWidget::OnSpinBoxChanged);
    }
    if (SizeZSpinBox)
    {
        SizeZSpinBox->SetMinValue(1);
        SizeZSpinBox->SetMaxValue(50);
        SizeZSpinBox->SetValue(10);
        SizeZSpinBox->OnValueChanged.AddDynamic(this, &UF12GeneratorWidget::OnSpinBoxChanged);
    }
    if (ThicknessSpinBox)
    {
        ThicknessSpinBox->SetMinValue(1);
        ThicknessSpinBox->SetMaxValue(10);
        ThicknessSpinBox->SetValue(1);
        ThicknessSpinBox->OnValueChanged.AddDynamic(this, &UF12GeneratorWidget::OnSpinBoxChanged);
    }
    if (OffsetXSpinBox)
    {
        OffsetXSpinBox->SetMinValue(-50);
        OffsetXSpinBox->SetMaxValue(50);
        OffsetXSpinBox->SetValue(0);
    }
    if (OffsetYSpinBox)
    {
        OffsetYSpinBox->SetMinValue(-50);
        OffsetYSpinBox->SetMaxValue(50);
        OffsetYSpinBox->SetValue(0);
    }
    if (OffsetZSpinBox)
    {
        OffsetZSpinBox->SetMinValue(-50);
        OffsetZSpinBox->SetMaxValue(50);
        OffsetZSpinBox->SetValue(0);
    }

    // Set default checkbox values
    if (CenterCheckBox)
    {
        CenterCheckBox->SetIsChecked(true);
    }
    if (ClearExistingCheckBox)
    {
        ClearExistingCheckBox->SetIsChecked(false);
    }
    if (PreserveCoreCheckBox)
    {
        PreserveCoreCheckBox->SetIsChecked(true);
    }

    // Bind button events
    if (GenerateButton)
    {
        GenerateButton->OnClicked.AddDynamic(this, &UF12GeneratorWidget::OnGenerateClicked);
    }
    if (ClearAllButton)
    {
        ClearAllButton->OnClicked.AddDynamic(this, &UF12GeneratorWidget::OnClearAllClicked);
    }
    if (CloseButton)
    {
        CloseButton->OnClicked.AddDynamic(this, &UF12GeneratorWidget::OnCloseClicked);
    }

    // Start hidden
    HidePanel();

    // Initial estimate
    UpdateEstimate();
}

void UF12GeneratorWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    // Throttle estimate updates
    EstimateUpdateTimer += InDeltaTime;
    if (EstimateUpdateTimer >= 0.2f && bPanelVisible)
    {
        EstimateUpdateTimer = 0.0f;
        UpdateEstimate();
    }
}

void UF12GeneratorWidget::InitializeComboBox()
{
    if (ShapeCombo)
    {
        ShapeCombo->ClearOptions();
        ShapeCombo->AddOption(TEXT("Hollow Box"));
        ShapeCombo->AddOption(TEXT("Solid Box"));
        ShapeCombo->AddOption(TEXT("Hollow Sphere"));
        ShapeCombo->AddOption(TEXT("Solid Sphere"));
        ShapeCombo->AddOption(TEXT("Cylinder"));
        ShapeCombo->AddOption(TEXT("Cross"));
        ShapeCombo->AddOption(TEXT("Ring"));
        ShapeCombo->SetSelectedIndex(0);
        ShapeCombo->OnSelectionChanged.AddDynamic(this, &UF12GeneratorWidget::OnShapeChanged);
    }
}

AF12BuilderController* UF12GeneratorWidget::GetBuilderController()
{
    if (!CachedController)
    {
        APlayerController* PC = GetOwningPlayer();
        if (PC)
        {
            CachedController = Cast<AF12BuilderController>(PC);
        }
    }
    return CachedController;
}

void UF12GeneratorWidget::ShowPanel()
{
    bPanelVisible = true;
    if (MainPanel)
    {
        MainPanel->SetVisibility(ESlateVisibility::Visible);
    }
    else
    {
        SetVisibility(ESlateVisibility::Visible);
    }
    UpdateEstimate();
}

void UF12GeneratorWidget::HidePanel()
{
    bPanelVisible = false;
    if (MainPanel)
    {
        MainPanel->SetVisibility(ESlateVisibility::Hidden);
    }
    else
    {
        SetVisibility(ESlateVisibility::Hidden);
    }
}

void UF12GeneratorWidget::TogglePanel()
{
    if (bPanelVisible)
    {
        HidePanel();
    }
    else
    {
        ShowPanel();
    }
}

bool UF12GeneratorWidget::IsPanelVisible() const
{
    return bPanelVisible;
}

FF12GenerationParams UF12GeneratorWidget::GetCurrentParams() const
{
    FF12GenerationParams Params;

    // Shape
    if (ShapeCombo)
    {
        int32 Index = ShapeCombo->GetSelectedIndex();
        Params.Shape = static_cast<EF12GeneratorShape>(Index);
    }

    // Size
    if (SizeXSpinBox) Params.SizeX = FMath::RoundToInt(SizeXSpinBox->GetValue());
    if (SizeYSpinBox) Params.SizeY = FMath::RoundToInt(SizeYSpinBox->GetValue());
    if (SizeZSpinBox) Params.SizeZ = FMath::RoundToInt(SizeZSpinBox->GetValue());

    // Thickness
    if (ThicknessSpinBox) Params.WallThickness = FMath::RoundToInt(ThicknessSpinBox->GetValue());

    // Offset
    if (OffsetXSpinBox) Params.Offset.X = FMath::RoundToInt(OffsetXSpinBox->GetValue());
    if (OffsetYSpinBox) Params.Offset.Y = FMath::RoundToInt(OffsetYSpinBox->GetValue());
    if (OffsetZSpinBox) Params.Offset.Z = FMath::RoundToInt(OffsetZSpinBox->GetValue());

    // Options
    if (CenterCheckBox) Params.bCenterOnOffset = CenterCheckBox->IsChecked();
    if (ClearExistingCheckBox) Params.bClearExisting = ClearExistingCheckBox->IsChecked();
    if (PreserveCoreCheckBox) Params.bPreserveCore = PreserveCoreCheckBox->IsChecked();

    return Params;
}

void UF12GeneratorWidget::UpdateEstimate()
{
    AF12BuilderController* Controller = GetBuilderController();
    if (!Controller || !Controller->ProceduralGenerator)
        return;

    FF12GenerationParams Params = GetCurrentParams();
    int32 Estimate = Controller->ProceduralGenerator->EstimateModuleCount(Params);

    if (EstimateText)
    {
        EstimateText->SetText(FText::FromString(FString::Printf(TEXT("Estimated: %d modules"), Estimate)));
    }
}

void UF12GeneratorWidget::OnParamsChanged()
{
    // Trigger estimate update on next tick
    EstimateUpdateTimer = 0.2f;
}

void UF12GeneratorWidget::OnShapeChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    OnParamsChanged();
}

void UF12GeneratorWidget::OnSpinBoxChanged(float Value)
{
    OnParamsChanged();
}

void UF12GeneratorWidget::OnGenerateClicked()
{
    AF12BuilderController* Controller = GetBuilderController();
    if (!Controller || !Controller->ProceduralGenerator)
    {
        if (StatusText)
        {
            StatusText->SetText(FText::FromString(TEXT("Error: Generator not available")));
        }
        return;
    }

    FF12GenerationParams Params = GetCurrentParams();
    FF12GenerationResult Result = Controller->ProceduralGenerator->Generate(Params);

    if (StatusText)
    {
        StatusText->SetText(FText::FromString(Result.Message));
    }

    UE_LOG(LogTemp, Log, TEXT("Generation: %s"), *Result.Message);
}

void UF12GeneratorWidget::OnClearAllClicked()
{
    AF12BuilderController* Controller = GetBuilderController();
    if (!Controller || !Controller->ProceduralGenerator)
        return;

    bool bPreserveCore = PreserveCoreCheckBox ? PreserveCoreCheckBox->IsChecked() : true;
    int32 Cleared = Controller->ProceduralGenerator->ClearAll(bPreserveCore);

    if (StatusText)
    {
        StatusText->SetText(FText::FromString(FString::Printf(TEXT("Cleared %d modules"), Cleared)));
    }

    // Note: Clear all doesn't support undo currently (would need to store all module states)
}

void UF12GeneratorWidget::OnCloseClicked()
{
    HidePanel();
}