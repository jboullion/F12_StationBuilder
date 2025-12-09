// F12BuilderController.h
// Player controller for the F12 Station Builder with Mode System

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/UserWidget.h"
#include "F12GridSystem.h"
#include "F12BuilderController.generated.h"

class AF12Module;

// Builder modes
UENUM(BlueprintType)
enum class EF12BuilderMode : uint8
{
    Build    UMETA(DisplayName = "Build Mode"),
    Paint    UMETA(DisplayName = "Paint Mode"),
    Delete   UMETA(DisplayName = "Delete Mode")
};

UCLASS()
class AF12BuilderController : public APlayerController
{
    GENERATED_BODY()

public:
    AF12BuilderController();

    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;
    virtual void Tick(float DeltaTime) override;

    // === REFERENCES ===
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Builder")
    AF12GridSystem* GridSystem;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Builder")
    TSubclassOf<AF12Module> ModuleClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Builder")
    UMaterialInterface* DefaultModuleMaterial;

    // Array of materials for painting
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Builder|Paint")
    TArray<UMaterialInterface*> PaintMaterials;

    // Parallel array of colors for HUD display (set these to match your materials)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Builder|Paint")
    TArray<FLinearColor> PaintColors;

    // === DELETE HIGHLIGHT ===
    
    // Material to show when hovering in delete mode (create a red emissive material)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Builder|Highlight")
    UMaterialInterface* DeleteHighlightMaterial;

    // === HUD ===
    
    // Widget class to spawn for HUD (set this to your WBP_BuilderHUD)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Builder|HUD")
    TSubclassOf<UUserWidget> HUDWidgetClass;

    // === MODE SYSTEM ===
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Builder|Mode")
    EF12BuilderMode CurrentMode = EF12BuilderMode::Build;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Builder|Mode")
    int32 CurrentPaintMaterialIndex = 0;

    // === PREVIEW ===
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Builder")
    AF12Module* GhostModule;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Builder")
    FF12GridCoord CurrentGridCoord;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Builder")
    bool bValidPlacement;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Builder")
    float TraceDistance = 10000.0f;

    // === MODE SWITCHING ===
    
    UFUNCTION(BlueprintCallable, Category = "Builder|Mode")
    void SetMode(EF12BuilderMode NewMode);

    UFUNCTION(BlueprintCallable, Category = "Builder|Mode")
    void CycleMode();

    UFUNCTION(BlueprintCallable, Category = "Builder|Mode")
    void SetBuildMode();

    UFUNCTION(BlueprintCallable, Category = "Builder|Mode")
    void SetPaintMode();

    UFUNCTION(BlueprintCallable, Category = "Builder|Mode")
    void SetDeleteMode();

    // === ACTIONS ===
    
    UFUNCTION(BlueprintCallable, Category = "Builder")
    void PlaceModule();

    UFUNCTION(BlueprintCallable, Category = "Builder")
    void RemoveModule();

    UFUNCTION(BlueprintCallable, Category = "Builder")
    void PrimaryAction();      // Left click - context dependent on mode

    UFUNCTION(BlueprintCallable, Category = "Builder")
    void SecondaryAction();    // Right click - context dependent on mode

    UFUNCTION(BlueprintCallable, Category = "Builder")
    void CyclePaintMaterial(); // Scroll wheel in paint mode

    // === HUD HELPERS ===
    
    // Get current paint color for HUD display
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Builder|HUD")
    FLinearColor GetCurrentPaintColor() const;

    // Get mode name as string
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Builder|HUD")
    FString GetModeName() const;

protected:
    // Input handlers
    void OnPrimaryAction();
    void OnSecondaryAction();
    void OnCycleMode();
    void OnSetBuildMode();
    void OnSetPaintMode();
    void OnSetDeleteMode();
    void OnScrollUp();
    void OnScrollDown();
    void OnModifierPressed();
    void OnModifierReleased();

    // State
    bool bModifierHeld = false;  // Shift key for tile-level actions

    // === HIGHLIGHT TRACKING ===
    UPROPERTY()
    AF12Module* HighlightedModule = nullptr;
    
    int32 HighlightedTileIndex = -1;
    
    UPROPERTY()
    TArray<UMaterialInterface*> HighlightedOriginalMaterials;

    // Update the ghost preview position
    void UpdateGhostPreview();

    // Delete mode highlight
    void UpdateDeleteHighlight();
    void ClearHighlight();

    // Perform trace from camera
    bool TraceFromCamera(FHitResult& OutHit);

    // Get the module and tile under cursor
    AF12Module* GetModuleUnderCursor(int32& OutTileIndex);

    // Mode-specific action handlers
    void HandleBuildPrimary();
    void HandleBuildSecondary();
    void HandlePaintPrimary();
    void HandlePaintSecondary();
    void HandleDeletePrimary();
    void HandleDeleteSecondary();
};