// F12BuilderController.cpp
// Implementation of the builder player controller
// Updated with Undo/Redo and Drag-Build functionality

#include "F12BuilderController.h"
#include "F12Module.h"
#include "F12CommandSystem.h"
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

    // Create command history
    CommandHistory = NewObject<UF12CommandHistory>(this);

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
            if (DefaultModuleMaterial)
            {
                GhostModule->TileMaterial = DefaultModuleMaterial;
            }
            GhostModule->TileMaterials = PaintMaterials;
            GhostModule->GenerateModule();
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
            CoreModule->TileMaterials = PaintMaterials;
            CoreModule->GenerateModule();
        }
        
        GridSystem->SetOccupied(CoreCoord, CoreModule);
        UE_LOG(LogTemp, Log, TEXT("Spawned core module at origin"));
    }

    SetMode(EF12BuilderMode::Build);

    // Spawn the HUD widget
    if (HUDWidgetClass)
    {
        UUserWidget* HUDWidget = CreateWidget<UUserWidget>(this, HUDWidgetClass);
        if (HUDWidget)
        {
            HUDWidget->AddToViewport();
        }
    }
}

void AF12BuilderController::SetupInputComponent()
{
    Super::SetupInputComponent();

    // Primary/Secondary actions - need both pressed and released for drag
    InputComponent->BindAction("PrimaryAction", IE_Pressed, this, &AF12BuilderController::OnPrimaryAction);
    InputComponent->BindAction("PrimaryAction", IE_Released, this, &AF12BuilderController::OnPrimaryActionReleased);
    InputComponent->BindAction("SecondaryAction", IE_Pressed, this, &AF12BuilderController::OnSecondaryAction);
    
    // Mode switching
    InputComponent->BindAction("CycleMode", IE_Pressed, this, &AF12BuilderController::OnCycleMode);
    InputComponent->BindAction("BuildMode", IE_Pressed, this, &AF12BuilderController::OnSetBuildMode);
    InputComponent->BindAction("PaintMode", IE_Pressed, this, &AF12BuilderController::OnSetPaintMode);
    InputComponent->BindAction("DeleteMode", IE_Pressed, this, &AF12BuilderController::OnSetDeleteMode);
    
    // Scroll wheel
    InputComponent->BindAction("ScrollUp", IE_Pressed, this, &AF12BuilderController::OnScrollUp);
    InputComponent->BindAction("ScrollDown", IE_Pressed, this, &AF12BuilderController::OnScrollDown);
    
    // Modifier key (Shift)
    InputComponent->BindAction("Modifier", IE_Pressed, this, &AF12BuilderController::OnModifierPressed);
    InputComponent->BindAction("Modifier", IE_Released, this, &AF12BuilderController::OnModifierReleased);

    // Undo/Redo (Ctrl+Z, Ctrl+Y handled via action mappings)
    InputComponent->BindAction("Undo", IE_Pressed, this, &AF12BuilderController::OnUndo);
    InputComponent->BindAction("Redo", IE_Pressed, this, &AF12BuilderController::OnRedo);
}

