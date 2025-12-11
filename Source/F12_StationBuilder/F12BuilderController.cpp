// F12BuilderController.cpp
// Implementation of the builder player controller
// Uses instanced rendering exclusively

#include "F12BuilderController.h"
#include "F12InstancedRenderer.h"
#include "F12ProceduralGenerator.h"
#include "F12GeneratorWidget.h"
#include "F12BuilderPawn.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

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

    // Create procedural generator
    ProceduralGenerator = NewObject<UF12ProceduralGenerator>(this, TEXT("ProceduralGenerator"));

    // Find the grid system
    GridSystem = Cast<AF12GridSystem>(UGameplayStatics::GetActorOfClass(GetWorld(), AF12GridSystem::StaticClass()));
    
    if (!GridSystem)
    {
        FActorSpawnParameters SpawnParams;
        GridSystem = GetWorld()->SpawnActor<AF12GridSystem>(AF12GridSystem::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
        UE_LOG(LogTemp, Log, TEXT("Created GridSystem"));
    }

    // Find the instanced renderer
    if (!InstancedRenderer)
    {
        InstancedRenderer = Cast<AF12InstancedRenderer>(
            UGameplayStatics::GetActorOfClass(GetWorld(), AF12InstancedRenderer::StaticClass())
        );
    }

    if (!InstancedRenderer)
    {
        UE_LOG(LogTemp, Error, TEXT("No F12InstancedRenderer found in level! Add one and assign the static mesh."));
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("Found InstancedRenderer"));
        
        // Spawn the initial "core" module at origin
        FF12GridCoord CoreCoord(0, 0, 0);
        InstancedRenderer->AddModule(CoreCoord, 0);
        GridSystem->SetOccupied(CoreCoord, nullptr);
        UE_LOG(LogTemp, Log, TEXT("Spawned core module at origin"));
    }

    // Start in Build mode
    SetMode(EF12BuilderMode::Build);

    // Spawn the HUD widget
    if (HUDWidgetClass)
    {
        UUserWidget* HUDWidget = CreateWidget<UUserWidget>(this, HUDWidgetClass);
        if (HUDWidget)
        {
            HUDWidget->AddToViewport();
            UE_LOG(LogTemp, Log, TEXT("HUD Widget created"));
        }
    }

    // Initialize procedural generator
    if (ProceduralGenerator)
    {
        ProceduralGenerator->Initialize(GridSystem, this);
    }

    // Spawn the generator widget
    if (GeneratorWidgetClass)
    {
        GeneratorWidget = CreateWidget<UF12GeneratorWidget>(this, GeneratorWidgetClass);
        if (GeneratorWidget)
        {
            GeneratorWidget->AddToViewport(10);
            UE_LOG(LogTemp, Log, TEXT("Generator Widget created"));
        }
    }

    // Create ghost preview mesh components
    InitializeGhostPreview();

    // Cache the builder pawn reference
    CachedBuilderPawn = Cast<AF12BuilderPawn>(GetPawn());
    if (CachedBuilderPawn)
    {
        UE_LOG(LogTemp, Log, TEXT("Found BuilderPawn for third-person view"));
    }
}

