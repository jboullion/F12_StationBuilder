// F12InstancedRenderer.h
// Optimized Instance-Based Rendering for F12 Modules using Static Mesh
// Uses HierarchicalInstancedStaticMeshComponent for maximum performance

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "F12GridSystem.h"
#include "F12InstancedRenderer.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UStaticMesh;

// Data stored per module
USTRUCT()
struct FF12ModuleInstanceData
{
    GENERATED_BODY()

    // Material index for each of the 12 tiles (0-based index into TileMaterials array)
    UPROPERTY()
    TArray<int32> TileMaterials;

    // Visibility for each tile
    UPROPERTY()
    TArray<bool> TileVisibility;

    FF12ModuleInstanceData()
    {
        TileMaterials.SetNum(12);
        TileVisibility.SetNum(12);
        for (int32 i = 0; i < 12; i++)
        {
            TileMaterials[i] = 0;
            TileVisibility[i] = true;
        }
    }
};

// Key for instance lookup: combines HISM component index with instance index
USTRUCT()
struct FF12InstanceKey
{
    GENERATED_BODY()

    UPROPERTY()
    int32 ComponentIndex = 0;

    UPROPERTY()
    int32 InstanceIndex = 0;

    FF12InstanceKey() {}
    FF12InstanceKey(int32 InComp, int32 InInst) : ComponentIndex(InComp), InstanceIndex(InInst) {}

    bool operator==(const FF12InstanceKey& Other) const
    {
        return ComponentIndex == Other.ComponentIndex && InstanceIndex == Other.InstanceIndex;
    }

    friend uint32 GetTypeHash(const FF12InstanceKey& Key)
    {
        return HashCombine(GetTypeHash(Key.ComponentIndex), GetTypeHash(Key.InstanceIndex));
    }
};

// Data mapping an instance back to its source module and tile
USTRUCT()
struct FF12InstanceSourceData
{
    GENERATED_BODY()

    UPROPERTY()
    FF12GridCoord GridCoord;

    UPROPERTY()
    int32 TileIndex = 0;

    FF12InstanceSourceData() {}
    FF12InstanceSourceData(FF12GridCoord InCoord, int32 InTile) : GridCoord(InCoord), TileIndex(InTile) {}
};

/**
 * Renders F12 modules using GPU instancing for maximum performance.
 * Each module's 12 tiles are rendered as instances of a static mesh.
 */
UCLASS()
class AF12InstancedRenderer : public AActor
{
    GENERATED_BODY()

public:
    AF12InstancedRenderer();

    virtual void BeginPlay() override;

    // === CONFIGURATION ===

