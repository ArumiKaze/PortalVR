#include "PortalVR.h"
#include "HelperMacros.h"
#include "PortalCharacter.h"
#include "PortalLocalPlayer.h"
#include "Components/BoxComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/CapsuleComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/CanvasRenderTarget2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "GameFramework/Character.h"


//Constructor
APortalVR::APortalVR()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = ETickingGroup::TG_PostUpdateWork;

	//Root component/default scene component
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultSceneComponent"));
	RootComponent->Mobility = EComponentMobility::Static;

	//Portal screen mesh
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
	portalRightCapture->SetupAttachment(RootComponent);

	physicsTick.bCanEverTick = false;
	physicsTick.refPortal = this;
	physicsTick.TickGroup = TG_PostPhysics;

	resolutionPercentile = 1.0f;
	recursionAmount = 1;
}

void APortalVR::BeginPlay()
{
	Super::BeginPlay();

	PrimaryActorTick.SetTickFunctionEnable(false);
	FTimerHandle timer;
	FTimerDelegate timerDel;
	timerDel.BindUFunction(this, "Init");
	GetWorldTimerManager().SetTimer(timer, timerDel, 1.0f, false, 1.0f);
	//Init();
}

void APortalVR::Init()
{
	GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, TEXT("Some debug message!"));

	portalPlayerController = GetWorld()->GetFirstPlayerController();
	portalPlayerLocal = Cast<UPortalLocalPlayer>(portalPlayerController->GetLocalPlayer());
	portalPlayerCharacter = Cast<APortalCharacter>(portalPlayerController->GetCharacter());

	CreatePortalTextures();

	if (!portalTarget || !portalPlayerController || !portalPlayerLocal || !portalPlayerCharacter || !renderLeftTarget || !renderRightTarget || !portalMaterial)
	{
		Destroy();
	}

	physicsTick.bCanEverTick = true;
	physicsTick.RegisterTickFunction(GetWorld()->PersistentLevel);

	prevCameraLocation = portalPlayerCharacter->Camera->GetComponentLocation();

	PrimaryActorTick.SetTickFunctionEnable(true);
}

void APortalVR::PostPhysicsTick(float DeltaTime)
{
	UpdateCharacterTracking();
}

void FPostPhysicsTick::ExecuteTick(float DeltaTime, enum ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	refPortal->PostPhysicsTick(DeltaTime);
}

void APortalVR::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	portalMaterial->SetScalarParameterValue("ScaleOffset", 0.0f);
	ClearPortalView();

	UpdatePortalView();

	if (FVector::Distance(portalPlayerCharacter->Camera->GetComponentLocation(), portalMesh->GetComponentLocation()) < portalMesh->GetStaticMesh()->GetBoundingBox().Max.Y * portalMesh->GetComponentScale().Y)
	{
		portalMaterial->SetScalarParameterValue("ScaleOffset", 1.0f);
	}
}

void APortalVR::CreatePortalTextures()
{
	int viewportX, viewportY;
	portalPlayerController->GetViewportSize(viewportX, viewportY);
	viewportX *= resolutionPercentile;
	viewportY *= resolutionPercentile;

	renderLeftTarget = nullptr;
	renderRightTarget = nullptr;
	renderLeftTarget = UCanvasRenderTarget2D::CreateCanvasRenderTarget2D(GetWorld(), UCanvasRenderTarget2D::StaticClass(), viewportX, viewportY);
	renderRightTarget = UCanvasRenderTarget2D::CreateCanvasRenderTarget2D(GetWorld(), UCanvasRenderTarget2D::StaticClass(), viewportX, viewportY);

	portalMaterial = portalMesh->CreateDynamicMaterialInstance(0, portalMaterialInstance);
	portalMaterial->SetTextureParameterValue("RT_LeftEye", renderLeftTarget);
	portalMaterial->SetTextureParameterValue("RT_RightEye", renderRightTarget);

	portalLeftCapture->TextureTarget = renderLeftTarget;
	portalRightCapture->TextureTarget = renderRightTarget;
}

bool APortalVR::IsActorInFrontOfPortal(FVector location)
{
	FVector direction = (location - GetActorLocation()).GetSafeNormal();
	float dotProduct = FVector::DotProduct(direction, GetActorForwardVector());
	return dotProduct >= 0.0f;
}

