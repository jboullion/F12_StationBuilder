#include "F12BuilderPawn.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AF12BuilderPawn::AF12BuilderPawn()
{
    PrimaryActorTick.bCanEverTick = true;

    // === CREATE COLLISION CAPSULE (ROOT) ===
    CapsuleCollision = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CapsuleCollision"));
    CapsuleCollision->InitCapsuleSize(50.0f, 90.0f);  // Human-ish proportions
    CapsuleCollision->SetCollisionProfileName(TEXT("Pawn"));
    CapsuleCollision->SetSimulatePhysics(false);
    RootComponent = CapsuleCollision;

    // === CREATE AVATAR MESH ===
    AvatarMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AvatarMesh"));
    AvatarMesh->SetupAttachment(RootComponent);
    AvatarMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    AvatarMesh->SetRelativeLocation(FVector(0, 0, 0));
    
    // Use engine's built-in capsule mesh for the avatar
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CapsuleMesh(
        TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if (CapsuleMesh.Succeeded())
    {
        AvatarMesh->SetStaticMesh(CapsuleMesh.Object);
        // Scale to roughly human proportions (cylinder is 100 units tall by default)
        // We want ~180cm tall, ~50cm wide
        AvatarMesh->SetRelativeScale3D(FVector(0.5f, 0.5f, 1.8f));
    }

    // === CREATE SPRING ARM (Orbit Pivot) ===
    SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    SpringArm->SetupAttachment(RootComponent);
    SpringArm->TargetArmLength = CameraDistance;
    SpringArm->bDoCollisionTest = false;  // Don't collide with station modules
    SpringArm->bUsePawnControlRotation = false;  // We control rotation manually
    SpringArm->bInheritPitch = false;
    SpringArm->bInheritYaw = false;
    SpringArm->bInheritRoll = false;
    SpringArm->bEnableCameraLag = true;  // Smooth camera movement
    SpringArm->CameraLagSpeed = 10.0f;
    SpringArm->bEnableCameraRotationLag = true;
    SpringArm->CameraRotationLagSpeed = 10.0f;
    // Offset the pivot point slightly up (chest height rather than feet)
    SpringArm->SetRelativeLocation(FVector(0, 0, 50.0f));

    // === CREATE CAMERA ===
    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
    Camera->bUsePawnControlRotation = false;

    // === CREATE MOVEMENT COMPONENT ===
    MovementComponent = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("MovementComponent"));
    MovementComponent->MaxSpeed = MoveSpeed * SprintMultiplier;
    MovementComponent->Acceleration = MoveSpeed * 10.0f;
    MovementComponent->Deceleration = MoveSpeed * 10.0f;

    // Allow player controller to possess this pawn
    AutoPossessPlayer = EAutoReceiveInput::Player0;
}

void AF12BuilderPawn::BeginPlay()
{
    Super::BeginPlay();

    // Apply avatar material if set
    if (AvatarMaterial && AvatarMesh)
    {
        AvatarMesh->SetMaterial(0, AvatarMaterial);
    }

    // Apply orbit focus offset to spring arm
    if (SpringArm)
    {
        SpringArm->SetRelativeLocation(OrbitFocusOffset);
    }

    // Set initial camera position using world rotation
    UpdateCameraTransform();

    UE_LOG(LogTemp, Log, TEXT("F12BuilderPawn initialized. Camera distance: %.0f, Pitch: %.1f, Yaw: %.1f"), 
        CameraDistance, CameraPitch, CameraYaw);
}

void AF12BuilderPawn::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Note: Camera rotation is handled by the controller calling RotateCamera() directly

    // Apply movement
    ApplyMovement(DeltaTime);

    // Update avatar visibility based on zoom level
    UpdateAvatarVisibility();
}

void AF12BuilderPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    // Movement axes
    PlayerInputComponent->BindAxis("MoveForward", this, &AF12BuilderPawn::OnMoveForward);
    PlayerInputComponent->BindAxis("MoveRight", this, &AF12BuilderPawn::OnMoveRight);
    PlayerInputComponent->BindAxis("MoveUp", this, &AF12BuilderPawn::OnMoveUp);

    // Note: Camera rotation is handled by the controller (F12BuilderController)
    // Zoom is handled by controller (ZoomIn/ZoomOut actions call ZoomCamera)

    // Sprint
    PlayerInputComponent->BindAction("Sprint", IE_Pressed, this, &AF12BuilderPawn::OnSprintStart);
    PlayerInputComponent->BindAction("Sprint", IE_Released, this, &AF12BuilderPawn::OnSprintStop);
}

// === INPUT HANDLERS ===

void AF12BuilderPawn::OnMoveForward(float Value)
{
    MovementInput.X = Value;
}

void AF12BuilderPawn::OnMoveRight(float Value)
{
    MovementInput.Y = Value;
}

void AF12BuilderPawn::OnMoveUp(float Value)
{
    MovementInput.Z = Value;
}

