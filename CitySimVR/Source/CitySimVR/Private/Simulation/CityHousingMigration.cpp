#include "Simulation/CityHousingMigration.h"
#include "Simulation/CityHappinessSimulation.h"
#include "Simulation/CityPopulationSimulation.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Simulation/CityHealthSimulation.h"
#include "Simulation/CityCrimeSimulation.h"
#include "Commands/CityPlacementModel.h"

namespace
{
	struct FHouseholdGroup
	{
		uint64 Home = 0;
		TArray<int32> Members;
		int64 Happiness = 0, Target = 0, LatestMove = 0;
		int32 Seeking = 0, Leaving = 0;
		bool Blocked = false;
		bool WantsToMove() const
		{
			return !Members.IsEmpty() && Happiness / Members.Num() < FCityHappinessEvaluator::DiscontentThreshold &&
			       Target / Members.Num() < FCityHappinessEvaluator::DiscontentThreshold &&
			       Seeking * 2 >= Members.Num();
		}
	};
	TArray<FHouseholdGroup> Groups(const FCityPopulationSnapshot& P)
	{
		TArray<FHouseholdGroup> Result;
		TMap<uint64, int32> Homes;
		for (int32 I = 0; I < P.Citizens.Num(); ++I)
		{
			const auto& C = P.Citizens[I];
			int32 Index = INDEX_NONE;
			if (C.HomeEntityId)
				if (const auto* Existing = Homes.Find(C.HomeEntityId))
					Index = *Existing;
			if (Index == INDEX_NONE)
			{
				Index = Result.AddDefaulted();
				Result[Index].Home = C.HomeEntityId;
				if (C.HomeEntityId)
					Homes.Add(C.HomeEntityId, Index);
			}
			auto& G = Result[Index];
			G.Members.Add(I);
			G.Happiness += C.Happiness;
			G.Target += C.HappinessTarget;
			G.Seeking += C.DiscontentMonths >= FCityHappinessEvaluator::SustainedMonths;
			G.Leaving += C.DiscontentMonths >= FCityHousingMigration::DepartureMonths;
			G.LatestMove = FMath::Max(G.LatestMove, C.LastRelocationMonth);
			G.Blocked |= C.IsDetained() || C.bOnMedicalLeave || C.HousingStatus == ECityHousingStatus::WaitingForCare;
		}
		for (auto& G : Result)
			G.Members.Sort([&](int32 A, int32 B) { return P.Citizens[A].Id < P.Citizens[B].Id; });
		Result.Sort(
		    [&](const auto& A, const auto& B)
		    { return A.Home != B.Home ? A.Home < B.Home : P.Citizens[A.Members[0]].Id < P.Citizens[B.Members[0]].Id; });
		return Result;
	}
} // namespace

FCityHousingAdmissionPlan FCityHousingMigration::AdmissionPlan(const FCityPopulationSnapshot& P,
                                                               const FCityServiceSnapshot& S)
{
	FCityHousingAdmissionPlan Plan;
	int64 Capacity = 0;
	TArray<uint64> Homes;
	S.EntityResidents.GetKeys(Homes);
	Homes.Sort();
	TMap<uint64, int32> Occupants;
	for (const auto& C : P.Citizens)
		++Occupants.FindOrAdd(C.HomeEntityId);
	for (const auto& H : P.HousingVacancyHolds)
		if (H.UntilMonth > P.LastSettledMonthId)
			Plan.ReservedHomes.Add(H.HomeEntityId);
	for (const auto& G : Groups(P))
		if (G.Home && G.WantsToMove())
		{
			// New arrivals must not dilute the household's warning or occupy its destination.
			Plan.ReservedHomes.Add(G.Home);
			if (G.Blocked || (G.LatestMove && P.LastSettledMonthId - G.LatestMove < CooldownMonths))
				continue;
			for (uint64 Id : Homes)
				if (!Plan.ReservedHomes.Contains(Id) && !Occupants.FindRef(Id) &&
				    S.EntityResidents.FindRef(Id) >= G.Members.Num())
				{
					Plan.ReservedHomes.Add(Id);
					break;
				}
		}
	for (uint64 Id : Homes)
		if (!Plan.ReservedHomes.Contains(Id))
			Capacity += FMath::Max(0, S.EntityResidents.FindRef(Id));
	Plan.TargetResidents =
	    P.LastSettledMonthId < P.ImmigrationResumeMonth
	        ? P.Citizens.Num()
	        : FMath::Max(P.Citizens.Num(), static_cast<int32>(FMath::Min<int64>(MAX_int32, Capacity)));
	return Plan;
}

