#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Simulation/CityPedestrianPaths.h"
#include "Commands/CityPlacementModel.h"
#include "Persistence/CityPersistence.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Presentation/CityBoardActor.h"
#include "Presentation/CityRoadRibbon.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "ProceduralMeshComponent.h"
#include "Engine/World.h"

namespace
{
bool Road(FCityPlacementModel& City,FCityCellCoord A,FCityCellCoord B,FName Id=TEXT("LocalRoad"))
{
	auto P=City.BeginRoadPreview(A,Id);City.UpdateRoadPreviewEnd(P,B);return City.CommitRoad(P).IsSuccess();
}
uint64 Lot(FCityPlacementModel& City,FCityCellCoord P,int32 Side=15,int32 Turn=0,FName Type=TEXT("WalkLot"))
{
	FCityBuildingDefinition D;D.DefinitionId=Type;D.DisplayName=TEXT("Walk lot");D.Cost=0;
	D.Footprint={1,1};D.EntranceSides=Side;
	auto Preview=City.BeginPreview(D,P,Turn);
	return City.CommitPlacement(Preview,D).EntityId;
}
FCityTrafficVehicleState Walker(const FCityPlacementModel& City,const FCityRoadNetworkSnapshot& N,uint64 A,uint64 B,uint64 Id=11)
{
	FCityTrafficSimulation T;FCityPopulationSnapshot P;FCityCitizenRecord C;
	C.Id=Id;C.HomeEntityId=A;C.CurrentLocationEntityId=A;C.TripDestinationEntityId=B;
	C.TripPurpose=ECityTripPurpose::Commute;C.TripVehicleType=ECityTripVehicleType::Walk;P.Citizens.Add(C);
	T.UpdateTrips(City,nullptr,P,N,0);
	return T.GetSnapshot().Vehicles.IsEmpty()?FCityTrafficVehicleState():T.GetSnapshot().Vehicles[0];
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityEntranceWalkTest,"CitySim.Roads.EntranceAwareWalking",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FCityEntranceWalkTest::RunTest(const FString&)
{
	FCityPlacementModel City(100000);
	if(!TestTrue(TEXT("Street commits"),Road(City,FCityPlacementModel::EntranceCell,{20,8}))) return false;
	Road(City,{9,8},{9,16});
	const uint64 South=Lot(City,{6,9}),North=Lot(City,{6,7}),East=Lot(City,{16,9});
	FCityRoadNetworkSnapshot N;FCityRoadNetworkBuilder::Build(City,FCityRoadDefinition::Local(),N);
	const auto Crossings=CityPedestrianPaths::Crossings(City,nullptr);
	TestEqual(TEXT("Only the T intersection produces crossings, not three ordinary lots"),Crossings.Num(),3);
	TestFalse(TEXT("Ordinary frontage has no crossing"),Crossings.ContainsByPredicate([](const auto& X){return X.Cell==FCityCellCoord(6,8);}));
	auto Same=Walker(City,N,South,East),Opposite=Walker(City,N,South,North);
	if(!TestTrue(TEXT("Same-side scheduled path exists"),!Same.WalkPath.IsEmpty()) ||
	    !TestTrue(TEXT("Opposite same-cell scheduled path exists"),!Opposite.WalkPath.IsEmpty())) return false;
	TestFalse(TEXT("Same-cell trip cannot arrive at departure"),Opposite.bCompleted);
	TestTrue(TEXT("Crossing has genuine positive timed length"),CityPedestrianPaths::Length(Opposite.WalkPath)>1000);
	for(int32 I=1;I+1<Same.WalkPath.Num();++I)
		TestTrue(TEXT("Same-side travel keeps physical south sidewalk"),Same.WalkPath[I].Y>8500);
	bool UsedCrossing=false;
	TestTrue(TEXT("Opposite same-portal trip detours to shared intersection"),Opposite.WalkPath.ContainsByPredicate([](const auto& P){return P.X>9000;}));
	for(int32 I=1;I<Opposite.WalkPath.Num();++I)
		for(const auto& X:Crossings) UsedCrossing|=(Opposite.WalkPath[I-1]==X.A || Opposite.WalkPath[I-1]==X.B) &&
		    (FIntVector((X.A.X+X.B.X)/2,(X.A.Y+X.B.Y)/2,X.A.Z)==Opposite.WalkPath[I] || Opposite.WalkPath[I]==X.A || Opposite.WalkPath[I]==X.B);
	TestTrue(TEXT("Opposite-side trip uses same descriptor as paint"),UsedCrossing);
	FCityTrafficSimulation Live;TestTrue(TEXT("Explicit path validates"),Live.AddVehicle(Opposite,N));
	Live.AdvanceOneStep(N,1);TestFalse(TEXT("First step has not teleported into destination"),Live.GetSnapshot().Vehicles[0].bCompleted);
	FString Error,Json;FCitySaveDocument Doc,Read;Doc.Placement=City.ExportState();Doc.Traffic=Live.GetSnapshot();
	Doc.Economy.FixedStepId=Doc.Disasters.LastFixedStepId=1;
	TestTrue(TEXT("Mid-crossing state serializes"),FCityPersistence::Serialize(Doc,Json,Error));
	TestTrue(TEXT("Mid-crossing state reads"),FCityPersistence::Deserialize(Json,Read,Error));
	FCityTrafficSimulation Reload;TestTrue(TEXT("Mid-crossing state restores"),Reload.RestoreState(Read.Traffic,N,1,Error));
	for(int64 Step=2;Step<=80;++Step)
	{
		Live.AdvanceOneStep(N,Step);Reload.AdvanceOneStep(N,Step);
		TestEqual(TEXT("Reload preserves exact walking progress"),Reload.GetSnapshot().Vehicles[0].WalkDistanceUnits,Live.GetSnapshot().Vehicles[0].WalkDistanceUnits);
		TestEqual(TEXT("Reload preserves authoritative arrival"),Reload.GetSnapshot().Vehicles[0].bCompleted,Live.GetSnapshot().Vehicles[0].bCompleted);
	}
	TestTrue(TEXT("Crossing eventually arrives"),Live.GetSnapshot().Vehicles[0].bCompleted);
	auto Bad=Read.Traffic;Bad.Vehicles[0].WalkDistanceUnits=MAX_int32;
	TestFalse(TEXT("Invalid path progress rejected"),Reload.RestoreState(Bad,N,1,Error));
	Bad=Read.Traffic;Bad.Vehicles[0].WalkPath[1].Z=1;
	TestFalse(TEXT("Unconnected road-level jump rejected"),Reload.RestoreState(Bad,N,1,Error));
	// Reverse journey must end on the same physical building side.
	const auto Back=Walker(City,N,East,South,12);
	TestTrue(TEXT("Return walk has same physical sidewalk"),!Back.WalkPath.IsEmpty() && Back.WalkPath[1].Y>8500);
	// All authored entrance rotations resolve to the physical frontage side.
	for(int32 Turn=0;Turn<4;++Turn)
	{
		FCityPlacementModel Rotated(100000);
		Road(Rotated,FCityPlacementModel::EntranceCell,{20,8});
		Road(Rotated,{10,8},{10,3});Road(Rotated,{10,8},{10,16});
		const FCityCellCoord Sites[]={{6,9},{9,5},{6,7},{11,12}};
		const uint64 Building=Lot(Rotated,Sites[Turn],1,Turn);
		auto* Catalog=NewObject<UCityBuildingCatalog>();FCityBuildingDefinition D;
		D.DefinitionId=TEXT("WalkLot");D.Footprint={1,1};D.EntranceSides=1;Catalog->Definitions.Add(D);
		FCityCellCoord Portal;
		TestTrue(TEXT("Rotated entrance finds its supported frontage"),FCityTrafficSimulation::FindPortal(Rotated,Catalog,Building,Portal));
		const FCityCellCoord Expected[]={ {6,8},{10,5},{6,8},{10,12} };
		TestTrue(TEXT("Portal honors authored rotation"),Portal==Expected[Turn]);
	}
	// A turn through a T junction is a perimeter walk with explicit crossings, never a diagonal cut.
	Road(City,{10,8},{10,16});const uint64 Branch=Lot(City,{11,13});
	FCityRoadNetworkBuilder::Build(City,FCityRoadDefinition::Local(),N);
	const auto Turning=Walker(City,N,South,Branch,15);
	TestTrue(TEXT("Junction route reaches a building around the corner"),!Turning.WalkPath.IsEmpty());
	for(int32 I=1;I<Turning.WalkPath.Num();++I)
		TestTrue(TEXT("Junction path has no diagonal road cuts"),Turning.WalkPath[I].X==Turning.WalkPath[I-1].X || Turning.WalkPath[I].Y==Turning.WalkPath[I-1].Y);
	Live.ReconcileNetwork(N);TestTrue(TEXT("Road edits cancel old paths without arrival"),Live.GetSnapshot().Vehicles.IsEmpty());
	City.DemolishEntity(Branch);
	TestTrue(TEXT("Missing destination cannot schedule a walker"),Walker(City,N,South,Branch).WalkPath.IsEmpty());
	// Sparse mid-block exceptions share one descriptor for opposing public destinations.
	FCityPlacementModel PublicCity(100000);Road(PublicCity,FCityPlacementModel::EntranceCell,{20,8});
	Lot(PublicCity,{7,9},15,0,TEXT("PrimarySchool"));Lot(PublicCity,{7,7},15,0,TEXT("Park"));
	Lot(PublicCity,{12,9});Lot(PublicCity,{12,7});
	const auto PublicCrossings=CityPedestrianPaths::Crossings(PublicCity,nullptr);
	TestEqual(TEXT("School and park sharing frontage share one crossing"),PublicCrossings.Num(),1);
	TestTrue(TEXT("Public crossing stays at its frontage"),PublicCrossings.Num()==1 && PublicCrossings[0].Cell==FCityCellCoord(7,8));
	FCityPlacementModel DeadEnd(100000);Road(DeadEnd,FCityPlacementModel::EntranceCell,{20,8});
	const auto A=Lot(DeadEnd,{6,9}),B=Lot(DeadEnd,{6,7});
	FCityRoadNetworkSnapshot DeadNetwork;FCityRoadNetworkBuilder::Build(DeadEnd,FCityRoadDefinition::Local(),DeadNetwork);
	TestTrue(TEXT("Street with no intersection can walk around actual road end"),!Walker(DeadEnd,DeadNetwork,A,B).WalkPath.IsEmpty());
	TestTrue(TEXT("Road-end walkaround adds no painted crossing"),CityPedestrianPaths::Crossings(DeadEnd,nullptr).IsEmpty());
	for(FName Type:{FName(TEXT("DirtRoad")),FName(TEXT("Avenue")),FName(TEXT("Highway"))})
	{
		FCityPlacementModel Other(100000);Road(Other,FCityPlacementModel::EntranceCell,{20,8},Type);
		Lot(Other,{6,9},15,0,TEXT("Park"));
		const auto X=CityPedestrianPaths::Crossings(Other,nullptr);
		if(Type==TEXT("Highway")) TestTrue(TEXT("Highways have no crossings"),X.IsEmpty());
		if(Type==TEXT("DirtRoad")) for(const auto& C:X) TestFalse(TEXT("Dirt crossings stay unpainted"),C.bPainted);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityCrosswalkGeometryTest,"CitySim.Roads.CrosswalkGeometry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityCrosswalkGeometryTest::RunTest(const FString&)
{
	const UWorld::InitializationValues Values=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
	UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,TEXT("CrosswalkGeometry"),nullptr,true,ERHIFeatureLevel::Num,&Values);
	ACityBoardActor* B=World->SpawnActor<ACityBoardActor>();
	auto* S=World->GetSubsystem<UCitySimulationSubsystem>();S->PlacementModel->SetTerrain(nullptr);
	B->SimulationSubsystem=S;B->PlacementModel=S->PlacementModel.Get();
	Road(*B->PlacementModel,FCityPlacementModel::EntranceCell,{20,8});
	Road(*B->PlacementModel,{10,8},{10,15});
	Lot(*B->PlacementModel,{6,9});Lot(*B->PlacementModel,{6,7});
	B->RebuildRoadInstances();
	const auto* Paint=B->RoadDetailSurface->GetProcMeshSection(2);
	if(TestNotNull(TEXT("Crosswalk paint is real batched mesh"),Paint))
	{
		TestTrue(TEXT("Zebra paint contains separate stripes"),Paint->ProcIndexBuffer.Num()>12);
		for(const auto& V:Paint->ProcVertexBuffer)
			TestTrue(TEXT("Paint follows actual supported surface"),FMath::IsNearlyEqual(double(V.Position.Z),B->PedestrianSurfaceHeight(V.Position,0)+1.,.02));
	}
	const auto Crossings=CityPedestrianPaths::Crossings(*B->PlacementModel,B->BuildingCatalog);
	int32 JunctionCrossings=0;for(const auto& X:Crossings) if(X.Cell==FCityCellCoord(10,8)) ++JunctionCrossings;
	TestEqual(TEXT("T junction has three approach crossings"),JunctionCrossings,3);
	for(const auto& X:Crossings) if(X.Cell==FCityCellCoord(10,8))
	{
		if(X.A.Y==X.B.Y) TestTrue(TEXT("Positive-Y branch has its crossing on positive-Y approach"),X.A.Y>8500);
	}
	// Opposite T orientation must mirror the approach, rather than testing only counts.
	FCityPlacementModel Mirror(100000);Road(Mirror,FCityPlacementModel::EntranceCell,{20,8});Road(Mirror,{10,8},{10,2});
	for(const auto& X:CityPedestrianPaths::Crossings(Mirror,nullptr)) if(X.Cell==FCityCellCoord(10,8) && X.A.Y==X.B.Y)
		TestTrue(TEXT("Negative-Y branch has its crossing on negative-Y approach"),X.A.Y<8500);

	// Sample actual mesh coverage in the former gaps, not just its vertex count.
	auto Covered=[&](FVector2D P)
	{
		const auto* Mesh=B->SidewalkSurface->GetProcMeshSection(0);
		if(!Mesh) return false;
		for(int32 I=0;I<Mesh->ProcIndexBuffer.Num();I+=3)
		{
			const FVector2D A(Mesh->ProcVertexBuffer[Mesh->ProcIndexBuffer[I]].Position);
			const FVector2D C(Mesh->ProcVertexBuffer[Mesh->ProcIndexBuffer[I+1]].Position);
			const FVector2D D(Mesh->ProcVertexBuffer[Mesh->ProcIndexBuffer[I+2]].Position);
			if(FMath::Abs(FVector2D::CrossProduct(C-A,D-A))<.001) continue;
			const double U=FVector2D::CrossProduct(C-A,P-A),V=FVector2D::CrossProduct(D-C,P-C),W=FVector2D::CrossProduct(A-D,P-D);
			if((U>=-.001 && V>=-.001 && W>=-.001)||(U<=.001 && V<=.001 && W<=.001)) return true;
		}
		return false;
	};
	auto CheckJoins=[&]()
	{
		const FCityCellCoord Junction(10,8);const auto Mask=B->PlacementModel->GetRoadNeighborMask(Junction);
		for(int32 Side=0;Side<4;++Side) if(Mask&(1<<Side)) for(double Flank:{-1.,1.})
		{
			const bool AlongX=Side==1 || Side==3;const double Sign=Side<2?1.:-1.;
			for(double T:{.475,.4999,.5001})
			{
				const double Along=.5+Sign*T;
				const double Across=.5+Flank*(B->GetRoadWidthAt(Junction,AlongX,FMath::Clamp(Along,0.,1.))*.5+B->GetRoadWidthAt(Junction,AlongX,FMath::Clamp(Along,0.,1.),true)*.5);
				TestTrue(TEXT("Junction sidewalk reaches both sides of shared tile boundary"),Covered(FVector2D(Junction.X+(AlongX?Along:Across),Junction.Y+(AlongX?Across:Along))*800.));
			}
		}
		const auto Descriptors=CityPedestrianPaths::Crossings(*B->PlacementModel,B->BuildingCatalog);
		const auto* Section=B->RoadDetailSurface->GetProcMeshSection(2);
		if(!TestNotNull(TEXT("Clipped junction paint exists"),Section)) return;
		for(const auto& Vertex:Section->ProcVertexBuffer)
		{
			const FVector P(Vertex.Position);const FCityCrosswalk* Closest=nullptr;double Best=MAX_dbl;
			for(const auto& X:Descriptors)
			{
				const FVector A=B->PedestrianPointToLocal(X.A),C=B->PedestrianPointToLocal(X.B);
				const double T=FMath::Clamp(FVector::DotProduct(P-A,C-A)/(C-A).SizeSquared(),0.,1.);
				const double Score=FVector::DistSquared2D(P,FMath::Lerp(A,C,T));
				if(Score<Best){Best=Score;Closest=&X;}
			}
			if(!Closest) continue;
			const bool AlongX=Closest->A.X==Closest->B.X;
			const double Along=AlongX?P.X/800.-Closest->Cell.X:P.Y/800.-Closest->Cell.Y;
			const double Across=AlongX?P.Y/800.-Closest->Cell.Y-.5:P.X/800.-Closest->Cell.X-.5;
			TestTrue(TEXT("Every zebra vertex remains inside actual tapered carriageway"),FMath::Abs(Across)<B->GetRoadWidthAt(Closest->Cell,AlongX,Along)*.5-.001);
			TestTrue(TEXT("Paint stays at road height, never lifted onto sidewalk"),FMath::IsNearlyEqual(P.Z,B->GetRoadProfileHeight(P.X/800.,P.Y/800.,Closest->Cell.RoadLevel)+FCityRoadRibbonMesh::SurfaceLiftCm+1.,.02));
		}
	};
	CheckJoins();
	Road(*B->PlacementModel,{10,8},{10,2});B->RebuildRoadInstances();CheckJoins();
	Road(*B->PlacementModel,{10,8},{20,8},TEXT("Avenue"));B->RebuildRoadInstances();CheckJoins();

	// Reproduce a long bent entrance: equal clock increments must cover equal
	// physical distances on the entrance, street and destination approach.
	FCityTrafficVehicleState Walk;Walk.OriginEntityId=901;Walk.DestinationEntityId=902;
	Walk.WalkPath={{6500,9400,0},{6500,8900,0},{7500,8900,0},{7500,9400,0}};
	FVector PublicA=B->PedestrianPointToLocal(Walk.WalkPath[1]),PublicB=B->PedestrianPointToLocal(Walk.WalkPath[2]);
	PublicA.Z=B->PedestrianSurfaceHeight(PublicA,0);PublicB.Z=B->PedestrianSurfaceHeight(PublicB,0);
	B->CitizenEntrancePaths.Add(901,{PublicA+FVector(-3000,1000,0),PublicA+FVector(-3000,0,0),PublicA});
	B->CitizenEntrancePaths.Add(902,{PublicB+FVector(2000,0,0),PublicB});
	auto Travel=[&](int32 From)
	{
		FVector A,BPoint,Heading;Walk.WalkDistanceUnits=From;
		B->SampleEntranceAwareWalker(Walk,nullptr,1.,A,Heading);Walk.WalkDistanceUnits=From+10;
		B->SampleEntranceAwareWalker(Walk,nullptr,1.,BPoint,Heading);return FVector::Distance(A,BPoint);
	};
	const double StreetTravel=Travel(1300);
	TestTrue(TEXT("Long bent origin approach has same speed as street"),FMath::Abs(Travel(10)-StreetTravel)<1.);
	TestTrue(TEXT("Long destination approach has same speed as street"),FMath::Abs(Travel(1980)-StreetTravel)<1.);
	FVector At,Heading;Walk.WalkDistanceUnits=0;B->SampleEntranceAwareWalker(Walk,nullptr,1.,At,Heading);
	TestTrue(TEXT("Retimed path begins at exact doorway"),FVector::Dist2D(At,B->CitizenEntrancePaths[901][0])<.01);
	Walk.WalkDistanceUnits=CityPedestrianPaths::Length(Walk.WalkPath);B->SampleEntranceAwareWalker(Walk,nullptr,1.,At,Heading);
	TestTrue(TEXT("Retimed path reaches exact destination at arrival"),FVector::Dist2D(At,B->CitizenEntrancePaths[902][0])<.01);

	// The generated walkway exits ahead of the graph's original portal. Walking
	// along the frontage must never reverse to visit that obsolete start point.
	B->CitizenEntrancePaths.Add(901,{PublicA+FVector(200,1500,0),PublicA+FVector(200,0,0)});
	FVector Previous;Walk.WalkDistanceUnits=0;B->SampleEntranceAwareWalker(Walk,nullptr,1.,Previous,Heading);
	for(int32 D=10;D<=CityPedestrianPaths::Length(Walk.WalkPath);D+=10)
	{
		Walk.WalkDistanceUnits=D;B->SampleEntranceAwareWalker(Walk,nullptr,1.,At,Heading);
		TestTrue(TEXT("Offset departure never loops backwards along frontage"),At.X>=Previous.X-1.);
		Previous=At;
	}

	World->DestroyWorld(false);return true;
}
#endif
