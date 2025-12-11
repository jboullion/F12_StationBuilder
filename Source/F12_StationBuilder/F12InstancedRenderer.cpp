// F12InstancedRenderer.cpp
// Implementation of Optimized Instance-Based Rendering
// FIXED: Hit detection now uses instance tracking instead of nearest-module approximation

#include "F12InstancedRenderer.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"

AF12InstancedRenderer::AF12InstancedRenderer()
{
    PrimaryActorTick.bCanEverTick = false;
    
    // Create root component for HISM attachment
    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
    SetRootComponent(Root);
    
    // Will be populated in BeginPlay
    GridSystem = nullptr;
}

void AF12InstancedRenderer::BeginPlay()
{
    Super::BeginPlay();

    UE_LOG(LogTemp, Log, TEXT("F12InstancedRenderer::BeginPlay starting..."));

    // Find grid system
    GridSystem = Cast<AF12GridSystem>(
        UGameplayStatics::GetActorOfClass(GetWorld(), AF12GridSystem::StaticClass())
    );

    if (!GridSystem)
    {
        UE_LOG(LogTemp, Warning, TEXT("F12InstancedRenderer: No GridSystem found at BeginPlay (will retry later)"));
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("F12InstancedRenderer: Found GridSystem"));
    }

    // Validate static mesh
    if (!TileStaticMesh)
    {
        UE_LOG(LogTemp, Error, TEXT("F12InstancedRenderer: TileStaticMesh is not set! Assign your imported Blender mesh."));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("F12InstancedRenderer: TileStaticMesh = %s"), *TileStaticMesh->GetName());

    // Ensure we have at least one material
    if (TileMaterials.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("F12InstancedRenderer: No materials set, using mesh default material"));
        TileMaterials.Add(nullptr);  // Will use mesh's default material
    }

    NumMaterials = TileMaterials.Num();
    UE_LOG(LogTemp, Log, TEXT("F12InstancedRenderer: %d materials configured"), NumMaterials);

    // Compute face transforms
    ComputeFaceTransforms();
    UE_LOG(LogTemp, Log, TEXT("F12InstancedRenderer: Computed %d face transforms"), FaceTransforms.Num());

    // Initialize HISM components
    InitializeHISMComponents();

    UE_LOG(LogTemp, Log, TEXT("F12InstancedRenderer: BeginPlay complete. Ready to render modules."));
}

