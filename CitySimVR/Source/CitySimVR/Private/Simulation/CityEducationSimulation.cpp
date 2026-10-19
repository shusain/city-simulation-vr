#include "Simulation/CityEducationSimulation.h"
#include "Performance/CityEvaluationTiming.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Simulation/CityPopulationSimulation.h"
#include "Simulation/CityDevelopmentSimulation.h"
#include "Simulation/CityBuildingDefinition.h"
void FCityEducationEvaluator::Build(const FCityPlacementModel& City, FCityServiceSnapshot& S,
                                    const FCityEconomyTuning& T, const FCityPopulationSnapshot* P,
                                    const FCityDevelopmentSnapshot* D, const bool bPrepareQueries,
                                    FCityServiceEvaluationTiming* Timing)
{
	S.Dimensions = City.GetGrid().GetDimensions();
	S.EducationCoverage.Init(0, City.GetGrid().GetDimensions().GetCellCount());
	TArray<uint64> Schools;
	TMap<uint64, FCityServiceReach> Reaches;
	// Geometry is constant within this evaluation. Keep ordered nearby schools
	// per home; capacity is still consumed per citizen in the original order.
	TMap<uint64, TArray<uint64>> HomeSchools;
	City.GetEntities().GetKeys(Schools);
	Schools.Sort();
	Schools.RemoveAll(
	    [&](uint64 Id)
	    { return !FCityBuildingDefinition::IsSchoolFacility(City.GetEntities().FindChecked(Id).DefinitionId); });
	S.BuiltSchools = Schools.Num();
	const bool bHasCollege = Schools.ContainsByPredicate(
	    [&](uint64 Id) { return City.GetEntities().FindChecked(Id).DefinitionId == TEXT("College"); });
	for (uint64 Id : Schools)
	{
		if ((D && !D->IsOperational(Id)) || S.GetFailure(Id) != ECityOperationFailure::None)
			continue;
		const int32 Capacity =
		    FMath::Clamp(T.SchoolCapacity, 1, 500) * (P ? S.PublicStaffPercent.FindRef(Id) : 100) / 100;
		if (Capacity <= 0)
			continue;
		++S.OperationalSchools;
		S.EducationCapacity += Capacity;
		S.SchoolCapacities.Add(Id, Capacity);
		S.SchoolUsedCapacity.Add(Id, 0);
		FCityServiceEvaluator::BuildProviderReach(City, Id, FMath::Clamp(T.SchoolRoadRangeHops, 1, 64), true,
		                                          Reaches.Add(Id));
	}
	if (P)
		for (const auto& C : P->Citizens)
		{
			S.SecondaryResidents += C.Education >= ECityEducationLevel::Secondary ? 1 : 0;
			if (C.Education >= ECityEducationLevel::Higher)
				continue;
			if (C.Education >= ECityEducationLevel::Secondary && !bHasCollege)
				continue;
			++S.EducationDemand;
			if (!C.HomeEntityId)
				continue;
			++S.HomeEducationDemand.FindOrAdd(C.HomeEntityId);
			const auto* Home = City.GetEntities().Find(C.HomeEntityId);
			if (!Home || !S.EntityResidents.FindRef(C.HomeEntityId))
				continue;
			const TArray<uint64>* Candidates = &Schools;
			if (bPrepareQueries)
			{
				Candidates = HomeSchools.Find(C.HomeEntityId);
				if (!Candidates)
				{
					auto& Nearby = HomeSchools.Add(C.HomeEntityId);
					for (const uint64 School : Schools)
						if (Reaches.Contains(School) &&
						    FCityServiceEvaluator::IsFootprintReached(City, Reaches.FindChecked(School), Home->Anchor,
						                                              Home->FootprintSize))
							Nearby.Add(School);
					Candidates = &Nearby;
					if (Timing)
						++Timing->PreparedEducationQueries;
				}
			}
			else if (Timing)
				++Timing->ReferenceEducationCitizens;
			for (uint64 School : *Candidates)
				if (FCityBuildingDefinition::CanTeachEducation(City.GetEntities().FindChecked(School).DefinitionId,
				                                               static_cast<int32>(C.Education)) &&
				    Reaches.Contains(School) &&
				    S.SchoolUsedCapacity.FindRef(School) < S.SchoolCapacities.FindRef(School) &&
				    (bPrepareQueries || FCityServiceEvaluator::IsFootprintReached(City, Reaches.FindChecked(School),
				                                                                  Home->Anchor, Home->FootprintSize)))
				{
					++S.SchoolUsedCapacity.FindChecked(School);
					++S.EducationServed;
					if (C.Education >= ECityEducationLevel::Secondary)
						S.HigherEducationCitizenIds.Add(C.Id);
					else
						S.EducationCitizenIds.Add(C.Id);
					++S.HomeEducationPlaces.FindOrAdd(C.HomeEntityId);
					break;
				}
		}
	// One capped access value per cell, including space available to future homes.
	for (const auto& E : City.GetEntities())
	{
		if (E.Value.DefinitionId != T.HomeDefinitionId)
			continue;
		const int32 Demand = S.HomeEducationDemand.FindRef(E.Key);
		int32 Percent = Demand > 0 ? S.HomeEducationPlaces.FindRef(E.Key) * 100 / Demand : 0;
		if (Demand == 0 && S.EntityResidents.FindRef(E.Key) > 0)
			for (uint64 School : Schools)
				if (City.GetEntities().FindChecked(School).DefinitionId != TEXT("College") &&
				    Reaches.Contains(School) &&
				    S.SchoolUsedCapacity.FindRef(School) < S.SchoolCapacities.FindRef(School) &&
				    FCityServiceEvaluator::IsFootprintReached(City, Reaches.FindChecked(School), E.Value.Anchor,
				                                              E.Value.FootprintSize))
				{
					Percent = 100;
					break;
				}
		for (int32 Y = E.Value.Anchor.Y; Y < E.Value.Anchor.Y + E.Value.FootprintSize.Y; ++Y)
			for (int32 X = E.Value.Anchor.X; X < E.Value.Anchor.X + E.Value.FootprintSize.X; ++X)
				S.EducationCoverage[Y * City.GetGrid().GetDimensions().GetWidth() + X] = Percent;
	}
	// Reveal potential access beside reachable roads. Work scales with the bounded
	// provider reach rather than scanning every map cell for every school.
	const FCityCellCoord Offsets[] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
	for (const auto& Provider : Reaches)
	{
		if (S.SchoolUsedCapacity.FindRef(Provider.Key) >= S.SchoolCapacities.FindRef(Provider.Key))
			continue;
		for (const auto& Road : Provider.Value.RoadHopDistances)
			for (const auto& Offset : Offsets)
			{
				const FCityCellCoord Cell(Road.Key.X + Offset.X, Road.Key.Y + Offset.Y);
				if (City.GetGrid().IsValidCell(Cell) && City.GetGrid().GetOccupant(Cell) == 0)
					S.EducationCoverage[Cell.Y * City.GetGrid().GetDimensions().GetWidth() + Cell.X] = 100;
			}
	}
}
