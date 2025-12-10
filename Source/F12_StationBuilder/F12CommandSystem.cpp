// F12CommandSystem.cpp
// Implementation of Undo/Redo Command System

#include "F12CommandSystem.h"
#include "F12BuilderController.h"
#include "F12Module.h"
#include "ProceduralMeshComponent.h"

UF12CommandHistory::UF12CommandHistory()
{
    MaxHistorySize = 50;
}

void UF12CommandHistory::ExecuteCommand(const FF12Command& Command, AF12BuilderController* Controller)
{
    if (!Controller)
        return;

    // Clear redo stack when new command is executed
    RedoStack.Empty();

    // Add to undo stack
    UndoStack.Add(Command);

    // Trim history if too large
    while (UndoStack.Num() > MaxHistorySize)
    {
        UndoStack.RemoveAt(0);
    }

    UE_LOG(LogTemp, Log, TEXT("Command executed. Undo stack: %d, Redo stack: %d"), 
        UndoStack.Num(), RedoStack.Num());
}

bool UF12CommandHistory::Undo(AF12BuilderController* Controller)
{
    if (!CanUndo() || !Controller)
        return false;

    // Pop from undo stack
    FF12Command Command = UndoStack.Pop();

    // Create inverse command for redo
    FF12Command InverseCommand = CreateInverseCommand(Command, Controller);

    // Apply the undo
    ApplyCommand(Command, Controller, true);

    // Push inverse to redo stack
    RedoStack.Add(InverseCommand);

    UE_LOG(LogTemp, Log, TEXT("Undo performed. Undo stack: %d, Redo stack: %d"), 
        UndoStack.Num(), RedoStack.Num());

    return true;
}

bool UF12CommandHistory::Redo(AF12BuilderController* Controller)
{
    if (!CanRedo() || !Controller)
        return false;

    // Pop from redo stack
    FF12Command Command = RedoStack.Pop();

    // Create inverse for undo
    FF12Command InverseCommand = CreateInverseCommand(Command, Controller);

    // Apply the redo
    ApplyCommand(Command, Controller, false);

    // Push inverse to undo stack
    UndoStack.Add(InverseCommand);

    UE_LOG(LogTemp, Log, TEXT("Redo performed. Undo stack: %d, Redo stack: %d"), 
        UndoStack.Num(), RedoStack.Num());

    return true;
}

void UF12CommandHistory::ClearHistory()
{
    UndoStack.Empty();
    RedoStack.Empty();
}

