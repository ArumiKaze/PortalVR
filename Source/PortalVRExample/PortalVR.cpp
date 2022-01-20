#include "PortalVR.h"
#include "PortalCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/CanvasRenderTarget2D.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HelperMacros.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"

APortalVR::APortalVR()
	:prevCameraLocation{ 0 }, captureCameraClippingPlaneOffset{ -10.0f }
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.TickGroup = ETickingGroup::TG_PostUpdateWork;

	secondaryPostPhysicsTick.bCanEverTick = false;
	secondaryPostPhysicsTick.refPortal = this;
	secondaryPostPhysicsTick.TickGroup = TG_PostPhysics;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultSceneComponent"));
	RootComponent->Mobility = EComponentMobility::Static;

	portalMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PortalMesh"));
	portalMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	portalMesh->SetCollisionObjectType(ECC_Portal);
	portalMesh->CastShadow = false;
	portalMesh->SetupAttachment(RootComponent);

	//Scene capture component for left eye
	portalLeftCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("PortalLeftCapture"));
	portalLeftCapture->bEnableClipPlane = true;
	portalLeftCapture->bUseCustomProjectionMatrix = false;
	portalLeftCapture->bCaptureEveryFrame = false;
	portalLeftCapture->bCaptureOnMovement = false;
	portalLeftCapture->LODDistanceFactor = 3.0f;
	portalLeftCapture->TextureTarget = nullptr;
	portalLeftCapture->CaptureSource = ESceneCaptureSource::SCS_SceneColorSceneDepth;
	portalLeftCapture->bAlwaysPersistRenderingState = true;
	portalLeftCapture->bOverride_CustomNearClippingPlane = true;
	portalLeftCapture->bUseCustomProjectionMatrix = true;
	portalLeftCapture->SetupAttachment(RootComponent);

	//Scene capture component for right eye
	portalRightCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("PortalRightCapture"));
	portalRightCapture->bEnableClipPlane = true;
	portalRightCapture->bUseCustomProjectionMatrix = false;
	portalRightCapture->bCaptureEveryFrame = false;
	portalRightCapture->bCaptureOnMovement = false;
	portalRightCapture->LODDistanceFactor = 3.0f;
	portalRightCapture->TextureTarget = nullptr;
	portalRightCapture->CaptureSource = ESceneCaptureSource::SCS_SceneColorSceneDepth;
	portalRightCapture->bAlwaysPersistRenderingState = true;
	portalRightCapture->bOverride_CustomNearClippingPlane = true;
	portalRightCapture->bUseCustomProjectionMatrix = true;
	portalRightCapture->SetupAttachment(RootComponent);

	//Default resolution for the portal texture is max resolution for most immersion
	resolutionScale = 1.0f;
}

void APortalVR::BeginPlay()
{
	Super::BeginPlay();

	Init();
}

void APortalVR::Init()
{
	portalPlayerController = GetWorld()->GetFirstPlayerController();
	portalPlayerLocal = portalPlayerController->GetLocalPlayer();
	portalPlayerCharacter = Cast<APortalCharacter>(portalPlayerController->GetCharacter());

	portalLeftCapture->PostProcessSettings = portalPlayerCharacter->Camera->PostProcessSettings;
	portalRightCapture->PostProcessSettings = portalPlayerCharacter->Camera->PostProcessSettings;

	CreatePortalTexturesAndMaterial();

	//Asserting these here as these should never be missing/null before starting tick
	check(portalPlayerController && portalPlayerLocal && portalPlayerCharacter && renderLeftTarget && renderRightTarget && portalMaterial);

	//Not the end of the world, but output a warning in logs when the portal is not connected to another portal and destroys the actor since we can't render anything to this portal without the other portal
	if (ensureMsgf(!portalTarget, TEXT("Warning: Portal is not connected to another portal")))
	{
		Destroy();
	}

	//Activate our primary tick and our secondary tick now that everything is ready
	PrimaryActorTick.bCanEverTick = true;
	secondaryPostPhysicsTick.bCanEverTick = true;
	secondaryPostPhysicsTick.RegisterTickFunction(GetWorld()->PersistentLevel);
}

//Remember that this is called because we overrided FActorTickFunction's tick. We can call APortalVR's private member function UpdateCharacterTracking() because FPostPhysicsTick is a friend of APortalVR
void FPostPhysicsTick::ExecuteTick(float DeltaTime, enum ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	refPortal->UpdateCharacterTracking();
}

