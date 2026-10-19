#include "Presentation/CityServiceGuidance.h"
#include "Commands/CityPlacementModel.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Simulation/CityDevelopmentSimulation.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Simulation/CityPopulationSimulation.h"

namespace
{
	const TCHAR* EducationName(int32 Tier)
	{
		const TCHAR* Names[] = {TEXT("None"), TEXT("Basic"), TEXT("Secondary"), TEXT("Higher")};
		return Names[FMath::Clamp(Tier, 0, 3)];
	}
	int32 Distance(const FCityPlacementModel& City, const FCityServiceReach& Reach, const FCityPlacedEntity& Target)
	{
		TArray<FCityCellCoord> Roads;
		FCityServiceEvaluator::GetAdjacentRoadCells(City, Target.Anchor, Target.FootprintSize, false, Roads);
		int32 Best = MAX_int32;
		for (const auto& Road : Roads)
			if (const auto* Hops = Reach.RoadHopDistances.Find(Road))
				Best = FMath::Min(Best, *Hops);
		return Best;
	}
} // namespace

FString FCityServiceGuidance::Range(const FCityBuildingDefinition& D, const FCityEconomyTuning& T)
{
	if (FCityBuildingDefinition::IsSchoolFacility(D.DefinitionId))
		return FString::Printf(
		    TEXT(
		        "Reach %d road steps from school frontage; follows connected roads\nTeaches %s | %d served days per tier\nPlaces depend on staff, utilities and paid bills"),
		    FMath::Clamp(T.SchoolRoadRangeHops, 1, 64),
		    D.DefinitionId == TEXT("College")           ? TEXT("Secondary to Higher")
		    : D.DefinitionId == TEXT("PrimarySchool")   ? TEXT("None to Basic")
		    : D.DefinitionId == TEXT("SecondarySchool") ? TEXT("Basic to Secondary")
		                                                : TEXT("None to Secondary"),
		    T.SchoolMonthsPerLevel);
	if (FCityBuildingDefinition::IsWaterFacility(D.DefinitionId))
		return TEXT(
		    "Water follows road pipes: -2% pressure/step\n60%+ permits full flow; 20-59% partial; below20% none\nBlue pressure can still have too little source capacity");
	if (FCityBuildingDefinition::IsPowerFacility(D.DefinitionId))
		return TEXT("Power serves connected buildings citywide; limited by total output");
	if (D.ParkDesirabilityBonus > 0 || D.DefinitionId == T.ParkDefinitionId)
		return FString::Printf(
		    TEXT("Park benefit within %d grid steps from its edge; strongest overlapping benefit wins"),
		    D.ParkDesirabilityBonus > 0 ? D.ParkRadiusCells : T.ParkDesirabilityRadiusCells);
	if (const auto* Level = D.FindDensityLevel(ECityDensityLevel::Low); Level && Level->CivicRoadRange > 0)
		return FString::Printf(
		    TEXT(
		        "Starting reach %d road steps; follows connected roads\nEffective service also needs staff, capacity and utilities"),
		    Level->CivicRoadRange);
	return FString();
}

