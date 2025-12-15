// F12StationGenerator.cpp
// Implementation of Advanced Procedural Station Generator

#include "F12StationGenerator.h"
#include "F12BuilderController.h"
#include "F12InstancedRenderer.h"
#include "Engine/World.h"

UF12StationGenerator::UF12StationGenerator()
{
    GridSystem = nullptr;
    Controller = nullptr;
    
    InitializeDefaultMaterials();
}

void UF12StationGenerator::Initialize(AF12GridSystem* InGridSystem, AF12BuilderController* InController)
{
    GridSystem = InGridSystem;
    Controller = InController;
}

void UF12StationGenerator::InitializeDefaultMaterials()
{
    // Default zone to material index mapping
    // These indices should match the TileMaterials array in F12InstancedRenderer
    ZoneMaterialMap.Add(EF12ZoneType::Structural, 2);  // White
    ZoneMaterialMap.Add(EF12ZoneType::Hub, 4);         // Tan
    ZoneMaterialMap.Add(EF12ZoneType::Habitat, 0);     // Green
    ZoneMaterialMap.Add(EF12ZoneType::Power, 3);       // Blue
    ZoneMaterialMap.Add(EF12ZoneType::Docking, 1);     // Pink
}

FF12StationResult UF12StationGenerator::GenerateStation(const FF12StationParams& Params)
{
    FF12StationResult Result;
    
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

    // Initialize random stream with seed
    RandomStream.Initialize(Params.Seed);

    // Clear working data
    WorkingModules.Empty();
    SpineCoords.Empty();
    HubCoords.Empty();

    // Clear existing modules if requested
    if (Params.bClearExisting)
    {
        ClearAll(Params.bPreserveCore);
    }

    // === GENERATION PHASES (these are fast) ===
    
    UE_LOG(LogTemp, Log, TEXT("Station Gen: Planning structure..."));
    GenerateMainSpine(Params);
    Result.SpineModules = SpineCoords.Num();
    
    GenerateHubs(Params);
    Result.HubModules = HubCoords.Num();
    
    GenerateBranches(Params);
    CalculateConnectivity();
    AssignZones();

    // === PREPARE BATCH DATA ===
    
    TArray<FF12GridCoord> AllCoords;
    TArray<int32> AllMaterials;
    
    AllCoords.Reserve(WorkingModules.Num());
    AllMaterials.Reserve(WorkingModules.Num());
    
    for (const auto& Pair : WorkingModules)
    {
        // Apply offset
        FF12GridCoord FinalCoord;
        FinalCoord.X = Pair.Key.X + Params.Offset.X;
        FinalCoord.Y = Pair.Key.Y + Params.Offset.Y;
        FinalCoord.Z = Pair.Key.Z + Params.Offset.Z;

        // Skip if already occupied
        if (GridSystem->IsOccupied(FinalCoord))
            continue;

        // Preserve core if requested
        if (Params.bPreserveCore && FinalCoord.X == 0 && FinalCoord.Y == 0 && FinalCoord.Z == 0)
            continue;

        // Determine material
        int32 MatIndex = Params.bAutoAssignMaterials ? GetMaterialForZone(Pair.Value.Zone) : 0;

        AllCoords.Add(FinalCoord);
        AllMaterials.Add(MatIndex);
        
        Result.CreatedCoords.Add(FinalCoord);
        Result.ZoneAssignments.Add(FinalCoord, Pair.Value.Zone);

        // Count branch modules
        if (Pair.Value.bIsBranch && !Pair.Value.bIsSpine && !Pair.Value.bIsHub)
        {
            Result.BranchModules++;
        }
    }

    // === PLACE ALL MODULES IN SINGLE BATCH ===
    
    if (AllCoords.Num() > 0)
    {
        UE_LOG(LogTemp, Log, TEXT("Station Gen: Placing %d modules..."), AllCoords.Num());
        
        // Mark all as occupied in grid system
        for (const FF12GridCoord& Coord : AllCoords)
        {
            GridSystem->SetOccupied(Coord, nullptr);
        }
        
        // SINGLE batch operation with materials - only ONE rebuild!
        Renderer->AddModulesWithMaterials(AllCoords, AllMaterials);
    }

    Result.TotalModules = AllCoords.Num();
    Result.bSuccess = Result.TotalModules > 0;
    Result.Message = FString::Printf(TEXT("Generated %d modules (Spine: %d, Hubs: %d, Branches: %d)"),
        Result.TotalModules, Result.SpineModules, Result.HubModules, Result.BranchModules);

    UE_LOG(LogTemp, Log, TEXT("Station Gen: Complete - %s"), *Result.Message);

    return Result;
}

