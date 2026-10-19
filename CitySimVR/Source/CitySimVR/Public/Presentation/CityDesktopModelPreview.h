#pragma once

#include "CoreMinimal.h"
#include "Components/Image.h"
#include "CityDesktopModelPreview.generated.h"

class AActor;
class UStaticMesh;
class UStaticMeshComponent;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class UMaterialInterface;

// Read-only current presentation, resolved by logical entity identity, not a thumbnail catalog.
struct FCityDesktopModelSource
{
	uint64 EntityId = 0;
	UStaticMesh* Mesh = nullptr;
	TArray<UMaterialInterface*> Materials;
	FQuat Rotation = FQuat::Identity;
};

UCLASS()
class CITYSIMVR_API UCityDesktopModelPreview : public UImage
{
	GENERATED_BODY()
public:
	void Present(const FCityDesktopModelSource* Source);
	void ClearPreview();
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	bool HasModel() const
	{
		return PreviewMesh && PreviewEntity != 0;
	}
	uint64 GetPreviewEntity() const
	{
		return PreviewEntity;
	}
	UStaticMesh* GetPreviewMesh() const;
	UTextureRenderTarget2D* GetPreviewTarget() const
	{
		return Target;
	}
	uint64 GetCaptureCount() const
	{
		return CaptureCount;
	}

private:
	UPROPERTY(Transient)
	TObjectPtr<AActor> Stage;
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> PreviewMesh;
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Backdrop;
	UPROPERTY(Transient)
	TObjectPtr<USceneCaptureComponent2D> Capture;
	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> Target;
	uint64 PreviewEntity = 0;
	uint64 CaptureCount = 0;
	double NextCapture = 0;
};
