// F12BuilderController.cpp
// Implementation of the builder player controller with Mode System
// Includes: Undo/Redo, Drag Build Mode, Procedural Generation

#include "F12BuilderController.h"
#include "F12Module.h"
#include "F12ProceduralGenerator.h"
#include "F12GeneratorWidget.h"
#include "ProceduralMeshComponent/Public/ProceduralMeshComponent.h"
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

    // Create action history
    ActionHistory = NewObject<UF12ActionHistory>(this, TEXT("ActionHistory"));

    // Create procedural generator
    ProceduralGenerator = NewObject<UF12ProceduralGenerator>(this, TEXT("ProceduralGenerator"));

    // Find or create the grid system
    GridSystem = Cast<AF12GridSystem>(UGameplayStatics::GetActorOfClass(GetWorld(), AF12GridSystem::StaticClass()));
    
    if (!GridSystem)
    {
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
            GhostModule->SetActorEnableCollision(false);
            // Make ghost slightly transparent - would need a ghost material
        }
    }

    // Spawn the initial "core" module at origin
    if (ModuleClass && GridSystem)
    {
        FF12GridCoord CoreCoord(0, 0, 0);
        FVector CorePos = GridSystem->GridToWorld(CoreCoord);
        
        FActorSpawnParameters SpawnParams;
        AF12Module* CoreModule = GetWorld()->SpawnActor<AF12Module>(ModuleClass, CorePos, FRotator::ZeroRotator, SpawnParams);
        
        if (CoreModule)
        {
            if (DefaultModuleMaterial)
            {
                CoreModule->TileMaterial = DefaultModuleMaterial;
            }
            
            // Copy paint materials to module
            CoreModule->TileMaterials = PaintMaterials;
            CoreModule->GenerateModule();
        }
        
        GridSystem->SetOccupied(CoreCoord, CoreModule);
        
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
            UE_LOG(LogTemp, Log, TEXT("HUD Widget created and added to viewport"));
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
            GeneratorWidget->AddToViewport(10);  // Higher Z-order to be on top
            UE_LOG(LogTemp, Log, TEXT("Generator Widget created"));
        }
    }
}

void AF12BuilderController::SetupInputComponent()
{
    Super::SetupInputComponent();

    // Primary/Secondary actions
    InputComponent->BindAction("PrimaryAction", IE_Pressed, this, &AF12BuilderController::OnPrimaryAction);
    InputComponent->BindAction("PrimaryAction", IE_Released, this, &AF12BuilderController::OnPrimaryActionReleased);
    InputComponent->BindAction("SecondaryAction", IE_Pressed, this, &AF12BuilderController::OnSecondaryAction);
    
    // Mode switching
    InputComponent->BindAction("CycleMode", IE_Pressed, this, &AF12BuilderController::OnCycleMode);
    InputComponent->BindAction("BuildMode", IE_Pressed, this, &AF12BuilderController::OnSetBuildMode);
    InputComponent->BindAction("PaintMode", IE_Pressed, this, &AF12BuilderController::OnSetPaintMode);
    InputComponent->BindAction("DeleteMode", IE_Pressed, this, &AF12BuilderController::OnSetDeleteMode);
    
    // Scroll wheel for material selection
    InputComponent->BindAction("ScrollUp", IE_Pressed, this, &AF12BuilderController::OnScrollUp);
    InputComponent->BindAction("ScrollDown", IE_Pressed, this, &AF12BuilderController::OnScrollDown);
    
    // Modifier key (Shift) for tile-level operations and drag build
    InputComponent->BindAction("Modifier", IE_Pressed, this, &AF12BuilderController::OnModifierPressed);
    InputComponent->BindAction("Modifier", IE_Released, this, &AF12BuilderController::OnModifierReleased);

    // Undo/Redo
    InputComponent->BindAction("Undo", IE_Pressed, this, &AF12BuilderController::OnUndo);
    InputComponent->BindAction("Redo", IE_Pressed, this, &AF12BuilderController::OnRedo);

    // Generator panel toggle
    InputComponent->BindAction("ToggleGenerator", IE_Pressed, this, &AF12BuilderController::OnToggleGenerator);
}

void AF12BuilderController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
    // Update drag build if active
    if (bIsDragBuilding)
    {
        UpdateDragBuild();
    }
    else
    {
        UpdateGhostPreview();
    }
    
    UpdateDeleteHighlight();
}

// === MODE SWITCHING ===

