#include "Presentation/CityInspectionTrends.h"
#include "Presentation/CityUiContent.h"
#include "Commands/CityPlacementModel.h"
#include "Simulation/CityPopulationSimulation.h"

void FCityInspectionTrends::Observe(const FCityPlacementModel& City, const FCityPopulationSnapshot& Population,
                                   int64 Step, int64 Month, uint64 Generation)
{
	if (Generation != CityGeneration || Step < LastStep)
	{
		Reference.Reset(); Current.Reset(); LastStep = ReferenceStep = LastMonth = -1;
		CityGeneration = Generation;
	}
	if (Step == LastStep)
		return;
	if (Month != LastMonth && LastStep >= 0)
	{
		Reference = Current;
		ReferenceStep = LastStep;
	}
	Current.Reset();
	for (const auto& E : City.GetEntities())
		if (E.Value.DefinitionId == TEXT("Home"))
			Current.Add(E.Key);
	for (const auto& C : Population.Citizens)
		if (C.HomeEntityId)
		{
			auto& Row = Current.FindOrAdd(C.HomeEntityId);
			++Row.Residents; Row.Happiness += C.Happiness;
		}
	if (LastStep < 0)
	{
		Reference = Current;
		ReferenceStep = Step;
	}
	LastStep = Step; LastMonth = Month;
}

void FCityInspectionTrends::Present(uint64 EntityId, FCityUiSnapshot& Out) const
{
	if (Out.InspectionOverview.IsEmpty())
		return;
	Out.InspectionOverviewIcons.SetNum(3);
	Out.InspectionOverviewHelp.SetNum(3);
	const auto* Now = Current.Find(EntityId);
	const auto* Before = Reference.Find(EntityId);
	const bool Known = Now && Before && LastStep > ReferenceStep;
	for (int32 I = 0; I < 2; ++I)
	{
		if (I == 1 && (!Now || !Now->Residents))
		{ Out.InspectionOverview[I] = FText::GetEmpty(); continue; }
		const bool HasHistory = Known && (I == 0 || (Now->Residents && Before->Residents));
		const double Value = Now ? (I == 0 ? Now->Residents : double(Now->Happiness) / FMath::Max(1, Now->Residents)) : 0;
		const double Old = Before ? (I == 0 ? Before->Residents : double(Before->Happiness) / FMath::Max(1, Before->Residents)) : 0;
		const double Delta = HasHistory ? Value - Old : 0;
		const FString Direction = !HasHistory ? TEXT("collecting trend") : Delta > 0 ? TEXT("rising")
		    : Delta < 0 ? TEXT("falling") : TEXT("steady");
		Out.InspectionOverview[I] = FText::FromString((I == 0 ? TEXT("Population: ") : TEXT("Happiness: ")) + Direction);
		Out.InspectionOverviewIcons[I] = !HasHistory ? TEXT("TrendUnknown") : Delta > 0 ? TEXT("TrendUp")
		    : Delta < 0 ? TEXT("TrendDown") : TEXT("TrendSteady");
		Out.InspectionOverviewHelp[I] = FText::FromString(HasHistory
		    ? FString::Printf(TEXT("Now %.1f | previous %.1f | change %+.1f\nObserved since game day %lld in this city session.\n%s"),
		        Value, Old, Delta, ReferenceStep + 1, I == 1 ? TEXT("Resident-average happiness updates monthly; outlook is a separate forecast.") : TEXT("Current residents of this building."))
		    : TEXT("Collecting a comparison sample for this building. Trend history starts on load/new city and is not saved."));
	}
	Out.InspectionOverviewIcons[2] = TEXT("Leaving");
}