FString FCityServiceGuidance::Water(const FCityPlacementModel& City, const FCityServiceSnapshot& S, uint64 Id)
{
	const auto* Entity = City.GetEntities().Find(Id);
	const int32 Demand = S.Water.EntityDemand.FindRef(Id);
	if (!Entity || Demand <= 0)
		return FString();
	FString Text =
	    FString::Printf(TEXT("Water delivered %d/%d | pipe pressure %d%%"), S.Water.EntityDelivered.FindRef(Id), Demand,
	                    S.Water.EntityPressurePercent.FindRef(Id));
	switch (S.Water.EntityCauses.FindRef(Id))
	{
	case ECityWaterFailure::Disconnected:
		Text += TEXT("\nFix water: connect building frontage to a working source by road pipes");
		break;
	case ECityWaterFailure::SourceOutage:
		Text += TEXT("\nFix water: inspect the connected source for power, damage, freight or staffing");
		break;
	case ECityWaterFailure::LowPressure:
		Text += TEXT("\nFix water: add a closer source or shorten the pipe route; full flow needs60% pressure");
		break;
	case ECityWaterFailure::SourceCapacity:
		Text += TEXT(
		    "\nFix water: add/upgrade a connected source, improve its staffing or choose higher groundwater yield\nBlue pipes show pressure; source output is shared by buildings");
		break;
	default:
		break;
	}
	TArray<FCityCellCoord> Roads;
	City.GetAdjacentRoadCells(Entity->Anchor, Entity->FootprintSize, true, Roads, S.Water.RulesVersion > 0);
	const FCityWaterSource* Closest = nullptr;
	int32 Hops = MAX_int32;
	for (const auto& Source : S.Water.Sources)
	{
		const auto* Provider = City.GetEntities().Find(Source.EntityId);
		if (!Provider)
			continue;
		TArray<FCityCellCoord> Starts;
		City.GetAdjacentRoadCells(Provider->Anchor, Provider->FootprintSize, true, Starts, S.Water.RulesVersion > 0);
		TMap<FCityCellCoord, int32> Distances;
		City.GetRoadReach(Starts, MAX_int32, true, Distances);
		for (const auto& Road : Roads)
			if (const auto* D = Distances.Find(Road);
			    D && (*D < Hops || (*D == Hops && (!Closest || Source.EntityId < Closest->EntityId))))
			{
				Hops = *D;
				Closest = &Source;
			}
	}
	if (Closest)
	{
		const auto* Provider = City.GetEntities().Find(Closest->EntityId);
		if (Provider)
			Text += FString::Printf(
			    TEXT(
			        "\nNearest connected source #%llu (%d,%d): %d road steps | %s\nSource output used %d/%d | remaining %d"),
			    Closest->EntityId, Provider->Anchor.X, Provider->Anchor.Y, Hops,
			    Closest->bOperational ? TEXT("active") : TEXT("offline"), Closest->Used, Closest->Capacity,
			    FMath::Max(0, Closest->Capacity - Closest->Used));
	}
	return Text;
}