void AF12BuilderController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
    // Update drag preview if dragging
    if (bIsDragging)
    {
        UpdateDrag();
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
    ClearHighlight();
    CancelDrag();
    
    CurrentMode = NewMode;
    
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

void AF12BuilderController::OnPrimaryAction()
{
    bPrimaryHeld = true;
    PrimaryAction();
}

void AF12BuilderController::OnPrimaryActionReleased()
{
    bPrimaryHeld = false;
    
    if (CurrentMode == EF12BuilderMode::Build)
    {
        HandleBuildPrimaryRelease();
    }
}

void AF12BuilderController::OnSecondaryAction() { SecondaryAction(); }
void AF12BuilderController::OnCycleMode()       { CycleMode(); }
void AF12BuilderController::OnSetBuildMode()    { SetBuildMode(); }
void AF12BuilderController::OnSetPaintMode()    { SetPaintMode(); }
void AF12BuilderController::OnSetDeleteMode()   { SetDeleteMode(); }

void AF12BuilderController::OnModifierPressed()  
{ 
    bModifierHeld = true;
    
    // If in build mode with primary held, start drag
    if (CurrentMode == EF12BuilderMode::Build && bPrimaryHeld && !bIsDragging)
    {
        StartDrag();
    }
}

void AF12BuilderController::OnModifierReleased() 
{ 
    bModifierHeld = false;
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

// === UNDO/REDO ===

void AF12BuilderController::Undo()
{
    if (CommandHistory && CommandHistory->CanUndo())
    {
        CommandHistory->Undo(this);
        UE_LOG(LogTemp, Log, TEXT("Undo performed"));
    }
}

void AF12BuilderController::Redo()
{
    if (CommandHistory && CommandHistory->CanRedo())
    {
        CommandHistory->Redo(this);
        UE_LOG(LogTemp, Log, TEXT("Redo performed"));
    }
}

bool AF12BuilderController::CanUndo() const
{
    return CommandHistory && CommandHistory->CanUndo();
}

bool AF12BuilderController::CanRedo() const
{
    return CommandHistory && CommandHistory->CanRedo();
}

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
    // If shift is held, start drag mode
    if (bModifierHeld)
    {
        StartDrag();
    }
    else
    {
        // Single placement
        if (bValidPlacement)
        {
            ExecutePlaceModule(CurrentGridCoord);
        }
    }
}

void AF12BuilderController::HandleBuildPrimaryRelease()
{
    // End drag if active
    if (bIsDragging)
    {
        EndDrag();
    }
}

void AF12BuilderController::HandleBuildSecondary()
{
    // Cancel drag if active
    if (bIsDragging)
    {
        CancelDrag();
        return;
    }
    
    // Otherwise, remove module
    if (CurrentGridCoord.X == 0 && CurrentGridCoord.Y == 0 && CurrentGridCoord.Z == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot remove core module"));
        return;
    }
    
    ExecuteDeleteModule(CurrentGridCoord);
}

void AF12BuilderController::HandlePaintPrimary()
{
    int32 TileIndex = -1;
    AF12Module* Module = GetModuleUnderCursor(TileIndex);
    
    if (Module && PaintMaterials.Num() > 0)
    {
        if (bModifierHeld && TileIndex >= 0)
        {
            ExecutePaintTile(Module, TileIndex, CurrentPaintMaterialIndex);
        }
        else
        {
            ExecutePaintModule(Module, CurrentPaintMaterialIndex);
        }
    }
}

void AF12BuilderController::HandlePaintSecondary()
{
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
            ExecuteDeleteTile(Module, TileIndex);
        }
        else
        {
            FF12GridCoord Coord = GridSystem->WorldToGrid(Module->GetActorLocation());
            
            if (Coord.X == 0 && Coord.Y == 0 && Coord.Z == 0)
            {
                UE_LOG(LogTemp, Warning, TEXT("Cannot delete core module"));
                return;
            }
            
            ExecuteDeleteModule(Coord);
        }
    }
}

void AF12BuilderController::HandleDeleteSecondary()
{
    int32 TileIndex = -1;
    AF12Module* Module = GetModuleUnderCursor(TileIndex);
    
    if (Module && TileIndex >= 0)
    {
        ExecuteRestoreTile(Module, TileIndex);
    }
}

// === DRAG BUILD SYSTEM ===

void AF12BuilderController::StartDrag()
{
    if (!bValidPlacement || bIsDragging || !GridSystem)
    {
        UE_LOG(LogTemp, Warning, TEXT("StartDrag blocked: bValidPlacement=%d, bIsDragging=%d"), 
            bValidPlacement, bIsDragging);
        return;
    }

    // Do a raycast to find which face we're clicking on
    FHitResult Hit;
    if (!TraceFromCamera(Hit))
    {
        UE_LOG(LogTemp, Warning, TEXT("StartDrag: No hit"));
        return;
    }
    
    AF12Module* HitModule = Cast<AF12Module>(Hit.GetActor());
    if (!HitModule)
    {
        UE_LOG(LogTemp, Warning, TEXT("StartDrag: Hit was not a module"));
        return;
    }
    
    // Get the face we clicked on
    FF12GridCoord HitCoord = GridSystem->WorldToGrid(HitModule->GetActorLocation());
    DragFaceIndex = GridSystem->GetHitFaceIndex(HitCoord, Hit.Location);
    
    if (DragFaceIndex < 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("StartDrag: Invalid face index"));
        return;
    }

    bIsDragging = true;
    DragStartCoord = CurrentGridCoord;  // This is already the neighbor coord for that face
    DragCoords.Empty();
    
    // Store the starting world position for plane tracking
    DragStartWorldPos = GridSystem->GridToWorld(DragStartCoord);
    
    // Get camera direction for the plane normal
    FVector CamLoc, CamDir;
    DeprojectMousePositionToWorld(CamLoc, CamDir);
    DragPlaneNormal = -CamDir;
    
    UE_LOG(LogTemp, Log, TEXT("Started drag at (%d, %d, %d) from face %d"), 
        DragStartCoord.X, DragStartCoord.Y, DragStartCoord.Z, DragFaceIndex);
}

