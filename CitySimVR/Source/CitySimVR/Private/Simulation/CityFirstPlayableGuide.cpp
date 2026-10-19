#include "Simulation/CityFirstPlayableGuide.h"

FString FCityFirstPlayableGuide::DescribeNextStep(const FCityFirstPlayableGuideState& State)
{
	if (State.ScenarioPhase == ECityScenarioPhase::NotStarted)
	{
		return TEXT("Next: point at OBJECTIVES and trigger START");
	}
	if (State.ScenarioPhase == ECityScenarioPhase::Succeeded)
	{
		return TEXT("Next: scenario complete; SAVE keeps this city");
	}
	if (!State.bHasConnectedEntranceRoad)
	{
		return TEXT("Next: ROAD from the white entrance marker");
	}
	if (State.OperationalPowerBuildings == 0)
	{
		return State.BuiltPowerBuildings == 0 ? TEXT("Next: place POWER beside the connected road")
		                                      : TEXT("Next: connect POWER to the entrance road");
	}
	if (State.OperationalServiceBuildings == 0)
	{
		return State.BuiltServiceBuildings == 0 ? TEXT("Next: place WATER TREATMENT beside the connected road")
		                                        : TEXT("Next: connect WATER TREATMENT to the entrance road");
	}
	if (!State.bHasResidentialZone)
	{
		return TEXT("Next: ZONE a 2x2 residential area beside the road");
	}
	if (State.Residents > State.Jobs && !State.bHasCommercialZone)
	{
		return TEXT("Next: in ZONE, A/X switches to commercial; add jobs");
	}
	if (State.Residents < 100)
	{
		return TEXT("Next: add watered residential/commercial zones to reach 100");
	}
	return FString::Printf(TEXT("Next: keep income above upkeep for %d more month%s (3X speeds time)"),
	                       FMath::Max(0, 3 - State.ProfitableMonthStreak),
	                       State.ProfitableMonthStreak == 2 ? TEXT("") : TEXT("s"));
}

namespace
{
	FName FirstIncomplete(const TArray<FName>& Completed)
	{
		for (const FName Id : FCityFirstPlayableGuide::MilestoneIds())
			if (!Completed.Contains(Id))
				return Id;
		return TEXT("complete");
	}

	FString FundsHint(const int64 Funds, const int64 Cost)
	{
		return FString::Printf(TEXT("Needs $%lld; treasury $%lld.\nEarn income or use Settings > Reset."), Cost, Funds);
	}
} // namespace

const TArray<FName>& FCityFirstPlayableGuide::MilestoneIds()
{
	static const TArray<FName> Ids = {TEXT("entrance_road"), TEXT("power"),           TEXT("water"),
	                                  TEXT("homes_zone"),    TEXT("first_residents"), TEXT("first_jobs"),
	                                  TEXT("goods"),         TEXT("basic_services"),  TEXT("population"),
	                                  TEXT("stability"),     TEXT("police"),          TEXT("fire"),
	                                  TEXT("school"),        TEXT("education"),       TEXT("park_expansion"),
	                                  TEXT("hospital"),      TEXT("health_recovery"), TEXT("waste_collection"),
	                                  TEXT("recycling")};
	return Ids;
}

void FCityFirstPlayableGuide::UpdateProgress(const FCityFirstPlayableGuideState& State, FCityGuidedProgress& Progress)
{
	if (State.ScenarioPhase == ECityScenarioPhase::NotStarted)
		return;
	const bool Conditions[] = {State.bHasConnectedEntranceRoad,
	                           State.OperationalPowerBuildings > 0,
	                           State.OperationalServiceBuildings > 0,
	                           State.bHasResidentialZone || State.Residents > 0,
	                           State.Residents > 0,
	                           State.Jobs > 0,
	                           State.CompletedPurchases > 0,
	                           State.Residents > 0 && State.Jobs > 0 && State.bHasConnectedEntranceRoad &&
	                               State.OperationalPowerBuildings > 0 && State.OperationalServiceBuildings > 0 &&
	                               State.PowerShortage == 0 && State.WaterShortage == 0 && State.UpkeepShortfall == 0,
	                           State.Residents >= 100,
	                           State.ScenarioPhase == ECityScenarioPhase::Succeeded || State.ProfitableMonthStreak >= 3,
	                           State.bCrimeIntroduced && State.OperationalPolice > 0,
	                           State.bFireIntroduced && State.OperationalFire > 0,
	                           State.OperationalSchools > 0 && State.EducationServed > 0,
	                           State.SecondaryResidents > 0,
	                           State.OperationalLargeParks > 0,
	                           State.HospitalCapacity > 0,
	                           State.RecoveredResidents > 0,
	                           State.WasteFleet > 0 && State.WasteCollected > 0,
	                           State.WasteRecycled > 0};
	TArray<FName> Completed;
	for (int32 Index = 0; Index < MilestoneIds().Num(); ++Index)
		if (Conditions[Index] || Progress.CompletedIds.Contains(MilestoneIds()[Index]))
			Completed.Add(MilestoneIds()[Index]);
	Progress.CompletedIds = MoveTemp(Completed);
	Progress.CurrentId = FirstIncomplete(Progress.CompletedIds);
}

