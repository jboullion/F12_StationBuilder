// F12StationGenerator.h
// Advanced Procedural Station Generator for F12 Station Builder
// Generates realistic station structures using spine + branch architecture

#pragma once

#include "CoreMinimal.h"
#include "F12GridSystem.h"
#include "F12StationGenerator.generated.h"

class AF12BuilderController;
class AF12InstancedRenderer;

// Symmetry modes for station generation
UENUM(BlueprintType)
enum class EF12SymmetryMode : uint8
{
    None        UMETA(DisplayName = "None"),
    Bilateral   UMETA(DisplayName = "Bilateral (Mirror)"),
    Quad        UMETA(DisplayName = "Quadrilateral (4-way)"),
    Radial      UMETA(DisplayName = "Radial (6-way)")
};

// Primary axis for main spine
UENUM(BlueprintType)
enum class EF12PrimaryAxis : uint8
{
    X   UMETA(DisplayName = "X Axis"),
    Y   UMETA(DisplayName = "Y Axis"),
    Z   UMETA(DisplayName = "Z Axis")
};

// Zone types for automatic material assignment
UENUM(BlueprintType)
enum class EF12ZoneType : uint8
{
    Structural  UMETA(DisplayName = "Structural"),   // Spine modules
    Hub         UMETA(DisplayName = "Hub/Utility"),  // High connectivity nodes
    Habitat     UMETA(DisplayName = "Habitat"),      // Interior modules
    Power       UMETA(DisplayName = "Power"),        // Exterior facing
    Docking     UMETA(DisplayName = "Docking")       // Endpoint modules
};

// Parameters for station generation
USTRUCT(BlueprintType)
struct FF12StationParams
{
    GENERATED_BODY()

    // Random seed for reproducible generation
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
    int32 Seed = 12345;

    // Primary axis for main spine direction
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
    EF12PrimaryAxis PrimaryAxis = EF12PrimaryAxis::X;

    // Length of the main spine (in modules)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation", meta = (ClampMin = "3", ClampMax = "70"))
    int32 MainSpineLength = 15;

    // Number of branches off the main spine
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation", meta = (ClampMin = "0", ClampMax = "12"))
    int32 BranchCount = 4;

    // Minimum branch length
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation", meta = (ClampMin = "2", ClampMax = "30"))
    int32 BranchLengthMin = 3;

    // Maximum branch length
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation", meta = (ClampMin = "2", ClampMax = "30"))
    int32 BranchLengthMax = 8;

    // How often hubs appear along spines (every N modules)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation", meta = (ClampMin = "1", ClampMax = "10"))
    int32 HubFrequency = 5;

    // Size of hub clusters (radius in modules)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation", meta = (ClampMin = "1", ClampMax = "5"))
    int32 HubSize = 2;

    // Symmetry mode for branch placement
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
    EF12SymmetryMode SymmetryMode = EF12SymmetryMode::Bilateral;

    // Whether to clear existing modules before generating
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
    bool bClearExisting = true;

    // Whether to preserve the core module at origin
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
    bool bPreserveCore = true;

    // Whether to automatically assign materials based on zones
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
    bool bAutoAssignMaterials = true;

    // Offset from origin (in grid coordinates)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
    FIntVector Offset = FIntVector(0, 0, 0);

    FF12StationParams() {}
};

// Result of station generation
USTRUCT(BlueprintType)
struct FF12StationResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Generation")
    bool bSuccess = false;

    UPROPERTY(BlueprintReadOnly, Category = "Generation")
    int32 TotalModules = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Generation")
    int32 SpineModules = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Generation")
    int32 BranchModules = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Generation")
    int32 HubModules = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Generation")
    FString Message;

    // All created coordinates (for potential undo)
    UPROPERTY()
    TArray<FF12GridCoord> CreatedCoords;

    // Zone assignments for each coordinate
    UPROPERTY()
    TMap<FF12GridCoord, EF12ZoneType> ZoneAssignments;
};

// Internal structure for tracking module metadata during generation
struct FF12ModuleMetadata
{
    FF12GridCoord Coord;
    EF12ZoneType Zone = EF12ZoneType::Structural;
    int32 Connectivity = 0;  // Number of neighbors
    bool bIsSpine = false;
    bool bIsHub = false;
    bool bIsBranch = false;
    bool bIsEndpoint = false;
};

