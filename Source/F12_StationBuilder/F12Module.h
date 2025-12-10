// F12Module.h
// Modular space station building block - Rhombic Dodecahedron (12 faces)

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

    // === SIZE ===
    
    // Overall module size (diameter of inscribed sphere)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Module")
    float ModuleSize = 400.0f;

    // Thickness of each tile
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Module")
    float TileThickness = 50.0f;

    // === MATERIALS ===
    
    // Default material for all tiles
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Module|Materials")
    UMaterialInterface* TileMaterial;

    // Array of available materials for painting
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Module|Materials")
    TArray<UMaterialInterface*> TileMaterials;

    // Track which material index each tile is using
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Module|Materials")
    TArray<int32> TileMaterialIndices;

    // === TILE MESHES ===
    
    // Array of procedural mesh components (one per face)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Module")
    TArray<UProceduralMeshComponent*> TileMeshes;

    // === GENERATION ===
    
    UFUNCTION(BlueprintCallable, Category = "Module")
    void GenerateModule();

    // === TILE OPERATIONS ===
    
    UFUNCTION(BlueprintCallable, Category = "Module|Tiles")
    void SetTileMaterial(int32 TileIndex, UMaterialInterface* Material);

    UFUNCTION(BlueprintCallable, Category = "Module|Tiles")
    void SetTileMaterialIndex(int32 TileIndex, int32 MaterialIndex);

    UFUNCTION(BlueprintCallable, Category = "Module|Tiles")
    void SetAllTilesMaterialIndex(int32 MaterialIndex);

    UFUNCTION(BlueprintCallable, Category = "Module|Tiles")
    void SetTileVisible(int32 TileIndex, bool bVisible);

    UFUNCTION(BlueprintCallable, Category = "Module|Tiles")
    bool IsTileVisible(int32 TileIndex) const;

    // === UTILITY ===
    
    UFUNCTION(BlueprintCallable, Category = "Module")
    int32 GetNumFaces() const { return 12; }

    UFUNCTION(BlueprintCallable, Category = "Module")
    FVector GetFaceNormal(int32 FaceIndex) const;

    // Get tile index from a mesh component (for raycasting)
    UFUNCTION(BlueprintCallable, Category = "Module")
    int32 GetTileIndexFromComponent(UPrimitiveComponent* Component) const;

protected:
    // Clear existing meshes
    void ClearTileMeshes();

    // Helper to create a single tile mesh
    void CreateTileMesh(int32 TileIndex, const TArray<FVector>& Vertices, const TArray<int32>& Triangles, const FVector& Normal);

    // Store face normals
    TArray<FVector> FaceNormals;
};