void UF12CommandHistory::ApplyCommand(const FF12Command& Command, AF12BuilderController* Controller, bool bIsUndo)
{
    if (!Controller || !Controller->GridSystem)
        return;

    switch (Command.CommandType)
    {
        case EF12CommandType::PlaceModule:
        {
            if (bIsUndo)
            {
                // Undo place = delete
                for (const FF12GridCoord& Coord : Command.GridCoords)
                {
                    AActor* Module = Controller->GridSystem->GetModuleAt(Coord);
                    if (Module)
                    {
                        Controller->GridSystem->ClearOccupied(Coord);
                        Module->Destroy();
                    }
                }
            }
            else
            {
                // Redo place = place again
                for (int32 i = 0; i < Command.GridCoords.Num(); i++)
                {
                    const FF12GridCoord& Coord = Command.GridCoords[i];
                    FVector SpawnPos = Controller->GridSystem->GridToWorld(Coord);
                    
                    FActorSpawnParameters SpawnParams;
                    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
                    
                    AF12Module* NewModule = Controller->GetWorld()->SpawnActor<AF12Module>(
                        Command.ModuleClass, SpawnPos, FRotator::ZeroRotator, SpawnParams);
                    
                    if (NewModule)
                    {
                        if (Controller->DefaultModuleMaterial)
                        {
                            NewModule->TileMaterial = Controller->DefaultModuleMaterial;
                        }
                        NewModule->TileMaterials = Controller->PaintMaterials;
                        NewModule->GenerateModule();
                        
                        // Restore material indices if available
                        if (Command.ModuleMaterialIndices.Num() >= 12 * (i + 1))
                        {
                            for (int32 t = 0; t < 12; t++)
                            {
                                NewModule->SetTileMaterialIndex(t, Command.ModuleMaterialIndices[i * 12 + t]);
                            }
                        }
                        
                        Controller->GridSystem->SetOccupied(Coord, NewModule);
                    }
                }
            }
            break;
        }

        case EF12CommandType::DeleteModule:
        {
            if (bIsUndo)
            {
                // Undo delete = restore module
                for (int32 i = 0; i < Command.GridCoords.Num(); i++)
                {
                    const FF12GridCoord& Coord = Command.GridCoords[i];
                    FVector SpawnPos = Controller->GridSystem->GridToWorld(Coord);
                    
                    FActorSpawnParameters SpawnParams;
                    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
                    
                    AF12Module* NewModule = Controller->GetWorld()->SpawnActor<AF12Module>(
                        Command.ModuleClass, SpawnPos, FRotator::ZeroRotator, SpawnParams);
                    
                    if (NewModule)
                    {
                        if (Controller->DefaultModuleMaterial)
                        {
                            NewModule->TileMaterial = Controller->DefaultModuleMaterial;
                        }
                        NewModule->TileMaterials = Controller->PaintMaterials;
                        NewModule->GenerateModule();
                        
                        // Restore exact material state
                        if (Command.ModuleMaterialIndices.Num() >= 12)
                        {
                            for (int32 t = 0; t < 12; t++)
                            {
                                int32 MatIndex = (i * 12 + t < Command.ModuleMaterialIndices.Num()) 
                                    ? Command.ModuleMaterialIndices[i * 12 + t] : 0;
                                NewModule->SetTileMaterialIndex(t, MatIndex);
                            }
                        }
                        
                        // Restore tile visibility
                        if (Command.ModuleTileVisibility.Num() >= 12)
                        {
                            for (int32 t = 0; t < 12; t++)
                            {
                                bool bVisible = (i * 12 + t < Command.ModuleTileVisibility.Num())
                                    ? Command.ModuleTileVisibility[i * 12 + t] : true;
                                NewModule->SetTileVisible(t, bVisible);
                            }
                        }
                        
                        Controller->GridSystem->SetOccupied(Coord, NewModule);
                    }
                }
            }
            else
            {
                // Redo delete = delete again
                for (const FF12GridCoord& Coord : Command.GridCoords)
                {
                    AActor* Module = Controller->GridSystem->GetModuleAt(Coord);
                    if (Module)
                    {
                        Controller->GridSystem->ClearOccupied(Coord);
                        Module->Destroy();
                    }
                }
            }
            break;
        }

        case EF12CommandType::PaintTile:
        case EF12CommandType::PaintModule:
        {
            if (Command.GridCoords.Num() > 0)
            {
                AF12Module* Module = Cast<AF12Module>(
                    Controller->GridSystem->GetModuleAt(Command.GridCoords[0]));
                
                if (Module)
                {
                    if (bIsUndo)
                    {
                        // Restore previous materials using stored material pointers
                        // Directly set material on mesh components
                        if (Command.TileIndex >= 0)
                        {
                            // Single tile - restore the actual material
                            if (Command.PreviousMaterials.Num() > 0 && Command.PreviousMaterials[0])
                            {
                                if (Module->TileMeshes.IsValidIndex(Command.TileIndex) && Module->TileMeshes[Command.TileIndex])
                                {
                                    Module->TileMeshes[Command.TileIndex]->SetMaterial(0, Command.PreviousMaterials[0]);
                                }
                            }
                        }
                        else
                        {
                            // Whole module - restore all materials
                            for (int32 i = 0; i < FMath::Min(12, Command.PreviousMaterials.Num()); i++)
                            {
                                if (Command.PreviousMaterials[i] && Module->TileMeshes.IsValidIndex(i) && Module->TileMeshes[i])
                                {
                                    Module->TileMeshes[i]->SetMaterial(0, Command.PreviousMaterials[i]);
                                }
                            }
                        }
                    }
                    else
                    {
                        // Apply new material (redo)
                        if (Command.TileIndex >= 0)
                        {
                            Module->SetTileMaterialIndex(Command.TileIndex, Command.NewMaterialIndex);
                        }
                        else
                        {
                            for (int32 i = 0; i < 12; i++)
                            {
                                Module->SetTileMaterialIndex(i, Command.NewMaterialIndex);
                            }
                        }
                    }
                }
            }
            break;
        }

        case EF12CommandType::DeleteTile:
        case EF12CommandType::RestoreTile:
        {
            if (Command.GridCoords.Num() > 0)
            {
                AF12Module* Module = Cast<AF12Module>(
                    Controller->GridSystem->GetModuleAt(Command.GridCoords[0]));
                
                if (Module && Command.TileIndex >= 0)
                {
                    if (bIsUndo)
                    {
                        Module->SetTileVisible(Command.TileIndex, Command.bPreviousVisibility);
                    }
                    else
                    {
                        Module->SetTileVisible(Command.TileIndex, !Command.bPreviousVisibility);
                    }
                }
            }
            break;
        }

        case EF12CommandType::PlaceMultiple:
        {
            // Same as PlaceModule but for multiple
            if (bIsUndo)
            {
                for (const FF12GridCoord& Coord : Command.GridCoords)
                {
                    AActor* Module = Controller->GridSystem->GetModuleAt(Coord);
                    if (Module)
                    {
                        Controller->GridSystem->ClearOccupied(Coord);
                        Module->Destroy();
                    }
                }
            }
            else
            {
                for (const FF12GridCoord& Coord : Command.GridCoords)
                {
                    if (!Controller->GridSystem->IsOccupied(Coord))
                    {
                        FVector SpawnPos = Controller->GridSystem->GridToWorld(Coord);
                        
                        FActorSpawnParameters SpawnParams;
                        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
                        
                        AF12Module* NewModule = Controller->GetWorld()->SpawnActor<AF12Module>(
                            Command.ModuleClass, SpawnPos, FRotator::ZeroRotator, SpawnParams);
                        
                        if (NewModule)
                        {
                            if (Controller->DefaultModuleMaterial)
                            {
                                NewModule->TileMaterial = Controller->DefaultModuleMaterial;
                            }
                            NewModule->TileMaterials = Controller->PaintMaterials;
                            NewModule->GenerateModule();
                            Controller->GridSystem->SetOccupied(Coord, NewModule);
                        }
                    }
                }
            }
            break;
        }
    }
}