void AF12BuilderController::UpdateDrag()
{
    if (!bIsDragging || !GridSystem || !ModuleClass || DragFaceIndex < 0)
        return;

    // Get mouse ray
    FVector CamLoc, CamDir;
    if (!DeprojectMousePositionToWorld(CamLoc, CamDir))
    {
        return;
    }

    // Intersect ray with drag plane to get mouse world position
    FVector MouseWorldPos = DragStartWorldPos;
    float Denom = FVector::DotProduct(CamDir, DragPlaneNormal);
    if (FMath::Abs(Denom) > 0.0001f)
    {
        float t = FVector::DotProduct(DragStartWorldPos - CamLoc, DragPlaneNormal) / Denom;
        if (t > 0)
        {
            MouseWorldPos = CamLoc + CamDir * t;
        }
    }

    // Calculate distance from start position to mouse position
    float Distance = FVector::Dist(MouseWorldPos, DragStartWorldPos);
    
    // Calculate how many modules that distance represents
    // Use module size as the spacing (from PROJECT_SUMMARY: ModuleSize = 400)
    float ModuleSpacing = 400.0f;  // This should ideally come from GridSystem
    int32 NumModules = FMath::Max(1, FMath::RoundToInt(Distance / ModuleSpacing) + 1);
    NumModules = FMath::Min(NumModules, MaxDragModules);

    // Build the line of coords by repeatedly getting the neighbor in the drag direction
    TArray<FF12GridCoord> NewDragCoords;
    FF12GridCoord CurrentCoord = DragStartCoord;
    NewDragCoords.Add(CurrentCoord);
    
    for (int32 i = 1; i < NumModules; i++)
    {
        CurrentCoord = GridSystem->GetNeighborCoordForFace(CurrentCoord, DragFaceIndex);
        NewDragCoords.Add(CurrentCoord);
    }

    // Check if coords changed
    bool bCoordsChanged = NewDragCoords.Num() != DragCoords.Num();
    if (!bCoordsChanged)
    {
        for (int32 i = 0; i < NewDragCoords.Num(); i++)
        {
            if (!(NewDragCoords[i] == DragCoords[i]))
            {
                bCoordsChanged = true;
                break;
            }
        }
    }

    if (bCoordsChanged)
    {
        DragCoords = NewDragCoords;
        
        // Update preview modules
        ClearDragPreview();
        
        int32 PreviewsCreated = 0;
        for (const FF12GridCoord& Coord : DragCoords)
        {
            if (!GridSystem->IsOccupied(Coord))
            {
                FVector Pos = GridSystem->GridToWorld(Coord);
                
                FActorSpawnParameters SpawnParams;
                SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
                
                AF12Module* Preview = GetWorld()->SpawnActor<AF12Module>(ModuleClass, Pos, FRotator::ZeroRotator, SpawnParams);
                if (Preview)
                {
                    Preview->SetActorEnableCollision(false);
                    if (DefaultModuleMaterial)
                    {
                        Preview->TileMaterial = DefaultModuleMaterial;
                    }
                    Preview->TileMaterials = PaintMaterials;
                    Preview->GenerateModule();
                    DragPreviewModules.Add(Preview);
                    PreviewsCreated++;
                }
            }
        }
        
        UE_LOG(LogTemp, Log, TEXT("Drag: %d modules along face %d (dist=%.0f)"), 
            DragCoords.Num(), DragFaceIndex, Distance);
    }
}

void AF12BuilderController::EndDrag()
{
    if (!bIsDragging)
        return;

    bIsDragging = false;
    DragFaceIndex = -1;  // Reset face direction

    // Filter out occupied coords
    TArray<FF12GridCoord> ValidCoords;
    for (const FF12GridCoord& Coord : DragCoords)
    {
        if (!GridSystem->IsOccupied(Coord))
        {
            ValidCoords.Add(Coord);
        }
    }

    // Clear preview
    ClearDragPreview();

    // Place all modules
    if (ValidCoords.Num() > 0)
    {
        ExecutePlaceMultiple(ValidCoords);
    }

    DragCoords.Empty();
    
    UE_LOG(LogTemp, Log, TEXT("Ended drag, placed %d modules"), ValidCoords.Num());
}

