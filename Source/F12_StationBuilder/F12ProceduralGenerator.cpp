// F12ProceduralGenerator.cpp
// Implementation of the Procedural Generation System
// Updated to use instanced rendering

#include "F12ProceduralGenerator.h"
#include "F12BuilderController.h"
#include "F12InstancedRenderer.h"
#include "Engine/World.h"

UF12ProceduralGenerator::UF12ProceduralGenerator()
{
    GridSystem = nullptr;
    Controller = nullptr;
}

void UF12ProceduralGenerator::Initialize(AF12GridSystem* InGridSystem, AF12BuilderController* InController)
{
    GridSystem = InGridSystem;
    Controller = InController;
}

FF12GenerationResult UF12ProceduralGenerator::Generate(const FF12GenerationParams& Params)
{
    FF12GenerationResult Result;
    
    if (!GridSystem || !Controller)
    {
        Result.Message = TEXT("Generator not initialized");
        return Result;
    }

    AF12InstancedRenderer* Renderer = Controller->InstancedRenderer;
    if (!Renderer)
    {
        Result.Message = TEXT("No instanced renderer found");
        return Result;
    }

    // Get coordinates to generate
    TArray<FF12GridCoord> Coords = PreviewGeneration(Params);
    
    if (Coords.Num() == 0)
    {
        Result.Message = TEXT("No valid coordinates to generate");
        return Result;
    }

    // Clear existing if requested
    if (Params.bClearExisting)
    {
        // Calculate bounds
        FIntVector MinCoord(INT_MAX, INT_MAX, INT_MAX);
        FIntVector MaxCoord(INT_MIN, INT_MIN, INT_MIN);
        
        for (const FF12GridCoord& Coord : Coords)
        {
            MinCoord.X = FMath::Min(MinCoord.X, Coord.X);
            MinCoord.Y = FMath::Min(MinCoord.Y, Coord.Y);
            MinCoord.Z = FMath::Min(MinCoord.Z, Coord.Z);
            MaxCoord.X = FMath::Max(MaxCoord.X, Coord.X);
            MaxCoord.Y = FMath::Max(MaxCoord.Y, Coord.Y);
            MaxCoord.Z = FMath::Max(MaxCoord.Z, Coord.Z);
        }
        
        ClearRegion(MinCoord, MaxCoord, Params.bPreserveCore);
    }

    // Filter valid coordinates
    TArray<FF12GridCoord> ValidCoords;
    
    for (const FF12GridCoord& Coord : Coords)
    {
        // Skip if occupied
        if (GridSystem->IsOccupied(Coord))
        {
            Result.ModulesSkipped++;
            continue;
        }

        // Skip core if preserving
        if (Params.bPreserveCore && Coord.X == 0 && Coord.Y == 0 && Coord.Z == 0)
        {
            Result.ModulesSkipped++;
            continue;
        }

        ValidCoords.Add(Coord);
        GridSystem->SetOccupied(Coord, nullptr);
        Result.CreatedCoords.Add(Coord);
        Result.ModulesCreated++;
    }

    // Determine material index
    int32 MatIdx = 0;
    if (Params.MaterialIndex >= 0)
    {
        MatIdx = Params.MaterialIndex;
    }

    // Add all modules to instanced renderer at once
    if (ValidCoords.Num() > 0)
    {
        Renderer->AddModulesBulk(ValidCoords, MatIdx);
    }

    Result.bSuccess = Result.ModulesCreated > 0;
    Result.Message = FString::Printf(TEXT("Created %d modules, skipped %d"), 
        Result.ModulesCreated, Result.ModulesSkipped);
    
    UE_LOG(LogTemp, Log, TEXT("Generation complete: %s"), *Result.Message);
    
    return Result;
}

TArray<FF12GridCoord> UF12ProceduralGenerator::PreviewGeneration(const FF12GenerationParams& Params)
{
    switch (Params.Shape)
    {
        case EF12GeneratorShape::HollowBox:
            return GenerateHollowBoxCoords(Params);
        case EF12GeneratorShape::SolidBox:
            return GenerateSolidBoxCoords(Params);
        case EF12GeneratorShape::HollowSphere:
            return GenerateHollowSphereCoords(Params);
        case EF12GeneratorShape::SolidSphere:
            return GenerateSolidSphereCoords(Params);
        case EF12GeneratorShape::Cylinder:
            return GenerateCylinderCoords(Params);
        case EF12GeneratorShape::Cross:
            return GenerateCrossCoords(Params);
        case EF12GeneratorShape::Ring:
            return GenerateRingCoords(Params);
        default:
            return TArray<FF12GridCoord>();
    }
}

