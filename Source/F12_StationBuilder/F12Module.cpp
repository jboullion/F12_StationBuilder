// F12Module.cpp
// Implementation of the Rhombic Dodecahedron Module
// VERSION 4: Direct world-space vertex computation (no rotation matrices)

#include "F12Module.h"
#include "ProceduralMeshComponent/Public/ProceduralMeshComponent.h"

AF12Module::AF12Module()
{
    PrimaryActorTick.bCanEverTick = false;

    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent = Root;

    // Initialize tile arrays
    TileVisibility.SetNum(12);
    TileMaterialIndices.SetNum(12);
    for (int32 i = 0; i < 12; i++)
    {
        TileVisibility[i] = true;
        TileMaterialIndices[i] = 0;
    }
}

void AF12Module::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    GenerateModule();
}

// ============================================================================
// RHOMBIC DODECAHEDRON GEOMETRY
// ============================================================================
// 
// Vertices of a rhombic dodecahedron (14 total):
//   8 "cubic" vertices at (±1, ±1, ±1) * CubicScale
//   6 "octahedral" vertices at (±1,0,0), (0,±1,0), (0,0,±1) * OctaScale
//
// For inscribed sphere radius R (face-center distance):
//   CubicScale = R * sqrt(2)  (cubic vertices at distance R*sqrt(3) from origin)
//   OctaScale = R * 2         (octahedral vertices at distance R*2 from origin)
//
// Each face is a rhombus with:
//   - 2 cubic vertices (short diagonal)
//   - 2 octahedral vertices (long diagonal)
// ============================================================================

struct FRhombicDodecahedron
{
    // All 14 vertices
    FVector Vertices[14];
    
    // The 12 faces, each defined by 4 vertex indices [cubic, octa, cubic, octa]
    int32 Faces[12][4];
    
    void Build(float ModuleSize)
    {
        // CORRECTED SCALING
        // For a rhombic dodecahedron with standard vertices at:
        //   Cubic: (±1, ±1, ±1)
        //   Octahedral: (±2, 0, 0), (0, ±2, 0), (0, 0, ±2)
        // The inscribed sphere radius is sqrt(2).
        // To get inscribed sphere radius R = ModuleSize/2, we scale by R/sqrt(2)
        
        float R = ModuleSize * 0.5f;  // Inscribed sphere radius = 200 for ModuleSize 400
        float Scale = R / 1.41421356f;  // Scale factor = R / sqrt(2) ≈ 141.42
        
        // 8 cubic vertices at (±1, ±1, ±1) * Scale
        int32 idx = 0;
        for (int32 sz = -1; sz <= 1; sz += 2)
        {
            for (int32 sy = -1; sy <= 1; sy += 2)
            {
                for (int32 sx = -1; sx <= 1; sx += 2)
                {
                    Vertices[idx++] = FVector(sx, sy, sz) * Scale;
                }
            }
        }
        // Vertex indices after this loop:
        // 0: (-1,-1,-1), 1: (+1,-1,-1), 2: (-1,+1,-1), 3: (+1,+1,-1)
        // 4: (-1,-1,+1), 5: (+1,-1,+1), 6: (-1,+1,+1), 7: (+1,+1,+1)
        
        // 6 octahedral vertices at (±2, 0, 0), (0, ±2, 0), (0, 0, ±2) * Scale
        Vertices[8]  = FVector( 2.0f * Scale,  0,  0);  // +X
        Vertices[9]  = FVector(-2.0f * Scale,  0,  0);  // -X
        Vertices[10] = FVector( 0,  2.0f * Scale,  0);  // +Y
        Vertices[11] = FVector( 0, -2.0f * Scale,  0);  // -Y
        Vertices[12] = FVector( 0,  0,  2.0f * Scale);  // +Z
        Vertices[13] = FVector( 0,  0, -2.0f * Scale);  // -Z
        
        // 12 faces - each connects 2 cubic and 2 octahedral vertices
        // Order: [cubic1, octa1, cubic2, octa2] going around the rhombus
        
        // Faces around +X octahedral (index 8)
        Faces[0][0] = 3; Faces[0][1] = 8; Faces[0][2] = 1; Faces[0][3] = 13;  // +X, -Z quadrant
        Faces[1][0] = 1; Faces[1][1] = 8; Faces[1][2] = 5; Faces[1][3] = 11;  // +X, -Y quadrant
        Faces[2][0] = 5; Faces[2][1] = 8; Faces[2][2] = 7; Faces[2][3] = 12;  // +X, +Z quadrant
        Faces[3][0] = 7; Faces[3][1] = 8; Faces[3][2] = 3; Faces[3][3] = 10;  // +X, +Y quadrant
        
        // Faces around -X octahedral (index 9)
        Faces[4][0] = 0; Faces[4][1] = 9; Faces[4][2] = 2; Faces[4][3] = 13;  // -X, -Z quadrant
        Faces[5][0] = 2; Faces[5][1] = 9; Faces[5][2] = 6; Faces[5][3] = 10;  // -X, +Y quadrant
        Faces[6][0] = 6; Faces[6][1] = 9; Faces[6][2] = 4; Faces[6][3] = 12;  // -X, +Z quadrant
        Faces[7][0] = 4; Faces[7][1] = 9; Faces[7][2] = 0; Faces[7][3] = 11;  // -X, -Y quadrant
        
        // Remaining 4 faces (connecting +Y/-Y to +Z/-Z)
        Faces[8][0]  = 7; Faces[8][1]  = 10; Faces[8][2]  = 6; Faces[8][3]  = 12;  // +Y, +Z
        Faces[9][0]  = 2; Faces[9][1]  = 10; Faces[9][2]  = 3; Faces[9][3]  = 13;  // +Y, -Z
        Faces[10][0] = 4; Faces[10][1] = 11; Faces[10][2] = 5; Faces[10][3] = 12;  // -Y, +Z
        Faces[11][0] = 1; Faces[11][1] = 11; Faces[11][2] = 0; Faces[11][3] = 13;  // -Y, -Z
    }
    