bool FCityFirstPlayableGuide::ValidateProgress(const FCityGuidedProgress& Progress, FString& OutError)
{
	int32 PreviousIndex = INDEX_NONE;
	for (const FName Id : Progress.CompletedIds)
	{
		const int32 Index = MilestoneIds().Find(Id);
		if (Index == INDEX_NONE || Index <= PreviousIndex)
		{
			OutError = TEXT("Invalid, duplicated or unordered guided milestone ID");
			return false;
		}
		PreviousIndex = Index;
	}
	if (Progress.CurrentId != FirstIncomplete(Progress.CompletedIds))
	{
		OutError = TEXT("Guided current stage does not match completed milestones");
		return false;
	}
	return true;
}

FCityGuidedObjective FCityFirstPlayableGuide::BuildMilestone(const FName Id, const FCityFirstPlayableGuideState& State)
{
	if (Id == TEXT("entrance_road"))
		return {Id,
		        TEXT("Connect to the outside"),
		        TEXT("Roads bring people and deliveries."),
		        TEXT("Build > Roads > LOCAL: start at the\nwhite entrance marker on the map edge."),
		        FString::Printf(TEXT("Entrance road: %d / 1"), State.bHasConnectedEntranceRoad ? 1 : 0),
		        State.bHasConnectedEntranceRoad ? 1.0f : 0.0f};
	if (Id == TEXT("power"))
		return {Id,
		        TEXT("Bring power online"),
		        TEXT("Homes and businesses need power."),
		        State.BuiltPowerBuildings == 0
		            ? (State.PowerCost > State.Funds
		                   ? FundsHint(State.Funds, State.PowerCost)
		                   : TEXT("Build > Power: place beside a road\nconnected to the entrance."))
		            : TEXT("Inspect the power plant. Check its\nroad access and operating needs."),
		        FString::Printf(TEXT("Operating power plants: %d / 1"), State.OperationalPowerBuildings),
		        State.OperationalPowerBuildings > 0 ? 1.0f : 0.0f};
	if (Id == TEXT("water"))
		return {Id,
		        TEXT("Provide clean water"),
		        TEXT("Water coverage supports occupied homes."),
		        State.BuiltServiceBuildings == 0
		            ? (State.WaterCost > State.Funds
		                   ? FundsHint(State.Funds, State.WaterCost)
		                   : TEXT("Build > WATER: place beside\na connected road near future homes."))
		            : TEXT("Inspect water treatment. Check its\nroad access and operating needs."),
		        FString::Printf(TEXT("Operating water plants: %d / 1"), State.OperationalServiceBuildings),
		        State.OperationalServiceBuildings > 0 ? 1.0f : 0.0f};
	if (Id == TEXT("homes_zone"))
		return {Id,
		        TEXT("Make room for homes"),
		        TEXT("Residential zones allow homes to grow."),
		        TEXT("Zone > Residential: paint a 2x2 area\nbeside a connected, serviced road."),
		        TEXT("Residential zoning: 0 / 1"),
		        0};
	if (Id == TEXT("first_residents"))
		return {Id,
		        TEXT("Welcome your first residents"),
		        TEXT("Completed, serviced homes can fill."),
		        TEXT("Run time; delivery precedes construction.\nInspect delivery and utility needs."),
		        TEXT("Residents: 0 / 1"),
		        0};
	if (Id == TEXT("first_jobs"))
		return {Id,
		        TEXT("Create local jobs"),
		        TEXT("Businesses provide work for residents."),
		        State.bHasCommercialZone
		            ? TEXT("Run time. Inspect businesses for\ndelivery, goods and utility needs.")
		            : TEXT("Zone > Commercial: paint beside a\nconnected road with power and water."),
		        TEXT("Jobs: 0 / 1"),
		        0};
	if (Id == TEXT("goods"))
		return {Id,
		        TEXT("Supply household goods"),
		        TEXT("A resident must complete a shop visit."),
		        !State.bHasIndustrialZone
		            ? TEXT("Zone > Industrial: privately built.\nInspect shop stock and truck routes.")
		            : TEXT("Run time for delivery and shopping.\nInspect shops for stock and access."),
		        FString::Printf(TEXT("Completed purchases: %lld / 1"), State.CompletedPurchases),
		        State.CompletedPurchases > 0 ? 1.0f : 0.0f};
	if (Id == TEXT("basic_services"))
	{
		const int32 Met = (State.PowerShortage == 0 ? 1 : 0) + (State.WaterShortage == 0 ? 1 : 0) +
		                  (State.UpkeepShortfall == 0 ? 1 : 0);
		return {Id,
		        TEXT("Stabilize basic services"),
		        TEXT("Serve occupied homes and local jobs."),
		        State.UpkeepShortfall > 0
		            ? TEXT("City > TAXES: raise income, then Apply.\nTrim unused roads; run to a paid month.")
		            : TEXT("Use Power/Water overlays for gaps.\nCheck upkeep and income in City summary."),
		        FString::Printf(TEXT("Power gap %d | Water gap %d\nUpkeep shortfall $%lld"), State.PowerShortage,
		                        State.WaterShortage, State.UpkeepShortfall),
		        Met / 3.0f};
	}
	if (Id == TEXT("population"))
		return {Id,
		        TEXT("Grow to 100 residents"),
		        TEXT("Balance homes, jobs and utility needs."),
		        TEXT("Expand serviced zones. City summary\nand Inspect explain growth shortages."),
		        FString::Printf(TEXT("Residents: %d / 100"), State.Residents),
		        FMath::Clamp(State.Residents / 100.0f, 0.0f, 1.0f)};
	if (Id == TEXT("stability"))
		return {Id,
		        TEXT("Establish a stable city"),
		        TEXT("Keep 100 residents and a monthly profit."),
		        TEXT("Income must exceed upkeep at billing.\nA failed month resets this streak."),
		        FString::Printf(TEXT("Profitable months: %d / 3"), State.ProfitableMonthStreak),
		        FMath::Clamp(State.ProfitableMonthStreak / 3.0f, 0.0f, 1.0f)};
	if (Id == TEXT("police") || Id == TEXT("fire"))
	{
		const bool Fire = Id == TEXT("fire");
		const bool Introduced = Fire ? State.bFireIntroduced : State.bCrimeIntroduced;
		const int32 Operating = Fire ? State.OperationalFire : State.OperationalPolice;
		const int32 Built = Fire ? State.BuiltFire : State.BuiltPolice;
		const int32 Threshold = FMath::Max(1, Fire ? State.FirePopulationThreshold : State.CrimePopulationThreshold);
		const int32 Grace = FMath::Max(0, Fire ? State.FireGraceMonths : State.CrimeGraceMonths);
		const int64 Cost = Fire ? State.FireCost : State.PoliceCost;
		const TCHAR* Department = Fire ? TEXT("fire") : TEXT("police");
		const FString Title =
		    !Introduced ? FString::Printf(TEXT("Grow to %d residents"), Threshold)
		    : Operating > 0
		        ? FString::Printf(TEXT("%s protection ready"), Fire ? TEXT("Fire") : TEXT("Police"))
		        : FString::Printf(TEXT("%s %s protection"), Built > 0 ? TEXT("Restore") : TEXT("Provide"), Department);
		const FString Reason = FString::Printf(TEXT("%s begin at %d residents after %d month%s."),
		                                       Fire ? TEXT("Fires") : TEXT("Crime incidents"), Threshold, Grace,
		                                       Grace == 1 ? TEXT("") : TEXT("s"));
		const FString Hint =
		    !Introduced
		        ? Operating > 0
		              ? FString::Printf(TEXT("Expand serviced homes and jobs.\nYour %s station is ready for growth."),
		                                Department)
		              : FString::Printf(TEXT("Grow serviced homes and jobs.\nBuild > %s can prepare protection now."),
		                                Fire ? TEXT("FIRE") : TEXT("POLICE"))
		    : Built > 0 ? TEXT("Inspect station: roads, utilities, staff\nand paid wages enable protection.")
		    : Cost > State.Funds
		        ? FundsHint(State.Funds, Cost)
		        : FString::Printf(TEXT("Build > %s beside serviced roads.\nCity > %s VIEW shows exposed lots."),
		                          Fire ? TEXT("FIRE") : TEXT("POLICE"), Fire ? TEXT("FIRE") : TEXT("CRIME"));
		const FString Progress = !Introduced
		                             ? FString::Printf(TEXT("Residents %d / %d\nOperating %s stations %d / 1"),
		                                               State.Residents, Threshold, Department, Operating)
		                             : FString::Printf(TEXT("Operating %s stations %d / 1"), Department, Operating);
		const float Population =
		    Introduced ? 1.0f : FMath::Clamp(static_cast<float>(State.Residents) / Threshold, 0.0f, 1.0f);
		const float Fraction = (Population + (Operating > 0 ? 1.0f : 0.0f)) * 0.5f;
		return {Id, Title, Reason, Hint, Progress, Introduced ? Fraction : FMath::Min(Fraction, 0.99f)};
	}
	if (Id == TEXT("school"))
		return {Id,
		        TEXT("Open a public school"),
		        TEXT("Schools grow skills; parks improve nearby home appeal."),
		        TEXT("Build > EDUCATION > SECONDARY beside serviced roads.\nCity > EDUCATION shows resident places."),
		        FString::Printf(TEXT("Operating schools %d | resident places %d"), State.OperationalSchools,
		                        State.EducationServed),
		        State.OperationalSchools > 0 && State.EducationServed > 0 ? 1.0f : 0.0f};
	if (Id == TEXT("waste_collection"))
		return {
		    Id,
		    TEXT("Collect city waste"),
		    TEXT("Waste begins at350 residents with an operating hospital; effects have six months grace."),
		    TEXT(
		        "Build > WASTE > WASTE DEPOT. Fund Basic staff and utilities.\nTrucks plan largest piles within32 road cells; normal traffic applies."),
		    FString::Printf(TEXT("Trucks %d | collected %lld units"), State.WasteFleet, State.WasteCollected),
		    State.WasteCollected > 0 && State.WasteFleet > 0 ? 1.0f : 0.0f};
	if (Id == TEXT("recycling"))
		return {
		    Id,
		    TEXT("Recycle collected material"),
		    TEXT("Recycling diverts60% of processed waste; residue is disposed."),
		    TEXT(
		        "Build > WASTE > RECYCLING. Hire Secondary staff.\nKeep storage/processing and collection routes available."),
		    FString::Printf(TEXT("Recycled %lld units"), State.WasteRecycled),
		    State.WasteRecycled > 0 ? 1.0f : 0.0f};
	if (Id == TEXT("hospital"))
		return {Id,
		        TEXT("Open a hospital"),
		        TEXT("Later city growth introduces sickness."),
		        TEXT("Build > HOSPITAL: connect roads and utilities.\nHire six Secondary staff; keep payroll funded."),
		        FString::Printf(TEXT("Treatment places available: %d"), State.HospitalCapacity),
		        State.HospitalCapacity > 0 ? 1.0f : 0.0f};
	if (Id == TEXT("health_recovery"))
		return {
		    Id,
		    TEXT("Treat an ill resident"),
		    TEXT(
		        "Sickness begins after250 residents and25 skilled citizens, with safety established and six months grace."),
		    TEXT(
		        "Mild cases walk or drive to a clinic, then recover at home.\nSevere cases need an ambulance and hospital arrival."),
		    FString::Printf(TEXT("Ill %d | recovered %lld"), State.IllResidents, State.RecoveredResidents),
		    State.RecoveredResidents > 0 ? 1.0f : 0.0f};
	if (Id == TEXT("education"))
		return {Id,
		        TEXT("Develop skilled workers"),
		        TEXT("Secondary education opens higher-paid roles."),
		        TEXT("Keep school access for6 served months.\nLarger shops/factories offer skilled roles."),
		        FString::Printf(TEXT("Secondary or higher residents: %d/1"), State.SecondaryResidents),
		        State.SecondaryResidents > 0 ? 1.0f : 0.0f};
	if (Id == TEXT("park_expansion"))
		return {
		    Id,
		    TEXT("Invest in a larger park"),
		    TEXT("Parks raise nearby home appeal without adding jobs or utilities."),
		    TEXT(
		        "Build > PARKS: choose NEIGHBORHOOD or CITY PARK.\nPARK VIEW shows reach; inspect upkeep before buying."),
		    FString::Printf(TEXT("Operating larger parks: %d/1"), State.OperationalLargeParks),
		    State.OperationalLargeParks > 0 ? 1.0f : 0.0f};
	return {TEXT("complete"),
	        TEXT("City guide complete!"),
	        TEXT("Your teaching milestones are saved."),
	        TEXT("Keep expanding at your own pace.\nGame Settings > SAVE keeps your city."),
	        FString::Printf(TEXT("All %d milestones completed"), MilestoneIds().Num()),
	        1};
}

