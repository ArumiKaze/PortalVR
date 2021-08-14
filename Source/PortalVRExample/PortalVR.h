#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PortalVR.generated.h"

//Struct used to hold tracking actor information
USTRUCT(BlueprintType)
struct FTrackedActor
{
	GENERATED_BODY()

	FVector lastTrackedOrigin;
	USceneComponent* trackedComp;
	AActor* trackedDuplicate;

	//Default Constructor
	FTrackedActor()
	{
		lastTrackedOrigin = FVector::ZeroVector;
		trackedComp = nullptr;
		trackedDuplicate = nullptr;
	}

	//Main Constructor
	FTrackedActor(USceneComponent* trackingComponent)
	{
		lastTrackedOrigin = trackingComponent->GetComponentLocation();
		trackedComp = trackingComponent;
		trackedDuplicate = nullptr;
	}
};

USTRUCT()
struct FPostPhysicsTick : public FActorTickFunction
{
	GENERATED_BODY()

		/* Target actor. */
		class APortalVR* Target;

	/* Declaration of the new ticking function for this class. */
	virtual void ExecuteTick(float DeltaTime, enum ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent) override;
};

template <>
struct TStructOpsTypeTraits<FPostPhysicsTick> : public TStructOpsTypeTraitsBase2<FPostPhysicsTick>
{
	enum { WithCopy = false };
};

UCLASS()
class PORTALVREXAMPLE_API APortalVR : public AActor
{
	GENERATED_BODY()

	friend FPostPhysicsTick;

public:

	//---Components---//
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Portal")
	class UStaticMeshComponent* portalMesh;
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Portal")
	class UBoxComponent* portalBoxCollision;
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Portal")
	class USceneCaptureComponent2D* portalLeftCapture;
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Portal")
	class USceneCaptureComponent2D* portalRightCapture;

	//Reference to other portal actor
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Portal")
	APortalVR* portalTarget;

protected:

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Portal")
	int recursionAmount;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Portal", meta = (UIMin = "0.0", UIMax = "1.0", ClampMin = "0.0", ClampMax = "1.0"))
	float resolutionPercentile;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Portal")
	class UMaterialInterface* portalMaterialInstance;

private:

	//Reference Player controller
	APlayerController* portalPlayerController;

	//Reference Player character
	class APortalCharacter* portalPlayerCharacter;

	//Portal's render target texture for left eye
	class UCanvasRenderTarget2D* renderLeftTarget;
	//Portal's render target texture for right eye
	class UCanvasRenderTarget2D* renderRightTarget;

	//Portal's dynamic material instance
	UMaterialInstanceDynamic* portalMaterial;

	//Keep track of last world location of player, used to calculate when to teleport the player
	FVector prevCameraLocation;

	//Map of tracked actors with additional info like last location
	TMap<AActor*, FTrackedActor> mapTrackedActors;

	//Post ticking declaration
	FPostPhysicsTick physicsTick;

private:

	//Initialize function
	UFUNCTION()
	void Init();

	//Create render texture targets for portal
	void CreatePortalTextures();

	//Checks whether portal traveler is in front of the portal
	bool IsInFront(FVector location);

	//Adds a tracked actor to the tracked actor array updated in tick
	void AddTrackedActor(AActor* actorToAdd);

	//Removes a tracked actor and its duplicate
	void RemoveTrackedActor(AActor* actorToRemove);

	//Update the tracked actors relative to the target portal, takes care of teleporting physics objects as well as duplicating them and the pawn if overlapping
	void UpdateTrackedActors();

	//Copy actor's static mesh root component and sets it in the tracked actor struct
	void CopyActor(AActor* actorToCopy);

	void DeleteCopy(AActor* actorToDelete);

	//Hides a copied version of an actor from the main render pass so it still casts shadows
	void HideActor(AActor* actor, bool hide = true);

	//Update the render texture for this portal using the scene capture component
	void UpdatePortalView();

	//Clears the current render texture on the portal meshes dynamic material instance
	void ClearPortalView();

	//Convert a given velocity vector to the target portal
	FVector ConvertDirectionToTarget(FVector direction);

	//Convert a given location to the target portal
	FVector ConvertLocationToPortal(FVector location, APortalVR* currentPortal, APortalVR* endPortal, bool flip = true);

	//Convert a given rotation to the target portal
	FRotator ConvertRotationToPortal(FRotator rotation, APortalVR* currentPortal, APortalVR* endPortal, bool flip = true);

	//Check if a given location inside of this portal box
	bool LocationInsidePortal(FVector location);

	void PostPhysicsTick(float DeltaTime);

	//Updates the pawns tracking for going through portals. Cannot rely on detecting overlaps
	void UpdateCharacterTracking();

	//Function to teleport a given actor
	void TeleportObject(AActor* actor);

	//Updates the world offset in the dynamic material instance for the vertexes on the portal mesh when the camera gets too close
	void UpdateWorldOffset();

protected:
	//Called when the game starts or when spawned
	virtual void BeginPlay() override;

	//Called when the portal box is overlapped
	UFUNCTION(BlueprintCallable, Category = "Portal")
	void OnPortalBoxBeginOverlap(UPrimitiveComponent* portalMeshHit, AActor* overlappedActor, UPrimitiveComponent* overlappedComp, int32 otherBodyIndex, bool fromSweep, const FHitResult& portalHit);

	//Called when the portal box ends one of its overlap events
	UFUNCTION(BlueprintCallable, Category = "Portal")
	void OnPortalBoxEndOverlap(UPrimitiveComponent* portalMeshHit, AActor* overlappedActor, UPrimitiveComponent* overlappedComp, int32 otherBodyIndex);

	//Called when the portal mesh ends one of its overlap events
	UFUNCTION(BlueprintCallable, Category = "Portal")
	void OnPortalMeshBeginOverlap(UPrimitiveComponent* portalMeshHit, AActor* overlappedActor, UPrimitiveComponent* overlappedComp, int32 otherBodyIndex, bool fromSweep, const FHitResult& portalHit);

	//Called when the portal mesh ends one of its overlap events
	UFUNCTION(BlueprintCallable, Category = "Portal")
	void OnPortalMeshEndOverlap(UPrimitiveComponent* portalMeshHit, AActor* overlappedActor, UPrimitiveComponent* overlappedComp, int32 otherBodyIndex);
	
public:	
	//Sets default values for this actor's properties
	APortalVR();
	//Called every frame
	virtual void Tick(float DeltaTime) override;
};