FString FCityServiceGuidance::Education(const FCityPlacementModel& City, const FCityServiceSnapshot& S,
                                        const FCityPopulationSnapshot& P, const FCityEconomyTuning& T,
                                        const FCityDevelopmentSnapshot& Development, uint64 Id)
{
	const auto* Home = City.GetEntities().Find(Id);
	if (!Home)
		return FString();
	int32 Residents[4] = {}, Served[4] = {};
	for (const auto& C : P.Citizens)
		if (C.HomeEntityId == Id)
		{
			const int32 Tier = FMath::Clamp(static_cast<int32>(C.Education), 0, 3);
			++Residents[Tier];
			if (S.EducationCitizenIds.Contains(C.Id) || S.HigherEducationCitizenIds.Contains(C.Id))
				++Served[Tier];
		}
	FString Text = FString::Printf(
	    TEXT(
	        "Education places %d/%d residents seeking study\nSchool distance follows roads, not straight-line proximity (limit %d steps)"),
	    S.HomeEducationPlaces.FindRef(Id), S.HomeEducationDemand.FindRef(Id),
	    FMath::Clamp(T.SchoolRoadRangeHops, 1, 64));
	TArray<uint64> Schools;
	for (const auto& E : City.GetEntities())
		if (FCityBuildingDefinition::IsSchoolFacility(E.Value.DefinitionId))
			Schools.Add(E.Key);
	Schools.Sort();
	TMap<uint64, FCityServiceReach> Reaches;
	for (const uint64 School : Schools)
		FCityServiceEvaluator::BuildProviderReach(City, School, MAX_int32, true, Reaches.Add(School));
	for (int32 Tier = 0; Tier < 3; ++Tier)
	{
		if (!Residents[Tier] || Served[Tier] == Residents[Tier])
			continue;
		const int32 Missing = Residents[Tier] - Served[Tier];
		const TCHAR* Needed = Tier == 0   ? TEXT("Primary School or School")
		                      : Tier == 1 ? TEXT("Secondary School or School")
		                                  : TEXT("College");
		Text += FString::Printf(TEXT("\n%d need %s to %s: "), Missing, EducationName(Tier), EducationName(Tier + 1));
		uint64 Best = 0;
		int32 BestRank = MAX_int32, BestHops = MAX_int32;
		for (const uint64 School : Schools)
		{
			if (!FCityBuildingDefinition::CanTeachEducation(City.GetEntities().FindChecked(School).DefinitionId, Tier))
				continue;
			const int32 Hops = Distance(City, Reaches.FindChecked(School), *Home);
			const bool InRange = Hops <= FMath::Clamp(T.SchoolRoadRangeHops, 1, 64);
			const bool Operating = S.SchoolCapacities.FindRef(School) > 0;
			const int32 Rank = InRange ? (Operating ? 0 : 1) : 2;
			if (!Best || Rank < BestRank || (Rank == BestRank && Hops < BestHops))
			{
				Best = School;
				BestRank = Rank;
				BestHops = Hops;
			}
		}
		if (!Best)
		{
			Text += FString::Printf(TEXT("build %s; other school types cannot teach this tier"), Needed);
			continue;
		}
		const auto& School = City.GetEntities().FindChecked(Best);
		Text += FString::Printf(TEXT("%s #%llu (%d,%d)"), *School.DefinitionId.ToString(), Best, School.Anchor.X,
		                        School.Anchor.Y);
		if (BestHops == MAX_int32)
			Text += TEXT(" has no connected road path; connect both frontages to the same road network");
		else if (BestHops > FMath::Clamp(T.SchoolRoadRangeHops, 1, 64))
			Text += FString::Printf(TEXT(" is %d road steps away; shorten the route or build closer"), BestHops);
		else
		{
			Text += FString::Printf(TEXT(" at %d road steps: "), BestHops);
			if (!Development.IsOperational(Best))
				Text += TEXT("not open; finish construction and opening freight");
			else if (S.GetFailure(Best) != ECityOperationFailure::None)
				Text += TEXT("offline: ") + FCityServiceEvaluator::FailureToString(S.GetFailure(Best)) +
				        TEXT("; inspect school to restore service");
			else if (S.SchoolCapacities.FindRef(Best) <= 0)
				Text += TEXT("no staffed places; provide qualified staff and pay public bills");
			else if (!S.EntityResidents.FindRef(Id))
				Text += TEXT("home not operational; restore home utilities/road access first");
			else if (S.SchoolUsedCapacity.FindRef(Best) >= S.SchoolCapacities.FindRef(Best))
				Text += FString::Printf(
				    TEXT(
				        "places used %d/%d; add another compatible school or restore staffing\nSchool staff %d%%; places are shared across homes"),
				    S.SchoolUsedCapacity.FindRef(Best), S.SchoolCapacities.FindRef(Best),
				    S.PublicStaffPercent.FindRef(Best));
			else
				Text += TEXT("places available; restore the home's operating access and allow the next service update");
		}
	}
	if (Residents[3])
		Text += FString::Printf(TEXT("\nHigher educated %d: no further schooling needed"), Residents[3]);
	return Text;
}