TArray<FF12GridCoord> UF12StationGenerator::PreviewStation(const FF12StationParams& Params)
{
    // Initialize random stream
    RandomStream.Initialize(Params.Seed);

    // Clear working data
    WorkingModules.Empty();
    SpineCoords.Empty();
    HubCoords.Empty();

    // Run generation phases without placing
    GenerateMainSpine(Params);
    GenerateHubs(Params);
    GenerateBranches(Params);

    // Return all coordinates
    TArray<FF12GridCoord> Result;
    for (const auto& Pair : WorkingModules)
    {
        FF12GridCoord FinalCoord;
        FinalCoord.X = Pair.Key.X + Params.Offset.X;
        FinalCoord.Y = Pair.Key.Y + Params.Offset.Y;
        FinalCoord.Z = Pair.Key.Z + Params.Offset.Z;
        Result.Add(FinalCoord);
    }

    return Result;
}

int32 UF12StationGenerator::EstimateModuleCount(const FF12StationParams& Params)
{
    // Quick estimate without full generation
    int32 Estimate = 0;

    // Main spine
    Estimate += Params.MainSpineLength;

    // Hubs (rough estimate of hub modules)
    int32 NumHubs = Params.MainSpineLength / Params.HubFrequency;
    int32 HubModulesEach = Params.HubSize * Params.HubSize * 2;  // Rough cluster size
    Estimate += NumHubs * HubModulesEach;

    // Branches
    int32 AvgBranchLength = (Params.BranchLengthMin + Params.BranchLengthMax) / 2;
    int32 BranchesPerHub = Params.BranchCount / FMath::Max(1, NumHubs);
    
    // Account for symmetry
    int32 SymmetryMultiplier = 1;
    switch (Params.SymmetryMode)
    {
        case EF12SymmetryMode::Bilateral: SymmetryMultiplier = 2; break;
        case EF12SymmetryMode::Quad: SymmetryMultiplier = 4; break;
        case EF12SymmetryMode::Radial: SymmetryMultiplier = 6; break;
        default: break;
    }

    Estimate += Params.BranchCount * AvgBranchLength * SymmetryMultiplier;

    return Estimate;
}

int32 UF12StationGenerator::ClearAll(bool bPreserveCore)
{
    if (!GridSystem || !Controller || !Controller->InstancedRenderer)
        return 0;

    int32 Cleared = 0;
    AF12InstancedRenderer* Renderer = Controller->InstancedRenderer;

    // Get all occupied cells and clear them
    TArray<FF12GridCoord> ToRemove;
    
    // We need to iterate safely, so collect first
    const auto& OccupiedCells = GridSystem->GetOccupiedCells();
    for (const auto& Pair : OccupiedCells)
    {
        if (bPreserveCore && Pair.Key.X == 0 && Pair.Key.Y == 0 && Pair.Key.Z == 0)
        {
            continue;
        }
        ToRemove.Add(Pair.Key);
    }

    // Now remove them
    for (const FF12GridCoord& Coord : ToRemove)
    {
        Renderer->RemoveModule(Coord);
        GridSystem->ClearOccupied(Coord);
        Cleared++;
    }

    UE_LOG(LogTemp, Log, TEXT("Cleared %d modules"), Cleared);
    return Cleared;
}

EF12ZoneType UF12StationGenerator::GetZoneAtCoord(FF12GridCoord Coord) const
{
    const FF12ModuleMetadata* Metadata = WorkingModules.Find(Coord);
    if (Metadata)
    {
        return Metadata->Zone;
    }
    return EF12ZoneType::Structural;
}

