// Fill out your copyright notice in the Description page of Project Settings.


#include "PortalLocalPlayer.h"

UPortalLocalPlayer::UPortalLocalPlayer()
{
	cameraCut = false;
}

void UPortalLocalPlayer::CutCamera()
{
	cameraCut = true;
}

FMatrix UPortalLocalPlayer::GetCameraProjectionMatrix(EStereoscopicPass pass)
{
	FMatrix projectionMatrix;
	FSceneViewProjectionData projectionData;
	GetProjectionData(ViewportClient->Viewport, pass, projectionData);
	projectionMatrix = projectionData.ProjectionMatrix;
	return projectionMatrix;
}
