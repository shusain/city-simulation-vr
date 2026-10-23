#include "Presentation/CityBoardActor.h"
#include "Simulation/CityPedestrianPaths.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Presentation/CityRoadPresentation.h"
#include "Presentation/CityRoadRibbon.h"
#include "Presentation/CityCitizenPresentation.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/InstancedStaticMeshComponent.h"

FVector ACityBoardActor::PedestrianPointToLocal(const FIntVector& P) const
{
	const FCityCellCoord Cell(P.X/1000,P.Y/1000,P.Z);
	const uint8 Mask=PlacementModel->GetRoadNeighborMask(Cell);
	double X=P.X/1000.,Y=P.Y/1000.;
	const bool AlongX=Mask==2 || Mask==8 || Mask==10;
	const bool AlongY=Mask==1 || Mask==4 || Mask==5;
	if(AlongX || AlongY)
	{
		const double Along=AlongX?X-Cell.X:Y-Cell.Y;
		const double Actual=(GetRoadWidthAt(Cell,AlongX,Along)+GetRoadWidthAt(Cell,AlongX,Along,true))*.5;
		const double Nominal=CityRoadPresentation::SidewalkOffset(PlacementModel->GetRoadDefinitionId(Cell));
		if(AlongX) Y=Cell.Y+.5+(Y-Cell.Y-.5)*Actual/Nominal;
		else X=Cell.X+.5+(X-Cell.X-.5)*Actual/Nominal;
	}
	return FVector(X*800.,Y*800.,GetRoadProfileHeight(X,Y,P.Z));
}

double ACityBoardActor::PedestrianSurfaceHeight(const FVector& P,int32 Level) const
{
	const FCityCellCoord Cell(FMath::FloorToInt(P.X/800.),FMath::FloorToInt(P.Y/800.),Level);
	const auto Mask=PlacementModel->GetRoadNeighborMask(Cell);
	const double X=P.X/800.-Cell.X-.5,Y=P.Y/800.-Cell.Y-.5;
	const bool AlongX=Mask==2 || Mask==8 || Mask==10;
	const bool Straight=AlongX || Mask==1 || Mask==4 || Mask==5;
	const double Width=Straight?GetRoadWidthAt(Cell,AlongX,(AlongX?X:Y)+.5):CityRoadPresentation::SurfaceWidth(PlacementModel->GetRoadDefinitionId(Cell));
	// At junctions the open approaches are carriageway; the closed edges are sidewalk.
	bool Walk=false;
	if(Straight) Walk=FMath::Abs(AlongX?Y:X)>Width*.5;
	else
	{
		const double Half=Width*.5;
		bool Carriageway=FMath::Abs(X)<=Half && FMath::Abs(Y)<=Half;
		for(int32 Side=0;Side<4;++Side) if(Mask&(1<<Side))
		{
			const bool BranchAlongX=Side==1 || Side==3;const double Sign=Side<2?1.:-1.;
			const double Along=BranchAlongX?X:Y,Across=BranchAlongX?Y:X;
			if(Sign*Along>=Half && FMath::Abs(Across)<=GetRoadWidthAt(Cell,BranchAlongX,Along+.5)*.5) Carriageway=true;
		}
		Walk=!Carriageway;
	}
	return GetRoadProfileHeight(P.X/800.,P.Y/800.,Level)+
	    (Walk?FCityRoadRibbonMesh::SidewalkLiftCm:FCityRoadRibbonMesh::SurfaceLiftCm);
}