int32 UF12ProceduralGenerator::ClearRegion(FIntVector MinCoord, FIntVector MaxCoord, bool bPreserveCore)
{
    if (!GridSystem || !Controller || !Controller->InstancedRenderer)
        return 0;

    int32 Cleared = 0;

    for (int32 X = MinCoord.X; X <= MaxCoord.X; X++)
    {
        for (int32 Y = MinCoord.Y; Y <= MaxCoord.Y; Y++)
        {
            for (int32 Z = MinCoord.Z; Z <= MaxCoord.Z; Z++)
            {
                // Skip core if preserving
                if (bPreserveCore && X == 0 && Y == 0 && Z == 0)
                    continue;

                FF12GridCoord Coord(X, Y, Z);
                
                if (GridSystem->IsOccupied(Coord))
                {
                    Controller->InstancedRenderer->RemoveModule(Coord);
                    GridSystem->ClearOccupied(Coord);
                    Cleared++;
                }
            }
        }
    }

    UE_LOG(LogTemp, Log, TEXT("Cleared %d modules in region"), Cleared);
    return Cleared;
}

int32 UF12ProceduralGenerator::ClearAll(bool bPreserveCore)
{
    return ClearRegion(FIntVector(-100, -100, -100), FIntVector(100, 100, 100), bPreserveCore);
}

int32 UF12ProceduralGenerator::EstimateModuleCount(const FF12GenerationParams& Params)
{
    return PreviewGeneration(Params).Num();
}

// ============================================================================
// BCC LATTICE VALIDATION
// ============================================================================

bool UF12ProceduralGenerator::IsValidBCCPosition(const FF12GridCoord& Coord)
{
    // Rhombic dodecahedra tessellate on a BCC (body-centered cubic) lattice.
    // Valid positions are where X + Y + Z is even.
    return ((Coord.X + Coord.Y + Coord.Z) % 2) == 0;
}

// ============================================================================
// SHAPE GENERATION FUNCTIONS
// ============================================================================

TArray<FF12GridCoord> UF12ProceduralGenerator::GenerateHollowBoxCoords(const FF12GenerationParams& Params)
{
    TArray<FF12GridCoord> Coords;

    for (int32 X = 0; X < Params.SizeX; X++)
    {
        for (int32 Y = 0; Y < Params.SizeY; Y++)
        {
            for (int32 Z = 0; Z < Params.SizeZ; Z++)
            {
                if (IsOnBoxShell(X, Y, Z, Params.SizeX, Params.SizeY, Params.SizeZ, Params.WallThickness))
                {
                    FF12GridCoord Coord = ApplyOffset(X, Y, Z, Params);
                    if (IsValidBCCPosition(Coord))
                    {
                        Coords.Add(Coord);
                    }
                }
            }
        }
    }

    return Coords;
}

TArray<FF12GridCoord> UF12ProceduralGenerator::GenerateSolidBoxCoords(const FF12GenerationParams& Params)
{
    TArray<FF12GridCoord> Coords;

    for (int32 X = 0; X < Params.SizeX; X++)
    {
        for (int32 Y = 0; Y < Params.SizeY; Y++)
        {
            for (int32 Z = 0; Z < Params.SizeZ; Z++)
            {
                FF12GridCoord Coord = ApplyOffset(X, Y, Z, Params);
                if (IsValidBCCPosition(Coord))
                {
                    Coords.Add(Coord);
                }
            }
        }
    }

    return Coords;
}

