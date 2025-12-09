// F12BuilderGameMode.cpp
// Implementation of the builder game mode

#include "F12BuilderGameMode.h"
#include "F12BuilderController.h"
#include "GameFramework/SpectatorPawn.h"

AF12BuilderGameMode::AF12BuilderGameMode()
{
    // Use our custom player controller
    PlayerControllerClass = AF12BuilderController::StaticClass();
    
    // Use spectator pawn for free camera movement
    DefaultPawnClass = ASpectatorPawn::StaticClass();
}
