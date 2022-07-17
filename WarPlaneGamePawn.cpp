// Copyright Epic Games, Inc. All Rights Reserved.

#include "WarPlaneGamePawn.h"
#include "UObject/ConstructorHelpers.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"

AWarPlaneGamePawn::AWarPlaneGamePawn()
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		ConstructorHelpers::FObjectFinderOptional<UStaticMesh> PlaneMesh;
		FConstructorStatics()
			: PlaneMesh(TEXT("/Game/Flying/Meshes/UFO.UFO"))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	// Create static mesh component
	PlaneMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaneMesh0"));
	PlaneMesh->SetStaticMesh(ConstructorStatics.PlaneMesh.Get());	// Set static mesh
	RootComponent = PlaneMesh;

	// Create a spring arm component
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm0"));
	SpringArm->SetupAttachment(RootComponent);	// Attach SpringArm to RootComponent
	SpringArm->TargetArmLength = 500.0f; // The camera follows at this distance behind the character	
	SpringArm->SocketOffset = FVector(0.f,0.f,60.f);
	SpringArm->bEnableCameraLag = false;	// Do not allow camera to lag
	SpringArm->CameraLagSpeed = 15.f;

	// Create camera component 
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera0"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);	// Attach the camera
	Camera->bUsePawnControlRotation = false; // Don't rotate camera with controller

	// Set handling parameters
	ChangeValue = 40.f;
	Acceleration = 500.f;
	TurnSpeed = 50.f;
	MaxSpeed = 24000.f;
	MinSpeed = 6000.f;
	CurrentForwardSpeed = 10000.f;
	EngineBoostTime = 20.f;

	EngineControl = false;
	OverHeating = false;

	PitchValue = 0.f;
	YawValue = 0.f;
	RollValue = 0.f;
	
	
}

void AWarPlaneGamePawn::Tick(float DeltaSeconds)
{
	const FVector LocalMove = FVector(CurrentForwardSpeed*DeltaSeconds, 0.f, 0.f);

	// Move plan forwards (with sweep so we stop when we collide with things)
	AddActorLocalOffset(LocalMove, true);

	// Calculate change in rotation this frame
	FRotator DeltaRotation(0,0,0);
	DeltaRotation.Pitch = CurrentPitchSpeed * DeltaSeconds;
	DeltaRotation.Yaw = CurrentYawSpeed * DeltaSeconds;
	DeltaRotation.Roll = CurrentRollSpeed * DeltaSeconds;

	// Rotate plane
	AddActorLocalRotation(DeltaRotation);

	// Call any parent class Tick implementation
	Super::Tick(DeltaSeconds);
	
	
}

void AWarPlaneGamePawn::NotifyHit(class UPrimitiveComponent* MyComp, class AActor* Other, class UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit)
{
	Super::NotifyHit(MyComp, Other, OtherComp, bSelfMoved, HitLocation, HitNormal, NormalImpulse, Hit);

	// Deflect along the surface when we collide.
	FRotator CurrentRotation = GetActorRotation();
	SetActorRotation(FQuat::Slerp(CurrentRotation.Quaternion(), HitNormal.ToOrientationQuat(), 0.025f));
}

void AWarPlaneGamePawn::BeginPlay()
{
	Super::BeginPlay();

	EngineBoostTimer();
}


void AWarPlaneGamePawn::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
    // Check if PlayerInputComponent is valid (not NULL)
	check(PlayerInputComponent);
	
	PlayerInputComponent->BindAxis("EngineBoost", this, &AWarPlaneGamePawn::EngineBoostInput);
	PlayerInputComponent->BindAxis("SlowDown", this, &AWarPlaneGamePawn::SlowDownInput);
	PlayerInputComponent->BindAxis("RightRotation", this, &AWarPlaneGamePawn::RightRotationInput);
	//PlayerInputComponent->BindAxis("MoveRight", this, &AWarPlaneGamePawn::MoveRightInput);
}

//void AWarPlaneGamePawn::MoveRightInput(float Val)
//{
/*	// Target yaw speed is based on input
	float TargetYawSpeed = (Val * TurnSpeed);

	// Smoothly interpolate to target yaw speed
	CurrentYawSpeed = FMath::FInterpTo(CurrentYawSpeed, TargetYawSpeed, GetWorld()->GetDeltaSeconds(), 2.f);

	// Is there any left/right input?
	const bool bIsTurning = FMath::Abs(Val) > 0.2f;

	// If turning, yaw value is used to influence roll
	// If not turning, roll to reverse current roll value.
	float TargetRollSpeed = bIsTurning ? (CurrentYawSpeed * 0.5f) : (GetActorRotation().Roll * -2.f);

	// Smoothly interpolate roll speed
	CurrentRollSpeed = FMath::FInterpTo(CurrentRollSpeed, TargetRollSpeed, GetWorld()->GetDeltaSeconds(), 2.f); */