void ACityBoardActor::RebuildCrosswalkPaint()
{
	if(!PlacementModel || !SimulationSubsystem || !RoadDetailSurface) return;
	RoadDetailSurface->ClearMeshSection(2);
	FCityRoadRibbonMesh Paint;Paint.Terrain=&SimulationSubsystem->GetTerrain();
	for(const auto& X:CityPedestrianPaths::Crossings(*PlacementModel,BuildingCatalog))
	{
		if(!X.bPainted) continue;
		const FVector A=PedestrianPointToLocal(X.A),B=PedestrianPointToLocal(X.B);
		const FVector Direction=(B-A).GetSafeNormal2D();
		const FVector Across(-Direction.Y,Direction.X,0);
		const double Length=FVector::Dist2D(A,B);
		const bool RoadAlongX=FMath::Abs(Direction.Y)>.5;
		auto Along=[&](const FVector2D& P){return RoadAlongX?P.X-X.Cell.X:P.Y-X.Cell.Y;};
		auto AcrossCoordinate=[&](const FVector2D& P){return RoadAlongX?P.Y-X.Cell.Y-.5:P.X-X.Cell.X-.5;};
		auto Clip=[](const TArray<FVector2D>& Input,TFunctionRef<double(const FVector2D&)> Distance)
		{
			TArray<FVector2D> Out;
			for(int32 I=0;I<Input.Num();++I)
			{
				const auto P=Input[I],Q=Input[(I+1)%Input.Num()];const double DP=Distance(P),DQ=Distance(Q);
				if(DP>=0.) Out.Add(P);
				if((DP>=0.)!=(DQ>=0.)) Out.Add(FMath::Lerp(P,Q,DP/(DP-DQ)));
			}
			return Out;
		};
		// Width profiles are piecewise linear; split at every profile break so
		// stripe polygons remain entirely inside the carriageway, including tapers.
		TArray<double> Breaks={0.,.5,1.};
		const auto Mask=PlacementModel->GetRoadNeighborMask(X.Cell);
		if(FMath::CountBits(static_cast<uint64>(Mask))>=2 && Mask!=5 && Mask!=10)
		{
			const double Half=CityRoadPresentation::SurfaceWidth(PlacementModel->GetRoadDefinitionId(X.Cell))*.5;
			Breaks.Add(.5-Half);Breaks.Add(.5+Half);Breaks.Sort();
		}
		for(double D=30.;D+32.<Length-30.;D+=60.)
		{
			const FVector Center=A+Direction*(D+16.);
			TArray<FVector2D> Polygon;
			for(const auto& Corner:{FVector2D(-1,-1),FVector2D(1,-1),FVector2D(1,1),FVector2D(-1,1)})
			{
				const FVector P=Center+Direction*(Corner.X*16.)+Across*(Corner.Y*32.);
				Polygon.Add(FVector2D(P.X,P.Y)/800.);
			}
			for(int32 I=1;I<Breaks.Num();++I)
			{
				const double Low=Breaks[I-1],High=Breaks[I];
				const double W0=GetRoadWidthAt(X.Cell,RoadAlongX,Low)*.5-2./800.;
				const double W1=GetRoadWidthAt(X.Cell,RoadAlongX,High)*.5-2./800.;
				auto HalfWidth=[&](const FVector2D& P){return FMath::Lerp(W0,W1,(Along(P)-Low)/(High-Low));};
				auto Clipped=Clip(Polygon,[&](const FVector2D& P){return Along(P)-Low;});
				Clipped=Clip(Clipped,[&](const FVector2D& P){return High-Along(P);});
				Clipped=Clip(Clipped,[&](const FVector2D& P){return HalfWidth(P)-AcrossCoordinate(P);});
				Clipped=Clip(Clipped,[&](const FVector2D& P){return HalfWidth(P)+AcrossCoordinate(P);});
				Paint.AppendPolygon(Clipped,[&](double U,double V){return GetRoadProfileHeight(U,V,X.Cell.RoadLevel)+FCityRoadRibbonMesh::SurfaceLiftCm;},1.);
			}
		}
	}
	if(!Paint.Vertices.IsEmpty())
	{
		RoadDetailSurface->CreateMeshSection(2,Paint.Vertices,Paint.Triangles,Paint.Normals,Paint.UVs,TArray<FColor>(),TArray<FProcMeshTangent>(),false);
		auto* Material=Cast<UMaterialInstanceDynamic>(RoadDetailSurface->GetMaterial(2));
		if(!Material) Material=UMaterialInstanceDynamic::Create(RoadMarkingInstances->GetMaterial(0),this);
		Material->SetVectorParameterValue(TEXT("CityColor"),FLinearColor(.95,.95,.95,1.));
		RoadDetailSurface->SetMaterial(2,Material);
	}
}

