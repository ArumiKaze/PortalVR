#include "PortalLocalPlayer.h"

UPortalLocalPlayer::UPortalLocalPlayer()
{
}

FMatrix UPortalLocalPlayer::GetCameraProjectionMatrix(EStereoscopicPass pass)
{
	FMatrix projectionMatrix;
	FSceneViewProjectionData projectionData;
	GetProjectionData(ViewportClient->Viewport, pass, projectionData);
	projectionMatrix = projectionData.ProjectionMatrix;
	return projectionMatrix;
}