void APortalVR::UpdatePortalView()
{
	portalLeftCapture->PostProcessSettings = portalPlayerCharacter->Camera->PostProcessSettings;
	portalRightCapture->PostProcessSettings = portalPlayerCharacter->Camera->PostProcessSettings;

	portalLeftCapture->bEnableClipPlane = true;
	portalLeftCapture->bOverride_CustomNearClippingPlane = true;
	portalLeftCapture->ClipPlaneNormal = portalTarget->portalMesh->GetForwardVector();
	portalLeftCapture->ClipPlaneBase = portalTarget->portalMesh->GetComponentLocation() - portalLeftCapture->ClipPlaneNormal;

	portalRightCapture->bEnableClipPlane = true;
	portalRightCapture->bOverride_CustomNearClippingPlane = true;
	portalRightCapture->ClipPlaneNormal = portalTarget->portalMesh->GetForwardVector();
	portalRightCapture->ClipPlaneBase = portalTarget->portalMesh->GetComponentLocation() - portalRightCapture->ClipPlaneNormal;

	portalRightCapture->bUseCustomProjectionMatrix = true;
	portalRightCapture->CustomProjectionMatrix = portalPlayerLocal->GetCameraProjectionMatrix(EStereoscopicPass::eSSP_RIGHT_EYE);
	portalLeftCapture->bUseCustomProjectionMatrix = true;
	portalLeftCapture->CustomProjectionMatrix = portalPlayerLocal->GetCameraProjectionMatrix(EStereoscopicPass::eSSP_LEFT_EYE);

	FVector newCameraLocation = ConvertLocationToPortal(portalPlayerCharacter->Camera->GetComponentLocation(), this, portalTarget);
	FRotator newCameraRotation = ConvertRotationToPortal(portalPlayerCharacter->Camera->GetComponentRotation(), this, portalTarget);

	for (int i = recursionAmount; i >= 0.0f; --i)
	{
		FVector recursiveCameraLocation = newCameraLocation;
		FRotator recursiveCameraRotation = newCameraRotation;

		for (int j = 0; j < i; ++j)
		{
			recursiveCameraLocation = ConvertLocationToPortal(recursiveCameraLocation, this, portalTarget);
			recursiveCameraRotation = ConvertRotationToPortal(recursiveCameraRotation, this, portalTarget);
		}

		portalLeftCapture->SetWorldLocationAndRotation(recursiveCameraLocation, recursiveCameraRotation);
		portalRightCapture->SetWorldLocationAndRotation(recursiveCameraLocation, recursiveCameraRotation);

		if (i == recursionAmount)
		{
			portalMesh->SetVisibility(false);
		}

		portalLeftCapture->CaptureScene();
		portalRightCapture->CaptureScene();

		if (i == recursionAmount) {
			portalMesh->SetVisibility(true);
		}
	}
}

void APortalVR::ClearPortalView()
{
	UKismetRenderingLibrary::ClearRenderTarget2D(GetWorld(), renderLeftTarget);
	UKismetRenderingLibrary::ClearRenderTarget2D(GetWorld(), renderRightTarget);
}

void APortalVR::UpdateCharacterTracking()
{
	FVector currentCameraLocation = portalPlayerCharacter->Camera->GetComponentLocation();
	if (currentCameraLocation.ContainsNaN())
	{
		return;
	}
	FVector pointInterscetion;
	FPlane portalPlane = FPlane(portalMesh->GetComponentLocation(), portalMesh->GetForwardVector());
	bool passedThroughPlane = FMath::SegmentPlaneIntersection(prevCameraLocation, currentCameraLocation, portalPlane, pointInterscetion);
	FVector relativeIntersection = portalMesh->GetComponentTransform().InverseTransformPositionNoScale(pointInterscetion);
	FVector portalSize { 0.0f, portalMesh->GetStaticMesh()->GetBoundingBox().Max.Y * portalMesh->GetComponentScale().Y, portalMesh->GetStaticMesh()->GetBoundingBox().Max.Z * portalMesh->GetComponentScale().Z };
	bool passedWithinPortal = FMath::Abs(relativeIntersection.Z) <= portalSize.Z && FMath::Abs(relativeIntersection.Y) <= portalSize.Y;

	if (passedThroughPlane && passedWithinPortal && IsActorInFrontOfPortal(prevCameraLocation))
	{
		TeleportActor(portalPlayerCharacter);
	}

	prevCameraLocation = portalPlayerCharacter->Camera->GetComponentLocation();
}

void APortalVR::TeleportActor(AActor* actor)
{
	if (!actor)
	{
		return;
	}

	FVector newLocation = ConvertLocationToPortal(actor->GetActorLocation(), this, portalTarget);
	FRotator newRotation = ConvertRotationToPortal(actor->GetActorRotation(), this, portalTarget);
	actor->GetRootComponent()->SetWorldLocationAndRotation(newLocation, newRotation, false, nullptr, ETeleportType::TeleportPhysics);

	portalTarget->portalMaterial->SetScalarParameterValue("ScaleOffset", 1.0f);
	portalTarget->UpdatePortalView();
	portalTarget->prevCameraLocation = portalPlayerCharacter->Camera->GetComponentLocation();
}

FVector APortalVR::ConvertLocationToPortal(FVector location, APortalVR* currentPortal, APortalVR* endPortal, bool flip)
{
	FVector relativeLocationToPortal = currentPortal->portalMesh->GetComponentTransform().InverseTransformPositionNoScale(location);
	if (flip)
	{
		relativeLocationToPortal.X *= -1.0f;
		relativeLocationToPortal.Y *= -1.0f;
	}
	FVector newWorldLocation = endPortal->portalMesh->GetComponentTransform().TransformPositionNoScale(relativeLocationToPortal);

	return newWorldLocation;
}

FRotator APortalVR::ConvertRotationToPortal(FRotator rotation, APortalVR* currentPortal, APortalVR* endPortal, bool flip)
{
	FRotator relativeRotationToPortal = currentPortal->portalMesh->GetComponentTransform().InverseTransformRotation(rotation.Quaternion()).Rotator();
	if (flip)
	{
		relativeRotationToPortal.Yaw += 180.0f;
	}
	FRotator newWorldRotation = endPortal->portalMesh->GetComponentTransform().TransformRotation(relativeRotationToPortal.Quaternion()).Rotator();

	return newWorldRotation;
}