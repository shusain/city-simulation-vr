#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "CityDesktopIcon.generated.h"

class SCityDesktopIcon;

// Small scalable pictograms, paired with labels; no mesh-specific raster thumbnails.
UCLASS()
class CITYSIMVR_API UCityDesktopIcon : public UWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Icon")
	FName Symbol = TEXT("Build");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Icon")
	FLinearColor Tint = FLinearColor(.86f, .92f, .95f);
	void SetIcon(FName InSymbol, FLinearColor InTint);
	void SetConditionIcon(FName InSymbol, FLinearColor InTint);
	const FSlateBrush* GetConditionBrush() const
	{
		return ConditionTexture ? &ConditionBrush : nullptr;
	}
	static FName ForAction(int32 Action);
	static FLinearColor ColorForSymbol(FName InSymbol);
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	TSharedPtr<SCityDesktopIcon> Icon;
	UPROPERTY(Transient)
	TObjectPtr<class UTexture2D> ConditionTexture;
	FSlateBrush ConditionBrush;
};
