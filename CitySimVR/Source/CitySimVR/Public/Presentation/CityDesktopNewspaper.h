#pragma once
#include "Components/Widget.h"
#include "CoreMinimal.h"
#include "Presentation/CityUiContent.h"
#include "CityDesktopNewspaper.generated.h"
class UFont;

// One reusable paper renderer, composed by the existing Desktop Blueprint and
// shown on a single world-space sheet in VR. Content remains a read-only snapshot.
namespace CityNewspaper
{
	CITYSIMVR_API TSharedRef<SWidget> MakeSheet(TSharedRef<FCityUiNewspaper> Content, UFont* Font, float Width, float FontScale = 1);
}
UCLASS()
class CITYSIMVR_API UCityDesktopNewspaper : public UWidget
{
	GENERATED_BODY()
  public:
	UPROPERTY(EditAnywhere, Category = "City News") TObjectPtr<UFont> Typeface;
	bool SetEdition(const FCityUiNewspaper& InEdition);
	void SetPaperWidth(float Width);
	const FCityUiNewspaper& GetEdition() const
	{
		return *Content;
	}
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

  protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

  private:
	TSharedRef<FCityUiNewspaper> Content = MakeShared<FCityUiNewspaper>();
	TSharedPtr<SWidget> Sheet;
	float PaperWidth = 920;
};
