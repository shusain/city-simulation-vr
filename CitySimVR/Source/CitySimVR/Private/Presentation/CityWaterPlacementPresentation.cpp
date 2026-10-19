#include "Presentation/CityBoardActor.h"
#include "Interaction/CityBoardPicking.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "ProceduralMeshComponent.h"

void ACityBoardActor::ClearWaterPlacementOverlay()
{
	if (!WaterPlacementSurface || !WaterPlacementSurface->IsVisible())
		return;
	WaterPlacementSurface->SetVisibility(false);
	WaterPlacementSurface->ClearAllMeshSections();
	WaterPlacementOverlay = {};
	WaterPlacementSignature = WaterPlacementContentSignature = MAX_uint32;
}

void ACityBoardActor::UpdateWaterPlacementOverlay()
{
	const auto* Definition = GetSelectedDefinition();
	if (!SimulationSubsystem || !PlacementModel || !WaterPlacementSurface || ToolMode != ECityToolMode::Service ||
	    !Definition || !FCityBuildingDefinition::IsWaterFacility(Definition->DefinitionId) || bManipulationActive ||
	    SimulationSubsystem->IsEntryMenuSuspended() || IsHidden())
	{
		ClearWaterPlacementOverlay();
		return;
	}
	const auto& Services = SimulationSubsystem->GetServiceSnapshot();
	uint32 Signature = HashCombine(GetTypeHash(SimulationSubsystem->GetPresentationGeneration()),
	                               GetTypeHash(PlacementModel->GetRevision()));
	Signature = HashCombine(Signature, GetTypeHash(SimulationSubsystem->GetEconomySnapshot().FixedStepId));
	Signature = HashCombine(Signature, GetTypeHash(Services.Water.RulesVersion));
	for (const auto& Source : Services.Water.Sources)
	{
		Signature = HashCombine(Signature, GetTypeHash(Source.Capacity));
		Signature = HashCombine(Signature, GetTypeHash(Source.bOperational));
		Signature = HashCombine(Signature, GetTypeHash(Source.SourcePressurePercent));
	}
	if (WaterPlacementSurface->IsVisible() && Signature == WaterPlacementSignature)
		return;
	WaterPlacementOverlay =
	    FCityWaterPlacementOverlay::Build(*PlacementModel, SimulationSubsystem->GetTerrain(), Services);
	uint32 Content = HashCombine(GetTypeHash(SimulationSubsystem->GetPresentationGeneration()),
	                             GetTypeHash(WaterPlacementOverlay.Dimensions.GetWidth()));
	for (const auto Yield : WaterPlacementOverlay.GroundwaterYieldPercent)
		Content = HashCombine(Content, GetTypeHash(Yield));
	for (const auto& Road : WaterPlacementOverlay.Roads)
	{
		Content = HashCombine(Content, GetTypeHash(Road.Cell));
		Content = HashCombine(Content, GetTypeHash(Road.PressurePercent));
	}
	// Hover/rotation never rebuild this mesh. State checks can refresh the exact
	// descriptions without uploading unchanged terrain or road color buffers.
	if (Content != WaterPlacementContentSignature)
	{
		RebuildWaterPlacementOverlay();
		WaterPlacementContentSignature = Content;
	}
	WaterPlacementSignature = Signature;
	WaterPlacementSurface->SetVisibility(true);
}