void AF12BuilderController::CancelDrag()
{
    if (!bIsDragging)
        return;

    bIsDragging = false;
    DragFaceIndex = -1;  // Reset face direction
    ClearDragPreview();
    DragCoords.Empty();
    
    UE_LOG(LogTemp, Log, TEXT("Cancelled drag"));
}

void AF12BuilderController::ClearDragPreview()
{
    for (AF12Module* Preview : DragPreviewModules)
    {
        if (Preview)
        {
            Preview->Destroy();
        }
    }
    DragPreviewModules.Empty();
}

TArray<FF12GridCoord> AF12BuilderController::CalculateDragLine(const FF12GridCoord& Start, const FF12GridCoord& End)
{
    TArray<FF12GridCoord> Line;
    
    // Use 3D Bresenham-like algorithm
    int32 dx = FMath::Abs(End.X - Start.X);
    int32 dy = FMath::Abs(End.Y - Start.Y);
    int32 dz = FMath::Abs(End.Z - Start.Z);
    
    int32 sx = (Start.X < End.X) ? 1 : -1;
    int32 sy = (Start.Y < End.Y) ? 1 : -1;
    int32 sz = (Start.Z < End.Z) ? 1 : -1;
    
    int32 dm = FMath::Max3(dx, dy, dz);
    
    FF12GridCoord Current = Start;
    Line.Add(Current);
    
    if (dm == 0)
        return Line;

    // Step along the longest axis
    int32 x_inc = dx;
    int32 y_inc = dy;
    int32 z_inc = dz;
    
    int32 x_err = dm / 2;
    int32 y_err = dm / 2;
    int32 z_err = dm / 2;
    
    for (int32 i = 0; i < dm; i++)
    {
        x_err += dx;
        y_err += dy;
        z_err += dz;
        
        if (x_err >= dm)
        {
            Current.X += sx;
            x_err -= dm;
        }
        if (y_err >= dm)
        {
            Current.Y += sy;
            y_err -= dm;
        }
        if (z_err >= dm)
        {
            Current.Z += sz;
            z_err -= dm;
        }
        
        Line.Add(Current);
    }
    
    return Line;
}

// === COMMAND EXECUTION HELPERS ===

void AF12BuilderController::ExecutePlaceModule(const FF12GridCoord& Coord)
{
    if (!GridSystem || !ModuleClass || GridSystem->IsOccupied(Coord))
        return;

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
        
        GridSystem->SetOccupied(Coord, NewModule);
        
        // Create command for history
        FF12Command Cmd;
        Cmd.CommandType = EF12CommandType::PlaceModule;
        Cmd.GridCoords.Add(Coord);
        Cmd.ModuleClass = ModuleClass;
        
        if (CommandHistory)
        {
            CommandHistory->ExecuteCommand(Cmd, this);
        }
        
        UE_LOG(LogTemp, Log, TEXT("Placed module at (%d, %d, %d)"), Coord.X, Coord.Y, Coord.Z);
    }
}

void AF12BuilderController::ExecutePlaceMultiple(const TArray<FF12GridCoord>& Coords)
{
    if (!GridSystem || !ModuleClass || Coords.Num() == 0)
        return;

    TArray<FF12GridCoord> PlacedCoords;
    
    for (const FF12GridCoord& Coord : Coords)
    {
        if (GridSystem->IsOccupied(Coord))
            continue;

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
            
            GridSystem->SetOccupied(Coord, NewModule);
            PlacedCoords.Add(Coord);
        }
    }

    // Create single command for all placements
    if (PlacedCoords.Num() > 0)
    {
        FF12Command Cmd;
        Cmd.CommandType = EF12CommandType::PlaceMultiple;
        Cmd.GridCoords = PlacedCoords;
        Cmd.ModuleClass = ModuleClass;
        
        if (CommandHistory)
        {
            CommandHistory->ExecuteCommand(Cmd, this);
        }
        
        UE_LOG(LogTemp, Log, TEXT("Placed %d modules in drag operation"), PlacedCoords.Num());
    }
}

