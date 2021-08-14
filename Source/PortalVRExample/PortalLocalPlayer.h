// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/LocalPlayer.h"
#include "PortalLocalPlayer.generated.h"

/**
 * 
 */
UCLASS()
class PORTALVREXAMPLE_API UPortalLocalPlayer : public ULocalPlayer
{
	GENERATED_BODY()
	
public:

protected:

private:

	bool cameraCut;

public:

	UPortalLocalPlayer();

	//Cut the camera's frame
	void CutCamera();

	FMatrix GetCameraProjectionMatrix(EStereoscopicPass pass);

protected:

private:

};
