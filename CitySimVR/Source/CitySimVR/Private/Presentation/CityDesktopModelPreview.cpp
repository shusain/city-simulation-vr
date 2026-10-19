#include "Presentation/CityDesktopModelPreview.h"

#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"

void UCityDesktopModelPreview::Present(const FCityDesktopModelSource* Source)
{
	if (!Source || !Source->Mesh || !Source->EntityId || !GetWorld())
	{
		ClearPreview();
		return;
	}
	if (!IsValid(Stage))
	{
		FActorSpawnParameters Params;
		Params.ObjectFlags |= RF_Transient;
		Stage = GetWorld()->SpawnActor<AActor>(Params);
		if (!Stage)
			return;
		Stage->SetActorTickEnabled(false);
		PreviewMesh = NewObject<UStaticMeshComponent>(Stage);
		Stage->SetRootComponent(PreviewMesh);
		PreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PreviewMesh->SetVisibleInSceneCaptureOnly(true);
		PreviewMesh->SetCastShadow(false);
		PreviewMesh->RegisterComponent();
		Backdrop = NewObject<UStaticMeshComponent>(Stage);
		Backdrop->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")));
		Backdrop->SetMaterial(
		    0, LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/CitySim/UI/MI_CityUiPanel.MI_CityUiPanel")));
		Backdrop->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Backdrop->SetVisibleInSceneCaptureOnly(true);
		Backdrop->SetCastShadow(false);
		Backdrop->RegisterComponent();
		Capture = NewObject<USceneCaptureComponent2D>(Stage);
		Capture->bCaptureEveryFrame = false;
		Capture->bCaptureOnMovement = false;
		Capture->ProjectionType = ECameraProjectionMode::Perspective;
		Capture->FOVAngle = 40;
		Capture->CaptureSource = SCS_FinalColorLDR;
		Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
		Capture->ShowOnlyComponent(PreviewMesh);
		Capture->ShowOnlyComponent(Backdrop);
		Capture->ShowFlags.SetAtmosphere(false);
		Capture->ShowFlags.SetFog(false);
		Capture->ShowFlags.SetMotionBlur(false);
		Capture->ShowFlags.SetTemporalAA(false);
		Capture->PostProcessSettings.bOverride_AutoExposureMinBrightness = true;
		Capture->PostProcessSettings.bOverride_AutoExposureMaxBrightness = true;
		Capture->PostProcessSettings.AutoExposureMinBrightness = 1;
		Capture->PostProcessSettings.AutoExposureMaxBrightness = 1;
		Target = NewObject<UTextureRenderTarget2D>(this);
		Target->ClearColor = FLinearColor(.025f, .045f, .065f, 1);
		Target->InitCustomFormat(512, 320, PF_B8G8R8A8, false);
		Capture->TextureTarget = Target;
		Capture->RegisterComponent();
		SetBrushResourceObject(Target);
		SetDesiredSizeOverride(FVector2D(512, 320));
	}
	const bool Changed = PreviewEntity != Source->EntityId || PreviewMesh->GetStaticMesh() != Source->Mesh ||
	                     !PreviewMesh->GetComponentQuat().Equals(Source->Rotation);
	PreviewEntity = Source->EntityId;
	if (Changed)
	{
		PreviewMesh->EmptyOverrideMaterials();
		PreviewMesh->SetStaticMesh(Source->Mesh);
		const FBox Bounds = Source->Mesh->GetBoundingBox();
		const double Scale = 200.0 / FMath::Max(1.0, Bounds.GetSize().GetMax());
		// One isolated stage. It never appears in the player camera or participates in city picking.
		// Stay inside the baked room probe. The previous distant stage and
		// orthographic forward capture made exposed metal appear flat gray.
		const FVector Center(0, 0, -2500);
		PreviewMesh->SetWorldTransform(FTransform(
		    Source->Rotation, Center - Source->Rotation.RotateVector(Bounds.GetCenter() * Scale), FVector(Scale)));
		const FVector Direction = FVector(1, -1, .85).GetSafeNormal();
		const FRotationMatrix ViewRotation((-Direction).Rotation());
		const FVector Right = ViewRotation.GetUnitAxis(EAxis::Y), Up = ViewRotation.GetUnitAxis(EAxis::Z);
		const double TanHorizontal = FMath::Tan(FMath::DegreesToRadians(Capture->FOVAngle * .5));
		double Distance = 0;
		for (int32 Corner = 0; Corner < 8; ++Corner)
		{
			const FVector Point(Corner & 1 ? Bounds.Max.X : Bounds.Min.X,
			                    Corner & 2 ? Bounds.Max.Y : Bounds.Min.Y,
			                    Corner & 4 ? Bounds.Max.Z : Bounds.Min.Z);
			const FVector Rotated = Source->Rotation.RotateVector((Point - Bounds.GetCenter()) * Scale);
			Distance = FMath::Max(Distance, FVector::DotProduct(Rotated, Direction) +
			    FMath::Max(FMath::Abs(FVector::DotProduct(Rotated, Right)),
			               FMath::Abs(FVector::DotProduct(Rotated, Up)) * 1.6) * 1.15 / TanHorizontal);
		}
		const FVector CameraOffset = Direction * Distance;
		Capture->SetWorldLocationAndRotation(Center + CameraOffset, (-CameraOffset).Rotation());
		Backdrop->SetWorldTransform(
		    FTransform(FQuat::FindBetweenNormals(FVector::UpVector, CameraOffset.GetSafeNormal()),
		               Center - CameraOffset.GetSafeNormal() * 250, FVector(16)));
	}
	bool MaterialsChanged = PreviewMesh->GetNumMaterials() != Source->Materials.Num();
	for (int32 Index = 0; Index < Source->Materials.Num(); ++Index)
		if (PreviewMesh->GetMaterial(Index) != Source->Materials[Index])
		{
			PreviewMesh->SetMaterial(Index, Source->Materials[Index]);
			MaterialsChanged = true;
		}
	const double Now = FPlatformTime::Seconds();
	if (Changed || MaterialsChanged || Now >= NextCapture)
	{
		// Refresh current material/lighting parameters while open, without a continuously ticking capture.
		Capture->CaptureScene();
		++CaptureCount;
		NextCapture = Now + .5;
	}
}

void UCityDesktopModelPreview::ClearPreview()
{
	if (IsValid(Stage))
		Stage->Destroy();
	Stage = nullptr;
	PreviewMesh = nullptr;
	Backdrop = nullptr;
	Capture = nullptr;
	SetBrushResourceObject(nullptr);
	Target = nullptr;
	PreviewEntity = 0;
	NextCapture = 0;
}

UStaticMesh* UCityDesktopModelPreview::GetPreviewMesh() const
{
	return PreviewMesh ? PreviewMesh->GetStaticMesh() : nullptr;
}

void UCityDesktopModelPreview::ReleaseSlateResources(const bool bReleaseChildren)
{
	ClearPreview();
	Super::ReleaseSlateResources(bReleaseChildren);
}
