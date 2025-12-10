// F12Module.cpp
// Implementation of Rhombic Dodecahedron space station module

#include "F12Module.h"
#include "ProceduralMeshComponent.h"

AF12Module::AF12Module()
{
    PrimaryActorTick.bCanEverTick = false;

    // Create root component
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
}

void AF12Module::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    
    // Only generate in editor, not during play (BeginPlay handles runtime)
    if (!GetWorld()->IsGameWorld())
    {
        GenerateModule();
    }
}

FVector AF12Module::GetFaceNormal(int32 FaceIndex) const
{
    if (FaceNormals.IsValidIndex(FaceIndex))
    {
        return FaceNormals[FaceIndex];
    }
    return FVector::UpVector;
}

void AF12Module::GenerateModule()
{
    ClearTileMeshes();
    FaceNormals.Empty();

    float R = ModuleSize * 0.5f;
    float Scale = R / FMath::Sqrt(2.0f);
    float T = TileThickness;

    // 14 vertices: 8 cubic + 6 octahedral
    TArray<FVector> BaseVertices;
    
    // Cubic vertices (±1, ±1, ±1) * Scale
    BaseVertices.Add(FVector(-1, -1, -1) * Scale);  // 0
    BaseVertices.Add(FVector(+1, -1, -1) * Scale);  // 1
    BaseVertices.Add(FVector(-1, +1, -1) * Scale);  // 2
    BaseVertices.Add(FVector(+1, +1, -1) * Scale);  // 3
    BaseVertices.Add(FVector(-1, -1, +1) * Scale);  // 4
    BaseVertices.Add(FVector(+1, -1, +1) * Scale);  // 5
    BaseVertices.Add(FVector(-1, +1, +1) * Scale);  // 6
    BaseVertices.Add(FVector(+1, +1, +1) * Scale);  // 7

    // Octahedral vertices (±2, 0, 0), (0, ±2, 0), (0, 0, ±2) * Scale
    BaseVertices.Add(FVector(+2,  0,  0) * Scale);  // 8  (+X)
    BaseVertices.Add(FVector(-2,  0,  0) * Scale);  // 9  (-X)
    BaseVertices.Add(FVector( 0, +2,  0) * Scale);  // 10 (+Y)
    BaseVertices.Add(FVector( 0, -2,  0) * Scale);  // 11 (-Y)
    BaseVertices.Add(FVector( 0,  0, +2) * Scale);  // 12 (+Z)
    BaseVertices.Add(FVector( 0,  0, -2) * Scale);  // 13 (-Z)

    // Face definitions (vertex indices for each rhombus)
    // Order: cubic, octahedral, cubic, octahedral (alternating around the rhombus)
    int32 FaceIndices[12][4] = {
        {3, 8, 1, 13},   // Face 0: +X, -Z
        {1, 8, 5, 11},   // Face 1: +X, -Y
        {5, 8, 7, 12},   // Face 2: +X, +Z
        {7, 8, 3, 10},   // Face 3: +X, +Y
        {0, 9, 2, 13},   // Face 4: -X, -Z
        {2, 9, 6, 10},   // Face 5: -X, +Y
        {6, 9, 4, 12},   // Face 6: -X, +Z
        {4, 9, 0, 11},   // Face 7: -X, -Y
        {7, 10, 6, 12},  // Face 8: +Y, +Z
        {2, 10, 3, 13},  // Face 9: +Y, -Z
        {4, 11, 5, 12},  // Face 10: -Y, +Z
        {1, 11, 0, 13}   // Face 11: -Y, -Z
    };

    // Create each face
    for (int32 FaceIdx = 0; FaceIdx < 12; FaceIdx++)
    {
        // Get the 4 vertices of this face
        FVector V0 = BaseVertices[FaceIndices[FaceIdx][0]];
        FVector V1 = BaseVertices[FaceIndices[FaceIdx][1]];
        FVector V2 = BaseVertices[FaceIndices[FaceIdx][2]];
        FVector V3 = BaseVertices[FaceIndices[FaceIdx][3]];

        // Calculate face normal (cross product of diagonals)
        FVector Normal = FVector::CrossProduct(V2 - V0, V3 - V1).GetSafeNormal();

        // Create outer and inner vertices (for thickness)
        FVector OuterOffset = Normal * (T * 0.5f);
        FVector InnerOffset = -Normal * (T * 0.5f);

        TArray<FVector> TileVerts;
        // Outer face
        TileVerts.Add(V0 + OuterOffset);
        TileVerts.Add(V1 + OuterOffset);
        TileVerts.Add(V2 + OuterOffset);
        TileVerts.Add(V3 + OuterOffset);
        // Inner face
        TileVerts.Add(V0 + InnerOffset);
        TileVerts.Add(V1 + InnerOffset);
        TileVerts.Add(V2 + InnerOffset);
        TileVerts.Add(V3 + InnerOffset);

        // Triangles (both sides for visibility)
        TArray<int32> Tris;
        // Outer face (0,1,2,3)
        Tris.Append({0, 1, 2, 0, 2, 3});
        // Inner face (4,5,6,7) - reversed winding
        Tris.Append({4, 6, 5, 4, 7, 6});
        // Side edges
        Tris.Append({0, 4, 1, 1, 4, 5});  // Edge 0-1
        Tris.Append({1, 5, 2, 2, 5, 6});  // Edge 1-2
        Tris.Append({2, 6, 3, 3, 6, 7});  // Edge 2-3
        Tris.Append({3, 7, 0, 0, 7, 4});  // Edge 3-0

        CreateTileMesh(FaceIdx, TileVerts, Tris, Normal);
    }

    // Initialize material indices
    TileMaterialIndices.SetNum(12);
    for (int32 i = 0; i < 12; i++)
    {
        TileMaterialIndices[i] = 0;
    }
}

