// F12SpaceEnvironment.cpp
// Implementation of the Space Environment Manager

#include "F12SpaceEnvironment.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AF12SpaceEnvironment::AF12SpaceEnvironment()
{
    PrimaryActorTick.bCanEverTick = true;

    // Create root component
    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent = Root;

    // === CREATE EARTH MESH ===
    EarthMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EarthMesh"));
    EarthMesh->SetupAttachment(RootComponent);
    EarthMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    EarthMesh->SetCastShadow(false);
    
    // Use engine's built-in sphere mesh
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(
        TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (SphereMesh.Succeeded())
    {
        EarthMesh->SetStaticMesh(SphereMesh.Object);
    }

    // === CREATE CLOUD LAYER MESH ===
    CloudMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CloudMesh"));
    CloudMesh->SetupAttachment(EarthMesh);
    CloudMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    CloudMesh->SetCastShadow(false);
    if (SphereMesh.Succeeded())
    {
        CloudMesh->SetStaticMesh(SphereMesh.Object);
    }
    // Cloud layer slightly larger than Earth
    CloudMesh->SetRelativeScale3D(FVector(1.01f, 1.01f, 1.01f));

    // === CREATE STAR SPHERE ===
    StarSphereMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StarSphereMesh"));
    StarSphereMesh->SetupAttachment(RootComponent);
    StarSphereMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    StarSphereMesh->SetCastShadow(false);
    if (SphereMesh.Succeeded())
    {
        StarSphereMesh->SetStaticMesh(SphereMesh.Object);
    }

    // === CREATE SUN LIGHT ===
    SunLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("SunLight"));
    SunLight->SetupAttachment(RootComponent);
    SunLight->SetIntensity(SunIntensity);
    SunLight->SetLightColor(SunColor);
    SunLight->SetMobility(EComponentMobility::Movable);
    SunLight->bUseTemperature = false;
    SunLight->bEnableLightShaftBloom = true;
    SunLight->BloomScale = 0.5f;
    SunLight->BloomThreshold = 0.5f;
    // Cast shadows from the sun
    SunLight->CastShadows = true;
    SunLight->CastDynamicShadows = true;

    // === CREATE EARTH SHINE LIGHT ===
    EarthShineLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("EarthShineLight"));
    EarthShineLight->SetupAttachment(RootComponent);
    EarthShineLight->SetIntensity(EarthShineIntensity);
    EarthShineLight->SetLightColor(EarthShineColor);
    EarthShineLight->SetMobility(EComponentMobility::Movable);
    EarthShineLight->SetSourceRadius(EarthRadius * 0.5f);
    EarthShineLight->SetAttenuationRadius(EarthDistance * 2.0f);
    EarthShineLight->CastShadows = false;

    // === CREATE AMBIENT SKY LIGHT ===
    AmbientLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("AmbientLight"));
    AmbientLight->SetupAttachment(RootComponent);
    AmbientLight->SetIntensity(AmbientIntensity);
    AmbientLight->SetMobility(EComponentMobility::Movable);
    AmbientLight->bLowerHemisphereIsBlack = false;  // Space has stars all around

    // === CREATE POST PROCESS COMPONENT ===
    PostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("PostProcess"));
    PostProcess->SetupAttachment(RootComponent);
    PostProcess->bUnbound = true;  // Affects entire world
}

void AF12SpaceEnvironment::BeginPlay()
{
    Super::BeginPlay();
    
    SetupComponents();
    SetupMaterials();
    SetupLighting();
    SetupPostProcess();
    
    UE_LOG(LogTemp, Log, TEXT("Space Environment initialized"));
}

void AF12SpaceEnvironment::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
    if (bAnimateOrbit || bAnimateEarthRotation)
    {
        UpdateOrbitalPositions(DeltaTime);
    }
    
    UpdateDayNightCycle();
}

void AF12SpaceEnvironment::SetupComponents()
{
    // Position Earth below the station
    FVector EarthPosition = FVector(0, 0, -EarthDistance);
    EarthMesh->SetRelativeLocation(EarthPosition);
    EarthMesh->SetRelativeScale3D(FVector(EarthRadius * 0.02f));  // Scale factor for default sphere (radius 50)
    
    // Position earth shine light between station and Earth
    EarthShineLight->SetRelativeLocation(FVector(0, 0, -EarthDistance * 0.3f));
    
    // Scale star sphere to be huge
    StarSphereMesh->SetRelativeScale3D(FVector(StarSphereRadius * 0.02f));
    // Flip star sphere normals (render inside)
    StarSphereMesh->SetRelativeScale3D(StarSphereMesh->GetRelativeScale3D() * FVector(-1, 1, 1));
}