void AF12InstancedRenderer::ComputeFaceTransforms()
{
    FaceTransforms.Empty();
    FaceTransforms.SetNum(12);

    float R = ModuleSize * 0.5f;
    float Scale = R / FMath::Sqrt(2.0f);
    
    // Define the 14 vertices
    TArray<FVector> Vertices;
    Vertices.SetNum(14);
    
    // Cubic vertices
    Vertices[0] = FVector(-1, -1, -1) * Scale;
    Vertices[1] = FVector( 1, -1, -1) * Scale;
    Vertices[2] = FVector(-1,  1, -1) * Scale;
    Vertices[3] = FVector( 1,  1, -1) * Scale;
    Vertices[4] = FVector(-1, -1,  1) * Scale;
    Vertices[5] = FVector( 1, -1,  1) * Scale;
    Vertices[6] = FVector(-1,  1,  1) * Scale;
    Vertices[7] = FVector( 1,  1,  1) * Scale;
    
    // Octahedral vertices
    Vertices[8]  = FVector( 2,  0,  0) * Scale;
    Vertices[9]  = FVector(-2,  0,  0) * Scale;
    Vertices[10] = FVector( 0,  2,  0) * Scale;
    Vertices[11] = FVector( 0, -2,  0) * Scale;
    Vertices[12] = FVector( 0,  0,  2) * Scale;
    Vertices[13] = FVector( 0,  0, -2) * Scale;
    
    // Face definitions: [cubic1, octahedral1, cubic2, octahedral2]
    int32 FaceVertices[12][4] = {
        {3, 8, 1, 13},   // Face 0
        {1, 8, 5, 11},   // Face 1
        {5, 8, 7, 12},   // Face 2
        {7, 8, 3, 10},   // Face 3
        {0, 9, 2, 13},   // Face 4
        {2, 9, 6, 10},   // Face 5
        {6, 9, 4, 12},   // Face 6
        {4, 9, 0, 11},   // Face 7
        {7, 10, 6, 12},  // Face 8
        {2, 10, 3, 13},  // Face 9
        {4, 11, 5, 12},  // Face 10
        {1, 11, 0, 13},  // Face 11
    };
    
    for (int32 FaceIdx = 0; FaceIdx < 12; FaceIdx++)
    {
        FVector Cubic1 = Vertices[FaceVertices[FaceIdx][0]];
        FVector Oct1   = Vertices[FaceVertices[FaceIdx][1]];
        FVector Cubic2 = Vertices[FaceVertices[FaceIdx][2]];
        FVector Oct2   = Vertices[FaceVertices[FaceIdx][3]];
        
        // Face center
        FVector Center = (Cubic1 + Oct1 + Cubic2 + Oct2) * 0.25f;
        
        // Tile mesh orientation:
        // +X = long diagonal direction
        // +Y = short diagonal direction  
        // +Z = face normal (outward)
        
        // Compute axes - order matters for right-handed coordinate system!
        FVector TileZ = Center.GetSafeNormal();              // Normal pointing outward
        FVector TileX = (Oct1 - Oct2).GetSafeNormal();       // Long diagonal
        
        // Compute Y to ensure right-handed system: Y = Z cross X
        FVector TileY = FVector::CrossProduct(TileZ, TileX).GetSafeNormal();
        
        // Recompute X to ensure orthonormal: X = Y cross Z
        TileX = FVector::CrossProduct(TileY, TileZ).GetSafeNormal();
        
        // Build rotation from axes
        FMatrix RotMatrix;
        RotMatrix.SetIdentity();
        RotMatrix.SetAxis(0, TileX);  // X axis
        RotMatrix.SetAxis(1, TileY);  // Y axis
        RotMatrix.SetAxis(2, TileZ);  // Z axis
        
        FQuat Rotation = RotMatrix.ToQuat();
        
        FaceTransforms[FaceIdx] = FTransform(Rotation, Center, FVector::OneVector);
    }
}

void AF12InstancedRenderer::InitializeHISMComponents()
{
    // Clear any existing
    for (auto* HISM : HISMComponents)
    {
        if (HISM)
        {
            HISM->DestroyComponent();
        }
    }
    HISMComponents.Empty();

    if (HighlightHISM)
    {
        HighlightHISM->DestroyComponent();
        HighlightHISM = nullptr;
    }

    if (!TileStaticMesh)
    {
        UE_LOG(LogTemp, Error, TEXT("InitializeHISMComponents: TileStaticMesh is NULL!"));
        return;
    }

    // Create one HISM per face per material = 12 * NumMaterials
    int32 TotalComponents = 12 * NumMaterials;
    HISMComponents.SetNum(TotalComponents);

    for (int32 FaceIdx = 0; FaceIdx < 12; FaceIdx++)
    {
        for (int32 MatIdx = 0; MatIdx < NumMaterials; MatIdx++)
        {
            int32 ComponentIdx = FaceIdx * NumMaterials + MatIdx;
            
            UHierarchicalInstancedStaticMeshComponent* HISM = NewObject<UHierarchicalInstancedStaticMeshComponent>(this);
            HISM->SetStaticMesh(TileStaticMesh);
            HISM->SetMobility(EComponentMobility::Movable);
            HISM->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            HISM->SetCollisionResponseToAllChannels(ECR_Block);
            HISM->SetCanEverAffectNavigation(false);  // Disable navigation to prevent errors
            
            // Set material
            if (TileMaterials.IsValidIndex(MatIdx) && TileMaterials[MatIdx])
            {
                HISM->SetMaterial(0, TileMaterials[MatIdx]);
            }
            
            // Attach and register
            HISM->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
            HISM->RegisterComponent();
            
            HISMComponents[ComponentIdx] = HISM;
        }
    }

    // Create highlight HISM (renders on top for delete hover)
    HighlightHISM = NewObject<UHierarchicalInstancedStaticMeshComponent>(this);
    HighlightHISM->SetStaticMesh(TileStaticMesh);
    HighlightHISM->SetMobility(EComponentMobility::Movable);
    HighlightHISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HighlightHISM->SetCanEverAffectNavigation(false);  // Disable navigation
    if (HighlightMaterial)
    {
        HighlightHISM->SetMaterial(0, HighlightMaterial);
    }
    HighlightHISM->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
    HighlightHISM->RegisterComponent();

    UE_LOG(LogTemp, Log, TEXT("Created %d HISM components (12 faces x %d materials) + 1 highlight. Mesh: %s"), 
        TotalComponents, NumMaterials, *TileStaticMesh->GetName());
}