void AF12BuilderController::SetMode(EF12BuilderMode NewMode)
{
    // Cancel any drag build in progress
    if (bIsDragBuilding)
    {
        CancelDragBuild();
    }

    // Clear any existing highlight when changing modes
    ClearHighlight();
    
    CurrentMode = NewMode;
    
    // Update ghost visibility based on mode
    if (GhostModule)
    {
        GhostModule->SetActorHiddenInGame(CurrentMode != EF12BuilderMode::Build);
    }
    
    FString ModeName;
    switch (CurrentMode)
    {
        case EF12BuilderMode::Build:  ModeName = "BUILD"; break;
        case EF12BuilderMode::Paint:  ModeName = "PAINT"; break;
        case EF12BuilderMode::Delete: ModeName = "DELETE"; break;
    }
    UE_LOG(LogTemp, Log, TEXT("Mode changed to: %s"), *ModeName);
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

void AF12BuilderController::SetBuildMode()  { SetMode(EF12BuilderMode::Build); }
void AF12BuilderController::SetPaintMode()  { SetMode(EF12BuilderMode::Paint); }
void AF12BuilderController::SetDeleteMode() { SetMode(EF12BuilderMode::Delete); }

// === INPUT HANDLERS ===

void AF12BuilderController::OnPrimaryAction()   { PrimaryAction(); }
void AF12BuilderController::OnPrimaryActionReleased() { HandleBuildPrimaryRelease(); }
void AF12BuilderController::OnSecondaryAction() { SecondaryAction(); }
void AF12BuilderController::OnCycleMode()       { CycleMode(); }
void AF12BuilderController::OnSetBuildMode()    { SetBuildMode(); }
void AF12BuilderController::OnSetPaintMode()    { SetPaintMode(); }
void AF12BuilderController::OnSetDeleteMode()   { SetDeleteMode(); }

void AF12BuilderController::OnModifierPressed()  
{ 
    bModifierHeld = true; 
}

void AF12BuilderController::OnModifierReleased() 
{ 
    bModifierHeld = false;
    
    // If we were drag building, complete it
    if (bIsDragBuilding)
    {
        CancelDragBuild();
    }
}

void AF12BuilderController::OnScrollUp()
{
    if (CurrentMode == EF12BuilderMode::Paint && PaintMaterials.Num() > 0)
    {
        CurrentPaintMaterialIndex = (CurrentPaintMaterialIndex + 1) % PaintMaterials.Num();
        UE_LOG(LogTemp, Log, TEXT("Paint material: %d"), CurrentPaintMaterialIndex);
    }
}

void AF12BuilderController::OnScrollDown()
{
    if (CurrentMode == EF12BuilderMode::Paint && PaintMaterials.Num() > 0)
    {
        CurrentPaintMaterialIndex = (CurrentPaintMaterialIndex - 1 + PaintMaterials.Num()) % PaintMaterials.Num();
        UE_LOG(LogTemp, Log, TEXT("Paint material: %d"), CurrentPaintMaterialIndex);
    }
}

void AF12BuilderController::OnUndo() { Undo(); }
void AF12BuilderController::OnRedo() { Redo(); }
void AF12BuilderController::OnToggleGenerator() { ToggleGeneratorPanel(); }

// === MAIN ACTIONS ===

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

// === MODE-SPECIFIC HANDLERS ===

void AF12BuilderController::HandleBuildPrimary()
{
    // Check if we should start drag building (shift held)
    if (bModifierHeld && CurrentMode == EF12BuilderMode::Build)
    {
        // Get hit info to determine face and start drag
        FHitResult Hit;
        if (TraceFromCamera(Hit))
        {
            AF12Module* HitModule = Cast<AF12Module>(Hit.GetActor());
            if (HitModule)
            {
                FF12GridCoord HitCoord = GridSystem->WorldToGrid(HitModule->GetActorLocation());
                int32 FaceIndex = GridSystem->GetHitFaceIndex(HitCoord, Hit.Location);
                FF12GridCoord NeighborCoord = GridSystem->GetNeighborCoordForFace(HitCoord, FaceIndex);
                
                if (!GridSystem->IsOccupied(NeighborCoord))
                {
                    StartDragBuild(NeighborCoord, FaceIndex);
                    return;
                }
            }
        }
    }
    
    // Regular single module placement
    PlaceModule();
}

void AF12BuilderController::HandleBuildPrimaryRelease()
{
    // Complete drag build if active
    if (bIsDragBuilding)
    {
        CompleteDragBuild();
    }
}

void AF12BuilderController::HandleBuildSecondary()
{
    // Cancel drag build if active
    if (bIsDragBuilding)
    {
        CancelDragBuild();
        return;
    }
    
    // Remove module
    RemoveModule();
}

void AF12BuilderController::HandlePaintPrimary()
{
    int32 TileIndex = -1;
    AF12Module* Module = GetModuleUnderCursor(TileIndex);
    
    if (Module && PaintMaterials.Num() > 0)
    {
        if (bModifierHeld && TileIndex >= 0)
        {
            // Paint single tile - record for undo
            int32 OldIndex = Module->TileMaterialIndices.IsValidIndex(TileIndex) ? Module->TileMaterialIndices[TileIndex] : 0;
            
            FF12BuilderAction Action = FF12BuilderAction::CreatePaintTile(Module, TileIndex, OldIndex, CurrentPaintMaterialIndex);
            ActionHistory->AddAction(Action);
            
            Module->SetTileMaterialIndex(TileIndex, CurrentPaintMaterialIndex);
            UE_LOG(LogTemp, Log, TEXT("Painted tile %d with material %d"), TileIndex, CurrentPaintMaterialIndex);
        }
        else
        {
            // Paint entire module - record for undo
            TArray<int32> OldIndices = Module->TileMaterialIndices;
            
            FF12BuilderAction Action = FF12BuilderAction::CreatePaintModule(Module, OldIndices, CurrentPaintMaterialIndex);
            ActionHistory->AddAction(Action);
            
            for (int32 i = 0; i < 12; i++)
            {
                Module->SetTileMaterialIndex(i, CurrentPaintMaterialIndex);
            }
            UE_LOG(LogTemp, Log, TEXT("Painted entire module with material %d"), CurrentPaintMaterialIndex);
        }
    }
}

void AF12BuilderController::HandlePaintSecondary()
{
    // Cycle paint material
    if (PaintMaterials.Num() > 0)
    {
        CurrentPaintMaterialIndex = (CurrentPaintMaterialIndex + 1) % PaintMaterials.Num();
        UE_LOG(LogTemp, Log, TEXT("Cycled to paint material: %d"), CurrentPaintMaterialIndex);
    }
}

void AF12BuilderController::HandleDeletePrimary()
{
    int32 TileIndex = -1;
    AF12Module* Module = GetModuleUnderCursor(TileIndex);
    
    if (Module)
    {
        if (bModifierHeld && TileIndex >= 0)
        {
            // Delete single tile (hide it) - record for undo
            if (Module->IsTileVisible(TileIndex))
            {
                FF12BuilderAction Action = FF12BuilderAction::CreateDeleteTile(Module, TileIndex);
                ActionHistory->AddAction(Action);
                
                Module->SetTileVisible(TileIndex, false);
                UE_LOG(LogTemp, Log, TEXT("Deleted tile %d"), TileIndex);
            }
        }
        else
        {
            // Delete entire module
            FF12GridCoord Coord = GridSystem->WorldToGrid(Module->GetActorLocation());
            
            // Don't delete core module
            if (Coord.X == 0 && Coord.Y == 0 && Coord.Z == 0)
            {
                UE_LOG(LogTemp, Warning, TEXT("Cannot delete core module"));
                return;
            }
            
            // Record for undo
            FF12BuilderAction Action = FF12BuilderAction::CreateRemoveModule(Coord, Module);
            ActionHistory->AddAction(Action);
            
            GridSystem->ClearOccupied(Coord);
            Module->Destroy();
            UE_LOG(LogTemp, Log, TEXT("Deleted module"));
        }
    }
}

void AF12BuilderController::HandleDeleteSecondary()
{
    // Restore hidden tile
    int32 TileIndex = -1;
    AF12Module* Module = GetModuleUnderCursor(TileIndex);
    
    if (Module && TileIndex >= 0)
    {
        if (!Module->IsTileVisible(TileIndex))
        {
            FF12BuilderAction Action = FF12BuilderAction::CreateRestoreTile(Module, TileIndex);
            ActionHistory->AddAction(Action);
            
            Module->SetTileVisible(TileIndex, true);
            UE_LOG(LogTemp, Log, TEXT("Restored tile %d"), TileIndex);
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
        QueryParams.AddIgnoredActor(GhostModule);
        
        // Ignore drag ghost modules
        for (AF12Module* Ghost : DragGhostModules)
        {
            if (Ghost)
            {
                QueryParams.AddIgnoredActor(Ghost);
            }
        }
        
        return GetWorld()->LineTraceSingleByChannel(OutHit, TraceStart, TraceEnd, ECC_Visibility, QueryParams);
    }
    
    return false;
}

AF12Module* AF12BuilderController::GetModuleUnderCursor(int32& OutTileIndex)
{
    OutTileIndex = -1;
    
    FHitResult Hit;
    if (TraceFromCamera(Hit))
    {
        AF12Module* Module = Cast<AF12Module>(Hit.GetActor());
        if (Module)
        {
            OutTileIndex = Module->GetTileIndexFromComponent(Hit.GetComponent());
            return Module;
        }
    }
    
    return nullptr;
}

void AF12BuilderController::UpdateGhostPreview()
{
    if (!GhostModule || !GridSystem)
        return;

    // Only show ghost in Build mode when not drag building
    if (CurrentMode != EF12BuilderMode::Build || bIsDragBuilding)
    {
        GhostModule->SetActorHiddenInGame(true);
        return;
    }

    FHitResult Hit;
    
    if (TraceFromCamera(Hit))
    {
        AF12Module* HitModule = Cast<AF12Module>(Hit.GetActor());
        
        if (HitModule)
        {
            FF12GridCoord HitCoord = GridSystem->WorldToGrid(HitModule->GetActorLocation());
            int32 FaceIndex = GridSystem->GetHitFaceIndex(HitCoord, Hit.Location);
            CurrentGridCoord = GridSystem->GetNeighborCoordForFace(HitCoord, FaceIndex);
        }
        else
        {
            CurrentGridCoord = GridSystem->WorldToGrid(Hit.Location);
        }
        
        bValidPlacement = !GridSystem->IsOccupied(CurrentGridCoord);
        
        FVector GhostPos = GridSystem->GridToWorld(CurrentGridCoord);
        GhostModule->SetActorLocation(GhostPos);
        GhostModule->SetActorHiddenInGame(false);
    }
    else
    {
        GhostModule->SetActorHiddenInGame(true);
        bValidPlacement = false;
    }
}

void AF12BuilderController::PlaceModule()
{
    if (!bValidPlacement || !ModuleClass || !GridSystem)
        return;

    AF12Module* NewModule = SpawnModuleAtCoord(CurrentGridCoord);
    
    if (NewModule)
    {
        // Record for undo
        FF12BuilderAction Action = FF12BuilderAction::CreatePlaceModule(CurrentGridCoord, NewModule);
        ActionHistory->AddAction(Action);
        
        UE_LOG(LogTemp, Log, TEXT("Placed module at (%d, %d, %d)"), 
            CurrentGridCoord.X, CurrentGridCoord.Y, CurrentGridCoord.Z);
    }
}

void AF12BuilderController::RemoveModule()
{
    if (!GridSystem)
        return;

    // In build mode, remove at current grid position
    if (CurrentGridCoord.X == 0 && CurrentGridCoord.Y == 0 && CurrentGridCoord.Z == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot remove core module"));
        return;
    }

    AActor* ModuleActor = GridSystem->GetModuleAt(CurrentGridCoord);
    AF12Module* ModuleToRemove = Cast<AF12Module>(ModuleActor);
    
    if (ModuleToRemove)
    {
        // Record for undo
        FF12BuilderAction Action = FF12BuilderAction::CreateRemoveModule(CurrentGridCoord, ModuleToRemove);
        ActionHistory->AddAction(Action);
        
        GridSystem->ClearOccupied(CurrentGridCoord);
        ModuleToRemove->Destroy();
        
        UE_LOG(LogTemp, Log, TEXT("Removed module at (%d, %d, %d)"), 
            CurrentGridCoord.X, CurrentGridCoord.Y, CurrentGridCoord.Z);
    }
}

void AF12BuilderController::CyclePaintMaterial()
{
    if (PaintMaterials.Num() > 0)
    {
        CurrentPaintMaterialIndex = (CurrentPaintMaterialIndex + 1) % PaintMaterials.Num();
    }
}

// === DRAG BUILD SYSTEM ===

void AF12BuilderController::StartDragBuild(FF12GridCoord StartCoord, int32 FaceIndex)
{
    bIsDragBuilding = true;
    DragStartCoord = StartCoord;
    DragStartFaceIndex = FaceIndex;
    
    // Get the direction from the face normal
    TArray<FIntVector> Offsets = GridSystem->GetNeighborOffsets();
    if (FaceIndex >= 0 && FaceIndex < Offsets.Num())
    {
        DragDirection = Offsets[FaceIndex];
    }
    else
    {
        DragDirection = FIntVector(1, 0, 0);  // Default direction
    }
    
    // Hide the regular ghost
    if (GhostModule)
    {
        GhostModule->SetActorHiddenInGame(true);
    }
    
    // Clear any existing drag ghosts
    ClearDragGhosts();
    DragCoords.Empty();
    
    // Create initial ghost at start position
    DragCoords.Add(StartCoord);
    AF12Module* FirstGhost = CreateDragGhost();
    if (FirstGhost)
    {
        FirstGhost->SetActorLocation(GridSystem->GridToWorld(StartCoord));
    }
    
    UE_LOG(LogTemp, Log, TEXT("Started drag build at (%d, %d, %d) direction (%d, %d, %d)"),
        StartCoord.X, StartCoord.Y, StartCoord.Z,
        DragDirection.X, DragDirection.Y, DragDirection.Z);
}

void AF12BuilderController::UpdateDragBuild()
{
    if (!bIsDragBuilding || !GridSystem)
        return;

    // Get current cursor world position
    FVector WorldLocation, WorldDirection;
    if (!DeprojectMousePositionToWorld(WorldLocation, WorldDirection))
        return;

    // Calculate drag length based on cursor distance from start
    FVector StartWorldPos = GridSystem->GridToWorld(DragStartCoord);
    
    // Project cursor onto the drag direction
    FVector DragDirWorld = FVector(DragDirection.X, DragDirection.Y, DragDirection.Z).GetSafeNormal();
    float GridSpacing = GridSystem->ModuleSize * 0.707f;
    
    // Trace to find where cursor is pointing
    FHitResult Hit;
    FVector TraceStart = WorldLocation;
    FVector TraceEnd = WorldLocation + (WorldDirection * TraceDistance);
    
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(GhostModule);
    for (AF12Module* Ghost : DragGhostModules)
    {
        if (Ghost) QueryParams.AddIgnoredActor(Ghost);
    }
    
    FVector TargetPos = TraceEnd;  // Default far away
    if (GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
    {
        TargetPos = Hit.Location;
    }
    else
    {
        // If we didn't hit anything, project onto a plane at the start position
        FPlane DragPlane(StartWorldPos, FVector(0, 0, 1));  // Horizontal plane
        FVector IntersectionPoint;
        if (FMath::SegmentPlaneIntersection(TraceStart, TraceEnd, DragPlane, IntersectionPoint))
        {
            TargetPos = IntersectionPoint;
        }
    }
    
    // Calculate how many modules fit in the drag direction
    FVector Delta = TargetPos - StartWorldPos;
    float ProjectedDist = FVector::DotProduct(Delta, DragDirWorld);
    int32 NumModules = FMath::Max(1, FMath::RoundToInt(ProjectedDist / GridSpacing) + 1);
    NumModules = FMath::Min(NumModules, MaxDragModules);
    
    // Build new coord list
    TArray<FF12GridCoord> NewCoords;
    FF12GridCoord CurrentCoord = DragStartCoord;
    
    for (int32 i = 0; i < NumModules; i++)
    {
        if (!GridSystem->IsOccupied(CurrentCoord))
        {
            NewCoords.Add(CurrentCoord);
        }
        
        // Move to next position in drag direction
        CurrentCoord = FF12GridCoord(
            CurrentCoord.X + DragDirection.X,
            CurrentCoord.Y + DragDirection.Y,
            CurrentCoord.Z + DragDirection.Z
        );
    }
    
    // Only update ghosts if coords changed
    if (NewCoords != DragCoords)
    {
        DragCoords = NewCoords;
        
        // Adjust ghost count
        while (DragGhostModules.Num() < DragCoords.Num())
        {
            CreateDragGhost();
        }
        while (DragGhostModules.Num() > DragCoords.Num())
        {
            AF12Module* Ghost = DragGhostModules.Pop();
            if (Ghost) Ghost->Destroy();
        }
        
        // Position ghosts
        for (int32 i = 0; i < DragCoords.Num() && i < DragGhostModules.Num(); i++)
        {
            if (DragGhostModules[i])
            {
                DragGhostModules[i]->SetActorLocation(GridSystem->GridToWorld(DragCoords[i]));
                DragGhostModules[i]->SetActorHiddenInGame(false);
            }
        }
    }
}

void AF12BuilderController::CompleteDragBuild()
{
    if (!bIsDragBuilding || DragCoords.Num() == 0)
    {
        CancelDragBuild();
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("Completing drag build with %d modules"), DragCoords.Num());

    // Place all modules
    TArray<FF12GridCoord> PlacedCoords;
    for (const FF12GridCoord& Coord : DragCoords)
    {
        if (!GridSystem->IsOccupied(Coord))
        {
            AF12Module* NewModule = SpawnModuleAtCoord(Coord);
            if (NewModule)
            {
                PlacedCoords.Add(Coord);
            }
        }
    }

    // Record as a single action for undo
    if (PlacedCoords.Num() > 0)
    {
        FF12BuilderAction Action = FF12BuilderAction::CreatePlaceMultiple(PlacedCoords);
        ActionHistory->AddAction(Action);
    }

    // Clean up
    ClearDragGhosts();
    DragCoords.Empty();
    bIsDragBuilding = false;

    // Show regular ghost again
    if (GhostModule)
    {
        GhostModule->SetActorHiddenInGame(false);
    }
}

void AF12BuilderController::CancelDragBuild()
{
    ClearDragGhosts();
    DragCoords.Empty();
    bIsDragBuilding = false;

    // Show regular ghost again
    if (GhostModule && CurrentMode == EF12BuilderMode::Build)
    {
        GhostModule->SetActorHiddenInGame(false);
    }

    UE_LOG(LogTemp, Log, TEXT("Drag build cancelled"));
}

AF12Module* AF12BuilderController::CreateDragGhost()
{
    if (!ModuleClass)
        return nullptr;

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    
    AF12Module* Ghost = GetWorld()->SpawnActor<AF12Module>(ModuleClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
    
    if (Ghost)
    {
        Ghost->SetActorEnableCollision(false);
        
        // Apply ghost material if set
        if (DragGhostMaterial)
        {
            Ghost->TileMaterial = DragGhostMaterial;
        }
        else if (DefaultModuleMaterial)
        {
            Ghost->TileMaterial = DefaultModuleMaterial;
        }
        
        Ghost->TileMaterials = PaintMaterials;
        Ghost->GenerateModule();
        
        DragGhostModules.Add(Ghost);
    }
    
    return Ghost;
}

void AF12BuilderController::ClearDragGhosts()
{
    for (AF12Module* Ghost : DragGhostModules)
    {
        if (Ghost)
        {
            Ghost->Destroy();
        }
    }
    DragGhostModules.Empty();
}

// === DELETE HIGHLIGHT SYSTEM ===

void AF12BuilderController::UpdateDeleteHighlight()
{
    // Only show highlight in delete mode
    if (CurrentMode != EF12BuilderMode::Delete)
    {
        ClearHighlight();
        return;
    }

    int32 TileIndex = -1;
    AF12Module* Module = GetModuleUnderCursor(TileIndex);

    // Check if we're hovering over something new
    bool bSameTarget = (Module == HighlightedModule);
    if (bModifierHeld)
    {
        bSameTarget = bSameTarget && (TileIndex == HighlightedTileIndex);
    }

    if (!bSameTarget)
    {
        // Clear old highlight
        ClearHighlight();

        // Apply new highlight
        if (Module && DeleteHighlightMaterial)
        {
            HighlightedModule = Module;
            HighlightedTileIndex = TileIndex;
            HighlightedOriginalMaterials.Empty();

            if (bModifierHeld && TileIndex >= 0)
            {
                // Highlight single tile
                if (Module->TileMeshes.IsValidIndex(TileIndex) && Module->TileMeshes[TileIndex])
                {
                    HighlightedOriginalMaterials.Add(Module->TileMeshes[TileIndex]->GetMaterial(0));
                    Module->TileMeshes[TileIndex]->SetMaterial(0, DeleteHighlightMaterial);
                }
            }
            else
            {
                // Highlight entire module
                for (int32 i = 0; i < Module->TileMeshes.Num(); i++)
                {
                    if (Module->TileMeshes[i])
                    {
                        HighlightedOriginalMaterials.Add(Module->TileMeshes[i]->GetMaterial(0));
                        Module->TileMeshes[i]->SetMaterial(0, DeleteHighlightMaterial);
                    }
                }
            }
        }
    }
}

void AF12BuilderController::ClearHighlight()
{
    if (HighlightedModule && HighlightedOriginalMaterials.Num() > 0)
    {
        if (HighlightedTileIndex >= 0 && HighlightedOriginalMaterials.Num() == 1)
        {
            // Was highlighting single tile
            if (HighlightedModule->TileMeshes.IsValidIndex(HighlightedTileIndex) && 
                HighlightedModule->TileMeshes[HighlightedTileIndex])
            {
                HighlightedModule->TileMeshes[HighlightedTileIndex]->SetMaterial(0, HighlightedOriginalMaterials[0]);
            }
        }
        else
        {
            // Was highlighting entire module
            for (int32 i = 0; i < HighlightedModule->TileMeshes.Num() && i < HighlightedOriginalMaterials.Num(); i++)
            {
                if (HighlightedModule->TileMeshes[i])
                {
                    HighlightedModule->TileMeshes[i]->SetMaterial(0, HighlightedOriginalMaterials[i]);
                }
            }
        }
    }

    HighlightedModule = nullptr;
    HighlightedTileIndex = -1;
    HighlightedOriginalMaterials.Empty();
}

// === UNDO/REDO SYSTEM ===

void AF12BuilderController::Undo()
{
    if (!ActionHistory || !ActionHistory->CanUndo())
    {
        UE_LOG(LogTemp, Log, TEXT("Nothing to undo"));
        return;
    }

    FF12BuilderAction Action = ActionHistory->PopForUndo();
    ExecuteUndo(Action);
    
    UE_LOG(LogTemp, Log, TEXT("Undo performed. Remaining: %d undo, %d redo"), 
        ActionHistory->GetUndoCount(), ActionHistory->GetRedoCount());
}

void AF12BuilderController::Redo()
{
    if (!ActionHistory || !ActionHistory->CanRedo())
    {
        UE_LOG(LogTemp, Log, TEXT("Nothing to redo"));
        return;
    }

    FF12BuilderAction Action = ActionHistory->PopForRedo();
    ExecuteRedo(Action);
    
    UE_LOG(LogTemp, Log, TEXT("Redo performed. Remaining: %d undo, %d redo"), 
        ActionHistory->GetUndoCount(), ActionHistory->GetRedoCount());
}

bool AF12BuilderController::CanUndo() const
{
    return ActionHistory && ActionHistory->CanUndo();
}

bool AF12BuilderController::CanRedo() const
{
    return ActionHistory && ActionHistory->CanRedo();
}

void AF12BuilderController::ExecuteUndo(const FF12BuilderAction& Action)
{
    switch (Action.ActionType)
    {
        case EF12ActionType::PlaceModule:
        {
            // Undo place = remove
            RemoveModuleAtCoord(Action.GridCoord);
            break;
        }
        
        case EF12ActionType::RemoveModule:
        {
            // Undo remove = place with stored state
            AF12Module* Module = SpawnModuleAtCoord(Action.GridCoord, 
                &Action.StoredMaterialIndices, &Action.StoredVisibility);
            break;
        }
        
        case EF12ActionType::PaintTile:
        {
            AF12Module* Module = Action.AffectedModule.Get();
            if (Module)
            {
                Module->SetTileMaterialIndex(Action.TileIndex, Action.OldMaterialIndex);
            }
            break;
        }
        
        case EF12ActionType::PaintModule:
        {
            AF12Module* Module = Action.AffectedModule.Get();
            if (Module)
            {
                for (int32 i = 0; i < 12 && i < Action.StoredMaterialIndices.Num(); i++)
                {
                    Module->SetTileMaterialIndex(i, Action.StoredMaterialIndices[i]);
                }
            }
            break;
        }
        
        case EF12ActionType::DeleteTile:
        {
            AF12Module* Module = Action.AffectedModule.Get();
            if (Module)
            {
                Module->SetTileVisible(Action.TileIndex, true);  // Restore visibility
            }
            break;
        }
        
        case EF12ActionType::RestoreTile:
        {
            AF12Module* Module = Action.AffectedModule.Get();
            if (Module)
            {
                Module->SetTileVisible(Action.TileIndex, false);  // Hide again
            }
            break;
        }
        
        case EF12ActionType::PlaceMultipleModules:
        {
            // Undo multiple place = remove all
            for (const FF12GridCoord& Coord : Action.MultipleCoords)
            {
                RemoveModuleAtCoord(Coord);
            }
            break;
        }
    }
}

void AF12BuilderController::ExecuteRedo(const FF12BuilderAction& Action)
{
    switch (Action.ActionType)
    {
        case EF12ActionType::PlaceModule:
        {
            // Redo place = place again
            SpawnModuleAtCoord(Action.GridCoord);
            break;
        }
        
        case EF12ActionType::RemoveModule:
        {
            // Redo remove = remove again
            RemoveModuleAtCoord(Action.GridCoord);
            break;
        }
        
        case EF12ActionType::PaintTile:
        {
            AF12Module* Module = Action.AffectedModule.Get();
            if (Module)
            {
                Module->SetTileMaterialIndex(Action.TileIndex, Action.NewMaterialIndex);
            }
            break;
        }
        
        case EF12ActionType::PaintModule:
        {
            AF12Module* Module = Action.AffectedModule.Get();
            if (Module)
            {
                for (int32 i = 0; i < 12; i++)
                {
                    Module->SetTileMaterialIndex(i, Action.NewMaterialIndex);
                }
            }
            break;
        }
        
        case EF12ActionType::DeleteTile:
        {
            AF12Module* Module = Action.AffectedModule.Get();
            if (Module)
            {
                Module->SetTileVisible(Action.TileIndex, false);  // Hide again
            }
            break;
        }
        
        case EF12ActionType::RestoreTile:
        {
            AF12Module* Module = Action.AffectedModule.Get();
            if (Module)
            {
                Module->SetTileVisible(Action.TileIndex, true);  // Restore again
            }
            break;
        }
        
        case EF12ActionType::PlaceMultipleModules:
        {
            // Redo multiple place = place all again
            for (const FF12GridCoord& Coord : Action.MultipleCoords)
            {
                SpawnModuleAtCoord(Coord);
            }
            break;
        }
    }
}

AF12Module* AF12BuilderController::SpawnModuleAtCoord(FF12GridCoord Coord, 
    const TArray<int32>* MaterialIndices, const TArray<bool>* Visibility)
{
    if (!ModuleClass || !GridSystem || GridSystem->IsOccupied(Coord))
        return nullptr;

    FVector SpawnPos = GridSystem->GridToWorld(Coord);
    
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    
    AF12Module* NewModule = GetWorld()->SpawnActor<AF12Module>(ModuleClass, SpawnPos, FRotator::ZeroRotator, SpawnParams);
    
    if (NewModule)
    {
        if (DefaultModuleMaterial)
        {
            NewModule->TileMaterial = DefaultModuleMaterial;
        }
        
        NewModule->TileMaterials = PaintMaterials;
        NewModule->GenerateModule();
        
        // Restore material indices if provided
        if (MaterialIndices && MaterialIndices->Num() == 12)
        {
            for (int32 i = 0; i < 12; i++)
            {
                NewModule->SetTileMaterialIndex(i, (*MaterialIndices)[i]);
            }
        }
        
        // Restore visibility if provided
        if (Visibility && Visibility->Num() == 12)
        {
            for (int32 i = 0; i < 12; i++)
            {
                NewModule->SetTileVisible(i, (*Visibility)[i]);
            }
        }
        
        GridSystem->SetOccupied(Coord, NewModule);
    }
    
    return NewModule;
}

void AF12BuilderController::RemoveModuleAtCoord(FF12GridCoord Coord)
{
    if (!GridSystem)
        return;

    // Don't remove core module
    if (Coord.X == 0 && Coord.Y == 0 && Coord.Z == 0)
        return;

    AActor* ModuleActor = GridSystem->GetModuleAt(Coord);
    if (ModuleActor)
    {
        GridSystem->ClearOccupied(Coord);
        ModuleActor->Destroy();
    }
}

// === HUD HELPERS ===

FLinearColor AF12BuilderController::GetCurrentPaintColor() const
{
    if (PaintColors.Num() > 0 && PaintColors.IsValidIndex(CurrentPaintMaterialIndex))
    {
        return PaintColors[CurrentPaintMaterialIndex];
    }
    
    // Fallback: If no PaintColors set, generate a color based on index
    static const FLinearColor FallbackColors[] = {
        FLinearColor(1.0f, 0.3f, 0.3f, 1.0f),  // Red
        FLinearColor(0.3f, 1.0f, 0.3f, 1.0f),  // Green
        FLinearColor(0.3f, 0.3f, 1.0f, 1.0f),  // Blue
        FLinearColor(1.0f, 1.0f, 0.3f, 1.0f),  // Yellow
        FLinearColor(1.0f, 0.3f, 1.0f, 1.0f),  // Magenta
        FLinearColor(0.3f, 1.0f, 1.0f, 1.0f),  // Cyan
    };
    
    int32 ColorIndex = CurrentPaintMaterialIndex % 6;
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

int32 AF12BuilderController::GetUndoCount() const
{
    return ActionHistory ? ActionHistory->GetUndoCount() : 0;
}

int32 AF12BuilderController::GetRedoCount() const
{
    return ActionHistory ? ActionHistory->GetRedoCount() : 0;
}

// === GENERATOR PANEL ===

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
