#include "Simulation/CitySimulationSubsystem.h"
#include "Commands/CityPlacementModel.h"

FString UCitySimulationSubsystem::DescribeStationUpgrade(const uint64 EntityId) const
{
	const auto* E = PlacementModel ? PlacementModel->GetEntities().Find(EntityId) : nullptr;
	const auto* D = E && BuildingCatalog ? BuildingCatalog->FindDefinition(E->DefinitionId) : nullptr;
	const auto* S = Development.GetSnapshot().Find(EntityId);
	if (!D || !D->bPlayerPurchasedLevels || !S)
		return TEXT("Select an upgradeable civic facility");
	if (S->HasCivicUpgrade())
	{
		const double Remaining =
		    S->StationCompleteHalfDay >= 0
		        ? FMath::Max(0.0, S->StationCompleteHalfDay * 0.5 -
		                              (Economy.GetSnapshot().FixedStepId + GetSimulationStepProgress()) / FMath::Max(1, EconomyTuning.FixedStepsPerMonth))
		        : FMath::Max(0.0, static_cast<double>(S->CivicUpgradeCompleteStep - Economy.GetSnapshot().FixedStepId) / FMath::Max(1, EconomyTuning.FixedStepsPerMonth));
		const auto Failure = ServiceSnapshot.GetFailure(EntityId);
		if (Remaining == 0 &&
		    (Failure == ECityOperationFailure::OnFire || Failure == ECityOperationFailure::FireDamage ||
		     Failure == ECityOperationFailure::Repairing))
			return TEXT("Expansion ready; waiting for fire recovery/repair");
		return FString::Printf(
		    TEXT("Expansion underway: %.2f game days left\nCurrent stage continues under its normal operating rules"),
		    Remaining);
	}
	if (S->DensityLevel == ECityDensityLevel::High)
		return TEXT("Facility fully expanded");
	const auto* L = D->FindDensityLevel(static_cast<ECityDensityLevel>(static_cast<int32>(S->DensityLevel) + 1));
	if (!L)
		return TEXT("Facility fully expanded");
	FString Readiness;
	if (Economy.GetSnapshot().HasOperatingUpkeepShortfall())
		Readiness = TEXT("Settle overdue city bills first");
	else if (S->Stage != ECityDevelopmentStage::Operational ||
	         ServiceSnapshot.GetFailure(EntityId) != ECityOperationFailure::None ||
	         (ServiceSnapshot.bUsesPublicStaffing && ServiceSnapshot.PublicStaffPercent.FindRef(EntityId) <= 0))
		Readiness = TEXT("Requires a built, repaired, supplied and staffed facility");
	else if (!PlacementModel->HasRoadAccess(EntityId))
		Readiness = TEXT("Connect the facility to roads first");
	else if (PlacementModel->GetFunds() < L->UpgradeCost)
		Readiness = TEXT("Treasury cannot afford this upgrade");
	else
		Readiness = TEXT("Treasury and current facility ready");
	if (E->DefinitionId == TEXT("Power") || E->DefinitionId == TEXT("Service"))
	{
		const int32 Nominal = (E->DefinitionId == TEXT("Power") ? EconomyTuning.PowerCapacityPerBuilding
		                                                        : EconomyTuning.WaterCapacityPerTreatment) *
		                      L->UtilitySupplyPercent / 100;
		return FString::Printf(
		    TEXT(
		        "Next: %s | $%lld | %d game minutes\nSupply %d-%d with paid staffing\n%d staff | upkeep $%lld/day + payroll\nBenefits after completion; current output continues\n%s"),
		    L->Level == ECityDensityLevel::Medium ? TEXT("Capacity expansion") : TEXT("Advanced facility"),
		    L->UpgradeCost, L->CivicConstructionSteps * 1440 / FMath::Max(1, EconomyTuning.FixedStepsPerMonth), Nominal * EconomyTuning.GetUtilityOutputPercent(0) / 100,
		    Nominal, L->JobsProvided, L->MonthlyUpkeep, *Readiness);
	}
	if (FCityWasteSimulation::IsFacility(E->DefinitionId))
		return FString::Printf(
		    TEXT(
		        "Waste expansion | $%lld | %d game minutes\n%d %s staff | upkeep $%lld/day + payroll\nStorage4800 | processing160/day | up to %d trucks\nRange %d road cells | ordinary traffic\nBenefits after completion, staffing and supplies\n%s"),
		    L->UpgradeCost, L->CivicConstructionSteps * 1440 / FMath::Max(1, EconomyTuning.FixedStepsPerMonth), L->JobsProvided,
		    E->DefinitionId == TEXT("Recycling") ? TEXT("Secondary") : TEXT("Basic"), L->MonthlyUpkeep,
		    L->CivicResponseVehicles, L->CivicRoadRange, *Readiness);
	if (E->DefinitionId == TEXT("Hospital"))
		return FString::Printf(
		    TEXT(
		        "Ward expansion | $%lld | %d game minutes\n%d Secondary staff | upkeep $%lld/day + payroll\nPower %d | water %d\n%d treatment places | up to %d ambulances\nBenefits after completion, staffing and supplies\n%s"),
		    L->UpgradeCost, L->CivicConstructionSteps * 1440 / FMath::Max(1, EconomyTuning.FixedStepsPerMonth), L->JobsProvided, L->MonthlyUpkeep, L->PowerDemand,
		    L->WaterDemand, L->CivicProtectionCapacity, L->CivicResponseVehicles, *Readiness);
	return FString::Printf(
	    TEXT(
	        "Next: %s | $%lld | at most 0.5 game day\n%d staff | upkeep $%lld/day + payroll\nPower %d | water %d | %d road hops\nProtection %d | up to %d response crews\nBenefits after completion, staffing and supplies\n%s"),
	    L->Level == ECityDensityLevel::Medium ? TEXT("Garage expansion") : TEXT("Department wing"), L->UpgradeCost,
	    L->JobsProvided, L->MonthlyUpkeep, L->PowerDemand, L->WaterDemand, L->CivicRoadRange,
	    L->CivicProtectionCapacity, L->CivicResponseVehicles, *Readiness);
}

bool UCitySimulationSubsystem::PurchaseStationUpgrade(const uint64 EntityId, const ECityDensityLevel ExpectedLevel)
{
	if (!PlacementModel || Economy.GetSnapshot().HasOperatingUpkeepShortfall())
	{
		ControlStatus = TEXT("Settle overdue city operating bills before upgrading");
		return false;
	}
	const bool Success = Development.PurchaseCivicUpgrade(EntityId, ExpectedLevel, *PlacementModel, BuildingCatalog,
	                                                      ServiceSnapshot, ControlStatus, GetStationHalfDayClock(), EconomyTuning.FixedStepsPerMonth);
	if (Success)
	{
		++PresentationGeneration;
		RefreshDerivedSnapshots();
	}
	return Success;
}
