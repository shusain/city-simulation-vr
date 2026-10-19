#include "Simulation/CityDisasterSimulation.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Commands/CityPlacementModel.h"
#include "Simulation/CityDevelopmentSimulation.h"
#include "Simulation/CityFireSimulation.h"
namespace
{
	double FootprintDistance(const FCityDisasterEvent& E, const FCityPlacedEntity& B)
	{
		const int32 Dx =
		    FMath::Max(B.Anchor.X - E.Center.X, FMath::Max(0, E.Center.X - (B.Anchor.X + B.FootprintSize.X - 1)));
		const int32 Dy =
		    FMath::Max(B.Anchor.Y - E.Center.Y, FMath::Max(0, E.Center.Y - (B.Anchor.Y + B.FootprintSize.Y - 1)));
		return Dx * Dx + Dy * Dy;
	}
	FVector2D Curve(const FCityDisasterEvent& E, int32 Index, double T)
	{
		const FVector2D A(E.Path[Index].X, E.Path[Index].Y), B(E.Path[Index + 1].X, E.Path[Index + 1].Y);
		const auto D = B - A;
		const auto N = FVector2D(-D.Y, D.X).GetSafeNormal();
		const double Side = ((static_cast<uint64>(E.Id) + Index * 17) % 3) - 1.0;
		const FVector2D Before = Index > 0 ? FVector2D(E.Path[Index - 1].X, E.Path[Index - 1].Y) : A - D;
		const FVector2D After = Index + 2 < E.Path.Num() ? FVector2D(E.Path[Index + 2].X, E.Path[Index + 2].Y) : B + D;
		const auto M0 = (B - Before) * .5, M1 = (After - A) * .5;
		const double T2 = T * T, T3 = T2 * T;
		const auto P = A * (2 * T3 - 3 * T2 + 1) + M0 * (T3 - 2 * T2 + T) + B * (-2 * T3 + 3 * T2) + M1 * (T3 - T2) +
		               N * ((Side == 0 ? .35 : Side * .35) * FMath::Square(FMath::Sin(PI * T)));
		return {FMath::Clamp(P.X, 0., static_cast<double>(E.Dimensions.GetWidth() - 1)),
		        FMath::Clamp(P.Y, 0., static_cast<double>(E.Dimensions.GetHeight() - 1))};
	}
	double SegmentLength(const FCityDisasterEvent& E, int32 Index)
	{
		double Length = 0;
		auto Prev = Curve(E, Index, 0);
		for (int32 S = 1; S <= 32; ++S)
		{
			const auto P = Curve(E, Index, S / 32.);
			Length += (P - Prev).Size();
			Prev = P;
		}
		return Length;
	}
	bool ValidCell(const FCityCellCoord& C, const FCityGridDimensions& Dimensions)
	{
		return C.X >= 0 && C.Y >= 0 && C.X < Dimensions.GetWidth() && C.Y < Dimensions.GetHeight();
	}
} // namespace
const FCityDisasterEvent* FCityDisasterSnapshot::Current() const
{
	return !Events.IsEmpty() && Events.Last().EndStep > LastFixedStepId ? &Events.Last() : nullptr;
}
FString FCityDisasterSnapshot::Describe() const
{
	const auto* E = Current();
	if (!E)
		return FString();
	if (E->Profile >= 2)
		return LastFixedStepId < E->StartStep
		           ? FString::Printf(TEXT("%s warning: %lld days\nSeverity %d%% | %lld days"),
		                             FCityDisasterSimulation::Name(E->Kind), E->StartStep - LastFixedStepId,
		                             E->SeverityPercent, E->EndStep - E->StartStep)
		           : FString::Printf(TEXT("%s: %d/%d destroyed\nInspect / Repair"),
		                             FCityDisasterSimulation::Name(E->Kind), E->Affected.Num(),
		                             E->PlannedTargets.Num());
	return LastFixedStepId < E->StartStep
	           ? FString::Printf(TEXT("%s warning: %lld days\nPause to prepare"),
	                             FCityDisasterSimulation::Name(E->Kind), E->StartStep - LastFixedStepId)
	           : FString::Printf(TEXT("%s active: %d damaged\nInspect / Repair"),
	                             FCityDisasterSimulation::Name(E->Kind), E->Affected.Num());
}
const TCHAR* FCityDisasterSimulation::Name(const ECityNaturalDisaster Kind)
{
	return Kind == ECityNaturalDisaster::Earthquake ? TEXT("Earthquake") : TEXT("Tornado");
}
int32 FCityDisasterSimulation::CheckInterval(const ECityDisasterFrequency Frequency, const int32 StepsPerMonth)
{
	return FMath::Clamp(StepsPerMonth, 1, 365) * (Frequency == ECityDisasterFrequency::Low    ? 24
	                                              : Frequency == ECityDisasterFrequency::High ? 6
	                                                                                          : 12);
}
void FCityDisasterSimulation::Reset(const int64 Step, const uint32 Seed, const FCityGridDimensions Dimensions)
{
	checkf(Dimensions.IsValid(), TEXT("Validate disaster dimensions before reset"));
	Snapshot = {};
	Snapshot.Dimensions = Dimensions;
	Snapshot.LastFixedStepId = Step;
	Snapshot.NextCheckStep = Step;
	Snapshot.RandomState = Seed ? Seed : 1;
}
uint32 FCityDisasterSimulation::NextRandom()
{
	Snapshot.RandomState = Snapshot.RandomState * 1664525u + 1013904223u;
	if (!Snapshot.RandomState)
		Snapshot.RandomState = 1;
	return Snapshot.RandomState;
}
void FCityDisasterSimulation::SetFrequency(const ECityDisasterFrequency Frequency, const int32 StepsPerMonth)
{
	Snapshot.NextCheckStep = Snapshot.LastFixedStepId + CheckInterval(Frequency, StepsPerMonth);
	// Cancel only a pending random warning. Committed consequences and manual tests survive Off.
	if (Frequency == ECityDisasterFrequency::Off && Snapshot.Current() && !Snapshot.Events.Last().bTest &&
	    Snapshot.LastFixedStepId < Snapshot.Events.Last().StartStep)
		Snapshot.Events.Pop();
}
void FCityDisasterSimulation::Begin(const ECityNaturalDisaster Kind, const FCityCellCoord& Center,
                                    const int32 Direction, const bool bTest)
{
	FCityDisasterEvent E;
	E.Dimensions = Snapshot.Dimensions;
	E.Id = ++Snapshot.TotalEvents;
	E.Kind = Kind;
	E.Center = Center;
	E.Direction = FMath::Clamp(Direction, 0, 3);
	E.Profile = 4;
	E.SeverityPercent = 5 + static_cast<int32>((NextRandom() >> 16) % 46);
	E.WarningStep = Snapshot.LastFixedStepId;
	E.StartStep = E.WarningStep + (bTest ? 1 : 0);
	E.EndStep =
	    E.StartStep + (Kind == ECityNaturalDisaster::Earthquake ? 10 + E.SeverityPercent : 20 + 2 * E.SeverityPercent);
	E.bTest = bTest;
	if (Snapshot.Events.Num() == MaximumEvents)
		Snapshot.Events.RemoveAt(0);
	Snapshot.Events.Add(E);
}
bool FCityDisasterSimulation::StartTest(const ECityNaturalDisaster Kind, const FCityCellCoord& Center,
                                        const int32 Direction)
{
	if (Snapshot.Current() || !ValidCell(Center, Snapshot.Dimensions) || static_cast<uint8>(Kind) < 1 ||
	    static_cast<uint8>(Kind) > 2)
		return false;
	Begin(Kind, Center, Direction, true);
	return true;
}
void FCityDisasterSimulation::PrepareEvent(const FCityPlacementModel& City, const TArray<uint64>& Targets)
{
	auto& E = Snapshot.Events.Last();
	E.InitialBuildings = Targets.Num();
	TArray<uint64> Ordered = Targets;
	Ordered.Sort(
	    [&](uint64 A, uint64 B)
	    {
		    const double DA = FootprintDistance(E, City.GetEntities().FindChecked(A)),
		                 DB = FootprintDistance(E, City.GetEntities().FindChecked(B));
		    return DA == DB ? A < B : DA < DB;
	    });
	const int32 Budget = FMath::DivideAndRoundUp(E.InitialBuildings * E.SeverityPercent, 100);
	if (E.Profile >= 3 && E.Kind == ECityNaturalDisaster::Earthquake)
	{
		E.RadiusSquared = static_cast<int32>(FootprintDistance(E, City.GetEntities().FindChecked(Ordered[Budget - 1])));
		Ordered.RemoveAll([&](uint64 Id)
		                  { return FootprintDistance(E, City.GetEntities().FindChecked(Id)) > E.RadiusSquared; });
	}
	else
		Ordered.SetNum(FMath::Min(Ordered.Num(), Budget));
	if (E.Kind == ECityNaturalDisaster::Tornado)
	{
		TArray<uint64> Route;
		FCityCellCoord Current = E.Center;
		const FIntPoint Wind[] = {{0, 1}, {1, 0}, {0, -1}, {-1, 0}};
		const auto W = Wind[E.Direction];
		while (!Ordered.IsEmpty())
		{
			int32 Best = 0;
			double BestScore = MAX_dbl;
			for (int32 Index = 0; Index < Ordered.Num(); ++Index)
			{
				const auto P = City.GetEntities().FindChecked(Ordered[Index]).Anchor;
				const int32 Dx = P.X - Current.X, Dy = P.Y - Current.Y;
				const double Score = Dx * Dx + Dy * Dy + FMath::Max(0, -(Dx * W.X + Dy * W.Y)) * .5;
				if (Score < BestScore)
				{
					BestScore = Score;
					Best = Index;
				}
			}
			const uint64 Id = Ordered[Best];
			Route.Add(Id);
			Current = City.GetEntities().FindChecked(Id).Anchor;
			Ordered.RemoveAt(Best);
		}
		Ordered = MoveTemp(Route);
	}
	for (const uint64 Id : Ordered)
	{
		E.PlannedTargets.Add(Id);
		E.Path.Add(City.GetEntities().FindChecked(Id).Anchor);
	}
	if (E.Profile >= 3 && E.Kind == ECityNaturalDisaster::Tornado)
	{
		E.TravelDistances.Add(0);
		for (int32 Index = 0; Index < E.Path.Num() - 1; ++Index)
			E.TravelDistances.Add(E.TravelDistances.Last() + SegmentLength(E, Index));
	}
}
FVector2D FCityDisasterSimulation::ContinuousPosition(const FCityDisasterEvent& E, const double Step)
{
	if (E.Profile < 3 || E.Kind == ECityNaturalDisaster::Earthquake || E.Path.IsEmpty())
		return FVector2D(E.Center.X, E.Center.Y);
	const double Progress = FMath::Clamp((Step - E.StartStep) / (E.EndStep - E.StartStep - 1), 0., 1.);
	if (E.Path.Num() == 1)
	{
		const auto C = E.Path[0];
		const double Angle = Progress * 2 * PI;
		return FVector2D(
		    FMath::Clamp(C.X + FMath::Sin(Angle) * 1.5, 0., static_cast<double>(E.Dimensions.GetWidth() - 1)),
		    FMath::Clamp(C.Y + FMath::Sin(Angle * 2) * .5, 0., static_cast<double>(E.Dimensions.GetHeight() - 1)));
	}
	const double Distance = Progress * E.TravelDistances.Last();
	int32 Low = 0, High = E.Path.Num() - 2;
	while (Low < High)
	{
		const int32 Mid = (Low + High) / 2;
		if (E.TravelDistances[Mid + 1] < Distance)
			Low = Mid + 1;
		else
			High = Mid;
	}
	const double Local = Distance - E.TravelDistances[Low];
	double Walked = 0;
	auto Prev = Curve(E, Low, 0);
	for (int32 S = 1; S <= 32; ++S)
	{
		const auto P = Curve(E, Low, S / 32.);
		const double Length = (P - Prev).Size();
		if (Walked + Length >= Local)
			return Curve(E, Low, (S - 1 + FMath::Clamp((Local - Walked) / FMath::Max(.000001, Length), 0., 1.)) / 32.);
		Walked += Length;
		Prev = P;
	}
	return Curve(E, Low, 1);
}

