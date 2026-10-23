#include "Presentation/CityHomeWalk.h"
#include "Presentation/CityTerrainSurface.h"
#include "Simulation/CityTerrain.h"
#include "Algo/Reverse.h"

void FCityHomeWalk::BuildFrontage(const FVector& Door, const FVector& Sidewalk, const FVector& Outward,
    const FCityTerrainHeightfield& Terrain, TFunctionRef<double(const FVector&)> SidewalkHeight,
    double WidthCm, double DoorWidthCm, double LandingCm, double GroundReserveCm)
{
    *this = FCityHomeWalk();
    const FVector F = Outward.GetSafeNormal2D();
    FVector R = FVector::CrossProduct(FVector::UpVector,F);
    if (FVector::DotProduct(Sidewalk-Door,R)<0) R = -R;
    const double EndX = FVector::DotProduct(Sidewalk-Door,R)-WidthCm*.5;
    const double EndY = FVector::DotProduct(Sidewalk-Door,F);
    const double StartX = DoorWidthCm*.5;
    if (EndY < WidthCm || EndX-StartX < 150.) return;
    auto At = [&](double X,double Y) {return Door+R*X+F*Y;};
    auto ApronHeight = [&](const FVector& P) {
        const double Y = FVector::DotProduct(P-Door,F);
        const FVector Public = P+F*(EndY-Y);
        const double Blend = FMath::Clamp(Y/EndY,0.,1.);
        return GroundHeight(Terrain,P)+FMath::Lerp(6.,SidewalkHeight(Public)-GroundHeight(Terrain,Public),Blend);
    };
    Build(At(StartX,WidthCm*.5),At(EndX,WidthCm*.5),Terrain,ApronHeight,LandingCm,GroundReserveCm,WidthCm);
    Centerline.Insert(Door, 0);
    FVector Turn=At(EndX+WidthCm*.5,WidthCm*.5);Turn.Z=ApronHeight(Turn);Centerline.Add(Turn);
    for(double Y=WidthCm*.5+25.;Y<EndY;Y+=25.) {FVector P=At(EndX+WidthCm*.5,Y);P.Z=ApronHeight(P);Centerline.Add(P);}
    Centerline.Add(Sidewalk);
    auto Face = [&](TArray<FVector> P,const FVector& N) {
        for(int32 I=P.Num()-1;I>=0;--I)
            if(P.Num()>1 && P[I].Equals(P[(I+1)%P.Num()],.0001)) P.RemoveAt(I);
        if(P.Num()<3) return;
        while(P.Num()>3 && FVector::CrossProduct(P[1]-P[0],P[2]-P[0]).SizeSquared()<.00001) P.RemoveAt(1);
        if(FVector::CrossProduct(P[1]-P[0],P[2]-P[0]).SizeSquared()<.00001) return;
        if(FVector::DotProduct(FVector::CrossProduct(P[1]-P[0],P[2]-P[0]),N)>0) Algo::Reverse(P);
        const int32 Base=Vertices.Num();
        for(const auto& V:P) {Vertices.Add(V);Normals.Add(N);UVs.Add(FVector2D(V.X,V.Y)/100.);}
        for(int32 I=1;I+1<P.Num();++I)
            if(FMath::Abs(FVector::DotProduct(FVector::CrossProduct(P[I]-P[0],P[I+1]-P[0]),N))>=.00001)
                Triangles.Append({Base,Base+I,Base+I+1});
    };
    // Door landing is level and touches the authored threshold. Its walls
    // descend below terrain, independently of the building foundation.
    const FVector A=At(-StartX,0),B=At(StartX,0),C=At(StartX,WidthCm),D=At(-StartX,WidthCm);
    Face({A,B,C,D},FVector::UpVector);
    const FBox LandingBounds(TArray<FVector>{A,B,C,D});
    double Bottom=Door.Z-10;
    for(int32 Y=FMath::FloorToInt(LandingBounds.Min.Y/400.);Y<=FMath::CeilToInt(LandingBounds.Max.Y/400.);++Y)
        for(int32 X=FMath::FloorToInt(LandingBounds.Min.X/400.);X<=FMath::CeilToInt(LandingBounds.Max.X/400.);++X)
            Bottom=FMath::Min(Bottom,static_cast<double>(Terrain.GetSampleHeightCm(X,Y))-10.);
    for(const auto& Edge:TArray<TPair<FVector,FVector>>{{A,D},{D,C}}) {
        FVector LowA=Edge.Key,LowB=Edge.Value;LowA.Z=LowB.Z=Bottom;
        Face({Edge.Key,Edge.Value,LowB,LowA},Edge.Key.Equals(A)?-R:F);
    }
    // Ground apron is clipped to the same fine-grid triangles as the terrain.
    // It touches the flight at x=EndX and the actual public edge at y=EndY.
    auto Clip=[](TArray<FVector> P,TFunctionRef<double(const FVector&)> Distance) {
        TArray<FVector> Out;
        for(int32 I=0;I<P.Num();++I) {
            const FVector V=P[I],W=P[(I+1)%P.Num()];const double DV=Distance(V),DW=Distance(W);
            if(DV>=-.0001) Out.Add(V);
            if((DV>0 && DW<0)||(DV<0 && DW>0)) Out.Add(FMath::Lerp(V,W,DV/(DV-DW)));
        } return Out;
    };
    const FBox Bounds(TArray<FVector>{At(EndX,0),At(EndX+WidthCm,0),At(EndX,EndY),At(EndX+WidthCm,EndY)});
    for(int32 Y=FMath::FloorToInt(Bounds.Min.Y/400.);Y<=FMath::FloorToInt(Bounds.Max.Y/400.);++Y)
        for(int32 X=FMath::FloorToInt(Bounds.Min.X/400.);X<=FMath::FloorToInt(Bounds.Max.X/400.);++X) {
            FCityTerrainSurface::Facets(Terrain,X,Y,[&](const FVector& V0,const FVector& V1,const FVector& V2) {
                TArray<FVector> P{V0,V1,V2};
                P=Clip(P,[&](const FVector& V){return FVector::DotProduct(V-Door,R)-EndX;});
                P=Clip(P,[&](const FVector& V){return EndX+WidthCm-FVector::DotProduct(V-Door,R);});
                P=Clip(P,[&](const FVector& V){return FVector::DotProduct(V-Door,F);});
                P=Clip(P,[&](const FVector& V){return EndY-FVector::DotProduct(V-Door,F);});
                for(auto& V:P) V.Z=ApronHeight(V);
                Face(P,FVector::UpVector);
                for(int32 I=0;I<P.Num();++I)
                {
                    const FVector V=P[I],W=P[(I+1)%P.Num()];
                    const double VX=FVector::DotProduct(V-Door,R),WX=FVector::DotProduct(W-Door,R);
                    const double VY=FVector::DotProduct(V-Door,F),WY=FVector::DotProduct(W-Door,F);
                    FVector N=FVector::ZeroVector;
                    if(FMath::IsNearlyEqual(VX,WX,.01) && FMath::IsNearlyEqual(VX,EndX,.01)) N=-R;
                    if(FMath::IsNearlyEqual(VX,WX,.01) && FMath::IsNearlyEqual(VX,EndX+WidthCm,.01)) N=R;
                    if(FMath::Abs(VY)<.01 && FMath::Abs(WY)<.01) N=-F;
                    if(N.IsNearlyZero()) continue;
                    FVector LowV=V,LowW=W;LowV.Z=GroundHeight(Terrain,V)-10.;LowW.Z=GroundHeight(Terrain,W)-10.;
                    Face({V,W,LowW,LowV},N);
                }
            });
        }
    if(ActualStepCount<=4) return;
    // Guard the elevated top landing, leaving the door and flight opening
    // clear. Corner horizontals butt together; posts stop below the rail.
    auto RailFace=[&](TArray<FVector> P,const FVector& N) {
        if(FVector::DotProduct(FVector::CrossProduct(P[1]-P[0],P[2]-P[0]),N)>0) Algo::Reverse(P);
        const int32 Base=RailVertices.Num();
        for(const auto& V:P) {RailVertices.Add(V);RailNormals.Add(N);RailUVs.Add(FVector2D(V.X,V.Z)/100.);}
        RailTriangles.Append({Base,Base+1,Base+2,Base,Base+2,Base+3});
    };
    auto RailBox=[&](double X0,double X1,double Y0,double Y1,double Z0,double Z1) {
        auto P=[&](double X,double Y,double Z){auto V=At(X,Y);V.Z=Z;return V;};
        RailFace({P(X0,Y0,Z1),P(X1,Y0,Z1),P(X1,Y1,Z1),P(X0,Y1,Z1)},FVector::UpVector);
        RailFace({P(X0,Y0,Z0),P(X0,Y1,Z0),P(X1,Y1,Z0),P(X1,Y0,Z0)},-FVector::UpVector);
        for(const double X:{X0,X1}) RailFace({P(X,Y0,Z0),P(X,Y1,Z0),P(X,Y1,Z1),P(X,Y0,Z1)},R*(X==X0?-1.:1.));
        for(const double Y:{Y0,Y1}) RailFace({P(X0,Y,Z0),P(X1,Y,Z0),P(X1,Y,Z1),P(X0,Y,Z1)},F*(Y==Y0?-1.:1.));
    };
    RailBox(-StartX+3,StartX,WidthCm-7,WidthCm-3,Door.Z+86,Door.Z+90);
    RailBox(-StartX+3,-StartX+7,5,WidthCm-7,Door.Z+86,Door.Z+90);
    const int32 Posts=FMath::CeilToInt((DoorWidthCm-10)/75.);
    for(int32 I=0;I<=Posts;++I) {
        const double X=FMath::Lerp(-StartX+5,StartX-2,I/static_cast<double>(Posts));
        RailBox(X-2,X+2,WidthCm-7,WidthCm-3,Door.Z-1,Door.Z+86);
    }
    RailBox(-StartX+3,-StartX+7,5,9,Door.Z-1,Door.Z+86);
}