int32 UF12StationGenerator::GetMaterialForZone(EF12ZoneType Zone) const
{
    const int32* MatIndex = ZoneMaterialMap.Find(Zone);
    if (MatIndex)
    {
        return *MatIndex;
    }
    return 0;  // Default
}

// ============================================================================
// MAIN SPINE GENERATION
// ============================================================================

void UF12StationGenerator::GenerateMainSpine(const FF12StationParams& Params)
{
    // Start at origin (will be offset later)
    FF12GridCoord Current(0, 0, 0);
    
    // For bilateral/quad symmetry, we grow in both directions from center
    // For others, grow from one end
    
    bool bGrowBothDirections = (Params.SymmetryMode == EF12SymmetryMode::Bilateral ||
                                 Params.SymmetryMode == EF12SymmetryMode::Quad);

    int32 ForwardLength = bGrowBothDirections ? Params.MainSpineLength / 2 : Params.MainSpineLength;
    int32 BackwardLength = bGrowBothDirections ? Params.MainSpineLength - ForwardLength : 0;

    // Add center module
    AddWorkingModule(Current, true, false, false);
    SpineCoords.Add(Current);

    // Grow forward
    FF12GridCoord Forward = Current;
    for (int32 i = 0; i < ForwardLength; i++)
    {
        Forward = StepAlongSpine(Forward, Params.PrimaryAxis, i);
        if (IsValidBCCPosition(Forward) && IsValidPosition(Forward))
        {
            AddWorkingModule(Forward, true, false, false);
            SpineCoords.Add(Forward);
        }
    }

    // Grow backward (if symmetric)
    if (bGrowBothDirections)
    {
        FF12GridCoord Backward = Current;
        for (int32 i = 0; i < BackwardLength; i++)
        {
            // Step in opposite direction (add 1 to make it alternate differently)
            Backward = StepAlongSpine(Backward, Params.PrimaryAxis, i + 1);
            // Negate the step
            Backward.X = Current.X - (Backward.X - Current.X);
            Backward.Y = Current.Y - (Backward.Y - Current.Y);
            Backward.Z = Current.Z - (Backward.Z - Current.Z);
            
            if (IsValidBCCPosition(Backward) && IsValidPosition(Backward))
            {
                AddWorkingModule(Backward, true, false, false);
                SpineCoords.Add(Backward);
            }
        }
    }

    UE_LOG(LogTemp, Log, TEXT("Generated main spine with %d modules"), SpineCoords.Num());
}

void UF12StationGenerator::GetAxisFacePairs(EF12PrimaryAxis Axis, int32& OutFaceA, int32& OutFaceB) const
{
    // These face pairs create a zigzag that trends along the given axis
    // Based on the neighbor offsets:
    // Face 0: (1, 0, -1), Face 2: (1, 0, 1) -> X-trending, Z oscillation
    // Face 1: (1, -1, 0), Face 3: (1, 1, 0) -> X-trending, Y oscillation
    // etc.

    switch (Axis)
    {
        case EF12PrimaryAxis::X:
            OutFaceA = 0;  // (1, 0, -1)
            OutFaceB = 2;  // (1, 0, 1)
            break;
        case EF12PrimaryAxis::Y:
            OutFaceA = 3;  // (1, 1, 0)
            OutFaceB = 8;  // (0, 1, 1)
            break;
        case EF12PrimaryAxis::Z:
            OutFaceA = 2;  // (1, 0, 1)
            OutFaceB = 8;  // (0, 1, 1)
            break;
    }
}

