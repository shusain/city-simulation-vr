#include "Commands/CityPlacementModel.h"
#include "Simulation/CityTerrain.h"
#include "Simulation/CityRoadNetwork.h"

namespace
{
	constexpr int32 ClearanceCm = 480;
	constexpr int32 MaximumRisePerCellCm = 200;
	int64 BaseCost(const FName Id)
	{
		if (Id == TEXT("DirtRoad"))
			return FCityRoadDefinition::Dirt().PlacementCostPerCell;
		if (Id == TEXT("Avenue"))
			return FCityRoadDefinition::Avenue().PlacementCostPerCell;
		if (Id == TEXT("Highway"))
			return FCityRoadDefinition::Highway().PlacementCostPerCell;
		return FCityRoadDefinition::Local().PlacementCostPerCell;
	}
	int32 Distance(const FCityRoadStructure& S)
	{
		return FMath::Abs(S.End.X - S.Start.X) + FMath::Abs(S.End.Y - S.Start.Y);
	}
	bool Contains(const FCityRoadStructure& S, const FCityCellCoord& C)
	{
		return C.X >= FMath::Min(S.Start.X, S.End.X) && C.X <= FMath::Max(S.Start.X, S.End.X) &&
		       C.Y >= FMath::Min(S.Start.Y, S.End.Y) && C.Y <= FMath::Max(S.Start.Y, S.End.Y);
	}
} // namespace

const FCityRoadStructure* FCityPlacementModel::FindRoadStructure(const FCityCellCoord& Cell,
                                                                 const bool bIncludeApproaches) const
{
	for (const auto& Span : RoadStructures)
	{
		if (!Contains(Span, Cell))
			continue;
		const bool EndpointXY =
		    (Cell.X == Span.Start.X && Cell.Y == Span.Start.Y) || (Cell.X == Span.End.X && Cell.Y == Span.End.Y);
		const bool AddressBelongs =
		    EndpointXY ? (Cell.X == Span.Start.X && Cell.Y == Span.Start.Y ? Cell.RoadLevel == Span.Start.RoadLevel
		                                                                   : Cell.RoadLevel == Span.End.RoadLevel)
		               : Cell.RoadLevel == 1;
		if (AddressBelongs && (bIncludeApproaches || !EndpointXY || Cell.RoadLevel == 1))
			return &Span;
	}
	return nullptr;
}

const FCityRoadStructure* FCityPlacementModel::FindRoadCorridor(const FCityCellCoord& Cell) const
{
	for (const auto& Span : RoadStructures)
		if (Contains(Span, Cell))
			return &Span;
	return nullptr;
}

void FCityPlacementModel::GetConnectedRoadNeighbors(const FCityCellCoord& Cell, TArray<FCityCellCoord>& Out) const
{
	Out.Reset();
	for (const auto Offset : {FIntPoint(0, 1), FIntPoint(1, 0), FIntPoint(0, -1), FIntPoint(-1, 0)})
		for (int32 Level = 0; Level <= 1; ++Level)
		{
			const FCityCellCoord Neighbor(Cell.X + Offset.X, Cell.Y + Offset.Y, Level);
			if (CanRoadsConnect(Cell, Neighbor))
				Out.Add(Neighbor);
		}
}

FCityCellCoord FCityPlacementModel::GetRoadNeighbor(const FCityCellCoord& Cell, const int32 DX, const int32 DY) const
{
	for (int32 Level = 0; Level <= 1; ++Level)
	{
		FCityCellCoord N(Cell.X + DX, Cell.Y + DY, Level);
		if (CanRoadsConnect(Cell, N))
			return N;
	}
	return {Cell.X + DX, Cell.Y + DY, Cell.RoadLevel};
}

FName FCityPlacementModel::ResolveRaisedRoadClass(const FCityCellCoord& Cell) const
{
	FName Result = NAME_None;
	for (const auto& S : RoadStructures)
		if (Cell.RoadLevel == 1 && Contains(S, Cell) &&
		    !(Cell.X == S.Start.X && Cell.Y == S.Start.Y && S.Start.RoadLevel == 0) &&
		    !(Cell.X == S.End.X && Cell.Y == S.End.Y && S.End.RoadLevel == 0))
			if (Result.IsNone() || BaseCost(S.DefinitionId) > BaseCost(Result))
				Result = S.DefinitionId;
	return Result;
}