void AF12SpaceEnvironment::SetupMaterials()
{
    // Apply Earth material if set
    if (EarthMaterial)
    {
        EarthDynamicMaterial = UMaterialInstanceDynamic::Create(EarthMaterial, this);
        EarthMesh->SetMaterial(0, EarthDynamicMaterial);
        
        // Set textures if provided
        if (EarthDayTexture)
        {
            EarthDynamicMaterial->SetTextureParameterValue(TEXT("DayTexture"), EarthDayTexture);
        }
        if (EarthNightTexture)
        {
            EarthDynamicMaterial->SetTextureParameterValue(TEXT("NightTexture"), EarthNightTexture);
        }
        if (EarthNormalTexture)
        {
            EarthDynamicMaterial->SetTextureParameterValue(TEXT("NormalTexture"), EarthNormalTexture);
        }
        if (EarthSpecularTexture)
        {
            EarthDynamicMaterial->SetTextureParameterValue(TEXT("SpecularTexture"), EarthSpecularTexture);
        }
    }
    
    // Apply Cloud material if set
    if (CloudMaterial)
    {
        CloudDynamicMaterial = UMaterialInstanceDynamic::Create(CloudMaterial, this);
        CloudMesh->SetMaterial(0, CloudDynamicMaterial);
        
        if (EarthCloudsTexture)
        {
            CloudDynamicMaterial->SetTextureParameterValue(TEXT("CloudTexture"), EarthCloudsTexture);
        }
    }
    else
    {
        // Hide clouds if no material
        CloudMesh->SetVisibility(false);
    }
    
    // Apply Star sphere material if set
    if (StarSphereMaterial)
    {
        StarSphereMesh->SetMaterial(0, StarSphereMaterial);
    }
}

void AF12SpaceEnvironment::SetupLighting()
{
    // Set sun direction
    SunLight->SetWorldRotation(SunDirection);
    SunLight->SetIntensity(SunIntensity);
    SunLight->SetLightColor(SunColor);
    
    // Configure earth shine
    EarthShineLight->SetIntensity(EarthShineIntensity);
    EarthShineLight->SetLightColor(EarthShineColor);
    
    // Configure ambient
    AmbientLight->SetIntensity(AmbientIntensity);
}

void AF12SpaceEnvironment::SetupPostProcess()
{
    // Configure lens flare and bloom settings
    FPostProcessSettings& Settings = PostProcess->Settings;
    
    // Bloom (for sun glow)
    Settings.bOverride_BloomIntensity = true;
    Settings.BloomIntensity = 1.5f;
    
    Settings.bOverride_BloomThreshold = true;
    Settings.BloomThreshold = 1.0f;
    
    // Lens flare
    Settings.bOverride_LensFlareIntensity = true;
    Settings.LensFlareIntensity = LensFlareIntensity;
    
    Settings.bOverride_LensFlareBokehSize = true;
    Settings.LensFlareBokehSize = LensFlareBokehSize;
    
    Settings.bOverride_LensFlareThreshold = true;
    Settings.LensFlareThreshold = LensFlareThreshold;
    
    // Auto exposure settings for space (prevent over-exposure)
    Settings.bOverride_AutoExposureMethod = true;
    Settings.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
    
    Settings.bOverride_AutoExposureBias = true;
    Settings.AutoExposureBias = 0.0f;
    
    // Slight vignette for cinematic feel
    Settings.bOverride_VignetteIntensity = true;
    Settings.VignetteIntensity = 0.3f;
}

void AF12SpaceEnvironment::UpdateOrbitalPositions(float DeltaTime)
{
    // Update orbital position (station orbits Earth)
    if (bAnimateOrbit)
    {
        OrbitalPosition += StationOrbitSpeed * DeltaTime;
        if (OrbitalPosition >= 360.0f) OrbitalPosition -= 360.0f;
        
        // The station stays at origin, Earth moves relative to it
        // This creates the illusion of the station orbiting
        float OrbitRadians = FMath::DegreesToRadians(OrbitalPosition);
        
        // Move Earth in a circle below the station
        float EarthX = FMath::Sin(OrbitRadians) * EarthDistance * 0.3f;
        float EarthZ = -EarthDistance;
        float EarthY = FMath::Cos(OrbitRadians) * EarthDistance * 0.3f - EarthDistance * 0.7f;
        
        // Actually, for simplicity, keep Earth below but rotate around Z
        FVector EarthPosition = FVector(0, 0, -EarthDistance);
        EarthMesh->SetRelativeLocation(EarthPosition);
        
        // Rotate Earth to simulate day/night from orbital motion
        FRotator EarthRot = EarthMesh->GetRelativeRotation();
        EarthRot.Yaw = OrbitalPosition * 0.5f;  // Slower apparent rotation
        EarthMesh->SetRelativeRotation(EarthRot);
    }
    
    // Earth's own rotation (24-hour day cycle)
    if (bAnimateEarthRotation)
    {
        FRotator CurrentRot = EarthMesh->GetRelativeRotation();
        CurrentRot.Yaw += EarthRotationSpeed * DeltaTime;
        EarthMesh->SetRelativeRotation(CurrentRot);
        
        // Clouds rotate slightly different speed for parallax effect
        FRotator CloudRot = CloudMesh->GetRelativeRotation();
        CloudRot.Yaw += EarthRotationSpeed * 0.8f * DeltaTime;
        CloudMesh->SetRelativeRotation(CloudRot);
    }
}

void AF12SpaceEnvironment::UpdateDayNightCycle()
{
    // Update Earth material with sun direction for day/night terminator
    if (EarthDynamicMaterial)
    {
        FVector SunDir = SunLight->GetForwardVector();
        EarthDynamicMaterial->SetVectorParameterValue(TEXT("SunDirection"), FLinearColor(SunDir.X, SunDir.Y, SunDir.Z, 0));
    }
}

void AF12SpaceEnvironment::SetSunDirection(FRotator NewDirection)
{
    SunDirection = NewDirection;
    if (SunLight)
    {
        SunLight->SetWorldRotation(SunDirection);
    }
}

void AF12SpaceEnvironment::SetOrbitalPosition(float NewPosition)
{
    OrbitalPosition = FMath::Fmod(NewPosition, 360.0f);
    if (OrbitalPosition < 0) OrbitalPosition += 360.0f;
}