void AF12Module::ClearTileMeshes()
{
    for (UProceduralMeshComponent* Mesh : TileMeshes)
    {
        if (Mesh)
        {
            Mesh->DestroyComponent();
        }
    }
    TileMeshes.Empty();
}

void AF12Module::CreateTileMesh(int32 TileIndex, const TArray<FVector>& Vertices, const TArray<int32>& Triangles, const FVector& Normal)
{
    FString MeshName = FString::Printf(TEXT("Tile_%d"), TileIndex);
    UProceduralMeshComponent* TileMesh = NewObject<UProceduralMeshComponent>(this, *MeshName);
    TileMesh->SetupAttachment(RootComponent);
    TileMesh->RegisterComponent();

    // Generate UVs
    TArray<FVector2D> UVs;
    for (int32 i = 0; i < Vertices.Num(); i++)
    {
        UVs.Add(FVector2D(Vertices[i].X / ModuleSize + 0.5f, Vertices[i].Y / ModuleSize + 0.5f));
    }

    // Let UE auto-calculate normals - pass empty arrays
    TArray<FVector> Normals;
    TArray<FProcMeshTangent> Tangents;

    // Create the mesh section with auto-calculated normals
    TileMesh->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, TArray<FColor>(), Tangents, true);

    // Apply material
    if (TileMaterial)
    {
        TileMesh->SetMaterial(0, TileMaterial);
    }

    TileMeshes.Add(TileMesh);
    FaceNormals.Add(Normal);
}

// ============================================================================
// TILE OPERATIONS
// ============================================================================

void AF12Module::SetTileMaterial(int32 TileIndex, UMaterialInterface* Material)
{
    if (TileMeshes.IsValidIndex(TileIndex) && TileMeshes[TileIndex] && Material)
    {
        TileMeshes[TileIndex]->SetMaterial(0, Material);
    }
}

void AF12Module::SetTileMaterialIndex(int32 TileIndex, int32 MaterialIndex)
{
    if (TileMeshes.IsValidIndex(TileIndex) && TileMeshes[TileIndex] && 
        TileMaterials.IsValidIndex(MaterialIndex))
    {
        TileMeshes[TileIndex]->SetMaterial(0, TileMaterials[MaterialIndex]);
        
        if (TileMaterialIndices.IsValidIndex(TileIndex))
        {
            TileMaterialIndices[TileIndex] = MaterialIndex;
        }
    }
}

void AF12Module::SetAllTilesMaterialIndex(int32 MaterialIndex)
{
    for (int32 i = 0; i < TileMeshes.Num(); i++)
    {
        SetTileMaterialIndex(i, MaterialIndex);
    }
}

void AF12Module::SetTileVisible(int32 TileIndex, bool bVisible)
{
    if (TileMeshes.IsValidIndex(TileIndex) && TileMeshes[TileIndex])
    {
        TileMeshes[TileIndex]->SetVisibility(bVisible);
        TileMeshes[TileIndex]->SetCollisionEnabled(bVisible ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    }
}

bool AF12Module::IsTileVisible(int32 TileIndex) const
{
    if (TileMeshes.IsValidIndex(TileIndex) && TileMeshes[TileIndex])
    {
        return TileMeshes[TileIndex]->IsVisible();
    }
    return false;
}

int32 AF12Module::GetTileIndexFromComponent(UPrimitiveComponent* Component) const
{
    if (!Component)
        return -1;

    for (int32 i = 0; i < TileMeshes.Num(); i++)
    {
        if (TileMeshes[i] == Component)
        {
            return i;
        }
    }
    return -1;
}