double FCityHomeWalk::GroundHeight(const FCityTerrainHeightfield& T, const FVector& P)
{
	return FCityTerrainSurface::Height(T, P.X / 800., P.Y / 800.);
}

void FCityHomeWalk::Build(const FVector& Door, const FVector& Sidewalk, const FCityTerrainHeightfield& Terrain,
                          TFunctionRef<double(const FVector&)> SidewalkHeight, const double LandingCm, const double GroundReserveCm, const double WidthCm)
{
	Centerline.Reset(); Vertices.Reset(); Normals.Reset(); Triangles.Reset(); UVs.Reset(); StepCount = 0; ActualStepCount = 0;
	RailVertices.Reset(); RailNormals.Reset(); RailTriangles.Reset(); RailUVs.Reset();
	const FVector Forward = (Sidewalk - Door).GetSafeNormal2D();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);
	const double Length = FVector::Dist2D(Door, Sidewalk);
	const double HalfWidth = WidthCm * .5;
	constexpr double Lift = 6.;
	const double Landing = FMath::Max(10., LandingCm), Reserve = FMath::Max(40., GroundReserveCm);
	if (Length < 150) return;
	auto Point = [&](double Along, double Across) { return Door + Forward * Along + Right * Across; };
	auto Face = [&](TArray<FVector> Points, const FVector& Normal) {
		for(int32 I=Points.Num()-1;I>=0;--I)
            if(Points.Num()>1 && Points[I].Equals(Points[(I+1)%Points.Num()],.0001)) Points.RemoveAt(I);
        if(Points.Num()<3) return;
        while(Points.Num()>3 && FMath::Abs(FVector::DotProduct(FVector::CrossProduct(Points[1]-Points[0],Points[2]-Points[0]),Normal))<.00001) Points.RemoveAt(1);
		if (FVector::CrossProduct(Points[1] - Points[0], Points[2] - Points[0]).SizeSquared() < .00001) return;
		const int32 Base = Vertices.Num();
		// Unreal visible upper/exterior faces use clockwise winding.
		if (FVector::DotProduct(FVector::CrossProduct(Points[1] - Points[0], Points[2] - Points[0]), Normal) > 0)
			Algo::Reverse(Points);
		for (const auto& P : Points) { Vertices.Add(P); Normals.Add(Normal); UVs.Add(FVector2D(P.X, P.Y) / 100.); }
		for (int32 I = 1; I + 1 < Points.Num(); ++I)
            if(FMath::Abs(FVector::DotProduct(FVector::CrossProduct(Points[I]-Points[0],Points[I+1]-Points[0]),Normal))>=.00001)
                Triangles.Append({Base, Base + I, Base + I + 1});
	};
	// Select nominal18cm risers against the highest ground
	// across the landing. Each tread is solid down to terrain, never a flying
	// ribbon. On high ground the walk joins the doorstep directly.
	double Run = Landing;
	for (int32 I = 0; I < 12; ++I)
	{
		const FVector P = Point(Run, 0);
		const double Ground = FMath::Max3(GroundHeight(Terrain, P), GroundHeight(Terrain, P - Right * HalfWidth), GroundHeight(Terrain, P + Right * HalfWidth)) + Lift;
		const int32 Count = FMath::Clamp(FMath::CeilToInt((Door.Z - Ground) / 18.), 0, 16);
		const double NewRun = FMath::Min(Length - Reserve, Landing + Count * 28.);
		StepCount = Count;
		if (FMath::Abs(NewRun - Run) < .1) break;
		Run = (Run + NewRun) * .5;
	}
	Run = FMath::Min(Length - Reserve, Landing + StepCount * 28.);
	const double Tread = StepCount ? (Run - Landing) / StepCount : 0.;
	const double EndGround = FMath::Max3(GroundHeight(Terrain, Point(Run, 0)), GroundHeight(Terrain, Point(Run, -HalfWidth)), GroundHeight(Terrain, Point(Run, HalfWidth))) + Lift;
	const double Rise = StepCount ? FMath::Max(0., Door.Z - EndGround) / StepCount : 0.;
	TArray<double> TreadHeights, TreadBottoms;
	for (int32 Step = 0; Step <= StepCount; ++Step)
	{
		const double A = Step == 0 ? 0 : Landing + (Step - 1) * Tread;
		const double B = Step == 0 ? Landing : Landing + Step * Tread;
		const TArray<FVector> TreadCorners = {Point(A,-HalfWidth), Point(A,HalfWidth), Point(B,-HalfWidth), Point(B,HalfWidth)};
		const FBox TreadBounds(TreadCorners);
		double Z = Door.Z - Step * Rise;
		double Low = MAX_dbl;
		auto Sample = [&](double X, double Y) {
			if (X < TreadBounds.Min.X - .001 || X > TreadBounds.Max.X + .001 || Y < TreadBounds.Min.Y - .001 || Y > TreadBounds.Max.Y + .001) return;
			const double H = GroundHeight(Terrain,FVector(X,Y,0)); Low = FMath::Min(Low,H); Z = FMath::Max(Z,H+Lift);
		};
		for (const auto& P : TreadCorners) Sample(P.X,P.Y);
		// Extrema of a clipped piecewise-linear terrain occur at rectangle
		// corners or intersections with grid edges/diagonals. Include all of
		// them so a narrow tread cannot straddle an unmeasured crest or hollow.
		// Accepted bank rendering can refine a400cm cell to100cm facets.
        // Include those edge/diagonal intersections in the tread extrema too.
		for (int32 Y = FMath::FloorToInt(TreadBounds.Min.Y / 100.); Y <= FMath::CeilToInt(TreadBounds.Max.Y / 100.); ++Y)
		{
			Sample(TreadBounds.Min.X,Y*100.); Sample(TreadBounds.Max.X,Y*100.);
			for (int32 X = FMath::FloorToInt(TreadBounds.Min.X / 100.); X <= FMath::CeilToInt(TreadBounds.Max.X / 100.); ++X)
			{
				Sample(X*100.,Y*100.); Sample(X*100.,TreadBounds.Min.Y); Sample(X*100.,TreadBounds.Max.Y);
			}
		}
		for (int32 K = FMath::FloorToInt((TreadBounds.Min.X+TreadBounds.Min.Y)/100.); K <= FMath::CeilToInt((TreadBounds.Max.X+TreadBounds.Max.Y)/100.); ++K)
		{
			Sample(TreadBounds.Min.X,K*100.-TreadBounds.Min.X); Sample(TreadBounds.Max.X,K*100.-TreadBounds.Max.X);
			Sample(K*100.-TreadBounds.Min.Y,TreadBounds.Min.Y); Sample(K*100.-TreadBounds.Max.Y,TreadBounds.Max.Y);
		}
		TreadHeights.Add(Z); TreadBottoms.Add(Low-10);
	}
	// A crest under a tread must not cut through it or create an uphill step
	// halfway down the staircase. Merge those treads into a longer landing.
	for (int32 I = TreadHeights.Num() - 2; I >= 0; --I) TreadHeights[I] = FMath::Max(TreadHeights[I], TreadHeights[I+1]);
    for (int32 I=0; I<TreadHeights.Num(); ++I)
        if(TreadHeights[I]-(I+1<TreadHeights.Num()?TreadHeights[I+1]:EndGround)>.1) ++ActualStepCount;
    if(ActualStepCount>4)
    {
        // Continuous rectangular handrails have shared joins and only two end
        // caps; posts stop at the underside rather than overlapping a cap.
        auto RailFace=[&](TArray<FVector> P,const FVector& N) {
            if(FVector::DotProduct(FVector::CrossProduct(P[1]-P[0],P[2]-P[0]),N)>0) Algo::Reverse(P);
            const int32 Base=RailVertices.Num();
            for(const auto& V:P) {RailVertices.Add(V); RailNormals.Add(N); RailUVs.Add(FVector2D(V.X,V.Z)/100.);}
            RailTriangles.Append({Base,Base+1,Base+2,Base,Base+2,Base+3});
        };
        TArray<FVector2D> Profile={{0,TreadHeights[0]+90},{Landing,TreadHeights[0]+90}};
        for(int32 I=1; I<=StepCount; ++I) Profile.Add({Landing+I*Tread,TreadHeights[I]+90});
        for(const double Sign:{-1.,1.})
        {
            auto At=[&](double Along,double Across,double Z){FVector P=Point(Along,Sign*(HalfWidth-5)+Across);P.Z=Z;return P;};
            for(int32 I=0; I+1<Profile.Num(); ++I)
            {
                const auto A=Profile[I],B=Profile[I+1];
                const FVector Up=(-Forward*(B.Y-A.Y)+FVector::UpVector*(B.X-A.X)).GetSafeNormal();
                RailFace({At(A.X,-2,A.Y),At(B.X,-2,B.Y),At(B.X,2,B.Y),At(A.X,2,A.Y)},Up);
                RailFace({At(A.X,-2,A.Y-4),At(A.X,2,A.Y-4),At(B.X,2,B.Y-4),At(B.X,-2,B.Y-4)},-Up);
                for(const double Edge:{-2.,2.}) RailFace({At(A.X,Edge,A.Y-4),At(B.X,Edge,B.Y-4),At(B.X,Edge,B.Y),At(A.X,Edge,A.Y)},Right*FMath::Sign(Edge));
            }
            for(const bool End:{false,true}) {
                const auto P=End?Profile.Last():Profile[0];
                RailFace({At(P.X,-2,P.Y-4),At(P.X,2,P.Y-4),At(P.X,2,P.Y),At(P.X,-2,P.Y)},Forward*(End?1.:-1.));
            }
            // Posts at most75cm apart, anchored in their actual tread, with
            // rail height interpolated along the generated nosing profile.
            const int32 Posts=FMath::CeilToInt((Run-4)/75.);
            for(int32 I=0; I<=Posts; ++I) {
                const double X=FMath::Lerp(2.,Run-2,I/static_cast<double>(Posts));
                const int32 Step=X<Landing?0:FMath::Clamp(1+FMath::FloorToInt((X-Landing)/Tread),1,StepCount);
                int32 J=0; while(J+2<Profile.Num() && X>Profile[J+1].X) ++J;
                const double Top=FMath::Lerp(Profile[J].Y,Profile[J+1].Y,(X-Profile[J].X)/(Profile[J+1].X-Profile[J].X))-4;
                const double Bottom=TreadHeights[Step]-1;
                for(const double Edge:{-2.,2.}) {
                    RailFace({At(X-2,Edge,Bottom),At(X+2,Edge,Bottom),At(X+2,Edge,Top),At(X-2,Edge,Top)},Right*FMath::Sign(Edge));
                    RailFace({At(X+Edge,-2,Bottom),At(X+Edge,2,Bottom),At(X+Edge,2,Top),At(X+Edge,-2,Top)},Forward*FMath::Sign(Edge));
                }
            }
        }
    }
	Centerline.Add(Door);
	for (int32 I = 0; I <= StepCount; ++I)
	{
		const double A = I == 0 ? 0 : Landing + (I - 1) * Tread;
		const double B = I == 0 ? Landing : Landing + I * Tread;
		FVector P = Point(A, 0); P.Z = TreadHeights[I]; Centerline.Add(P);
		P = Point(B, 0); P.Z = TreadHeights[I]; Centerline.Add(P);
	}
	for (double Along = Run; Along < Length; Along += 25.)
	{
		FVector P = Point(Along, 0);
		const FVector End = Point(Length, 0);
		const double Blend = FMath::Clamp((Along - FMath::Max(Run, Length - 100)) / FMath::Min(100., Length - Run), 0., 1.);
		const double StairBlend = FMath::Clamp(1 - (Along - Run) / FMath::Min(50., Length - Run), 0., 1.);
		P.Z = GroundHeight(Terrain, P) + FMath::Max(Lift, FMath::Max(StairBlend * (EndGround - GroundHeight(Terrain, P)), Blend * (SidewalkHeight(End) - GroundHeight(Terrain, End))));
		Centerline.Add(P);
	}
	Centerline.Add(Sidewalk);
	// Level door landing then treads; all elevations remain presentation only.
	for (int32 Step = 0; Step <= StepCount; ++Step)
	{
		const double A = Step == 0 ? 0 : Landing + (Step - 1) * Tread;
		const double B = Step == 0 ? Landing : Landing + Step * Tread;
		const double Z = TreadHeights[Step];
		FVector P0 = Point(A, -HalfWidth), P1 = Point(A, HalfWidth), P2 = Point(B, HalfWidth), P3 = Point(B, -HalfWidth);
		P0.Z = P1.Z = P2.Z = P3.Z = Z;
		Face({P0, P1, P2, P3}, FVector::UpVector);
		for (const double Sign : {-1., 1.})
		{
			FVector A0 = Point(A, Sign * HalfWidth), B0 = Point(B, Sign * HalfWidth);
			A0.Z = B0.Z = TreadBottoms[Step];
			FVector A1 = A0, B1 = B0; A1.Z = B1.Z = Z;
			Face({A0, B0, B1, A1}, Right * Sign);
		}
		FVector Low0 = P3, Low1 = P2; Low0.Z = Low1.Z = Step == StepCount ? EndGround : TreadHeights[Step + 1];
		Face({P3, P2, Low1, Low0}, Forward);
	}
	// Clip the private walk against each real terrain triangle. Every crest,
	// hollow and cross-slope is retained, including fine-grid diagonals. Only
	// the final metre blends up to the sampled public sidewalk surface.
	const TArray<FVector> Corners = {Point(Run, -HalfWidth), Point(Run, HalfWidth), Point(Length, -HalfWidth), Point(Length, HalfWidth)};
	const FBox Bounds(Corners);
	auto Clip = [](TArray<FVector> P, TFunctionRef<double(const FVector&)> Distance) {
		TArray<FVector> Out;
		for (int32 I = 0; I < P.Num(); ++I) {
			const FVector A = P[I], B = P[(I + 1) % P.Num()]; const double DA = Distance(A), DB = Distance(B);
			if (DA >= -.0001) Out.Add(A);
			if ((DA > 0 && DB < 0) || (DA < 0 && DB > 0)) Out.Add(FMath::Lerp(A, B, DA / (DA - DB)));
		} return Out;
	};
	for (int32 Y = FMath::FloorToInt(Bounds.Min.Y / 400.); Y <= FMath::FloorToInt(Bounds.Max.Y / 400.); ++Y)
		for (int32 X = FMath::FloorToInt(Bounds.Min.X / 400.); X <= FMath::FloorToInt(Bounds.Max.X / 400.); ++X)
		{
			FCityTerrainSurface::Facets(Terrain, X, Y, [&](const FVector& A, const FVector& B, const FVector& C) {
				TArray<FVector> P{A, B, C};
				P = Clip(P, [&](const FVector& V) {return FVector::DotProduct(V - Door, Forward) - Run;});
				P = Clip(P, [&](const FVector& V) {return Length - FVector::DotProduct(V - Door, Forward);});
				P = Clip(P, [&](const FVector& V) {return HalfWidth - FVector::DotProduct(V - Door, Right);});
				P = Clip(P, [&](const FVector& V) {return HalfWidth + FVector::DotProduct(V - Door, Right);});
				if (P.Num() < 3) return;
				for (auto& V : P) {
					const double Along = FVector::DotProduct(V - Door, Forward);
					const FVector End = V + Forward * (Length - Along);
					const double Blend = FMath::Clamp((Along - FMath::Max(Run, Length - 100)) / FMath::Min(100., Length - Run), 0., 1.);
					// At the stair foot meet its highest cross-slope landing, then
					// return to ground over50cm without a vertical side gap.
					const double StairBlend = FMath::Clamp(1 - (Along - Run) / FMath::Min(50., Length - Run), 0., 1.);
					V.Z += FMath::Max(Lift, FMath::Max(StairBlend * (EndGround - GroundHeight(Terrain, V)), Blend * (SidewalkHeight(End) - GroundHeight(Terrain, End))));
                    // A road without a sidewalk has a4cm paving lift. Its
                    // endpoint must meet that surface rather than retain6cm.
                    if(Blend>=1.-.000001) V.Z=SidewalkHeight(End);
				}
				Face(P, FVector::UpVector);
				for (int32 I = 0; I < P.Num(); ++I) {
					const FVector A0 = P[I], B0 = P[(I + 1) % P.Num()];
					const double AcrossA = FVector::DotProduct(A0 - Door, Right), AcrossB = FVector::DotProduct(B0 - Door, Right);
					if (!FMath::IsNearlyEqual(FMath::Abs(AcrossA), HalfWidth, .01) || !FMath::IsNearlyEqual(AcrossA, AcrossB, .01)) continue;
					FVector LowA = A0, LowB = B0; LowA.Z = GroundHeight(Terrain, LowA) - 10; LowB.Z = GroundHeight(Terrain, LowB) - 10;
					Face({A0, B0, LowB, LowA}, Right * FMath::Sign(AcrossA));
				}
			});
		}
}
