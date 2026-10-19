#include "Interaction/CityBoardPicking.h"
#include "Simulation/CityTerrain.h"

namespace
{
	bool ClipAxis(const double Origin, const double Direction, const double Extent, double& InOutMin, double& InOutMax)
	{
		if (FMath::IsNearlyZero(Direction))
		{
			return Origin >= 0.0 && Origin <= Extent;
		}
		double A = (0.0 - Origin) / Direction;
		double B = (Extent - Origin) / Direction;
		if (A > B)
			Swap(A, B);
		InOutMin = FMath::Max(InOutMin, A);
		InOutMax = FMath::Min(InOutMax, B);
		return InOutMin <= InOutMax;
	}
} // namespace

FVector2D CityBoardPicking::GetLogicalBoardSize(const FCityGridDimensions Dimensions)
{
	return Dimensions.IsValid()
	           ? FVector2D(Dimensions.GetWidth() * LogicalCellSizeCm, Dimensions.GetHeight() * LogicalCellSizeCm)
	           : FVector2D::ZeroVector;
}

bool CityBoardPicking::PickCellFromWorldRay(const FTransform& BoardTransform, const FVector& WorldRayOrigin,
                                            const FVector& WorldRayDirection, FCityCellCoord& OutCell,
                                            FVector* OutWorldHit)
{
	return PickCellFromWorldRay(BoardTransform, FCityGridDimensions(), WorldRayOrigin, WorldRayDirection, OutCell,
	                            OutWorldHit);
}

bool CityBoardPicking::PickCellFromWorldRay(const FTransform& BoardTransform, const FCityGridDimensions Dimensions,
                                            const FVector& WorldRayOrigin, const FVector& WorldRayDirection,
                                            FCityCellCoord& OutCell, FVector* OutWorldHit)
{
	if (!Dimensions.IsValid() || WorldRayOrigin.ContainsNaN() || WorldRayDirection.ContainsNaN() ||
	    BoardTransform.ContainsNaN() || WorldRayDirection.IsNearlyZero() ||
	    BoardTransform.GetScale3D().GetAbsMin() <= UE_DOUBLE_SMALL_NUMBER)
	{
		return false;
	}

	const FVector LocalOrigin = BoardTransform.InverseTransformPosition(WorldRayOrigin);
	const FVector LocalDirection = BoardTransform.InverseTransformVector(WorldRayDirection);
	if (FMath::IsNearlyZero(LocalDirection.Z))
	{
		return false;
	}

	const double DistanceAlongRay = -LocalOrigin.Z / LocalDirection.Z;
	if (DistanceAlongRay < 0.0)
	{
		return false;
	}

	const FVector LocalHit = LocalOrigin + LocalDirection * DistanceAlongRay;
	const auto Size = GetLogicalBoardSize(Dimensions);
	if (LocalHit.X < 0.0 || LocalHit.Y < 0.0 || LocalHit.X >= Size.X || LocalHit.Y >= Size.Y)
	{
		return false;
	}

	OutCell = FCityCellCoord(FMath::FloorToInt(LocalHit.X / LogicalCellSizeCm),
	                         FMath::FloorToInt(LocalHit.Y / LogicalCellSizeCm));
	if (OutWorldHit)
	{
		*OutWorldHit = BoardTransform.TransformPosition(LocalHit);
	}
	return true;
}

bool CityBoardPicking::PickCellFromWorldRay(const FTransform& BoardTransform, const FCityTerrainHeightfield& Terrain,
                                            const FVector& WorldRayOrigin, const FVector& WorldRayDirection,
                                            FCityCellCoord& OutCell, FVector* OutWorldHit)
{
	if (WorldRayOrigin.ContainsNaN() || WorldRayDirection.ContainsNaN() || BoardTransform.ContainsNaN() ||
	    WorldRayDirection.IsNearlyZero() || BoardTransform.GetScale3D().GetAbsMin() <= UE_DOUBLE_SMALL_NUMBER)
	{
		return false;
	}
	const FVector Origin = BoardTransform.InverseTransformPosition(WorldRayOrigin);
	const FVector Direction = BoardTransform.InverseTransformVector(WorldRayDirection);
	const auto Size = GetLogicalBoardSize(Terrain.GetDimensions());
	double MinT = 0.0;
	double MaxT = 10000000.0;
	if (Size.IsNearlyZero() || !ClipAxis(Origin.X, Direction.X, Size.X, MinT, MaxT) ||
	    !ClipAxis(Origin.Y, Direction.Y, Size.Y, MinT, MaxT))
	{
		return false;
	}

	auto SurfaceDelta = [&Terrain, &Origin, &Direction](const double T)
	{
		const FVector Point = Origin + Direction * T;
		return Point.Z - Terrain.SampleHeightCm(Point.X / LogicalCellSizeCm, Point.Y / LogicalCellSizeCm);
	};
	double PreviousT = MinT;
	double PreviousDelta = SurfaceDelta(PreviousT);
	constexpr int32 Steps = 256;
	for (int32 Step = 1; Step <= Steps; ++Step)
	{
		const double T = FMath::Lerp(MinT, MaxT, static_cast<double>(Step) / Steps);
		const double Delta = SurfaceDelta(T);
		if ((PreviousDelta >= 0.0 && Delta <= 0.0) || (PreviousDelta <= 0.0 && Delta >= 0.0))
		{
			double Low = PreviousT;
			double High = T;
			for (int32 Iteration = 0; Iteration < 16; ++Iteration)
			{
				const double Mid = (Low + High) * 0.5;
				if ((SurfaceDelta(Low) >= 0.0) == (SurfaceDelta(Mid) >= 0.0))
					Low = Mid;
				else
					High = Mid;
			}
			const FVector LocalHit = Origin + Direction * ((Low + High) * 0.5);
			if (LocalHit.X < 0.0 || LocalHit.Y < 0.0 || LocalHit.X >= Size.X || LocalHit.Y >= Size.Y)
			{
				return false;
			}
			OutCell = FCityCellCoord(FMath::FloorToInt(LocalHit.X / LogicalCellSizeCm),
			                         FMath::FloorToInt(LocalHit.Y / LogicalCellSizeCm));
			if (OutWorldHit)
				*OutWorldHit = BoardTransform.TransformPosition(LocalHit);
			return true;
		}
		PreviousT = T;
		PreviousDelta = Delta;
	}
	return false;
}

FVector CityBoardPicking::GetCellCenterLocal(const FCityCellCoord& Cell)
{
	return FVector((Cell.X + 0.5) * LogicalCellSizeCm, (Cell.Y + 0.5) * LogicalCellSizeCm, 0.0);
}
