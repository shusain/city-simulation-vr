#include "Presentation/CityConditionIndicators.h"
#include "Commands/CityPlacementModel.h"
#include "Simulation/CityFireSimulation.h"
#include "Simulation/CityCrimeSimulation.h"
#include "Simulation/CityHealthSimulation.h"
#include "Simulation/CityWasteSimulation.h"
#include "Simulation/CityPopulationSimulation.h"
#include "Simulation/CityHousingHistory.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Simulation/CityHappinessSimulation.h"

FName FCityConditionIndicator::Symbol() const
{
	if (bFire)
		return TEXT("Fire");
	if (bCrime)
		return TEXT("Police");
	if (bNeedsRepair)
		return TEXT("Repair");
	const auto Failure = static_cast<ECityOperationFailure>(OperationFailure);
	if (Failure == ECityOperationFailure::NoPower || Failure == ECityOperationFailure::PowerCapacity)
		return TEXT("Power");
	if (Failure == ECityOperationFailure::NoService || Failure == ECityOperationFailure::ServiceCapacity ||
	    Failure == ECityOperationFailure::LowWaterPressure)
		return TEXT("Water");
	if (Failure == ECityOperationFailure::NoStaff)
		return TEXT("People");
	if (OperationFailure)
		return TEXT("Inspect");
	if (IllResidents)
		return TEXT("Health");
	if (WasteUnits)
		return TEXT("Waste");
	if (bRetailStockout || bSupplyEmpty) return TEXT("Shop");
	if (SettledConcerns & static_cast<int32>(ECityHappinessConcern::Purchases))
		return TEXT("Shop");
	if (SearchingResidents)
		return TEXT("Leaving");
	if (bSustainedDiscontent)
		return TEXT("Happiness");
	if (bRepairing)
		return TEXT("Repairing");
	return RecentHousingKind ? FName(TEXT("Leaving")) : FName(TEXT("Health"));
}
int32 FCityConditionIndicator::Priority() const
{
	return bFire                                        ? 0
	       : bCrime                                     ? 1
	       : bNeedsRepair                               ? 2
	       : OperationFailure                           ? 3
	       : IllResidents                               ? 4
	       : WasteUnits                                 ? 5
	       : SettledConcerns || bRetailStockout || bSupplyEmpty ? 6
	       : SearchingResidents || bSustainedDiscontent ? 7
	       : bRepairing                                 ? 8
	                                                    : 9;
}
int32 FCityConditionIndicator::CauseCount() const
{
	uint32 Mask = CauseMask();
	int32 Count = 0;
	while (Mask)
	{
		Count += Mask & 1;
		Mask >>= 1;
	}
	return Count;
}
uint32 FCityConditionIndicator::CauseMask() const
{
	return uint32(bFire) | (uint32(bCrime) << 1) | (uint32(bNeedsRepair) << 2) | (uint32(bRepairing) << 3) |
	       (uint32(OperationFailure != 0) << 4) | (uint32(IllResidents > 0) << 5) |
	       (uint32(PatientsInCare > 0) << 6) | (uint32(WasteUnits > 0) << 7) |
	       (uint32(SearchingResidents > 0) << 8) | (uint32(bSustainedDiscontent) << 9) |
	       (uint32(SettledConcerns != 0) << 10) | (uint32(RecentHousingKind != 0) << 11) |
	       (uint32(bSupplyEmpty) << 12) | (uint32(bRetailStockout) << 13);
}

TArray<FName> FCityConditionIndicator::Symbols() const
{
	TArray<FName> Result;
	Result.Add(Symbol());
	if (bFire)
		Result.AddUnique(TEXT("Fire"));
	if (bCrime)
		Result.AddUnique(TEXT("Police"));
	if (bNeedsRepair)
		Result.AddUnique(TEXT("Repair"));
	if (bRepairing)
		Result.AddUnique(TEXT("Repairing"));
	if (OperationFailure)
	{
		FCityConditionIndicator Failure;
		Failure.OperationFailure = OperationFailure;
		Result.AddUnique(Failure.Symbol());
	}
	if (IllResidents || PatientsInCare)
		Result.AddUnique(TEXT("Health"));
	if (WasteUnits)
		Result.AddUnique(TEXT("Waste"));
	if (SettledConcerns || bSupplyEmpty || bRetailStockout)
		Result.AddUnique(TEXT("Shop"));
	if (bSustainedDiscontent)
		Result.AddUnique(TEXT("Happiness"));
	if (SearchingResidents || RecentHousingKind)
		Result.AddUnique(TEXT("Leaving"));
	return Result;
}