void AF12BuilderController::SetupInputComponent()
{
    Super::SetupInputComponent();

    InputComponent->BindAction("PrimaryAction", IE_Pressed, this, &AF12BuilderController::OnPrimaryAction);
    InputComponent->BindAction("PrimaryAction", IE_Released, this, &AF12BuilderController::OnPrimaryActionReleased);
    InputComponent->BindAction("SecondaryAction", IE_Pressed, this, &AF12BuilderController::OnSecondaryAction);
    
    InputComponent->BindAction("CycleMode", IE_Pressed, this, &AF12BuilderController::OnCycleMode);
    InputComponent->BindAction("BuildMode", IE_Pressed, this, &AF12BuilderController::OnSetBuildMode);
    InputComponent->BindAction("PaintMode", IE_Pressed, this, &AF12BuilderController::OnSetPaintMode);
    InputComponent->BindAction("DeleteMode", IE_Pressed, this, &AF12BuilderController::OnSetDeleteMode);
    
    InputComponent->BindAction("ZoomIn", IE_Pressed, this, &AF12BuilderController::OnScrollUp);
    InputComponent->BindAction("ZoomOut", IE_Pressed, this, &AF12BuilderController::OnScrollDown);
    
    InputComponent->BindAction("Modifier", IE_Pressed, this, &AF12BuilderController::OnModifierPressed);
    InputComponent->BindAction("Modifier", IE_Released, this, &AF12BuilderController::OnModifierReleased);

    InputComponent->BindAction("ToggleGenerator", IE_Pressed, this, &AF12BuilderController::OnToggleGenerator);
    
    // Camera rotation (handled by controller, forwarded to pawn)
    InputComponent->BindAction("CameraRotate", IE_Pressed, this, &AF12BuilderController::OnCameraRotatePressed);
    InputComponent->BindAction("CameraRotate", IE_Released, this, &AF12BuilderController::OnCameraRotateReleased);
    
    // Paint cycling
    InputComponent->BindAction("CyclePaint", IE_Pressed, this, &AF12BuilderController::OnCyclePaint);
}

void AF12BuilderController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
    // Handle camera rotation
    UpdateCameraRotation();
    
    UpdatePreview();
    
    // Update drag preview in build mode
    if (CurrentMode == EF12BuilderMode::Build)
    {
        if (bIsDragging)
        {
            UpdateDragPreview();
        }
        else
        {
            UpdateGhostPreview();
        }
    }
    else
    {
        HideAllGhosts();
        bIsDragging = false;
    }
    
    // Handle drag painting in paint mode
    if (bIsPainting && CurrentMode == EF12BuilderMode::Paint && InstancedRenderer)
    {
        FHitResult Hit;
        if (TraceFromCamera(Hit))
        {
            FF12GridCoord HitGridCoord;
            int32 TileIndex;
            
            if (InstancedRenderer->GetHitModuleAndTile(Hit, HitGridCoord, TileIndex))
            {
                bool bShouldPaint = false;
                
                if (bModifierHeld)
                {
                    // Single tile mode - check if different tile
                    if (!(HitGridCoord == LastPaintedCoord && TileIndex == LastPaintedTile))
                    {
                        bShouldPaint = true;
                    }
                }
                else
                {
                    // Module mode - check if different module
                    if (!(HitGridCoord == LastPaintedCoord))
                    {
                        bShouldPaint = true;
                    }
                }
                
                if (bShouldPaint)
                {
                    if (bModifierHeld)
                    {
                        InstancedRenderer->SetTileMaterial(HitGridCoord, TileIndex, CurrentPaintMaterialIndex);
                    }
                    else
                    {
                        InstancedRenderer->SetModuleMaterial(HitGridCoord, CurrentPaintMaterialIndex);
                    }
                    LastPaintedCoord = HitGridCoord;
                    LastPaintedTile = TileIndex;
                }
            }
        }
    }
    
    // Update delete highlight
    if (CurrentMode == EF12BuilderMode::Delete)
    {
        UpdateDeleteHighlight();
    }
    else if (InstancedRenderer)
    {
        // Clear highlights when not in delete mode
        if (bHasHighlight)
        {
            InstancedRenderer->ClearAllHighlights();
            bHasHighlight = false;
        }
    }
}

// === CAMERA HELPERS ===

AF12BuilderPawn* AF12BuilderController::GetBuilderPawn() const
{
    if (CachedBuilderPawn)
    {
        return CachedBuilderPawn;
    }
    
    // Try to find it if not cached
    return Cast<AF12BuilderPawn>(GetPawn());
}

// === MODE SYSTEM ===

void AF12BuilderController::SetMode(EF12BuilderMode NewMode)
{
    CurrentMode = NewMode;
    UE_LOG(LogTemp, Log, TEXT("Mode changed to: %s"), *GetModeName());
}

