// Fill out your copyright notice in the Description page of Project Settings.

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

protected:

	//How fast to correct the players orientation after a teleport event
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Camera")
	float orientationCorrectionTime;

private:

	bool needFixOrientation;

	float orientationStart;

public:	

	//Sets default values for this character's properties
	APortalCharacter();

	//Called every frame
	virtual void Tick(float DeltaTime) override;

	//Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	//Called from portalvr to a character when teleporting
	void PortalTeleport();

protected:

	//Called when the game starts or when spawned
	virtual void BeginPlay() override;

private:

	//Return the player to the correct orientation after a teleport event from a portalvr class
	void ReturnToOrientation();

};
