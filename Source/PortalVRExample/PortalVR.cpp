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
 	//Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
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

	//Portal box collision to detect actors
	portalBoxCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("PortalBoxCollision"));
	portalBoxCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	portalBoxCollision->SetUseCCD(true);
	portalBoxCollision->SetCollisionProfileName("Portal");
	portalBoxCollision->SetupAttachment(portalMesh);

	//Scene capture component for left eye
	portalLeftCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("PortalLeftCapture"));
	portalLeftCapture->bEnableClipPlane = true;
	portalLeftCapture->bUseCustomProjectionMatrix = false;
	portalLeftCapture->bCaptureEveryFrame = false;
	portalLeftCapture->bCaptureOnMovement = false;
	portalLeftCapture->LODDistanceFactor = 3.0f;
	portalLeftCapture->TextureTarget = nullptr;
	portalLeftCapture->CaptureSource = ESceneCaptureSource::SCS_SceneColorSceneDepth;
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
	portalRightCapture->SetupAttachment(RootComponent);

	physicsTick.bCanEverTick = false;
	physicsTick.Target = this;
	physicsTick.TickGroup = TG_PostPhysics;

	resolutionPercentile = 1.0f;
	recursionAmount = 1;
}

void APortalVR::BeginPlay()
{
	Super::BeginPlay();

	GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, TEXT("30"));
	
	Init();
}

void APortalVR::Init()
{
	portalPlayerController = GetWorld()->GetFirstPlayerController();
	portalPlayerCharacter = Cast<APortalCharacter>(portalPlayerController->GetCharacter());

	CreatePortalTextures();

	if (!portalTarget || !portalPlayerController || !portalPlayerCharacter || !renderLeftTarget || !renderRightTarget || !portalMaterial)
	{
		Destroy();
	}

	physicsTick.bCanEverTick = true;
	physicsTick.RegisterTickFunction(GetWorld()->PersistentLevel);

	prevCameraLocation = portalPlayerCharacter->Camera->GetComponentLocation();

	portalBoxCollision->OnComponentBeginOverlap.AddDynamic(this, &APortalVR::OnPortalBoxBeginOverlap);
	portalBoxCollision->OnComponentEndOverlap.AddDynamic(this, &APortalVR::OnPortalBoxEndOverlap);
	portalMesh->OnComponentBeginOverlap.AddDynamic(this, &APortalVR::OnPortalMeshBeginOverlap);
	portalMesh->OnComponentEndOverlap.AddDynamic(this, &APortalVR::OnPortalMeshEndOverlap);

	TSet<AActor*> overlappingActors;
	portalBoxCollision->GetOverlappingActors(overlappingActors);
	if (overlappingActors.Num() > 0)
	{
		for (auto& overlappedActorElement: overlappingActors)
		{
			USceneComponent* overlappedRootComponent = overlappedActorElement->GetRootComponent();
			if (overlappedRootComponent && overlappedRootComponent->IsSimulatingPhysics())
			{
				if (!mapTrackedActors.Contains(overlappedActorElement) && IsInFront(overlappedRootComponent->GetComponentLocation()))
				{
					AddTrackedActor(overlappedActorElement);
				}
			}
		}
	}

	PrimaryActorTick.SetTickFunctionEnable(true);
}

void APortalVR::PostPhysicsTick(float DeltaTime)
{
	//Check if the character has passed through this portal
	UpdateCharacterTracking();

	// Update tracked actors post physics...
	UpdateTrackedActors();
}

void FPostPhysicsTick::ExecuteTick(float DeltaTime, enum ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	//Call the Portals second tick function for running post tick.
	if (Target)
	{
		Target->PostPhysicsTick(DeltaTime);
	}
}

void APortalVR::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	portalMaterial->SetScalarParameterValue("ScaleOffset", 0.0f);
	ClearPortalView();

	UpdatePortalView();

	if (LocationInsidePortal(portalPlayerCharacter->Camera->GetComponentLocation()))
	{
		portalMaterial->SetScalarParameterValue("ScaleOffset", 1.0f);
	}
}

