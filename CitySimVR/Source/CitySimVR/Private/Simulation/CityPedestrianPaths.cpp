#include "Simulation/CityPedestrianPaths.h"
#include "Commands/CityPlacementModel.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Presentation/CityRoadPresentation.h"
#include "Algo/Reverse.h"

namespace
{
// Clockwise ring: north midpoint, NE, east midpoint, SE, south midpoint, SW, west midpoint, NW.
const FIntPoint Ring[] = {{0,-1},{1,-1},{1,0},{1,1},{0,1},{-1,1},{-1,0},{-1,-1}};
// Placement mask order is +Y, +X, -Y, -X, unlike the ring/entrance order.
const uint8 WalkRoadBits[] = {4,2,1,8};
const FIntPoint WalkDirections[] = {{0,-1},{1,0},{0,1},{-1,0}};
int32 Span(const FIntVector& A, const FIntVector& B)
{
	return FMath::Max(1, FMath::RoundToInt(FVector2D(B.X-A.X,B.Y-A.Y).Size()));
}
int32 Offset(const FCityPlacementModel& City, const FCityCellCoord& Cell)
{
	return FMath::RoundToInt(CityRoadPresentation::SidewalkOffset(City.GetRoadDefinitionId(Cell))*1000.);
}
FIntVector Point(const FCityPlacementModel& City, const FCityCellCoord& Cell, int32 Index)
{
	const int32 K=Offset(City,Cell);
	return {Cell.X*1000+500+Ring[Index].X*K,Cell.Y*1000+500+Ring[Index].Y*K,Cell.RoadLevel};
}
bool Portal(const FCityPlacementModel& City,const UCityBuildingCatalog* Catalog,uint64 Id,FCityCellCoord& Cell,int32& Side)
{
	if(!FCityTrafficSimulation::FindPortal(City,Catalog,Id,Cell)) return false;
	if(City.GetRoadDefinitionId(Cell)==TEXT("Highway")) return false;
	const auto* E=City.GetEntities().Find(Id);
	if(!E) {Side=0;return Id==0;}
	// Side is the physical side of the road facing the building.
	Side=Cell.Y<E->Anchor.Y?2:Cell.Y>=E->Anchor.Y+E->FootprintSize.Y?0:
	    Cell.X<E->Anchor.X?1:3;
	return true;
}
}

TArray<FCityCrosswalk> CityPedestrianPaths::Crossings(const FCityPlacementModel& City,const UCityBuildingCatalog* Catalog)
{
	TSet<FCityCellCoord> Frontages;
	for(const auto& Pair:City.GetEntities())
	{
		FCityCellCoord Cell;int32 Side;
		const auto Id=Pair.Value.DefinitionId;
		const auto* Definition=Catalog?Catalog->FindDefinition(Id):nullptr;
		const bool PublicDestination=FCityBuildingDefinition::IsSchoolFacility(Id) || Id==TEXT("Park") ||
		    (Definition && Definition->ParkRadiusCells>0);
		if(PublicDestination && Portal(City,Catalog,Pair.Key,Cell,Side)) Frontages.Add(Cell);
	}
	auto Cells=City.GetRoadCells().Array();
	Cells.Sort([](const auto& A,const auto& B){return A.Y!=B.Y?A.Y<B.Y:A.X!=B.X?A.X<B.X:A.RoadLevel<B.RoadLevel;});
	TArray<FCityCrosswalk> Result;
	for(const auto& Cell:Cells)
	{
		const auto Id=City.GetRoadDefinitionId(Cell);
		if(Id==TEXT("Highway")) continue;
		const uint8 Mask=City.GetRoadNeighborMask(Cell);
		const bool Junction=FMath::CountBits(static_cast<uint64>(Mask))>=3;
		if(Junction)
		{
			for(int32 Side=0;Side<4;++Side) if(Mask&WalkRoadBits[Side])
				Result.Add({Cell,Point(City,Cell,(Side*2+7)%8),Point(City,Cell,Side*2+1),Id!=TEXT("DirtRoad")});
		}
		else if(Frontages.Contains(Cell))
		{
			const bool AlongX=Mask==2 || Mask==8 || Mask==10;
			Result.Add({Cell,Point(City,Cell,AlongX?0:6),Point(City,Cell,AlongX?4:2),Id!=TEXT("DirtRoad")});
		}
	}
	return Result;
}

