// F12BuilderController.h
// Player controller for the F12 Station Builder
// Uses instanced rendering exclusively for maximum performance

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/UserWidget.h"
#include "F12GridSystem.h"
#include "F12BuilderController.generated.h"

class AF12InstancedRenderer;
class UF12ProceduralGenerator;
class UF12GeneratorWidget;
class AF12BuilderPawn;

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
    AF12InstancedRenderer* InstancedRenderer;

    // Array of colors for HUD display (should match renderer's TileMaterials)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Builder|Paint")
    TArray<FLinearColor> PaintColors;

    // Ghost preview material (semi-transparent)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Builder|Preview")
    UMaterialInterface* GhostMaterial;

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
    FF12GridCoord CurrentGridCoord;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Builder")
    bool bValidPlacement = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Builder")
    float TraceDistance = 10000.0f;

    // === PROCEDURAL GENERATION ===
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Builder|Generation")
    UF12ProceduralGenerator* ProceduralGenerator;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Builder|Generation")
    TSubclassOf<UUserWidget> GeneratorWidgetClass;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Builder|Generation")
    UF12GeneratorWidget* GeneratorWidget;

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
    void PrimaryAction();

    UFUNCTION(BlueprintCallable, Category = "Builder")
    void SecondaryAction();

    UFUNCTION(BlueprintCallable, Category = "Builder")
    void CyclePaintMaterial();

    // === PROCEDURAL GENERATION ===
    
    UFUNCTION(BlueprintCallable, Category = "Builder|Generation")
    void ToggleGeneratorPanel();

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Builder|Generation")
    bool IsGeneratorPanelVisible() const;

    // Generate modules at given coordinates
    UFUNCTION(BlueprintCallable, Category = "Builder|Generation")
    void GenerateModules(const TArray<FF12GridCoord>& Coords);

    // === HUD HELPERS ===
    
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Builder|HUD")
    FLinearColor GetCurrentPaintColor() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Builder|HUD")
    FString GetModeName() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Builder|HUD")
    int32 GetModuleCount() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Builder|HUD")
    FString GetPerformanceStats() const;

    // === CAMERA HELPERS ===

    // Get the builder pawn (for camera control)
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Builder|Camera")
    AF12BuilderPawn* GetBuilderPawn() const;

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
    void OnToggleGenerator();
    void OnCyclePaint();
    
    // Camera rotation handlers
    void OnCameraRotatePressed();
    void OnCameraRotateReleased();
    
    // Camera rotation state
    bool bIsRotatingCamera = false;
    void UpdateCameraRotation();

    // State
    bool bModifierHeld = false;  // Shift key for tile-level operations
    bool bIsDragging = false;    // Currently dragging to build a line
    bool bIsPainting = false;    // Currently drag-painting
    FF12GridCoord LastPaintedCoord;  // Prevent duplicate paints during drag
    int32 LastPaintedTile = -1;
    
    // Drag line state
    FF12GridCoord DragStartCoord;      // Where the drag started
    FVector DragDirection;              // Direction to extend (face normal)
    FVector DragStartWorldPos;          // World position where drag started
    int32 DragFaceIndex;                // Which face was clicked
    TArray<FF12GridCoord> DragPreviewCoords;  // Coords being previewed

    // Ghost preview meshes for build mode (12 tiles per preview module)
    UPROPERTY()
    TArray<class UStaticMeshComponent*> GhostMeshComponents;
    
    // Cached face transforms for ghost positioning
    TArray<FTransform> GhostFaceTransforms;
    
    // Maximum modules per ghost set (for drag preview)
    static const int32 MaxDragPreview = 50;
    
    void InitializeGhostPreview();
    void UpdateGhostPreview();
    void UpdateDragPreview();
    void ShowGhostAtCoord(FF12GridCoord Coord, int32 GhostSetIndex = 0);
    void HideGhost();
    void HideAllGhosts();
    void PlaceDraggedModules();

    // Highlight tracking for delete mode
    FF12GridCoord LastHighlightCoord;
    int32 LastHighlightTile = -1;
    bool bLastHighlightWasSingleTile = false;
    bool bHasHighlight = false;

    // Perform trace from camera
    bool TraceFromCamera(FHitResult& OutHit);

    // Update ghost preview / cursor position
    void UpdatePreview();

    // Update delete highlight
    void UpdateDeleteHighlight();

    // Mode-specific action handlers
    void HandleBuildPrimary();
    void HandleBuildSecondary();
    void HandlePaintPrimary();
    void HandlePaintSecondary();
    void HandleDeletePrimary();
    void HandleDeleteSecondary();

    // Place a module at current grid coord
    void PlaceModule();

    // Remove the module under cursor
    void RemoveModule();

    // Cached builder pawn reference
    UPROPERTY()
    AF12BuilderPawn* CachedBuilderPawn;
};