void FCityPlacementModel::RebuildRaisedRoadClasses()
{
	SpatialCache.Invalidate();
	for (const auto& Cell : RoadCells)
		if (Cell.RoadLevel == 1)
			RoadDefinitionIds.Add(Cell, ResolveRaisedRoadClass(Cell));
}

bool FCityPlacementModel::CanRaisedSpansIntersect(const FCityRoadStructure& A, const FCityRoadStructure& B,
                                                  const FCityCellCoord& Cell, FString& Error) const
{
	const auto Flat = [&](const FCityRoadStructure& S)
	{
		const int32 Along = FMath::Abs(Cell.X - S.Start.X) + FMath::Abs(Cell.Y - S.Start.Y);
		return Contains(S, Cell) && (S.Start.RoadLevel == 1 || Along > S.StartRampCells) &&
		       (S.End.RoadLevel == 1 || Along < Distance(S) - S.EndRampCells);
	};
	const bool BothEndpoints = (Cell == A.Start || Cell == A.End) && (Cell == B.Start || Cell == B.End);
	if (Cell.RoadLevel != 1 || ((A.Start.Y == A.End.Y) == (B.Start.Y == B.End.Y) && !BothEndpoints) || !Flat(A) ||
	    !Flat(B))
	{
		Error = TEXT("Raised crossings need perpendicular flat deck tiles; ramps cannot intersect");
		return false;
	}
	if (A.DeckHeightCm != B.DeckHeightCm)
	{
		Error = TEXT("Raised crossings need the same deck height; adjust height or approaches");
		return false;
	}
	return true;
}

bool FCityPlacementModel::CanGroundCrossSpan(const FCityRoadStructure& Span, const FCityCellCoord& Cell,
                                             FString& Error) const
{
	const int32 Along = FMath::Abs(Cell.X - Span.Start.X) + FMath::Abs(Cell.Y - Span.Start.Y);
	// The entire crossing cell must be plateau. Alternate interior cells contain
	// piers. Clearance includes road surfaces, tallest vehicle and a safety gap.
	if (Cell.RoadLevel != 0 || Along <= Span.StartRampCells || Along >= Distance(Span) - Span.EndRampCells ||
	    Along % 2 == 1)
	{
		Error = TEXT("Underpass needs a clear deck tile: ramps and support tiles are blocked");
		return false;
	}
	constexpr int32 MinimumVehicleEnvelopeCm = 67 + 320 + 40 + 23; // Surface, vehicle, safety gap, deck underside.
	if (Terrain && (Terrain->IsWaterCell(Cell) ||
	                Span.DeckHeightCm - Terrain->GetCellMaxHeightCm(Cell) < MinimumVehicleEnvelopeCm))
	{
		Error = TEXT("Underpass needs dry ground and 4.5 m vehicle clearance");
		return false;
	}
	const bool SpanX = Span.Start.Y == Span.End.Y;
	// Reject parallel branches. Placement requires a complete perpendicular
	// crossing; later demolition may leave a perpendicular stub.
	const uint8 Mask = GetRoadNeighborMask(Cell);
	if (Mask & (SpanX ? 10 : 5))
	{
		Error = TEXT("Ground underpass must cross straight and perpendicular to the span");
		return false;
	}
	return true;
}

ECityRoadProfile FCityPlacementModel::ResolveRoadDrawingProfile(const FCityCellCoord& Start, const FCityCellCoord& End,
                                                                const bool bRaised) const
{
	// Treatment is resolved from authority before validation; invalid water spans
	// still receive the bridge's approach/clearance explanation and cannot commit.
	TArray<FCityCellCoord> Path;
	if (Grid.IsValidCell(Start) && Grid.IsValidCell(End) && Terrain)
	{
		BuildOrthogonalRoadPath(Start, End, Path);
		for (const auto& Cell : Path)
			if (Terrain->IsWaterCell(Cell))
				return ECityRoadProfile::Bridge;
	}
	return bRaised || Start.RoadLevel == 1 || End.RoadLevel == 1 ? ECityRoadProfile::Elevated
	                                                             : ECityRoadProfile::Ground;
}

