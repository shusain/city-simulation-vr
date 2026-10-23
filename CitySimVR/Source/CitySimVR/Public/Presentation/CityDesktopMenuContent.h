#pragma once
#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "Presentation/CityUiContent.h"
#include "CityDesktopMenuContent.generated.h"

// Desktop composition inside the Blueprint-authored menu scroll area.
UCLASS()
class CITYSIMVR_API UCityDesktopMenuContent : public UWidget
{
    GENERATED_BODY()
public:
    void Present(const FCityUiSnapshot& Snapshot);
    int32 SetDetailsExpanded(bool Open);
    virtual void ReleaseSlateResources(bool bReleaseChildren) override;
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
private:
    FString ContentKey;
    TSet<FString> ExpandedSections;
    TSharedPtr<class SVerticalBox> Content;
    TArray<TSharedPtr<class SExpandableArea>> Expanders;
    TArray<FString> ExpanderIds;
};
