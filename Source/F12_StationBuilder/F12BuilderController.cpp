// F12BuilderController.cpp
// Implementation of the builder player controller with Mode System

#include "F12BuilderController.h"
#include "F12Module.h"
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
}

void AF12BuilderController::SetupInputComponent()
{
    Super::SetupInputComponent();

    // Primary/Secondary actions
    InputComponent->BindAction("PrimaryAction", IE_Pressed, this, &AF12BuilderController::OnPrimaryAction);
    InputComponent->BindAction("SecondaryAction", IE_Pressed, this, &AF12BuilderController::OnSecondaryAction);
    
    // Mode switching
    InputComponent->BindAction("CycleMode", IE_Pressed, this, &AF12BuilderController::OnCycleMode);
    InputComponent->BindAction("BuildMode", IE_Pressed, this, &AF12BuilderController::OnSetBuildMode);
    InputComponent->BindAction("PaintMode", IE_Pressed, this, &AF12BuilderController::OnSetPaintMode);
    InputComponent->BindAction("DeleteMode", IE_Pressed, this, &AF12BuilderController::OnSetDeleteMode);
    
    // Scroll wheel for material selection
    InputComponent->BindAction("ScrollUp", IE_Pressed, this, &AF12BuilderController::OnScrollUp);
    InputComponent->BindAction("ScrollDown", IE_Pressed, this, &AF12BuilderController::OnScrollDown);
    
    // Modifier key (Shift) for tile-level operations
    InputComponent->BindAction("Modifier", IE_Pressed, this, &AF12BuilderController::OnModifierPressed);
    InputComponent->BindAction("Modifier", IE_Released, this, &AF12BuilderController::OnModifierReleased);
}

void AF12BuilderController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    UpdateGhostPreview();
    UpdateDeleteHighlight();
}

// === MODE SWITCHING ===

void AF12BuilderController::SetMode(EF12BuilderMode NewMode)
{
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
void AF12BuilderController::OnSecondaryAction() { SecondaryAction(); }
void AF12BuilderController::OnCycleMode()       { CycleMode(); }
void AF12BuilderController::OnSetBuildMode()    { SetBuildMode(); }
void AF12BuilderController::OnSetPaintMode()    { SetPaintMode(); }
void AF12BuilderController::OnSetDeleteMode()   { SetDeleteMode(); }

void AF12BuilderController::OnModifierPressed()  { bModifierHeld = true; }
void AF12BuilderController::OnModifierReleased() { bModifierHeld = false; }

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
    // Place module
    PlaceModule();
}

void AF12BuilderController::HandleBuildSecondary()
{
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
            // Paint single tile
            Module->SetTileMaterialIndex(TileIndex, CurrentPaintMaterialIndex);
            UE_LOG(LogTemp, Log, TEXT("Painted tile %d with material %d"), TileIndex, CurrentPaintMaterialIndex);
        }
        else
        {
            // Paint entire module
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
            // Delete single tile (hide it)
            Module->SetTileVisible(TileIndex, false);
            UE_LOG(LogTemp, Log, TEXT("Deleted tile %d"), TileIndex);
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
        Module->SetTileVisible(TileIndex, true);
        UE_LOG(LogTemp, Log, TEXT("Restored tile %d"), TileIndex);
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

    // Only show ghost in Build mode
    if (CurrentMode != EF12BuilderMode::Build)
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

    FVector SpawnPos = GridSystem->GridToWorld(CurrentGridCoord);
    
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    
    AF12Module* NewModule = GetWorld()->SpawnActor<AF12Module>(ModuleClass, SpawnPos, FRotator::ZeroRotator, SpawnParams);
    
    if (NewModule)
    {
        if (DefaultModuleMaterial)
        {
            NewModule->TileMaterial = DefaultModuleMaterial;
        }
        
        // Copy paint materials
        NewModule->TileMaterials = PaintMaterials;
        NewModule->GenerateModule();
        
        GridSystem->SetOccupied(CurrentGridCoord, NewModule);
        
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

    AActor* ModuleToRemove = GridSystem->GetModuleAt(CurrentGridCoord);
    
    if (ModuleToRemove)
    {
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

// === HUD HELPERS ===

FLinearColor AF12BuilderController::GetCurrentPaintColor() const
{
    if (PaintColors.Num() > 0 && PaintColors.IsValidIndex(CurrentPaintMaterialIndex))
    {
        return PaintColors[CurrentPaintMaterialIndex];
    }
    
    // Fallback: If no PaintColors set, generate a color based on index
    // This creates a rainbow effect so user can see cycling is working
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