FCityGuidedObjective FCityFirstPlayableGuide::BuildObjective(const FCityFirstPlayableGuideState& State)
{
	FCityGuidedProgress Progress;
	UpdateProgress(State, Progress);
	return BuildObjective(State, Progress);
}

FCityGuidedObjective FCityFirstPlayableGuide::BuildObjective(const FCityFirstPlayableGuideState& State,
                                                             const FCityGuidedProgress& Progress)
{
	if (State.ScenarioPhase == ECityScenarioPhase::NotStarted)
		return {TEXT("start"),
		        TEXT("Start your city"),
		        TEXT("Build a city of 100 residents."),
		        TEXT("Point at this card and pull trigger.\nYour existing city will be kept."),
		        TEXT("START  >"),
		        0};
	FName RecoveryId;
	if (Progress.CompletedIds.Contains(TEXT("entrance_road")) && !State.bHasConnectedEntranceRoad)
		RecoveryId = TEXT("entrance_road");
	else if (Progress.CompletedIds.Contains(TEXT("power")) && State.OperationalPowerBuildings == 0)
		RecoveryId = TEXT("power");
	else if (Progress.CompletedIds.Contains(TEXT("water")) && State.OperationalServiceBuildings == 0)
		RecoveryId = TEXT("water");
	else if (Progress.CompletedIds.Contains(TEXT("basic_services")) &&
	         (State.PowerShortage > 0 || State.WaterShortage > 0 || State.UpkeepShortfall > 0))
		RecoveryId = TEXT("basic_services");
	FCityGuidedObjective Card = BuildMilestone(RecoveryId.IsNone() ? Progress.CurrentId : RecoveryId, State);
	if (RecoveryId.IsNone() && State.bCrimeIntroduced && State.OperationalPolice == 0)
		Card = BuildMilestone(TEXT("police"), State);
	if (RecoveryId.IsNone() && State.bFireIntroduced && State.OperationalFire == 0)
		Card = BuildMilestone(TEXT("fire"), State);
	if (RecoveryId.IsNone() && State.IllResidents > 0 && State.HospitalCapacity == 0)
		Card = BuildMilestone(TEXT("hospital"), State);
	if (RecoveryId.IsNone() && State.BurningBuildings > 0)
		Card = {TEXT("fire_response"),
		        TEXT("Fire response needed"),
		        TEXT("Burning buildings stop operating."),
		        TEXT("City > FIRE VIEW shows burning lots.\nInspect FIRE for road/water/upkeep gaps."),
		        FString::Printf(TEXT("Burning buildings: %d"), State.BurningBuildings),
		        0};
	else if (RecoveryId.IsNone() && State.DamagedBuildings > 0)
		Card = {TEXT("fire_repair"),
		        TEXT("Repair fire damage"),
		        TEXT("Repairs restore housing, jobs and services."),
		        TEXT("Inspect damage for cost and time.\nCity > REPAIR: trigger on a damaged lot."),
		        FString::Printf(TEXT("Damaged/repairing lots: %d"), State.DamagedBuildings),
		        0};
	if (!RecoveryId.IsNone())
	{
		Card.Title = TEXT("Restore: ") + Card.Title;
		Card.Reason = TEXT("An earlier service needs attention.");
	}
	Card.Progress +=
	    FString::Printf(TEXT("\nGuide milestones: %d / %d"), Progress.CompletedIds.Num(), MilestoneIds().Num());
	return Card;
}

FCityGuidedObjective FCityFirstPlayableGuide::BuildCompletedObjective(const FName Id, const int32 Index,
                                                                      const int32 Total)
{
	FCityGuidedObjective Card = BuildMilestone(Id, {});
	Card.Reason = TEXT("Completed teaching milestone.");
	Card.Hint = TEXT("History stays recorded after changes.\nChoose LIVE to see the next action.");
	Card.Progress = FString::Printf(TEXT("Completed milestone: %d / %d"), Index + 1, Total);
	Card.Fraction = 1;
	return Card;
}