UHierarchicalInstancedStaticMeshComponent* AF12InstancedRenderer::GetHISMForFaceAndMaterial(int32 FaceIndex, int32 MaterialIndex)
{
    if (FaceIndex < 0 || FaceIndex >= 12)
        return nullptr;
    
    MaterialIndex = FMath::Clamp(MaterialIndex, 0, NumMaterials - 1);
    int32 ComponentIdx = FaceIndex * NumMaterials + MaterialIndex;
    
    if (HISMComponents.IsValidIndex(ComponentIdx))
    {
        return HISMComponents[ComponentIdx];
    }
    
    return nullptr;
}

FTransform AF12InstancedRenderer::GetTileWorldTransform(FF12GridCoord GridCoord, int32 TileIndex) const
{
    if (TileIndex < 0 || TileIndex >= 12)
        return FTransform::Identity;

    // Lazy lookup of GridSystem if not cached
    if (!GridSystem)
    {
        // Cast away const for lazy init
        AF12InstancedRenderer* MutableThis = const_cast<AF12InstancedRenderer*>(this);
        MutableThis->GridSystem = Cast<AF12GridSystem>(
            UGameplayStatics::GetActorOfClass(GetWorld(), AF12GridSystem::StaticClass())
        );
        
        if (!GridSystem)
        {
            UE_LOG(LogTemp, Error, TEXT("GetTileWorldTransform: No GridSystem found!"));
            return FTransform::Identity;
        }
    }

    // Get module world position
    FVector ModulePos = GridSystem->GridToWorld(GridCoord);
    
    // Get face local transform
    if (!FaceTransforms.IsValidIndex(TileIndex))
    {
        UE_LOG(LogTemp, Error, TEXT("GetTileWorldTransform: Invalid TileIndex %d, FaceTransforms has %d elements"), 
            TileIndex, FaceTransforms.Num());
        return FTransform::Identity;
    }
    
    FTransform FaceLocal = FaceTransforms[TileIndex];
    
    // Combine: face transform relative to module center
    FTransform ModuleTransform(FQuat::Identity, ModulePos, FVector::OneVector);
    
    FTransform Result = FaceLocal * ModuleTransform;
    
    return Result;
}

// === MODULE MANAGEMENT ===

void AF12InstancedRenderer::AddModule(FF12GridCoord GridCoord, int32 MaterialIndex)
{
    if (ModuleData.Contains(GridCoord))
        return;  // Already exists

    // Validate we have HISM components
    if (HISMComponents.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("AddModule: No HISM components! Was BeginPlay called?"));
        return;
    }

    // Create module data
    FF12ModuleInstanceData Data;
    for (int32 i = 0; i < 12; i++)
    {
        Data.TileMaterials[i] = MaterialIndex;
        Data.TileVisibility[i] = true;
    }
    
    ModuleData.Add(GridCoord, Data);
    
    int32 InstancesAdded = 0;
    
    // Add instances for each visible tile
    for (int32 TileIdx = 0; TileIdx < 12; TileIdx++)
    {
        if (Data.TileVisibility[TileIdx])
        {
            UHierarchicalInstancedStaticMeshComponent* HISM = GetHISMForFaceAndMaterial(TileIdx, MaterialIndex);
            if (HISM)
            {
                FTransform TileTransform = GetTileWorldTransform(GridCoord, TileIdx);
                int32 InstanceIdx = HISM->AddInstance(TileTransform, true);
                
                // Track this instance -> source mapping
                int32 ComponentIdx = TileIdx * NumMaterials + FMath::Clamp(MaterialIndex, 0, NumMaterials - 1);
                FF12InstanceKey Key(ComponentIdx, InstanceIdx);
                InstanceToSourceMap.Add(Key, FF12InstanceSourceData(GridCoord, TileIdx));
                
                InstancesAdded++;
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("AddModule: No HISM for face %d, material %d"), TileIdx, MaterialIndex);
            }
        }
    }
    
    UE_LOG(LogTemp, Log, TEXT("AddModule at (%d,%d,%d): Added %d instances, Total modules: %d"), 
        GridCoord.X, GridCoord.Y, GridCoord.Z, InstancesAdded, ModuleData.Num());
}

