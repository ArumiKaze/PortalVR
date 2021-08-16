#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "PortalCharacter.generated.h"

UCLASS()
class PORTALVREXAMPLE_API APortalCharacter : public ACharacter
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	USceneComponent* VROrigin;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	class UCameraComponent* Camera;

public:	

	APortalCharacter();

	virtual void Tick(float DeltaTime) override;

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:

	virtual void BeginPlay() override;

};