bool FCityPlacementModel::HasRoadSupport(const FCityCellCoord& Cell) const
{
	if (!IsRoad(Cell))
		return false;
	bool Raised = false;
	for (const auto& Span : RoadStructures)
		if (Cell.RoadLevel == 1 && Contains(Span, Cell))
		{
			Raised = true;
			if ((FMath::Abs(Cell.X - Span.Start.X) + FMath::Abs(Cell.Y - Span.Start.Y)) % 2 == 1)
				return true;
		}
	if (Raised)
		return false;
	const auto Class = EvaluateRoadTerrain(Cell).TerrainClass;
	return Class == ECityRoadTerrainClass::Bridge || Class == ECityRoadTerrainClass::Supported;
}

bool FCityPlacementModel::CanRoadsConnect(const FCityCellCoord& A, const FCityCellCoord& B) const
{
	if (!IsRoad(A) || !IsRoad(B) || FMath::Abs(A.X - B.X) + FMath::Abs(A.Y - B.Y) != 1)
		return false;
	const auto* SA = FindRoadStructure(A, false);
	const auto* SB = FindRoadStructure(B, false);
	// Raised corridors connect along their own span only. Adjacent ground roads
	// cannot silently become at-grade intersections or building entrances.
	if (!SA && !SB)
		return A.RoadLevel == 0 && B.RoadLevel == 0;
	if (SA && SB)
	{
		// A shared junction belongs to both spans. Keep parallel adjacent decks separate.
		for (const auto& S : RoadStructures)
			if (Contains(S, A) && Contains(S, B))
				return true;
		return false;
	}
	const auto* Span = SA ? SA : SB;
	const auto& Ground = SA ? B : A;
	return Ground.RoadLevel == 0 && (Ground == Span->Start || Ground == Span->End);
}

double FCityPlacementModel::SampleStructureHeight(const FCityRoadStructure& S, const double X, const double Y) const
{
	const int32 Length = Distance(S) - 1;
	const double Along = -.5 + (S.Start.Y == S.End.Y ? (X - S.Start.X - .5) * (S.End.X > S.Start.X ? 1 : -1)
	                                                 : (Y - S.Start.Y - .5) * (S.End.Y > S.Start.Y ? 1 : -1));
	if (S.Start.RoadLevel == 1 && Along <= 0)
		return S.DeckHeightCm;
	if (S.End.RoadLevel == 1 && Along >= Length)
		return S.DeckHeightCm;
	if (Terrain && ((S.Start.RoadLevel == 0 && Along <= 0) || (S.End.RoadLevel == 0 && Along >= Length)))
		return Terrain->SampleHeightCm(X, Y);
	const double H = S.StartRampCells > 0 && Along < S.StartRampCells
	                     ? FMath::Lerp(static_cast<double>(S.StartHeightCm), static_cast<double>(S.DeckHeightCm),
	                                   FMath::Clamp(Along / S.StartRampCells, 0., 1.))
	                 : S.EndRampCells > 0 && Along > Length - S.EndRampCells
	                     ? FMath::Lerp(static_cast<double>(S.EndHeightCm), static_cast<double>(S.DeckHeightCm),
	                                   FMath::Clamp((Length - Along) / S.EndRampCells, 0., 1.))
	                     : S.DeckHeightCm;
	if (!Terrain)
		return H;
	// Fade transverse slope into the first/last ramp cell. Endpoint center and
	// outward half remain on terrain, including every ground boundary vertex.
	const double Blend = FMath::Clamp(
	    FMath::Min(S.Start.RoadLevel == 1 ? 1. : Along, S.End.RoadLevel == 1 ? 1. : Length - Along), 0., 1.);
	const double Ground = Terrain->SampleHeightCm(X, Y);
	const double Center =
	    Terrain->SampleHeightCm(S.Start.Y == S.End.Y ? X : S.Start.X + .5, S.Start.Y == S.End.Y ? S.Start.Y + .5 : Y);
	return H + (Ground - Center) * (1 - Blend);
}

