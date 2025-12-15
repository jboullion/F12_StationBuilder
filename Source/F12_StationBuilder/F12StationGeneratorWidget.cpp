// F12StationGeneratorWidget.cpp
// Implementation of Station Generator UI Widget

#include "F12StationGeneratorWidget.h"
#include "F12BuilderController.h"
#include "F12InstancedRenderer.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/ComboBoxString.h"
#include "Components/SpinBox.h"
#include "Components/CheckBox.h"
#include "Components/Border.h"
#include "Components/EditableTextBox.h"
#include "Kismet/GameplayStatics.h"

void UF12StationGeneratorWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // Initialize UI elements
    InitializeComboBoxes();
    SetupSpinBoxes();
    BindButtonEvents();

    // Set default values
    FF12StationParams DefaultParams;
    SetParamsToUI(DefaultParams);

    // Start hidden
    HidePanel();

    // Initial estimate
    bEstimateNeedsUpdate = true;

    UE_LOG(LogTemp, Log, TEXT("StationGeneratorWidget constructed"));
}

void UF12StationGeneratorWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    // Throttle estimate updates
    if (bEstimateNeedsUpdate && bPanelVisible)
    {
        EstimateUpdateTimer += InDeltaTime;
        if (EstimateUpdateTimer >= 0.3f)
        {
            EstimateUpdateTimer = 0.0f;
            bEstimateNeedsUpdate = false;
            UpdateEstimate();
        }
    }
}

void UF12StationGeneratorWidget::InitializeComboBoxes()
{
    // Primary axis combo
    if (AxisCombo)
    {
        AxisCombo->ClearOptions();
        AxisCombo->AddOption(TEXT("X Axis"));
        AxisCombo->AddOption(TEXT("Y Axis"));
        AxisCombo->AddOption(TEXT("Z Axis"));
        AxisCombo->SetSelectedIndex(0);
        AxisCombo->OnSelectionChanged.AddDynamic(this, &UF12StationGeneratorWidget::OnAxisChanged);
    }

    // Symmetry mode combo
    if (SymmetryCombo)
    {
        SymmetryCombo->ClearOptions();
        SymmetryCombo->AddOption(TEXT("None"));
        SymmetryCombo->AddOption(TEXT("Bilateral (Mirror)"));
        SymmetryCombo->AddOption(TEXT("Quadrilateral (4-way)"));
        SymmetryCombo->AddOption(TEXT("Radial (6-way)"));
        SymmetryCombo->SetSelectedIndex(1);  // Default to bilateral
        SymmetryCombo->OnSelectionChanged.AddDynamic(this, &UF12StationGeneratorWidget::OnSymmetryChanged);
    }
}

