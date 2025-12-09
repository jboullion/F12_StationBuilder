// F12BuilderController.cpp
// Implementation of the builder player controller

#include "F12BuilderController.h"
#include "F12Module.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

AF12BuilderController::AF12BuilderController()
{
    PrimaryActorTick.bCanEverTick = true;
    bShowMouseCursor = true;
    bEnableClickEvents = true;
    bEnableMouseOverEvents = true;
}

void AF12BuilderController::BeginPlay()
{
    Super::BeginPlay();

    // Find or create the grid system
    GridSystem = Cast<AF12GridSystem>(UGameplayStatics::GetActorOfClass(GetWorld(), AF12GridSystem::StaticClass()));
    
    if (!GridSystem)
    {
        // Spawn a grid system if one doesn't exist
        FActorSpawnParameters SpawnParams;
        GridSystem = GetWorld()->SpawnActor<AF12GridSystem>(AF12GridSystem::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
    }

    // Create ghost preview module
    if (ModuleClass)
    {
        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        
        GhostModule = GetWorld()->SpawnActor<AF12Module>(ModuleClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
        
        if (GhostModule)
        {
            // Make it translucent/ghostly
            GhostModule->SetActorEnableCollision(false);
            // You'd apply a translucent material here
        }
    }

    // Spawn the initial "core" module at origin
    if (ModuleClass && GridSystem)
    {
        FF12GridCoord CoreCoord(0, 0, 0);
        FVector CorePos = GridSystem->GridToWorld(CoreCoord);
        
        FActorSpawnParameters SpawnParams;
        AF12Module* CoreModule = GetWorld()->SpawnActor<AF12Module>(ModuleClass, CorePos, FRotator::ZeroRotator, SpawnParams);
        
        if (CoreModule && DefaultModuleMaterial)
        {
            CoreModule->TileMaterial = DefaultModuleMaterial;
            CoreModule->GenerateModule();
        }
        
        GridSystem->SetOccupied(CoreCoord, CoreModule);
        
        UE_LOG(LogTemp, Log, TEXT("Spawned core module at origin"));
    }
}

void AF12BuilderController::SetupInputComponent()
{
    Super::SetupInputComponent();

    // Bind left click to place
    InputComponent->BindAction("PlaceModule", IE_Pressed, this, &AF12BuilderController::OnLeftClick);
    
    // Bind right click to remove
    InputComponent->BindAction("RemoveModule", IE_Pressed, this, &AF12BuilderController::OnRightClick);
}

void AF12BuilderController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    UpdateGhostPreview();
}

bool AF12BuilderController::TraceFromCamera(FHitResult& OutHit)
{
    // Get mouse position in world
    FVector WorldLocation, WorldDirection;
    
    if (DeprojectMousePositionToWorld(WorldLocation, WorldDirection))
    {
        FVector TraceStart = WorldLocation;
        FVector TraceEnd = WorldLocation + (WorldDirection * TraceDistance);
        
        FCollisionQueryParams QueryParams;
        QueryParams.AddIgnoredActor(GhostModule);
        
        return GetWorld()->LineTraceSingleByChannel(OutHit, TraceStart, TraceEnd, ECC_Visibility, QueryParams);
    }
    
    return false;
}

void AF12BuilderController::UpdateGhostPreview()
{
    if (!GhostModule || !GridSystem)
        return;

    FHitResult Hit;
    
    if (TraceFromCamera(Hit))
    {
        // Did we hit an existing module?
        AF12Module* HitModule = Cast<AF12Module>(Hit.GetActor());
        
        if (HitModule)
        {
            // Find which face was hit and get the neighbor position
            // For now, find the grid coord of the hit module
            FF12GridCoord HitCoord = GridSystem->WorldToGrid(HitModule->GetActorLocation());
            
            // Determine which face was hit
            int32 FaceIndex = GridSystem->GetHitFaceIndex(HitCoord, Hit.Location);
            
            // Get the neighbor coordinate for that face
            CurrentGridCoord = GridSystem->GetNeighborCoordForFace(HitCoord, FaceIndex);
        }
        else
        {
            // Hit something else (ground plane, etc.) - snap to grid
            CurrentGridCoord = GridSystem->WorldToGrid(Hit.Location);
        }
        
        // Check if position is valid (not occupied)
        bValidPlacement = !GridSystem->IsOccupied(CurrentGridCoord);
        
        // Update ghost position
        FVector GhostPos = GridSystem->GridToWorld(CurrentGridCoord);
        GhostModule->SetActorLocation(GhostPos);
        GhostModule->SetActorHiddenInGame(false);
        
        // TODO: Change ghost material color based on validity
        // Green = valid, Red = invalid
    }
    else
    {
        // No hit - hide ghost
        GhostModule->SetActorHiddenInGame(true);
        bValidPlacement = false;
    }
}

void AF12BuilderController::OnLeftClick()
{
    PlaceModule();
}

void AF12BuilderController::OnRightClick()
{
    RemoveModule();
}

void AF12BuilderController::PlaceModule()
{
    if (!bValidPlacement || !ModuleClass || !GridSystem)
        return;

    FVector SpawnPos = GridSystem->GridToWorld(CurrentGridCoord);
    
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    
    AF12Module* NewModule = GetWorld()->SpawnActor<AF12Module>(ModuleClass, SpawnPos, FRotator::ZeroRotator, SpawnParams);
    
    if (NewModule)
    {
        if (DefaultModuleMaterial)
        {
            NewModule->TileMaterial = DefaultModuleMaterial;
            NewModule->GenerateModule();
        }
        
        GridSystem->SetOccupied(CurrentGridCoord, NewModule);
        
        UE_LOG(LogTemp, Log, TEXT("Placed module at (%d, %d, %d)"), 
            CurrentGridCoord.X, CurrentGridCoord.Y, CurrentGridCoord.Z);
    }
}

void AF12BuilderController::RemoveModule()
{
    if (!GridSystem)
        return;

    // Don't allow removing the core module at origin
    if (CurrentGridCoord.X == 0 && CurrentGridCoord.Y == 0 && CurrentGridCoord.Z == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot remove core module"));
        return;
    }

    AActor* ModuleToRemove = GridSystem->GetModuleAt(CurrentGridCoord);
    
    if (ModuleToRemove)
    {
        GridSystem->ClearOccupied(CurrentGridCoord);
        ModuleToRemove->Destroy();
        
        UE_LOG(LogTemp, Log, TEXT("Removed module at (%d, %d, %d)"), 
            CurrentGridCoord.X, CurrentGridCoord.Y, CurrentGridCoord.Z);
    }
}