void AF12BuilderController::CycleMode()
{
    switch (CurrentMode)
    {
        case EF12BuilderMode::Build:  SetMode(EF12BuilderMode::Paint); break;
        case EF12BuilderMode::Paint:  SetMode(EF12BuilderMode::Delete); break;
        case EF12BuilderMode::Delete: SetMode(EF12BuilderMode::Build); break;
    }
}

void AF12BuilderController::SetBuildMode() { SetMode(EF12BuilderMode::Build); }
void AF12BuilderController::SetPaintMode() { SetMode(EF12BuilderMode::Paint); }
void AF12BuilderController::SetDeleteMode() { SetMode(EF12BuilderMode::Delete); }

// === INPUT HANDLERS ===

void AF12BuilderController::OnPrimaryAction() 
{ 
    if (CurrentMode == EF12BuilderMode::Build)
    {
        // Only start drag if shift is held
        if (bModifierHeld)
        {
            FHitResult Hit;
            if (TraceFromCamera(Hit) && InstancedRenderer && GridSystem)
            {
                FF12GridCoord HitGridCoord;
                int32 TileIndex;
                
                if (InstancedRenderer->GetHitModuleAndTile(Hit, HitGridCoord, TileIndex))
                {
                    // Get the neighbor coord and direction for this face
                    DragStartCoord = GridSystem->GetNeighborCoordForFace(HitGridCoord, TileIndex);
                    DragFaceIndex = TileIndex;
                    
                    // Get the face normal direction from the grid system
                    DragDirection = GridSystem->GetFaceNormal(TileIndex);
                    
                    // Store the world position where we started dragging
                    DragStartWorldPos = Hit.ImpactPoint;
                    
                    // Only start drag if the start position is valid
                    if (!GridSystem->IsOccupied(DragStartCoord))
                    {
                        bIsDragging = true;
                        DragPreviewCoords.Empty();
                        DragPreviewCoords.Add(DragStartCoord);
                        UE_LOG(LogTemp, Log, TEXT("Started drag from face %d, direction: %s"), TileIndex, *DragDirection.ToString());
                    }
                }
            }
        }
        else
        {
            // No shift held - just place a single module
            if (bValidPlacement)
            {
                PlaceModule();
            }
        }
    }
    else if (CurrentMode == EF12BuilderMode::Paint)
    {
        // Start drag painting
        bIsPainting = true;
        LastPaintedCoord = FF12GridCoord(INT_MAX, INT_MAX, INT_MAX);
        LastPaintedTile = -1;
        PrimaryAction();  // Paint the first one
    }
    else
    {
        PrimaryAction(); 
    }
}

void AF12BuilderController::OnPrimaryActionReleased()
{
    if (bIsDragging && CurrentMode == EF12BuilderMode::Build)
    {
        // Place all modules in the drag preview
        PlaceDraggedModules();
    }
    bIsDragging = false;
    bIsPainting = false;
    DragPreviewCoords.Empty();
    HideAllGhosts();
}
void AF12BuilderController::OnSecondaryAction() { SecondaryAction(); }
void AF12BuilderController::OnCycleMode() { CycleMode(); }
void AF12BuilderController::OnSetBuildMode() { SetBuildMode(); }
void AF12BuilderController::OnSetPaintMode() { SetPaintMode(); }
void AF12BuilderController::OnSetDeleteMode() { SetDeleteMode(); }

void AF12BuilderController::OnScrollUp()
{
    // Always zoom camera (use T to cycle paints)
    if (AF12BuilderPawn* BuilderPawn = GetBuilderPawn())
    {
        BuilderPawn->ZoomCamera(-BuilderPawn->ZoomSpeed);
    }
}

void AF12BuilderController::OnScrollDown()
{
    // Always zoom camera (use T to cycle paints)
    if (AF12BuilderPawn* BuilderPawn = GetBuilderPawn())
    {
        BuilderPawn->ZoomCamera(BuilderPawn->ZoomSpeed);
    }
}

void AF12BuilderController::OnModifierPressed() { bModifierHeld = true; }
void AF12BuilderController::OnModifierReleased() { bModifierHeld = false; }
void AF12BuilderController::OnToggleGenerator() { ToggleGeneratorPanel(); }

