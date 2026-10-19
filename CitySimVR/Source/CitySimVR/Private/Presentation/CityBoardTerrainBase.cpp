#include "Presentation/CityBoardActor.h"

#include "Interaction/CityBoardPicking.h"
#include "ProceduralMeshComponent.h"
#include "Simulation/CitySimulationSubsystem.h"

void ACityBoardActor::RebuildTerrainBase()
{
	if (!TerrainBase || !SimulationSubsystem)
		return;
	const auto& Terrain = SimulationSubsystem->GetTerrain();
	const int32 LastX = Terrain.GetSampleWidth() - 1;
	const int32 LastY = Terrain.GetSampleHeight() - 1;
	constexpr double Step = CityBoardPicking::LogicalCellSizeCm / FCityTerrainHeightfield::SamplesPerCell;
	// A five-centimetre plinth on the default two-metre board, below every basin.
	// Boundary vertices match the terrain exactly; this is never a flat cap through hills/water.
	double Bottom = Terrain.GetSampleHeightCm(0, 0);
	for (const int32 Height : Terrain.GetQuantizedHeightsCm())
		Bottom = FMath::Min(Bottom, static_cast<double>(Height));
	Bottom -= Terrain.GetDimensions().GetWidth() * CityBoardPicking::LogicalCellSizeCm * .025;
	TArray<FVector> Rim;
	auto AddRim = [&](const int32 X, const int32 Y)
	{ Rim.Add(FVector(X * Step, Y * Step, Terrain.GetSampleHeightCm(X, Y))); };
	for (int32 X = 0; X < LastX; ++X)
		AddRim(X, 0);
	for (int32 Y = 0; Y < LastY; ++Y)
		AddRim(LastX, Y);
	for (int32 X = LastX; X > 0; --X)
		AddRim(X, LastY);
	for (int32 Y = LastY; Y > 0; --Y)
		AddRim(0, Y);

	TArray<FVector> Vertices, Normals;
	TArray<int32> Triangles;
	TArray<FVector2D> UVs;
	auto AddQuad = [&](const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FVector& Normal)
	{
		const int32 First = Vertices.Num();
		Vertices.Append({A, B, C, D});
		Normals.Append({Normal, Normal, Normal, Normal});
		UVs.Append({FVector2D(0, 0), FVector2D(1, 0), FVector2D(1, 1), FVector2D(0, 1)});
		Triangles.Append({First, First + 1, First + 2, First, First + 2, First + 3});
	};
	for (int32 Index = 0; Index < Rim.Num(); ++Index)
	{
		const FVector A = Rim[Index], B = Rim[(Index + 1) % Rim.Num()];
		const FVector Edge = B - A;
		AddQuad(A, B, FVector(B.X, B.Y, Bottom), FVector(A.X, A.Y, Bottom),
		        FVector(Edge.Y, -Edge.X, 0).GetSafeNormal());
	}
	AddQuad(FVector(0, 0, Bottom), FVector(LastX * Step, 0, Bottom), FVector(LastX * Step, LastY * Step, Bottom),
	        FVector(0, LastY * Step, Bottom), -FVector::UpVector);
	TerrainBase->ClearAllMeshSections();
	TerrainBase->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, {}, {}, false);
	UE_LOG(LogTemp, Display, TEXT("CITYSIM_TERRAIN_BASE_REBUILT RimSamples=%d BottomCm=%.1f Triangles=%d"), Rim.Num(),
	       Bottom, Triangles.Num() / 3);
}