void UF12StationGeneratorWidget::SetupSpinBoxes()
{
    // Seed input (EditableTextBox handles this)
    if (SeedInput)
    {
        SeedInput->SetText(FText::FromString(TEXT("12345")));
        SeedInput->OnTextCommitted.AddDynamic(this, &UF12StationGeneratorWidget::OnSeedTextCommitted);
    }

    // Spine length
    if (SpineLengthSpinBox)
    {
        SpineLengthSpinBox->SetMinValue(3);
        SpineLengthSpinBox->SetMaxValue(70);
        SpineLengthSpinBox->SetMinSliderValue(3);
        SpineLengthSpinBox->SetMaxSliderValue(70);
        SpineLengthSpinBox->SetValue(15);
        SpineLengthSpinBox->OnValueChanged.AddDynamic(this, &UF12StationGeneratorWidget::OnSpinBoxChanged);
    }

    // Branch count
    if (BranchCountSpinBox)
    {
        BranchCountSpinBox->SetMinValue(0);
        BranchCountSpinBox->SetMaxValue(12);
        BranchCountSpinBox->SetMinSliderValue(0);
        BranchCountSpinBox->SetMaxSliderValue(12);
        BranchCountSpinBox->SetValue(4);
        BranchCountSpinBox->OnValueChanged.AddDynamic(this, &UF12StationGeneratorWidget::OnSpinBoxChanged);
    }

    // Branch length min
    if (BranchLengthMinSpinBox)
    {
        BranchLengthMinSpinBox->SetMinValue(2);
        BranchLengthMinSpinBox->SetMaxValue(30);
        BranchLengthMinSpinBox->SetMinSliderValue(2);
        BranchLengthMinSpinBox->SetMaxSliderValue(30);
        BranchLengthMinSpinBox->SetValue(3);
        BranchLengthMinSpinBox->OnValueChanged.AddDynamic(this, &UF12StationGeneratorWidget::OnSpinBoxChanged);
    }

    // Branch length max
    if (BranchLengthMaxSpinBox)
    {
        BranchLengthMaxSpinBox->SetMinValue(2);
        BranchLengthMaxSpinBox->SetMaxValue(30);
        BranchLengthMaxSpinBox->SetMinSliderValue(2);
        BranchLengthMaxSpinBox->SetMaxSliderValue(30);
        BranchLengthMaxSpinBox->SetValue(8);
        BranchLengthMaxSpinBox->OnValueChanged.AddDynamic(this, &UF12StationGeneratorWidget::OnSpinBoxChanged);
    }

    // Hub frequency
    if (HubFrequencySpinBox)
    {
        HubFrequencySpinBox->SetMinValue(1);
        HubFrequencySpinBox->SetMaxValue(10);
        HubFrequencySpinBox->SetMinSliderValue(1);
        HubFrequencySpinBox->SetMaxSliderValue(10);
        HubFrequencySpinBox->SetValue(5);
        HubFrequencySpinBox->OnValueChanged.AddDynamic(this, &UF12StationGeneratorWidget::OnSpinBoxChanged);
    }

    // Hub size
    if (HubSizeSpinBox)
    {
        HubSizeSpinBox->SetMinValue(1);
        HubSizeSpinBox->SetMaxValue(5);
        HubSizeSpinBox->SetMinSliderValue(1);
        HubSizeSpinBox->SetMaxSliderValue(5);
        HubSizeSpinBox->SetValue(2);
        HubSizeSpinBox->OnValueChanged.AddDynamic(this, &UF12StationGeneratorWidget::OnSpinBoxChanged);
    }

    // Offset X/Y/Z
    if (OffsetXSpinBox)
    {
        OffsetXSpinBox->SetMinValue(-35);
        OffsetXSpinBox->SetMaxValue(35);
        OffsetXSpinBox->SetMinSliderValue(-35);
        OffsetXSpinBox->SetMaxSliderValue(35);
        OffsetXSpinBox->SetValue(0);
        OffsetXSpinBox->OnValueChanged.AddDynamic(this, &UF12StationGeneratorWidget::OnSpinBoxChanged);
    }
    if (OffsetYSpinBox)
    {
        OffsetYSpinBox->SetMinValue(-35);
        OffsetYSpinBox->SetMaxValue(35);
        OffsetYSpinBox->SetMinSliderValue(-35);
        OffsetYSpinBox->SetMaxSliderValue(35);
        OffsetYSpinBox->SetValue(0);
        OffsetYSpinBox->OnValueChanged.AddDynamic(this, &UF12StationGeneratorWidget::OnSpinBoxChanged);
    }
    if (OffsetZSpinBox)
    {
        OffsetZSpinBox->SetMinValue(-35);
        OffsetZSpinBox->SetMaxValue(35);
        OffsetZSpinBox->SetMinSliderValue(-35);
        OffsetZSpinBox->SetMaxSliderValue(35);
        OffsetZSpinBox->SetValue(0);
        OffsetZSpinBox->OnValueChanged.AddDynamic(this, &UF12StationGeneratorWidget::OnSpinBoxChanged);
    }

    // Checkboxes
    if (ClearExistingCheckBox)
    {
        ClearExistingCheckBox->SetIsChecked(true);
    }
    if (PreserveCoreCheckBox)
    {
        PreserveCoreCheckBox->SetIsChecked(true);
    }
    if (AutoMaterialsCheckBox)
    {
        AutoMaterialsCheckBox->SetIsChecked(true);
    }
}

void UF12StationGeneratorWidget::BindButtonEvents()
{
    if (GenerateButton)
    {
        GenerateButton->OnClicked.AddDynamic(this, &UF12StationGeneratorWidget::OnGenerateClicked);
    }
    if (PreviewButton)
    {
        PreviewButton->OnClicked.AddDynamic(this, &UF12StationGeneratorWidget::OnPreviewClicked);
    }
    if (ClearAllButton)
    {
        ClearAllButton->OnClicked.AddDynamic(this, &UF12StationGeneratorWidget::OnClearAllClicked);
    }
    if (CloseButton)
    {
        CloseButton->OnClicked.AddDynamic(this, &UF12StationGeneratorWidget::OnCloseClicked);
    }
    if (RandomSeedButton)
    {
        RandomSeedButton->OnClicked.AddDynamic(this, &UF12StationGeneratorWidget::OnRandomSeedClicked);
    }
}

AF12BuilderController* UF12StationGeneratorWidget::GetBuilderController()
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

