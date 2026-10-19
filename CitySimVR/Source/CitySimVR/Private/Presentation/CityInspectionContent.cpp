#include "Presentation/CityUiContent.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Simulation/CityTechnology.h"
#include "Simulation/CityCommercialAi.h"
#include "Simulation/CityHappinessSimulation.h"
#include "Presentation/CityConditionIndicators.h"

namespace
{
	FString Money(int64 Value) { return TEXT("$") + FText::AsNumber(Value).ToString(); }
	void Row(FCityUiInspectionSection& Section, const TCHAR* Label, const FString& Value)
	{
		FCityUiGauge R; R.Label = FText::FromString(Label); R.Value = FText::FromString(Value); Section.Rows.Add(R);
	}
	void Gauge(FCityUiInspectionSection& Section, const TCHAR* Label, int64 Available, int64 Needed, bool Fulfilment = true)
	{
		FCityUiGauge G; G.Label = FText::FromString(Label);
		G.Value = FText::FromString(FString::Printf(TEXT("%lld / %lld"), Available, Needed));
		G.Fill = Needed > 0 ? FMath::Clamp(double(Available) / Needed, 0., 1.) : 0.;
		G.Color = Fulfilment && Available < Needed ? FLinearColor(1.f, .64f, .25f) : FLinearColor(.3f, .76f, .85f);
		Section.Gauges.Add(G);
	}
	int32 Route(const FCityPlacementModel& City, const FCityServiceReach& Reach, const FCityPlacedEntity& Entity)
	{
		TArray<FCityCellCoord> Roads;
		FCityServiceEvaluator::GetAdjacentRoadCells(City, Entity.Anchor, Entity.FootprintSize, false, Roads);
		int32 Best = MAX_int32;
		for (const auto& Road : Roads)
			if (const auto* H = Reach.RoadHopDistances.Find(Road)) Best = FMath::Min(Best, *H);
		return Best;
	}
}