void AF12BuilderController::OnCyclePaint()
{
    if (PaintColors.Num() > 0)
    {
        CurrentPaintMaterialIndex = (CurrentPaintMaterialIndex + 1) % PaintColors.Num();
        UE_LOG(LogTemp, Log, TEXT("Paint material cycled to: %d"), CurrentPaintMaterialIndex);
    }
}

void AF12BuilderController::OnCameraRotatePressed()
{
    bIsRotatingCamera = true;
}

void AF12BuilderController::OnCameraRotateReleased()
{
    bIsRotatingCamera = false;
}

void AF12BuilderController::UpdateCameraRotation()
{
    if (!bIsRotatingCamera)
        return;
    
    AF12BuilderPawn* BuilderPawn = GetBuilderPawn();
    if (!BuilderPawn)
        return;
    
    float MouseX, MouseY;
    GetInputMouseDelta(MouseX, MouseY);
    
    if (MouseX != 0.0f || MouseY != 0.0f)
    {
        // Forward rotation to pawn (Y is NOT inverted - mouse up = look up)
        float Sensitivity = BuilderPawn->CameraRotationSpeed;
        BuilderPawn->RotateCamera(MouseX * Sensitivity, MouseY * Sensitivity);
    }
}

// === ACTIONS ===

void AF12BuilderController::PrimaryAction()
{
    switch (CurrentMode)
    {
        case EF12BuilderMode::Build:  HandleBuildPrimary(); break;
        case EF12BuilderMode::Paint:  HandlePaintPrimary(); break;
        case EF12BuilderMode::Delete: HandleDeletePrimary(); break;
    }
}

void AF12BuilderController::SecondaryAction()
{
    switch (CurrentMode)
    {
        case EF12BuilderMode::Build:  HandleBuildSecondary(); break;
        case EF12BuilderMode::Paint:  HandlePaintSecondary(); break;
        case EF12BuilderMode::Delete: HandleDeleteSecondary(); break;
    }
}

void AF12BuilderController::CyclePaintMaterial()
{
    if (PaintColors.Num() > 0)
    {
        CurrentPaintMaterialIndex = (CurrentPaintMaterialIndex + 1) % PaintColors.Num();
    }
}

// === MODE-SPECIFIC HANDLERS ===

void AF12BuilderController::HandleBuildPrimary()
{
    PlaceModule();
}

void AF12BuilderController::HandleBuildSecondary()
{
    // Right-click in build mode does nothing
    // Use Delete mode (key 3) to remove modules
}

void AF12BuilderController::HandlePaintPrimary()
{
    if (!InstancedRenderer)
        return;

    FHitResult Hit;
    if (TraceFromCamera(Hit))
    {
        FF12GridCoord GridCoord;
        int32 TileIndex;
        
        if (InstancedRenderer->GetHitModuleAndTile(Hit, GridCoord, TileIndex))
        {
            if (bModifierHeld)
            {
                // Paint single tile
                InstancedRenderer->SetTileMaterial(GridCoord, TileIndex, CurrentPaintMaterialIndex);
                UE_LOG(LogTemp, Log, TEXT("Painted tile %d with material %d"), TileIndex, CurrentPaintMaterialIndex);
            }
            else
            {
                // Paint whole module
                InstancedRenderer->SetModuleMaterial(GridCoord, CurrentPaintMaterialIndex);
                UE_LOG(LogTemp, Log, TEXT("Painted module with material %d"), CurrentPaintMaterialIndex);
            }
        }
    }
}

void AF12BuilderController::HandlePaintSecondary()
{
    CyclePaintMaterial();
}