bool FCityPlacementModel::TrySampleStructureHeight(const double X, const double Y, double& OutHeight) const
{
	const FCityCellCoord Cell(FMath::Clamp(FMath::FloorToInt(X), 0, Grid.GetDimensions().GetWidth() - 1),
	                          FMath::Clamp(FMath::FloorToInt(Y), 0, Grid.GetDimensions().GetHeight() - 1));
	// Neighbor search gives identical values on exact cell boundaries.
	for (const auto& S : RoadStructures)
		if (Contains(S, Cell) ||
		    (FMath::IsNearlyEqual(X, FMath::RoundToDouble(X), .00001) && Contains(S, {Cell.X - 1, Cell.Y})) ||
		    (FMath::IsNearlyEqual(Y, FMath::RoundToDouble(Y), .00001) && Contains(S, {Cell.X, Cell.Y - 1})))
		{
			OutHeight = SampleStructureHeight(S, X, Y);
			return true;
		}
	return false;
}

bool FCityPlacementModel::MakeRoadStructure(const FCityRoadPreview& P, FCityRoadStructure& Out, FString& Error,
                                            const bool bSnapDeck) const
{
	if (!Terrain || P.Start.RoadLevel > 1 || P.End.RoadLevel > 1 || !Grid.IsValidCell(P.Start) ||
	    !Grid.IsValidCell(P.End) || (P.Start.X != P.End.X && P.Start.Y != P.End.Y))
	{
		Error = TEXT("Bridge/elevated roads need a straight span inside the city");
		return false;
	}
	const int32 Length = FMath::Abs(P.End.X - P.Start.X) + FMath::Abs(P.End.Y - P.Start.Y);
	if (P.ElevationStep < 0 || P.ElevationStep > MaximumRoadElevationStep ||
	    Length < (P.Start.RoadLevel == 0 && P.End.RoadLevel == 0 ? 6 : 2) || Length > 63 ||
	    (P.Profile != ECityRoadProfile::Bridge && P.Profile != ECityRoadProfile::Elevated) ||
	    P.PlacementCostPerCell != BaseCost(P.DefinitionId))
	{
		Error = TEXT("Use a straight span of at least 7 cells to leave room for ramps");
		return false;
	}
	if ((P.Start.RoadLevel == 0 &&
	     (Terrain->IsWaterCell(P.Start) || Terrain->GetCellMaxHeightCm(P.Start) - Terrain->GetCellMinHeightCm(P.Start) >
	                                           MaximumSupportedRoadReliefCm)) ||
	    (P.End.RoadLevel == 0 &&
	     (Terrain->IsWaterCell(P.End) ||
	      Terrain->GetCellMaxHeightCm(P.End) - Terrain->GetCellMinHeightCm(P.End) > MaximumSupportedRoadReliefCm)))
	{
		Error = TEXT("Both approaches need dry, buildable road grades");
		return false;
	}
	Out.DefinitionId = P.DefinitionId;
	Out.Start = P.Start;
	Out.End = P.End;
	Out.Profile = P.Profile;
	Out.ElevationStep = P.ElevationStep;
	const int32 DX = FMath::Sign(P.End.X - P.Start.X), DY = FMath::Sign(P.End.Y - P.Start.Y);
	Out.StartHeightCm = FMath::RoundToInt(Terrain->SampleHeightCm(P.Start.X + .5 + DX * .5, P.Start.Y + .5 + DY * .5));
	Out.EndHeightCm = FMath::RoundToInt(Terrain->SampleHeightCm(P.End.X + .5 - DX * .5, P.End.Y + .5 - DY * .5));
	int32 Top = FMath::Max(Out.StartHeightCm, Out.EndHeightCm);
	bool bWater = false;
	TArray<FCityCellCoord> Path;
	BuildOrthogonalRoadPath(P.Start, P.End, Path);
	for (int32 I = 0; I < Path.Num(); ++I)
	{
		bWater |= Terrain->IsWaterCell(Path[I]);
		Top = FMath::Max(Top, Terrain->IsWaterCell(Path[I]) ? Terrain->GetWaterSurfaceHeightCm(Path[I])
		                                                    : Terrain->GetCellMaxHeightCm(Path[I]));
	}
	if ((P.Profile == ECityRoadProfile::Bridge && !bWater) || (P.Profile == ECityRoadProfile::Elevated && bWater))
	{
		Error = P.Profile == ECityRoadProfile::Bridge ? TEXT("Bridge must cross water; choose Elevated for dry terrain")
		                                              : TEXT("Elevated roads need dry land; choose Bridge for water");
		return false;
	}
	// Round upward to absolute 2 m levels, preserving minimum terrain/water clearance.
	const int32 MinimumDeck = Top + ClearanceCm;
	Out.DeckHeightCm =
	    (bSnapDeck ? FMath::CeilToInt(static_cast<double>(MinimumDeck) / RoadElevationStepCm) * RoadElevationStepCm
	               : MinimumDeck) +
	    P.ElevationStep * RoadElevationStepCm;
	if (P.Start.RoadLevel == 1)
		Out.StartHeightCm = Out.DeckHeightCm;
	if (P.End.RoadLevel == 1)
		Out.EndHeightCm = Out.DeckHeightCm;
	Out.StartRampCells = FMath::DivideAndRoundUp(Out.DeckHeightCm - Out.StartHeightCm, MaximumRisePerCellCm);
	Out.EndRampCells = FMath::DivideAndRoundUp(Out.DeckHeightCm - Out.EndHeightCm, MaximumRisePerCellCm);
	if (Out.StartRampCells + Out.EndRampCells > Length - 2)
	{
		Error = TEXT("Extend the span: approaches need longer ramps for a safe grade");
		return false;
	}
	for (int32 I = 0; I < Path.Num(); ++I)
	{
		const auto C = Path[I];
		for (int32 U = 0; U < 4; ++U)
		{
			const bool AlongX = P.Start.Y == P.End.Y;
			const double X = C.X + (AlongX ? U * .25 : .5), Y = C.Y + (AlongX ? .5 : U * .25);
			const double A = SampleStructureHeight(Out, X, Y),
			             B = SampleStructureHeight(Out, X + (AlongX ? .25 : 0), Y + (AlongX ? 0 : .25));
			if (FMath::Abs(B - A) > MaximumRisePerCellCm * .25 + 1)
			{
				Error = TEXT("Approach grade is too steep; choose gentler endpoints");
				return false;
			}
		}
		// Match the rendering subdivision and include both pavement/sidewalk edges.
		for (int32 U = 0; U <= 4; ++U)
			for (int32 V = 0; V <= 4; ++V)
			{
				const double X = C.X + U * .25, Y = C.Y + V * .25;
				const double H = SampleStructureHeight(Out, X, Y);
				if (H + 1 < Terrain->SampleHeightCm(X, Y))
				{
					Error = TEXT("Ramp would cut through terrain; choose gentler approaches or a longer span");
					return false;
				}
				if (Terrain->IsWaterCell(C) && H < Terrain->GetWaterSurfaceHeightCm(C) + 400)
				{
					Error = TEXT("Water crossing needs 4 m clearance; extend ramps onto dry land");
					return false;
				}
			}
	}
	Out.RecordedCost = (Path.Num() - 2 + P.Start.RoadLevel + P.End.RoadLevel) *
	                   (BaseCost(P.DefinitionId) + (P.Profile == ECityRoadProfile::Bridge ? BridgeSurcharge : 40) +
	                    P.ElevationStep * 20);
	return true;
}

