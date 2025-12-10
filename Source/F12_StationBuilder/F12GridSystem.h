// F12GridSystem.h
// Handles the BCC lattice grid for placing rhombic dodecahedron modules
// Rhombic dodecahedra tessellate on a Body-Centered Cubic (BCC) lattice

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "F12GridSystem.generated.h"

// Grid coordinate for a module position
USTRUCT(BlueprintType)
struct FF12GridCoord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 X = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Y = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Z = 0;

    FF12GridCoord() {}
    FF12GridCoord(int32 InX, int32 InY, int32 InZ) : X(InX), Y(InY), Z(InZ) {}

    bool operator==(const FF12GridCoord& Other) const
    {
        return X == Other.X && Y == Other.Y && Z == Other.Z;
    }

    bool operator!=(const FF12GridCoord& Other) const
    {
        return !(*this == Other);
    }

    friend uint32 GetTypeHash(const FF12GridCoord& Coord)
    {
        return HashCombine(HashCombine(GetTypeHash(Coord.X), GetTypeHash(Coord.Y)), GetTypeHash(Coord.Z));
    }
};

UCLASS()
class AF12GridSystem : public AActor
{
    GENERATED_BODY()

public:
    AF12GridSystem();

    // Size of each module (should match F12Module::ModuleSize)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
    float ModuleSize = 400.0f;

    // Convert world position to nearest grid coordinate
    UFUNCTION(BlueprintCallable, Category = "Grid")
    FF12GridCoord WorldToGrid(FVector WorldPosition);

    // Convert grid coordinate to world position
    UFUNCTION(BlueprintCallable, Category = "Grid")
    FVector GridToWorld(FF12GridCoord GridCoord);

    // Check if a grid position is occupied
    UFUNCTION(BlueprintCallable, Category = "Grid")
    bool IsOccupied(FF12GridCoord GridCoord);

    // Mark a grid position as occupied
    UFUNCTION(BlueprintCallable, Category = "Grid")
    void SetOccupied(FF12GridCoord GridCoord, AActor* Module);

    // Remove occupancy
    UFUNCTION(BlueprintCallable, Category = "Grid")
    void ClearOccupied(FF12GridCoord GridCoord);

    // Get the module at a grid position (nullptr if empty)
    UFUNCTION(BlueprintCallable, Category = "Grid")
    AActor* GetModuleAt(FF12GridCoord GridCoord);

    // Get all 12 neighboring grid coordinates for a given position
    UFUNCTION(BlueprintCallable, Category = "Grid")
    TArray<FF12GridCoord> GetNeighborCoords(FF12GridCoord GridCoord);

    // Find which face of a module was clicked based on hit location
    // Returns face index 0-11, or -1 if not determinable
    UFUNCTION(BlueprintCallable, Category = "Grid")
    int32 GetHitFaceIndex(FF12GridCoord ModuleCoord, FVector HitLocation);

    // Get the grid coordinate of the neighboring module for a given face
    UFUNCTION(BlueprintCallable, Category = "Grid")
    FF12GridCoord GetNeighborCoordForFace(FF12GridCoord ModuleCoord, int32 FaceIndex);

    // The 12 neighbor offset directions in grid space (public for drag build)
    // These correspond to the 12 faces of the rhombic dodecahedron
    UFUNCTION(BlueprintCallable, Category = "Grid")
    TArray<FIntVector> GetNeighborOffsets();
    
    // Face normals for determining which face was hit (public for visualization)
    UFUNCTION(BlueprintCallable, Category = "Grid")
    TArray<FVector> GetFaceNormals();

protected:
    // Map of occupied positions to module actors
    UPROPERTY()
    TMap<FF12GridCoord, AActor*> OccupiedPositions;
};