bool ACityBoardActor::SampleEntranceAwareWalker(const FCityTrafficVehicleState& V,const FCityTrafficVehicleState* Before,
    double Alpha,FVector& Location,FVector& Direction) const
{
	double Distance=V.WalkDistanceUnits;
	if(Before && Before->WalkPath==V.WalkPath && Before->bReturning==V.bReturning)
		Distance=FMath::Lerp(double(Before->WalkDistanceUnits),Distance,FMath::Clamp(Alpha,0.,1.));
	if(V.WalkPath.Num()<2) return false;
	const uint64 Origin=V.bReturning?V.DestinationEntityId:V.OriginEntityId;
	const uint64 Destination=V.bReturning?V.OriginEntityId:V.DestinationEntityId;
	const auto* Start=CitizenEntrancePaths.Find(Origin);
	const auto* End=CitizenEntrancePaths.Find(Destination);
	TArray<FVector> RoadPoints;
	for(const auto& P:V.WalkPath) RoadPoints.Add(PedestrianPointToLocal(P));
	// Offset authored exits may already be ahead along the first sidewalk run.
	// Join that run at the exit instead of walking back to its old graph start.
	// Stop at its first turn/crossing; never skip a carriageway crossing.
	auto TrimJoin=[&](const TArray<FVector>* Authored,bool Reverse)
	{
		if(!Authored || Authored->Num()<2 || RoadPoints.Num()<4) return;
		const int32 Step=Reverse?-1:1,StartIndex=Reverse?RoadPoints.Num()-2:1;
		const FVector Exit=Authored->Last();FVector Forward;
		double Best=FVector::DistSquared2D(Exit,RoadPoints[StartIndex]);
		FVector Join=RoadPoints[StartIndex];int32 JoinIndex=StartIndex;
		for(int32 I=StartIndex;I+Step>=1 && I+Step<RoadPoints.Num()-1;I+=Step)
		{
			const FVector A=RoadPoints[I],B=RoadPoints[I+Step],Delta=B-A;
			const FVector Heading=Delta.GetSafeNormal2D();
			if(Heading.IsNearlyZero() || V.WalkPath[I].Z!=V.WalkPath[I+Step].Z) break;
			const FVector Probe=(A+B)*.5;
			if(PedestrianSurfaceHeight(Probe,V.WalkPath[I].Z)<GetRoadProfileHeight(Probe.X/800.,Probe.Y/800.,V.WalkPath[I].Z)+FCityRoadRibbonMesh::SidewalkLiftCm-.1) break;
			if(Forward.IsNearlyZero()) Forward=Heading;
			if(FVector::DotProduct(Forward,Heading)<.999) break;
			const double T=FMath::Clamp(FVector::DotProduct(Exit-A,Delta)/Delta.SizeSquared(),0.,1.);
			const FVector Candidate=FMath::Lerp(A,B,T);const double Score=FVector::DistSquared2D(Exit,Candidate);
			if(Score<Best && Score<=FMath::Square(100.)) {Best=Score;Join=Candidate;JoinIndex=I;}
		}
		for(int32 I=StartIndex;;I+=Step) {RoadPoints[I]=Join;if(I==JoinIndex) break;}
	};
	TrimJoin(Start,false);TrimJoin(End,true);
	auto ApproachPoints=[&](bool First)
	{
		const auto* Authored=First?Start:End;
		TArray<FVector> Points=Authored?*Authored:TArray<FVector>();
		FVector Public=First?RoadPoints[1]:RoadPoints[RoadPoints.Num()-2];
		Public.Z=PedestrianSurfaceHeight(Public,0);
		Points.Add(Public);return Points;
	};
	// The simulation owns the journey clock. Distribute that clock over the full
	// physical arc length, rather than squeezing any long bent apron into 500 units.
	// Both doorway endpoints still coincide with authoritative departure/arrival.
	TArray<double> PhysicalLengths,LogicalLengths;
	double PhysicalTotal=0.;
	for(int32 I=1;I<V.WalkPath.Num();++I)
	{
		const bool First=I==1,Last=I==V.WalkPath.Num()-1;
		double Length=0.;
		if((First && Start && Start->Num()>1)||(Last && End && End->Num()>1))
		{
			const auto Points=ApproachPoints(First);
			for(int32 J=1;J<Points.Num();++J) Length+=FVector::Distance(Points[J-1],Points[J]);
		}
		else Length=FVector::Distance(RoadPoints[I-1],RoadPoints[I]);
		PhysicalLengths.Add(Length);PhysicalTotal+=Length;
		LogicalLengths.Add(FMath::Max(1,FMath::RoundToInt(FVector2D(V.WalkPath[I].X-V.WalkPath[I-1].X,V.WalkPath[I].Y-V.WalkPath[I-1].Y).Size())));
	}
	double Remaining=FMath::Clamp(Distance/double(CityPedestrianPaths::Length(V.WalkPath)),0.,1.)*PhysicalTotal;
	Distance=0.;
	for(int32 I=0;I<PhysicalLengths.Num();++I)
	{
		if(Remaining<=PhysicalLengths[I] || I==PhysicalLengths.Num()-1)
		{
			Distance+=LogicalLengths[I]*FMath::Clamp(Remaining/FMath::Max(.001,PhysicalLengths[I]),0.,1.);break;
		}
		Remaining-=PhysicalLengths[I];Distance+=LogicalLengths[I];
	}
	FVector Grid;int32 Segment=0;double Fraction=0;
	if(!CityPedestrianPaths::Sample(V.WalkPath,Distance,Grid,Direction,&Segment,&Fraction)) return false;
	auto Entrance=[&](const TArray<FVector>& Points,double T,bool Reverse)
	{
		double Total=0.;for(int32 I=1;I<Points.Num();++I) Total+=FVector::Distance(Points[I-1],Points[I]);
		double Left=(Reverse?1.-T:T)*Total;
		for(int32 I=1;I<Points.Num();++I)
		{
			const double L=FVector::Distance(Points[I-1],Points[I]);
			if(Left<=L || I==Points.Num()-1)
			{
				Location=FMath::Lerp(Points[I-1],Points[I],FMath::Clamp(Left/FMath::Max(.001,L),0.,1.));
				Direction=(Points[I]-Points[I-1]).GetSafeNormal2D()*(Reverse?-1.:1.);
				return;
			}
			Left-=L;
		}
	};
	const bool First=Segment==0,Last=Segment==V.WalkPath.Num()-2;
	if((First && Start && Start->Num()>1)||(Last && End && End->Num()>1))
	{
		// Join the authored apron to the exact graph point along its physical sidewalk.
		// Never move a crossing endpoint to the doorway's lateral position.
		const auto Approach=ApproachPoints(First);
		Entrance(Approach,Fraction,Last);
		Location.Z+=CityCitizenPresentation::StreetOriginOffsetCm-FCityRoadRibbonMesh::SidewalkLiftCm;
		return true;
	}
	FVector A=RoadPoints[Segment];
	FVector B=RoadPoints[Segment+1];
	if(First && !Start)
	{
		const auto P=V.WalkPath[1]; const FVector Center((P.X/1000+.5)*800.,(P.Y/1000+.5)*800.,0);
		const FVector Out=(A-B).GetSafeNormal2D(); A=B+Out*FMath::Max(0.,400.-FMath::Abs(FVector::DotProduct(B-Center,Out)));
	}
	if(Last && !End)
	{
		const auto P=V.WalkPath[V.WalkPath.Num()-2];const FVector Center((P.X/1000+.5)*800.,(P.Y/1000+.5)*800.,0);
		const FVector Out=(B-A).GetSafeNormal2D(); B=A+Out*FMath::Max(0.,400.-FMath::Abs(FVector::DotProduct(A-Center,Out)));
	}
	Location=FMath::Lerp(A,B,Fraction);
	Direction=(B-A).GetSafeNormal2D();
	const double Track=FCityTrafficSimulation::SamplePedestrianOffsetCm(V,Before,Alpha,80.);
	int32 FirstStreet=1,LastStreet=V.WalkPath.Num()-3;
	while(FirstStreet<LastStreet && PhysicalLengths[FirstStreet]<.001) ++FirstStreet;
	while(LastStreet>FirstStreet && PhysicalLengths[LastStreet]<.001) --LastStreet;
	const double Taper=First?Fraction:Last?1.-Fraction:
	    FMath::Min(Segment==FirstStreet?FMath::Clamp(Fraction*4.,0.,1.):1.,Segment==LastStreet?FMath::Clamp((1.-Fraction)*4.,0.,1.):1.);
	Location+=FVector(-Direction.Y,Direction.X,0)*Track*Taper;
	Location.Z=PedestrianSurfaceHeight(Location,Grid.Z>0.?1:0)+CityCitizenPresentation::StreetOriginOffsetCm-FCityRoadRibbonMesh::SidewalkLiftCm;
	return true;
}