void AF12BuilderController::ExecuteDeleteModule(const FF12GridCoord& Coord)
{
    if (!GridSystem)
        return;

    AActor* ModuleActor = GridSystem->GetModuleAt(Coord);
    AF12Module* Module = Cast<AF12Module>(ModuleActor);
    
    if (!Module)
        return;

    // Store state for undo
    FF12Command Cmd;
    Cmd.CommandType = EF12CommandType::DeleteModule;
    Cmd.GridCoords.Add(Coord);
    Cmd.ModuleClass = ModuleClass;
    
    // Store material indices
    for (int32 i = 0; i < 12; i++)
    {
        Cmd.ModuleMaterialIndices.Add(Module->TileMaterialIndices.IsValidIndex(i) ? Module->TileMaterialIndices[i] : 0);
        Cmd.ModuleTileVisibility.Add(Module->IsTileVisible(i));
    }
    
    // Delete
    GridSystem->ClearOccupied(Coord);
    Module->Destroy();
    
    if (CommandHistory)
    {
        CommandHistory->ExecuteCommand(Cmd, this);
    }
    
    UE_LOG(LogTemp, Log, TEXT("Deleted module at (%d, %d, %d)"), Coord.X, Coord.Y, Coord.Z);
}

void AF12BuilderController::ExecutePaintTile(AF12Module* Module, int32 TileIndex, int32 NewMaterialIndex)
{
    if (!Module || !GridSystem || TileIndex < 0 || TileIndex >= 12)
        return;

    FF12GridCoord Coord = GridSystem->WorldToGrid(Module->GetActorLocation());
    
    // Store previous state - get the ACTUAL material currently on the mesh
    FF12Command Cmd;
    Cmd.CommandType = EF12CommandType::PaintTile;
    Cmd.GridCoords.Add(Coord);
    Cmd.TileIndex = TileIndex;
    Cmd.NewMaterialIndex = NewMaterialIndex;
    
    // Store the actual material pointer from the mesh (not the index)
    if (Module->TileMeshes.IsValidIndex(TileIndex) && Module->TileMeshes[TileIndex])
    {
        Cmd.PreviousMaterials.Add(Module->TileMeshes[TileIndex]->GetMaterial(0));
    }
    else
    {
        Cmd.PreviousMaterials.Add(nullptr);
    }
    
    // Apply paint
    Module->SetTileMaterialIndex(TileIndex, NewMaterialIndex);
    
    if (CommandHistory)
    {
        CommandHistory->ExecuteCommand(Cmd, this);
    }
    
    UE_LOG(LogTemp, Log, TEXT("Painted tile %d with material %d"), TileIndex, NewMaterialIndex);
}

void AF12BuilderController::ExecutePaintModule(AF12Module* Module, int32 NewMaterialIndex)
{
    if (!Module || !GridSystem)
        return;

    FF12GridCoord Coord = GridSystem->WorldToGrid(Module->GetActorLocation());
    
    // Store previous state
    FF12Command Cmd;
    Cmd.CommandType = EF12CommandType::PaintModule;
    Cmd.GridCoords.Add(Coord);
    Cmd.TileIndex = -1;  // -1 indicates whole module
    Cmd.NewMaterialIndex = NewMaterialIndex;
    
    // Store the ACTUAL material pointers from each mesh (not indices)
    for (int32 i = 0; i < 12; i++)
    {
        if (Module->TileMeshes.IsValidIndex(i) && Module->TileMeshes[i])
        {
            Cmd.PreviousMaterials.Add(Module->TileMeshes[i]->GetMaterial(0));
        }
        else
        {
            Cmd.PreviousMaterials.Add(nullptr);
        }
    }
    
    // Apply paint
    for (int32 i = 0; i < 12; i++)
    {
        Module->SetTileMaterialIndex(i, NewMaterialIndex);
    }
    
    if (CommandHistory)
    {
        CommandHistory->ExecuteCommand(Cmd, this);
    }
    
    UE_LOG(LogTemp, Log, TEXT("Painted entire module with material %d"), NewMaterialIndex);
}

void AF12BuilderController::ExecuteDeleteTile(AF12Module* Module, int32 TileIndex)
{
    if (!Module || !GridSystem || TileIndex < 0 || TileIndex >= 12)
        return;

    FF12GridCoord Coord = GridSystem->WorldToGrid(Module->GetActorLocation());
    
    FF12Command Cmd;
    Cmd.CommandType = EF12CommandType::DeleteTile;
    Cmd.GridCoords.Add(Coord);
    Cmd.TileIndex = TileIndex;
    Cmd.bPreviousVisibility = Module->IsTileVisible(TileIndex);
    
    Module->SetTileVisible(TileIndex, false);
    
    if (CommandHistory)
    {
        CommandHistory->ExecuteCommand(Cmd, this);
    }
    
    UE_LOG(LogTemp, Log, TEXT("Deleted tile %d"), TileIndex);
}

