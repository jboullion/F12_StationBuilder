// F12BuilderActions.h
// Undo/Redo Action System for F12 Station Builder

#pragma once

#include "CoreMinimal.h"
#include "F12GridSystem.h"
#include "F12BuilderActions.generated.h"

class AF12Module;
class AF12BuilderController;

// ============================================================================
// BASE ACTION CLASS
// ============================================================================

UENUM(BlueprintType)
enum class EF12ActionType : uint8
{
    PlaceModule,
    RemoveModule,
    PaintTile,
    PaintModule,
    DeleteTile,
    RestoreTile,
    PlaceMultipleModules  // For drag-build
};

USTRUCT(BlueprintType)
struct FF12BuilderAction
{
    GENERATED_BODY()

    UPROPERTY()
    EF12ActionType ActionType = EF12ActionType::PlaceModule;

    // For module placement/removal
    UPROPERTY()
    FF12GridCoord GridCoord;

    // For paint operations
    UPROPERTY()
    int32 TileIndex = -1;

    UPROPERTY()
    int32 OldMaterialIndex = 0;

    UPROPERTY()
    int32 NewMaterialIndex = 0;

    // For tile visibility
    UPROPERTY()
    bool bOldVisibility = true;

    UPROPERTY()
    bool bNewVisibility = true;

    // For storing module state when removed (material indices for all 12 tiles)
    UPROPERTY()
    TArray<int32> StoredMaterialIndices;

    UPROPERTY()
    TArray<bool> StoredVisibility;

    // For multi-module operations (drag build)
    UPROPERTY()
    TArray<FF12GridCoord> MultipleCoords;

    // Weak reference to affected module (may be invalid after undo)
    UPROPERTY()
    TWeakObjectPtr<AF12Module> AffectedModule;

    FF12BuilderAction() {}

    // Factory methods for creating specific action types
    static FF12BuilderAction CreatePlaceModule(FF12GridCoord Coord, AF12Module* Module);
    static FF12BuilderAction CreateRemoveModule(FF12GridCoord Coord, AF12Module* Module);
    static FF12BuilderAction CreatePaintTile(AF12Module* Module, int32 TileIdx, int32 OldMatIdx, int32 NewMatIdx);
    static FF12BuilderAction CreatePaintModule(AF12Module* Module, const TArray<int32>& OldMatIndices, int32 NewMatIdx);
    static FF12BuilderAction CreateDeleteTile(AF12Module* Module, int32 TileIdx);
    static FF12BuilderAction CreateRestoreTile(AF12Module* Module, int32 TileIdx);
    static FF12BuilderAction CreatePlaceMultiple(const TArray<FF12GridCoord>& Coords);
};

// ============================================================================
// ACTION HISTORY MANAGER
// ============================================================================

UCLASS()
class UF12ActionHistory : public UObject
{
    GENERATED_BODY()

public:
    UF12ActionHistory();

    // Maximum number of actions to store
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "History")
    int32 MaxHistorySize = 100;

    // Add an action to history (clears redo stack)
    void AddAction(const FF12BuilderAction& Action);

    // Check if undo/redo is available
    bool CanUndo() const;
    bool CanRedo() const;

    // Get the action to undo/redo (doesn't modify stacks)
    FF12BuilderAction* PeekUndo();
    FF12BuilderAction* PeekRedo();

    // Pop action from undo stack and push to redo
    FF12BuilderAction PopForUndo();

    // Pop action from redo stack and push to undo
    FF12BuilderAction PopForRedo();

    // Clear all history
    void ClearHistory();

    // Get stack sizes (for UI display)
    int32 GetUndoCount() const { return UndoStack.Num(); }
    int32 GetRedoCount() const { return RedoStack.Num(); }

protected:
    UPROPERTY()
    TArray<FF12BuilderAction> UndoStack;

    UPROPERTY()
    TArray<FF12BuilderAction> RedoStack;
};