bool FCityPlacementModel::ValidateRoadStructures(FString& Error) const
{
	TMap<FCityCellCoord, const FCityRoadStructure*> Seen;
	if (RoadStructures.Num() > Grid.GetDimensions().GetCellCount() / 3)
	{
		Error = TEXT("Too many road structures");
		return false;
	}
	for (const auto& S : RoadStructures)
	{
		const int32 Length = Distance(S);
		if ((S.DefinitionId != TEXT("DirtRoad") && S.DefinitionId != TEXT("LocalRoad") &&
		     S.DefinitionId != TEXT("Avenue") && S.DefinitionId != TEXT("Highway")) ||
		    S.Start.RoadLevel > 1 || S.End.RoadLevel > 1 || !Grid.IsValidCell(S.Start) || !Grid.IsValidCell(S.End) ||
		    (S.Start.X != S.End.X && S.Start.Y != S.End.Y) || S.ElevationStep < 0 ||
		    S.ElevationStep > MaximumRoadElevationStep ||
		    Length < (S.Start.RoadLevel == 0 && S.End.RoadLevel == 0 ? 6 : 2) ||
		    S.StartRampCells < (S.Start.RoadLevel == 0 ? 1 : 0) || S.EndRampCells < (S.End.RoadLevel == 0 ? 1 : 0) ||
		    S.StartRampCells + S.EndRampCells > Length - 2 || S.RecordedCost < 0 ||
		    (S.Start.RoadLevel == 0 ? S.DeckHeightCm <= S.StartHeightCm
		                            : S.StartHeightCm != S.DeckHeightCm || S.StartRampCells != 0) ||
		    (S.End.RoadLevel == 0 ? S.DeckHeightCm <= S.EndHeightCm
		                          : S.EndHeightCm != S.DeckHeightCm || S.EndRampCells != 0) ||
		    static_cast<uint8>(S.Profile) < 2 || static_cast<uint8>(S.Profile) > 3 || S.DeckHeightCm > 100000 ||
		    S.StartHeightCm < -100000 || S.EndHeightCm < -100000)
		{
			Error = TEXT("Invalid saved road structure profile");
			return false;
		}
		TArray<FCityCellCoord> Path;
		BuildOrthogonalRoadPath(S.Start, S.End, Path);
		for (int32 I = 1; I < Path.Num() - 1; ++I)
			Path[I].RoadLevel = 1;
		for (const auto& C : Path)
		{
			if (!IsRoad(C))
			{
				Error = TEXT("Incomplete saved road span");
				return false;
			}
			if (const auto* Existing = Seen.Find(C))
			{
				if (!CanRaisedSpansIntersect(S, **Existing, C, Error))
					return false;
			}
			else
				Seen.Add(C, &S);
			if (C.RoadLevel == 1 && GetRoadDefinitionId(C) != ResolveRaisedRoadClass(C))
			{
				Error = TEXT("Road span has inconsistent classes");
				return false;
			}
		}
		if (Terrain)
		{
			auto P = BeginRoadPreview(S.Start, S.DefinitionId, BaseCost(S.DefinitionId));
			P.End = S.End;
			P.Profile = S.Profile;
			P.ElevationStep = S.ElevationStep;
			FCityRoadStructure Expected;
			const bool bCurrentMatches = MakeRoadStructure(P, Expected, Error) && Expected == S;
			// Existing saves retain their original terrain-relative decks and ramps.
			if (!bCurrentMatches)
				Error.Reset();
			if (!bCurrentMatches && (!MakeRoadStructure(P, Expected, Error, false) || !(Expected == S)))
			{
				if (Error.IsEmpty())
					Error = TEXT("Saved road span disagrees with terrain, ramps or cost");
				return false;
			}
		}
	}
	for (const auto& Cell : RoadCells)
	{
		if (Cell.RoadLevel == 1 && !FindRoadStructure(Cell, false))
		{
			Error = TEXT("Raised road has no owning span");
			return false;
		}
		if (Cell.RoadLevel == 0)
			for (const auto& Span : RoadStructures)
				if (Contains(Span, Cell) && Cell != Span.Start && Cell != Span.End)
					if (!CanGroundCrossSpan(Span, Cell, Error))
						return false;
	}
	return true;
}