void AF12BuilderController::HandleDeletePrimary()
{
    if (!InstancedRenderer || !GridSystem)
        return;

    FHitResult Hit;
    if (TraceFromCamera(Hit))
    {
        FF12GridCoord GridCoord;
        int32 TileIndex;
        
        if (InstancedRenderer->GetHitModuleAndTile(Hit, GridCoord, TileIndex))
        {
            if (bModifierHeld)
            {
                // Delete single tile (hide it)
                InstancedRenderer->SetTileVisible(GridCoord, TileIndex, false);
                UE_LOG(LogTemp, Log, TEXT("Hid tile %d"), TileIndex);
            }
            else
            {
                // Can't delete core
                if (GridCoord.X == 0 && GridCoord.Y == 0 && GridCoord.Z == 0)
                {
                    UE_LOG(LogTemp, Warning, TEXT("Cannot delete core module"));
                    return;
                }
                
                // Delete whole module
                InstancedRenderer->RemoveModule(GridCoord);
                GridSystem->ClearOccupied(GridCoord);
                UE_LOG(LogTemp, Log, TEXT("Deleted module at (%d, %d, %d)"), GridCoord.X, GridCoord.Y, GridCoord.Z);
            }
        }
    }
}

void AF12BuilderController::HandleDeleteSecondary()
{
    // Restore hidden tile
    if (!InstancedRenderer)
        return;

    FHitResult Hit;
    if (TraceFromCamera(Hit))
    {
        FF12GridCoord GridCoord;
        int32 TileIndex;
        
        if (InstancedRenderer->GetHitModuleAndTile(Hit, GridCoord, TileIndex))
        {
            if (!InstancedRenderer->GetTileVisible(GridCoord, TileIndex))
            {
                InstancedRenderer->SetTileVisible(GridCoord, TileIndex, true);
                UE_LOG(LogTemp, Log, TEXT("Restored tile %d"), TileIndex);
            }
        }
    }
}

// === UTILITY ===

bool AF12BuilderController::TraceFromCamera(FHitResult& OutHit)
{
    FVector WorldLocation, WorldDirection;
    
    if (DeprojectMousePositionToWorld(WorldLocation, WorldDirection))
    {
        FVector TraceStart = WorldLocation;
        FVector TraceEnd = WorldLocation + (WorldDirection * TraceDistance);
        
        FCollisionQueryParams QueryParams;
        
        // Ignore the pawn we're controlling
        if (GetPawn())
        {
            QueryParams.AddIgnoredActor(GetPawn());
        }
        
        return GetWorld()->LineTraceSingleByChannel(OutHit, TraceStart, TraceEnd, ECC_Visibility, QueryParams);
    }
    
    return false;
}

void AF12BuilderController::UpdatePreview()
{
    if (!GridSystem || !InstancedRenderer)
    {
        bValidPlacement = false;
        return;
    }

    // Only update placement preview in Build mode
    if (CurrentMode != EF12BuilderMode::Build)
    {
        bValidPlacement = false;
        return;
    }

    FHitResult Hit;
    
    if (TraceFromCamera(Hit))
    {
        FF12GridCoord HitGridCoord;
        int32 TileIndex;
        
        // Check if we hit an existing module
        if (InstancedRenderer->GetHitModuleAndTile(Hit, HitGridCoord, TileIndex))
        {
            // Get the neighbor coord for the hit face
            CurrentGridCoord = GridSystem->GetNeighborCoordForFace(HitGridCoord, TileIndex);
        }
        else
        {
            // Hit something else, use grid position
            CurrentGridCoord = GridSystem->WorldToGrid(Hit.Location);
        }
        
        bValidPlacement = !GridSystem->IsOccupied(CurrentGridCoord);
    }
    else
    {
        bValidPlacement = false;
    }
}

void AF12BuilderController::UpdateDeleteHighlight()
{
    if (!InstancedRenderer)
        return;

    FHitResult Hit;
    
    if (TraceFromCamera(Hit))
    {
        FF12GridCoord HitGridCoord;
        int32 TileIndex;
        
        if (InstancedRenderer->GetHitModuleAndTile(Hit, HitGridCoord, TileIndex))
        {
            // Shift = single tile, no shift = full module
            bool bSingleTile = bModifierHeld;
            
            // Check if highlight needs to change
            bool bNeedsUpdate = !bHasHighlight ||
                               !(HitGridCoord == LastHighlightCoord) ||
                               (bSingleTile && LastHighlightTile != TileIndex) ||
                               (bSingleTile != bLastHighlightWasSingleTile);
            
            if (bNeedsUpdate)
            {
                InstancedRenderer->SetTileHighlight(HitGridCoord, TileIndex, true, bSingleTile);
                LastHighlightCoord = HitGridCoord;
                LastHighlightTile = TileIndex;
                bLastHighlightWasSingleTile = bSingleTile;
                bHasHighlight = true;
            }
            return;
        }
    }
    
    // No hit - clear highlight
    if (bHasHighlight)
    {
        InstancedRenderer->ClearAllHighlights();
        bHasHighlight = false;
        LastHighlightTile = -1;
    }
}

