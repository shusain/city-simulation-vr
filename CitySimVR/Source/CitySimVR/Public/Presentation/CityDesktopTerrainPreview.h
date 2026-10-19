#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "Simulation/CityTerrain.h"
#include "CityDesktopTerrainPreview.generated.h"

// Blueprint owns composition; terrain remains a deterministic, read-only proposal.
UCLASS()
class CITYSIMVR_API UCityDesktopTerrainPreview : public UWidget
{
	GENERATED_BODY()
public:
	void Present(const FCityTerrainGenerationParameters* Parameters);
	const TOptional<FCityTerrainGenerationParameters>& GetParameters() const
	{
		return CachedParameters;
	}
	const TArray<FLinearColor>& GetCells() const
	{
		return Cells;
	}
	const FString& GetLegend() const
	{
		return Legend;
	}
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	TOptional<FCityTerrainGenerationParameters> CachedParameters;
	TArray<FLinearColor> Cells;
	FString Legend;
	TSharedPtr<SWidget> PreviewWidget;
};
