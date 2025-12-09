// F12Module.h
// Rhombic Dodecahedron Module for F12 Station Builder

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "F12Module.generated.h"

class UProceduralMeshComponent;

UCLASS()
class AF12Module : public AActor
{
    GENERATED_BODY()

public:
    AF12Module();

    virtual void OnConstruction(const FTransform& Transform) override;

    // Module size (diameter in Unreal units, 100 = 1 meter)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "F12")
    float ModuleSize = 400.0f;  // 4 meters

    // Material to apply to tiles
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "F12")
    UMaterialInterface* TileMaterial;

    // Array of materials for painting different tile types
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "F12")
    TArray<UMaterialInterface*> TileMaterials;

    // Individual tile visibility (for creating openings)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "F12")
    TArray<bool> TileVisibility;

    // Individual tile material indices
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "F12")
    TArray<int32> TileMaterialIndices;

    // Regenerate the module mesh
    UFUNCTION(BlueprintCallable, Category = "F12")
    void GenerateModule();

    // === TILE MANAGEMENT ===
    
    // Set visibility and collision of a specific tile
    UFUNCTION(BlueprintCallable, Category = "F12")
    void SetTileVisible(int32 TileIndex, bool bVisible);

    // Check if a tile is visible
    UFUNCTION(BlueprintCallable, Category = "F12")
    bool IsTileVisible(int32 TileIndex) const;

    // Set material on a specific tile
    UFUNCTION(BlueprintCallable, Category = "F12")
    void SetTileMaterial(int32 TileIndex, UMaterialInterface* Material);

    // Set material by index from TileMaterials array
    UFUNCTION(BlueprintCallable, Category = "F12")
    void SetTileMaterialIndex(int32 TileIndex, int32 MaterialIndex);

    // Cycle to next material on a tile
    UFUNCTION(BlueprintCallable, Category = "F12")
    void CycleTileMaterial(int32 TileIndex);

    // Set material on ALL tiles
    UFUNCTION(BlueprintCallable, Category = "F12")
    void SetAllTilesMaterial(UMaterialInterface* Material);

    // Cycle material on ALL tiles
    UFUNCTION(BlueprintCallable, Category = "F12")
    void CycleAllTilesMaterial();

    // Get the tile index from a hit component
    UFUNCTION(BlueprintCallable, Category = "F12")
    int32 GetTileIndexFromComponent(UPrimitiveComponent* Component) const;

    // Get face normal for a specific tile (used by grid system)
    UFUNCTION(BlueprintCallable, Category = "F12")
    FVector GetFaceNormal(int32 FaceIndex);

    // The 12 procedural mesh components for each tile (public for highlight system)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "F12")
    TArray<UProceduralMeshComponent*> TileMeshes;

protected:
    // Current material index for cycling (module-wide)
    UPROPERTY()
    int32 CurrentMaterialIndex = 0;

private:
    // Generate geometry for a single tile at specific world positions
    void GenerateTileAtPosition(UProceduralMeshComponent* MeshComp,
        const FVector& V0, const FVector& V1, const FVector& V2, const FVector& V3,
        const FVector& Normal, float Thickness);

    // Legacy function - kept for compatibility
    void GenerateTileGeometry(UProceduralMeshComponent* MeshComp, float Size);

    // Get the transform for a specific face (0-11)
    FTransform GetFaceTransform(int32 FaceIndex);

    // Get the roll adjustment (legacy, not used)
    float GetFaceRollAdjust(int32 FaceIndex);
};