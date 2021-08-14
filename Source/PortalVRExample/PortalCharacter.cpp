// Fill out your copyright notice in the Description page of Project Settings.


#include "PortalCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"

// Sets default values
APortalCharacter::APortalCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	VROrigin = CreateDefaultSubobject<USceneComponent>(TEXT("VROrigin"));
	VROrigin->SetupAttachment(RootComponent);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(VROrigin);

	needFixOrientation = false;
	orientationCorrectionTime = 1.8f;
}

// Called when the game starts or when spawned
void APortalCharacter::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void APortalCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (needFixOrientation)
	{
		ReturnToOrientation();
	}
}

// Called to bind functionality to input
void APortalCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

void APortalCharacter::PortalTeleport()
{
	orientationStart = GetWorld()->GetTimeSeconds();
	needFixOrientation = true;
}

void APortalCharacter::ReturnToOrientation()
{
	float alpha = (GetWorld()->GetTimeSeconds() - orientationStart) / orientationCorrectionTime;
	FRotator currentOrientation = GetCapsuleComponent()->GetComponentRotation();
	FQuat target = FRotator(0.0f, currentOrientation.Yaw, 0.0f).Quaternion();
	FQuat newOrientation = FQuat::Slerp(currentOrientation.Quaternion(), target, alpha);
	GetCapsuleComponent()->SetWorldRotation(newOrientation.Rotator(), false, nullptr, ETeleportType::TeleportPhysics);
	if (alpha >= 1.0f)
	{
		needFixOrientation = false;
	}
}