void AF12InstancedRenderer::AddModulesBulk(const TArray<FF12GridCoord>& GridCoords, int32 MaterialIndex)
{
    for (const FF12GridCoord& Coord : GridCoords)
    {
        if (!ModuleData.Contains(Coord))
        {
            FF12ModuleInstanceData Data;
            for (int32 i = 0; i < 12; i++)
            {
                Data.TileMaterials[i] = MaterialIndex;
                Data.TileVisibility[i] = true;
            }
            ModuleData.Add(Coord, Data);
        }
    }
    
    // Rebuild all instances (more efficient for bulk adds)
    RebuildInstances();
}

void AF12InstancedRenderer::RemoveModule(FF12GridCoord GridCoord)
{
    if (!ModuleData.Contains(GridCoord))
        return;

    ModuleData.Remove(GridCoord);
    RebuildInstances();
}

void AF12InstancedRenderer::ClearAll()
{
    ModuleData.Empty();
    InstanceToSourceMap.Empty();
    
    for (auto* HISM : HISMComponents)
    {
        if (HISM)
        {
            HISM->ClearInstances();
        }
    }
}

bool AF12InstancedRenderer::HasModule(FF12GridCoord GridCoord) const
{
    return ModuleData.Contains(GridCoord);
}

// === TILE OPERATIONS ===

void AF12InstancedRenderer::SetTileMaterial(FF12GridCoord GridCoord, int32 TileIndex, int32 MaterialIndex)
{
    if (!ModuleData.Contains(GridCoord) || TileIndex < 0 || TileIndex >= 12)
        return;

    int32 OldMatIdx = ModuleData[GridCoord].TileMaterials[TileIndex];
    int32 NewMatIdx = FMath::Clamp(MaterialIndex, 0, NumMaterials - 1);
    
    if (OldMatIdx == NewMatIdx)
        return;  // No change needed

    // Update data
    ModuleData[GridCoord].TileMaterials[TileIndex] = NewMatIdx;
    
    // Only rebuild if tile is visible
    if (ModuleData[GridCoord].TileVisibility[TileIndex])
    {
        RebuildInstances();
    }
}

void AF12InstancedRenderer::SetModuleMaterial(FF12GridCoord GridCoord, int32 MaterialIndex)
{
    if (!ModuleData.Contains(GridCoord))
        return;

    MaterialIndex = FMath::Clamp(MaterialIndex, 0, NumMaterials - 1);
    
    bool bChanged = false;
    for (int32 i = 0; i < 12; i++)
    {
        if (ModuleData[GridCoord].TileMaterials[i] != MaterialIndex)
        {
            ModuleData[GridCoord].TileMaterials[i] = MaterialIndex;
            bChanged = true;
        }
    }
    
    if (bChanged)
    {
        RebuildInstances();
    }
}

void AF12InstancedRenderer::SetTileVisible(FF12GridCoord GridCoord, int32 TileIndex, bool bVisible)
{
    if (!ModuleData.Contains(GridCoord) || TileIndex < 0 || TileIndex >= 12)
        return;

    ModuleData[GridCoord].TileVisibility[TileIndex] = bVisible;
    RebuildInstances();
}

int32 AF12InstancedRenderer::GetTileMaterial(FF12GridCoord GridCoord, int32 TileIndex) const
{
    if (!ModuleData.Contains(GridCoord) || TileIndex < 0 || TileIndex >= 12)
        return 0;

    return ModuleData[GridCoord].TileMaterials[TileIndex];
}

