#include "Simulation/CityCalendar.h"
#include "Presentation/CityBoardActor.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Interaction/CityBoardPicking.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "UObject/ConstructorHelpers.h"

void ACityBoardActor::CreateSeasonalDecorationPools()
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> HomeSpace(TEXT(
	    "/Game/CitySim/Art/Meshes/SM_Building_Residential_SeasonalSpace_A.SM_Building_Residential_SeasonalSpace_A"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Park(
	    TEXT("/Game/CitySim/Art/Meshes/SM_Building_Park_A.SM_Building_Park_A"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ParkDecember(
	    TEXT("/Game/CitySim/Art/Meshes/SM_Building_Park_December_A.SM_Building_Park_December_A"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Neighborhood(
	    TEXT("/Game/CitySim/Art/Meshes/SM_Building_NeighborhoodPark_A.SM_Building_NeighborhoodPark_A"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> NeighborhoodDecember(TEXT(
	    "/Game/CitySim/Art/Meshes/SM_Building_NeighborhoodPark_December_A.SM_Building_NeighborhoodPark_December_A"));
	SeasonalResidentialMesh = HomeSpace.Object;
	PocketParkMesh = Park.Object;
	PocketParkDecemberMesh = ParkDecember.Object;
	NeighborhoodParkMesh = Neighborhood.Object;
	NeighborhoodParkDecemberMesh = NeighborhoodDecember.Object;
	const TCHAR* Names[] = {TEXT("FlowerBed"), TEXT("Pumpkin"),  TEXT("Snowman"),      TEXT("FestiveTree"),
	                        TEXT("Gift"),      TEXT("LeafPile"), TEXT("WreathAutumn"), TEXT("WreathWinter")};
	for (int32 I = 0; I < UE_ARRAY_COUNT(Names); ++I)
	{
		const FString ComponentName = FString(TEXT("SeasonalDecoration")) + Names[I];
		auto* Pool = CreateDefaultSubobject<UInstancedStaticMeshComponent>(*ComponentName);
		Pool->SetupAttachment(BoardRoot);
		Pool->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Pool->SetGenerateOverlapEvents(false);
		Pool->SetCanEverAffectNavigation(false);
		Pool->SetCastShadow(false);
		Pool->PrimaryComponentTick.bCanEverTick = false;
		const FString Asset =
		    FString::Printf(TEXT("/Game/CitySim/Art/Meshes/SM_Seasonal_%s_A.SM_Seasonal_%s_A"), Names[I], Names[I]);
		ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(*Asset);
		Pool->SetStaticMesh(Mesh.Object);
		SeasonalDecorationPools.Add(Pool);
	}
}

UStaticMesh* ACityBoardActor::GetSeasonalParkMesh(const FName DefinitionId, const int32 Month) const
{
	const auto* Definition = BuildingCatalog ? BuildingCatalog->FindDefinition(DefinitionId) : nullptr;
	const int32 Side = DefinitionId == TEXT("Park") ? 3 : DefinitionId == TEXT("NeighborhoodPark") ? 5 : 0;
	if (!Definition || Side == 0 || Definition->Footprint.Width != Side || Definition->Footprint.Height != Side)
		return nullptr;
	if (DefinitionId == TEXT("Park"))
		return Month == 12 && PocketParkDecemberMesh ? PocketParkDecemberMesh.Get() : PocketParkMesh.Get();
	if (DefinitionId == TEXT("NeighborhoodPark"))
		return Month == 12 && NeighborhoodParkDecemberMesh ? NeighborhoodParkDecemberMesh.Get()
		                                                   : NeighborhoodParkMesh.Get();
	return nullptr;
}

void ACityBoardActor::UpdateSeasonalBuildingMeshes(const int32 Month)
{
	// Only reviewed, completed park families share this presentation swap.
	// Keep instance transforms, logical footprints and damaged/construction pools.
	for (const FName Id : {FName(TEXT("Park")), FName(TEXT("NeighborhoodPark"))})
	{
		if (auto* Pool = BuildingInstances.Find(Id))
		{
			UStaticMesh* Mesh = GetSeasonalParkMesh(Id, Month);
			if (Mesh && (*Pool)->GetStaticMesh() != Mesh)
			{
				(*Pool)->SetStaticMesh(Mesh);
				ApplyBuildingFade(**Pool);
			}
		}
	}
}

void ACityBoardActor::ClearSeasonalDecorations()
{
	for (const auto& Pool : SeasonalDecorationPools)
		if (Pool)
		{
			if (Pool->GetInstanceCount())
				Pool->ClearInstances();
			Pool->SetVisibility(false);
		}
	SeasonalDecorationSamples.Reset();
	SeasonalDecorationCheckSignature = SeasonalDecorationSelectionSignature = MAX_uint32;
}

void ACityBoardActor::UpdateSeasonalDecorations()
{
	if (!SimulationSubsystem || !PlacementModel || !BuildingCatalog || SimulationSubsystem->IsEntryMenuSuspended())
	{
		ClearSeasonalDecorations();
		return;
	}
	const auto& Economy = SimulationSubsystem->GetEconomySnapshot();
	const auto& Weather = SimulationSubsystem->GetEnvironmentSnapshot();
	const auto& Development = SimulationSubsystem->GetDevelopmentSnapshot();
	const auto& Services = SimulationSubsystem->GetServiceSnapshot();
	const int32 Month =
	    static_cast<int32>(
	        FCityCalendar::MonthIndex(Economy.FixedStepId, SimulationSubsystem->GetEconomyTuning().FixedStepsPerMonth) % 12) +
	    1;
	uint32 Check = HashCombine(GetTypeHash(SimulationSubsystem->GetPresentationGeneration()),
	                           GetTypeHash(PlacementModel->GetRevision()));
	Check = HashCombine(Check, GetTypeHash(Economy.FixedStepId));
	// Authority changes on fixed steps or command revisions. Do not scan lots
	// or generate climate fields for every rendered frame/pointer movement.
	if (Check != SeasonalDecorationCheckSignature)
	{
		UpdateSeasonalBuildingMeshes(Month);
		uint32 Selection = HashCombine(GetTypeHash(SimulationSubsystem->GetPresentationGeneration()),
		                               GetTypeHash(PlacementModel->GetRevision()));
		Selection = HashCombine(Selection, GetTypeHash(Month));
		Selection = HashCombine(Selection, GetTypeHash(static_cast<uint8>(Weather.Season)));
		Selection = HashCombine(Selection, GetTypeHash(static_cast<uint8>(Weather.Temperature)));
		for (const auto& Pair : PlacementModel->GetEntities())
		{
			Selection = HashCombine(Selection, GetTypeHash(Pair.Key));
			const auto* Failure = Services.EntityFailures.Find(Pair.Key);
			Selection = HashCombine(Selection, Failure ? GetTypeHash(static_cast<uint8>(*Failure)) : MAX_uint32);
			Selection = HashCombine(Selection, GetTypeHash(Development.IsOperational(Pair.Key)));
			if (const auto* State = Development.Find(Pair.Key))
			{
				Selection = HashCombine(Selection, GetTypeHash(static_cast<uint8>(State->DensityLevel)));
				Selection = HashCombine(Selection, GetTypeHash(State->HasCivicUpgrade()));
			}
		}
		for (const auto& Park : Services.ParkProviders)
			Selection = HashCombine(Selection, GetTypeHash(Park.EntityId));
		if (Selection != SeasonalDecorationSelectionSignature)
		{
			TArray<FCitySeasonalDecorationSample> Samples;
			FCitySeasonalDecorations::Build(*PlacementModel, SimulationSubsystem->GetTerrain(), Services, Development,
			                                BuildingCatalog, Weather, Month, Samples);
			if (Samples != SeasonalDecorationSamples)
			{
				++SeasonalDecorationRebuildCount;
				for (const auto& Pool : SeasonalDecorationPools)
					Pool->ClearInstances();
				for (const auto& Sample : Samples)
				{
					const auto* Entity = PlacementModel->GetEntities().Find(Sample.EntityId);
					const auto* Definition = Entity ? BuildingCatalog->FindDefinition(Entity->DefinitionId) : nullptr;
					const int32 Kind = static_cast<int32>(Sample.Kind);
					if (!Entity || !Definition || !SeasonalDecorationPools.IsValidIndex(Kind) ||
					    !SeasonalDecorationPools[Kind]->GetStaticMesh())
						continue;
					const double Base =
					    Sample.bUseBuildingBase
					        ? GetBuildingBaseHeight(Entity->Anchor, *Definition, Entity->QuarterTurnsClockwise) + 40.0
					        : SimulationSubsystem->GetTerrain().SampleHeightCm(Sample.CellPosition.X,
					                                                           Sample.CellPosition.Y);
					const FVector Position(Sample.CellPosition.X * CityBoardPicking::LogicalCellSizeCm,
					                       Sample.CellPosition.Y * CityBoardPicking::LogicalCellSizeCm,
					                       Base + Sample.SurfaceOffsetCm);
					SeasonalDecorationPools[Kind]->AddInstance(
					    FTransform(FRotator(0, Sample.YawDegrees, 0), Position, FVector(Sample.Scale)));
				}
				SeasonalDecorationSamples = MoveTemp(Samples);
			}
			SeasonalDecorationSelectionSignature = Selection;
		}
		SeasonalDecorationCheckSignature = Check;
	}
	for (const auto& Pool : SeasonalDecorationPools)
		Pool->SetVisibility(!bSeasonalDecorationsAnalytical && Pool->GetInstanceCount() > 0);
}
