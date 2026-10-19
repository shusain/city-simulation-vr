#include "Presentation/CityRoadRibbon.h"
#include "Presentation/CityTerrainSurface.h"
#include "Interaction/CityBoardPicking.h"
#include "Algo/Reverse.h"

double FCityRoadRibbonMesh::WidthAt(const double Along, const double Start, const double Center, const double End)
{
	return Along <= .5 ? FMath::Lerp(Start, Center, FMath::Clamp(Along * 2, 0., 1.))
	                   : FMath::Lerp(Center, End, FMath::Clamp((Along - .5) * 2, 0., 1.));
}
void FCityRoadRibbonMesh::AppendStraight(const FCityCellCoord& Cell, const bool bHorizontal, const double WidthCells,
                                         TFunctionRef<double(double, double)> Height, const double StartWidth,
                                         const double EndWidth)
{
	const double Start = StartWidth < 0 ? WidthCells : StartWidth, End = EndWidth < 0 ? WidthCells : EndWidth;
	AppendPatch(
	    [&](double Along, double Across)
	    {
		    const double Cross = .5 + (Across - .5) * WidthAt(Along, Start, WidthCells, End);
		    return FVector2D(Cell.X + (bHorizontal ? Along : Cross), Cell.Y + (bHorizontal ? Cross : Along));
	    },
	    Height);
}
void FCityRoadRibbonMesh::AppendJunction(const FCityCellCoord& Cell, const uint8 Mask, const double WidthCells,
                                         const TArray<double>& PortalWidths,
                                         TFunctionRef<double(double, double)> Height)
{
	const double Half = WidthCells * .5;
	AppendPatch([&](double U, double V)
	            { return FVector2D(Cell.X + .5 + (U - .5) * WidthCells, Cell.Y + .5 + (V - .5) * WidthCells); },
	            Height);
	for (int32 Side = 0; Side < 4; ++Side)
	{
		if ((Mask & (1 << Side)) == 0)
			continue;
		const bool bAlongX = Side == 1 || Side == 3;
		const double Sign = Side < 2 ? 1. : -1.;
		AppendPatch(
		    [&](double U, double V)
		    {
			    const double Along = .5 + Sign * FMath::Lerp(Half, .5, U);
			    const double Across = .5 + (V - .5) * FMath::Lerp(WidthCells, PortalWidths[Side], U);
			    return FVector2D(Cell.X + (bAlongX ? Along : Across), Cell.Y + (bAlongX ? Across : Along));
		    },
		    Height);
	}
}
void FCityRoadRibbonMesh::AppendPolygon(const TArray<FVector2D>& Polygon,
    TFunctionRef<double(double, double)> Height, const double Lift)
{
    if (Polygon.Num() < 3) return;
    auto Clip = [](const TArray<FVector2D>& Input, TFunctionRef<double(const FVector2D&)> Distance) {
        TArray<FVector2D> Out;
        for (int32 I=0; I<Input.Num(); ++I) {
            const auto A=Input[I], B=Input[(I+1)%Input.Num()];
            const double DA=Distance(A), DB=Distance(B);
            if (DA >= -1.e-8) Out.Add(A);
            if ((DA>0 && DB<0) || (DA<0 && DB>0)) Out.Add(FMath::Lerp(A,B,DA/(DA-DB)));
        }
        return Out;
    };
    FVector2D Min=Polygon[0], Max=Min;
    for (const auto& P:Polygon) { Min.X=FMath::Min(Min.X,P.X); Min.Y=FMath::Min(Min.Y,P.Y); Max.X=FMath::Max(Max.X,P.X); Max.Y=FMath::Max(Max.Y,P.Y); }
    constexpr double Grid=.5, Size=CityBoardPicking::LogicalCellSizeCm;
    for (int32 Y=FMath::FloorToInt(Min.Y/Grid); Y<FMath::CeilToInt(Max.Y/Grid); ++Y)
        for (int32 X=FMath::FloorToInt(Min.X/Grid); X<FMath::CeilToInt(Max.X/Grid); ++X)
		{
			const int32 N = Terrain ? FCityTerrainSurface::Subdivisions(*Terrain, X, Y) : 1;
			const double Fine = Grid / N;
			for (int32 FineY = 0; FineY < N; ++FineY)
				for (int32 FineX = 0; FineX < N; ++FineX)
					for (const bool Lower : {true, false})
					{
						const double Left = X * Grid + FineX * Fine, Bottom = Y * Grid + FineY * Fine;
						auto P = Clip(Polygon, [&](const FVector2D& V) { return V.X - Left; });
						P = Clip(P, [&](const FVector2D& V) { return Left + Fine - V.X; });
						P = Clip(P, [&](const FVector2D& V) { return V.Y - Bottom; });
						P = Clip(P, [&](const FVector2D& V) { return Bottom + Fine - V.Y; });
						P = Clip(P,
						         [&](const FVector2D& V)
						         {
							         const double D = Left + Bottom + Fine - V.X - V.Y;
							         return Lower ? D : -D;
						         });
						if (P.Num() < 3)
							continue;
						double Area = 0;
						for (int32 I = 0; I < P.Num(); ++I)
							Area += P[I].X * P[(I + 1) % P.Num()].Y - P[I].Y * P[(I + 1) % P.Num()].X;
						if (FMath::Abs(Area) < 1.e-9)
							continue;
						if (Area > 0)
							Algo::Reverse(P);
						const int32 Base = Vertices.Num();
						for (const auto& V : P)
						{
							Vertices.Add(FVector(V.X * Size, V.Y * Size, Height(V.X, V.Y) + Lift));
							UVs.Add(V);
							constexpr double R = .0001;
							Normals.Add(FVector(-(Height(V.X + R, V.Y) - Height(V.X - R, V.Y)) / (2 * R * Size),
							                    -(Height(V.X, V.Y + R) - Height(V.X, V.Y - R)) / (2 * R * Size), 1)
							                .GetSafeNormal());
						}
						for (int32 I = 1; I + 1 < P.Num(); ++I)
						{
							if (FMath::Abs(FVector::CrossProduct(Vertices[Base + I] - Vertices[Base],
							                                     Vertices[Base + I + 1] - Vertices[Base])
							                   .Z) > .00001)
								Triangles.Append({Base, Base + I, Base + I + 1});
						}
					}
		}
}
void FCityRoadRibbonMesh::AppendPatch(TFunctionRef<FVector2D(double, double)> Position,
                                      TFunctionRef<double(double, double)> Height)
{
    // Retain the width profile midpoint/portal subdivisions before clipping.
    for(int32 V=0;V<Segments;++V) for(int32 U=0;U<Segments;++U) {
        const double A=U/static_cast<double>(Segments),B=(U+1)/static_cast<double>(Segments);
        const double C=V/static_cast<double>(Segments),D=(V+1)/static_cast<double>(Segments);
        AppendPolygon({Position(A,C),Position(B,C),Position(B,D),Position(A,D)},Height,SurfaceLiftCm);
    }
}