void AF12BuilderController::ExecuteRestoreTile(AF12Module* Module, int32 TileIndex)
{
    if (!Module || !GridSystem || TileIndex < 0 || TileIndex >= 12)
        return;

    FF12GridCoord Coord = GridSystem->WorldToGrid(Module->GetActorLocation());
    
    FF12Command Cmd;
    Cmd.CommandType = EF12CommandType::RestoreTile;
    Cmd.GridCoords.Add(Coord);
    Cmd.TileIndex = TileIndex;
    Cmd.bPreviousVisibility = Module->IsTileVisible(TileIndex);
    
    Module->SetTileVisible(TileIndex, true);
    
    if (CommandHistory)
    {
        CommandHistory->ExecuteCommand(Cmd, this);
    }
    
    UE_LOG(LogTemp, Log, TEXT("Restored tile %d"), TileIndex);
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
        
        // Ignore drag preview modules
        for (AF12Module* Preview : DragPreviewModules)
        {
            if (Preview)
            {
                QueryParams.AddIgnoredActor(Preview);
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

    if (CurrentMode != EF12BuilderMode::Build)
    {
        GhostModule->SetActorHiddenInGame(true);
        return;
    }

    // Hide ghost during drag (we use drag previews instead)
    if (bIsDragging)
    {
        GhostModule->SetActorHiddenInGame(true);
        // But still update current coord
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
        
        if (!bIsDragging)
        {
            FVector GhostPos = GridSystem->GridToWorld(CurrentGridCoord);
            GhostModule->SetActorLocation(GhostPos);
            GhostModule->SetActorHiddenInGame(false);
        }
    }
    else
    {
        GhostModule->SetActorHiddenInGame(true);
        bValidPlacement = false;
    }
}

void AF12BuilderController::PlaceModule()
{
    ExecutePlaceModule(CurrentGridCoord);
}

void AF12BuilderController::RemoveModule()
{
    ExecuteDeleteModule(CurrentGridCoord);
}

void AF12BuilderController::CyclePaintMaterial()
{
    if (PaintMaterials.Num() > 0)
    {
        CurrentPaintMaterialIndex = (CurrentPaintMaterialIndex + 1) % PaintMaterials.Num();
    }
}

// === DELETE HIGHLIGHT SYSTEM ===

void AF12BuilderController::UpdateDeleteHighlight()
{
    if (CurrentMode != EF12BuilderMode::Delete)
    {
        ClearHighlight();
        return;
    }

    int32 TileIndex = -1;
    AF12Module* Module = GetModuleUnderCursor(TileIndex);

    bool bSameTarget = (Module == HighlightedModule);
    if (bModifierHeld)
    {
        bSameTarget = bSameTarget && (TileIndex == HighlightedTileIndex);
    }

    if (!bSameTarget)
    {
        ClearHighlight();

        if (Module && DeleteHighlightMaterial)
        {
            HighlightedModule = Module;
            HighlightedTileIndex = TileIndex;
            HighlightedOriginalMaterials.Empty();

            if (bModifierHeld && TileIndex >= 0)
            {
                if (Module->TileMeshes.IsValidIndex(TileIndex) && Module->TileMeshes[TileIndex])
                {
                    HighlightedOriginalMaterials.Add(Module->TileMeshes[TileIndex]->GetMaterial(0));
                    Module->TileMeshes[TileIndex]->SetMaterial(0, DeleteHighlightMaterial);
                }
            }
            else
            {
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
            if (HighlightedModule->TileMeshes.IsValidIndex(HighlightedTileIndex) && 
                HighlightedModule->TileMeshes[HighlightedTileIndex])
            {
                HighlightedModule->TileMeshes[HighlightedTileIndex]->SetMaterial(0, HighlightedOriginalMaterials[0]);
            }
        }
        else
        {
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

// === HUD HELPERS ===

FLinearColor AF12BuilderController::GetCurrentPaintColor() const
{
    if (PaintColors.Num() > 0 && PaintColors.IsValidIndex(CurrentPaintMaterialIndex))
    {
        return PaintColors[CurrentPaintMaterialIndex];
    }
    
    static const FLinearColor FallbackColors[] = {
        FLinearColor(1.0f, 0.3f, 0.3f, 1.0f),
        FLinearColor(0.3f, 1.0f, 0.3f, 1.0f),
        FLinearColor(0.3f, 0.3f, 1.0f, 1.0f),
        FLinearColor(1.0f, 1.0f, 0.3f, 1.0f),
        FLinearColor(1.0f, 0.3f, 1.0f, 1.0f),
        FLinearColor(0.3f, 1.0f, 1.0f, 1.0f),
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