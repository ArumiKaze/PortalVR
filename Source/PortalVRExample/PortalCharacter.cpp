#include "PortalCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"

APortalCharacter::APortalCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	VROrigin = CreateDefaultSubobject<USceneComponent>(TEXT("VROrigin"));
	VROrigin->SetupAttachment(RootComponent);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(VROrigin);
}

void APortalCharacter::BeginPlay()
{
	Super::BeginPlay();
}

void APortalCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void APortalCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}