void ACityBoardActor::RebuildWaterPlacementOverlay()
{
	++WaterPlacementRebuildCount;
	WaterPlacementSurface->ClearAllMeshSections();
	const auto& Terrain = SimulationSubsystem->GetTerrain();
	TArray<FVector> Vertices, Normals;
	TArray<int32> Triangles;
	TArray<FVector2D> UVs;
	TArray<FLinearColor> Colors;
	const TArray<FProcMeshTangent> Tangents;
	const double Size = CityBoardPicking::LogicalCellSizeCm;
	const int32 Side = Terrain.GetSampleWidth(), Rows = Terrain.GetSampleHeight();
	// Reuse the terrain sampling/winding exactly so the thin film hugs peaks,
	// slopes and shore banks rather than spanning them with coarse tile quads.
	for (int32 Y = 0; Y < Rows; ++Y)
		for (int32 X = 0; X < Side; ++X)
		{
			const double CellX = X / double(FCityTerrainHeightfield::SamplesPerCell),
			             CellY = Y / double(FCityTerrainHeightfield::SamplesPerCell);
			const double CenterX = CellX - .5, CenterY = CellY - .5;
			const int32 LowerX = FMath::FloorToInt(CenterX), LowerY = FMath::FloorToInt(CenterY);
			auto SiteColor = [&](int32 Dx, int32 Dy)
			{
				return WaterPlacementOverlay.GroundwaterColorAt(
				    {FMath::Clamp(LowerX + Dx, 0, Terrain.GetDimensions().GetWidth() - 1),
				     FMath::Clamp(LowerY + Dy, 0, Terrain.GetDimensions().GetHeight() - 1)});
			};
			FLinearColor Color = FMath::Lerp(FMath::Lerp(SiteColor(0, 0), SiteColor(1, 0), float(CenterX - LowerX)),
			                                 FMath::Lerp(SiteColor(0, 1), SiteColor(1, 1), float(CenterX - LowerX)),
			                                 float(CenterY - LowerY));
			Color.A *= .34f;
			Vertices.Add(FVector(CellX * Size, CellY * Size, Terrain.GetSampleHeightCm(X, Y) + 18.0));
			Normals.Add(FVector::UpVector);
			UVs.Add(FVector2D(CellX, CellY));
			Colors.Add(Color);
		}
	for (int32 Y = 0; Y < Rows - 1; ++Y)
		for (int32 X = 0; X < Side - 1; ++X)
		{
			if (Terrain.IsWaterCell(
			        {X / FCityTerrainHeightfield::SamplesPerCell, Y / FCityTerrainHeightfield::SamplesPerCell}))
				continue;
			const int32 Base = Y * Side + X;
			Triangles.Append({Base, Base + Side, Base + 1, Base + 1, Base + Side, Base + Side + 1});
		}
	WaterPlacementSurface->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, Colors, Tangents, false);
	Vertices.Reset();
	Normals.Reset();
	Triangles.Reset();
	UVs.Reset();
	Colors.Reset();
	TMap<FCityCellCoord, int32> Pressure;
	for (const auto& Road : WaterPlacementOverlay.Roads)
		Pressure.Add(Road.Cell, Road.PressurePercent);
	for (const auto& Road : WaterPlacementOverlay.Roads)
	{
		const auto Cell = Road.Cell;
		TArray<FCityCellCoord> Neighbors;
		PlacementModel->GetConnectedRoadNeighbors(Cell, Neighbors);
		const double HalfWidth = FMath::Clamp(GetRoadWidthAt(Cell, true, .5) * .18, .045, .12);
		auto Quad = [&](const FVector2D& Min, const FVector2D& Max, const int32 EndPressure, const bool bAlongX,
		                const bool bPositive)
		{
			const int32 Base = Vertices.Num();
			for (const auto Offset :
			     {FVector2D(Min.X, Min.Y), FVector2D(Max.X, Min.Y), FVector2D(Max.X, Max.Y), FVector2D(Min.X, Max.Y)})
			{
				const double X = Cell.X + Offset.X, Y = Cell.Y + Offset.Y;
				const double Along = bAlongX ? Offset.X : Offset.Y;
				const float Fraction = FMath::Clamp(float((bPositive ? Along - .5 : .5 - Along) * 2), 0.f, 1.f);
				FLinearColor Color = FMath::Lerp(FCityWaterPlacementOverlay::PressureColor(Road.PressurePercent),
				                                 FCityWaterPlacementOverlay::PressureColor(EndPressure), Fraction);
				Color.A = .65f;
				Vertices.Add(FVector(X * Size, Y * Size, GetRoadProfileHeight(X, Y, Cell.RoadLevel) + 68.0));
				Normals.Add(FVector::UpVector);
				UVs.Add(FVector2D(X, Y));
				Colors.Add(Color);
			}
			Triangles.Append({Base, Base + 2, Base + 1, Base, Base + 3, Base + 2});
		};
		if (Neighbors.IsEmpty())
			Quad(FVector2D(.5 - HalfWidth, .5 - HalfWidth), FVector2D(.5 + HalfWidth, .5 + HalfWidth),
			     Road.PressurePercent, true, true);
		for (const auto& Next : Neighbors)
		{
			const bool bAlongX = Next.X != Cell.X, bPositive = bAlongX ? Next.X > Cell.X : Next.Y > Cell.Y;
			const int32 BorderPressure = (Road.PressurePercent + Pressure.FindRef(Next)) / 2;
			Quad(
			    bAlongX ? FVector2D(bPositive ? .5 : 0, .5 - HalfWidth) : FVector2D(.5 - HalfWidth, bPositive ? .5 : 0),
			    bAlongX ? FVector2D(bPositive ? 1 : .5, .5 + HalfWidth) : FVector2D(.5 + HalfWidth, bPositive ? 1 : .5),
			    BorderPressure, bAlongX, bPositive);
		}
	}
	if (!Triangles.IsEmpty())
		WaterPlacementSurface->CreateMeshSection_LinearColor(1, Vertices, Triangles, Normals, UVs, Colors, Tangents,
		                                                     false);
}
