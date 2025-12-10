// F12BuilderActions.cpp
// Implementation of the Undo/Redo Action System

#include "F12BuilderActions.h"
#include "F12Module.h"

// ============================================================================
// FF12BuilderAction Factory Methods
// ============================================================================

FF12BuilderAction FF12BuilderAction::CreatePlaceModule(FF12GridCoord Coord, AF12Module* Module)
{
    FF12BuilderAction Action;
    Action.ActionType = EF12ActionType::PlaceModule;
    Action.GridCoord = Coord;
    Action.AffectedModule = Module;
    return Action;
}

FF12BuilderAction FF12BuilderAction::CreateRemoveModule(FF12GridCoord Coord, AF12Module* Module)
{
    FF12BuilderAction Action;
    Action.ActionType = EF12ActionType::RemoveModule;
    Action.GridCoord = Coord;
    Action.AffectedModule = Module;

    // Store the module's current state so we can restore it on undo
    if (Module)
    {
        Action.StoredMaterialIndices = Module->TileMaterialIndices;
        Action.StoredVisibility = Module->TileVisibility;
    }

    return Action;
}

FF12BuilderAction FF12BuilderAction::CreatePaintTile(AF12Module* Module, int32 TileIdx, int32 OldMatIdx, int32 NewMatIdx)
{
    FF12BuilderAction Action;
    Action.ActionType = EF12ActionType::PaintTile;
    Action.AffectedModule = Module;
    Action.TileIndex = TileIdx;
    Action.OldMaterialIndex = OldMatIdx;
    Action.NewMaterialIndex = NewMatIdx;
    return Action;
}

FF12BuilderAction FF12BuilderAction::CreatePaintModule(AF12Module* Module, const TArray<int32>& OldMatIndices, int32 NewMatIdx)
{
    FF12BuilderAction Action;
    Action.ActionType = EF12ActionType::PaintModule;
    Action.AffectedModule = Module;
    Action.StoredMaterialIndices = OldMatIndices;
    Action.NewMaterialIndex = NewMatIdx;
    return Action;
}

FF12BuilderAction FF12BuilderAction::CreateDeleteTile(AF12Module* Module, int32 TileIdx)
{
    FF12BuilderAction Action;
    Action.ActionType = EF12ActionType::DeleteTile;
    Action.AffectedModule = Module;
    Action.TileIndex = TileIdx;
    Action.bOldVisibility = true;
    Action.bNewVisibility = false;
    return Action;
}

FF12BuilderAction FF12BuilderAction::CreateRestoreTile(AF12Module* Module, int32 TileIdx)
{
    FF12BuilderAction Action;
    Action.ActionType = EF12ActionType::RestoreTile;
    Action.AffectedModule = Module;
    Action.TileIndex = TileIdx;
    Action.bOldVisibility = false;
    Action.bNewVisibility = true;
    return Action;
}

FF12BuilderAction FF12BuilderAction::CreatePlaceMultiple(const TArray<FF12GridCoord>& Coords)
{
    FF12BuilderAction Action;
    Action.ActionType = EF12ActionType::PlaceMultipleModules;
    Action.MultipleCoords = Coords;
    return Action;
}

// ============================================================================
// UF12ActionHistory Implementation
// ============================================================================

UF12ActionHistory::UF12ActionHistory()
{
    MaxHistorySize = 100;
}

void UF12ActionHistory::AddAction(const FF12BuilderAction& Action)
{
    // Clear redo stack when a new action is performed
    RedoStack.Empty();

    // Add to undo stack
    UndoStack.Push(Action);

    // Trim if over max size
    while (UndoStack.Num() > MaxHistorySize)
    {
        UndoStack.RemoveAt(0);
    }

    UE_LOG(LogTemp, Log, TEXT("Action added to history. Undo stack: %d, Redo stack: %d"), 
        UndoStack.Num(), RedoStack.Num());
}

bool UF12ActionHistory::CanUndo() const
{
    return UndoStack.Num() > 0;
}

bool UF12ActionHistory::CanRedo() const
{
    return RedoStack.Num() > 0;
}

FF12BuilderAction* UF12ActionHistory::PeekUndo()
{
    if (UndoStack.Num() > 0)
    {
        return &UndoStack.Last();
    }
    return nullptr;
}

FF12BuilderAction* UF12ActionHistory::PeekRedo()
{
    if (RedoStack.Num() > 0)
    {
        return &RedoStack.Last();
    }
    return nullptr;
}

FF12BuilderAction UF12ActionHistory::PopForUndo()
{
    FF12BuilderAction Action = UndoStack.Pop();
    RedoStack.Push(Action);
    return Action;
}

FF12BuilderAction UF12ActionHistory::PopForRedo()
{
    FF12BuilderAction Action = RedoStack.Pop();
    UndoStack.Push(Action);
    return Action;
}

void UF12ActionHistory::ClearHistory()
{
    UndoStack.Empty();
    RedoStack.Empty();
    UE_LOG(LogTemp, Log, TEXT("Action history cleared"));
}