// F12Module.h
// Rhombic Dodecahedron Module for F12 Station Builder
// Each module consists of 12 rhombic tiles forming a complete polyhedron

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

    // Individual tile visibility (for creating openings)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "F12")
    TArray<bool> TileVisibility;

    // Regenerate the module mesh
    UFUNCTION(BlueprintCallable, Category = "F12")
    void GenerateModule();

    // Set visibility of a specific tile
    UFUNCTION(BlueprintCallable, Category = "F12")
    void SetTileVisible(int32 TileIndex, bool bVisible);

protected:
    // The 12 procedural mesh components for each tile
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "F12")
    TArray<UProceduralMeshComponent*> TileMeshes;

private:
    // Generate geometry for a single rhombic tile at specific world positions
    void GenerateTileAtPosition(UProceduralMeshComponent* MeshComp,
        const FVector& V0, const FVector& V1, const FVector& V2, const FVector& V3,
        const FVector& Normal, float Thickness);
    
    // Legacy function - kept for compatibility
    void GenerateTileGeometry(UProceduralMeshComponent* MeshComp, float Size);

    // Get the transform for a specific face (0-11)
    FTransform GetFaceTransform(int32 FaceIndex);

    // Get the face normal direction for a specific face
    FVector GetFaceNormal(int32 FaceIndex);

    // Get the roll adjustment for proper edge alignment
    float GetFaceRollAdjust(int32 FaceIndex);
};