bool AF12InstancedRenderer::GetTileVisible(FF12GridCoord GridCoord, int32 TileIndex) const
{
    if (!ModuleData.Contains(GridCoord) || TileIndex < 0 || TileIndex >= 12)
        return false;

    return ModuleData[GridCoord].TileVisibility[TileIndex];
}

// === HIGHLIGHT SYSTEM ===

void AF12InstancedRenderer::SetTileHighlight(FF12GridCoord GridCoord, int32 TileIndex, bool bHighlight, bool bSingleTile)
{
    if (!HighlightHISM)
        return;

    // Check if we're already highlighting this exact thing
    if (bHasHighlight && HighlightedCoord == GridCoord && bHighlight)
    {
        if (bSingleTile && HighlightedTileIndex == TileIndex)
            return;  // Already highlighting this tile
        if (!bSingleTile && HighlightedTileIndex == -1)
            return;  // Already highlighting this module
    }

    // Clear previous highlights
    HighlightHISM->ClearInstances();
    bHasHighlight = false;
    HighlightedTileIndex = -1;

    if (bHighlight && ModuleData.Contains(GridCoord))
    {
        if (bSingleTile)
        {
            // Highlight just the one tile
            if (TileIndex >= 0 && TileIndex < 12 && ModuleData[GridCoord].TileVisibility[TileIndex])
            {
                FTransform TileTransform = GetTileWorldTransform(GridCoord, TileIndex);
                TileTransform.SetScale3D(FVector(1.02f, 1.02f, 1.02f));
                HighlightHISM->AddInstance(TileTransform, true);
                
                HighlightedCoord = GridCoord;
                HighlightedTileIndex = TileIndex;
                bHasHighlight = true;
            }
        }
        else
        {
            // Add highlight instances for ALL 12 tiles of the module
            for (int32 i = 0; i < 12; i++)
            {
                if (ModuleData[GridCoord].TileVisibility[i])
                {
                    FTransform TileTransform = GetTileWorldTransform(GridCoord, i);
                    TileTransform.SetScale3D(FVector(1.02f, 1.02f, 1.02f));
                    HighlightHISM->AddInstance(TileTransform, false);
                }
            }
            
            HighlightHISM->MarkRenderStateDirty();
            
            HighlightedCoord = GridCoord;
            HighlightedTileIndex = -1;  // -1 means full module
            bHasHighlight = true;
        }
    }
}

void AF12InstancedRenderer::ClearAllHighlights()
{
    if (HighlightHISM)
    {
        HighlightHISM->ClearInstances();
    }
    bHasHighlight = false;
    HighlightedTileIndex = -1;
}

// === RAYCASTING ===
// FIXED: Now uses instance tracking instead of nearest-module approximation

bool AF12InstancedRenderer::GetHitModuleAndTile(const FHitResult& Hit, FF12GridCoord& OutGridCoord, int32& OutTileIndex) const
{
    if (!GridSystem)
        return false;

    // Check if we hit one of our HISM components
    UHierarchicalInstancedStaticMeshComponent* HitHISM = Cast<UHierarchicalInstancedStaticMeshComponent>(Hit.GetComponent());
    if (!HitHISM)
        return false;

    // Find which component was hit
    int32 HitComponentIdx = HISMComponents.Find(HitHISM);
    if (HitComponentIdx == INDEX_NONE)
        return false;

    // Get the instance index from the hit result
    // Hit.Item contains the instance index for instanced static meshes
    int32 HitInstanceIdx = Hit.Item;
    
    // Look up the source module and tile from our tracking map
    FF12InstanceKey Key(HitComponentIdx, HitInstanceIdx);
    const FF12InstanceSourceData* SourceData = InstanceToSourceMap.Find(Key);
    
    if (SourceData)
    {
        // Found exact match in our tracking map
        OutGridCoord = SourceData->GridCoord;
        OutTileIndex = SourceData->TileIndex;
        return true;
    }
    
    // Fallback: If instance tracking failed (shouldn't happen normally),
    // use the old nearest-module approach
    UE_LOG(LogTemp, Warning, TEXT("GetHitModuleAndTile: Instance not found in tracking map (Comp=%d, Inst=%d), using fallback"), 
        HitComponentIdx, HitInstanceIdx);
    
    // Decode face index from component index as fallback
    OutTileIndex = HitComponentIdx / NumMaterials;
    
    // Get world position of hit and convert to grid coordinate
    FVector HitLocation = Hit.ImpactPoint;
    
    // Find the nearest module to this hit point
    float NearestDistSq = FLT_MAX;
    FF12GridCoord NearestCoord;
    bool bFound = false;
    
    for (const auto& Pair : ModuleData)
    {
        FVector ModulePos = GridSystem->GridToWorld(Pair.Key);
        float DistSq = FVector::DistSquared(HitLocation, ModulePos);
        
        if (DistSq < NearestDistSq)
        {
            NearestDistSq = DistSq;
            NearestCoord = Pair.Key;
            bFound = true;
        }
    }
    
    if (bFound)
    {
        OutGridCoord = NearestCoord;
        return true;
    }
    
    return false;
}