UF12StationGenerator* UF12StationGeneratorWidget::GetGenerator()
{
    if (!Generator)
    {
        AF12BuilderController* Controller = GetBuilderController();
        if (Controller && Controller->GridSystem)
        {
            Generator = NewObject<UF12StationGenerator>(this, TEXT("StationGenerator"));
            Generator->Initialize(Controller->GridSystem, Controller);
        }
    }
    return Generator;
}

void UF12StationGeneratorWidget::ShowPanel()
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
    bEstimateNeedsUpdate = true;
}

void UF12StationGeneratorWidget::HidePanel()
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

void UF12StationGeneratorWidget::TogglePanel()
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

bool UF12StationGeneratorWidget::IsPanelVisible() const
{
    return bPanelVisible;
}

FF12StationParams UF12StationGeneratorWidget::GetCurrentParams() const
{
    FF12StationParams Params;

    // Seed
    if (SeedInput)
    {
        Params.Seed = FCString::Atoi(*SeedInput->GetText().ToString());
    }

    // Primary axis
    if (AxisCombo)
    {
        Params.PrimaryAxis = static_cast<EF12PrimaryAxis>(AxisCombo->GetSelectedIndex());
    }

    // Symmetry mode
    if (SymmetryCombo)
    {
        Params.SymmetryMode = static_cast<EF12SymmetryMode>(SymmetryCombo->GetSelectedIndex());
    }

    // Spine length
    if (SpineLengthSpinBox)
    {
        Params.MainSpineLength = FMath::RoundToInt(SpineLengthSpinBox->GetValue());
    }

    // Branch count
    if (BranchCountSpinBox)
    {
        Params.BranchCount = FMath::RoundToInt(BranchCountSpinBox->GetValue());
    }

    // Branch length range
    if (BranchLengthMinSpinBox)
    {
        Params.BranchLengthMin = FMath::RoundToInt(BranchLengthMinSpinBox->GetValue());
    }
    if (BranchLengthMaxSpinBox)
    {
        Params.BranchLengthMax = FMath::RoundToInt(BranchLengthMaxSpinBox->GetValue());
    }

    // Ensure min <= max
    if (Params.BranchLengthMin > Params.BranchLengthMax)
    {
        Params.BranchLengthMax = Params.BranchLengthMin;
    }

    // Hub settings
    if (HubFrequencySpinBox)
    {
        Params.HubFrequency = FMath::RoundToInt(HubFrequencySpinBox->GetValue());
    }
    if (HubSizeSpinBox)
    {
        Params.HubSize = FMath::RoundToInt(HubSizeSpinBox->GetValue());
    }

    // Offset
    if (OffsetXSpinBox)
    {
        Params.Offset.X = FMath::RoundToInt(OffsetXSpinBox->GetValue());
    }
    if (OffsetYSpinBox)
    {
        Params.Offset.Y = FMath::RoundToInt(OffsetYSpinBox->GetValue());
    }
    if (OffsetZSpinBox)
    {
        Params.Offset.Z = FMath::RoundToInt(OffsetZSpinBox->GetValue());
    }

    // Options
    if (ClearExistingCheckBox)
    {
        Params.bClearExisting = ClearExistingCheckBox->IsChecked();
    }
    if (PreserveCoreCheckBox)
    {
        Params.bPreserveCore = PreserveCoreCheckBox->IsChecked();
    }
    if (AutoMaterialsCheckBox)
    {
        Params.bAutoAssignMaterials = AutoMaterialsCheckBox->IsChecked();
    }

    return Params;
}

void UF12StationGeneratorWidget::SetParamsToUI(const FF12StationParams& Params)
{
    if (SeedInput)
    {
        SeedInput->SetText(FText::FromString(FString::FromInt(Params.Seed)));
    }

    if (AxisCombo)
    {
        AxisCombo->SetSelectedIndex(static_cast<int32>(Params.PrimaryAxis));
    }

    if (SymmetryCombo)
    {
        SymmetryCombo->SetSelectedIndex(static_cast<int32>(Params.SymmetryMode));
    }

    if (SpineLengthSpinBox)
    {
        SpineLengthSpinBox->SetValue(Params.MainSpineLength);
    }

    if (BranchCountSpinBox)
    {
        BranchCountSpinBox->SetValue(Params.BranchCount);
    }

    if (BranchLengthMinSpinBox)
    {
        BranchLengthMinSpinBox->SetValue(Params.BranchLengthMin);
    }

    if (BranchLengthMaxSpinBox)
    {
        BranchLengthMaxSpinBox->SetValue(Params.BranchLengthMax);
    }

    if (HubFrequencySpinBox)
    {
        HubFrequencySpinBox->SetValue(Params.HubFrequency);
    }

    if (HubSizeSpinBox)
    {
        HubSizeSpinBox->SetValue(Params.HubSize);
    }

    if (OffsetXSpinBox)
    {
        OffsetXSpinBox->SetValue(Params.Offset.X);
    }

    if (OffsetYSpinBox)
    {
        OffsetYSpinBox->SetValue(Params.Offset.Y);
    }

    if (OffsetZSpinBox)
    {
        OffsetZSpinBox->SetValue(Params.Offset.Z);
    }

    if (ClearExistingCheckBox)
    {
        ClearExistingCheckBox->SetIsChecked(Params.bClearExisting);
    }

    if (PreserveCoreCheckBox)
    {
        PreserveCoreCheckBox->SetIsChecked(Params.bPreserveCore);
    }

    if (AutoMaterialsCheckBox)
    {
        AutoMaterialsCheckBox->SetIsChecked(Params.bAutoAssignMaterials);
    }

    bEstimateNeedsUpdate = true;
}