void AF12BuilderController::InitializeGhostPreview()
{
    if (!InstancedRenderer || !InstancedRenderer->TileStaticMesh)
        return;

    // Get face transforms from renderer
    GhostFaceTransforms = InstancedRenderer->GetFaceTransforms();
    
    if (GhostFaceTransforms.Num() != 12)
    {
        UE_LOG(LogTemp, Warning, TEXT("Ghost init: Expected 12 face transforms, got %d"), GhostFaceTransforms.Num());
        return;
    }

    // Create an actor to hold our ghost meshes
    AActor* GhostActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass());
    if (!GhostActor)
        return;

    // Create root component
    USceneComponent* GhostRoot = NewObject<USceneComponent>(GhostActor);
    GhostRoot->RegisterComponent();
    GhostActor->SetRootComponent(GhostRoot);

    // Create enough mesh components for MaxDragPreview modules (12 tiles each)
    int32 TotalMeshes = MaxDragPreview * 12;
    GhostMeshComponents.SetNum(TotalMeshes);
    
    for (int32 i = 0; i < TotalMeshes; i++)
    {
        UStaticMeshComponent* GhostMesh = NewObject<UStaticMeshComponent>(GhostActor);
        GhostMesh->SetStaticMesh(InstancedRenderer->TileStaticMesh);
        GhostMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        GhostMesh->SetVisibility(false);
        
        // Apply ghost material if set
        if (GhostMaterial)
        {
            GhostMesh->SetMaterial(0, GhostMaterial);
        }
        
        GhostMesh->AttachToComponent(GhostRoot, FAttachmentTransformRules::KeepRelativeTransform);
        GhostMesh->RegisterComponent();
        
        GhostMeshComponents[i] = GhostMesh;
    }
    
    UE_LOG(LogTemp, Log, TEXT("Ghost preview created with %d tile meshes (max %d modules)"), TotalMeshes, MaxDragPreview);
}

void AF12BuilderController::UpdateGhostPreview()
{
    // Single ghost at cursor when not dragging
    if (bValidPlacement && !bIsDragging)
    {
        ShowGhostAtCoord(CurrentGridCoord, 0);
        
        // Hide remaining ghosts
        for (int32 i = 12; i < GhostMeshComponents.Num(); i++)
        {
            if (GhostMeshComponents[i])
            {
                GhostMeshComponents[i]->SetVisibility(false);
            }
        }
    }
    else if (!bIsDragging)
    {
        HideAllGhosts();
    }
}