//Note that the primary tick comes directly after the secondary tick in terms of order
void APortalVR::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdatePortalView();

	//Don't apply offset to the portal material if the player is far from the portal. Note that the distance is determined by the width of the portal, factoring in scaling too
	if (FVector::Distance(portalPlayerCharacter->Camera->GetComponentLocation(), portalMesh->GetComponentLocation()) < portalMesh->GetStaticMesh()->GetBoundingBox().Max.Y * portalMesh->GetComponentScale().Y)
	{
		portalMaterial->SetScalarParameterValue("ScaleOffset", 1.0f);
	}
	else
	{
		portalMaterial->SetScalarParameterValue("ScaleOffset", 0.0f);
	}
}

void APortalVR::CreatePortalTexturesAndMaterial()
{
	//we get the current viewport size here and multiply it by our scalar to determine our XY resolution for the portal texture
	int viewportX, viewportY;
	portalPlayerController->GetViewportSize(viewportX, viewportY);
	viewportX *= resolutionScale;
	viewportY *= resolutionScale;

	renderLeftTarget = UCanvasRenderTarget2D::CreateCanvasRenderTarget2D(GetWorld(), UCanvasRenderTarget2D::StaticClass(), viewportX, viewportY);
	renderRightTarget = UCanvasRenderTarget2D::CreateCanvasRenderTarget2D(GetWorld(), UCanvasRenderTarget2D::StaticClass(), viewportX, viewportY);

	//using a dynamic material instance so we can assign the instantiated render targets to the material's textures
	portalMaterial = portalMesh->CreateDynamicMaterialInstance(0, portalMaterialInterface);
	portalMaterial->SetTextureParameterValue("RT_LeftEye", renderLeftTarget);
	portalMaterial->SetTextureParameterValue("RT_RightEye", renderRightTarget);

	portalLeftCapture->TextureTarget = renderLeftTarget;
	portalRightCapture->TextureTarget = renderRightTarget;
}

void APortalVR::UpdatePortalView()
{
	//we enabled and update a clip plane for our cameras so that objects/walls won't be rendered between the cameras and the portal
	portalLeftCapture->ClipPlaneNormal = portalTarget->GetActorForwardVector();
	portalLeftCapture->ClipPlaneBase = portalTarget->GetActorLocation() + portalLeftCapture->ClipPlaneNormal * captureCameraClippingPlaneOffset;
	portalRightCapture->ClipPlaneNormal = portalTarget->GetActorForwardVector();
	portalRightCapture->ClipPlaneBase = portalTarget->GetActorLocation() + portalRightCapture->ClipPlaneNormal * captureCameraClippingPlaneOffset;

	//we are overriding the camera capture projection with a custom one based on the player's viewport
	FMatrix projectionMatrix;
	FSceneViewProjectionData projectionData;
	portalPlayerLocal->GetProjectionData(portalPlayerLocal->ViewportClient->Viewport, EStereoscopicPass::eSSP_RIGHT_EYE, projectionData);
	portalRightCapture->CustomProjectionMatrix = projectionData.ProjectionMatrix;
	portalPlayerLocal->GetProjectionData(portalPlayerLocal->ViewportClient->Viewport, EStereoscopicPass::eSSP_LEFT_EYE, projectionData);
	portalLeftCapture->CustomProjectionMatrix = projectionData.ProjectionMatrix;

	//Move and rotate the cameras so that they match the player's perspective through the exit/target portal
	FVector newCameraLocation;
	FRotator newCameraRotation;
	ConvertLocationRotationToPortal(newCameraLocation, newCameraRotation, portalPlayerCharacter->Camera->GetComponentTransform());
	portalLeftCapture->SetWorldLocationAndRotation(newCameraLocation, newCameraRotation);
	portalRightCapture->SetWorldLocationAndRotation(newCameraLocation, newCameraRotation);

	portalLeftCapture->CaptureScene();
	portalRightCapture->CaptureScene();
}

bool APortalVR::WasActorInFrontOfPortal(const FVector& location) const
{
	FVector directionPlayerMovement{ (location - GetActorLocation()).GetSafeNormal() };
	return FVector::DotProduct(directionPlayerMovement, GetActorForwardVector()) >= 0.0f;
}