//}

void AWarPlaneGamePawn::EngineBoostInput(float Val)
{
	if(Val != 0 && EngineBoostTime > 1 && OverHeating == false)
	{
		EngineControl = true;
		if(CurrentForwardSpeed < MaxSpeed)
		{
			CurrentForwardSpeed = CurrentForwardSpeed + (Val*ChangeValue);
		}
		else
		{
			CurrentForwardSpeed = MaxSpeed;
		}
	}
	else
	{
		EngineControl = false;
		if(CurrentForwardSpeed > 10000) CurrentForwardSpeed -= ChangeValue;
	}
}

void AWarPlaneGamePawn::SlowDownInput(float Val)
{
	if(Val != 0)
	{
		if(CurrentForwardSpeed > MinSpeed)
		{
			CurrentForwardSpeed = CurrentForwardSpeed - (Val*ChangeValue);
		}
		else
		{
			CurrentForwardSpeed = MinSpeed;
		}
	}
	else
	{
		if(CurrentForwardSpeed < 10000) CurrentForwardSpeed += ChangeValue;
	}
}

void AWarPlaneGamePawn::RightRotationInput(float Val)
{
	FRotator NewRotation = FRotator(GetActorRotation());
	FRotator NewCameraRotation = FRotator(Camera->GetSocketRotation("None"));
	if(Val == 1)
	{
		if(NewRotation.Roll <= 80)
		{
			NewRotation.Yaw = NewRotation.Yaw + (Val/50);
			NewRotation.Roll = NewRotation.Roll + Val;
			SetActorRotation(NewRotation);
			NewCameraRotation.Roll = 0;
			Camera->SetWorldRotation(NewCameraRotation);
		}
	}

	if(Val == -1)
	{
		if(NewRotation.Roll >= -80)
		{
			NewRotation.Yaw = NewRotation.Yaw + (Val/50);
			NewRotation.Roll = NewRotation.Roll + Val;
			SetActorRotation(NewRotation);
			NewCameraRotation.Roll = 0;
			Camera->SetWorldRotation(NewCameraRotation);
		}
	}
	if(Val == 0)
	{
		if(NewRotation.Roll > 1)
		{
			NewRotation.Roll = NewRotation.Roll - 1;
			SetActorRotation(NewRotation);
			NewCameraRotation.Roll = 0;
			NewCameraRotation.Yaw = NewCameraRotation.Yaw + 0.02f;
			Camera->SetWorldRotation(NewCameraRotation);
			GEngine->AddOnScreenDebugMessage(-1,1.0f,FColor::Red,TEXT("sadsa"));
		}
		if(NewRotation.Roll < -1)
		{
			NewRotation.Roll = NewRotation.Roll + 1;
			SetActorRotation(NewRotation);	
			NewCameraRotation.Roll = 0;
			NewCameraRotation.Yaw = NewCameraRotation.Yaw - 0.02f;
			Camera->SetWorldRotation(NewCameraRotation);
			GEngine->AddOnScreenDebugMessage(-1,1.0f,FColor::Red,TEXT("Deneme"));
		}
		
	}
}

void AWarPlaneGamePawn::EngineBoostTimer()
{
	if(EngineBoostTime <= 19.9f && EngineControl == false)
	{
		EngineBoostTime = EngineBoostTime + 0.1f;
		
	}
	if(EngineBoostTime >= 1.f && EngineControl)
	{
		EngineBoostTime = EngineBoostTime - 0.1f;
		if(EngineBoostTime <= 1.1)
		{
			OverHeating = true;
			GetWorldTimerManager().SetTimer(Timer2, this, &AWarPlaneGamePawn::EngineOverHeat, 19.f, true);
			
		}
	}
	
	GetWorldTimerManager().SetTimer(Timer, this, &AWarPlaneGamePawn::EngineBoostTimer, 0.1f, true);
}

void AWarPlaneGamePawn::EngineOverHeat()
{
	OverHeating = false;
}