TArray<int32> UF12StationGenerator::GetPerpendicularFaces(EF12PrimaryAxis Axis) const
{
    TArray<int32> Faces;
    
    // Return faces that are roughly perpendicular to the given axis
    switch (Axis)
    {
        case EF12PrimaryAxis::X:
            // Y and Z trending faces
            Faces.Add(8);   // (0, 1, 1)
            Faces.Add(9);   // (0, 1, -1)
            Faces.Add(10);  // (0, -1, 1)
            Faces.Add(11);  // (0, -1, -1)
            break;
        case EF12PrimaryAxis::Y:
            // X and Z trending faces
            Faces.Add(0);   // (1, 0, -1)
            Faces.Add(2);   // (1, 0, 1)
            Faces.Add(4);   // (-1, 0, -1)
            Faces.Add(6);   // (-1, 0, 1)
            break;
        case EF12PrimaryAxis::Z:
            // X and Y trending faces
            Faces.Add(1);   // (1, -1, 0)
            Faces.Add(3);   // (1, 1, 0)
            Faces.Add(5);   // (-1, 1, 0)
            Faces.Add(7);   // (-1, -1, 0)
            break;
    }
    
    return Faces;
}

FF12GridCoord UF12StationGenerator::StepAlongSpine(FF12GridCoord Current, EF12PrimaryAxis Axis, int32 StepIndex) const
{
    int32 FaceA, FaceB;
    GetAxisFacePairs(Axis, FaceA, FaceB);
    
    // Alternate between the two faces for zigzag
    int32 FaceToUse = (StepIndex % 2 == 0) ? FaceA : FaceB;
    
    return StepInDirection(Current, FaceToUse);
}

FF12GridCoord UF12StationGenerator::StepInDirection(FF12GridCoord Current, int32 FaceIndex) const
{
    TArray<FIntVector> Offsets = GetNeighborOffsets();
    
    if (FaceIndex >= 0 && FaceIndex < Offsets.Num())
    {
        FIntVector Offset = Offsets[FaceIndex];
        return FF12GridCoord(
            Current.X + Offset.X,
            Current.Y + Offset.Y,
            Current.Z + Offset.Z
        );
    }
    
    return Current;
}

// ============================================================================
// HUB GENERATION
// ============================================================================

void UF12StationGenerator::GenerateHubs(const FF12StationParams& Params)
{
    if (Params.HubFrequency <= 0 || SpineCoords.Num() == 0)
        return;

    // Place hubs at regular intervals along the spine
    for (int32 i = 0; i < SpineCoords.Num(); i += Params.HubFrequency)
    {
        FF12GridCoord HubCenter = SpineCoords[i];
        GenerateHubCluster(HubCenter, Params.HubSize);
        HubCoords.Add(HubCenter);
        
        // Mark as hub
        if (FF12ModuleMetadata* Metadata = WorkingModules.Find(HubCenter))
        {
            Metadata->bIsHub = true;
        }
    }

    // Always add hub at start and end of spine
    if (SpineCoords.Num() > 0)
    {
        FF12GridCoord StartHub = SpineCoords[0];
        FF12GridCoord EndHub = SpineCoords.Last();
        
        if (!HubCoords.Contains(StartHub))
        {
            GenerateHubCluster(StartHub, Params.HubSize);
            HubCoords.Add(StartHub);
            if (FF12ModuleMetadata* Metadata = WorkingModules.Find(StartHub))
            {
                Metadata->bIsHub = true;
            }
        }
        
        if (!HubCoords.Contains(EndHub))
        {
            GenerateHubCluster(EndHub, Params.HubSize);
            HubCoords.Add(EndHub);
            if (FF12ModuleMetadata* Metadata = WorkingModules.Find(EndHub))
            {
                Metadata->bIsHub = true;
            }
        }
    }

    UE_LOG(LogTemp, Log, TEXT("Generated %d hubs"), HubCoords.Num());
}