FCityCellCoord FCityDisasterSimulation::Position(const FCityDisasterEvent& E, const double Step)
{
	if (E.Profile == 2 && !E.Path.IsEmpty())
	{
		const int64 Elapsed =
		    FMath::Clamp<int64>(static_cast<int64>(Step) - E.StartStep, 0, E.EndStep - E.StartStep - 1);
		return E.Path[FMath::Min<int64>(E.Path.Num() - 1, Elapsed * E.Path.Num() / (E.EndStep - E.StartStep))];
	}
	if (E.Profile >= 3)
	{
		const auto P = ContinuousPosition(E, Step);
		return {static_cast<int32>(FMath::RoundToInt(P.X)), static_cast<int32>(FMath::RoundToInt(P.Y))};
	}
	const FIntPoint Directions[] = {{0, 1}, {1, 0}, {0, -1}, {-1, 0}};
	const int32 Distance =
	    E.Kind == ECityNaturalDisaster::Tornado ? FMath::Clamp(static_cast<int32>(Step - E.StartStep), 0, 11) : 0;
	const auto D = Directions[FMath::Clamp(E.Direction, 0, 3)];
	return {FMath::Clamp(E.Center.X + D.X * Distance, 0, E.Dimensions.GetWidth() - 1),
	        FMath::Clamp(E.Center.Y + D.Y * Distance, 0, E.Dimensions.GetHeight() - 1)};
}
void FCityDisasterSimulation::AdvanceToStep(const int64 Step, const FCityPlacementModel& City,
                                            const FCityDevelopmentSnapshot& Development, FCityFireSimulation& Damage,
                                            const FCityIdentity& Identity, const int32 Residents,
                                            const int32 StepsPerMonth, const int32 WindDirection)
{
	checkf(Snapshot.Dimensions == City.GetGrid().GetDimensions(), TEXT("Disaster and city dimensions must match"));
	while (Snapshot.LastFixedStepId < Step)
	{
		++Snapshot.LastFixedStepId;
		TArray<uint64> Ids;
		City.GetEntities().GetKeys(Ids);
		Ids.Sort();
		TArray<uint64> Targets, AllTargets, TornadoTargets;
		for (const uint64 Id : Ids)
		{
			const auto& E = City.GetEntities().FindChecked(Id);
			if (Development.IsOperational(Id) && !Damage.GetSnapshot().IsDisabled(Id))
				AllTargets.Add(Id);
			const bool bUrban =
			    E.DefinitionId == TEXT("Home") || E.DefinitionId == TEXT("Shop") || E.DefinitionId == TEXT("Factory");
			if (Development.IsOperational(Id) && !Damage.GetSnapshot().IsDisabled(Id))
			{
				if (bUrban)
					Targets.Add(Id);
				if (bUrban || FCityBuildingDefinition::IsPowerFacility(E.DefinitionId) ||
				    FCityBuildingDefinition::IsWaterFacility(E.DefinitionId))
					TornadoTargets.Add(Id);
			}
		}
		if (!Snapshot.Current() && Identity.DisasterFrequency != ECityDisasterFrequency::Off &&
		    Snapshot.LastFixedStepId >= Snapshot.NextCheckStep)
		{
			Snapshot.NextCheckStep =
			    Snapshot.LastFixedStepId + CheckInterval(Identity.DisasterFrequency, StepsPerMonth);
			const int32 Threshold = Identity.Difficulty == ECityDifficulty::Relaxed       ? 300
			                        : Identity.Difficulty == ECityDifficulty::Challenging ? 150
			                                                                              : 200;
			// Establishment grace and a probability per scheduled check, never a guaranteed event.
			if (Residents >= Threshold &&
			    Snapshot.LastFixedStepId >= static_cast<int64>(FMath::Clamp(StepsPerMonth, 1, 365)) * 12 &&
			    !Targets.IsEmpty() && (NextRandom() >> 16) % 100 < 25)
			{
				const uint64 Target = Targets[(NextRandom() >> 16) % Targets.Num()];
				const auto Kind =
				    (NextRandom() >> 16) & 1 ? ECityNaturalDisaster::Earthquake : ECityNaturalDisaster::Tornado;
				const FCityCellCoord Center =
				    Kind == ECityNaturalDisaster::Earthquake
				        ? FCityCellCoord{static_cast<int32>((NextRandom() >> 16) % Snapshot.Dimensions.GetWidth()),
				                         static_cast<int32>((NextRandom() >> 16) % Snapshot.Dimensions.GetHeight())}
				        : City.GetEntities().FindChecked(Target).Anchor;
				Begin(Kind, Center, WindDirection, false);
			}
		}
		if (Snapshot.Current() && Snapshot.Events.Last().Profile >= 2 && Snapshot.Events.Last().InitialBuildings == 0)
		{
			const auto& Event = Snapshot.Events.Last();
			const auto& Eligible = Event.Profile >= 3 && Event.Kind == ECityNaturalDisaster::Earthquake ? AllTargets
			                       : Event.Profile >= 4 && Event.Kind == ECityNaturalDisaster::Tornado  ? TornadoTargets
			                                                                                            : Targets;
			if (!Eligible.IsEmpty())
				PrepareEvent(City, Eligible);
		}
		if (!Snapshot.Current() || Snapshot.LastFixedStepId < Snapshot.Events.Last().StartStep)
			continue;
		auto& Planned = Snapshot.Events.Last();
		if (Planned.Profile >= 3)
		{
			if (Planned.Kind == ECityNaturalDisaster::Earthquake)
			{
				if (Snapshot.LastFixedStepId == Planned.StartStep)
				{
					Planned.PlannedTargets.Reset();
					Planned.Path.Reset();
					Planned.InitialBuildings = AllTargets.Num();
					for (uint64 Id : AllTargets)
						if (FootprintDistance(Planned, City.GetEntities().FindChecked(Id)) <= Planned.RadiusSquared)
						{
							Planned.PlannedTargets.Add(Id);
							Planned.Path.Add(City.GetEntities().FindChecked(Id).Anchor);
							if (Damage.ApplyNaturalDamage(Id, 1, 100))
								Planned.Affected.Add(Id);
						}
				}
			}
			else
			{
				const double Progress = FMath::Clamp(static_cast<double>(Snapshot.LastFixedStepId - Planned.StartStep) /
				                                         (Planned.EndStep - Planned.StartStep - 1),
				                                     0., 1.);
				const double Previous = static_cast<double>(Snapshot.LastFixedStepId - Planned.StartStep - 1) /
				                        (Planned.EndStep - Planned.StartStep - 1);
				const double Length = Planned.TravelDistances.IsEmpty() ? 0 : Planned.TravelDistances.Last();
				for (int32 Index = 0; Index < Planned.PlannedTargets.Num(); ++Index)
					if ((Index == 0 ? Snapshot.LastFixedStepId == Planned.StartStep
					                : Planned.TravelDistances[Index] > Previous * Length &&
					                      Planned.TravelDistances[Index] <= Progress * Length))
					{
						const uint64 Id = Planned.PlannedTargets[Index];
						if (City.GetEntities().Contains(Id) && !Planned.Affected.Contains(Id) &&
						    Damage.ApplyNaturalDamage(Id, 2, 100))
							Planned.Affected.Add(Id);
					}
			}
			continue;
		}
		if (Planned.Profile == 2)
		{
			const int64 Elapsed = Snapshot.LastFixedStepId - Planned.StartStep;
			const int32 First = static_cast<int32>(
			    FMath::DivideAndRoundUp(Elapsed * Planned.PlannedTargets.Num(), Planned.EndStep - Planned.StartStep));
			const int32 Last = static_cast<int32>(FMath::DivideAndRoundUp((Elapsed + 1) * Planned.PlannedTargets.Num(),
			                                                              Planned.EndStep - Planned.StartStep));
			for (int32 Index = First; Index < Last; ++Index)
			{
				const uint64 Id = Planned.PlannedTargets[Index];
				if (City.GetEntities().Contains(Id) && !Planned.Affected.Contains(Id) &&
				    Damage.ApplyNaturalDamage(Id, Planned.Kind == ECityNaturalDisaster::Earthquake ? 1 : 2, 100))
					Planned.Affected.Add(Id);
			}
			continue;
		}
		auto& E = Snapshot.Events.Last();
		if (E.Kind == ECityNaturalDisaster::Earthquake && Snapshot.LastFixedStepId != E.StartStep)
			continue;
		const auto Center = Position(E, Snapshot.LastFixedStepId);
		for (const uint64 Id : Targets)
		{
			if (E.Affected.Num() >= 3)
				break;
			if (E.Affected.Contains(Id))
				continue;
			const auto& B = City.GetEntities().FindChecked(Id);
			const int32 Dx =
			    FMath::Max(B.Anchor.X - Center.X, FMath::Max(0, Center.X - (B.Anchor.X + B.FootprintSize.X - 1)));
			const int32 Dy =
			    FMath::Max(B.Anchor.Y - Center.Y, FMath::Max(0, Center.Y - (B.Anchor.Y + B.FootprintSize.Y - 1)));
			if (Dx * Dx + Dy * Dy > (E.Kind == ECityNaturalDisaster::Earthquake ? 9 : 1))
				continue;
			if (Damage.ApplyNaturalDamage(Id, E.Kind == ECityNaturalDisaster::Earthquake ? 1 : 2, 40))
				E.Affected.Add(Id);
		}
	}
}
bool FCityDisasterSimulation::ValidateState(const FCityDisasterSnapshot& S, const int64 MaximumStep, FString& Error)
{
	if (!S.Dimensions.IsValid() || S.LastFixedStepId < 0 || S.LastFixedStepId > MaximumStep ||
	    S.LastFixedStepId > MAX_int64 - 200000 || S.NextCheckStep < 0 || S.NextCheckStep > S.LastFixedStepId + 100000 ||
	    !S.RandomState || S.TotalEvents < 0 || S.TotalEvents > MAX_int64 - 100000 || S.Events.Num() > MaximumEvents)
	{
		Error = TEXT("Invalid natural disaster clock/cap");
		return false;
	}
	int64 Prior = 0;
	for (const auto& E : S.Events)
	{
		if (E.Dimensions != S.Dimensions || E.Profile < 1 || E.Profile > 4 ||
		    (E.Profile == 1 &&
		     (E.SeverityPercent || E.InitialBuildings || !E.Path.IsEmpty() || !E.PlannedTargets.IsEmpty())) ||
		    (E.Profile >= 2 &&
		     (E.SeverityPercent < 5 || E.SeverityPercent > 50 || E.InitialBuildings < 0 ||
		      E.InitialBuildings > S.Dimensions.GetCellCount() || E.Path.Num() != E.PlannedTargets.Num() ||
		      (E.Profile == 2 || E.Kind == ECityNaturalDisaster::Tornado
		           ? E.Path.Num() != FMath::DivideAndRoundUp(E.InitialBuildings * E.SeverityPercent, 100)
		           : E.Path.Num() > E.InitialBuildings))))
		{
			Error = TEXT("Invalid disaster severity/plan");
			return false;
		}
		if (E.RadiusSquared < 0 ||
		    E.RadiusSquared > 2 * (S.Dimensions.GetWidth() - 1) * (S.Dimensions.GetHeight() - 1) ||
		    (E.Profile < 3 && (E.RadiusSquared || !E.TravelDistances.IsEmpty())) ||
		    (E.Profile >= 3 && (E.Kind == ECityNaturalDisaster::Earthquake ? !E.TravelDistances.IsEmpty()
		                                                                   : E.TravelDistances.Num() != E.Path.Num())))
		{
			Error = TEXT("Invalid disaster radius/travel");
			return false;
		}
		for (int32 Index = 0; Index < E.TravelDistances.Num(); ++Index)
			if (!FMath::IsFinite(E.TravelDistances[Index]) ||
			    (Index == 0 ? E.TravelDistances[Index] != 0
			                : !FMath::IsNearlyEqual(E.TravelDistances[Index] - E.TravelDistances[Index - 1],
			                                        SegmentLength(E, Index - 1), .00001)))
			{
				Error = TEXT("Invalid disaster travel distance");
				return false;
			}
		TSet<uint64> PlannedIds;
		for (int32 Index = 0; Index < E.Path.Num(); ++Index)
		{
			if (!ValidCell(E.Path[Index], S.Dimensions) || !E.PlannedTargets[Index] ||
			    PlannedIds.Contains(E.PlannedTargets[Index]))
			{
				Error = TEXT("Invalid disaster path/target");
				return false;
			}
			PlannedIds.Add(E.PlannedTargets[Index]);
		}
		TSet<uint64> Unique;
		for (const uint64 Id : E.Affected)
		{
			if (!Id || Unique.Contains(Id) || (E.Profile >= 2 && !PlannedIds.Contains(Id)))
			{
				Error = TEXT("Invalid disaster affected identity");
				return false;
			}
			Unique.Add(Id);
		}
		if (E.Id <= Prior || E.Id > S.TotalEvents || static_cast<uint8>(E.Kind) < 1 || static_cast<uint8>(E.Kind) > 2 ||
		    !ValidCell(E.Center, S.Dimensions) || E.Direction < 0 || E.Direction > 3 || E.WarningStep < 0 ||
		    E.WarningStep > S.LastFixedStepId ||
		    E.StartStep != E.WarningStep + (E.Profile >= 4 ? (E.bTest ? 1 : 0) : 5) ||
		    E.EndStep != E.StartStep + (E.Profile == 1 ? (E.Kind == ECityNaturalDisaster::Earthquake ? 6 : 12)
		                                               : (E.Kind == ECityNaturalDisaster::Earthquake
		                                                      ? 10 + E.SeverityPercent
		                                                      : 20 + 2 * E.SeverityPercent)) ||
		    E.Affected.Num() > (E.Profile == 1 ? 3 : E.PlannedTargets.Num()) ||
		    (S.LastFixedStepId < E.StartStep && !E.Affected.IsEmpty()) ||
		    (Prior && S.Events[&E - S.Events.GetData() - 1].EndStep > E.WarningStep))
		{
			Error = TEXT("Invalid natural disaster event");
			return false;
		}
		Prior = E.Id;
	}
	return true;
}
bool FCityDisasterSimulation::RestoreState(const FCityDisasterSnapshot& S, const int64 MaximumStep, FString& Error)
{
	if (!ValidateState(S, MaximumStep, Error))
		return false;
	Snapshot = S;
	return true;
}