void AF12BuilderController::UpdateDragPreview()
{
    if (!GridSystem || !InstancedRenderer)
        return;

    // Get current mouse position in world
    FHitResult Hit;
    FVector CurrentMouseWorld;
    
    if (TraceFromCamera(Hit))
    {
        CurrentMouseWorld = Hit.ImpactPoint;
    }
    else
    {
        // No hit - try to project mouse into world at a reasonable distance
        FVector WorldLocation, WorldDirection;
        if (DeprojectMousePositionToWorld(WorldLocation, WorldDirection))
        {
            CurrentMouseWorld = WorldLocation + WorldDirection * 2000.0f;
        }
        else
        {
            return;
        }
    }
    
    // Calculate how far we've dragged along the drag direction
    FVector DragVector = CurrentMouseWorld - DragStartWorldPos;
    float DragDistance = FVector::DotProduct(DragVector, DragDirection);
    
    // Clamp to positive (can only drag outward)
    DragDistance = FMath::Max(0.0f, DragDistance);
    
    // Calculate number of modules based on distance
    // Use the actual module spacing for accurate placement
    float StepSize = GridSystem->GetModuleSpacing();
    int32 NumModules = FMath::FloorToInt(DragDistance / StepSize) + 1;
    NumModules = FMath::Clamp(NumModules, 1, MaxDragPreview);
    
    // Build list of preview coordinates
    DragPreviewCoords.Empty();
    FIntVector GridOffset = GridSystem->GetGridOffsetForFace(DragFaceIndex);
    
    for (int32 i = 0; i < NumModules; i++)
    {
        FF12GridCoord Coord;
        Coord.X = DragStartCoord.X + GridOffset.X * i;
        Coord.Y = DragStartCoord.Y + GridOffset.Y * i;
        Coord.Z = DragStartCoord.Z + GridOffset.Z * i;
        
        // Only add if not occupied
        if (!GridSystem->IsOccupied(Coord))
        {
            DragPreviewCoords.Add(Coord);
        }
        else
        {
            break;  // Stop at first occupied cell
        }
    }
    
    // Show ghosts for each preview coord
    for (int32 i = 0; i < DragPreviewCoords.Num(); i++)
    {
        ShowGhostAtCoord(DragPreviewCoords[i], i);
    }
    
    // Hide remaining ghosts
    int32 StartHide = DragPreviewCoords.Num() * 12;
    for (int32 i = StartHide; i < GhostMeshComponents.Num(); i++)
    {
        if (GhostMeshComponents[i])
        {
            GhostMeshComponents[i]->SetVisibility(false);
        }
    }
}

void AF12BuilderController::ShowGhostAtCoord(FF12GridCoord Coord, int32 GhostSetIndex)
{
    if (!GridSystem)
        return;
    
    int32 BaseIndex = GhostSetIndex * 12;
    if (BaseIndex + 11 >= GhostMeshComponents.Num())
        return;

    // Get world position for this module coordinate
    FVector ModuleCenter = GridSystem->GridToWorld(Coord);
    
    // Position each ghost tile at its face location
    for (int32 i = 0; i < 12; i++)
    {
        int32 MeshIndex = BaseIndex + i;
        if (GhostMeshComponents[MeshIndex] && GhostFaceTransforms.IsValidIndex(i))
        {
            // Face transform is relative to module center
            FTransform FaceLocal = GhostFaceTransforms[i];
            
            // Apply to world position
            FVector TilePos = ModuleCenter + FaceLocal.GetLocation();
            FQuat TileRot = FaceLocal.GetRotation();
            
            GhostMeshComponents[MeshIndex]->SetWorldLocation(TilePos);
            GhostMeshComponents[MeshIndex]->SetWorldRotation(TileRot);
            GhostMeshComponents[MeshIndex]->SetVisibility(true);
        }
    }
}

void AF12BuilderController::HideGhost()
{
    // Hide first ghost module only
    for (int32 i = 0; i < 12 && i < GhostMeshComponents.Num(); i++)
    {
        if (GhostMeshComponents[i])
        {
            GhostMeshComponents[i]->SetVisibility(false);
        }
    }
}

void AF12BuilderController::HideAllGhosts()
{
    for (UStaticMeshComponent* GhostMesh : GhostMeshComponents)
    {
        if (GhostMesh)
        {
            GhostMesh->SetVisibility(false);
        }
    }
}

void AF12BuilderController::PlaceDraggedModules()
{
    if (!GridSystem || !InstancedRenderer || DragPreviewCoords.Num() == 0)
        return;

    for (const FF12GridCoord& Coord : DragPreviewCoords)
    {
        if (!GridSystem->IsOccupied(Coord))
        {
            InstancedRenderer->AddModule(Coord, CurrentPaintMaterialIndex);
            GridSystem->SetOccupied(Coord, nullptr);
        }
    }
    
    UE_LOG(LogTemp, Log, TEXT("Placed %d modules from drag"), DragPreviewCoords.Num());
}

