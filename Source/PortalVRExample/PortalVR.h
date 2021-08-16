#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PortalVR.generated.h"

USTRUCT()
struct FPostPhysicsTick : public FActorTickFunction
{
	GENERATED_BODY()

	//Reference to PortalVR
	class APortalVR* refPortal;

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
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Portal")
	class USceneCaptureComponent2D* portalLeftCapture;
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Portal")
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

	//Reference Player Local
	class UPortalLocalPlayer* portalPlayerLocal;

	//Portal's render target texture for left eye
	class UCanvasRenderTarget2D* renderLeftTarget;
	//Portal's render target texture for right eye
	class UCanvasRenderTarget2D* renderRightTarget;

	//Portal's dynamic material instance
	UMaterialInstanceDynamic* portalMaterial;

	//Keep track of last world location of player, used to calculate when to teleport the player
	FVector prevCameraLocation;

	//Post ticking declaration
	FPostPhysicsTick physicsTick;

public:

	//Sets default values for this actor's properties
	APortalVR();
	//Called every frame
	virtual void Tick(float DeltaTime) override;

protected:

	//Called when the game starts or when spawned
	virtual void BeginPlay() override;

private:

	//Initialize function
	UFUNCTION()
	void Init();

	//Create render texture targets for portal
	void CreatePortalTextures();

	//Checks whether portal traveler is in front of the portal
	bool IsActorInFrontOfPortal(FVector location);

	//Update the render texture for this portal using the scene capture component
	void UpdatePortalView();

	//Clears the current render texture on the portal meshes dynamic material instance
	void ClearPortalView();

	//Convert a given location to the target portal
	FVector ConvertLocationToPortal(FVector location, APortalVR* currentPortal, APortalVR* endPortal, bool flip = true);

	//Convert a given rotation to the target portal
	FRotator ConvertRotationToPortal(FRotator rotation, APortalVR* currentPortal, APortalVR* endPortal, bool flip = true);

	void PostPhysicsTick(float DeltaTime);

	//Updates the pawns tracking for going through portals. Cannot rely on detecting overlaps
	void UpdateCharacterTracking();

	//Function to teleport a given actor
	void TeleportActor(AActor* actor);
};