    void GetFaceVertices(int32 FaceIndex, FVector& V0, FVector& V1, FVector& V2, FVector& V3) const
    {
        V0 = Vertices[Faces[FaceIndex][0]];
        V1 = Vertices[Faces[FaceIndex][1]];
        V2 = Vertices[Faces[FaceIndex][2]];
        V3 = Vertices[Faces[FaceIndex][3]];
    }
    
    FVector GetFaceCenter(int32 FaceIndex) const
    {
        return (Vertices[Faces[FaceIndex][0]] + 
                Vertices[Faces[FaceIndex][1]] + 
                Vertices[Faces[FaceIndex][2]] + 
                Vertices[Faces[FaceIndex][3]]) * 0.25f;
    }
    
    FVector GetFaceNormal(int32 FaceIndex) const
    {
        FVector V0 = Vertices[Faces[FaceIndex][0]];
        FVector V1 = Vertices[Faces[FaceIndex][1]];
        FVector V2 = Vertices[Faces[FaceIndex][2]];
        FVector V3 = Vertices[Faces[FaceIndex][3]];
        
        // Cross product of diagonals
        FVector Diag1 = V2 - V0;  // Short diagonal (cubic to cubic)
        FVector Diag2 = V3 - V1;  // Long diagonal (octa to octa)
        FVector Normal = FVector::CrossProduct(Diag1, Diag2).GetSafeNormal();
        
        // Ensure normal points outward
        FVector Center = GetFaceCenter(FaceIndex);
        if (FVector::DotProduct(Normal, Center) < 0)
        {
            Normal = -Normal;
        }
        
        return Normal;
    }
};

void AF12Module::GenerateModule()
{
    // Clear existing meshes
    for (UProceduralMeshComponent* Mesh : TileMeshes)
    {
        if (Mesh)
        {
            Mesh->DestroyComponent();
        }
    }
    TileMeshes.Empty();

    // Build the rhombic dodecahedron geometry
    FRhombicDodecahedron RD;
    RD.Build(ModuleSize);

    float Thickness = 50.0f;  // 0.5m panel thickness

    // Create 12 tiles
    for (int32 i = 0; i < 12; i++)
    {
        FName CompName = *FString::Printf(TEXT("Tile_%d"), i);
        UProceduralMeshComponent* TileMesh = NewObject<UProceduralMeshComponent>(this, CompName);
        TileMesh->SetupAttachment(RootComponent);
        TileMesh->RegisterComponent();

        // Get face vertices in world space
        FVector V0, V1, V2, V3;
        RD.GetFaceVertices(i, V0, V1, V2, V3);
        FVector Normal = RD.GetFaceNormal(i);

        // Generate the tile mesh directly with world-space vertices
        GenerateTileAtPosition(TileMesh, V0, V1, V2, V3, Normal, Thickness);

        // Apply material
        if (TileMaterial)
        {
            TileMesh->SetMaterial(0, TileMaterial);
        }

        // Set visibility
        TileMesh->SetVisibility(TileVisibility[i]);

        // Enable collision
        TileMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        TileMesh->SetCollisionResponseToAllChannels(ECR_Block);

        TileMeshes.Add(TileMesh);
    }
}