void AF12BuilderPawn::OnCameraRotateStart()
{
    bIsRotatingCamera = true;
    
    // Hide cursor while rotating
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (PC)
    {
        // Don't hide cursor - we still need it for building
        // PC->bShowMouseCursor = false;
    }
}

void AF12BuilderPawn::OnCameraRotateStop()
{
    bIsRotatingCamera = false;
}

void AF12BuilderPawn::OnZoomIn()
{
    ZoomCamera(-ZoomSpeed);
}

void AF12BuilderPawn::OnZoomOut()
{
    ZoomCamera(ZoomSpeed);
}

void AF12BuilderPawn::OnSprintStart()
{
    bIsSprinting = true;
}

void AF12BuilderPawn::OnSprintStop()
{
    bIsSprinting = false;
}

// === CAMERA FUNCTIONS ===

void AF12BuilderPawn::ZoomCamera(float ZoomDelta)
{
    // Zoom is proportional to current distance for smooth feel at all scales
    float ZoomAmount = CameraDistance * ZoomDelta;
    SetCameraDistance(CameraDistance + ZoomAmount);
}

void AF12BuilderPawn::SetCameraDistance(float NewDistance)
{
    CameraDistance = FMath::Clamp(NewDistance, MinCameraDistance, MaxCameraDistance);
    
    if (SpringArm)
    {
        SpringArm->TargetArmLength = CameraDistance;
    }
}

void AF12BuilderPawn::RotateCamera(float DeltaYaw, float DeltaPitch)
{
    CameraYaw += DeltaYaw;
    CameraPitch = FMath::Clamp(CameraPitch + DeltaPitch, MinCameraPitch, MaxCameraPitch);
    
    UpdateCameraTransform();
}

void AF12BuilderPawn::UpdateCameraTransform()
{
    if (SpringArm)
    {
        // Use world rotation so orbit works regardless of pawn orientation
        FRotator NewRotation(CameraPitch, CameraYaw, 0);
        SpringArm->SetWorldRotation(NewRotation);
    }
}

void AF12BuilderPawn::UpdateAvatarVisibility()
{
    if (AvatarMesh)
    {
        bool bShouldBeVisible = CameraDistance > HideAvatarDistance;
        if (AvatarMesh->IsVisible() != bShouldBeVisible)
        {
            AvatarMesh->SetVisibility(bShouldBeVisible);
        }
    }
}

// === MOVEMENT ===

FVector AF12BuilderPawn::GetMovementForward() const
{
    // Get forward direction from camera's actual yaw (ignore pitch for horizontal movement)
    if (SpringArm)
    {
        FRotator CameraRot = SpringArm->GetComponentRotation();
        // Zero out pitch and roll - we only want horizontal direction
        FRotator YawOnly(0, CameraRot.Yaw, 0);
        return YawOnly.Vector();
    }
    
    // Fallback to stored yaw
    float YawRadians = FMath::DegreesToRadians(CameraYaw);
    return FVector(FMath::Cos(YawRadians), FMath::Sin(YawRadians), 0.0f);
}

FVector AF12BuilderPawn::GetMovementRight() const
{
    // Get right direction from camera's actual yaw
    if (SpringArm)
    {
        FRotator CameraRot = SpringArm->GetComponentRotation();
        FRotator YawOnly(0, CameraRot.Yaw, 0);
        return FRotationMatrix(YawOnly).GetScaledAxis(EAxis::Y);
    }
    
    // Fallback to stored yaw
    float YawRadians = FMath::DegreesToRadians(CameraYaw + 90.0f);
    return FVector(FMath::Cos(YawRadians), FMath::Sin(YawRadians), 0.0f);
}

void AF12BuilderPawn::ApplyMovement(float DeltaTime)
{
    if (MovementInput.IsNearlyZero())
    {
        return;
    }

    // Calculate movement direction in world space
    FVector Forward = GetMovementForward();
    FVector Right = GetMovementRight();
    FVector Up = FVector::UpVector;

    FVector DesiredDirection = Forward * MovementInput.X + Right * MovementInput.Y + Up * MovementInput.Z;
    DesiredDirection.Normalize();

    // Apply speed (with sprint multiplier if sprinting)
    float CurrentSpeed = MoveSpeed;
    if (bIsSprinting)
    {
        CurrentSpeed *= SprintMultiplier;
    }

    // Scale speed with zoom level for better control at different scales
    // When zoomed way out, move faster; when zoomed in, move slower
    float ZoomSpeedScale = FMath::Clamp(CameraDistance / 500.0f, 0.5f, 10.0f);
    CurrentSpeed *= ZoomSpeedScale;

    // Add movement input to the movement component
    if (MovementComponent)
    {
        MovementComponent->MaxSpeed = CurrentSpeed;
        AddMovementInput(DesiredDirection, 1.0f);
    }

    // Clear movement input for next frame
    MovementInput = FVector::ZeroVector;
}