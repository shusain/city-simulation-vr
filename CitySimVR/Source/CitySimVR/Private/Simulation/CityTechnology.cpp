#include "Simulation/CityTechnology.h"
#include "Simulation/CityCommercialAi.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Simulation/CityDevelopmentSimulation.h"
#include "Simulation/CityUtilitySimulation.h"

TArray<FName> FCityTechnology::FacilityIds()
{
	return {TEXT("DigitalExchange"), TEXT("ComputingCenter"), TEXT("ResearchCampus"),
	        TEXT("AIInstitute"), TEXT("EnterpriseAICenter")};
}
bool FCityTechnology::IsFacility(const FName Id)
{
	return Id == TEXT("DigitalExchange") || Id == TEXT("ComputingCenter") || Id == TEXT("ResearchCampus") ||
	       Id == TEXT("AIInstitute") || Id == TEXT("EnterpriseAICenter");
}
FString FCityTechnology::Review(const FName Requested, const FCityServiceSnapshot& S,
                                const UCityBuildingCatalog* Catalog)
{
	const FName Id = IsFacility(Requested) ? Requested : FName(TEXT("DigitalExchange"));
	const auto* D = Catalog ? Catalog->FindDefinition(Id) : nullptr;
	if (!D)
		return TEXT("Technology catalog unavailable");
	const TCHAR* Benefit = Id == TEXT("DigitalExchange")   ? TEXT("Enables connected computing facilities")
	                       : Id == TEXT("ComputingCenter") ? TEXT("Enables a research campus")
	                       : Id == TEXT("ResearchCampus") ? TEXT("Enables an AI institute")
	                       : Id == TEXT("AIInstitute") ? TEXT("Enables enterprise AI and future robotics")
	                       : TEXT("Supports an optional 2-shop commercial pilot");
	return FString::Printf(
	    TEXT("Operating: exchange %d | computing %d | research %d\n%s | %s\n"
	         "Build $%lld | upkeep $%lld/day + public payroll\nPower %d | water %d | staff %d (%s)\n"
	         "Operating requires at least 50%% qualified, paid staff.\n%s\n%s\n"
	         "Outages suspend this capability and dependent facilities."),
	    S.OperatingFacilities.FindRef(TEXT("DigitalExchange")),
	    S.OperatingFacilities.FindRef(TEXT("ComputingCenter")),
	    S.OperatingFacilities.FindRef(TEXT("ResearchCampus")), *D->DisplayName,
	    D->PrerequisitesMet(&S) ? TEXT("BUILD READY") : TEXT("LOCKED"), D->Cost, D->MonthlyUpkeep,
	    D->PowerDemand, D->WaterDemand, D->JobsProvided,
	    D->MinimumStaffEducation == 3 ? TEXT("Higher") : TEXT("Secondary+"),
	    *D->DescribePrerequisites(S, Catalog), Benefit);
}
void FCityTechnology::Evaluate(const FCityPlacementModel& City, FCityServiceSnapshot& S,
                               const UCityBuildingCatalog* Catalog, const FCityPopulationSnapshot* P,
                               const FCityDevelopmentSnapshot* Development,
                               const FCityEnvironmentSnapshot* Weather)
{
	if (!Catalog)
		return;
	TArray<uint64> Ids;
	City.GetEntities().GetKeys(Ids);
	Ids.Sort();
	for (const uint64 Id : Ids)
	{
		const auto& E = City.GetEntities().FindChecked(Id);
		if (!IsFacility(E.DefinitionId) &&
		    !(FCityBuildingDefinition::IsPowerFacility(E.DefinitionId) &&
		      FCityBuildingDefinition::IsAlternativeUtility(E.DefinitionId)) &&
		    S.GetFailure(Id) == ECityOperationFailure::None &&
		    (!Development || Development->IsOperational(Id)) && City.HasRoadAccess(Id) &&
		    (!FCityBuildingDefinition::IsSchoolFacility(E.DefinitionId) ||
		     S.SchoolCapacities.FindRef(Id) > 0) &&
		    (!FCityBuildingDefinition::IsWaterFacility(E.DefinitionId) ||
		     S.Water.Sources.ContainsByPredicate(
		         [Id](const auto& V) { return V.EntityId == Id && V.bOperational; })))
			++S.OperatingFacilities.FindOrAdd(E.DefinitionId);
		const auto* D = Catalog->FindDefinition(E.DefinitionId);
		if (D && FCityBuildingDefinition::IsAlternativeUtility(E.DefinitionId) &&
		    FCityBuildingDefinition::IsPowerFacility(E.DefinitionId) &&
		    S.GetFailure(Id) == ECityOperationFailure::None &&
		    (!Development || Development->IsOperational(Id)) && City.HasRoadAccess(Id) &&
		    int64(D->UtilityCapacity) *
		            CityUtilityOutputPercent(
		                *D, CityUtilityStaffPercent(P, Id, D->JobsProvided, D->MinimumStaffEducation)) *
		            CityUtilityWeatherPercent(E.DefinitionId, Weather) / 10000 >
		        0)
		{
			++S.OperatingAlternativePower;
			++S.OperatingFacilities.FindOrAdd(E.DefinitionId);
		}
	}
	// Topological facility order, then stable entity ID: demolition/rebuild order cannot change
	// prerequisites.
	for (const FName Kind : FacilityIds())
		for (const uint64 Id : Ids)
		{
			const auto& E = City.GetEntities().FindChecked(Id);
			if (E.DefinitionId != Kind)
				continue;
			const auto* D = Catalog->FindDefinition(Kind);
			if (!D)
				continue;
			++S.BuiltTechnology.FindOrAdd(Kind);
			S.CivicUpkeep += D->MonthlyUpkeep;
			S.PublicMinimumEducation.Add(Id, D->MinimumStaffEducation);
			S.PublicJobs.Add(Id, 0);
			S.PublicStaffPercent.Add(Id, 0);
			S.EntityJobs.Add(Id, 0);
			S.EntityOutput.Add(Id, 0);
			S.BuiltJobs += D->JobsProvided;
			if ((Development && !Development->IsOperational(Id)) ||
			    S.GetFailure(Id) == ECityOperationFailure::OnFire ||
			    S.GetFailure(Id) == ECityOperationFailure::FireDamage ||
			    S.GetFailure(Id) == ECityOperationFailure::Repairing)
				continue;
			ECityOperationFailure Failure = !D->PrerequisitesMet(&S)
			                                    ? ECityOperationFailure::TechnologyPrerequisite
			                                : !City.HasRoadAccess(Id) ? ECityOperationFailure::NoRoad
			                                                          : ECityOperationFailure::None;
			if (Failure == ECityOperationFailure::None)
			{
				S.PowerDemand += D->PowerDemand;
				const int32 Water =
				    (D->WaterDemand * (Weather ? Weather->GetWaterDemandModifierPercent() : 100) + 99) / 100;
				S.ServiceDemand += Water;
				int32 Delivered = 0;
				if (S.Water.RulesVersion > 0)
					Delivered =
					    FCityWaterEvaluator::Allocate(City, S.Water, Id, E.Anchor, E.FootprintSize, Water);
				else if (FCityServiceEvaluator::IsFootprintReached(City, S.ServiceReach, E.Anchor,
				                                                   E.FootprintSize))
					Delivered = FMath::Min(Water, S.GetAvailableService());
				S.ServiceUsed += Delivered;
				if (FCityServiceEvaluator::IsFootprintReached(City, S.ServiceReach, E.Anchor,
				                                              E.FootprintSize))
					S.ServiceCoveredDemand += Water;
				if (S.Water.RulesVersion == 0)
				{
					S.Water.EntityDemand.Add(Id, Water);
					S.Water.EntityDelivered.Add(Id, Delivered);
					S.Water.EntityPressurePercent.Add(Id, Delivered >= Water ? 100 : 0);
					S.Water.EntityCauses.Add(Id, Delivered >= Water ? ECityWaterFailure::None
					                             : Delivered > 0    ? ECityWaterFailure::SourceCapacity
					                                                : ECityWaterFailure::Disconnected);
				}
				if (Delivered < Water)
					Failure = S.Water.EntityCauses.FindRef(Id) == ECityWaterFailure::LowPressure
					              ? ECityOperationFailure::LowWaterPressure
					          : S.Water.EntityCauses.FindRef(Id) == ECityWaterFailure::SourceCapacity
					              ? ECityOperationFailure::ServiceCapacity
					              : ECityOperationFailure::NoService;
				else if (S.GetAvailablePower() < D->PowerDemand)
					Failure = S.PowerCapacity <= 0 ? ECityOperationFailure::NoPower
					                               : ECityOperationFailure::PowerCapacity;
				else
				{
					S.PowerUsed += D->PowerDemand;
					// Offer vacancies before staffing exists, so these facilities can bootstrap hiring.
					S.PublicJobs.Add(Id, D->JobsProvided);
					S.EntityJobs.Add(Id, D->JobsProvided);
					S.Jobs += D->JobsProvided;
					int32 Qualified = 0;
					if (P)
						for (const auto& C : P->Citizens)
							Qualified += C.WorkplaceEntityId == Id && !C.IsDetained() && !C.bOnMedicalLeave &&
							                     static_cast<int32>(C.Education) >= D->MinimumStaffEducation
							                 ? 1
							                 : 0;
					int32 Staff = P ? FMath::Min(100, Qualified * 100 / FMath::Max(1, D->JobsProvided)) : 100;
					if (P)
					{
						if (const auto* Employer = P->PublicEmployers.FindByPredicate(
						        [Id](const auto& V) { return V.EntityId == Id; }))
						{
							if (Employer->PayrollDue > 0)
								Staff =
								    static_cast<int32>(Staff * Employer->PayrollPaid / Employer->PayrollDue);
						}
						else
							Staff = 0;
					}
					S.PublicStaffPercent.Add(Id, Staff);
					if (Staff < 50)
						Failure = ECityOperationFailure::NoStaff;
					else
					{
						++S.OperatingFacilities.FindOrAdd(Kind);
						if (Kind == TEXT("EnterpriseAICenter")) S.OperatingAiCenters.Add(Id);
					}
				}
			}
			S.EntityFailures.Add(Id, Failure);
			S.bHasDisabledEntities |= Failure != ECityOperationFailure::None;
		}
	FCityCommercialAi::Apply(P, S);
	if (S.Water.RulesVersion == 0)
	{
		int32 Remaining = S.ServiceUsed;
		for (auto& Source : S.Water.Sources)
		{
			Source.Used = FMath::Min(Remaining, Source.Capacity);
			Remaining -= Source.Used;
		}
	}
}