    // The static mesh to use for tiles (import from Blender)
    // REQUIRED - must be set before play
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "F12|Mesh")
    UStaticMesh* TileStaticMesh;

    // Materials for each tile type (index 0 = default, 1-N = paint materials)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "F12|Materials")
    TArray<UMaterialInterface*> TileMaterials;

    // Module geometry settings (must match your static mesh and grid system)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "F12|Geometry")
    float ModuleSize = 600.0f;

    // === MODULE MANAGEMENT ===

    // Add a module at the given grid coordinate
    UFUNCTION(BlueprintCallable, Category = "F12|Modules")
    void AddModule(FF12GridCoord GridCoord, int32 MaterialIndex = 0);

    // Add multiple modules at once (more efficient than individual adds)
    UFUNCTION(BlueprintCallable, Category = "F12|Modules")
    void AddModulesBulk(const TArray<FF12GridCoord>& GridCoords, int32 MaterialIndex = 0);

    // Remove a module
    UFUNCTION(BlueprintCallable, Category = "F12|Modules")
    void RemoveModule(FF12GridCoord GridCoord);

    // Clear all modules
    UFUNCTION(BlueprintCallable, Category = "F12|Modules")
    void ClearAll();

    // Check if a module exists at coordinate
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "F12|Modules")
    bool HasModule(FF12GridCoord GridCoord) const;

    // === TILE OPERATIONS ===

    // Set material for a specific tile
    UFUNCTION(BlueprintCallable, Category = "F12|Tiles")
    void SetTileMaterial(FF12GridCoord GridCoord, int32 TileIndex, int32 MaterialIndex);

    // Set material for all tiles in a module
    UFUNCTION(BlueprintCallable, Category = "F12|Tiles")
    void SetModuleMaterial(FF12GridCoord GridCoord, int32 MaterialIndex);

    // Set tile visibility
    UFUNCTION(BlueprintCallable, Category = "F12|Tiles")
    void SetTileVisible(FF12GridCoord GridCoord, int32 TileIndex, bool bVisible);

    // Get tile material index
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "F12|Tiles")
    int32 GetTileMaterial(FF12GridCoord GridCoord, int32 TileIndex) const;

    // Get tile visibility
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "F12|Tiles")
    bool GetTileVisible(FF12GridCoord GridCoord, int32 TileIndex) const;

    // === HIGHLIGHT SYSTEM ===

    // Highlight material for delete mode hover
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "F12|Materials")
    UMaterialInterface* HighlightMaterial;

    // Set highlight on a specific tile (for delete hover)
    // If bSingleTile is true, only highlight that one tile; otherwise highlight entire module
    UFUNCTION(BlueprintCallable, Category = "F12|Highlight")
    void SetTileHighlight(FF12GridCoord GridCoord, int32 TileIndex, bool bHighlight, bool bSingleTile = false);

    // Clear all highlights
    UFUNCTION(BlueprintCallable, Category = "F12|Highlight")
    void ClearAllHighlights();

    // === RAYCASTING ===

    // Get the module and tile index from a hit result
    UFUNCTION(BlueprintCallable, Category = "F12|Interaction")
    bool GetHitModuleAndTile(const FHitResult& Hit, FF12GridCoord& OutGridCoord, int32& OutTileIndex) const;

    // === STATISTICS ===

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "F12|Stats")
    int32 GetModuleCount() const { return ModuleData.Num(); }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "F12|Stats")
    int32 GetTotalInstanceCount() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "F12|Stats")
    FString GetPerformanceStats() const;

    // Get face transforms for ghost preview
    UFUNCTION(BlueprintCallable, Category = "F12|Geometry")
    const TArray<FTransform>& GetFaceTransforms() const { return FaceTransforms; }

protected:
    // HISM components organized by [FaceIndex * NumMaterials + MaterialIndex]
    UPROPERTY()
    TArray<UHierarchicalInstancedStaticMeshComponent*> HISMComponents;

    // Separate HISM for highlighted tiles (12 instances for full module highlight)
    UPROPERTY()
    UHierarchicalInstancedStaticMeshComponent* HighlightHISM;

    // Currently highlighted module/tile
    FF12GridCoord HighlightedCoord;
    int32 HighlightedTileIndex = -1;  // -1 means full module, 0-11 means single tile
    bool bHasHighlight = false;

    // Module data storage
    UPROPERTY()
    TMap<FF12GridCoord, FF12ModuleInstanceData> ModuleData;

    // Instance-to-source mapping: maps (ComponentIndex, InstanceIndex) -> (GridCoord, TileIndex)
    // This allows us to know exactly which module/tile was hit by a raycast
    UPROPERTY()
    TMap<FF12InstanceKey, FF12InstanceSourceData> InstanceToSourceMap;

    // Cached face transforms (computed once at BeginPlay)
    TArray<FTransform> FaceTransforms;

    // Number of materials
    int32 NumMaterials = 1;

    // Initialize HISM components
    void InitializeHISMComponents();

    // Compute the 12 face transforms for a rhombic dodecahedron
    void ComputeFaceTransforms();

    // Rebuild all instances (called after changes)
    void RebuildInstances();

    // Get HISM component for a face and material
    UHierarchicalInstancedStaticMeshComponent* GetHISMForFaceAndMaterial(int32 FaceIndex, int32 MaterialIndex);

    // Get world transform for a tile
    FTransform GetTileWorldTransform(FF12GridCoord GridCoord, int32 TileIndex) const;

    // Reference to grid system for coordinate conversion
    UPROPERTY()
    AF12GridSystem* GridSystem;
};