void UF12StationGeneratorWidget::UpdateEstimate()
{
    UF12StationGenerator* Gen = GetGenerator();
    if (!Gen)
    {
        if (EstimateText)
        {
            EstimateText->SetText(FText::FromString(TEXT("Generator not available")));
        }
        return;
    }

    FF12StationParams Params = GetCurrentParams();
    int32 Estimate = Gen->EstimateModuleCount(Params);

    if (EstimateText)
    {
        EstimateText->SetText(FText::FromString(FString::Printf(TEXT("Estimated: ~%d modules"), Estimate)));
    }
}

void UF12StationGeneratorWidget::OnParamsChanged()
{
    bEstimateNeedsUpdate = true;
    EstimateUpdateTimer = 0.0f;
}

void UF12StationGeneratorWidget::OnAxisChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    OnParamsChanged();
}

void UF12StationGeneratorWidget::OnSymmetryChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    OnParamsChanged();
}

void UF12StationGeneratorWidget::OnSpinBoxChanged(float Value)
{
    OnParamsChanged();
}

void UF12StationGeneratorWidget::OnSeedTextCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
    OnParamsChanged();
}

void UF12StationGeneratorWidget::OnGenerateClicked()
{
    UF12StationGenerator* Gen = GetGenerator();
    if (!Gen)
    {
        if (StatusText)
        {
            StatusText->SetText(FText::FromString(TEXT("Error: Generator not available")));
        }
        return;
    }

    FF12StationParams Params = GetCurrentParams();
    
    if (StatusText)
    {
        StatusText->SetText(FText::FromString(TEXT("Generating...")));
    }

    // Generate the station
    LastResult = Gen->GenerateStation(Params);

    if (StatusText)
    {
        StatusText->SetText(FText::FromString(LastResult.bSuccess ? TEXT("Generation complete!") : TEXT("Generation failed")));
    }

    if (LastResultText)
    {
        LastResultText->SetText(FText::FromString(LastResult.Message));
    }

    UE_LOG(LogTemp, Log, TEXT("Station Generation: %s"), *LastResult.Message);
}

void UF12StationGeneratorWidget::OnPreviewClicked()
{
    UF12StationGenerator* Gen = GetGenerator();
    if (!Gen)
        return;

    FF12StationParams Params = GetCurrentParams();
    TArray<FF12GridCoord> PreviewCoords = Gen->PreviewStation(Params);

    if (StatusText)
    {
        StatusText->SetText(FText::FromString(FString::Printf(TEXT("Preview: %d modules"), PreviewCoords.Num())));
    }

    // TODO: Could visualize preview with ghost meshes here
}

void UF12StationGeneratorWidget::OnClearAllClicked()
{
    UF12StationGenerator* Gen = GetGenerator();
    if (!Gen)
        return;

    bool bPreserveCore = PreserveCoreCheckBox ? PreserveCoreCheckBox->IsChecked() : true;
    int32 Cleared = Gen->ClearAll(bPreserveCore);

    if (StatusText)
    {
        StatusText->SetText(FText::FromString(FString::Printf(TEXT("Cleared %d modules"), Cleared)));
    }

    if (LastResultText)
    {
        LastResultText->SetText(FText::FromString(TEXT("")));
    }
}

void UF12StationGeneratorWidget::OnCloseClicked()
{
    HidePanel();
}

void UF12StationGeneratorWidget::OnRandomSeedClicked()
{
    if (SeedInput)
    {
        int32 RandomSeed = FMath::RandRange(1, 999999);
        SeedInput->SetText(FText::FromString(FString::FromInt(RandomSeed)));
        OnParamsChanged();
    }
}