void UF12StationGenerator::GenerateHubCluster(FF12GridCoord Center, int32 Radius)
{
    if (Radius <= 0)
        return;

    // Simple BFS-like expansion from center
    TSet<FF12GridCoord> Visited;
    TArray<FF12GridCoord> CurrentRing;
    CurrentRing.Add(Center);
    Visited.Add(Center);

    for (int32 Ring = 0; Ring < Radius; Ring++)
    {
        TArray<FF12GridCoord> NextRing;
        
        for (const FF12GridCoord& Coord : CurrentRing)
        {
            // Get all valid neighbors
            TArray<FIntVector> Offsets = GetNeighborOffsets();
            
            for (const FIntVector& Offset : Offsets)
            {
                FF12GridCoord Neighbor(
                    Coord.X + Offset.X,
                    Coord.Y + Offset.Y,
                    Coord.Z + Offset.Z
                );
                
                if (!Visited.Contains(Neighbor) && IsValidBCCPosition(Neighbor))
                {
                    // Random chance to add (creates organic shapes)
                    if (RandomStream.FRand() < 0.7f)
                    {
                        AddWorkingModule(Neighbor, false, true, false);
                        NextRing.Add(Neighbor);
                    }
                    Visited.Add(Neighbor);
                }
            }
        }
        
        CurrentRing = NextRing;
    }
}

// ============================================================================
// BRANCH GENERATION
// ============================================================================

void UF12StationGenerator::GenerateBranches(const FF12StationParams& Params)
{
    if (Params.BranchCount <= 0 || HubCoords.Num() == 0)
        return;

    TArray<int32> PerpendicularFaces = GetPerpendicularFaces(Params.PrimaryAxis);
    
    if (PerpendicularFaces.Num() == 0)
        return;

    // Distribute branches among hubs
    int32 BranchesPerHub = FMath::Max(1, Params.BranchCount / HubCoords.Num());
    int32 BranchesCreated = 0;

    for (const FF12GridCoord& HubCenter : HubCoords)
    {
        if (BranchesCreated >= Params.BranchCount)
            break;

        // Create branches from this hub
        for (int32 b = 0; b < BranchesPerHub && BranchesCreated < Params.BranchCount; b++)
        {
            // Pick a random perpendicular direction
            int32 FaceIndex = PerpendicularFaces[RandomStream.RandRange(0, PerpendicularFaces.Num() - 1)];
            
            // Random branch length
            int32 BranchLength = RandomStream.RandRange(Params.BranchLengthMin, Params.BranchLengthMax);
            
            // Generate the branch
            FF12GridCoord BranchPos = HubCenter;
            TArray<FF12GridCoord> BranchCoords;
            
            for (int32 i = 0; i < BranchLength; i++)
            {
                BranchPos = StepInDirection(BranchPos, FaceIndex);
                
                if (IsValidBCCPosition(BranchPos) && IsValidPosition(BranchPos))
                {
                    AddWorkingModule(BranchPos, false, false, true);
                    BranchCoords.Add(BranchPos);
                }
                else
                {
                    break;  // Hit obstacle
                }
                
                // Occasionally alternate direction for organic look
                if (RandomStream.FRand() < 0.3f && i < BranchLength - 1)
                {
                    // Pick a different perpendicular face
                    int32 NewFace = PerpendicularFaces[RandomStream.RandRange(0, PerpendicularFaces.Num() - 1)];
                    if (NewFace != FaceIndex)
                    {
                        FaceIndex = NewFace;
                    }
                }
            }

            // Apply symmetry to this branch
            if (Params.SymmetryMode != EF12SymmetryMode::None)
            {
                for (const FF12GridCoord& BranchCoord : BranchCoords)
                {
                    TArray<FF12GridCoord> SymmetricCoords = GetSymmetricCoords(
                        BranchCoord, Params.SymmetryMode, Params.PrimaryAxis);
                    
                    for (const FF12GridCoord& SymCoord : SymmetricCoords)
                    {
                        if (IsValidBCCPosition(SymCoord) && IsValidPosition(SymCoord))
                        {
                            AddWorkingModule(SymCoord, false, false, true);
                        }
                    }
                }
            }

            BranchesCreated++;
        }
    }

    UE_LOG(LogTemp, Log, TEXT("Generated %d branches"), BranchesCreated);
}

