// F12BuilderController.h
// Player controller for the F12 Station Builder with Mode System
// Includes: Undo/Redo, Drag Build Mode, Procedural Generation

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/UserWidget.h"
#include "F12GridSystem.h"
#include "F12BuilderActions.h"
#include "F12BuilderController.generated.h"

class AF12Module;
class UF12ProceduralGenerator;
class UF12GeneratorWidget;

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

    // === UNDO/REDO SYSTEM ===
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Builder|History")
    UF12ActionHistory* ActionHistory;

    // === PROCEDURAL GENERATION ===
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Builder|Generation")
    UF12ProceduralGenerator* ProceduralGenerator;

    // Widget class for generation UI
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Builder|Generation")
    TSubclassOf<UUserWidget> GeneratorWidgetClass;

    // Reference to spawned generator widget
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Builder|Generation")
    UF12GeneratorWidget* GeneratorWidget;

    // === DRAG BUILD SETTINGS ===
    
    // Maximum number of modules that can be placed in a single drag
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Builder|DragBuild")
    int32 MaxDragModules = 20;

    // Material for ghost modules during drag (optional - uses default if not set)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Builder|DragBuild")
    UMaterialInterface* DragGhostMaterial;

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

    // === UNDO/REDO ===
    
    UFUNCTION(BlueprintCallable, Category = "Builder|History")
    void Undo();

    UFUNCTION(BlueprintCallable, Category = "Builder|History")
    void Redo();

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Builder|History")
    bool CanUndo() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Builder|History")
    bool CanRedo() const;

    // === PROCEDURAL GENERATION ===
    
    UFUNCTION(BlueprintCallable, Category = "Builder|Generation")
    void ToggleGeneratorPanel();

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Builder|Generation")
    bool IsGeneratorPanelVisible() const;

    // === HUD HELPERS ===
    
    // Get current paint color for HUD display
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Builder|HUD")
    FLinearColor GetCurrentPaintColor() const;

    // Get mode name as string
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Builder|HUD")
    FString GetModeName() const;

    // Get undo/redo counts for HUD
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Builder|HUD")
    int32 GetUndoCount() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Builder|HUD")
    int32 GetRedoCount() const;

    // Check if currently drag building
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Builder|HUD")
    bool IsDragBuilding() const { return bIsDragBuilding; }

    // Get number of modules in current drag
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Builder|HUD")
    int32 GetDragModuleCount() const { return DragGhostModules.Num(); }

protected:
    // Input handlers
    void OnPrimaryAction();
    void OnPrimaryActionReleased();
    void OnSecondaryAction();
    void OnCycleMode();
    void OnSetBuildMode();
    void OnSetPaintMode();
    void OnSetDeleteMode();
    void OnScrollUp();
    void OnScrollDown();
    void OnModifierPressed();
    void OnModifierReleased();
    void OnUndo();
    void OnRedo();
    void OnToggleGenerator();

    // State
    bool bModifierHeld = false;  // Shift key for tile-level actions and drag build

    // === DRAG BUILD STATE ===
    
    bool bIsDragBuilding = false;
    FF12GridCoord DragStartCoord;
    FIntVector DragDirection;  // The direction we're building in (from face normal)
    int32 DragStartFaceIndex = -1;
    
    // Ghost modules for drag preview
    UPROPERTY()
    TArray<AF12Module*> DragGhostModules;

    // Coordinates that will be filled on drag complete
    TArray<FF12GridCoord> DragCoords;

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
    void HandleBuildPrimaryRelease();
    void HandleBuildSecondary();
    void HandlePaintPrimary();
    void HandlePaintSecondary();
    void HandleDeletePrimary();
    void HandleDeleteSecondary();

    // === DRAG BUILD HELPERS ===
    
    // Start a drag build operation
    void StartDragBuild(FF12GridCoord StartCoord, int32 FaceIndex);

    // Update drag build preview based on current mouse position
    void UpdateDragBuild();

    // Complete the drag build and place all modules
    void CompleteDragBuild();

    // Cancel drag build without placing
    void CancelDragBuild();

    // Create a ghost module for drag preview
    AF12Module* CreateDragGhost();

    // Clear all drag ghost modules
    void ClearDragGhosts();

    // Calculate how many modules fit in a direction based on cursor distance
    int32 CalculateDragLength(const FVector& CursorWorldPos);

    // === UNDO/REDO EXECUTION ===
    
    // Execute an undo operation
    void ExecuteUndo(const FF12BuilderAction& Action);

    // Execute a redo operation  
    void ExecuteRedo(const FF12BuilderAction& Action);

    // Helper to spawn a module at a grid coord (used by undo/redo)
    AF12Module* SpawnModuleAtCoord(FF12GridCoord Coord, const TArray<int32>* MaterialIndices = nullptr, const TArray<bool>* Visibility = nullptr);

    // Helper to remove a module at a grid coord (used by undo/redo)
    void RemoveModuleAtCoord(FF12GridCoord Coord);
};
