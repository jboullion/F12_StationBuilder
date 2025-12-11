// F12ProceduralGenerator.h
// Procedural Generation System for F12 Station Builder
// Allows bulk creation of module structures

#pragma once

#include "CoreMinimal.h"
#include "F12GridSystem.h"
#include "F12ProceduralGenerator.generated.h"

class AF12BuilderController;

// Shape types for generation
UENUM(BlueprintType)
enum class EF12GeneratorShape : uint8
{
    HollowBox      UMETA(DisplayName = "Hollow Box"),
    SolidBox       UMETA(DisplayName = "Solid Box"),
    HollowSphere   UMETA(DisplayName = "Hollow Sphere"),
    SolidSphere    UMETA(DisplayName = "Solid Sphere"),
    Cylinder       UMETA(DisplayName = "Cylinder"),
    Cross          UMETA(DisplayName = "Cross/Plus"),
    Ring           UMETA(DisplayName = "Ring/Torus")
};

// Generation parameters
USTRUCT(BlueprintType)
struct FF12GenerationParams
{
    GENERATED_BODY()

    // Shape to generate
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
    EF12GeneratorShape Shape = EF12GeneratorShape::HollowBox;

    // Size in each dimension (in module count)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation", meta = (ClampMin = "1", ClampMax = "50"))
    int32 SizeX = 10;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation", meta = (ClampMin = "1", ClampMax = "50"))
    int32 SizeY = 10;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation", meta = (ClampMin = "1", ClampMax = "50"))
    int32 SizeZ = 10;

    // Wall thickness for hollow shapes (in modules)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation", meta = (ClampMin = "1", ClampMax = "10"))
    int32 WallThickness = 1;

    // Offset from origin (in grid coordinates)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
    FIntVector Offset = FIntVector(0, 0, 0);

    // Whether to center the shape on the offset point
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
    bool bCenterOnOffset = true;

    // Whether to clear existing modules in the area first
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
    bool bClearExisting = false;

    // Whether to preserve the core module at origin
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
    bool bPreserveCore = true;

    // Material index to use for generated modules (-1 for default)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
    int32 MaterialIndex = -1;

    FF12GenerationParams() {}
};

// Result of a generation operation
USTRUCT(BlueprintType)
struct FF12GenerationResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Generation")
    bool bSuccess = false;

    UPROPERTY(BlueprintReadOnly, Category = "Generation")
    int32 ModulesCreated = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Generation")
    int32 ModulesSkipped = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Generation")
    FString Message;

    // Coordinates of all created modules (for undo)
    UPROPERTY()
    TArray<FF12GridCoord> CreatedCoords;
};

/**
 * Procedural Generator for creating bulk module structures
 */
UCLASS(BlueprintType)
class UF12ProceduralGenerator : public UObject
{
    GENERATED_BODY()

public:
    UF12ProceduralGenerator();

    // Initialize with references
    void Initialize(AF12GridSystem* InGridSystem, AF12BuilderController* InController);

    // Generate a structure with given parameters
    UFUNCTION(BlueprintCallable, Category = "Generation")
    FF12GenerationResult Generate(const FF12GenerationParams& Params);

    // Preview generation (returns coordinates without placing)
    UFUNCTION(BlueprintCallable, Category = "Generation")
    TArray<FF12GridCoord> PreviewGeneration(const FF12GenerationParams& Params);

    // Clear all modules in a region
    UFUNCTION(BlueprintCallable, Category = "Generation")
    int32 ClearRegion(FIntVector MinCoord, FIntVector MaxCoord, bool bPreserveCore = true);

    // Clear all modules except core
    UFUNCTION(BlueprintCallable, Category = "Generation")
    int32 ClearAll(bool bPreserveCore = true);

    // Get estimated module count for parameters
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Generation")
    int32 EstimateModuleCount(const FF12GenerationParams& Params);

protected:
    UPROPERTY()
    AF12GridSystem* GridSystem;

    UPROPERTY()
    AF12BuilderController* Controller;

    // Shape generation functions - return list of coordinates to fill
    TArray<FF12GridCoord> GenerateHollowBoxCoords(const FF12GenerationParams& Params);
    TArray<FF12GridCoord> GenerateSolidBoxCoords(const FF12GenerationParams& Params);
    TArray<FF12GridCoord> GenerateHollowSphereCoords(const FF12GenerationParams& Params);
    TArray<FF12GridCoord> GenerateSolidSphereCoords(const FF12GenerationParams& Params);
    TArray<FF12GridCoord> GenerateCylinderCoords(const FF12GenerationParams& Params);
    TArray<FF12GridCoord> GenerateCrossCoords(const FF12GenerationParams& Params);
    TArray<FF12GridCoord> GenerateRingCoords(const FF12GenerationParams& Params);

    // Helper to check if a point is on the shell of a box
    bool IsOnBoxShell(int32 X, int32 Y, int32 Z, int32 SizeX, int32 SizeY, int32 SizeZ, int32 Thickness);

    // Helper to check if a point is within a sphere
    bool IsInSphere(int32 X, int32 Y, int32 Z, float CenterX, float CenterY, float CenterZ, float Radius);

    // Apply offset and centering to coordinates
    FF12GridCoord ApplyOffset(int32 X, int32 Y, int32 Z, const FF12GenerationParams& Params);

    // Check if a grid coordinate is valid for BCC lattice tessellation
    bool IsValidBCCPosition(const FF12GridCoord& Coord);
};