TArray<FF12GridCoord> UF12StationGenerator::GetSymmetricCoords(
    FF12GridCoord Coord, EF12SymmetryMode Mode, EF12PrimaryAxis Axis) const
{
    TArray<FF12GridCoord> Result;
    
    if (Mode == EF12SymmetryMode::None)
        return Result;

    // Mirror based on symmetry mode and axis
    switch (Mode)
    {
        case EF12SymmetryMode::Bilateral:
        {
            // Mirror across the plane perpendicular to one axis
            FF12GridCoord Mirrored = Coord;
            switch (Axis)
            {
                case EF12PrimaryAxis::X:
                    Mirrored.Y = -Mirrored.Y;
                    break;
                case EF12PrimaryAxis::Y:
                    Mirrored.X = -Mirrored.X;
                    break;
                case EF12PrimaryAxis::Z:
                    Mirrored.X = -Mirrored.X;
                    break;
            }
            if (!(Mirrored == Coord))
            {
                Result.Add(Mirrored);
            }
            break;
        }
        
        case EF12SymmetryMode::Quad:
        {
            // 4-way symmetry (mirror on two axes)
            FF12GridCoord M1 = Coord, M2 = Coord, M3 = Coord;
            switch (Axis)
            {
                case EF12PrimaryAxis::X:
                    M1.Y = -M1.Y;
                    M2.Z = -M2.Z;
                    M3.Y = -M3.Y; M3.Z = -M3.Z;
                    break;
                case EF12PrimaryAxis::Y:
                    M1.X = -M1.X;
                    M2.Z = -M2.Z;
                    M3.X = -M3.X; M3.Z = -M3.Z;
                    break;
                case EF12PrimaryAxis::Z:
                    M1.X = -M1.X;
                    M2.Y = -M2.Y;
                    M3.X = -M3.X; M3.Y = -M3.Y;
                    break;
            }
            if (!(M1 == Coord)) Result.Add(M1);
            if (!(M2 == Coord)) Result.Add(M2);
            if (!(M3 == Coord)) Result.Add(M3);
            break;
        }
        
        case EF12SymmetryMode::Radial:
        {
            // 6-way rotational symmetry around the primary axis
            // This is approximate for BCC - we rotate in 60 degree increments
            for (int32 i = 1; i < 6; i++)
            {
                float Angle = i * 60.0f * PI / 180.0f;
                float CosA = FMath::Cos(Angle);
                float SinA = FMath::Sin(Angle);
                
                FF12GridCoord Rotated = Coord;
                switch (Axis)
                {
                    case EF12PrimaryAxis::X:
                    {
                        float NewY = Coord.Y * CosA - Coord.Z * SinA;
                        float NewZ = Coord.Y * SinA + Coord.Z * CosA;
                        Rotated.Y = FMath::RoundToInt(NewY);
                        Rotated.Z = FMath::RoundToInt(NewZ);
                        break;
                    }
                    case EF12PrimaryAxis::Y:
                    {
                        float NewX = Coord.X * CosA - Coord.Z * SinA;
                        float NewZ = Coord.X * SinA + Coord.Z * CosA;
                        Rotated.X = FMath::RoundToInt(NewX);
                        Rotated.Z = FMath::RoundToInt(NewZ);
                        break;
                    }
                    case EF12PrimaryAxis::Z:
                    {
                        float NewX = Coord.X * CosA - Coord.Y * SinA;
                        float NewY = Coord.X * SinA + Coord.Y * CosA;
                        Rotated.X = FMath::RoundToInt(NewX);
                        Rotated.Y = FMath::RoundToInt(NewY);
                        break;
                    }
                }
                
                // Snap to valid BCC position
                if (IsValidBCCPosition(Rotated) && !(Rotated == Coord))
                {
                    Result.Add(Rotated);
                }
            }
            break;
        }
        
        default:
            break;
    }
    
    return Result;
}

// ============================================================================
// ZONE ASSIGNMENT
// ============================================================================

void UF12StationGenerator::CalculateConnectivity()
{
    TArray<FIntVector> Offsets = GetNeighborOffsets();
    
    for (auto& Pair : WorkingModules)
    {
        int32 NeighborCount = 0;
        
        for (const FIntVector& Offset : Offsets)
        {
            FF12GridCoord Neighbor(
                Pair.Key.X + Offset.X,
                Pair.Key.Y + Offset.Y,
                Pair.Key.Z + Offset.Z
            );
            
            if (WorkingModules.Contains(Neighbor))
            {
                NeighborCount++;
            }
        }
        
        Pair.Value.Connectivity = NeighborCount;
        Pair.Value.bIsEndpoint = (NeighborCount == 1);
    }
}