bool CityPedestrianPaths::Build(const FCityPlacementModel& City,const UCityBuildingCatalog* Catalog,
    const FCityRoadNetworkSnapshot& Network,const TArray<FCityCrosswalk>& Crosswalks,
    uint64 Origin,uint64 Destination,FCityTrafficVehicleState& Vehicle)
{
	FCityCellCoord Start,End;int32 StartSide,EndSide;
	if(!Portal(City,Catalog,Origin,Start,StartSide) || !Portal(City,Catalog,Destination,End,EndSide)) return false;
	// A valid crossing may lie beyond either frontage. Search the connected walking
	// component, rather than confining pedestrians to the shortest centre-road route.
	TMap<FCityCellCoord,int32> Reachable;
	Network.FindWalkingDistances(Start,Reachable);
	if(!Reachable.Contains(End)) return false;
	TArray<FCityCellCoord> Unique;Reachable.GetKeys(Unique);
	Unique.Sort([](const auto& A,const auto& B){return A.Y!=B.Y?A.Y<B.Y:A.X!=B.X?A.X<B.X:A.RoadLevel<B.RoadLevel;});
	TMap<FCityCellCoord,int32> Indices;
	for(int32 I=0;I<Unique.Num();++I) Indices.Add(Unique[I],I);
	TMap<FCityCellCoord,TArray<FCityCellCoord>> Connections;
	for(const auto& Edge:Network.Edges) if(EnumHasAllFlags(Edge.Access,ECityRoadAccess::Pedestrian))
		for(int32 I=1;I<Edge.Cells.Num();++I)
		{
			Connections.FindOrAdd(Edge.Cells[I-1]).AddUnique(Edge.Cells[I]);
			Connections.FindOrAdd(Edge.Cells[I]).AddUnique(Edge.Cells[I-1]);
		}
	struct FNode {FIntVector P;TArray<int32> Adj;};
	TArray<FNode> Nodes;Nodes.SetNum(Unique.Num()*8);
	TMap<FCityCellCoord,TArray<FCityCrosswalk>> ByCell;
	for(const auto& X:Crosswalks) ByCell.FindOrAdd(X.Cell).Add(X);
	auto Link=[&](int32 A,int32 B){Nodes[A].Adj.AddUnique(B);Nodes[B].Adj.AddUnique(A);};
	for(int32 C=0;C<Unique.Num();++C)
	{
		const auto Cell=Unique[C];const auto Mask=City.GetRoadNeighborMask(Cell);
		const auto* Local=ByCell.Find(Cell);
		for(int32 I=0;I<8;++I) Nodes[C*8+I].P=Point(City,Cell,I);
		for(int32 Side=0;Side<4;++Side)
		{
			const int32 Mid=Side*2,Before=(Mid+7)%8,After=Mid+1;
			const bool Open=(Mask&WalkRoadBits[Side])!=0;
			const bool Crossing=Local && Local->ContainsByPredicate([&](const auto& X){return
			    ((X.A==Nodes[C*8+Before].P && X.B==Nodes[C*8+After].P)||(X.B==Nodes[C*8+Before].P && X.A==Nodes[C*8+After].P));});
			if(!Open || Crossing) {Link(C*8+Before,C*8+Mid);Link(C*8+Mid,C*8+After);}
			const FCityCellCoord Next(Cell.X+WalkDirections[Side].X,Cell.Y+WalkDirections[Side].Y,Cell.RoadLevel);
			const int32* N=nullptr;
			if(const auto* Adjacent=Connections.Find(Cell)) for(const auto& Candidate:*Adjacent)
				if(Candidate.X==Next.X && Candidate.Y==Next.Y) {N=Indices.Find(Candidate);break;}
			if(Open && N)
			{
				// Join matching physical sidewalk edges across a cell boundary.
				Link(C*8+Before,*N*8+(Mid+5)%8);Link(C*8+After,*N*8+(Mid+3)%8);
			}
		}
		if(Local) for(const auto& X:*Local)
		{
			int32 A=INDEX_NONE,B=INDEX_NONE;
			for(int32 I=0;I<8;++I) {if(Nodes[C*8+I].P==X.A) A=C*8+I;if(Nodes[C*8+I].P==X.B) B=C*8+I;}
			if(A!=INDEX_NONE && B!=INDEX_NONE) Link(A,B);
		}
	}
	const int32 From=Indices[Start]*8+StartSide*2,To=Indices[End]*8+EndSide*2;
	TArray<int32> Dist,Prev;Dist.Init(MAX_int32,Nodes.Num());Prev.Init(INDEX_NONE,Nodes.Num());
	struct FQueue {int32 Cost,Id;bool operator<(const FQueue& B) const{return Cost==B.Cost?Id<B.Id:Cost<B.Cost;}};
	TArray<FQueue> Queue;Dist[From]=0;Queue.HeapPush({0,From});
	while(!Queue.IsEmpty())
	{
		FQueue Current;Queue.HeapPop(Current,EAllowShrinking::No);
		if(Current.Cost!=Dist[Current.Id]) continue;
		if(Current.Id==To) break;
		for(const int32 N:Nodes[Current.Id].Adj)
		{
			const int32 Cost=Current.Cost+Span(Nodes[Current.Id].P,Nodes[N].P);
			if(Cost<Dist[N]) {Dist[N]=Cost;Prev[N]=Current.Id;Queue.HeapPush({Cost,N});}
		}
	}
	if(Dist[To]==MAX_int32) return false;
	TArray<FIntVector> Path;
	for(int32 N=To;N!=INDEX_NONE;N=Prev[N]) Path.Add(Nodes[N].P);
	Algo::Reverse(Path);
	// Explicit entrance phases are timed too, including opposite buildings sharing one road portal.
	FIntVector DoorA=Path[0],DoorB=Path.Last();
	DoorA.X+=WalkDirections[StartSide].X*500;DoorA.Y+=WalkDirections[StartSide].Y*500;
	DoorB.X+=WalkDirections[EndSide].X*500;DoorB.Y+=WalkDirections[EndSide].Y*500;
	Path.Insert(DoorA,0);Path.Add(DoorB);
	Vehicle.WalkPath=MoveTemp(Path);Vehicle.WalkDistanceUnits=0;
	return true;
}

