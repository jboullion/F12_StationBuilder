// F12BuilderController.h
// Player controller for the F12 Station Builder
// Handles click-to-place module interaction

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "F12GridSystem.h"
#include "F12BuilderController.generated.h"

class AF12Module;

UCLASS()
class AF12BuilderController : public APlayerController
{
    GENERATED_BODY()

public:
    AF12BuilderController();

    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;
    virtual void Tick(float DeltaTime) override;
    

    // Reference to the grid system (find or spawn in BeginPlay)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Builder")
    AF12GridSystem* GridSystem;

    // Module class to spawn
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Builder")
    TSubclassOf<AF12Module> ModuleClass;

    // Material to apply to new modules
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Builder")
    UMaterialInterface* DefaultModuleMaterial;

    // Ghost preview mesh
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Builder")
    AF12Module* GhostModule;

    // Current placement position
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Builder")
    FF12GridCoord CurrentGridCoord;

    // Is the current position valid for placement?
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Builder")
    bool bValidPlacement;

    // Trace distance for placement
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Builder")
    float TraceDistance = 10000.0f;

    // Place a module at the current position
    UFUNCTION(BlueprintCallable, Category = "Builder")
    void PlaceModule();

    // Remove a module at the current position
    UFUNCTION(BlueprintCallable, Category = "Builder")
    void RemoveModule();

protected:
    // Input handlers
    void OnLeftClick();
    void OnRightClick();

    // Update the ghost preview position
    void UpdateGhostPreview();

    // Perform trace from camera
    bool TraceFromCamera(FHitResult& OutHit);
};
