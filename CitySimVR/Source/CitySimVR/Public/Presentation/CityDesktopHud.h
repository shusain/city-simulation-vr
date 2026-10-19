#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Presentation/CityInspectionTrends.h"
#include "CityDesktopHud.generated.h"

class ACityVRToolController;
class UBorder;
class UTextBlock;

UCLASS()
class CITYSIMVR_API UCityDesktopActionButton : public UButton
{
	GENERATED_BODY()
public:
	UCityDesktopActionButton();
	void Configure(ACityVRToolController* InSession, int32 InAction);
	void SetPresentation(FLinearColor Accent, bool bSelected);
	void SetSelected(bool bSelected)
	{
		bIsSelected = bSelected;
	}
	bool IsSelected() const
	{
		return bIsSelected;
	}

private:
	TWeakObjectPtr<ACityVRToolController> Session;
	int32 Action = INDEX_NONE;
	bool bIsSelected = false;
	bool bPresentationSet = false;
	FLinearColor PresentationAccent = FLinearColor::Transparent;
	uint64 PressedCondition = 0;
	uint64 PressedConditionSession = MAX_uint64;
	UFUNCTION()
	void RememberConditionTarget();
	UFUNCTION()
	void Activate();
};

// Layout is authored in WBP_CityDesktopHud. Native code presents the shared session snapshot.
UCLASS()
class CITYSIMVR_API UCityDesktopHud : public UUserWidget
{
	GENERATED_BODY()
public:
	void SetSession(ACityVRToolController* InSession);
	bool IsPointerOverUi() const;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaSeconds) override;
	virtual void NativeDestruct() override;
	void ToggleInspectionDetails()
	{
		OpenInspectionSections = OpenInspectionSections ? 0 : 63;
	}
	void ToggleInspectionSection(int32 Index)
	{
		if (Index >= 0 && Index < 6)
			OpenInspectionSections ^= 1 << Index;
	}
	bool IsInspectionSectionOpen(int32 Index) const { return (OpenInspectionSections & (1 << Index)) != 0; }
	void ToggleCityDetails()
	{
		bCityDetails = !bCityDetails;
	}
	bool HasCityDetails() const
	{
		return bCityDetails;
	}
	void ResetPresentationMetrics()
	{
		PresentationSeconds = 0;
		PresentationCount = 0;
	}
	double GetAveragePresentationMs() const
	{
		return PresentationCount ? PresentationSeconds * 1000 / PresentationCount : 0;
	}

private:
	TOptional<FLinearColor> OriginalMenuColor;
	TWeakObjectPtr<ACityVRToolController> Session;
	void Present();
	double PresentationSeconds = 0;
	uint64 PresentationCount = 0;
	uint8 OpenInspectionSections = 0;
	bool bCityDetails = false;
	uint64 LastPreviewEntity = 0;
	FCityInspectionTrends InspectionTrends;
	void PresentConditions();
	void SelectCondition(int32 Index);
	TArray<uint64> ConditionTargets;
	TArray<uint64> AllConditionTargets;
	uint64 ConditionSession = MAX_uint64;
	uint64 LastConditionTarget = 0;
	friend class UCityDesktopActionButton;
	friend class FCityDesktopConditionsCommand;
};