void AF12BuilderController::PlaceModule()
{
    if (!bValidPlacement || !GridSystem || !InstancedRenderer)
        return;

    InstancedRenderer->AddModule(CurrentGridCoord, CurrentPaintMaterialIndex);
    GridSystem->SetOccupied(CurrentGridCoord, nullptr);
    
    UE_LOG(LogTemp, Log, TEXT("Placed module at (%d, %d, %d)"), 
        CurrentGridCoord.X, CurrentGridCoord.Y, CurrentGridCoord.Z);
}

void AF12BuilderController::RemoveModule()
{
    if (!GridSystem || !InstancedRenderer)
        return;

    FHitResult Hit;
    if (TraceFromCamera(Hit))
    {
        FF12GridCoord GridCoord;
        int32 TileIndex;
        
        if (InstancedRenderer->GetHitModuleAndTile(Hit, GridCoord, TileIndex))
        {
            // Can't delete core
            if (GridCoord.X == 0 && GridCoord.Y == 0 && GridCoord.Z == 0)
            {
                UE_LOG(LogTemp, Warning, TEXT("Cannot remove core module"));
                return;
            }
            
            InstancedRenderer->RemoveModule(GridCoord);
            GridSystem->ClearOccupied(GridCoord);
            
            UE_LOG(LogTemp, Log, TEXT("Removed module at (%d, %d, %d)"), GridCoord.X, GridCoord.Y, GridCoord.Z);
        }
    }
}

// === PROCEDURAL GENERATION ===

void AF12BuilderController::ToggleGeneratorPanel()
{
    if (GeneratorWidget)
    {
        GeneratorWidget->TogglePanel();
    }
}

bool AF12BuilderController::IsGeneratorPanelVisible() const
{
    return GeneratorWidget && GeneratorWidget->IsPanelVisible();
}

void AF12BuilderController::GenerateModules(const TArray<FF12GridCoord>& Coords)
{
    if (!InstancedRenderer || !GridSystem || Coords.Num() == 0)
        return;

    TArray<FF12GridCoord> ValidCoords;
    
    for (const FF12GridCoord& Coord : Coords)
    {
        if (!GridSystem->IsOccupied(Coord))
        {
            ValidCoords.Add(Coord);
            GridSystem->SetOccupied(Coord, nullptr);
        }
    }
    
    InstancedRenderer->AddModulesBulk(ValidCoords, CurrentPaintMaterialIndex);
    
    UE_LOG(LogTemp, Log, TEXT("Generated %d modules"), ValidCoords.Num());
}

// === HUD HELPERS ===

FLinearColor AF12BuilderController::GetCurrentPaintColor() const
{
    if (PaintColors.Num() > 0 && PaintColors.IsValidIndex(CurrentPaintMaterialIndex))
    {
        return PaintColors[CurrentPaintMaterialIndex];
    }
    
    // Fallback colors
    static const FLinearColor FallbackColors[] = {
        FLinearColor(0.3f, 1.0f, 0.3f, 1.0f),  // Green
        FLinearColor(1.0f, 0.5f, 0.8f, 1.0f),  // Pink
        FLinearColor(1.0f, 1.0f, 1.0f, 1.0f),  // White
        FLinearColor(0.3f, 0.5f, 1.0f, 1.0f),  // Blue
        FLinearColor(0.9f, 0.8f, 0.6f, 1.0f),  // Tan
    };
    
    int32 ColorIndex = CurrentPaintMaterialIndex % 5;
    return FallbackColors[ColorIndex];
}

FString AF12BuilderController::GetModeName() const
{
    switch (CurrentMode)
    {
        case EF12BuilderMode::Build:  return TEXT("BUILD");
        case EF12BuilderMode::Paint:  return TEXT("PAINT");
        case EF12BuilderMode::Delete: return TEXT("DELETE");
        default: return TEXT("UNKNOWN");
    }
}

int32 AF12BuilderController::GetModuleCount() const
{
    return InstancedRenderer ? InstancedRenderer->GetModuleCount() : 0;
}

FString AF12BuilderController::GetPerformanceStats() const
{
    return InstancedRenderer ? InstancedRenderer->GetPerformanceStats() : TEXT("No renderer");
}