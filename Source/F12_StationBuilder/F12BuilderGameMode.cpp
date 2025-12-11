// F12BuilderGameMode.cpp
// Implementation of the builder game mode

#include "F12BuilderGameMode.h"
#include "F12BuilderController.h"
#include "F12BuilderPawn.h"

AF12BuilderGameMode::AF12BuilderGameMode()
{
    // Use our custom player controller
    PlayerControllerClass = AF12BuilderController::StaticClass();
    
    // Use our third-person pawn for scale visualization
    DefaultPawnClass = AF12BuilderPawn::StaticClass();
}