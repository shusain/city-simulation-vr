#include "Simulation/CityHousingHistory.h"
#include "Simulation/CityPopulationSimulation.h"
#include "Simulation/CityHappinessSimulation.h"
#include "Simulation/CityHousingMigration.h"

bool FCityHousingHistory::IsSearching(uint8 Status)
{
	return Status == static_cast<uint8>(ECityHousingStatus::Searching) ||
	       Status == static_cast<uint8>(ECityHousingStatus::CannotAfford) ||
	       Status == static_cast<uint8>(ECityHousingStatus::WaitingForCare);
}
void FCityHousingHistory::Record(FCityPopulationSnapshot& P, FCityHousingEvent E)
{
	if (P.NextHousingEventId == MAX_uint64)
		return;
	E.Id = P.NextHousingEventId++;
	if (E.CitizenIds.Num() > MaximumCitizenIds)
		E.CitizenIds.SetNum(MaximumCitizenIds);
	P.HousingEvents.Add(MoveTemp(E));
	if (P.HousingEvents.Num() > MaximumEvents)
		P.HousingEvents.RemoveAt(0, P.HousingEvents.Num() - MaximumEvents);
}
bool FCityHousingHistory::Validate(const FCityPopulationSnapshot& P, FString& Error,
                                   const FCityGridDimensions* Dimensions, uint64 NextHomeId)
{
	uint64 LastId = 0;
	int64 LastMonth = 0;
	if (!P.NextHousingEventId || P.HousingEvents.Num() > MaximumEvents)
	{
		Error = TEXT("Invalid housing history size/identity");
		return false;
	}
	for (const auto& E : P.HousingEvents)
	{
		const bool Move = E.Kind == ECityHousingEventKind::Relocated;
		const auto Status = static_cast<ECityHousingStatus>(E.HousingStatus);
		const bool StatusMatches =
		    Move                                         ? Status == ECityHousingStatus::Settling
		    : E.Kind == ECityHousingEventKind::Searching ? Status == ECityHousingStatus::Searching
		    : E.Kind == ECityHousingEventKind::Waiting
		        ? Status == ECityHousingStatus::CannotAfford || Status == ECityHousingStatus::WaitingForCare
		    : E.Kind == ECityHousingEventKind::Recovered
		        ? Status == ECityHousingStatus::Stable || Status == ECityHousingStatus::Recovering
		        : Status == ECityHousingStatus::Searching || Status == ECityHousingStatus::CannotAfford;
		if (!E.Id || E.Id <= LastId || E.Id >= P.NextHousingEventId || E.Month <= 0 || E.Month < LastMonth ||
		    E.Month > P.LastMigrationMonth ||
		    static_cast<uint8>(E.Kind) > static_cast<uint8>(ECityHousingEventKind::Departed) ||
		    E.HousingStatus > static_cast<uint8>(ECityHousingStatus::Recovering) || E.Residents <= 0 ||
		    E.CitizenIds.Num() != FMath::Min(E.Residents, MaximumCitizenIds) || E.Happiness < 0 || E.Happiness > 100 ||
		    E.Outlook < 0 || E.Outlook > 100 || E.DestinationOutlook < 0 || E.DestinationOutlook > 100 ||
		    E.Concerns < 0 || (E.Concerns & ~FCityHappinessEvaluator::ValidConcerns) || E.Money < 0 ||
		    (Move ? !E.DestinationHome || E.SourceHome == E.DestinationHome : E.DestinationHome != 0) ||
		    (E.SourceHome ? E.SourceCell.X < 0 || E.SourceCell.Y < 0 : E.SourceCell != FCityCellCoord(-1, -1)) ||
		    (Move ? E.DestinationCell.X < 0 || E.DestinationCell.Y < 0 : E.DestinationCell != FCityCellCoord(-1, -1)) ||
		    (E.SourceCell.RoadLevel || E.DestinationCell.RoadLevel) ||
		    (!Move && E.Kind != ECityHousingEventKind::Departed && E.Money != 0) || !StatusMatches ||
		    (Move && (E.DestinationOutlook < 50 || E.DestinationOutlook < E.Outlook + 10 || E.Money <= 0)) ||
		    E.SourceHome >= NextHomeId || E.DestinationHome >= NextHomeId ||
		    (Dimensions && ((E.SourceHome && !Dimensions->IsValidCell(E.SourceCell)) ||
		                    (E.DestinationHome && !Dimensions->IsValidCell(E.DestinationCell)))))
		{
			Error = TEXT("Invalid housing event snapshot");
			return false;
		}
		uint64 LastCitizen = 0;
		for (uint64 Id : E.CitizenIds)
		{
			if (!Id || Id <= LastCitizen || Id >= P.NextCitizenId)
			{
				Error = TEXT("Invalid housing event citizen identity");
				return false;
			}
			LastCitizen = Id;
		}
		LastId = E.Id;
		LastMonth = E.Month;
	}
	return true;
}
namespace
{
	FString Home(uint64 Id, const FCityCellCoord& Cell)
	{
		return Id ? FString::Printf(TEXT("Home #%llu (%d,%d)"), Id, Cell.X, Cell.Y) : TEXT("No home");
	}
	FString WrapReasons(FString Text)
	{
		FString R;
		int32 Length = 0;
		TArray<FString> Words;
		Text.ParseIntoArray(Words, TEXT(" "), true);
		for (const auto& Word : Words)
		{
			if (Length && Length + 1 + Word.Len() > 100)
			{
				R += TEXT("\n");
				Length = 0;
			}
			else if (Length)
			{
				R += TEXT(" ");
				++Length;
			}
			R += Word;
			Length += Word.Len();
		}
		return R;
	}
} // namespace
FString FCityHousingHistory::Describe(const FCityHousingEvent& E)
{
	const TCHAR* K = E.Kind == ECityHousingEventKind::Relocated   ? TEXT("MOVED")
	                 : E.Kind == ECityHousingEventKind::Departed  ? TEXT("LEFT CITY")
	                 : E.Kind == ECityHousingEventKind::Recovered ? TEXT("SEARCH ENDED")
	                 : E.Kind == ECityHousingEventKind::Waiting   ? TEXT("MOVE DELAYED")
	                                                              : TEXT("HOME SEARCH");
	FString R = FString::Printf(TEXT("Month %lld | %s | %d residents\n%s"), E.Month, K, E.Residents,
	                            *Home(E.SourceHome, E.SourceCell));
	if (E.DestinationHome)
		R += TEXT(" -> ") + Home(E.DestinationHome, E.DestinationCell);
	TArray<FString> Ids;
	for (int32 I = 0; I < FMath::Min(4, E.CitizenIds.Num()); ++I)
		Ids.Add(FString::Printf(TEXT("#%llu"), E.CitizenIds[I]));
	R += TEXT("\nCitizens ") + FString::Join(Ids, TEXT(", "));
	if (E.Residents > Ids.Num())
		R += FString::Printf(TEXT(" (+%d more)"), E.Residents - Ids.Num());
	R += FString::Printf(TEXT("\nHappiness %d | outlook %d"), E.Happiness, E.Outlook);
	if (E.DestinationHome)
		R += FString::Printf(TEXT(" -> %d | moving fee $%lld"), E.DestinationOutlook, E.Money);
	else if (E.Kind == ECityHousingEventKind::Departed)
		R += FString::Printf(TEXT(" | cash carried out $%lld"), E.Money);
	else
		R += TEXT(" | ") + FString(FCityHousingMigration::Describe(static_cast<ECityHousingStatus>(E.HousingStatus)));
	R += TEXT("\n") + WrapReasons(TEXT("Concerns: ") + FCityHappinessEvaluator::DescribeConcerns(E.Concerns));
	if (E.Kind == ECityHousingEventKind::Departed)
		R += TEXT("\nNo viable affordable home; six unresolved low months");
	return R;
}
FString FCityHousingHistory::Recent(const FCityPopulationSnapshot& P, int32 Page, int32 PerPage)
{
	if (P.HousingEvents.IsEmpty())
		return TEXT(
		    "No housing events recorded yet.\nHistory begins with new monthly decisions; no past events inferred.");
	PerPage = FMath::Clamp(PerPage, 1, EventsPerPage);
	Page = FMath::Clamp(Page, 0, (P.HousingEvents.Num() - 1) / PerPage);
	FString R;
	const int32 First = P.HousingEvents.Num() - 1 - Page * PerPage;
	for (int32 I = First; I >= 0 && I > First - PerPage; --I)
	{
		if (!R.IsEmpty())
			R += TEXT("\n\n");
		R += Describe(P.HousingEvents[I]);
	}
	return R;
}
FString FCityHousingHistory::Warnings(const FCityPopulationSnapshot& P)
{
	TSet<uint64> Homes;
	int32 Residents = 0, Unhoused = 0;
	for (const auto& C : P.Citizens)
		if (IsSearching(static_cast<uint8>(C.HousingStatus)))
		{
			++Residents;
			if (C.HomeEntityId)
				Homes.Add(C.HomeEntityId);
			else
				++Unhoused;
		}
	return FString::Printf(
	    TEXT(
	        "Seeking better housing: %d residents | %d homes | %d unhoused\nInspect warning badges for needs; monthly decisions only."),
	    Residents, Homes.Num(), Unhoused);
}