bool FCityHousingMigration::Validate(const FCityPopulationSnapshot& P, FString& Error, const FCityPlacementModel* City)
{
	if (P.LastMigrationMonth < 0 || P.LastMigrationMonth > P.LastSettledMonthId || P.ImmigrationResumeMonth < 0 ||
	    (P.ImmigrationResumeMonth > P.LastMigrationMonth &&
	     P.ImmigrationResumeMonth - P.LastMigrationMonth > CooldownMonths) ||
	    P.RelocatedResidents < 0 || P.DepartedResidents < 0 || P.MovingExpenses < 0 || P.DepartureMoney < 0 ||
	    (P.ImmigrationResumeMonth > 0 && P.DepartedResidents == 0))
	{
		Error = TEXT("Invalid housing migration ledger or clock");
		return false;
	}
	TSet<uint64> Held;
	for (const auto& H : P.HousingVacancyHolds)
	{
		const auto* Home = City ? City->GetEntities().Find(H.HomeEntityId) : nullptr;
		if (!H.HomeEntityId || Held.Contains(H.HomeEntityId) || H.UntilMonth <= P.LastMigrationMonth ||
		    H.UntilMonth - P.LastMigrationMonth > CooldownMonths ||
		    (City && Home && Home->DefinitionId != TEXT("Home")))
		{
			Error = TEXT("Invalid housing vacancy hold");
			return false;
		}
		Held.Add(H.HomeEntityId);
	}
	for (const auto& C : P.Citizens)
		if (C.LastRelocationMonth > P.LastMigrationMonth || C.AvailableMoney < 0 || Held.Contains(C.HomeEntityId))
		{
			Error = TEXT("Citizen conflicts with housing migration state");
			return false;
		}
	return FCityHousingHistory::Validate(P, Error);
}

const TCHAR* FCityHousingMigration::Describe(ECityHousingStatus Status)
{
	switch (Status)
	{
	case ECityHousingStatus::Searching:
		return TEXT("seeking a better available home");
	case ECityHousingStatus::CannotAfford:
		return TEXT("moving fee unaffordable");
	case ECityHousingStatus::WaitingForCare:
		return TEXT("care/custody/incident hold");
	case ECityHousingStatus::Settling:
		return TEXT("settling after a recent move");
	case ECityHousingStatus::Recovering:
		return TEXT("outlook improved; recovering");
	default:
		return TEXT("stable housing");
	}
}