FString FCityServiceGuidance::Employment(const FCityPopulationSnapshot& P, const FCityServiceSnapshot& S, uint64 Id,
                                         bool bHome)
{
	int32 Assigned[4] = {}, Active = 0, Unemployed[4] = {}, HomeJobs = 0;
	for (const auto& C : P.Citizens)
	{
		const int32 Tier = FMath::Clamp(static_cast<int32>(C.Education), 0, 3);
		if (C.WorkplaceEntityId == Id)
		{
			++Assigned[Tier];
			Active += !C.IsDetained() && !C.bOnMedicalLeave;
		}
		if (C.HomeEntityId == Id && bHome)
		{
			HomeJobs += C.WorkplaceEntityId != 0;
			if (!C.WorkplaceEntityId && !C.IsDetained() && !C.bOnMedicalLeave)
				++Unemployed[Tier];
		}
	}
	const auto* Business = P.Businesses.FindByPredicate([&](const auto& B) { return B.EntityId == Id; });
	const auto* Public = P.PublicEmployers.FindByPredicate([&](const auto& E) { return E.EntityId == Id; });
	if (!bHome && !Business && !Public)
		return FString();
	if (!bHome)
	{
		const int32 Required =
		    Public ? (S.PublicMinimumEducation.Contains(Id) ? S.PublicMinimumEducation.FindRef(Id) : 1) : 0;
		int32 Available = 0, Qualified = 0;
		for (const auto& C : P.Citizens)
			if (!C.WorkplaceEntityId && !C.IsDetained() && !C.bOnMedicalLeave)
			{
				Available += static_cast<int32>(C.Education) >= Required;
				Qualified += C.Education >= ECityEducationLevel::Secondary;
			}
		FString Text = FString::Printf(
		    TEXT(
		        "Staff qualifications: None %d | Basic %d | Secondary %d | Higher %d\nActive staff %d; assigned staff on leave/in custody %d"),
		    Assigned[0], Assigned[1], Assigned[2], Assigned[3], Active,
		    Assigned[0] + Assigned[1] + Assigned[2] + Assigned[3] - Active);
		if (Public)
			Text += FString::Printf(
			    TEXT(
			        "\nHiring requires %s or higher; %d eligible unemployed citywide\nIf short: educate residents, grow housing and check public payroll"),
			    EducationName(Required), Available);
		else
			Text += FString::Printf(
			    TEXT(
			        "\nEntry roles accept any education; skilled roles require Secondary+\nAvailable unemployed: entry %d | Secondary+ %d\nIf short: grow housing or provide Secondary education; public employers hire first"),
			    Available, Qualified);
		return Text;
	}
	int32 Entry = 0, Skilled = 0, Civic[4] = {};
	for (const auto& B : P.Businesses)
	{
		int32 FilledEntry = 0, FilledSkilled = 0;
		for (const auto& C : P.Citizens)
			if (C.WorkplaceEntityId == B.EntityId)
				(C.bSkilledJob ? FilledSkilled : FilledEntry)++;
		Entry += FMath::Max(0, B.JobCapacity - B.SkilledJobCapacity - FilledEntry);
		Skilled += FMath::Max(0, B.SkilledJobCapacity - FilledSkilled);
	}
	for (const auto& E : P.PublicEmployers)
	{
		const int32 Minimum =
		    S.PublicMinimumEducation.Contains(E.EntityId) ? S.PublicMinimumEducation.FindRef(E.EntityId) : 1;
		Civic[FMath::Clamp(Minimum, 0, 3)] += FMath::Max(0, E.JobCapacity - P.Workers(E.EntityId));
	}
	FString Text = FString::Printf(
	    TEXT(
	        "Work: %d residents assigned jobs\nReady unemployed: None %d | Basic %d | Secondary %d | Higher %d\nCity vacancies: entry %d | skilled (Secondary+) %d\nPublic vacancies by minimum: None %d | Basic %d | Secondary %d | Higher %d\nHiring is citywide; nearby jobs have no separate distance radius"),
	    HomeJobs, Unemployed[0], Unemployed[1], Unemployed[2], Unemployed[3], Entry, Skilled, Civic[0], Civic[1],
	    Civic[2], Civic[3]);
	if (Unemployed[0] + Unemployed[1] + Unemployed[2] + Unemployed[3] > 0)
		Text +=
		    TEXT("\nFix work: add operating shops/industry for entry jobs, or teach the tier needed by vacant roles");
	return Text;
}