// === REBUILD ===

void AF12InstancedRenderer::RebuildInstances()
{
    // Safety check - ensure components are initialized
    if (HISMComponents.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("RebuildInstances called but HISMComponents not initialized"));
        return;
    }
    
    // Clear all instances and tracking map
    for (auto* HISM : HISMComponents)
    {
        if (HISM && HISM->IsRegistered())
        {
            HISM->ClearInstances();
        }
    }
    
    // Clear the instance tracking map - will be rebuilt
    InstanceToSourceMap.Empty();

    // Rebuild from module data (batch add without immediate updates)
    for (const auto& Pair : ModuleData)
    {
        const FF12GridCoord& Coord = Pair.Key;
        const FF12ModuleInstanceData& Data = Pair.Value;
        
        for (int32 TileIdx = 0; TileIdx < 12; TileIdx++)
        {
            if (Data.TileVisibility[TileIdx])
            {
                int32 MatIdx = Data.TileMaterials[TileIdx];
                int32 ComponentIdx = TileIdx * NumMaterials + FMath::Clamp(MatIdx, 0, NumMaterials - 1);
                
                UHierarchicalInstancedStaticMeshComponent* HISM = GetHISMForFaceAndMaterial(TileIdx, MatIdx);
                
                if (HISM && HISM->IsRegistered())
                {
                    FTransform TileTransform = GetTileWorldTransform(Coord, TileIdx);
                    int32 InstanceIdx = HISM->AddInstance(TileTransform, false);  // Don't update immediately
                    
                    // Track this instance -> source mapping
                    FF12InstanceKey Key(ComponentIdx, InstanceIdx);
                    InstanceToSourceMap.Add(Key, FF12InstanceSourceData(Coord, TileIdx));
                }
            }
        }
    }
    
    // Mark all components dirty after batch add
    for (auto* HISM : HISMComponents)
    {
        if (HISM && HISM->IsRegistered() && HISM->GetInstanceCount() > 0)
        {
            HISM->MarkRenderStateDirty();
        }
    }
    
    UE_LOG(LogTemp, Log, TEXT("RebuildInstances: %d modules, %d instances tracked"), 
        ModuleData.Num(), InstanceToSourceMap.Num());
}

// === STATISTICS ===

int32 AF12InstancedRenderer::GetTotalInstanceCount() const
{
    int32 Total = 0;
    for (const auto* HISM : HISMComponents)
    {
        if (HISM)
        {
            Total += HISM->GetInstanceCount();
        }
    }
    return Total;
}

FString AF12InstancedRenderer::GetPerformanceStats() const
{
    int32 ModuleCount = ModuleData.Num();
    int32 InstanceCount = GetTotalInstanceCount();
    int32 DrawCalls = 0;

    for (const auto* HISM : HISMComponents)
    {
        if (HISM && HISM->GetInstanceCount() > 0)
        {
            DrawCalls++;
        }
    }

    return FString::Printf(
        TEXT("Modules: %d | Tiles: %d | Draw Calls: %d"),
        ModuleCount,
        InstanceCount,
        DrawCalls
    );
}