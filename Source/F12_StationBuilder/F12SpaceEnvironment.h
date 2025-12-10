// F12SpaceEnvironment.h
// Space Environment Manager for F12 Station Builder
// Creates Earth, stars, sun, and proper space lighting

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "F12SpaceEnvironment.generated.h"

class UStaticMeshComponent;
class UDirectionalLightComponent;
class UPointLightComponent;
class USkyLightComponent;
class UPostProcessComponent;
class UMaterialInstanceDynamic;

UCLASS()
class AF12SpaceEnvironment : public AActor
{
    GENERATED_BODY()

public:
    AF12SpaceEnvironment();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // === EARTH CONFIGURATION ===
    
    // Distance from station to Earth center (in Unreal units, 100 = 1 meter)
    // Low Earth Orbit is ~400km, but we'll use a smaller scale for visuals
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Space|Earth")
    float EarthDistance = 500000.0f;  // 5km scaled distance
    
    // Earth sphere radius
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Space|Earth")
    float EarthRadius = 300000.0f;  // 3km scaled radius
    
    // Earth rotation speed (degrees per second)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Space|Earth")
    float EarthRotationSpeed = 1.0f;
    
    // Station orbital speed around Earth (degrees per second)
    // Real ISS: ~90 minute orbit = 4 degrees per minute
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Space|Earth")
    float StationOrbitSpeed = 0.5f;
    
    // Current orbital position (0-360)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Space|Earth")
    float OrbitalPosition = 0.0f;
    
    // === MATERIALS (Set in Blueprint) ===
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Space|Materials")
    UMaterialInterface* EarthMaterial;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Space|Materials")
    UMaterialInterface* CloudMaterial;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Space|Materials")
    UMaterialInterface* StarSphereMaterial;
    
    // === TEXTURES (Set in Blueprint if not using material) ===
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Space|Textures")
    UTexture2D* EarthDayTexture;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Space|Textures")
    UTexture2D* EarthNightTexture;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Space|Textures")
    UTexture2D* EarthCloudsTexture;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Space|Textures")
    UTexture2D* EarthNormalTexture;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Space|Textures")
    UTexture2D* EarthSpecularTexture;
    
    // === SUN CONFIGURATION ===
    
    // Sun direction (where the sun is relative to the scene)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Space|Sun")
    FRotator SunDirection = FRotator(-30.0f, 45.0f, 0.0f);
    
    // Sun light intensity
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Space|Sun")
    float SunIntensity = 10.0f;
    
    // Sun light color
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Space|Sun")
    FLinearColor SunColor = FLinearColor(1.0f, 0.98f, 0.95f, 1.0f);
    
    // === EARTH SHINE (Blue ambient from Earth) ===
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Space|EarthShine")
    float EarthShineIntensity = 0.5f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Space|EarthShine")
    FLinearColor EarthShineColor = FLinearColor(0.4f, 0.6f, 1.0f, 1.0f);
    
    // === AMBIENT SPACE LIGHT ===
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Space|Ambient")
    float AmbientIntensity = 0.1f;
    
    // === STAR SPHERE ===
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Space|Stars")
    float StarSphereRadius = 10000000.0f;  // 100km - far enough to be "infinite"
    
    // === LENS FLARE ===
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Space|LensFlare")
    float LensFlareIntensity = 1.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Space|LensFlare")
    float LensFlareBokehSize = 3.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Space|LensFlare")
    float LensFlareThreshold = 1.0f;
    
    // === COMPONENTS ===
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Space|Components")
    UStaticMeshComponent* EarthMesh;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Space|Components")
    UStaticMeshComponent* CloudMesh;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Space|Components")
    UStaticMeshComponent* StarSphereMesh;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Space|Components")
    UDirectionalLightComponent* SunLight;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Space|Components")
    UPointLightComponent* EarthShineLight;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Space|Components")
    USkyLightComponent* AmbientLight;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Space|Components")
    UPostProcessComponent* PostProcess;
    
    // === FUNCTIONS ===
    
    // Update the sun direction (call this to change time of day)
    UFUNCTION(BlueprintCallable, Category = "Space")
    void SetSunDirection(FRotator NewDirection);
    
    // Update orbital position manually
    UFUNCTION(BlueprintCallable, Category = "Space")
    void SetOrbitalPosition(float NewPosition);
    
    // Enable/disable orbital motion
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Space|Animation")
    bool bAnimateOrbit = true;
    
    // Enable/disable Earth rotation
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Space|Animation")
    bool bAnimateEarthRotation = true;
    
protected:
    // Dynamic material instances for runtime updates
    UPROPERTY()
    UMaterialInstanceDynamic* EarthDynamicMaterial;
    
    UPROPERTY()
    UMaterialInstanceDynamic* CloudDynamicMaterial;
    
    // Update positions based on orbital mechanics
    void UpdateOrbitalPositions(float DeltaTime);
    
    // Update Earth's day/night based on sun direction
    void UpdateDayNightCycle();
    
    // Initialize all components
    void SetupComponents();
    
    // Apply materials to meshes
    void SetupMaterials();
    
    // Configure lighting
    void SetupLighting();
    
    // Configure post-process effects (lens flare, bloom)
    void SetupPostProcess();
};