void APortalVR::OnPortalBoxBeginOverlap(UPrimitiveComponent* portalMeshHit, AActor* overlappedActor, UPrimitiveComponent* overlappedComp, int32 otherBodyIndex, bool fromSweep, const FHitResult& portalHit)
{
	USceneComponent* overlappedRootComponent = overlappedActor->GetRootComponent();
	if (overlappedRootComponent && overlappedRootComponent->IsSimulatingPhysics())
	{
		if (!mapTrackedActors.Contains(overlappedActor) && IsInFront(overlappedRootComponent->GetComponentLocation()))
		{
			AddTrackedActor(overlappedActor);
		}
	}
}

void APortalVR::OnPortalBoxEndOverlap(UPrimitiveComponent* portalMeshHit, AActor* overlappedActor, UPrimitiveComponent* overlappedComp, int32 otherBodyIndex)
{
	if (mapTrackedActors.Contains(overlappedActor))
	{
		RemoveTrackedActor(overlappedActor);
	}
}

void APortalVR::OnPortalMeshBeginOverlap(UPrimitiveComponent* portalMeshHit, AActor* overlappedActor, UPrimitiveComponent* overlappedComp, int32 otherBodyIndex, bool fromSweep, const FHitResult& portalHit)
{
	if (mapTrackedActors.Contains(overlappedActor))
	{
		AActor* dupeActor = mapTrackedActors[overlappedActor].trackedDuplicate;
		if (dupeActor)
		{
			HideActor(dupeActor, false);
		}
	}
}

