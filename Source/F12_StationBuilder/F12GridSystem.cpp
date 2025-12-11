// F12GridSystem.cpp
// Implementation of the BCC lattice grid system

#include "F12GridSystem.h"

AF12GridSystem::AF12GridSystem()
{
    PrimaryActorTick.bCanEverTick = false;
}

TArray<FIntVector> AF12GridSystem::GetNeighborOffsets()
{
    // Offsets matching the face normals in GetFaceNormals()
    // Each offset points in the direction of the corresponding face
    
    TArray<FIntVector> Offsets;
    
    // Faces 0-3: around +X octahedral
    Offsets.Add(FIntVector( 1,  0, -1));  // Face 0: +X, -Z
    Offsets.Add(FIntVector( 1, -1,  0));  // Face 1: +X, -Y
    Offsets.Add(FIntVector( 1,  0,  1));  // Face 2: +X, +Z
    Offsets.Add(FIntVector( 1,  1,  0));  // Face 3: +X, +Y
    
    // Faces 4-7: around -X octahedral
    Offsets.Add(FIntVector(-1,  0, -1));  // Face 4: -X, -Z
    Offsets.Add(FIntVector(-1,  1,  0));  // Face 5: -X, +Y
    Offsets.Add(FIntVector(-1,  0,  1));  // Face 6: -X, +Z
    Offsets.Add(FIntVector(-1, -1,  0));  // Face 7: -X, -Y
    
    // Faces 8-11: connecting Y and Z axes
    Offsets.Add(FIntVector( 0,  1,  1));  // Face 8: +Y, +Z
    Offsets.Add(FIntVector( 0,  1, -1));  // Face 9: +Y, -Z
    Offsets.Add(FIntVector( 0, -1,  1));  // Face 10: -Y, +Z
    Offsets.Add(FIntVector( 0, -1, -1));  // Face 11: -Y, -Z
    
    return Offsets;
}

TArray<FVector> AF12GridSystem::GetFaceNormals()
{
    // Face normals matching F12Module's FRhombicDodecahedron
    // These are computed from the face vertex positions
    static const float S = 0.707107f;  // sqrt(2)/2
    
    TArray<FVector> Normals;
    
    // Faces 0-3: around +X octahedral
    Normals.Add(FVector( S,  0, -S));   // Face 0: +X, -Z
    Normals.Add(FVector( S, -S,  0));   // Face 1: +X, -Y
    Normals.Add(FVector( S,  0,  S));   // Face 2: +X, +Z
    Normals.Add(FVector( S,  S,  0));   // Face 3: +X, +Y
    
    // Faces 4-7: around -X octahedral
    Normals.Add(FVector(-S,  0, -S));   // Face 4: -X, -Z
    Normals.Add(FVector(-S,  S,  0));   // Face 5: -X, +Y
    Normals.Add(FVector(-S,  0,  S));   // Face 6: -X, +Z
    Normals.Add(FVector(-S, -S,  0));   // Face 7: -X, -Y
    
    // Faces 8-11: connecting Y and Z axes
    Normals.Add(FVector( 0,  S,  S));   // Face 8: +Y, +Z
    Normals.Add(FVector( 0,  S, -S));   // Face 9: +Y, -Z
    Normals.Add(FVector( 0, -S,  S));   // Face 10: -Y, +Z
    Normals.Add(FVector( 0, -S, -S));   // Face 11: -Y, -Z
    
    return Normals;
}

FF12GridCoord AF12GridSystem::WorldToGrid(FVector WorldPosition)
{
    // Convert world position to grid coordinates
    // The grid spacing must account for tile thickness to prevent overlap
    // 
    // Geometry:
    // - Face center is at distance R = ModuleSize/2 from module center
    // - Tiles have thickness T, extending T/2 outward
    // - For tiles to just touch (not overlap): center-to-center = 2*(R + T/2)
    // - Base spacing factor is sqrt(2)/2 ≈ 0.707 for BCC lattice
    // - Adjusted factor = 0.707 * (ModuleSize + TileThickness) / ModuleSize
    
    float AdjustedSpacing = ModuleSize * 0.707f * (ModuleSize + TileThickness) / ModuleSize;
    
    FF12GridCoord Coord;
    Coord.X = FMath::RoundToInt(WorldPosition.X / AdjustedSpacing);
    Coord.Y = FMath::RoundToInt(WorldPosition.Y / AdjustedSpacing);
    Coord.Z = FMath::RoundToInt(WorldPosition.Z / AdjustedSpacing);
    
    return Coord;
}

