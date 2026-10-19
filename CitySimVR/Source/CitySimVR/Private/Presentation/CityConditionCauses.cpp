#include "Presentation/CityConditionIndicators.h"
#include "Presentation/CityServiceGuidance.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Simulation/CityHappinessSimulation.h"
#include "Simulation/CityTrafficSimulation.h"

FString FCityConditionIndicators::Explain(const UCitySimulationSubsystem& Sim, const uint64 Id)
{
	const auto& City = Sim.GetPlacementModel();
	const auto& People = Sim.GetPopulationSnapshot();
	const auto Rows = Build(City, Sim.GetFireSnapshot(), Sim.GetCrimeSnapshot(),
	                        Sim.GetEconomyTuning().FireRepairSteps, &Sim.GetHealthSnapshot(), &People, 0,
	                        &Sim.GetWasteSnapshot(), &Sim.GetServiceSnapshot(), MAX_int32, 0);
	const auto* Row = Rows.FindByPredicate([&](const auto& R) { return R.EntityId == Id; });
	FCityConditionIndicator Neutral;
	if (!Row)
	{
		const auto* H = People.Households.FindByPredicate([&](const auto& Home) { return Home.HomeEntityId == Id; });
		if (H && H->bUsesStoredSupplies) Row = &Neutral;
		else
		{
			const auto* Business = People.Businesses.FindByPredicate([&](const auto& B) { return B.EntityId == Id; });
			if (Business && !Business->bInitialFreightReceived)
				return TEXT("Opening delivery pending | Startup grace; freight must actually arrive");
			return FString();
		}
	}
	TArray<FString> Lines;
	if (Row != &Neutral) Lines.Add(Row->Title(Sim.GetRepairCost(Id)) + TEXT(" | ") + Row->Detail(Sim.GetEconomyTuning().FixedStepsPerMonth));
	if (Row->bNeedsRepair && Row->bCrime)
		Lines.Add(TEXT("Also structural damage | Repair"));
	if (Row->bRepairing && (Row->bFire || Row->bCrime))
		Lines.Add(
		    FString::Printf(TEXT("Repair %d%% | %lld game minutes left"), Row->RepairPercent, Row->RepairDays * 1440 / FMath::Max(1, Sim.GetEconomyTuning().FixedStepsPerMonth)));
	if (Row->IllResidents && Row->Symbol() != TEXT("Health"))
		Lines.Add(FString::Printf(TEXT("Also %d ill residents | Hospital access / care"), Row->IllResidents));
	if (Row->WasteUnits && Row->Symbol() != TEXT("Waste"))
		Lines.Add(TEXT("Also waste accumulation | Collection access / capacity"));
	if (Row->SearchingResidents && Row->Symbol() != TEXT("Leaving"))
		Lines.Add(TEXT("Also searching for housing | Inspect recorded concerns"));
	if (Row->RecentHousingKind)
		for (const auto& Event : People.HousingEvents)
			if (Event.SourceHome == Id && Event.Month == Row->RecentHousingMonth &&
			    static_cast<uint8>(Event.Kind) + 1 == Row->RecentHousingKind)
				Lines.Add(FCityHousingHistory::Describe(Event));
	if (Row->OperationFailure)
	{
		const auto Failure = static_cast<ECityOperationFailure>(Row->OperationFailure);
		if (Failure == ECityOperationFailure::NoService ||
		    Failure == ECityOperationFailure::ServiceCapacity ||
		    Failure == ECityOperationFailure::LowWaterPressure)
		{
			const auto Cause = Sim.GetServiceSnapshot().Water.EntityCauses.FindRef(Id);
			Lines.Add(FCityWaterEvaluator::FailureToString(Cause) + TEXT(" | Water source / road pipes"));
		}
	}
	if (Row->SettledConcerns || Row->bSupplyEmpty || Row == &Neutral)
	{
		const auto* Home =
		    People.Households.FindByPredicate([&](const auto& H) { return H.HomeEntityId == Id; });
		if (Home)
		{
			TMap<uint64, int32> HeldStock;
			int64 Funds = 0;
			int32 HeldOrders = 0;
			for (const auto& C : People.Citizens)
				if (C.HouseholdId == Home->Id)
					Funds += C.AvailableMoney;
			const FCityPurchaseReservation* Reserved = nullptr;
			for (const auto& Order : People.PurchaseReservations)
			{
				HeldStock.FindOrAdd(Order.ShopId) += Order.Quantity;
				if (Order.HouseholdId == Home->Id)
				{
					Funds -= Order.Price;
					++HeldOrders;
					if (!Reserved)
						Reserved = &Order;
				}
			}
			if (Home->bUsesStoredSupplies)
			{
				Lines.Add(FString::Printf(TEXT("Essentials stored: %d units | %.1f days of supplies"), Home->SupplyUnits,
				    Home->ConsumptionOrders > 0 ? double(Home->SupplyUnits) / Home->ConsumptionOrders : 0.));
				Lines.Add(Home->LastEssentialPurchaseDay < 0 ? TEXT("Starter supplies | First shopping trip pending")
				    : TEXT("Bulk essentials: about weekly | Top up when one day remains"));
				Lines.Add(FString::Printf(TEXT("Goods bought yesterday: %d | Today: %d | Orders: %d"), Home->CompletedPurchases, Home->PurchasesThisMonth, HeldOrders));
			}
			else Lines.Add(FString::Printf(TEXT("Yesterday: %d/%d purchases | Today: %d/%d | Orders reserved: %d"),
			    Home->CompletedPurchases, Home->ConsumptionOrders, Home->PurchasesThisMonth,
			    Home->ConsumptionOrders, HeldOrders));
			const int64 Price = FMath::Max<int64>(0, Sim.GetEconomyTuning().HouseholdIncomePerGoodsPurchase);
			const auto* Shop = People.Businesses.FindByPredicate([&](const auto& B) {
				return FCityPopulationSimulation::CanReserveShop(B, HeldStock.FindRef(B.EntityId));
			});
			if (Reserved)
			{
				Lines.Add(FString::Printf(TEXT("%s order: %d goods / $%lld"), Reserved->ComfortTier ? TEXT("Optional comfort") : TEXT("Essential"), Reserved->Quantity, Reserved->Price));
				const auto* Buyer =
				    People.Citizens.FindByPredicate([&](const auto& C) { return C.Id == Reserved->BuyerId; });
				FCityCellCoord Start, End;
				FCityTrafficVehicleState Route;
				Route.VehicleType = ECityTripVehicleType::Walk;
				const uint64 Origin =
				    Buyer && Buyer->CurrentLocationEntityId ? Buyer->CurrentLocationEntityId : Id;
				const bool Access =
				    FCityTrafficSimulation::FindPortal(City, Sim.GetBuildingCatalog(), Origin, Start) &&
				    FCityTrafficSimulation::FindPortal(City, Sim.GetBuildingCatalog(), Reserved->ShopId,
				                                       End) &&
				    (Start == End ||
				     FCityTrafficSimulation::BuildRoute(Sim.GetRoadNetworkSnapshot(), Start, End, Route));
				Lines.Add(FString::Printf(TEXT("Current order: shop #%llu | %s"), Reserved->ShopId,
				                          Access ? TEXT("walk route available; awaiting actual visit")
				                                 : TEXT("no valid walk route; check road access")));
				if (Buyer)
				{
					const auto MatchesBuyer = [&](const auto& V) {
						return !V.bTransitTrip && V.SourceId == Buyer->Id &&
						       V.OriginEntityId == Origin && V.DestinationEntityId == Reserved->ShopId;
					};
					const auto& Traffic = Sim.GetTrafficSnapshot();
					if (Buyer->Schedule == ECityCitizenSchedule::Work)
					{
						const int64 Minutes = (Buyer->NextActionStep % FMath::Max(1, Sim.GetEconomyTuning().FixedStepsPerMonth)) * 1440 / FMath::Max(1, Sim.GetEconomyTuning().FixedStepsPerMonth);
						Lines.Add(FString::Printf(TEXT("Buyer at work until %02lld:%02lld | Shopping follows the shift"), Minutes / 60, Minutes % 60));
					}
					else if (Buyer->Schedule == ECityCitizenSchedule::ToWork)
						Lines.Add(TEXT("Buyer commuting to work | Shopping follows the shift"));
					else if (Buyer->Schedule == ECityCitizenSchedule::ToHome)
						Lines.Add(TEXT("Buyer returning home | Reserved shopping follows arrival"));
					else if (Buyer->Schedule == ECityCitizenSchedule::Home && Buyer->NextActionStep > Traffic.FixedStepId)
						Lines.Add(TEXT("Buyer at home | Waiting for morning routine"));
					else if (!Access)
						Lines.Add(TEXT("Shopping route unavailable | Reallocation retries during the day"));
					else if (const auto* Waiting = Traffic.WaitingDepartures.FindByPredicate(MatchesBuyer))
						Lines.Add(FString::Printf(TEXT("Departure blocked by traffic | queued %lld game minutes; waiting for a safe gap"),
						    FMath::Max<int64>(0, Traffic.FixedStepId - Waiting->DepartureRequestedStep) * 1440 / FMath::Max(1, Sim.GetEconomyTuning().FixedStepsPerMonth)));
					else if (const auto* Moving = Traffic.Vehicles.FindByPredicate(MatchesBuyer))
						Lines.Add(Moving->bCompleted ? TEXT("Shop arrival recorded | settling purchase")
						                            : Buyer->bShopBeforeWork ? TEXT("Shopper travelling before work | Work follows the visit")
						                            : TEXT("Shopper travelling | purchase settles on arrival"));
					else
						Lines.Add(FString::Printf(TEXT("Shopping departure pending | trip requested %lld game minutes ago"),
						    FMath::Max<int64>(0, Traffic.FixedStepId - Buyer->NextActionStep) * 1440 / FMath::Max(1, Sim.GetEconomyTuning().FixedStepsPerMonth)));
				}
			}
			else if (Home->bUsesStoredSupplies && Home->SupplyUnits > Home->ConsumptionOrders)
				Lines.Add(TEXT("Essentials covered | No daily shopping trip required"));
			else if (!Home->bUsesStoredSupplies && Home->PurchasesThisMonth >= Home->ConsumptionOrders)
				Lines.Add(TEXT("Today's shopping fulfilled | Warning reflects yesterday's purchases"));
			else if (Funds < Price)
				Lines.Add(FString::Printf(
				    TEXT("Current unreserved cash $%lld / $%lld per purchase | Income / reserves"), Funds,
				    Price));
			else if (!Shop)
			{
				bool Stock = false, Slots = false;
				for (const auto& B : People.Businesses)
					if (!B.bIndustrial)
					{
						Stock |= B.Inventory > HeldStock.FindRef(B.EntityId);
						Slots |= B.PendingOrders - B.SalesThisMonth > HeldStock.FindRef(B.EntityId);
					}
				Lines.Add(!Stock   ? TEXT("Current shops: no unreserved stock | Retail / deliveries")
				          : !Slots ? TEXT("Current shops: no free purchase slots | Retail capacity")
				                   : TEXT("Current shops: stock and free slots are at different shops | "
				                          "Retail capacity"));
			}
			else
			{
				FCityCellCoord Start;
				TMap<FCityCellCoord, int32> Distances;
				if (FCityTrafficSimulation::FindPortal(City, Sim.GetBuildingCatalog(), Id, Start))
					Sim.GetRoadNetworkSnapshot().FindWalkingDistances(Start, Distances);
				const bool Reachable = People.Businesses.ContainsByPredicate([&](const auto& B) {
					FCityCellCoord End;
					return FCityPopulationSimulation::CanReserveShop(B, HeldStock.FindRef(B.EntityId)) &&
					    FCityTrafficSimulation::FindPortal(City, Sim.GetBuildingCatalog(), B.EntityId, End) && Distances.Contains(End);
				});
				Lines.Add(Reachable ? TEXT("Stock, capacity and cash available | Allocation retries during the day")
				                    : TEXT("Current shops: no valid walk route from home | Check pedestrian road access"));
				if (!Home->bUsesStoredSupplies) Lines.Add(TEXT("Yesterday's shortfall is recorded separately from today's orders"));
			}
			if (Home->bUsesStoredSupplies)
			{
				const int32 Comfort = FCityPopulationSimulation::PreferredComfortTier(*Home, Funds, Price, People.Citizens);
				Lines.Add(Comfort ? FString::Printf(TEXT("Optional comfort preference: %s commercial goods | Essentials stay first"), Comfort == 2 ? TEXT("high-density") : TEXT("medium-density"))
				                  : TEXT("Comfort goods are optional | Build savings and cover essentials first"));
			}
		}
	}
	if (Row->bSustainedDiscontent || Row->SearchingResidents)
	{
		const auto Mood = FCityHappinessEvaluator::Summarize(People, Id);
		Lines.Add(TEXT("Last settled concerns: ") + FCityHappinessEvaluator::DescribeConcerns(Mood.Concerns));
	}
	return FString::Join(Lines, TEXT("\n"));
}

