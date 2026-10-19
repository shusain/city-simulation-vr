#include "Persistence/CityWastePersistence.h"
#include "Dom/JsonObject.h"

namespace
{
	void Put(const TSharedPtr<FJsonObject>& O, const TCHAR* K, int64 V)
	{
		O->SetStringField(K, LexToString(V));
	}
	bool Get(const TSharedPtr<FJsonObject>& O, const TCHAR* K, int64& V)
	{
		FString S;
		if (!O || !O->TryGetStringField(K, S) || S.IsEmpty() || S.Len() > 20)
			return false;
		for (int32 I = 0; I < S.Len(); ++I)
			if (!FChar::IsDigit(S[I]) && !(I == 0 && S[I] == '-'))
				return false;
		return LexTryParseString(V, *S) && LexToString(V) == S;
	}
	bool GetId(const TSharedPtr<FJsonObject>& O, const TCHAR* K, uint64& V)
	{
		int64 N = 0;
		if (!Get(O, K, N) || N < 0)
			return false;
		V = N;
		return true;
	}
	bool GetInt(const TSharedPtr<FJsonObject>& O, const TCHAR* K, int32& V)
	{
		int64 N = 0;
		if (!Get(O, K, N) || N < MIN_int32 || N > MAX_int32)
			return false;
		V = N;
		return true;
	}
} // namespace
TSharedPtr<FJsonObject> FCityWastePersistence::Write(const FCityWasteSnapshot& S)
{
	auto O = MakeShared<FJsonObject>();
	Put(O, TEXT("step"), S.LastStep);
	Put(O, TEXT("introduced"), S.IntroductionStep);
	Put(O, TEXT("nextTruck"), S.NextTruckId);
	Put(O, TEXT("generated"), S.Generated);
	Put(O, TEXT("collected"), S.Collected);
	Put(O, TEXT("disposed"), S.Disposed);
	Put(O, TEXT("recycled"), S.Recycled);
	Put(O, TEXT("overflow"), S.Overflow);
	Put(O, TEXT("removed"), S.Removed);
	TArray<TSharedPtr<FJsonValue>> Sites, Depots, Trucks;
	for (const auto& S1 : S.Sites)
	{
		auto V = MakeShared<FJsonObject>();
		Put(V, TEXT("id"), S1.EntityId);
		Put(V, TEXT("units"), S1.Units);
		Put(V, TEXT("capacity"), S1.Capacity);
		Put(V, TEXT("remainder"), S1.GenerationRemainder);
		Put(V, TEXT("collectedAt"), S1.LastCollectionStep);
		Sites.Add(MakeShared<FJsonValueObject>(V));
	}
	for (const auto& D : S.Depots)
	{
		auto V = MakeShared<FJsonObject>();
		Put(V, TEXT("id"), D.EntityId);
		Put(V, TEXT("stored"), D.Stored);
		Put(V, TEXT("processed"), D.Processed);
		Put(V, TEXT("recycled"), D.Recycled);
		Depots.Add(MakeShared<FJsonValueObject>(V));
	}
	for (const auto& T : S.Trucks)
	{
		auto V = MakeShared<FJsonObject>();
		Put(V, TEXT("id"), T.Id);
		Put(V, TEXT("depot"), T.DepotId);
		Put(V, TEXT("current"), T.CurrentEntityId);
		Put(V, TEXT("trip"), T.TripId);
		Put(V, TEXT("cargo"), T.Cargo);
		Put(V, TEXT("workedAt"), T.LastWorkStep);
		TArray<TSharedPtr<FJsonValue>> Stops;
		for (uint64 Id : T.Stops)
			Stops.Add(MakeShared<FJsonValueString>(LexToString(Id)));
		V->SetArrayField(TEXT("stops"), Stops);
		Trucks.Add(MakeShared<FJsonValueObject>(V));
	}
	O->SetArrayField(TEXT("sites"), Sites);
	O->SetArrayField(TEXT("depots"), Depots);
	O->SetArrayField(TEXT("trucks"), Trucks);
	return O;
}
bool FCityWastePersistence::Read(const TSharedPtr<FJsonObject>& O, FCityWasteSnapshot& OutState, int64 MaximumStep,
                                 FString& Error, const FCityGridDimensions Dimensions)
{
	if (!Dimensions.IsValid())
	{
		Error = TEXT("Invalid waste grid dimensions");
		return false;
	}
	FCityWasteSnapshot S;
	S.Dimensions = Dimensions;
	Error = TEXT("Malformed waste state");
	if (!Get(O, TEXT("step"), S.LastStep) || !Get(O, TEXT("introduced"), S.IntroductionStep) ||
	    !GetId(O, TEXT("nextTruck"), S.NextTruckId) || !Get(O, TEXT("generated"), S.Generated) ||
	    !Get(O, TEXT("collected"), S.Collected) || !Get(O, TEXT("disposed"), S.Disposed) ||
	    !Get(O, TEXT("recycled"), S.Recycled) || !Get(O, TEXT("overflow"), S.Overflow) ||
	    !Get(O, TEXT("removed"), S.Removed))
		return false;
	const TArray<TSharedPtr<FJsonValue>>*Sites = nullptr, *Depots = nullptr, *Trucks = nullptr;
	if (!O->TryGetArrayField(TEXT("sites"), Sites) || Sites->Num() > Dimensions.GetCellCount() ||
	    !O->TryGetArrayField(TEXT("depots"), Depots) || Depots->Num() > Dimensions.GetCellCount() ||
	    !O->TryGetArrayField(TEXT("trucks"), Trucks) || Trucks->Num() > Dimensions.GetCellCount() * 3)
		return false;
	for (const auto& X : *Sites)
	{
		const auto V = X->AsObject();
		FCityWasteSite A;
		if (!GetId(V, TEXT("id"), A.EntityId) || !GetInt(V, TEXT("units"), A.Units) ||
		    !GetInt(V, TEXT("capacity"), A.Capacity) || !GetInt(V, TEXT("remainder"), A.GenerationRemainder) ||
		    !Get(V, TEXT("collectedAt"), A.LastCollectionStep))
			return false;
		S.Sites.Add(A);
	}
	for (const auto& X : *Depots)
	{
		const auto V = X->AsObject();
		FCityWasteDepot A;
		if (!GetId(V, TEXT("id"), A.EntityId) || !GetInt(V, TEXT("stored"), A.Stored) ||
		    !Get(V, TEXT("processed"), A.Processed) || !Get(V, TEXT("recycled"), A.Recycled))
			return false;
		S.Depots.Add(A);
	}
	for (const auto& X : *Trucks)
	{
		const auto V = X->AsObject();
		FCityWasteTruck A;
		const TArray<TSharedPtr<FJsonValue>>* Stops = nullptr;
		if (!GetId(V, TEXT("id"), A.Id) || !GetId(V, TEXT("depot"), A.DepotId) ||
		    !GetId(V, TEXT("current"), A.CurrentEntityId) || !GetId(V, TEXT("trip"), A.TripId) ||
		    !GetInt(V, TEXT("cargo"), A.Cargo) || !Get(V, TEXT("workedAt"), A.LastWorkStep) ||
		    !V->TryGetArrayField(TEXT("stops"), Stops) || Stops->Num() > FCityWasteTuning::MaxStops)
			return false;
		for (const auto& Stop : *Stops)
		{
			FString Id;
			if (!Stop->TryGetString(Id))
				return false;
			auto W = MakeShared<FJsonObject>();
			W->SetStringField(TEXT("id"), Id);
			uint64 N = 0;
			if (!GetId(W, TEXT("id"), N))
				return false;
			A.Stops.Add(N);
		}
		S.Trucks.Add(A);
	}
	if (!FCityWasteSimulation::ValidateState(S, MaximumStep, Error))
		return false;
	OutState = MoveTemp(S);
	Error.Reset();
	return true;
}