void CityUiContent::InspectionDetails(const UCitySimulationSubsystem& Sim, uint64 Id, FCityUiSnapshot& Out)
{
	if (Out.InspectionSections.Num() != 6) return;
	const auto& City = Sim.GetPlacementModel();
	const auto* Entity = City.GetEntities().Find(Id);
	if (!Entity) return;
	const auto& Services = Sim.GetServiceSnapshot();
	const auto& People = Sim.GetPopulationSnapshot();
	const auto* D = Sim.GetBuildingCatalog() ? Sim.GetBuildingCatalog()->FindDefinition(Entity->DefinitionId) : nullptr;
	const auto* State = Sim.GetDevelopmentSnapshot().Find(Id);
	const auto* Level = D ? D->FindDensityLevel(State ? State->DensityLevel : ECityDensityLevel::Low) : nullptr;
	for (auto& Section : Out.InspectionSections)
	{
		Section.Help = Section.Detail;
		Section.Detail = FText::GetEmpty(); Section.Rows.Reset();
	}
	auto& Utilities = Out.InspectionSections[0];
	auto& Work = Out.InspectionSections[1];
	auto& Study = Out.InspectionSections[2];
	auto& Health = Out.InspectionSections[3];
	auto& Wellbeing = Out.InspectionSections[4];
	auto& Safety = Out.InspectionSections[5];
	// Existing inspection gauges keep less common facility metrics, but use short labels.
	for (auto& Section : Out.InspectionSections)
		for (auto& G : Section.Gauges)
		{
			FString L = G.Label.ToString();
			L = L.Replace(TEXT("Water delivered"), TEXT("Water / need"))
			    .Replace(TEXT("Water source used"), TEXT("Source / limit"))
			    .Replace(TEXT("Education access"), TEXT("Study / need"))
			    .Replace(TEXT("Secondary or higher"), TEXT("Secondary+"))
			    .Replace(TEXT("Treatment places used"), TEXT("Patients / beds"))
			    .Replace(TEXT("Resident places used"), TEXT("Places / limit"))
			    .Replace(TEXT("College places used"), TEXT("Places / limit"))
			    .Replace(TEXT("Payroll last day $"), TEXT("Payroll $"))
			    .Replace(TEXT("Discontented residents"), TEXT("Discontented"))
			    .Replace(TEXT("Skilled roles filled"), TEXT("Skilled / jobs"))
			    .Replace(TEXT("Output next day"), TEXT("Output / orders"))
			    .Replace(TEXT("Sales today"), TEXT("Sales / orders"))
			    .Replace(TEXT("Purchases today"), TEXT("Buys / need"))
			    .Replace(TEXT("Purchases last day"), TEXT("Yesterday / need"))
			    .Replace(TEXT("Fire capacity used"), TEXT("Fire / limit"))
			    .Replace(TEXT("Protection capacity used"), TEXT("Police / limit"))
			    .Replace(TEXT("Jail spaces occupied"), TEXT("Jail / limit"));
			G.Label = FText::FromString(L);
		}
	const int32 PowerNeed = Level ? Level->PowerDemand : D ? D->PowerDemand : 0;
	if (PowerNeed)
	{
		Row(Utilities, TEXT("Power need"), FString::FromInt(PowerNeed));
		Row(Utilities, TEXT("City spare"), FString::FromInt(Services.GetAvailablePower()));
	}
	if (Services.Water.EntityDemand.FindRef(Id))
	{
		Row(Utilities, TEXT("Pipe pressure"), FString::Printf(TEXT("%d%%"), Services.Water.EntityPressurePercent.FindRef(Id)));
		switch (Services.Water.EntityCauses.FindRef(Id))
		{
		case ECityWaterFailure::SourceCapacity: Utilities.Detail = FText::FromString(TEXT("Water short: add/upgrade supply or improve source staff/yield.")); break;
		case ECityWaterFailure::LowPressure: Utilities.Detail = FText::FromString(TEXT("Low pressure: build a closer source or shorten the road route.")); break;
		case ECityWaterFailure::SourceOutage: Utilities.Detail = FText::FromString(TEXT("Source offline: inspect its power, staff, damage and freight.")); break;
		case ECityWaterFailure::Disconnected: Utilities.Detail = FText::FromString(TEXT("No pipe route: connect both frontages by roads.")); break;
		default: break;
		}
		const FCityWaterSource* Nearest = nullptr; int32 Distance = MAX_int32;
		TArray<FCityCellCoord> Roads; City.GetAdjacentRoadCells(Entity->Anchor, Entity->FootprintSize, true, Roads, Services.Water.RulesVersion > 0);
		for (const auto& Source : Services.Water.Sources)
		{
			const auto* Provider = City.GetEntities().Find(Source.EntityId); if (!Provider) continue;
			TArray<FCityCellCoord> Starts; City.GetAdjacentRoadCells(Provider->Anchor, Provider->FootprintSize, true, Starts, Services.Water.RulesVersion > 0);
			TMap<FCityCellCoord, int32> Distances; City.GetRoadReach(Starts, MAX_int32, true, Distances);
			for (const auto& Road : Roads)
				if (const auto* H = Distances.Find(Road); H && (*H < Distance || (*H == Distance && (!Nearest || Source.EntityId < Nearest->EntityId))))
				{ Nearest = &Source; Distance = *H; }
		}
		if (Nearest)
		{
			Row(Utilities, TEXT("Water source"), FString::Printf(TEXT("#%llu | %s"), Nearest->EntityId, Nearest->bOperational ? TEXT("on") : TEXT("off")));
			Row(Utilities, TEXT("Road distance"), FString::Printf(TEXT("%d steps"), Distance));
			Row(Utilities, TEXT("Spare output"), FString::FromInt(FMath::Max(0, Nearest->Capacity - Nearest->Used)));
		}
	}
	if (const auto* Source = Services.Water.Sources.FindByPredicate([&](const auto& V) { return V.EntityId == Id; }))
	{
		int64 CityDemand = 0, CityDelivered = 0;
		for (const auto& V : Services.Water.EntityDemand) CityDemand += V.Value;
		for (const auto& V : Services.Water.EntityDelivered) CityDelivered += V.Value;
		if (CityDemand) Gauge(Utilities, TEXT("City flow / need"), CityDelivered, CityDemand);
		Row(Utilities, TEXT("Groundwater"), FString::Printf(TEXT("%d%% yield"), Source->YieldPercent));
		Row(Utilities, TEXT("Source head"), FString::Printf(TEXT("%d%%"), Source->SourcePressurePercent));
		Row(Utilities, TEXT("Spare output"), FString::FromInt(FMath::Max(0, Source->Capacity - Source->Used)));
	}
	if (FCityBuildingDefinition::IsPowerFacility(Entity->DefinitionId) && Services.PowerDemand > 0)
		Gauge(Utilities, TEXT("City power / need"), Services.PowerCapacity, Services.PowerDemand);
	int32 Residents = 0, Employed = 0, TierCount[4] = {}, TierServed[4] = {}, Ready[4] = {};
	int32 Assigned = 0, Active = 0;
	for (const auto& C : People.Citizens)
	{
		const int32 Tier = FMath::Clamp(static_cast<int32>(C.Education), 0, 3);
		if (!C.WorkplaceEntityId && !C.IsDetained() && !C.bOnMedicalLeave) ++Ready[Tier];
		if (C.HomeEntityId == Id)
		{
			++Residents; Employed += C.WorkplaceEntityId != 0; ++TierCount[Tier];
			TierServed[Tier] += Services.EducationCitizenIds.Contains(C.Id) || Services.HigherEducationCitizenIds.Contains(C.Id);
		}
		if (C.WorkplaceEntityId == Id) { ++Assigned; Active += !C.IsDetained() && !C.bOnMedicalLeave; }
	}
	const auto* Household = People.Households.FindByPredicate([&](const auto& V) { return V.HomeEntityId == Id; });
	const auto* Business = People.Businesses.FindByPredicate([&](const auto& V) { return V.EntityId == Id; });
	const auto* Public = People.PublicEmployers.FindByPredicate([&](const auto& V) { return V.EntityId == Id; });
	if (Household)
	{
		Gauge(Work, TEXT("Employed / people"), Employed, Residents, false);
		Row(Work, TEXT("Savings"), Money(Household->Balance));
		int64 Grants=0;for(const auto& C:People.Citizens) if(C.HomeEntityId==Id && C.LastPolicyGrantMonth==Sim.GetEconomySnapshot().Policies.LastSettledMonth) Grants+=C.LastPolicyGrant;
		Row(Work,TEXT("Last policy grants"),Sim.GetEconomySnapshot().Policies.LastSettledMonth>0 ? Money(Grants) : TEXT("Not recorded"));

	}
	if (Business)
	{
		Row(Work, TEXT("Cash"), Money(Business->Balance));
		Row(Work, TEXT("Stock"), FString::FromInt(Business->Inventory));
		if (!Business->bInitialFreightReceived) Row(Work, TEXT("Opening supply"),
		    Business->OpeningGraceUntilStep < 0 || Sim.GetEconomySnapshot().FixedStepId < Business->OpeningGraceUntilStep
		        ? TEXT("First delivery pending; startup grace") : TEXT("Opening delivery overdue; check freight route"));
		else if (!Business->bIndustrial && !Business->bHasReceivedGoods && !Business->Inventory) Row(Work, TEXT("Retail supply"), TEXT("Waiting for first goods shipment"));
		if (!Business->bIndustrial) Row(Work, TEXT("Goods offered"), Business->RetailTier == 2 ? TEXT("Essentials + high-end comfort") : Business->RetailTier == 1 ? TEXT("Essentials + comfort") : TEXT("Essentials"));
	}
	if (Public)
	{
		Gauge(Work, TEXT("Payroll $"), Public->PayrollPaid, Public->PayrollDue);
		const TCHAR* Education[] = {TEXT("None"), TEXT("Basic"), TEXT("Secondary"), TEXT("Higher")};
		Row(Work, TEXT("Education min"), Education[FMath::Clamp((Services.PublicMinimumEducation.Contains(Id) ? Services.PublicMinimumEducation.FindRef(Id) : 1), 0, 3)]);
	}
	if (Assigned) Row(Work, TEXT("Active staff"), FString::Printf(TEXT("%d / %d assigned"), Active, Assigned));
	if (Public)
	{
		int32 Eligible = 0; const int32 Min = Public ? (Services.PublicMinimumEducation.Contains(Id) ? Services.PublicMinimumEducation.FindRef(Id) : 1) : 0;
		for (int32 T = Min; T < 4; ++T) Eligible += Ready[T];
		Row(Work, TEXT("Ready / vacant"), FString::Printf(TEXT("%d / %d"), Eligible, FMath::Max(0, (Public ? Public->JobCapacity : Business->JobCapacity) - Assigned)));
	}
	if (Household || Business)
	{
		int32 VacantEntry = 0, VacantSkilled = 0, VacantHigher = 0;
		for (const auto& B : People.Businesses)
		{
			if (Business && B.EntityId != Id) continue;
			int32 Entry = 0, Skilled = 0;
			for (const auto& C : People.Citizens) if (C.WorkplaceEntityId == B.EntityId) (C.bSkilledJob ? Skilled : Entry)++;
			VacantEntry += FMath::Max(0, B.JobCapacity - B.SkilledJobCapacity - Entry);
			if (B.SkilledMinimumEducation == 3) VacantHigher += FMath::Max(0, B.SkilledJobCapacity - Skilled);
            else VacantSkilled += FMath::Max(0, B.SkilledJobCapacity - Skilled);
		}
		Row(Work, TEXT("Entry ready/open"), FString::Printf(TEXT("%d / %d"), Ready[0] + Ready[1] + Ready[2] + Ready[3], VacantEntry));
		Row(Work, TEXT("Secondary+ ready/open"), FString::Printf(TEXT("%d / %d"), Ready[2] + Ready[3], VacantSkilled));
        if (VacantHigher) Row(Work, TEXT("Higher ready/open"), FString::Printf(TEXT("%d / %d"), Ready[3], VacantHigher));
	}
	if (D && (Public || Business)) Row(Work, TEXT("Upkeep / day"), Money(Level && Level->MonthlyUpkeep > 0 ? Level->MonthlyUpkeep : D->MonthlyUpkeep));
	if (Residents)
	{
		TArray<uint64> Schools;
		for (const auto& E : City.GetEntities()) if (FCityBuildingDefinition::IsSchoolFacility(E.Value.DefinitionId)) Schools.Add(E.Key);
		Schools.Sort(); TMap<uint64, FCityServiceReach> Reach;
		for (uint64 School : Schools) FCityServiceEvaluator::BuildProviderReach(City, School, MAX_int32, true, Reach.Add(School));
		Study.Gauges.Reset();
		const TCHAR* Names[] = {TEXT("Basic study"), TEXT("Secondary study"), TEXT("Higher study")};
		const TCHAR* Required[] = {TEXT("Primary/School"), TEXT("Secondary/School"), TEXT("College")};
		for (int32 Tier = 0; Tier < 3; ++Tier)
		{
			if (!TierCount[Tier]) continue;
			Gauge(Study, Names[Tier], TierServed[Tier], TierCount[Tier]);
			if (TierServed[Tier] >= TierCount[Tier]) continue;
			uint64 Best = 0; int32 Hops = MAX_int32, Rank = MAX_int32;
			for (uint64 School : Schools)
			{
				if (!FCityBuildingDefinition::CanTeachEducation(City.GetEntities().FindChecked(School).DefinitionId, Tier)) continue;
				const int32 Distance = Route(City, Reach.FindChecked(School), *Entity);
				const int32 R = Distance <= Sim.GetEconomyTuning().SchoolRoadRangeHops ? (Services.SchoolCapacities.FindRef(School) > 0 ? 0 : 1) : 2;
				if (!Best || R < Rank || (R == Rank && Distance < Hops)) { Best = School; Hops = Distance; Rank = R; }
			}
			Row(Study, Names[Tier], Best ? FString::Printf(TEXT("School #%llu"), Best) : Required[Tier]);
			if (Best)
			{
				Row(Study, TEXT("Route / limit"), Hops == MAX_int32 ? TEXT("Disconnected") : FString::Printf(TEXT("%d / %d steps"), Hops, Sim.GetEconomyTuning().SchoolRoadRangeHops));
				Row(Study, TEXT("Used / places"), FString::Printf(TEXT("%d / %d"), Services.SchoolUsedCapacity.FindRef(Best), Services.SchoolCapacities.FindRef(Best)));
				Row(Study, TEXT("Staff"), FString::Printf(TEXT("%d%%"), Services.PublicStaffPercent.FindRef(Best)));
			}
			if (Study.Detail.IsEmpty()) Study.Detail = FText::FromString(!Best ? FString(TEXT("Build ")) + Required[Tier] + TEXT(" for this tier.")
			    : Rank == 2 ? TEXT("School out of reach: connect roads or build closer.")
			    : Rank == 1 ? TEXT("School offline: restore staff, utilities and paid bills.")
			    : TEXT("Study places short: add a matching school or restore home services."));
		}
		Row(Study, TEXT("Learning time"), FString::Printf(TEXT("%d served days"), Sim.GetEconomyTuning().SchoolMonthsPerLevel));
		const auto Mood = FCityHappinessEvaluator::Summarize(People, Id);
		Wellbeing.Gauges.Reset(); Gauge(Wellbeing, TEXT("Residents / beds"), Residents, Level ? Level->ResidentsProvided : Residents, false);
		Gauge(Wellbeing, TEXT("Happiness now"), Mood.Average, 100, false); Gauge(Wellbeing, TEXT("Outlook target"), Mood.Target, 100, false);
		Row(Wellbeing, TEXT("Discontented"), FString::Printf(TEXT("%d / %d"), Mood.Discontented, Residents));
		if (!Mood.Concerns) Row(Wellbeing, TEXT("Concerns"), TEXT("None recorded"));
		const TCHAR* Concerns[] = {TEXT("Housing"), TEXT("Utilities"), TEXT("Shopping"), TEXT("Savings"), TEXT("Pollution"), TEXT("Safety"), TEXT("Health"), TEXT("Waste"), TEXT("Taxes")};
		for (int32 I = 0; I < 9; ++I) if (I != 2 && I != 6 && (Mood.Concerns & (1 << I))) Row(Wellbeing, Concerns[I], TEXT("Concern"));
		int32 Ill = 0, Severe = 0, Care = 0, Waiting = 0;
		for (const auto& P : Sim.GetHealthSnapshot().Patients)
			if (People.Citizens.ContainsByPredicate([&](const auto& C) { return C.Id == P.CitizenId && C.HomeEntityId == Id; }))
			{ ++Ill; Severe += P.Illness == ECityIllness::Severe; Care += P.ReceivingCare(); Waiting += P.AwaitingCare(); }
		Health.Gauges.Reset(); Health.Rows.Reset();
		Health.Detail = Health.Help = FText::GetEmpty();
		Gauge(Health, TEXT("Ill residents"), Ill, Residents, false);
		Gauge(Health, TEXT("Severe"), Severe, Residents, false);
		Gauge(Health, TEXT("Receiving care"), Care, Residents, false);
		Gauge(Health, TEXT("Awaiting care"), Waiting, Residents, false);
	}
	else
	{
		Wellbeing.Gauges.Reset(); Wellbeing.Rows.Reset();
		if (Entity->DefinitionId == TEXT("Home")) Gauge(Wellbeing, TEXT("Residents / beds"), 0, Level ? Level->ResidentsProvided : 0, false);
		if (!FCityBuildingDefinition::IsSchoolFacility(Entity->DefinitionId)) Study.Gauges.Reset();
	}
	if (Household)
	{
		// Reuse the authored six-accordion layout; isolate supplies from work/finance prose.
		Wellbeing.Title = FText::FromString(TEXT("Work & wellbeing"));
		for (const auto& G : Work.Gauges)
			if (G.Label.ToString() != TEXT("Buys / need") && G.Label.ToString() != TEXT("Yesterday / need")) Wellbeing.Gauges.Add(G);
		Wellbeing.Rows.Append(Work.Rows);
		Work.Title = FText::FromString(TEXT("Shopping & supplies"));
		Work.Gauges.Reset(); Work.Rows.Reset(); Work.Help = FText::GetEmpty();
		Work.Detail = FText::FromString(FCityConditionIndicators::ShoppingBlocker(Sim, Id));
		const int32 Need = FMath::Max(1, Household->ConsumptionOrders);
		const int32 Weekly = Need * 7;
		int32 HeldGoods = 0;
		for (const auto& O : People.PurchaseReservations) if (O.HouseholdId == Household->Id && !O.ComfortTier) HeldGoods += O.Quantity;
		Gauge(Work, TEXT("Supplies"), Household->bUsesStoredSupplies ? Household->SupplyUnits : 0, Weekly, false);
		Work.Gauges.Last().Value = FText::FromString(FString::Printf(TEXT("%.1f days"), Household->bUsesStoredSupplies ? double(Household->SupplyUnits) / Need : 0.));
		Work.Gauges.Last().Color = Household->SupplyUnits <= 0 ? FLinearColor(1.f, .55f, .25f) : FLinearColor(.3f, .76f, .65f);
		Gauge(Work, TEXT("On order"), HeldGoods, Weekly, false);
		Gauge(Work, TEXT("Bought today"), Household->PurchasesThisMonth, Weekly, false);
		Gauge(Work, TEXT("Yesterday"), Household->CompletedPurchases, Weekly, false);
		// Illness and supplies belong in their accordions, not the always-open summary.
		Out.InspectionSummary = FText::GetEmpty();
		auto Conditions = FCityConditionIndicators::Build(City, Sim.GetFireSnapshot(), Sim.GetCrimeSnapshot(),
		    Sim.GetEconomyTuning().FireRepairSteps, nullptr, &People, 0, &Sim.GetWasteSnapshot(), &Services, MAX_int32, 0);
		if (auto* Cause = Conditions.FindByPredicate([&](const auto& V) { return V.EntityId == Id; }))
		{
			Cause->bSupplyEmpty = false;
			Cause->IllResidents = Cause->PatientsInCare = 0;
			Cause->SettledConcerns &= ~(static_cast<int32>(ECityHappinessConcern::Purchases) | static_cast<int32>(ECityHappinessConcern::Health));
			if (Cause->CauseCount()) Out.InspectionSummary = FText::FromString(Cause->Title(Sim.GetRepairCost(Id)) + TEXT(" | ") + Cause->Detail(Sim.GetEconomyTuning().FixedStepsPerMonth));
		}
	}
	if (FCityBuildingDefinition::IsSchoolFacility(Entity->DefinitionId))
	{
		Row(Study, TEXT("Road reach"), FString::Printf(TEXT("%d steps"), Sim.GetEconomyTuning().SchoolRoadRangeHops));
		Row(Study, TEXT("Teaching"), Entity->DefinitionId == TEXT("College") ? TEXT("Secondary -> Higher") : Entity->DefinitionId == TEXT("PrimarySchool") ? TEXT("None -> Basic") : Entity->DefinitionId == TEXT("SecondarySchool") ? TEXT("Basic -> Secondary") : TEXT("None -> Secondary"));
		Row(Study, TEXT("Learning time"), FString::Printf(TEXT("%d served days"), Sim.GetEconomyTuning().SchoolMonthsPerLevel));
	}
	if (Entity->DefinitionId == TEXT("Hospital"))
	{
		Row(Health, TEXT("Operating beds"), FString::FromInt(Services.HospitalCapacities.FindRef(Id)));
		Row(Health, TEXT("Ambulances"), FString::FromInt(Services.HospitalFleetCapacities.FindRef(Id)));
	}
    if (Business && !Business->bIndustrial)
    {
        Row(Work, TEXT("Commercial AI"), FCityCommercialAi::Status(People, Services, Id));
        if (Services.CommercialAiProviders.Contains(Id))
            Row(Work, TEXT("Access center"), FString::Printf(TEXT("#%llu"), Services.CommercialAiProviders.FindRef(Id)));
        Row(Work, TEXT("Specialist requirement"), Business->SkilledMinimumEducation == 3 ? TEXT("Higher") : TEXT("Secondary+"));
        if (Business->AiFeePaid > 0)
        {
            Row(Work, TEXT("Last software receipt"), FString::Printf(TEXT("$%lld | calendar month %lld | center #%llu"),
                Business->AiFeePaid, Business->AiPaidMonth, Business->AiCenterAtPayment));
            Row(Work, TEXT("Contract roles"), FString::Printf(TEXT("%d -> %d vacancies; %d Higher specialist roles"),
                Business->AiJobsAtPayment, Business->AiJobsAtPayment - 1, FMath::Max(1, Business->AiSpecialistsAtPayment)));
            Work.Detail = FText::FromString(TEXT("Payment goes to the external software provider. Roles reconcile on simulation updates; no layoff is inferred."));
        }
    }
	if (D && FCityTechnology::IsFacility(Entity->DefinitionId))
	{
		Row(Work, TEXT("Capability"),
		    Services.GetFailure(Id) == ECityOperationFailure::None &&
		            (!State || State->Stage != ECityDevelopmentStage::Construction)
		        ? TEXT("Operating")
		        : TEXT("Offline"));
		Row(Work, TEXT("Qualified paid staff"),
		    FString::Printf(TEXT("%d%% / 50%%"), Services.PublicStaffPercent.FindRef(Id)));
		Row(Work, TEXT("Daily upkeep"), Money(D->MonthlyUpkeep) + TEXT(" + payroll"));
		Work.Detail = FText::FromString(D->DescribePrerequisites(Services, Sim.GetBuildingCatalog()));
	}
	if (const auto* Damage = Sim.GetFireSnapshot().DamagedBuildings.Find(Id))
	{
		Row(Safety, TEXT("Repair cost"), Money(Sim.GetRepairCost(Id)));
		if (Damage->RepairCompleteStep < 0) Safety.Detail = FText::FromString(TEXT("Select Repair beside Inspect to restore this building."));
	}
	if (Sim.GetFireSnapshot().EntityRisk.Contains(Id))
	{
		Row(Safety, TEXT("Fire risk"), FString::Printf(TEXT("%d%%"), Sim.GetFireSnapshot().EntityRisk.FindRef(Id)));
		Row(Safety, TEXT("Protection"), FString::Printf(TEXT("%d%%"), Sim.GetFireSnapshot().EntityProtection.FindRef(Id)));
	}
}