FString FCityConditionIndicator::Title(const int64 RepairCost) const
{
	const int32 Count = EntityIds.Num();
	if (bFire)
		return TEXT("FIRE");
	if (bCrime)
		return Count > 1 ? FString::Printf(TEXT("CRIME %d LOTS"), Count) : TEXT("CRIME");
	if (OperationFailure && !bNeedsRepair)
	{
		switch (static_cast<ECityOperationFailure>(OperationFailure))
		{
		case ECityOperationFailure::NoPower:
		case ECityOperationFailure::PowerCapacity:
			return TEXT("POWER INTERRUPTED");
		case ECityOperationFailure::NoService:
		case ECityOperationFailure::ServiceCapacity:
		case ECityOperationFailure::LowWaterPressure:
			return TEXT("WATER INTERRUPTED");
		case ECityOperationFailure::NoStaff:
			return TEXT("STAFF NEEDED");
		case ECityOperationFailure::TechnologyPrerequisite:
			return TEXT("DEPENDENCY OFFLINE");
		case ECityOperationFailure::NoRoad:
			return TEXT("ROAD ACCESS");
		case ECityOperationFailure::AwaitingFreight:
			return TEXT("OPENING DELIVERY OVERDUE");
		default:
			return TEXT("OPERATION STOPPED");
		}
	}

	if (bNeedsRepair)
		return Count > 1 ? FString::Printf(TEXT("REPAIR %d | $%lld"), Count, RepairCost)
		                 : FString::Printf(TEXT("REPAIR $%lld"), RepairCost);
	if (bCrime)
		return Count > 1 ? FString::Printf(TEXT("CRIME %d LOTS"), Count) : TEXT("CRIME");
	if (IllResidents)
		return FString::Printf(TEXT("ILL %d"), IllResidents);
	if (WasteUnits > 0)
		return FString::Printf(TEXT("WASTE %d"), WasteUnits);
	if (bSupplyEmpty) return TEXT("SUPPLIES EXHAUSTED");
	if (bRetailStockout) return TEXT("OUT OF STOCK");
	if (SettledConcerns)
		return TEXT("UNMET PURCHASES");
	if (bSustainedDiscontent && !SearchingResidents)
		return TEXT("SUSTAINED DISCONTENT");
	if (SearchingResidents > 0)
		return FString::Printf(TEXT("HOME SEARCH %d"), SearchingResidents);
	if (bRepairing)
		return Count > 1 ? FString::Printf(TEXT("REPAIRING %d | %d%%"), Count, RepairPercent)
		                 : FString::Printf(TEXT("REPAIRING %d%%"), RepairPercent);
	if (RecentHousingKind && !IllResidents && !PatientsInCare)
		return RecentHousingKind == static_cast<uint8>(ECityHousingEventKind::Recovered) + 1
		           ? TEXT("SEARCH RECOVERED")
		       : RecentHousingKind == static_cast<uint8>(ECityHousingEventKind::Departed) + 1
		           ? TEXT("RESIDENTS LEFT")
		           : TEXT("RESIDENTS MOVED");
	return PatientsInCare ? FString::Printf(TEXT("CARE %d"), PatientsInCare)
	                      : FString::Printf(TEXT("ILL %d"), IllResidents);
}

