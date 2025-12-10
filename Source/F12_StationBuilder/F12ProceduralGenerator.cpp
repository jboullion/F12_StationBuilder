// F12ProceduralGenerator.cpp
// Implementation of the Procedural Generation System

#include "F12ProceduralGenerator.h"
#include "F12Module.h"
#include "F12BuilderController.h"
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

    // Spawn modules at each coordinate
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

        // Spawn module
        FVector SpawnPos = GridSystem->GridToWorld(Coord);
        
        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        
        AF12Module* NewModule = Controller->GetWorld()->SpawnActor<AF12Module>(
            Controller->ModuleClass, SpawnPos, FRotator::ZeroRotator, SpawnParams);
        
        if (NewModule)
        {
            if (Controller->DefaultModuleMaterial)
            {
                NewModule->TileMaterial = Controller->DefaultModuleMaterial;
            }
            
            NewModule->TileMaterials = Controller->PaintMaterials;
            NewModule->GenerateModule();

            // Apply material if specified
            if (Params.MaterialIndex >= 0 && Controller->PaintMaterials.Num() > 0)
            {
                int32 MatIdx = Params.MaterialIndex % Controller->PaintMaterials.Num();
                for (int32 i = 0; i < 12; i++)
                {
                    NewModule->SetTileMaterialIndex(i, MatIdx);
                }
            }
            
            GridSystem->SetOccupied(Coord, NewModule);
            Result.CreatedCoords.Add(Coord);
            Result.ModulesCreated++;
        }
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
    if (!GridSystem)
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
                AActor* Module = GridSystem->GetModuleAt(Coord);
                
                if (Module)
                {
                    GridSystem->ClearOccupied(Coord);
                    Module->Destroy();
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
    // Clear a large region - this is a bit brute force but works
    return ClearRegion(FIntVector(-100, -100, -100), FIntVector(100, 100, 100), bPreserveCore);
}

int32 UF12ProceduralGenerator::EstimateModuleCount(const FF12GenerationParams& Params)
{
    return PreviewGeneration(Params).Num();
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
                    Coords.Add(ApplyOffset(X, Y, Z, Params));
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
                Coords.Add(ApplyOffset(X, Y, Z, Params));
            }
        }
    }

    return Coords;
}

TArray<FF12GridCoord> UF12ProceduralGenerator::GenerateHollowSphereCoords(const FF12GenerationParams& Params)
{
    TArray<FF12GridCoord> Coords;

    // Use the average of sizes as radius
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
                    Coords.Add(ApplyOffset(X, Y, Z, Params));
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
                    Coords.Add(ApplyOffset(X, Y, Z, Params));
                }
            }
        }
    }

    return Coords;
}

TArray<FF12GridCoord> UF12ProceduralGenerator::GenerateCylinderCoords(const FF12GenerationParams& Params)
{
    TArray<FF12GridCoord> Coords;

    // Cylinder along Z axis
    float RadiusX = Params.SizeX / 2.0f;
    float RadiusY = Params.SizeY / 2.0f;
    float CenterX = RadiusX;
    float CenterY = RadiusY;

    for (int32 X = 0; X < Params.SizeX; X++)
    {
        for (int32 Y = 0; Y < Params.SizeY; Y++)
        {
            // Check if point is within ellipse
            float DX = (X - CenterX) / RadiusX;
            float DY = (Y - CenterY) / RadiusY;
            float DistSq = DX * DX + DY * DY;
            
            if (DistSq <= 1.0f)
            {
                // Check if on shell
                float InnerRadiusX = FMath::Max(0.0f, RadiusX - Params.WallThickness);
                float InnerRadiusY = FMath::Max(0.0f, RadiusY - Params.WallThickness);
                float InnerDX = InnerRadiusX > 0 ? (X - CenterX) / InnerRadiusX : 999.0f;
                float InnerDY = InnerRadiusY > 0 ? (Y - CenterY) / InnerRadiusY : 999.0f;
                float InnerDistSq = InnerDX * InnerDX + InnerDY * InnerDY;
                
                for (int32 Z = 0; Z < Params.SizeZ; Z++)
                {
                    // Include if on cylinder wall OR on top/bottom caps
                    bool bOnWall = InnerDistSq > 1.0f;
                    bool bOnCap = Z < Params.WallThickness || Z >= Params.SizeZ - Params.WallThickness;
                    
                    if (bOnWall || bOnCap)
                    {
                        Coords.Add(ApplyOffset(X, Y, Z, Params));
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

    // Three intersecting bars
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
                    Coords.Add(ApplyOffset(X, Y, Z, Params));
                }
            }
        }
    }

    return Coords;
}

TArray<FF12GridCoord> UF12ProceduralGenerator::GenerateRingCoords(const FF12GenerationParams& Params)
{
    TArray<FF12GridCoord> Coords;

    // Torus in XY plane
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
                // Distance from center in XY
                float DX = X - CenterX;
                float DY = Y - CenterY;
                float DistXY = FMath::Sqrt(DX * DX + DY * DY);
                
                // Distance from the ring centerline
                float DistFromRing = FMath::Sqrt(
                    FMath::Square(DistXY - MajorRadius) + 
                    FMath::Square(Z - CenterZ)
                );
                
                if (DistFromRing <= MinorRadius)
                {
                    Coords.Add(ApplyOffset(X, Y, Z, Params));
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
    // Check if point is within Thickness distance of any face
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
        // Shift so center of shape is at offset
        OffsetX -= Params.SizeX / 2;
        OffsetY -= Params.SizeY / 2;
        OffsetZ -= Params.SizeZ / 2;
    }
    
    return FF12GridCoord(X + OffsetX, Y + OffsetY, Z + OffsetZ);
}