FF12Command UF12CommandHistory::CreateInverseCommand(const FF12Command& Command, AF12BuilderController* Controller)
{
    FF12Command Inverse;
    Inverse.GridCoords = Command.GridCoords;
    Inverse.TileIndex = Command.TileIndex;
    Inverse.ModuleClass = Command.ModuleClass;

    switch (Command.CommandType)
    {
        case EF12CommandType::PlaceModule:
        case EF12CommandType::PlaceMultiple:
            // Inverse of place is delete
            Inverse.CommandType = EF12CommandType::DeleteModule;
            // Store current state for potential re-redo
            for (const FF12GridCoord& Coord : Command.GridCoords)
            {
                AF12Module* Module = Cast<AF12Module>(Controller->GridSystem->GetModuleAt(Coord));
                if (Module)
                {
                    for (int32 i = 0; i < 12; i++)
                    {
                        Inverse.ModuleMaterialIndices.Add(Module->TileMaterialIndices.IsValidIndex(i) ? Module->TileMaterialIndices[i] : 0);
                        Inverse.ModuleTileVisibility.Add(Module->IsTileVisible(i));
                    }
                }
            }
            break;

        case EF12CommandType::DeleteModule:
            // Inverse of delete is place
            Inverse.CommandType = EF12CommandType::PlaceModule;
            Inverse.ModuleMaterialIndices = Command.ModuleMaterialIndices;
            Inverse.ModuleTileVisibility = Command.ModuleTileVisibility;
            break;

        case EF12CommandType::PaintTile:
        case EF12CommandType::PaintModule:
            // Inverse is paint with previous colors
            Inverse.CommandType = Command.CommandType;
            Inverse.NewMaterialIndex = Command.PreviousMaterialIndices.Num() > 0 ? Command.PreviousMaterialIndices[0] : 0;
            Inverse.PreviousMaterialIndices.Add(Command.NewMaterialIndex);
            break;

        case EF12CommandType::DeleteTile:
            Inverse.CommandType = EF12CommandType::RestoreTile;
            Inverse.bPreviousVisibility = !Command.bPreviousVisibility;
            break;

        case EF12CommandType::RestoreTile:
            Inverse.CommandType = EF12CommandType::DeleteTile;
            Inverse.bPreviousVisibility = !Command.bPreviousVisibility;
            break;
    }

    return Inverse;
}
