// F12CommandSystem.h
// Undo/Redo Command System for F12 Station Builder
// Uses Command Pattern to track all building actions

#pragma once

#include "CoreMinimal.h"
#include "F12GridSystem.h"
#include "F12CommandSystem.generated.h"

class AF12Module;
class AF12BuilderController;

// Base command type enum
UENUM(BlueprintType)
enum class EF12CommandType : uint8
{
    PlaceModule,
    DeleteModule,
    PaintTile,
    PaintModule,
    DeleteTile,
    RestoreTile,
    PlaceMultiple  // For drag-build
};

// Single command structure - stores everything needed to undo/redo
USTRUCT(BlueprintType)
struct FF12Command
{
    GENERATED_BODY()

    // What type of command this is
    UPROPERTY()
    EF12CommandType CommandType;

    // Grid coordinate(s) affected
    UPROPERTY()
    TArray<FF12GridCoord> GridCoords;

    // For paint commands - which tile index (-1 = whole module)
    UPROPERTY()
    int32 TileIndex = -1;

    // For paint commands - store actual material pointers (more reliable than indices)
    UPROPERTY()
    TArray<UMaterialInterface*> PreviousMaterials;

    UPROPERTY()
    int32 NewMaterialIndex = 0;
    
    // For paint - also store indices as backup
    UPROPERTY()
    TArray<int32> PreviousMaterialIndices;

    // For delete/restore tile - visibility state
    UPROPERTY()
    bool bPreviousVisibility = true;

    // For module placement - store the module class used
    UPROPERTY()
    TSubclassOf<AF12Module> ModuleClass;

    // For module deletion - store material states so we can restore exactly
    UPROPERTY()
    TArray<int32> ModuleMaterialIndices;

    UPROPERTY()
    TArray<bool> ModuleTileVisibility;

    FF12Command() {}
};

// Command history manager
UCLASS(BlueprintType)
class UF12CommandHistory : public UObject
{
    GENERATED_BODY()

public:
    UF12CommandHistory();

    // Maximum number of commands to store
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "History")
    int32 MaxHistorySize = 50;

    // Execute a new command and add to history
    UFUNCTION(BlueprintCallable, Category = "History")
    void ExecuteCommand(const FF12Command& Command, AF12BuilderController* Controller);

    // Undo the last command
    UFUNCTION(BlueprintCallable, Category = "History")
    bool Undo(AF12BuilderController* Controller);

    // Redo the last undone command
    UFUNCTION(BlueprintCallable, Category = "History")
    bool Redo(AF12BuilderController* Controller);

    // Check if undo/redo are available
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "History")
    bool CanUndo() const { return UndoStack.Num() > 0; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "History")
    bool CanRedo() const { return RedoStack.Num() > 0; }

    // Clear all history
    UFUNCTION(BlueprintCallable, Category = "History")
    void ClearHistory();

    // Get current history sizes (for UI display)
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "History")
    int32 GetUndoCount() const { return UndoStack.Num(); }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "History")
    int32 GetRedoCount() const { return RedoStack.Num(); }

protected:
    // Command stacks
    UPROPERTY()
    TArray<FF12Command> UndoStack;

    UPROPERTY()
    TArray<FF12Command> RedoStack;

    // Internal execution (doesn't add to history - used by undo/redo)
    void ApplyCommand(const FF12Command& Command, AF12BuilderController* Controller, bool bIsUndo);

    // Create the inverse of a command for undo
    FF12Command CreateInverseCommand(const FF12Command& Command, AF12BuilderController* Controller);
};
