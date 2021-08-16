#pragma once

#include "CoreMinimal.h"
#include "Engine/LocalPlayer.h"
#include "PortalLocalPlayer.generated.h"

UCLASS()
class PORTALVREXAMPLE_API UPortalLocalPlayer : public ULocalPlayer
{
	GENERATED_BODY()

public:

	UPortalLocalPlayer();

	FMatrix GetCameraProjectionMatrix(EStereoscopicPass pass);
};