FString FCityConditionIndicator::Detail(int32 StepsPerDay) const
{
	const int32 Count = EntityIds.Num();
	if (bFire)
		return TEXT("Active fire | Fire protection");
	if (OperationFailure && !bNeedsRepair && !bCrime)
	{
		switch (static_cast<ECityOperationFailure>(OperationFailure))
		{
		case ECityOperationFailure::NoPower:
		case ECityOperationFailure::PowerCapacity:
			return TEXT("City grid supply | Power / staffing");
		case ECityOperationFailure::NoStaff:
			return TEXT("Operating staff missing | Workforce / education");
		case ECityOperationFailure::NoRoad:
			return TEXT("No entrance road connection | Roads");
		case ECityOperationFailure::AwaitingFreight:
			return TEXT("Opening freight not received | Delivery access");
		case ECityOperationFailure::TechnologyPrerequisite:
			return TEXT("Operating prerequisite missing | Technology");
		default:
			return TEXT("Water allocation short | Inspect water source / route");
		}
	}
	if (bSupplyEmpty || bRetailStockout)
		return bSupplyEmpty ? TEXT("Stored essentials used up | Shopping / affordability / routes")
		                    : TEXT("Retail stock used up | Factory deliveries / routes");
	if (SettledConcerns && !bCrime && !bNeedsRepair && !IllResidents && !WasteUnits)
		return TEXT("Yesterday's purchases | Inspect today's orders / progress");
	if (bSustainedDiscontent && !SearchingResidents && !bCrime && !bNeedsRepair && !IllResidents &&
	    !WasteUnits)
		return TEXT("Low happiness for 3+ days | Inspect recorded concerns");
	if (bCrime && bRepairing)
		return FString::Printf(TEXT("CRIME %lldmin | FIX %lldmin"), CrimeDays * 1440 / FMath::Max(1, StepsPerDay), RepairDays * 1440 / FMath::Max(1, StepsPerDay));
	if (bCrime)
		return Count > 1 ? FString::Printf(TEXT("Up to %lldmin | %d lots"), CrimeDays * 1440 / FMath::Max(1, StepsPerDay), Count)
		                 : FString::Printf(TEXT("CRIME %lldmin | Inspect"), CrimeDays * 1440 / FMath::Max(1, StepsPerDay));
	if (bRepairing && Priority() == 8)
		return Count > 1 ? FString::Printf(TEXT("Up to %lldmin | %d lots"), RepairDays * 1440 / FMath::Max(1, StepsPerDay), Count)
		                 : FString::Printf(TEXT("%lld game minutes left"), RepairDays * 1440 / FMath::Max(1, StepsPerDay));
	if (bNeedsRepair)
		return TEXT("Structural damage | City > Repair");
	if (IllResidents)
		return TEXT("Ill residents | Hospital access / care");
	if (WasteUnits > 0)
		return TEXT("Collection needed | Inspect");
	if (SearchingResidents > 0)
		return bHousingCareHold         ? TEXT("Care hold | Inspect needs")
		       : bMovingFeeUnaffordable ? TEXT("Moving fee needed | Inspect")
		                                : FString::Printf(TEXT("Low %dmo | Inspect needs"), LongestDiscontent);
	if (IllResidents || PatientsInCare)
		return Count > 1 ? FString::Printf(TEXT("%d lots | Inspect access"), Count) : TEXT("Inspect hospital access");
	if (RecentHousingKind && !bNeedsRepair)
		return FString::Printf(TEXT("Recorded month %lld | %d residents | Housing history"),
		                       RecentHousingMonth, RecentHousingResidents);
	return Count > 1 ? FString::Printf(TEXT("%d lots | City > Repair"), Count) : TEXT("City > Repair");
}