void APortalVR::OnPortalMeshEndOverlap(UPrimitiveComponent* portalMeshHit, AActor* overlappedActor, UPrimitiveComponent* overlappedComp, int32 otherBodyIndex)
{
	if (mapTrackedActors.Contains(overlappedActor))
	{
		AActor* dupeActor = mapTrackedActors[overlappedActor].trackedDuplicate;
		if (dupeActor)
		{
			HideActor(dupeActor);
		}
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

bool APortalVR::IsInFront(FVector location)
{
	FVector direction = (location - GetActorLocation()).GetSafeNormal();
	float dotProduct = FVector::DotProduct(direction, GetActorForwardVector());
	return dotProduct >= 0.0f;
}

void APortalVR::AddTrackedActor(AActor* actorToAdd)
{
	if (!actorToAdd)
	{
		return;
	}

	// NOTE: If its the pawn track the camera otherwise track the root component...
	FTrackedActor track = FTrackedActor(actorToAdd->GetRootComponent());
	track.lastTrackedOrigin = actorToAdd->GetActorLocation();

	mapTrackedActors.Add(actorToAdd, track);
	//actorsBeingTracked++;
	
	CopyActor(actorToAdd);
}

void APortalVR::RemoveTrackedActor(AActor* actorToRemove)
{
	if (!actorToRemove)
	{
		return;
	}

	DeleteCopy(actorToRemove);

	mapTrackedActors.Remove(actorToRemove);
	//actorsBeingTracked--;
}

void APortalVR::UpdateTrackedActors()
{
	if (mapTrackedActors.Num() > 0)
	{
		TArray<AActor*> teleportedActors;
		for (TMap<AActor*, FTrackedActor>::TIterator trackedActor = mapTrackedActors.CreateIterator(); trackedActor; ++trackedActor)
		{
			// Update the positions for the duplicate tracked actors at the target portal. NOTE: Only if it isn't null.
			AActor* isValid = trackedActor->Value.trackedDuplicate;
			if (isValid && isValid->IsValidLowLevel())
			{
				FVector convertedLoc = ConvertLocationToPortal(trackedActor->Key->GetActorLocation(), this, portalTarget);
				FRotator convertedRot = ConvertRotationToPortal(trackedActor->Key->GetActorRotation(), this, portalTarget);
				isValid->SetActorLocationAndRotation(convertedLoc, convertedRot);
			}

			// If its the player skip this next part as its handled in UpdatePawnTracking.
			// NOTE: Still want to track the actor and position duplicate mesh...
			if (APortalCharacter * refPlayer = Cast<APortalCharacter>(trackedActor->Key)) continue;

			// Check for when the actors origin passes to the other side of the portal between frames.
			FVector currLocation = trackedActor->Value.trackedComp->GetComponentLocation();
			FVector pointInterscetion;
			FPlane portalPlane = FPlane(portalMesh->GetComponentLocation(), portalMesh->GetForwardVector());
			bool passedThroughPortal = FMath::SegmentPlaneIntersection(trackedActor->Value.lastTrackedOrigin, currLocation, portalPlane, pointInterscetion);
			if (passedThroughPortal)
			{
				// Teleport the actor.
				// NOTE: If actor is simulating physics and has 
				// CCD it will effect physics objects around it when moved.
				TeleportObject(trackedActor->Key);

				// Add to be removed.
				teleportedActors.Add(trackedActor->Key);

				// Skip to next actor once removed.
				continue;
			}

			// Update last tracked origin.
			trackedActor->Value.lastTrackedOrigin = currLocation;
		}

		// Ensure the tracked actor has been removed.
		// Ensure the tracked actor has been added to target portal as its been teleported there.
		// Ensure that the duplicate mesh at this portal spawned by the target portal is not hidden from the render pass.
		for (AActor* actor : teleportedActors)
		{
			if (!actor || !actor->IsValidLowLevelFast()) continue;
			if (mapTrackedActors.Contains(actor)) RemoveTrackedActor(actor);
			if (!portalTarget->mapTrackedActors.Contains(actor)) portalTarget->AddTrackedActor(actor);
			if (AActor * hasDuplicate = portalTarget->mapTrackedActors.FindRef(actor).trackedDuplicate) HideActor(hasDuplicate, false);
		}
	}
}

void APortalVR::CopyActor(AActor* actorToCopy)
{
	FName newActorName = MakeUniqueObjectName(this, AActor::StaticClass(), "CopiedActor");
	AActor* newActor = NewObject<AActor>(this, newActorName, RF_NoFlags, actorToCopy);
	TArray<UActorComponent*> actorStaticMeshes;
	newActor->GetComponents(actorStaticMeshes);
	newActor->RegisterAllComponents();

	ACharacter* refCharacter = Cast<ACharacter>(newActor);
	if (refCharacter)
	{
		refCharacter->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_PortalBox, ECR_Ignore);
		refCharacter->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		refCharacter->GetCapsuleComponent()->SetSimulatePhysics(false);
		refCharacter->PrimaryActorTick.SetTickFunctionEnable(false);
	}
	else
	{
		for (auto& actorComponentElement: actorStaticMeshes)
		{
			UStaticMeshComponent* staticMeshComponent = Cast<UStaticMeshComponent>(actorComponentElement);
			staticMeshComponent->SetCollisionResponseToChannel(ECC_PortalBox, ECR_Ignore);
			staticMeshComponent->SetCollisionResponseToChannel(ECC_Interactable, ECR_Ignore);
			staticMeshComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
			staticMeshComponent->SetSimulatePhysics(false);
		}
	}

	mapTrackedActors[actorToCopy].trackedDuplicate = newActor;
	//FTrackedActor newTrack = mapTrackedActors.FindRef(actorToCopy);
	//newTrack.trackedDuplicate = newActor;
	//mapTrackedActors.Remove(actorToCopy);
	//mapTrackedActors.Add(actorToCopy, newTrack);

	FVector newLocation = ConvertLocationToPortal(newActor->GetActorLocation(), this, portalTarget);
	FRotator newRotation = ConvertRotationToPortal(newActor->GetActorRotation(), this, portalTarget);
	newActor->SetActorLocationAndRotation(newLocation, newRotation);

	//duplicateMap.Add(newActor, actorToCopy);

	HideActor(newActor);
}

void APortalVR::DeleteCopy(AActor* actorToDelete)
{
	if (mapTrackedActors.Contains(actorToDelete))
	{
		AActor* dupeActor = mapTrackedActors.FindRef(actorToDelete).trackedDuplicate;
		if (dupeActor)
		{
			//duplicateMap.Remove(dupeActor);
			if (dupeActor->IsValidLowLevel() && !dupeActor->IsPendingKillOrUnreachable() && !dupeActor->IsPendingKillPending())
			{
				if (GetWorld())
				{
					GetWorld()->DestroyActor(dupeActor);
					dupeActor = nullptr;
					GEngine->ForceGarbageCollection();
				}
			}
		}
	}
}

void APortalVR::HideActor(AActor* actor, bool hide)
{
	if (actor->IsValidLowLevel())
	{
		TArray<UActorComponent*> components;
		actor->GetComponents(components);
		for (auto componentElement: components)
		{
			UStaticMeshComponent* staticMeshComponent = Cast<UStaticMeshComponent>(componentElement);
			staticMeshComponent->SetRenderInMainPass(!hide);
		}
	}
}

void APortalVR::UpdatePortalView()
{
	//currentFrameCount++;

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

	UPortalLocalPlayer* portalLocalPlayer = Cast<UPortalLocalPlayer>(portalPlayerController->GetLocalPlayer());
	if (portalLocalPlayer)
	{
		portalRightCapture->bUseCustomProjectionMatrix = true;
		portalRightCapture->CustomProjectionMatrix = portalLocalPlayer->GetCameraProjectionMatrix(EStereoscopicPass::eSSP_RIGHT_EYE);
		portalLeftCapture->bUseCustomProjectionMatrix = true;
		portalLeftCapture->CustomProjectionMatrix = portalLocalPlayer->GetCameraProjectionMatrix(EStereoscopicPass::eSSP_LEFT_EYE);
	}

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

FVector APortalVR::ConvertDirectionToTarget(FVector direction)
{
	//Flip the given direction to the target portal
	FVector flippedVel;
	flippedVel.X = FVector::DotProduct(direction, portalMesh->GetForwardVector());
	flippedVel.Y = FVector::DotProduct(direction, portalMesh->GetRightVector());
	flippedVel.Z = FVector::DotProduct(direction, portalMesh->GetUpVector());
	FVector newVelocity = flippedVel.X * -portalTarget->portalMesh->GetForwardVector() + flippedVel.Y * -portalTarget->portalMesh->GetRightVector() + flippedVel.Z * portalTarget->portalMesh->GetUpVector();

	//Return flipped vector for the given direction
	return newVelocity;
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

bool APortalVR::LocationInsidePortal(FVector location)
{
	FVector halfHeight = portalBoxCollision->GetScaledBoxExtent();
	FVector direction = location - portalBoxCollision->GetComponentLocation();

	bool withinX = FMath::Abs(FVector::DotProduct(direction, portalBoxCollision->GetForwardVector())) <= halfHeight.X;
	bool withinY = FMath::Abs(FVector::DotProduct(direction, portalBoxCollision->GetRightVector())) <= halfHeight.Y;
	bool withinZ = FMath::Abs(FVector::DotProduct(direction, portalBoxCollision->GetUpVector())) <= halfHeight.Z;

	return withinX && withinY && withinZ;
}

void APortalVR::UpdateCharacterTracking()
{
	FVector currentLocation = portalPlayerCharacter->Camera->GetComponentLocation();
	if (currentLocation.ContainsNaN())
	{
		return;
	}
	FVector pointInterscetion;
	FPlane portalPlane = FPlane(portalMesh->GetComponentLocation(), portalMesh->GetForwardVector());
	bool passedThroughPlane = FMath::SegmentPlaneIntersection(prevCameraLocation, currentLocation, portalPlane, pointInterscetion);
	FVector relativeIntersection = portalMesh->GetComponentTransform().InverseTransformPositionNoScale(pointInterscetion);
	FVector portalSize = portalBoxCollision->GetScaledBoxExtent();
	bool passedWithinPortal = FMath::Abs(relativeIntersection.Z) <= portalSize.Z && FMath::Abs(relativeIntersection.Y) <= portalSize.Y;

	if (passedThroughPlane && passedWithinPortal && IsInFront(prevCameraLocation))
	{
		TeleportObject(portalPlayerCharacter);
	}

	prevCameraLocation = portalPlayerCharacter->Camera->GetComponentLocation();
}

void APortalVR::TeleportObject(AActor* actor)
{
	if (!actor)
	{
		return;
	}

	//Perform a camera cut so the teleportation is seamless with the render functions.
	UPortalLocalPlayer* portalLocalPlayer = Cast<UPortalLocalPlayer>(portalPlayerController->GetLocalPlayer());
	if (!portalLocalPlayer)
	{
		Destroy();
	}
	portalLocalPlayer->CutCamera();

	// Teleport the physics object. Teleport both position and relative velocity.
	UPrimitiveComponent* primComp = Cast<UPrimitiveComponent>(actor->GetRootComponent());
	FVector newLinearVelocity = ConvertDirectionToTarget(primComp->GetPhysicsLinearVelocity());
	FVector newAngularVelocity = ConvertDirectionToTarget(primComp->GetPhysicsAngularVelocityInDegrees());
	FVector convertedLoc = ConvertLocationToPortal(actor->GetActorLocation(), this, portalTarget);
	FRotator convertedRot = ConvertRotationToPortal(actor->GetActorRotation(), this, portalTarget);
	primComp->SetWorldLocationAndRotation(convertedLoc, convertedRot, false, nullptr, ETeleportType::TeleportPhysics);
	primComp->SetPhysicsLinearVelocity(newLinearVelocity);
	primComp->SetPhysicsAngularVelocityInDegrees(newAngularVelocity);

	// If its a player handle any extra teleporting functionality in the player class.
	APortalCharacter* refPlayerCharacter = Cast<APortalCharacter>(actor);
	if (refPlayerCharacter)
	{
		refPlayerCharacter->PortalTeleport();
		//refPlayerCharacter->ReleaseInteractable();
	}
	/*
	else
	{
		// If the actor is grabbed by the pawn update the offset after teleporting.
		UPrimitiveComponent* refGrabbedComponent = refPlayerCharacter->physicsHandle->GetGrabbedComponent();
		if (refGrabbedComponent)
		{
			if (refGrabbedComponent == Cast<UPrimitiveComponent>(actor->GetRootComponent()))
			{
				refPlayerCharacter->ReleaseInteractable();
			}
		}
	}
	*/

	// Update the portal view and world offset for the target portal.
	portalTarget->UpdateWorldOffset();
	portalTarget->UpdatePortalView();
	portalTarget->prevCameraLocation = portalPlayerCharacter->Camera->GetComponentLocation();

	// Make sure the duplicate created is not hidden after teleported.
	if (portalTarget->mapTrackedActors.Contains(actor))
	{
		if (AActor * hasDuplicate = portalTarget->mapTrackedActors.FindRef(actor).trackedDuplicate)
		{
			HideActor(hasDuplicate, false);
		}
	}
}

void APortalVR::UpdateWorldOffset()
{
	if (LocationInsidePortal(portalPlayerCharacter->Camera->GetComponentLocation()))
	{
		portalMaterial->SetScalarParameterValue("ScaleOffset", 1.0f);
	}
	else
	{
		portalMaterial->SetScalarParameterValue("ScaleOffset", 0.0f);
	}
}

/*
int APortal::GetNumberOfTrackedActors()
{
	return actorsBeingTracked;
}

TMap<AActor*, AActor*>& APortal::GetDuplicateMap()
{
	return duplicateMap;
}
*/