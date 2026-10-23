#include "Presentation/CityDesktopMenuContent.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace CityDesktopMenuStyle
{
const FLinearColor Ink(.9f, .94f, 1.f), Muted(.53f, .66f, .74f), Accent(.3f, .8f, .7f), Warning(1.f, .72f, .32f);
const FSlateRoundedBoxBrush Card(FLinearColor(.028f, .055f, .075f), 7.f);
TSharedRef<STextBlock> Label(const FText& Text, int32 Size = 12, FLinearColor Color = Ink, bool Bold = false)
{
    return SNew(STextBlock).Text(Text).AutoWrapText(true).ColorAndOpacity(Color)
        .Font(FCoreStyle::GetDefaultFontStyle(Bold ? TEXT("Bold") : TEXT("Regular"), Size));
}
}

TSharedRef<SWidget> UCityDesktopMenuContent::RebuildWidget()
{
    ContentKey.Empty();
    return SAssignNew(Content, SVerticalBox);
}
void UCityDesktopMenuContent::ReleaseSlateResources(bool bReleaseChildren)
{
    Super::ReleaseSlateResources(bReleaseChildren);
    Content.Reset();
    Expanders.Reset();
    ExpanderIds.Reset();
}
int32 UCityDesktopMenuContent::SetDetailsExpanded(bool Open)
{
    for (int32 I = 0; I < Expanders.Num(); ++I)
    {
        Expanders[I]->SetExpanded(Open);
        if (Open) ExpandedSections.Add(ExpanderIds[I]); else ExpandedSections.Remove(ExpanderIds[I]);
    }
    return Expanders.Num();
}
void UCityDesktopMenuContent::Present(const FCityUiSnapshot& Data)
{
    using namespace CityDesktopMenuStyle;
    FString Key = Data.MenuTitle.ToString() + Data.MenuStatus.ToString();
    for (const auto& M : Data.MenuMetrics) Key += TEXT("|") + M.Label.ToString() + M.Value.ToString();
    for (const auto& S : Data.MenuSections)
    {
        Key += TEXT("|") + S.Id + S.Title.ToString() + S.Body.ToString() + (S.bCollapsible ? TEXT("C") : TEXT("O")) + (S.bWarning ? TEXT("W") : TEXT("N"));
        for (const auto& R : S.Rows) Key += TEXT("|") + R.Label.ToString() + R.Value.ToString();
    }
    if (!Content || Key == ContentKey) return;
    ContentKey = Key;
    Content->ClearChildren();
    Expanders.Reset(); ExpanderIds.Reset();
    if (!Data.MenuStatus.IsEmpty())
        Content->AddSlot().AutoHeight().Padding(0, 4, 0, 10)[
            SNew(SBorder).BorderImage(&Card).Padding(10)[Label(Data.MenuStatus, 12, Accent, true)]];
    if (Data.MenuMetrics.Num())
    {
        TSharedRef<SHorizontalBox> Metrics = SNew(SHorizontalBox);
        for (const auto& M : Data.MenuMetrics)
            Metrics->AddSlot().FillWidth(1).Padding(0, 0, 6, 0)[
                SNew(SBorder).BorderImage(&Card).Padding(10)[
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()[Label(M.Label, 11, Muted)]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 5, 0, 0)[Label(M.Value, 18, Ink, true)]]];
        Content->AddSlot().AutoHeight().Padding(0, 0, 0, 12)[Metrics];
    }
    for (const auto& S : Data.MenuSections)
    {
        TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
        if (!S.Body.IsEmpty()) Body->AddSlot().AutoHeight().Padding(0, 2, 0, 3)[Label(S.Body, 12, S.bWarning ? Warning : Ink)];
        for (const auto& R : S.Rows)
            Body->AddSlot().AutoHeight().Padding(0, 3)[
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(.58f).Padding(0, 0, 12, 0)[Label(R.Label, 12, Muted)]
                + SHorizontalBox::Slot().FillWidth(.42f).HAlign(HAlign_Right)[Label(R.Value, 12, Ink, true)]];
        if (S.bCollapsible)
        {
            const FString Id = Data.MenuTitle.ToString() + TEXT("|") + S.Id;
            const TWeakObjectPtr<UCityDesktopMenuContent> WeakThis(this);
            TSharedPtr<SExpandableArea> Area;
            Content->AddSlot().AutoHeight().Padding(0, 3, 0, 9)[
                SAssignNew(Area, SExpandableArea).BorderImage(&Card).HeaderPadding(FMargin(8, 5)).InitiallyCollapsed(!ExpandedSections.Contains(Id))
                .OnAreaExpansionChanged_Lambda([WeakThis, Id](bool Open) {
                    if (auto* Self = WeakThis.Get()) { if (Open) Self->ExpandedSections.Add(Id); else Self->ExpandedSections.Remove(Id); }
                })
                .HeaderContent()[Label(S.Title, 12, Muted)]
                .BodyContent()[SNew(SBorder).BorderImage(&Card).Padding(10)[Body]]];
            Expanders.Add(Area); ExpanderIds.Add(Id);
        }
        else
            Content->AddSlot().AutoHeight().Padding(0, 0, 0, 10)[
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 3)[Label(S.Title, 11, S.bWarning ? Warning : Accent, true)]
                + SVerticalBox::Slot().AutoHeight()[Body]];
    }
}