TArray<FCityConditionIndicator> FCityConditionIndicators::Build(
    const FCityPlacementModel& City, const FCityFireSnapshot& Fire, const FCityCrimeSnapshot& Crime,
    const int32 RepairDuration, const FCityHealthSnapshot* Health, const FCityPopulationSnapshot* People,
    const uint64 ExcludedEntityId, const FCityWasteSnapshot* Waste, const FCityServiceSnapshot* Services,
    const int32 Limit, const double ClusterDiameter, int32* OutAffectedLots)
{
	TMap<uint64, FCityConditionIndicator> Lots;
	if (People && People->LastSettledMonthId > 0)
		for (const auto& Event : People->HousingEvents)
			if (Event.Month == People->LastSettledMonthId && City.GetEntities().Contains(Event.SourceHome) &&
			    (Event.Kind == ECityHousingEventKind::Recovered ||
			     Event.Kind == ECityHousingEventKind::Departed ||
			     Event.Kind == ECityHousingEventKind::Relocated))
			{
				auto& Row = Lots.FindOrAdd(Event.SourceHome);
				Row.EntityId = Event.SourceHome;
				Row.RecentHousingKind = static_cast<uint8>(Event.Kind) + 1;
				Row.RecentHousingResidents = Event.Residents;
				Row.RecentHousingMonth = Event.Month;
			}
	for (const auto& I : Fire.RecentIncidents)
		if (City.GetEntities().Contains(I.EntityId) && I.StartStep <= Fire.LastFixedStepId &&
		    I.ResolvedStep < 0)
		{
			auto& Row = Lots.FindOrAdd(I.EntityId);
			Row.EntityId = I.EntityId;
			Row.bFire = true;
		}
	if (Services)
		for (const auto& Pair : Services->EntityFailures)
			if (City.GetEntities().Contains(Pair.Key) && Pair.Value != ECityOperationFailure::None &&
			    Pair.Value != ECityOperationFailure::OnFire &&
			    Pair.Value != ECityOperationFailure::FireDamage &&
			    Pair.Value != ECityOperationFailure::Repairing)
			{
				if (Pair.Value == ECityOperationFailure::AwaitingFreight && People &&
				    (City.GetEntities().FindChecked(Pair.Key).DefinitionId == TEXT("Shop") ||
				     City.GetEntities().FindChecked(Pair.Key).DefinitionId == TEXT("Factory")))
				{
					const auto* Business = People->Businesses.FindByPredicate([&](const auto& B) { return B.EntityId == Pair.Key; });
					if (!Business || Business->OpeningGraceUntilStep < 0 || Fire.LastFixedStepId < Business->OpeningGraceUntilStep)
						continue; // Normal opening freight is informational during startup grace.
				}
				auto& Row = Lots.FindOrAdd(Pair.Key);
				Row.EntityId = Pair.Key;
				Row.OperationFailure = static_cast<uint8>(Pair.Value);
			}
	TMap<uint64, const FCityHouseholdRecord*> HouseholdByHome;
	if (People)
	{
		for (const auto& H : People->Households)
		{
			HouseholdByHome.Add(H.HomeEntityId, &H);
			if (H.bUsesStoredSupplies && H.ConsumptionOrders > 0 && H.SupplyUnits == 0 && City.GetEntities().Contains(H.HomeEntityId))
			{
				auto& Row = Lots.FindOrAdd(H.HomeEntityId); Row.EntityId = H.HomeEntityId;
				Row.bSupplyEmpty = true;
			}
		}
		for (const auto& B : People->Businesses)
			if (!B.bIndustrial && B.bHasReceivedGoods && B.Inventory == 0 && City.GetEntities().Contains(B.EntityId))
			{
				auto& Row = Lots.FindOrAdd(B.EntityId); Row.EntityId = B.EntityId; Row.bRetailStockout = true;
			}
	}
	if (People)
		for (const auto& C : People->Citizens)
			if (City.GetEntities().Contains(C.HomeEntityId) && C.LastHappinessMonth > 0 &&
			    ((C.HappinessConcerns & static_cast<int32>(ECityHappinessConcern::Purchases)) ||
			     C.DiscontentMonths >= FCityHappinessEvaluator::SustainedMonths))
			{
				const auto* H = HouseholdByHome.FindRef(C.HomeEntityId);
				const bool Stored = H && H->bUsesStoredSupplies;
				if (Stored && C.DiscontentMonths < FCityHappinessEvaluator::SustainedMonths) continue;
				auto& Row = Lots.FindOrAdd(C.HomeEntityId);
				Row.EntityId = C.HomeEntityId;
				Row.SettledConcerns |=
				    Stored ? 0 : C.HappinessConcerns & static_cast<int32>(ECityHappinessConcern::Purchases);
				Row.bSustainedDiscontent |= C.DiscontentMonths >= FCityHappinessEvaluator::SustainedMonths;
			}

	for (const auto& Pair : Fire.DamagedBuildings)
	{
		if (!City.GetEntities().Contains(Pair.Key) || Pair.Value.Damage <= 0 || Fire.IsBurning(Pair.Key))
			continue;
		const auto& Damage = Pair.Value;
		if (Damage.RepairCompleteStep >= 0 && Damage.RepairCompleteStep <= Fire.LastFixedStepId)
			continue;
		auto& Row = Lots.FindOrAdd(Pair.Key);
		Row.EntityId = Pair.Key;
		Row.bRepairing = Damage.RepairCompleteStep >= 0;
		Row.bNeedsRepair = !Row.bRepairing;
		if (Row.bRepairing)
		{
			Row.RepairDays = Damage.RepairCompleteStep - Fire.LastFixedStepId;
			const int64 Duration = FMath::Max(1, RepairDuration);
			Row.RepairPercent = static_cast<int32>(
			    FMath::Clamp<int64>((Duration - FMath::Min(Duration, Row.RepairDays)) * 100 / Duration, 0, 100));
		}
	}
	for (const auto& Incident : Crime.RecentIncidents)
	{
		if (!City.GetEntities().Contains(Incident.EntityId) || Incident.StartStep > Crime.LastFixedStepId ||
		    Incident.EndStep <= Crime.LastFixedStepId)
			continue;
		auto& Row = Lots.FindOrAdd(Incident.EntityId);
		Row.EntityId = Incident.EntityId;
		Row.bCrime = true;
		Row.CrimeDays = FMath::Max(Row.CrimeDays, Incident.EndStep - Crime.LastFixedStepId);
	}
	TMap<uint64, uint64> Homes;
	if (Health && People)
		for (const auto& C : People->Citizens)
			Homes.Add(C.Id, C.HomeEntityId);
	if (Health && People)
		for (const auto& P : Health->Patients)
		{
			const uint64* Home = Homes.Find(P.CitizenId);
			if (Home && City.GetEntities().Contains(*Home))
			{
				auto& Row = Lots.FindOrAdd(*Home);
				Row.EntityId = *Home;
				++Row.IllResidents;
			}
			if (P.HospitalId && (P.UsesBed() || P.UsesOutpatientPlace()) && City.GetEntities().Contains(P.HospitalId))
			{
				auto& Row = Lots.FindOrAdd(P.HospitalId);
				Row.EntityId = P.HospitalId;
				++Row.PatientsInCare;
			}
		}
	if (Waste)
		for (const auto& S : Waste->Sites)
			if (S.Units * 2 >= S.Capacity && City.GetEntities().Contains(S.EntityId))
			{
				auto& Row = Lots.FindOrAdd(S.EntityId);
				Row.EntityId = S.EntityId;
				Row.WasteUnits = S.Units;
			}
	Lots.Remove(ExcludedEntityId);
	if (People)
		for (const auto& C : People->Citizens)
			if (City.GetEntities().Contains(C.HomeEntityId) &&
			    FCityHousingHistory::IsSearching(static_cast<uint8>(C.HousingStatus)))
			{
				auto& Row = Lots.FindOrAdd(C.HomeEntityId);
				Row.EntityId = C.HomeEntityId;
				++Row.SearchingResidents;
				Row.LongestDiscontent = FMath::Max(Row.LongestDiscontent, C.DiscontentMonths);
				Row.bHousingCareHold |= C.HousingStatus == ECityHousingStatus::WaitingForCare;
				Row.bMovingFeeUnaffordable |= C.HousingStatus == ECityHousingStatus::CannotAfford;
			}
	Lots.Remove(ExcludedEntityId);
	TArray<FCityConditionIndicator> Rows;
	Lots.GenerateValueArray(Rows);
	Rows.Sort([](const auto& A, const auto& B) {
		if (A.Priority() != B.Priority())
			return A.Priority() < B.Priority();
		return A.EntityId < B.EntityId;
	});
	TArray<FCityConditionIndicator> Clusters;
	for (auto& Row : Rows)
	{
		const auto& Entity = City.GetEntities().FindChecked(Row.EntityId);
		const FVector2D Center(Entity.Anchor.X + Entity.FootprintSize.X * .5,
		                       Entity.Anchor.Y + Entity.FootprintSize.Y * .5);
		Row.EntityIds.Add(Row.EntityId);
		Row.RegionBounds = FBox2D(Center, Center);
		auto* Cluster = Clusters.FindByPredicate([&](const auto& Other) {
			if (Other.RecentHousingKind || Row.RecentHousingKind || Other.bFire != Row.bFire ||
			    Other.OperationFailure != Row.OperationFailure ||
			    Other.SettledConcerns != Row.SettledConcerns ||
			    Other.bSupplyEmpty != Row.bSupplyEmpty || Other.bRetailStockout != Row.bRetailStockout ||
			    Other.bSustainedDiscontent != Row.bSustainedDiscontent ||
			    Other.bNeedsRepair != Row.bNeedsRepair || Other.bRepairing != Row.bRepairing ||
			    Other.bCrime != Row.bCrime || (Other.IllResidents > 0) != (Row.IllResidents > 0) ||
			    (Other.PatientsInCare > 0) != (Row.PatientsInCare > 0) ||
			    (Other.WasteUnits > 0) != (Row.WasteUnits > 0) ||
			    (Other.SearchingResidents > 0) != (Row.SearchingResidents > 0) ||
			    Other.bHousingCareHold != Row.bHousingCareHold ||
			    Other.bMovingFeeUnaffordable != Row.bMovingFeeUnaffordable)
				return false;
			FBox2D Bounds = Other.RegionBounds;
			Bounds += Center;
			return Bounds.GetSize().SizeSquared() <= FMath::Square(FMath::Max(0., ClusterDiameter));
		});
		if (!Cluster)
		{
			Clusters.Add(MoveTemp(Row));
			continue;
		}
		Cluster->EntityIds.Append(Row.EntityIds);
		Cluster->RegionBounds += Center;
		Cluster->WasteUnits += Row.WasteUnits;
		Cluster->SearchingResidents += Row.SearchingResidents;
		Cluster->LongestDiscontent = FMath::Max(Cluster->LongestDiscontent, Row.LongestDiscontent);
		Cluster->IllResidents += Row.IllResidents;
		Cluster->PatientsInCare += Row.PatientsInCare;
		Cluster->CrimeDays = FMath::Max(Cluster->CrimeDays, Row.CrimeDays);
		Cluster->RepairDays = FMath::Max(Cluster->RepairDays, Row.RepairDays);
		Cluster->RepairPercent = FMath::Min(Cluster->RepairPercent, Row.RepairPercent);
	}
	// Aggregate before limiting popups so a dense neighborhood's totals are complete.
	if (OutAffectedLots)
		*OutAffectedLots = Rows.Num();
	if (Clusters.Num() > FMath::Max(0, Limit))
		Clusters.SetNum(FMath::Max(0, Limit));
	return Clusters;
}

bool FCityConditionIndicators::Hit(const FTransform& Pose, const FVector& Origin, const FVector& Direction,
                                   double& Distance)
{
	if (Origin.ContainsNaN() || Direction.ContainsNaN() || Direction.IsNearlyZero())
		return false;
	const FVector Ray = Direction.GetSafeNormal(), Normal = Pose.GetRotation().GetForwardVector();
	const double Denominator = FVector::DotProduct(Ray, Normal);
	if (Denominator >= -KINDA_SMALL_NUMBER)
		return false;
	Distance = FVector::DotProduct(Pose.GetLocation() - Origin, Normal) / Denominator;
	if (Distance < 0 || Distance > 1000)
		return false;
	const FVector Local = Pose.InverseTransformPosition(Origin + Ray * Distance);
	return FMath::Abs(Local.Y) <= HalfWidth && FMath::Abs(Local.Z) <= HalfHeight;
}