void AF12Module::GenerateTileAtPosition(UProceduralMeshComponent* MeshComp,
    const FVector& V0, const FVector& V1, const FVector& V2, const FVector& V3,
    const FVector& Normal, float Thickness)
{
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FColor> Colors;
    TArray<FProcMeshTangent> Tangents;

    // Offset for thickness
    FVector OuterOffset = Normal * (Thickness * 0.5f);
    FVector InnerOffset = -Normal * (Thickness * 0.5f);

    // Front face (exterior) - offset outward
    Vertices.Add(V0 + OuterOffset);  // 0
    Vertices.Add(V1 + OuterOffset);  // 1
    Vertices.Add(V2 + OuterOffset);  // 2
    Vertices.Add(V3 + OuterOffset);  // 3

    // Back face (interior) - offset inward
    Vertices.Add(V0 + InnerOffset);  // 4
    Vertices.Add(V1 + InnerOffset);  // 5
    Vertices.Add(V2 + InnerOffset);  // 6
    Vertices.Add(V3 + InnerOffset);  // 7

    // Front face triangles - add both windings so it renders from both sides
    // Winding A (CCW from +Normal direction)
    Triangles.Add(0); Triangles.Add(1); Triangles.Add(2);
    Triangles.Add(0); Triangles.Add(2); Triangles.Add(3);
    // Winding B (CCW from -Normal direction) 
    Triangles.Add(0); Triangles.Add(2); Triangles.Add(1);
    Triangles.Add(0); Triangles.Add(3); Triangles.Add(2);

    // Back face triangles - add both windings
    // Winding A
    Triangles.Add(6); Triangles.Add(5); Triangles.Add(4);
    Triangles.Add(7); Triangles.Add(6); Triangles.Add(4);
    // Winding B
    Triangles.Add(4); Triangles.Add(5); Triangles.Add(6);
    Triangles.Add(4); Triangles.Add(6); Triangles.Add(7);

    // Side faces (4 edges, each a quad = 2 triangles each direction)
    // Edge 0-1 - both windings
    Triangles.Add(0); Triangles.Add(4); Triangles.Add(5);
    Triangles.Add(0); Triangles.Add(5); Triangles.Add(1);
    Triangles.Add(5); Triangles.Add(4); Triangles.Add(0);
    Triangles.Add(1); Triangles.Add(5); Triangles.Add(0);
    
    // Edge 1-2 - both windings
    Triangles.Add(1); Triangles.Add(5); Triangles.Add(6);
    Triangles.Add(1); Triangles.Add(6); Triangles.Add(2);
    Triangles.Add(6); Triangles.Add(5); Triangles.Add(1);
    Triangles.Add(2); Triangles.Add(6); Triangles.Add(1);
    
    // Edge 2-3 - both windings
    Triangles.Add(2); Triangles.Add(6); Triangles.Add(7);
    Triangles.Add(2); Triangles.Add(7); Triangles.Add(3);
    Triangles.Add(7); Triangles.Add(6); Triangles.Add(2);
    Triangles.Add(3); Triangles.Add(7); Triangles.Add(2);
    
    // Edge 3-0 - both windings
    Triangles.Add(3); Triangles.Add(7); Triangles.Add(4);
    Triangles.Add(3); Triangles.Add(4); Triangles.Add(0);
    Triangles.Add(4); Triangles.Add(7); Triangles.Add(3);
    Triangles.Add(0); Triangles.Add(4); Triangles.Add(3);

    // Normals - simplified (face normal for front/back, will be auto-calculated for sides)
    for (int32 i = 0; i < 4; i++) Normals.Add(Normal);      // Front face vertices
    for (int32 i = 0; i < 4; i++) Normals.Add(-Normal);     // Back face vertices

    // UVs
    UVs.Add(FVector2D(0.0f, 0.5f));  // V0
    UVs.Add(FVector2D(0.5f, 0.0f));  // V1
    UVs.Add(FVector2D(1.0f, 0.5f));  // V2
    UVs.Add(FVector2D(0.5f, 1.0f));  // V3
    UVs.Add(FVector2D(0.0f, 0.5f));  // V0 back
    UVs.Add(FVector2D(0.5f, 0.0f));  // V1 back
    UVs.Add(FVector2D(1.0f, 0.5f));  // V2 back
    UVs.Add(FVector2D(0.5f, 1.0f));  // V3 back

    // Vertex colors
    for (int32 i = 0; i < 8; i++) Colors.Add(FColor::White);

    MeshComp->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, Colors, Tangents, true);
}