void UF12StationGenerator::AssignZones()
{
    for (auto& Pair : WorkingModules)
    {
        FF12ModuleMetadata& Meta = Pair.Value;
        
        // Priority-based zone assignment
        
        // 1. Endpoints are always docking
        if (Meta.bIsEndpoint)
        {
            Meta.Zone = EF12ZoneType::Docking;
        }
        // 2. Hub centers are utility/hub
        else if (Meta.bIsHub)
        {
            Meta.Zone = EF12ZoneType::Hub;
        }
        // 3. Spine modules are structural
        else if (Meta.bIsSpine)
        {
            Meta.Zone = EF12ZoneType::Structural;
        }
        // 4. High connectivity (surrounded) = habitat
        else if (Meta.Connectivity >= 4)
        {
            Meta.Zone = EF12ZoneType::Habitat;
        }
        // 5. Low connectivity (exterior) = power
        else if (Meta.Connectivity <= 2)
        {
            Meta.Zone = EF12ZoneType::Power;
        }
        // 6. Default to structural
        else
        {
            Meta.Zone = EF12ZoneType::Structural;
        }
    }
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

bool UF12StationGenerator::IsValidPosition(FF12GridCoord Coord) const
{
    // Check if position is not already in our working set
    return !WorkingModules.Contains(Coord);
}

void UF12StationGenerator::AddWorkingModule(FF12GridCoord Coord, bool bIsSpine, bool bIsHub, bool bIsBranch)
{
    if (WorkingModules.Contains(Coord))
    {
        // Update flags if already exists
        FF12ModuleMetadata& Meta = WorkingModules[Coord];
        Meta.bIsSpine = Meta.bIsSpine || bIsSpine;
        Meta.bIsHub = Meta.bIsHub || bIsHub;
        Meta.bIsBranch = Meta.bIsBranch || bIsBranch;
        return;
    }

    FF12ModuleMetadata Meta;
    Meta.Coord = Coord;
    Meta.bIsSpine = bIsSpine;
    Meta.bIsHub = bIsHub;
    Meta.bIsBranch = bIsBranch;
    Meta.Zone = EF12ZoneType::Structural;  // Default, will be reassigned
    
    WorkingModules.Add(Coord, Meta);
}

bool UF12StationGenerator::IsValidBCCPosition(FF12GridCoord Coord) const
{
    // Valid BCC positions have X + Y + Z as even
    return ((Coord.X + Coord.Y + Coord.Z) % 2) == 0;
}

TArray<FIntVector> UF12StationGenerator::GetNeighborOffsets() const
{
    // Same as F12GridSystem::GetNeighborOffsets()
    TArray<FIntVector> Offsets;
    
    // Faces 0-3: around +X octahedral
    Offsets.Add(FIntVector( 1,  0, -1));  // Face 0
    Offsets.Add(FIntVector( 1, -1,  0));  // Face 1
    Offsets.Add(FIntVector( 1,  0,  1));  // Face 2
    Offsets.Add(FIntVector( 1,  1,  0));  // Face 3
    
    // Faces 4-7: around -X octahedral
    Offsets.Add(FIntVector(-1,  0, -1));  // Face 4
    Offsets.Add(FIntVector(-1,  1,  0));  // Face 5
    Offsets.Add(FIntVector(-1,  0,  1));  // Face 6
    Offsets.Add(FIntVector(-1, -1,  0));  // Face 7
    
    // Faces 8-11: connecting Y and Z axes
    Offsets.Add(FIntVector( 0,  1,  1));  // Face 8
    Offsets.Add(FIntVector( 0,  1, -1));  // Face 9
    Offsets.Add(FIntVector( 0, -1,  1));  // Face 10
    Offsets.Add(FIntVector( 0, -1, -1));  // Face 11
    
    return Offsets;
}
