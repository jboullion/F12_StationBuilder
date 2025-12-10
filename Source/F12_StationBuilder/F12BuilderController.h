// F12BuilderController.h
// Player controller for the F12 Station Builder with Mode System
// Updated with Undo/Redo and Drag-Build functionality

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/UserWidget.h"
#include "F12GridSystem.h"
#include "F12BuilderController.generated.h"

class AF12Module;
class UF12CommandHistory;
struct FF12Command;

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

    // Parallel array of colors for HUD display
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Builder|Paint")
    TArray<FLinearColor> PaintColors;

    // === DELETE HIGHLIGHT ===
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Builder|Highlight")
    UMaterialInterface* DeleteHighlightMaterial;

    // === HUD ===
    
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
    UF12CommandHistory* CommandHistory;

    // === DRAG BUILD SYSTEM ===
    
    // Is drag-build currently active?
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Builder|DragBuild")
    bool bIsDragging = false;

    // Starting coordinate of drag
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Builder|DragBuild")
    FF12GridCoord DragStartCoord;

    // Preview modules shown during drag
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Builder|DragBuild")
    TArray<AF12Module*> DragPreviewModules;

    // Coordinates that will be placed on drag release
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Builder|DragBuild")
    TArray<FF12GridCoord> DragCoords;

    // Maximum modules per drag operation
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Builder|DragBuild")
    int32 MaxDragModules = 20;

    // Drag plane tracking (for tracking mouse in 3D space during drag)
    FVector DragStartWorldPos;
    FVector DragPlaneNormal;
    
    // Face-based direction for straight-line dragging
    int32 DragFaceIndex = -1;  // Which face was clicked to start the drag

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
    void PrimaryAction();

    UFUNCTION(BlueprintCallable, Category = "Builder")
    void SecondaryAction();

    UFUNCTION(BlueprintCallable, Category = "Builder")
    void CyclePaintMaterial();

    // === UNDO/REDO ===
    
    UFUNCTION(BlueprintCallable, Category = "Builder|History")
    void Undo();

    UFUNCTION(BlueprintCallable, Category = "Builder|History")
    void Redo();

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Builder|History")
    bool CanUndo() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Builder|History")
    bool CanRedo() const;

    // === HUD HELPERS ===
    
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Builder|HUD")
    FLinearColor GetCurrentPaintColor() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Builder|HUD")
    FString GetModeName() const;

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

    // State
    bool bModifierHeld = false;
    bool bPrimaryHeld = false;

    // === HIGHLIGHT TRACKING ===
    UPROPERTY()
    AF12Module* HighlightedModule = nullptr;
    
    int32 HighlightedTileIndex = -1;
    
    UPROPERTY()
    TArray<UMaterialInterface*> HighlightedOriginalMaterials;

    // Update functions
    void UpdateGhostPreview();
    void UpdateDeleteHighlight();
    void ClearHighlight();

    // Drag build functions
    void StartDrag();
    void UpdateDrag();
    void EndDrag();
    void CancelDrag();
    void ClearDragPreview();
    TArray<FF12GridCoord> CalculateDragLine(const FF12GridCoord& Start, const FF12GridCoord& End);

    // Utility
    bool TraceFromCamera(FHitResult& OutHit);
    AF12Module* GetModuleUnderCursor(int32& OutTileIndex);

    // Mode-specific action handlers
    void HandleBuildPrimary();
    void HandleBuildPrimaryRelease();
    void HandleBuildSecondary();
    void HandlePaintPrimary();
    void HandlePaintSecondary();
    void HandleDeletePrimary();
    void HandleDeleteSecondary();

    // Command helpers (creates and executes commands with history)
    void ExecutePlaceModule(const FF12GridCoord& Coord);
    void ExecutePlaceMultiple(const TArray<FF12GridCoord>& Coords);
    void ExecuteDeleteModule(const FF12GridCoord& Coord);
    void ExecutePaintTile(AF12Module* Module, int32 TileIndex, int32 NewMaterialIndex);
    void ExecutePaintModule(AF12Module* Module, int32 NewMaterialIndex);
    void ExecuteDeleteTile(AF12Module* Module, int32 TileIndex);
    void ExecuteRestoreTile(AF12Module* Module, int32 TileIndex);
};