bool FCityPopulationSimulation::AdvanceHousingMigration(
    int64 Month, const FCityPlacementModel& City, const FCityServiceSnapshot& Services,
    const FCityEnvironmentSnapshot& Environment, const FCityEconomySnapshot& Economy, const FCityEconomyTuning& Tuning,
    const FCityHealthSnapshot& Health, const FCityWasteSnapshot& Waste, const FCityCrimeSnapshot& Crime)
{
	if (Month <= Snapshot.LastMigrationMonth || Month != Snapshot.LastSettledMonthId ||
	    Month > MAX_int64 - FCityHousingMigration::CooldownMonths)
		return false;
	Snapshot.LastMigrationMonth = Month;
	Snapshot.HousingVacancyHolds.RemoveAll(
	    [&](const auto& H) { return H.UntilMonth <= Month || !City.GetEntities().Contains(H.HomeEntityId); });
	TSet<uint64> Unavailable, Departing;
	for (const auto& H : Snapshot.HousingVacancyHolds)
		Unavailable.Add(H.HomeEntityId);
	for (const auto& C : Snapshot.Citizens)
		if (C.HomeEntityId)
			Unavailable.Add(C.HomeEntityId);
	TSet<uint64> CareOrCustody;
	for (const auto& P : Health.Patients)
		CareOrCustody.Add(P.CitizenId);
	for (const auto& I : Crime.RecentIncidents)
		if (I.ArsonOutcome == ECityArsonOutcome::Pending || I.ArsonOutcome == ECityArsonOutcome::Detained)
			CareOrCustody.Add(I.OffenderCitizenId);
	TArray<uint64> Homes;
	Services.EntityResidents.GetKeys(Homes);
	Homes.Sort();
	bool Changed = false;
	for (const auto& G : Groups(Snapshot))
	{
		const auto Event = [&](ECityHousingEventKind Kind, ECityHousingStatus Status, uint64 Destination = 0,
		                       int32 DestinationOutlook = 0, int64 Money = 0)
		{
			FCityHousingEvent E;
			E.Kind = Kind;
			E.HousingStatus = static_cast<uint8>(Status);
			E.Month = Month;
			E.SourceHome = G.Home;
			E.DestinationHome = Destination;
			if (const auto* H = City.GetEntities().Find(G.Home))
				E.SourceCell = H->Anchor;
			if (const auto* H = City.GetEntities().Find(Destination))
				E.DestinationCell = H->Anchor;
			E.Residents = G.Members.Num();
			E.Happiness = G.Happiness / E.Residents;
			E.Outlook = G.Target / E.Residents;
			E.DestinationOutlook = DestinationOutlook;
			E.Money = Money;
			for (int32 I : G.Members)
			{
				E.CitizenIds.Add(Snapshot.Citizens[I].Id);
				E.Concerns |= Snapshot.Citizens[I].HappinessConcerns;
			}
			FCityHousingHistory::Record(Snapshot, MoveTemp(E));
		};
		const auto SetStatus = [&](ECityHousingStatus Status)
		{
			const auto Previous = Snapshot.Citizens[G.Members[0]].HousingStatus;
			if (Previous != Status)
			{
				if (Status == ECityHousingStatus::Searching)
					Event(ECityHousingEventKind::Searching, Status);
				else if (Status == ECityHousingStatus::CannotAfford || Status == ECityHousingStatus::WaitingForCare)
					Event(ECityHousingEventKind::Waiting, Status);
				else if (FCityHousingHistory::IsSearching(static_cast<uint8>(Previous)) &&
				         (Status == ECityHousingStatus::Stable || Status == ECityHousingStatus::Recovering))
					Event(ECityHousingEventKind::Recovered, Status);
			}
			for (int32 I : G.Members)
				Snapshot.Citizens[I].HousingStatus = Status;
		};
		if (!G.WantsToMove())
		{
			SetStatus(G.Happiness / G.Members.Num() < FCityHappinessEvaluator::DiscontentThreshold &&
			                  G.Target / G.Members.Num() >= FCityHappinessEvaluator::DiscontentThreshold
			              ? ECityHousingStatus::Recovering
			              : ECityHousingStatus::Stable);
			continue;
		}
		bool Blocked = false;
		int64 Funds = 0;
		for (int32 I : G.Members)
		{
			const auto& C = Snapshot.Citizens[I];
			Blocked |= C.IsDetained() || C.bOnMedicalLeave || CareOrCustody.Contains(C.Id);
			if (C.AvailableMoney < 0 || C.AvailableMoney > MAX_int64 - Funds)
				Blocked = true;
			else
				Funds += C.AvailableMoney;
		}
		if (Blocked)
		{
			SetStatus(ECityHousingStatus::WaitingForCare);
			continue;
		}
		if (G.LatestMove && Month - G.LatestMove < FCityHousingMigration::CooldownMonths)
		{
			SetStatus(ECityHousingStatus::Settling);
			continue;
		}
		const auto* Household =
		    Snapshot.Households.FindByPredicate([&](const auto& H) { return H.HomeEntityId == G.Home; });
		FCityHouseholdRecord Candidate = Household ? *Household : FCityHouseholdRecord();
		if (!Household)
			for (int32 I : G.Members)
				Candidate.ConsumptionOrders += Snapshot.Citizens[I].ConsumptionNeed;
		const int64 Fee = static_cast<int64>(FMath::Max(1, Candidate.ConsumptionOrders)) *
		                  FMath::Max(1, Tuning.HouseholdIncomePerGoodsPurchase);
		Candidate.Balance = FMath::Max<int64>(0, Funds - Fee);
		uint64 Best = 0;
		FCityHappinessAssessment BestAssessment;
		int32 BestTarget = -1;
		for (uint64 Id : Homes)
		{
			const auto* Home = City.GetEntities().Find(Id);
			if (Unavailable.Contains(Id) || !Home || Home->DefinitionId != TEXT("Home") ||
			    Services.EntityResidents.FindRef(Id) < G.Members.Num() ||
			    Services.GetFailure(Id) != ECityOperationFailure::None)
				continue;
			Candidate.HomeEntityId = Id;
			const auto A =
			    FCityHappinessEvaluator::AssessHome(Candidate, City, Services, Environment, Economy, Tuning, Waste);
			const int32 Target = FMath::Clamp(A.Target, 0, 100);
			if (Target >= FCityHappinessEvaluator::DiscontentThreshold &&
			    Target >= G.Target / G.Members.Num() + FCityHousingMigration::MinimumImprovement && Target > BestTarget)
			{
				Best = Id;
				BestTarget = Target;
				BestAssessment = A;
			}
		}
		const bool CanPay = Funds >= Fee && Snapshot.MovingExpenses <= MAX_int64 - Fee &&
		                    Snapshot.RelocatedResidents <= MAX_int64 - G.Members.Num();
		if (Best && CanPay)
		{
			Event(ECityHousingEventKind::Relocated, ECityHousingStatus::Settling, Best, BestTarget, Fee);
			int64 Remaining = Fee;
			for (int32 I : G.Members)
			{
				auto& C = Snapshot.Citizens[I];
				const int64 Debit = FMath::Min(Remaining, C.AvailableMoney);
				C.AvailableMoney -= Debit;
				Remaining -= Debit;
				C.HomeEntityId = C.HouseholdId = C.CurrentLocationEntityId = Best;
				C.LastRelocationMonth = Month;
				C.HousingStatus = ECityHousingStatus::Settling;
				C.HappinessTarget = BestTarget;
				C.HappinessConcerns = BestAssessment.Concerns;
				C.TripPurpose = ECityTripPurpose::None;
				C.TripDestinationEntityId = C.PendingShopEntityId = 0;
				C.Activity = ECityCitizenActivity::Home;
				C.Schedule = ECityCitizenSchedule::Home;
				C.NextActionStep = Economy.FixedStepId + 1 + C.Id % 4;
			}
			if (Household)
			{
				auto* Moving =
				    Snapshot.Households.FindByPredicate([&](const auto& H) { return H.HomeEntityId == G.Home; });
				Moving->Id = Moving->HomeEntityId = Best;
			}
			else
			{
				Candidate.Id = Candidate.HomeEntityId = Best;
				Snapshot.Households.Add(Candidate);
			}
			Snapshot.RelocatedResidents += G.Members.Num();
			Snapshot.MovingExpenses += Fee; // Named transfer: resident wallets -> external movers.
			Unavailable.Add(Best);
			Changed = true;
		}
		else
		{
			SetStatus(Best ? ECityHousingStatus::CannotAfford : ECityHousingStatus::Searching);
			if (G.Leaving * 2 < G.Members.Num() || Snapshot.DepartureMoney > MAX_int64 - Funds ||
			    Snapshot.DepartedResidents > MAX_int64 - G.Members.Num())
				continue;
			for (int32 I : G.Members)
				Departing.Add(Snapshot.Citizens[I].Id);
			Event(ECityHousingEventKind::Departed,
			      Best ? ECityHousingStatus::CannotAfford : ECityHousingStatus::Searching, 0, 0, Funds);
			Snapshot.DepartedResidents += G.Members.Num();
			Snapshot.DepartureMoney += Funds; // Resident assets cross the city boundary once.
			Snapshot.ImmigrationResumeMonth = Month + FCityHousingMigration::CooldownMonths;
			Changed = true;
		}
		if (G.Home)
		{
			Snapshot.HousingVacancyHolds.Add({G.Home, Month + FCityHousingMigration::CooldownMonths});
			Unavailable.Add(G.Home);
		}
	}
	Snapshot.Citizens.RemoveAll([&](const auto& C) { return Departing.Contains(C.Id); });
	Snapshot.Households.RemoveAll(
	    [&](const auto& H)
	    { return !Snapshot.Citizens.ContainsByPredicate([&](const auto& C) { return C.HouseholdId == H.Id; }); });
	if (Changed)
	{
		// Reservations lock existing wallets/stock; invalidation releases them without a refund/sale.
		PrunePurchaseReservations(City);
		RefreshHouseholdBalances();
	}
	Snapshot.Households.Sort([](const auto& A, const auto& B) { return A.Id < B.Id; });
	Snapshot.HousingVacancyHolds.Sort([](const auto& A, const auto& B) { return A.HomeEntityId < B.HomeEntityId; });
	return Changed;
}