/**
 * Advanced Station Generator using spine + branch architecture
 * Creates realistic, varied station structures
 */
UCLASS(BlueprintType)
class UF12StationGenerator : public UObject
{
    GENERATED_BODY()

public:
    UF12StationGenerator();

    // Initialize with references to required systems
    UFUNCTION(BlueprintCallable, Category = "Generation")
    void Initialize(AF12GridSystem* InGridSystem, AF12BuilderController* InController);

    // Generate a complete station with given parameters
    UFUNCTION(BlueprintCallable, Category = "Generation")
    FF12StationResult GenerateStation(const FF12StationParams& Params);

    // Preview generation (returns coordinates without placing modules)
    UFUNCTION(BlueprintCallable, Category = "Generation")
    TArray<FF12GridCoord> PreviewStation(const FF12StationParams& Params);

    // Estimate module count for given parameters
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Generation")
    int32 EstimateModuleCount(const FF12StationParams& Params);

    // Clear all modules (optionally preserve core)
    UFUNCTION(BlueprintCallable, Category = "Generation")
    int32 ClearAll(bool bPreserveCore = true);

    // Get zone type for a specific coordinate (after generation)
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Generation")
    EF12ZoneType GetZoneAtCoord(FF12GridCoord Coord) const;

    // Get material index for a zone type
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Generation")
    int32 GetMaterialForZone(EF12ZoneType Zone) const;

    // Zone to material mapping (configurable)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation|Materials")
    TMap<EF12ZoneType, int32> ZoneMaterialMap;

protected:
    UPROPERTY()
    AF12GridSystem* GridSystem;

    UPROPERTY()
    AF12BuilderController* Controller;

    // Random stream for reproducible generation
    FRandomStream RandomStream;

    // Current generation working data
    TMap<FF12GridCoord, FF12ModuleMetadata> WorkingModules;
    TArray<FF12GridCoord> SpineCoords;
    TArray<FF12GridCoord> HubCoords;

    // === GENERATION CORE ===

    // Generate the main spine along the primary axis
    void GenerateMainSpine(const FF12StationParams& Params);

    // Generate hubs at intervals along the spine
    void GenerateHubs(const FF12StationParams& Params);

    // Generate branches from hubs
    void GenerateBranches(const FF12StationParams& Params);

    // Apply symmetry to branch placement
    void ApplySymmetry(const FF12StationParams& Params);

    // Analyze and assign zones based on topology
    void AssignZones();

    // Apply materials based on zone assignments
    void ApplyMaterials();

    // === SPINE HELPERS ===

    // Get the face pairs for zigzag movement along an axis
    void GetAxisFacePairs(EF12PrimaryAxis Axis, int32& OutFaceA, int32& OutFaceB) const;

    // Get perpendicular face directions for branching
    TArray<int32> GetPerpendicularFaces(EF12PrimaryAxis Axis) const;

    // Step along a spine direction (handles zigzag)
    FF12GridCoord StepAlongSpine(FF12GridCoord Current, EF12PrimaryAxis Axis, int32 StepIndex) const;

    // Step in a specific face direction
    FF12GridCoord StepInDirection(FF12GridCoord Current, int32 FaceIndex) const;

    // === HUB HELPERS ===

    // Generate a cluster of modules around a center point
    void GenerateHubCluster(FF12GridCoord Center, int32 Radius);

    // Get valid neighbors for hub expansion
    TArray<FF12GridCoord> GetValidHubNeighbors(FF12GridCoord Center) const;

    // === UTILITY ===

    // Check if coordinate is valid and unoccupied in working set
    bool IsValidPosition(FF12GridCoord Coord) const;

    // Add a module to the working set
    void AddWorkingModule(FF12GridCoord Coord, bool bIsSpine, bool bIsHub, bool bIsBranch);

    // Check BCC lattice validity
    bool IsValidBCCPosition(FF12GridCoord Coord) const;

    // Calculate connectivity (neighbor count) for all modules
    void CalculateConnectivity();

    // Mirror a coordinate based on symmetry mode
    TArray<FF12GridCoord> GetSymmetricCoords(FF12GridCoord Coord, EF12SymmetryMode Mode, EF12PrimaryAxis Axis) const;

    // Get neighbor offsets from grid system
    TArray<FIntVector> GetNeighborOffsets() const;

    // Initialize default zone material mapping
    void InitializeDefaultMaterials();
};