TArray<FF12GridCoord> UF12ProceduralGenerator::GenerateHollowSphereCoords(const FF12GenerationParams& Params)
{
    TArray<FF12GridCoord> Coords;

    float Radius = (Params.SizeX + Params.SizeY + Params.SizeZ) / 6.0f;
    float InnerRadius = FMath::Max(0.0f, Radius - Params.WallThickness);
    
    int32 Diameter = FMath::CeilToInt(Radius * 2.0f);
    float CenterX = Radius;
    float CenterY = Radius;
    float CenterZ = Radius;

    for (int32 X = 0; X <= Diameter; X++)
    {
        for (int32 Y = 0; Y <= Diameter; Y++)
        {
            for (int32 Z = 0; Z <= Diameter; Z++)
            {
                bool bInOuter = IsInSphere(X, Y, Z, CenterX, CenterY, CenterZ, Radius);
                bool bInInner = IsInSphere(X, Y, Z, CenterX, CenterY, CenterZ, InnerRadius);
                
                if (bInOuter && !bInInner)
                {
                    FF12GridCoord Coord = ApplyOffset(X, Y, Z, Params);
                    if (IsValidBCCPosition(Coord))
                    {
                        Coords.Add(Coord);
                    }
                }
            }
        }
    }

    return Coords;
}

TArray<FF12GridCoord> UF12ProceduralGenerator::GenerateSolidSphereCoords(const FF12GenerationParams& Params)
{
    TArray<FF12GridCoord> Coords;

    float Radius = (Params.SizeX + Params.SizeY + Params.SizeZ) / 6.0f;
    int32 Diameter = FMath::CeilToInt(Radius * 2.0f);
    float CenterX = Radius;
    float CenterY = Radius;
    float CenterZ = Radius;

    for (int32 X = 0; X <= Diameter; X++)
    {
        for (int32 Y = 0; Y <= Diameter; Y++)
        {
            for (int32 Z = 0; Z <= Diameter; Z++)
            {
                if (IsInSphere(X, Y, Z, CenterX, CenterY, CenterZ, Radius))
                {
                    FF12GridCoord Coord = ApplyOffset(X, Y, Z, Params);
                    if (IsValidBCCPosition(Coord))
                    {
                        Coords.Add(Coord);
                    }
                }
            }
        }
    }

    return Coords;
}

TArray<FF12GridCoord> UF12ProceduralGenerator::GenerateCylinderCoords(const FF12GenerationParams& Params)
{
    TArray<FF12GridCoord> Coords;

    float RadiusX = Params.SizeX / 2.0f;
    float RadiusY = Params.SizeY / 2.0f;
    float CenterX = RadiusX;
    float CenterY = RadiusY;

    for (int32 X = 0; X < Params.SizeX; X++)
    {
        for (int32 Y = 0; Y < Params.SizeY; Y++)
        {
            float DX = (X - CenterX) / RadiusX;
            float DY = (Y - CenterY) / RadiusY;
            float DistSq = DX * DX + DY * DY;
            
            if (DistSq <= 1.0f)
            {
                float InnerRadiusX = FMath::Max(0.0f, RadiusX - Params.WallThickness);
                float InnerRadiusY = FMath::Max(0.0f, RadiusY - Params.WallThickness);
                float InnerDX = InnerRadiusX > 0 ? (X - CenterX) / InnerRadiusX : 999.0f;
                float InnerDY = InnerRadiusY > 0 ? (Y - CenterY) / InnerRadiusY : 999.0f;
                float InnerDistSq = InnerDX * InnerDX + InnerDY * InnerDY;
                
                for (int32 Z = 0; Z < Params.SizeZ; Z++)
                {
                    bool bOnWall = InnerDistSq > 1.0f;
                    bool bOnCap = Z < Params.WallThickness || Z >= Params.SizeZ - Params.WallThickness;
                    
                    if (bOnWall || bOnCap)
                    {
                        FF12GridCoord Coord = ApplyOffset(X, Y, Z, Params);
                        if (IsValidBCCPosition(Coord))
                        {
                            Coords.Add(Coord);
                        }
                    }
                }
            }
        }
    }

    return Coords;
}

