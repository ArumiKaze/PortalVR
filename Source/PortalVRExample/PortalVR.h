#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PortalVR.generated.h"

/* 
 * A struct inheriting from FActorTickFunction is needed because UE4 does not support two tick functions in a single object by default
 * This is a workaround so that we don't have to have a seperate object with the only purpose of having a PostPhysicsTick in it
 * We will use this to update the player/character's position after the physics update and after the portal's texture have been updated
 */
USTRUCT()
struct FPostPhysicsTick : public FActorTickFunction
{
	GENERATED_BODY()

	class APortalVR* refPortal;

	virtual void ExecuteTick(float DeltaTime, enum ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent) override;
};

//FActorTickFunctions are unsafe to copy and its copy assignment operator has been deleted, so we need to inform UE4 about our FPostPhysicsTick struct to not copy it via copy assignment operator
template <>
struct TStructOpsTypeTraits<FPostPhysicsTick> : public TStructOpsTypeTraitsBase2<FPostPhysicsTick>
{
	enum { WithCopy = false };
};

UCLASS()
class PORTALVREXAMPLE_API APortalVR : public AActor
{
	GENERATED_BODY()

	//Declare FPostPhysicsTick as our friend so that it can access APortalVR's private member function
	friend FPostPhysicsTick;

protected:

	//---Components---//
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Portal")
	class UStaticMeshComponent* portalMesh;
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Portal")
	class USceneCaptureComponent2D* portalLeftCapture;
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Portal")
	class USceneCaptureComponent2D* portalRightCapture;

	//Reference to the other portal connected to this portal
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Portal")
	APortalVR* portalTarget;

	//Adjusts the resolution of portal texture for performance
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Portal", meta = (UIMin = "0.0", UIMax = "1.0", ClampMin = "0.0", ClampMax = "1.0"))
	float resolutionScale;

	//Adjusts the clipping plane offset of the scene capture cameras
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Portal")
	float captureCameraClippingPlaneOffset;

	//Set to portal's material so that it can dynamically create a new material at runtime to assign the render targets to the material's textures
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Portal")
	class UMaterialInterface* portalMaterialInterface;

private:

	//References (pointers) to relevant player/character objects so we don't have to keep calling "GetPlayerXXX()"
	APlayerController* portalPlayerController;
	class APortalCharacter* portalPlayerCharacter;
	ULocalPlayer* portalPlayerLocal;

	//Render target texture for left eye, used in portal's material to display portal on mesh
	class UCanvasRenderTarget2D* renderLeftTarget;
	//Render target texture for right eye, used in portal's material to display portal on mesh
	class UCanvasRenderTarget2D* renderRightTarget;

	//Portal's material
	UMaterialInstanceDynamic* portalMaterial;

	//Our object that will run/act as our second tick that will tick on post physics
	FPostPhysicsTick secondaryPostPhysicsTick;

	//Keeps track of last world location of player camera (VR headset), used to calculate when to teleport the player by determining whether the camera clips through the portal mesh
	FVector prevCameraLocation;

public:

	APortalVR();

	//Our primary tick, will run on "post update work" which is after our "post physics" tick
	virtual void Tick(float DeltaTime) override;

protected:

	virtual void BeginPlay() override;

private:

	//Setups the portal texture/render targets and doubles check whether everything needed to function is present before starting the tick functions
	void Init();

	//Create render target textures and material for portal
	void CreatePortalTexturesAndMaterial();

	//Helper function used to check whether the player was in front of the portal
	bool WasActorInFrontOfPortal(const FVector& location) const;

	//Called in the secondary post physics tick, checks whether the player moved through a portal and calls TeleportActor() if they did
	void UpdateCharacterTracking();

	//Teleports the player and updates the exit portal on the same frame
	void TeleportCharacter();

	//Updates the clipping planes, projection matrices, and location/rotation of the portal capture cameras
	void UpdatePortalView();

	//Calculates a new location and rotation via matrices for the player/camera relative to the target/exit portal
	void ConvertLocationRotationToPortal(FVector& newLocation, FRotator& newRotator, const FTransform& actorTransform) const;
};