int32 CityPedestrianPaths::Length(const TArray<FIntVector>& Points)
{
	int32 Total=0;for(int32 I=1;I<Points.Num();++I) Total+=Span(Points[I-1],Points[I]);return Total;
}

bool CityPedestrianPaths::Sample(const TArray<FIntVector>& Points,double Distance,FVector& Position,FVector& Direction,int32* Segment,double* Fraction)
{
	if(Points.Num()<2) return false;
	for(int32 I=1;I<Points.Num();++I)
	{
		const int32 L=Span(Points[I-1],Points[I]);
		if(Distance<=L || I==Points.Num()-1)
		{
			const double T=FMath::Clamp(Distance/L,0.,1.);
			const auto A=Points[I-1],B=Points[I];
			Position=FVector(FMath::Lerp(double(A.X),double(B.X),T)/1000.,FMath::Lerp(double(A.Y),double(B.Y),T)/1000.,FMath::Lerp(double(A.Z),double(B.Z),T));
			Direction=FVector(B.X-A.X,B.Y-A.Y,0).GetSafeNormal();
			if(Segment) *Segment=I-1;if(Fraction) *Fraction=T;return true;
		}
		Distance-=L;
	}
	return false;
}

bool CityPedestrianPaths::HasRoom(const FCityTrafficVehicleState& Vehicle,int32 Distance,const TArray<FCityTrafficVehicleState>& Others)
{
	FVector P,D;int32 Segment=0;if(!Sample(Vehicle.WalkPath,Distance,P,D,&Segment)) return false;
	const bool Entrance=Segment==0 || Segment==Vehicle.WalkPath.Num()-2;
	for(const auto& Other:Others)
	{
		if(Other.Id==Vehicle.Id || Other.bCompleted || Other.WalkPath.IsEmpty()) continue;
		FVector Q,E;int32 OtherSegment=0;Sample(Other.WalkPath,Other.WalkDistanceUnits,Q,E,&OtherSegment);
		const bool SharedEntrance=Entrance && (OtherSegment==0 || OtherSegment==Other.WalkPath.Num()-2);
		if(!SharedEntrance && Other.PedestrianTrack!=Vehicle.PedestrianTrack) continue;
		if(P.Z==Q.Z && FVector::DistSquared2D(P,Q)<FMath::Square(.15) && (SharedEntrance || FVector::DotProduct(D,E)>.5)) return false;
	}
	return true;
}

void CityPedestrianPaths::UpdateRoadPosition(FCityTrafficVehicleState& V,const FCityRoadNetworkSnapshot& Network)
{
	FVector P,D;if(!Sample(V.WalkPath,V.WalkDistanceUnits,P,D)) return;
	P-=FVector(.5,.5,0);
	double Best=MAX_dbl;int32 BestLeg=V.RouteLegIndex,BestDistance=V.DistanceUnits;
	for(int32 L=0;L<V.Route.Num();++L)
	{
		const auto& Leg=V.Route[L];const auto& E=Network.Edges[Leg.EdgeId];
		for(int32 I=1;I<E.Cells.Num();++I)
		{
			const auto CA=E.Cells[Leg.bReverse?E.Cells.Num()-I:I-1];
			const auto CB=E.Cells[Leg.bReverse?E.Cells.Num()-I-1:I];
			const FVector A(CA.X,CA.Y,CA.RoadLevel),B(CB.X,CB.Y,CB.RoadLevel);
			const double T=FMath::Clamp(FVector::DotProduct(P-A,B-A)/(B-A).SizeSquared(),0.,1.);
			const int32 Lower=L==0?V.StartDistanceUnits:0;
			const int32 Upper=L==V.Route.Num()-1?V.EndDistanceUnits:FCityTrafficSimulation::GetEdgeLengthUnits(E);
			const int32 Distance=FMath::Clamp(FMath::RoundToInt((I-1+T)*1000.),Lower,Upper);
			const double Score=FVector::DistSquared(P,FMath::Lerp(A,B,T));
			if(Score<Best){Best=Score;BestLeg=L;BestDistance=Distance;}
		}
	}
	V.RouteLegIndex=BestLeg;V.DistanceUnits=BestDistance;
}