// These functions are now simpler - just used for grid system compatibility
FVector AF12Module::GetFaceNormal(int32 FaceIndex)
{
    FRhombicDodecahedron RD;
    RD.Build(ModuleSize);
    return RD.GetFaceNormal(FaceIndex);
}

float AF12Module::GetFaceRollAdjust(int32 FaceIndex)
{
    return 0.0f;  // No longer used
}

FTransform AF12Module::GetFaceTransform(int32 FaceIndex)
{
    // This is no longer used for tile placement, but keep it for compatibility
    FRhombicDodecahedron RD;
    RD.Build(ModuleSize);
    FVector Center = RD.GetFaceCenter(FaceIndex);
    return FTransform(FRotator::ZeroRotator, Center, FVector::OneVector);
}

void AF12Module::SetTileVisible(int32 TileIndex, bool bVisible)
{
    if (TileIndex >= 0 && TileIndex < 12)
    {
        TileVisibility[TileIndex] = bVisible;
        
        if (TileMeshes.IsValidIndex(TileIndex) && TileMeshes[TileIndex])
        {
            TileMeshes[TileIndex]->SetVisibility(bVisible);
            
            // Also toggle collision
            if (bVisible)
            {
                TileMeshes[TileIndex]->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            }
            else
            {
                TileMeshes[TileIndex]->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            }
        }
    }
}

bool AF12Module::IsTileVisible(int32 TileIndex) const
{
    if (TileIndex >= 0 && TileIndex < 12 && TileVisibility.IsValidIndex(TileIndex))
    {
        return TileVisibility[TileIndex];
    }
    return false;
}

void AF12Module::SetTileMaterial(int32 TileIndex, UMaterialInterface* Material)
{
    if (TileIndex >= 0 && TileIndex < 12 && TileMeshes.IsValidIndex(TileIndex) && TileMeshes[TileIndex])
    {
        TileMeshes[TileIndex]->SetMaterial(0, Material);
    }
}

void AF12Module::SetTileMaterialIndex(int32 TileIndex, int32 MaterialIndex)
{
    if (TileIndex >= 0 && TileIndex < 12 && TileMaterials.Num() > 0)
    {
        int32 ClampedIndex = MaterialIndex % TileMaterials.Num();
        TileMaterialIndices[TileIndex] = ClampedIndex;
        
        if (TileMaterials.IsValidIndex(ClampedIndex))
        {
            SetTileMaterial(TileIndex, TileMaterials[ClampedIndex]);
        }
    }
}

void AF12Module::CycleTileMaterial(int32 TileIndex)
{
    if (TileIndex >= 0 && TileIndex < 12 && TileMaterials.Num() > 0)
    {
        int32 CurrentIndex = TileMaterialIndices.IsValidIndex(TileIndex) ? TileMaterialIndices[TileIndex] : 0;
        int32 NextIndex = (CurrentIndex + 1) % TileMaterials.Num();
        SetTileMaterialIndex(TileIndex, NextIndex);
    }
}

void AF12Module::SetAllTilesMaterial(UMaterialInterface* Material)
{
    for (int32 i = 0; i < 12; i++)
    {
        SetTileMaterial(i, Material);
    }
}

void AF12Module::CycleAllTilesMaterial()
{
    if (TileMaterials.Num() > 0)
    {
        CurrentMaterialIndex = (CurrentMaterialIndex + 1) % TileMaterials.Num();
        
        for (int32 i = 0; i < 12; i++)
        {
            SetTileMaterialIndex(i, CurrentMaterialIndex);
        }
    }
}

int32 AF12Module::GetTileIndexFromComponent(UPrimitiveComponent* Component) const
{
    for (int32 i = 0; i < TileMeshes.Num(); i++)
    {
        if (TileMeshes[i] == Component)
        {
            return i;
        }
    }
    return -1;
}

// New helper function for tile generation (not in header yet, but add if needed)
void AF12Module::GenerateTileGeometry(UProceduralMeshComponent* MeshComp, float Size)
{
    // This function is no longer the primary method - we use GenerateTileAtPosition instead
    // Keeping it for API compatibility but it won't produce correct results on its own
}