FVector AF12GridSystem::GridToWorld(FF12GridCoord GridCoord)
{
    // Convert grid coordinates to world position
    // Use adjusted spacing to prevent tile overlap
    float AdjustedSpacing = ModuleSize * 0.707f * (ModuleSize + TileThickness) / ModuleSize;
    
    return FVector(
        GridCoord.X * AdjustedSpacing,
        GridCoord.Y * AdjustedSpacing,
        GridCoord.Z * AdjustedSpacing
    );
}

bool AF12GridSystem::IsOccupied(FF12GridCoord GridCoord)
{
    return OccupiedPositions.Contains(GridCoord);
}

void AF12GridSystem::SetOccupied(FF12GridCoord GridCoord, AActor* Module)
{
    OccupiedPositions.Add(GridCoord, Module);
}

void AF12GridSystem::ClearOccupied(FF12GridCoord GridCoord)
{
    OccupiedPositions.Remove(GridCoord);
}

AActor* AF12GridSystem::GetModuleAt(FF12GridCoord GridCoord)
{
    AActor** Found = OccupiedPositions.Find(GridCoord);
    return Found ? *Found : nullptr;
}

TArray<FF12GridCoord> AF12GridSystem::GetNeighborCoords(FF12GridCoord GridCoord)
{
    TArray<FF12GridCoord> Neighbors;
    TArray<FIntVector> Offsets = GetNeighborOffsets();
    
    for (const FIntVector& Offset : Offsets)
    {
        Neighbors.Add(FF12GridCoord(
            GridCoord.X + Offset.X,
            GridCoord.Y + Offset.Y,
            GridCoord.Z + Offset.Z
        ));
    }
    
    return Neighbors;
}

int32 AF12GridSystem::GetHitFaceIndex(FF12GridCoord ModuleCoord, FVector HitLocation)
{
    // Determine which face was hit based on the direction from module center to hit point
    FVector ModuleCenter = GridToWorld(ModuleCoord);
    FVector HitDirection = (HitLocation - ModuleCenter).GetSafeNormal();
    
    TArray<FVector> Normals = GetFaceNormals();
    
    // Find the face whose normal is most aligned with the hit direction
    int32 BestFace = -1;
    float BestDot = -2.0f;
    
    for (int32 i = 0; i < 12; i++)
    {
        float Dot = FVector::DotProduct(HitDirection, Normals[i]);
        if (Dot > BestDot)
        {
            BestDot = Dot;
            BestFace = i;
        }
    }
    
    return BestFace;
}

FF12GridCoord AF12GridSystem::GetNeighborCoordForFace(FF12GridCoord ModuleCoord, int32 FaceIndex)
{
    TArray<FIntVector> Offsets = GetNeighborOffsets();
    
    if (FaceIndex >= 0 && FaceIndex < 12)
    {
        FIntVector Offset = Offsets[FaceIndex];
        return FF12GridCoord(
            ModuleCoord.X + Offset.X,
            ModuleCoord.Y + Offset.Y,
            ModuleCoord.Z + Offset.Z
        );
    }
    
    return ModuleCoord;
}

FVector AF12GridSystem::GetFaceNormal(int32 FaceIndex)
{
    TArray<FVector> Normals = GetFaceNormals();
    
    if (FaceIndex >= 0 && FaceIndex < 12)
    {
        return Normals[FaceIndex];
    }
    
    return FVector::UpVector;
}

FIntVector AF12GridSystem::GetGridOffsetForFace(int32 FaceIndex)
{
    TArray<FIntVector> Offsets = GetNeighborOffsets();
    
    if (FaceIndex >= 0 && FaceIndex < 12)
    {
        return Offsets[FaceIndex];
    }
    
    return FIntVector(0, 0, 1);  // Default to up
}

float AF12GridSystem::GetModuleSpacing() const
{
    // Distance between adjacent module centers
    // Must account for tile thickness to match GridToWorld
    float AdjustedSpacing = ModuleSize * 0.707f * (ModuleSize + TileThickness) / ModuleSize;
    
    // For BCC lattice, diagonal offsets like (1,0,-1) have magnitude sqrt(2)
    return AdjustedSpacing * FMath::Sqrt(2.0f);
}