uint64 FCityConditionIndicators::RelatedLot(const UCitySimulationSubsystem& Sim, const uint64 Id,
                                            FString& Label)
{
	Label.Reset();
	const auto& City = Sim.GetPlacementModel();
	const auto& People = Sim.GetPopulationSnapshot();
	const auto* Home = People.Households.FindByPredicate([&](const auto& H) { return H.HomeEntityId == Id; });
	if (Home)
		for (const auto& Order : People.PurchaseReservations)
			if (Order.HouseholdId == Home->Id && City.GetEntities().Contains(Order.ShopId))
			{
				Label = FString::Printf(TEXT("Inspect reserved shop #%llu"), Order.ShopId);
				return Order.ShopId;
			}
	const auto* Entity = City.GetEntities().Find(Id);
	if (!Entity)
		return 0;
	const auto& Services = Sim.GetServiceSnapshot();
	if (Services.GetFailure(Id) == ECityOperationFailure::TechnologyPrerequisite && Sim.GetBuildingCatalog())
		if (const auto* Definition = Sim.GetBuildingCatalog()->FindDefinition(Entity->DefinitionId))
			for (const auto Required : Definition->RequiredOperatingFacilities)
				if (Services.OperatingFacilities.FindRef(Required) <= 0)
				{
					uint64 Candidate = 0;
					for (const auto& Pair : City.GetEntities())
						if (Pair.Value.DefinitionId == Required && (!Candidate || Pair.Key < Candidate))
							Candidate = Pair.Key;
					if (Candidate)
					{
						Label = FString::Printf(TEXT("Inspect offline prerequisite #%llu"), Candidate);
						return Candidate;
					}
				}
	const auto& Water = Sim.GetServiceSnapshot().Water;
	if (!Water.EntityDemand.FindRef(Id) || Water.EntityCauses.FindRef(Id) == ECityWaterFailure::None)
		return 0;
	TArray<FCityCellCoord> Roads;
	City.GetAdjacentRoadCells(Entity->Anchor, Entity->FootprintSize, true, Roads, Water.RulesVersion > 0);
	uint64 Closest = 0;
	int32 Distance = MAX_int32;
	for (const auto& Source : Water.Sources)
		if (Source.EntityId != Id && City.GetEntities().Contains(Source.EntityId))
			for (const auto& Road : Roads)
				if (const auto* Hops = Source.RoadDistances.Find(Road);
				    Hops &&
				    (*Hops < Distance || (*Hops == Distance && (!Closest || Source.EntityId < Closest))))
				{
					Closest = Source.EntityId;
					Distance = *Hops;
				}
	if (Closest)
		Label = FString::Printf(TEXT("Inspect nearest connected source #%llu"), Closest);
	return Closest;
}
