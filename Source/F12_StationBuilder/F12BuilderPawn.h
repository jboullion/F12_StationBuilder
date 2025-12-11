// F12BuilderPawn.h
// Third-person pawn for F12 Station Builder
// Provides a visible avatar and zoomable camera for scale reference

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "F12BuilderPawn.generated.h"

class UCapsuleComponent;
class UStaticMeshComponent;
class USpringArmComponent;
class UCameraComponent;
class UFloatingPawnMovement;

UCLASS()
class AF12BuilderPawn : public APawn
{
    GENERATED_BODY()

public:
    AF12BuilderPawn();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    // === COMPONENTS ===

    // Collision capsule (root)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UCapsuleComponent* CapsuleCollision;

    // Visible mesh representing the "astronaut"
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* AvatarMesh;

    // Spring arm for camera distance
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USpringArmComponent* SpringArm;

    // Third-person camera
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UCameraComponent* Camera;

    // Movement component
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UFloatingPawnMovement* MovementComponent;

    // === CAMERA SETTINGS ===

    // Current camera distance from pawn
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    float CameraDistance = 500.0f;

    // Minimum zoom distance
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    float MinCameraDistance = 100.0f;

    // Maximum zoom distance (far enough to see large stations)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    float MaxCameraDistance = 50000.0f;

    // Zoom speed per scroll tick
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    float ZoomSpeed = 0.1f;  // Percentage of current distance

    // Camera rotation speed (degrees per pixel)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    float CameraRotationSpeed = 0.5f;

    // Current camera pitch (clamped)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    float CameraPitch = -30.0f;

    // Current camera yaw
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    float CameraYaw = 0.0f;

    // Min/max pitch angles
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    float MinCameraPitch = -85.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    float MaxCameraPitch = 85.0f;

    // Orbit focus offset (where the camera looks relative to pawn center)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    FVector OrbitFocusOffset = FVector(0, 0, 50.0f);

    // === MOVEMENT SETTINGS ===

    // Movement speed (Unreal units per second)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float MoveSpeed = 1000.0f;

    // Sprint multiplier (when holding shift)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float SprintMultiplier = 3.0f;

    // Movement smoothing
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float MovementSmoothTime = 0.1f;

    // === AVATAR SETTINGS ===

    // Avatar scale (1.0 = roughly human sized at ~180cm)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avatar")
    float AvatarScale = 1.0f;

    // Avatar material (set in Blueprint)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avatar")
    UMaterialInterface* AvatarMaterial;

    // Hide avatar when zoomed in close
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avatar")
    float HideAvatarDistance = 200.0f;

    // === FUNCTIONS ===

    // Zoom the camera in or out
    UFUNCTION(BlueprintCallable, Category = "Camera")
    void ZoomCamera(float ZoomDelta);

    // Set camera distance directly
    UFUNCTION(BlueprintCallable, Category = "Camera")
    void SetCameraDistance(float NewDistance);

    // Rotate camera by delta
    UFUNCTION(BlueprintCallable, Category = "Camera")
    void RotateCamera(float DeltaYaw, float DeltaPitch);

    // Check if currently rotating camera (right mouse held)
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Camera")
    bool IsRotatingCamera() const { return bIsRotatingCamera; }

    // Get the forward vector for movement (based on camera yaw)
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Movement")
    FVector GetMovementForward() const;

    // Get the right vector for movement (based on camera yaw)
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Movement")
    FVector GetMovementRight() const;

protected:
    // Input state
    bool bIsRotatingCamera = false;
    bool bIsSprinting = false;
    FVector2D LastMousePosition;
    
    // Movement input (accumulated each frame)
    FVector MovementInput;
    FVector CurrentVelocity;

    // Input handlers
    void OnMoveForward(float Value);
    void OnMoveRight(float Value);
    void OnMoveUp(float Value);
    
    void OnCameraRotateStart();
    void OnCameraRotateStop();
    void OnMouseMove(float DeltaX, float DeltaY);
    
    void OnZoomIn();
    void OnZoomOut();
    
    void OnSprintStart();
    void OnSprintStop();

    // Update camera transform based on pitch/yaw/distance
    void UpdateCameraTransform();

    // Update avatar visibility based on zoom
    void UpdateAvatarVisibility();

    // Apply movement with smoothing
    void ApplyMovement(float DeltaTime);
};