void APortalVR::UpdateCharacterTracking()
{
	//We first check whether the VR headset's current location this current frame makes it clip through the portal plane by comparing it to the VR headset's location in the previous frame
	FPlane portalPlane{ portalMesh->GetComponentLocation(), portalMesh->GetForwardVector() };
	FVector currentCameraLocation{ portalPlayerCharacter->Camera->GetComponentLocation() };
	FVector pointInterscetion;
	bool passedThroughPlane = FMath::SegmentPlaneIntersection(prevCameraLocation, currentCameraLocation, portalPlane, pointInterscetion);

	/*
	 * Gets relative location of where the headset intersected with the portal plane relative to the portal mesh itself.
	 * Then check whether the relative location exceeds the size of the portal mesh (with mesh scaling in mind if size was changed via scale by someone)
	 */
	FVector relativeIntersection = portalMesh->GetComponentTransform().InverseTransformPositionNoScale(pointInterscetion);
	FVector portalSize { 0.0f, portalMesh->GetStaticMesh()->GetBoundingBox().Max.Y * portalMesh->GetComponentScale().Y, portalMesh->GetStaticMesh()->GetBoundingBox().Max.Z * portalMesh->GetComponentScale().Z };
	bool passedWithinPortal{ FMath::Abs(relativeIntersection.Z) <= portalSize.Z && FMath::Abs(relativeIntersection.Y) <= portalSize.Y };

	//Along with the other two checks we did, also double check whether the actor was indeed in front of the portal and didn't enter from behind the portal
	if (passedThroughPlane && passedWithinPortal && WasActorInFrontOfPortal(prevCameraLocation))
	{
		TeleportCharacter();
	}

	prevCameraLocation = portalPlayerCharacter->Camera->GetComponentLocation();
}

void APortalVR::TeleportCharacter()
{
	FVector newLocation;
	FRotator newRotation;
	ConvertLocationRotationToPortal(newLocation, newRotation, portalPlayerCharacter->GetActorTransform());

	portalPlayerCharacter->GetRootComponent()->SetWorldLocationAndRotation(newLocation, newRotation, false, nullptr, ETeleportType::TeleportPhysics);

	/* 
	 * Translate the old velocity right before the character teleports to a new velocity that takes in account the new direction of the character after teleporting
	 * If we don't do this, it will keep the old velocity/momentum after the character teleports and it will move with the old momentum very briefly after teleporting causing inconsistent movement
	 */
	FVector lastVelocity{ -portalPlayerCharacter->GetVelocity() };
	FVector relativeVelocity{ UKismetMathLibrary::Dot_VectorVector(lastVelocity, GetActorForwardVector()), UKismetMathLibrary::Dot_VectorVector(lastVelocity, GetActorRightVector()), UKismetMathLibrary::Dot_VectorVector(lastVelocity, GetActorUpVector()) };
	FVector newVelocity{ relativeVelocity.X * portalTarget->GetActorForwardVector() + relativeVelocity.Y * portalTarget->GetActorRightVector() + relativeVelocity.Z * portalTarget->GetActorUpVector() };
	portalPlayerCharacter->GetCharacterMovement()->Velocity = newVelocity;

	//"Turn on" the exit portal so that it updates the portal the same frame you teleport
	portalTarget->portalMaterial->SetScalarParameterValue("ScaleOffset", 1.0f);
	portalTarget->UpdatePortalView();
	portalTarget->prevCameraLocation = portalPlayerCharacter->Camera->GetComponentLocation();
}

void APortalVR::ConvertLocationRotationToPortal(FVector& newLocation, FRotator& newRotator, const FTransform& actorTransform) const
{
	FMatrix actorRelativeMatrix{ actorTransform.ToMatrixWithScale() * GetActorTransform().ToMatrixWithScale().Inverse() };	//Get world to local position (matrix) of our actor/player/camera relative to this portal
	actorRelativeMatrix *= FRotationMatrix{ FRotator{ 0.0f, 180.0f, 0.0f } };												//Flip the matrix on the Yaw axis because the position should be mirrored for the other portal
	FMatrix actorWorldMatrix{ actorRelativeMatrix * portalTarget->GetActorTransform().ToMatrixWithScale() };				//Get local to world position (matrix) of our actor/player/camera relative to the target/exit portal

	//return output via reference
	newLocation = actorWorldMatrix.GetOrigin();
	newRotator = actorWorldMatrix.Rotator();
}