TArray<FF12GridCoord> UF12ProceduralGenerator::GenerateCrossCoords(const FF12GenerationParams& Params)
{
    TArray<FF12GridCoord> Coords;

    int32 ArmWidth = FMath::Max(1, Params.WallThickness);
    
    int32 CenterX = Params.SizeX / 2;
    int32 CenterY = Params.SizeY / 2;
    int32 CenterZ = Params.SizeZ / 2;

    for (int32 X = 0; X < Params.SizeX; X++)
    {
        for (int32 Y = 0; Y < Params.SizeY; Y++)
        {
            for (int32 Z = 0; Z < Params.SizeZ; Z++)
            {
                bool bInXArm = FMath::Abs(Y - CenterY) < ArmWidth && FMath::Abs(Z - CenterZ) < ArmWidth;
                bool bInYArm = FMath::Abs(X - CenterX) < ArmWidth && FMath::Abs(Z - CenterZ) < ArmWidth;
                bool bInZArm = FMath::Abs(X - CenterX) < ArmWidth && FMath::Abs(Y - CenterY) < ArmWidth;
                
                if (bInXArm || bInYArm || bInZArm)
                {
                    FF12GridCoord Coord = ApplyOffset(X, Y, Z, Params);
                    if (IsValidBCCPosition(Coord))
                    {
                        Coords.Add(Coord);
                    }
                }
            }
        }
    }

    return Coords;
}

TArray<FF12GridCoord> UF12ProceduralGenerator::GenerateRingCoords(const FF12GenerationParams& Params)
{
    TArray<FF12GridCoord> Coords;

    float MajorRadius = FMath::Min(Params.SizeX, Params.SizeY) / 2.0f - Params.WallThickness;
    float MinorRadius = (float)Params.WallThickness;
    
    float CenterX = Params.SizeX / 2.0f;
    float CenterY = Params.SizeY / 2.0f;
    float CenterZ = Params.SizeZ / 2.0f;

    for (int32 X = 0; X < Params.SizeX; X++)
    {
        for (int32 Y = 0; Y < Params.SizeY; Y++)
        {
            for (int32 Z = 0; Z < Params.SizeZ; Z++)
            {
                float DX = X - CenterX;
                float DY = Y - CenterY;
                float DistXY = FMath::Sqrt(DX * DX + DY * DY);
                
                float DistFromRing = FMath::Sqrt(
                    FMath::Square(DistXY - MajorRadius) + 
                    FMath::Square(Z - CenterZ)
                );
                
                if (DistFromRing <= MinorRadius)
                {
                    FF12GridCoord Coord = ApplyOffset(X, Y, Z, Params);
                    if (IsValidBCCPosition(Coord))
                    {
                        Coords.Add(Coord);
                    }
                }
            }
        }
    }

    return Coords;
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

bool UF12ProceduralGenerator::IsOnBoxShell(int32 X, int32 Y, int32 Z, int32 SizeX, int32 SizeY, int32 SizeZ, int32 Thickness)
{
    bool bNearMinX = X < Thickness;
    bool bNearMaxX = X >= SizeX - Thickness;
    bool bNearMinY = Y < Thickness;
    bool bNearMaxY = Y >= SizeY - Thickness;
    bool bNearMinZ = Z < Thickness;
    bool bNearMaxZ = Z >= SizeZ - Thickness;
    
    return bNearMinX || bNearMaxX || bNearMinY || bNearMaxY || bNearMinZ || bNearMaxZ;
}

bool UF12ProceduralGenerator::IsInSphere(int32 X, int32 Y, int32 Z, float CenterX, float CenterY, float CenterZ, float Radius)
{
    float DX = X - CenterX;
    float DY = Y - CenterY;
    float DZ = Z - CenterZ;
    float DistSq = DX * DX + DY * DY + DZ * DZ;
    return DistSq <= Radius * Radius;
}

FF12GridCoord UF12ProceduralGenerator::ApplyOffset(int32 X, int32 Y, int32 Z, const FF12GenerationParams& Params)
{
    int32 OffsetX = Params.Offset.X;
    int32 OffsetY = Params.Offset.Y;
    int32 OffsetZ = Params.Offset.Z;
    
    if (Params.bCenterOnOffset)
    {
        OffsetX -= Params.SizeX / 2;
        OffsetY -= Params.SizeY / 2;
        OffsetZ -= Params.SizeZ / 2;
    }
    
    return FF12GridCoord(X + OffsetX, Y + OffsetY, Z + OffsetZ);
}
