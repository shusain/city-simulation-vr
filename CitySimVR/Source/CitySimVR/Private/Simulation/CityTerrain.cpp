#include "Simulation/CityTerrain.h"
#include "Commands/CityPlacementModel.h"
#include "Interaction/CityBoardPicking.h"
#include "Presentation/CityScenery.h"

namespace
{
	uint32 Hash2D(uint32 X, uint32 Y, uint32 Seed)
	{
		uint32 Value = X * 0x8da6b343u ^ Y * 0xd8163841u ^ Seed * 0xcb1ab31fu;
		Value ^= Value >> 13;
		Value *= 0x85ebca6bu;
		Value ^= Value >> 16;
		return Value;
	}

	int32 SignedNoise(const int32 X, const int32 Y, const int32 Seed, const int32 Amplitude)
	{
		return static_cast<int32>(Hash2D(X, Y, Seed) % static_cast<uint32>(Amplitude * 2 + 1)) - Amplitude;
	}

	int32 InterpolatedNoise(const int32 SampleX, const int32 SampleY, const int32 LatticeSpacing, const int32 Seed,
	                        const int32 Amplitude)
	{
		const int32 X0 = SampleX / LatticeSpacing;
		const int32 Y0 = SampleY / LatticeSpacing;
		const int32 RemainderX = SampleX % LatticeSpacing;
		const int32 RemainderY = SampleY % LatticeSpacing;
		const int32 A = SignedNoise(X0, Y0, Seed, Amplitude);
		const int32 B = SignedNoise(X0 + 1, Y0, Seed, Amplitude);
		const int32 C = SignedNoise(X0, Y0 + 1, Seed, Amplitude);
		const int32 D = SignedNoise(X0 + 1, Y0 + 1, Seed, Amplitude);
		const int32 Bottom = FMath::Lerp(A, B, static_cast<double>(RemainderX) / LatticeSpacing);
		const int32 Top = FMath::Lerp(C, D, static_cast<double>(RemainderX) / LatticeSpacing);
		return FMath::Lerp(Bottom, Top, static_cast<double>(RemainderY) / LatticeSpacing);
	}

	int32 WaterLayoutFor(const FCityTerrainGenerationParameters& Parameters)
	{
		return Parameters.GeneratorVersion == 0
		           ? 0
		           : static_cast<int32>((Hash2D(17, 29, Parameters.Seed ^ 0x6a09e667u) >> 24) %
		                                (Parameters.GeneratorVersion == 1 ? 4 : 6));
	}

	struct FLakeLobe
	{
		FVector2D Center;
		FVector2D Axis;
		double Scale;
		double Stretch;
		double Distance(const FVector2D& Point) const
		{
			const FVector2D Delta = Point - Center;
			return FVector2D(Delta.X * Axis.X + Delta.Y * Axis.Y, (-Delta.X * Axis.Y + Delta.Y * Axis.X) * Stretch)
			           .Size() /
			       Scale;
		}
	};

	TArray<FLakeLobe> OrganicLakeLobes(const FCityTerrainGenerationParameters& Parameters)
	{
		TArray<FLakeLobe> Lobes;
		const int32 Layout = WaterLayoutFor(Parameters);
		if (Layout == 1 || Layout == 4)
			return Lobes; // River-only or dry land.
		const uint32 LakeHash = Hash2D(17, 29, Parameters.Seed ^ 0x6a09e667u);
		const int32 Groups = Layout == 3 || Layout == 5 ? 2 + static_cast<int32>((LakeHash >> 12) % 3) : 1;
		for (int32 Group = 0; Group < Groups; ++Group)
		{
			const uint32 Hash = Hash2D(Group, 83, Parameters.Seed);
			const FVector2D Center = Group == 0 ? FVector2D(26.5 + LakeHash % 23, 14.5 + (LakeHash >> 8) % 36)
			                                    : FVector2D(24.5 + Hash % 34, 7.5 + (Hash >> 8) % 50);
			const double Angle = FMath::DegreesToRadians(static_cast<double>((Hash >> 16) % 360));
			const FVector2D Axis(FMath::Cos(Angle), FMath::Sin(Angle)), Side(-Axis.Y, Axis.X);
			const double Scale =
			    (.55 + ((LakeHash >> 5) % 8) * .08) * (Group == 0 ? 1.0 : .55 + ((Hash >> 24) % 4) * .1);
			const double Stretch = 1.05 + ((Hash >> 20) % 5) * .1;
			Lobes.Add({Center, Axis, Scale, Stretch});
			Lobes.Add(
			    {Center + Axis * (Scale * 4.0) + Side * (Scale * SignedNoise(Group, 92, Parameters.Seed, 100) / 100.0),
			     Axis, Scale * (.70 + ((Hash >> 4) % 4) * .1), Stretch});
			if ((Hash & 1u) != 0)
				Lobes.Add({Center - Axis * (Scale * 2.5) + Side * (Scale * 2.5), Side, Scale * .65, 1.1});
		}
		return Lobes;
	}
} // namespace

int32 FCityTerrainGenerationParameters::GetMapRotation() const
{
	return GeneratorVersion == 0 ? 0 : static_cast<int32>((Hash2D(93, 71, Seed) >> 16) % 4);
}

FCityCellCoord FCityTerrainGenerationParameters::GetEntranceCell() const
{
	if (GeneratorVersion == 0)
		return FCityPlacementModel::EntranceCell;
	FCityCellCoord Cell(0, 8 + static_cast<int32>((Hash2D(93, 71, Seed) >> 8) % 48));
	for (int32 Turn = 0; Turn < GetMapRotation(); ++Turn)
		Cell = {Dimensions.GetWidth() - 1 - Cell.Y, Cell.X};
	return Cell;
}

FString FCityTerrainGenerationParameters::DescribeLandscape() const
{
	if (GeneratorVersion == 0)
		return TEXT("Rolling lake / river");
	const TCHAR* Relief[] = {TEXT("Lowlands"), TEXT("Rolling hills"), TEXT("Mountain foothills")};
	const TCHAR* Water[] = {TEXT("lake / river"), TEXT("river valley"), TEXT("lake basin"), TEXT("scattered lakes")};
	if (GeneratorVersion == 1)
		return FString::Printf(TEXT("%s / %s"), Relief[Hash2D(93, 71, Seed) % 3], Water[WaterLayoutFor(*this)]);
	const TCHAR* OrganicRelief[] = {TEXT("Lowlands"), TEXT("Rolling hills"), TEXT("Mountain clusters")};
	const TCHAR* OrganicWater[] = {TEXT("lake / river"),  TEXT("river valley"),
	                               TEXT("lake basin"),    TEXT("scattered lakes / river"),
	                               TEXT("dry landscape"), TEXT("lake chain")};
	const FString Landscape =
	    FString::Printf(TEXT("%s / %s"), OrganicRelief[Hash2D(93, 71, Seed) % 3], OrganicWater[WaterLayoutFor(*this)]);
	return SceneryVersion > 0 ? Landscape + TEXT(" / ") + FCitySceneryField::ClimateDescription(Seed) : Landscape;
}

void FCityTerrainHeightfield::GenerateVariedLandscape()
{
	const uint32 Layout = Hash2D(93, 71, Parameters.Seed);
	const int32 Relief = static_cast<int32>(Layout % 3);
	const bool bDetailed = Parameters.GeneratorVersion >= 2;
	const int32 EntryY = 8 + static_cast<int32>((Layout >> 8) % 48);
	const int32 BroadAmplitude = Parameters.BroadElevationAmplitudeCm * (Relief + 1) / 3;
	const int32 PeakCount = Relief == 0 ? 2 : Relief == 1 ? 4 : bDetailed ? 2 : 7;
	for (int32 Y = 0; Y < GetSampleHeight(); ++Y)
		for (int32 X = 0; X < GetSampleWidth(); ++X)
		{
			int32 Height = InterpolatedNoise(X, Y, 16, Parameters.Seed, BroadAmplitude) +
			               InterpolatedNoise(X, Y, 6, Parameters.Seed ^ 0x51f15e5d, BroadAmplitude / 10);
			for (int32 Peak = 0; Peak < PeakCount; ++Peak)
			{
				const uint32 Hash = Hash2D(Peak, 37, Parameters.Seed);
				const int32 Px = (22 + Hash % 37) * SamplesPerCell;
				const int32 Py = (5 + (Hash >> 8) % 54) * SamplesPerCell;
				const int32 Radius =
				    (Relief == 2 && !bDetailed ? 5 + (Hash >> 16) % 8 : 10 + (Hash >> 16) % 12) * SamplesPerCell;
				const int32 DistanceSq = (X - Px) * (X - Px) + (Y - Py) * (Y - Py);
				const int32 PeakAmplitude = (Parameters.HillAmplitudeCm + Parameters.RidgeAmplitudeCm) *
				                            (Relief == 0 || (Relief == 2 && bDetailed) ? 1
				                             : Relief == 1                             ? 4
				                                                                       : 16) /
				                            4;
				if (DistanceSq < Radius * Radius)
				{
					const double Alpha = 1.0 - static_cast<double>(DistanceSq) / (Radius * Radius);
					Height += FMath::RoundToInt(PeakAmplitude * Alpha * Alpha);
				}
			}
			HeightsCm[Index(X, Y)] = Height;
		}
	for (int32 Pass = 0; Pass < Parameters.SmoothingPasses; ++Pass)
	{
		TArray<int32> Smoothed = HeightsCm;
		for (int32 Y = 1; Y < GetSampleHeight() - 1; ++Y)
			for (int32 X = 1; X < GetSampleWidth() - 1; ++X)
				Smoothed[Index(X, Y)] =
				    (HeightsCm[Index(X, Y)] * 4 + HeightsCm[Index(X - 1, Y)] + HeightsCm[Index(X + 1, Y)] +
				     HeightsCm[Index(X, Y - 1)] + HeightsCm[Index(X, Y + 1)]) /
				    8;
		HeightsCm = MoveTemp(Smoothed);
	}
	if (bDetailed)
		AddMountainDetail();
	// A dry, level starting parcel and gradual transition survive every orientation.
	const int32 EntryHeight = FMath::Max(Parameters.SeaLevelCm + 400, HeightsCm[Index(0, EntryY * SamplesPerCell)]);
	for (int32 Y = 0; Y < GetSampleHeight(); ++Y)
		for (int32 X = 0; X < GetSampleWidth(); ++X)
		{
			const double Dx = FMath::Max(0, X - (Parameters.StarterDepthCells + 1) * SamplesPerCell);
			const double Dy = FMath::Max(0, FMath::Abs(Y - EntryY * SamplesPerCell) -
			                                    (Parameters.StarterHalfWidthCells + 1) * SamplesPerCell);
			const double Alpha = FMath::Clamp(FMath::Sqrt(Dx * Dx + Dy * Dy) / (10.0 * SamplesPerCell), 0.0, 1.0);
			const double Blend = Alpha * Alpha * (3.0 - 2.0 * Alpha);
			HeightsCm[Index(X, Y)] = FMath::RoundToInt(FMath::Lerp(static_cast<double>(EntryHeight),
			                                                       static_cast<double>(HeightsCm[Index(X, Y)]), Blend) /
			                                           Parameters.QuantizationCm) *
			                         Parameters.QuantizationCm;
		}
	GenerateHydrology();
	// Rotate all authoritative surfaces together, never just the preview or marker.
	for (int32 Turn = 0; Turn < Parameters.GetMapRotation(); ++Turn)
	{
		TArray<int32> Rotated = HeightsCm;
		for (int32 Y = 0; Y < GetSampleHeight(); ++Y)
			for (int32 X = 0; X < GetSampleWidth(); ++X)
				Rotated[Index(GetSampleWidth() - 1 - Y, X)] = HeightsCm[Index(X, Y)];
		HeightsCm = MoveTemp(Rotated);
		auto Classes = WaterClasses;
		auto Surfaces = WaterSurfaceHeightsCm;
		for (int32 Y = 0; Y < GetDimensions().GetHeight(); ++Y)
			for (int32 X = 0; X < GetDimensions().GetWidth(); ++X)
			{
				const int32 Target = CellIndex({GetDimensions().GetWidth() - 1 - Y, X});
				Classes[Target] = WaterClasses[CellIndex({X, Y})];
				Surfaces[Target] = WaterSurfaceHeightsCm[CellIndex({X, Y})];
			}
		WaterClasses = MoveTemp(Classes);
		WaterSurfaceHeightsCm = MoveTemp(Surfaces);
		for (FCityCellCoord& Cell : RiverCells)
			Cell = {GetDimensions().GetWidth() - 1 - Cell.Y, Cell.X};
	}
}

void FCityTerrainHeightfield::AddMountainDetail()
{
	const uint32 Layout = Hash2D(93, 71, Parameters.Seed);
	if (Layout % 3 != 2)
		return;
	const auto Lakes = OrganicLakeLobes(Parameters);
	const int32 Count = 1 + static_cast<int32>((Layout >> 20) % 3);
	TArray<FVector2D> Centers;
	for (int32 Cluster = 0; Cluster < Count; ++Cluster)
	{
		const uint32 Hash = Hash2D(Cluster, 113, Parameters.Seed);
		const double Radius = 4.5 + ((Hash >> 16) % 6) * .5;
		FVector2D Center;
		double BestClearance = -DBL_MAX;
		// Keep peaks beside lowland lakes rather than carving a circular crater
		// through the summit. Also separate clusters so steep land stays localized.
		for (int32 Attempt = 0; Attempt < 24; ++Attempt)
		{
			const uint32 CandidateHash = Hash2D(Attempt, 131 + Cluster, Parameters.Seed);
			const FVector2D Candidate(25.5 + CandidateHash % 33, 6.5 + (CandidateHash >> 8) % 51);
			double Clearance = 100.0;
			for (const auto& Lake : Lakes)
				Clearance = FMath::Min(Clearance, (Candidate - Lake.Center).Size() - Lake.Scale * 6.0 - Radius);
			for (const auto& Previous : Centers)
				Clearance = FMath::Min(Clearance, (Candidate - Previous).Size() - Radius * 2.0);
			if (Clearance > BestClearance)
			{
				BestClearance = Clearance;
				Center = Candidate;
			}
			if (Clearance > 0)
				break;
		}
		Centers.Add(Center);
		const double Angle = FMath::DegreesToRadians(static_cast<double>((Hash >> 8) % 360));
		const FVector2D Axis(FMath::Cos(Angle), FMath::Sin(Angle));
		const double Amplitude = (Parameters.HillAmplitudeCm + Parameters.RidgeAmplitudeCm) * (4 + Hash % 5);
		for (int32 Y = 0; Y < GetSampleHeight(); ++Y)
			for (int32 X = 0; X < GetSampleWidth(); ++X)
			{
				const FVector2D Delta(static_cast<double>(X) / SamplesPerCell - Center.X,
				                      static_cast<double>(Y) / SamplesPerCell - Center.Y);
				const double U = (Delta.X * Axis.X + Delta.Y * Axis.Y) / Radius;
				const double V = (-Delta.X * Axis.Y + Delta.Y * Axis.X) / (Radius * .85);
				const double Envelope = FMath::Max(0.0, 1.0 - FMath::Sqrt(U * U + V * V));
				if (Envelope == 0)
					continue;
				const double Ridge = 1.0 - FMath::Abs(InterpolatedNoise(X, Y, 6, Hash, 1000)) / 1000.0;
				const double Gully = InterpolatedNoise(X, Y, 3, Hash ^ 0x71bb93u, 1000) / 1000.0;
				const double Detail = InterpolatedNoise(X, Y, 2, Hash ^ 0x93cc71u, 1000) / 1000.0;
				// Add unsmoothed, nested ridges after the gentle background is smoothed.
				// A pointed envelope fades all roughness to zero outside each small massif.
				HeightsCm[Index(X, Y)] += FMath::RoundToInt(Amplitude * FMath::Pow(Envelope, 1.15) *
				                                            (.72 + .52 * Ridge + .20 * Gully + .12 * Detail));
			}
	}
}

bool FCityTerrainGenerationParameters::IsValid(FString* OutError) const
{
	const bool bValid =
	    Dimensions.IsValid() && GeneratorVersion >= 0 && GeneratorVersion <= 2 && WaterRulesVersion >= 0 &&
	    WaterRulesVersion <= 1 && SceneryVersion >= 0 && SceneryVersion <= 1 && BroadElevationAmplitudeCm >= 0 &&
	    BroadElevationAmplitudeCm <= 5000 && HillAmplitudeCm >= 0 && HillAmplitudeCm <= 5000 && RidgeAmplitudeCm >= 0 &&
	    RidgeAmplitudeCm <= 5000 && SmoothingPasses >= 0 && SmoothingPasses <= 8 && QuantizationCm >= 1 &&
	    QuantizationCm <= 100 && SeaLevelCm >= -5000 && SeaLevelCm <= 1000 && StarterHalfWidthCells >= 2 &&
	    StarterHalfWidthCells <= 16 && StarterDepthCells >= 2 && StarterDepthCells <= 24;
	if (!bValid && OutError)
	{
		*OutError = TEXT("Terrain generation parameters are outside supported bounds");
	}
	return bValid;
}

bool FCityTerrainHeightfield::Generate(const FCityTerrainGenerationParameters& InParameters, FString* OutError)
{
	if (!InParameters.IsValid(OutError))
	{
		return false;
	}
	Parameters = InParameters;
	HeightsCm.SetNumZeroed(GetSampleCount());
	if (Parameters.GeneratorVersion >= 1)
	{
		GenerateVariedLandscape();
		return true;
	}
	// Keep the tabletop broadly rolling rather than treating every hill as a
	// potential foundation failure. Scaling the persisted parameters here also
	// improves established saves when they regenerate their heightfield.
	const int32 BroadAmplitude = Parameters.BroadElevationAmplitudeCm / 2;
	const int32 HillAmplitude = Parameters.HillAmplitudeCm / 2;
	const int32 RidgeAmplitude = Parameters.RidgeAmplitudeCm / 2;

	// Low-frequency integer noise gives broad elevation without platform-specific
	// floating point randomness. Analytic hills/ridge then establish recognizable fixtures.
	for (int32 Y = 0; Y < GetSampleHeight(); ++Y)
	{
		for (int32 X = 0; X < GetSampleWidth(); ++X)
		{
			const int32 Broad = InterpolatedNoise(X, Y, 8 * SamplesPerCell, Parameters.Seed, BroadAmplitude);
			const int32 Detail =
			    InterpolatedNoise(X, Y, 2 * SamplesPerCell, Parameters.Seed ^ 0x51f15e5d, BroadAmplitude / 12);
			const int32 HillDx = X - 15 * SamplesPerCell;
			const int32 HillDy = Y - 45 * SamplesPerCell;
			const int32 HillRadiusSq = 15 * SamplesPerCell * 15 * SamplesPerCell;
			const int32 HillDistanceSq = HillDx * HillDx + HillDy * HillDy;
			const int32 Hill =
			    HillDistanceSq < HillRadiusSq ? HillAmplitude * (HillRadiusSq - HillDistanceSq) / HillRadiusSq : 0;
			const int32 RidgeDistance = FMath::Abs((X - 48 * SamplesPerCell) * 2 + (Y - 34 * SamplesPerCell));
			const int32 RidgeWidth = 18 * SamplesPerCell;
			const int32 Ridge =
			    RidgeDistance < RidgeWidth ? RidgeAmplitude * (RidgeWidth - RidgeDistance) / RidgeWidth : 0;
			HeightsCm[Index(X, Y)] = Broad + Detail + Hill + Ridge;
		}
	}

	// Existing saves persist the authored terrain parameters. Apply four baseline
	// smoothing passes here so their terrain becomes gentler too, instead of
	// requiring players to abandon an established city for the new defaults.
	const int32 EffectiveSmoothingPasses = FMath::Min(8, Parameters.SmoothingPasses + 4);
	for (int32 Pass = 0; Pass < EffectiveSmoothingPasses; ++Pass)
	{
		TArray<int32> Smoothed = HeightsCm;
		for (int32 Y = 1; Y < GetSampleHeight() - 1; ++Y)
		{
			for (int32 X = 1; X < GetSampleWidth() - 1; ++X)
			{
				Smoothed[Index(X, Y)] =
				    (HeightsCm[Index(X, Y)] * 4 + HeightsCm[Index(X - 1, Y)] + HeightsCm[Index(X + 1, Y)] +
				     HeightsCm[Index(X, Y - 1)] + HeightsCm[Index(X, Y + 1)]) /
				    8;
			}
		}
		HeightsCm = MoveTemp(Smoothed);
	}

	const int32 StarterMinX =
	    FMath::Clamp((FCityPlacementModel::EntranceCell.X - Parameters.StarterHalfWidthCells) * SamplesPerCell, 0,
	                 GetSampleWidth() - 1);
	const int32 StarterMaxX =
	    FMath::Clamp((FCityPlacementModel::EntranceCell.X + Parameters.StarterHalfWidthCells + 1) * SamplesPerCell, 0,
	                 GetSampleWidth() - 1);
	const int32 StarterMaxY =
	    FMath::Clamp((Parameters.StarterDepthCells + 1) * SamplesPerCell, 0, GetSampleHeight() - 1);
	const int32 EntranceSampleX = FCityPlacementModel::EntranceCell.X * SamplesPerCell + SamplesPerCell / 2;
	const int32 EntranceSampleY = FCityPlacementModel::EntranceCell.Y * SamplesPerCell + SamplesPerCell / 2;
	const int32 StarterHeight = HeightsCm[Index(EntranceSampleX, EntranceSampleY)];
	constexpr int32 StarterFeatherCells = 10;
	const int32 StarterFeatherSamples = StarterFeatherCells * SamplesPerCell;
	for (int32 Y = 0; Y <= FMath::Min(StarterMaxY + StarterFeatherSamples, GetSampleHeight() - 1); ++Y)
	{
		for (int32 X = FMath::Max(StarterMinX - StarterFeatherSamples, 0);
		     X <= FMath::Min(StarterMaxX + StarterFeatherSamples, GetSampleWidth() - 1); ++X)
		{
			const int32 DistanceX = FMath::Max(StarterMinX - X, FMath::Max(0, X - StarterMaxX));
			const int32 DistanceY = FMath::Max(0, Y - StarterMaxY);
			const double Distance = FMath::Sqrt(static_cast<double>(DistanceX * DistanceX + DistanceY * DistanceY));
			if (Distance >= StarterFeatherSamples)
			{
				continue;
			}
			const double Alpha = FMath::Clamp(Distance / StarterFeatherSamples, 0.0, 1.0);
			const double SmoothAlpha = Alpha * Alpha * (3.0 - 2.0 * Alpha);
			HeightsCm[Index(X, Y)] = FMath::RoundToInt(FMath::Lerp(
			    static_cast<double>(StarterHeight), static_cast<double>(HeightsCm[Index(X, Y)]), SmoothAlpha));
		}
	}

	for (int32& Height : HeightsCm)
	{
		Height = FMath::RoundToInt(static_cast<double>(Height) / Parameters.QuantizationCm) * Parameters.QuantizationCm;
	}
	GenerateHydrology();
	return true;
}

int32 FCityTerrainHeightfield::GetGroundwaterYieldPercent(const FCityCellCoord& Cell) const
{
	if (!IsValidCell(Cell) || HeightsCm.IsEmpty())
		return 0;
	// Renewable underground availability is independent of visible water and
	// generator geometry. Coherent integer patches make nearby sites comparable.
	const int32 Broad = InterpolatedNoise(Cell.X, Cell.Y, 12, Parameters.Seed ^ 0x34a15e27, 30);
	const int32 Detail = InterpolatedNoise(Cell.X, Cell.Y, 5, Parameters.Seed ^ 0x6279b10f, 10);
	const double AboveSea = GetCellCenterHeightCm(Cell) - Parameters.SeaLevelCm;
	const int32 Basin = AboveSea < 500 ? 10 : AboveSea > 2000 ? -10 : 0;
	return FMath::Clamp(100 + Broad + Detail + Basin, 60, 140);
}

void FCityTerrainHeightfield::GenerateHydrology()
{
	WaterClasses.Init(ECityWaterClass::Land, GetDimensions().GetCellCount());
	WaterSurfaceHeightsCm.Init(MIN_int32, GetDimensions().GetCellCount());
	RiverCells.Reset();
	const uint32 LakeHash = Hash2D(17, 29, Parameters.Seed ^ 0x6a09e667u);
	FCityCellCoord LakeCenter(44 + static_cast<int32>(LakeHash % 5) - 2,
	                          44 + static_cast<int32>((LakeHash >> 8) % 5) - 2);
	const bool bVaried = Parameters.GeneratorVersion >= 1;
	const bool bDetailed = Parameters.GeneratorVersion >= 2;
	const int32 WaterLayout = WaterLayoutFor(Parameters);
	const auto Lobes = bDetailed ? OrganicLakeLobes(Parameters) : TArray<FLakeLobe>();
	if (bVaried)
		LakeCenter = {26 + static_cast<int32>(LakeHash % 23), 14 + static_cast<int32>((LakeHash >> 8) % 36)};
	const int32 OutletY =
	    bVaried ? 8 + static_cast<int32>((LakeHash >> 16) % 48) : 32 + static_cast<int32>((LakeHash >> 16) % 25);
	int32 RiverY = LakeCenter.Y;
	for (int32 X = WaterLayout == 1 ? 20 : LakeCenter.X;
	     (WaterLayout == 0 || WaterLayout == 1 || WaterLayout == 3) && X < GetDimensions().GetWidth(); ++X)
	{
		RiverCells.AddUnique(FCityCellCoord(X, RiverY));
		const int32 TargetY =
		    bVaried && X < 50 ? FMath::Clamp(OutletY + SignedNoise(X / 6, 7, Parameters.Seed, 5), 5, 58) : OutletY;
		if (X < GetDimensions().GetWidth() - 1 && X % 2 == 1 && RiverY != TargetY)
		{
			RiverY += FMath::Sign(TargetY - RiverY);
			RiverCells.AddUnique(FCityCellCoord(X, RiverY));
		}
	}

	// Shape the terrain and hydrology as one model. The lake basin and connected
	// river channel cross one constant sea-level datum; classification happens
	// only after this terrain surface exists.
	const double LakeScale = bVaried && !bDetailed ? 0.65 + ((LakeHash >> 5) % 8) * 0.1 : 1.0;
	const double LakeDeepRadiusCells = 3.0 * LakeScale;
	const double LakeShallowRadiusCells = 6.0 * LakeScale;
	const double LakeBankRadiusCells = 8.5 * LakeScale;
	constexpr double RiverBedRadiusCells = 0.55;
	const double RiverBankRadiusCells = bVaried ? 3.5 : 1.5;
	for (int32 SampleY = 0; SampleY < GetSampleHeight(); ++SampleY)
	{
		for (int32 SampleX = 0; SampleX < GetSampleWidth(); ++SampleX)
		{
			const double CellX = static_cast<double>(SampleX) / SamplesPerCell;
			const double CellY = static_cast<double>(SampleY) / SamplesPerCell;
			double LakeDistance =
			    FVector2D::Distance(FVector2D(CellX, CellY), FVector2D(LakeCenter.X + 0.5, LakeCenter.Y + 0.5));
			if (bDetailed)
			{
				LakeDistance = TNumericLimits<double>::Max();
				for (const auto& Lobe : Lobes)
					LakeDistance = FMath::Min(LakeDistance, Lobe.Distance(FVector2D(CellX, CellY)));
			}
			else if (WaterLayout == 1)
				LakeDistance = TNumericLimits<double>::Max(); // River valley.
			if (!bDetailed && WaterLayout == 3)               // Frozen generator1 scattered lakes.
				LakeDistance =
				    FMath::Min(LakeDistance, FVector2D::Distance(FVector2D(CellX, CellY),
				                                                 FVector2D(27.5, LakeCenter.Y < 32 ? 49.5 : 14.5)) *
				                                 1.35);
			double TargetHeight = TNumericLimits<double>::Max();
			if (LakeDistance <= LakeBankRadiusCells)
			{
				if (LakeDistance <= LakeDeepRadiusCells)
				{
					TargetHeight = Parameters.SeaLevelCm - 220.0;
				}
				else if (LakeDistance <= LakeShallowRadiusCells)
				{
					const double Alpha =
					    (LakeDistance - LakeDeepRadiusCells) / (LakeShallowRadiusCells - LakeDeepRadiusCells);
					TargetHeight = FMath::Lerp(Parameters.SeaLevelCm - 220.0, Parameters.SeaLevelCm - 60.0, Alpha);
				}
				else
				{
					const double Alpha =
					    (LakeDistance - LakeShallowRadiusCells) / (LakeBankRadiusCells - LakeShallowRadiusCells);
					TargetHeight = FMath::Lerp(Parameters.SeaLevelCm - 60.0,
					                           bVaried ? static_cast<double>(HeightsCm[Index(SampleX, SampleY)])
					                                   : Parameters.SeaLevelCm + 160.0,
					                           bVaried ? Alpha * Alpha * (3.0 - 2.0 * Alpha) : Alpha);
				}
			}

			double RiverDistance = TNumericLimits<double>::Max();
			for (const FCityCellCoord& RiverCell : RiverCells)
			{
				RiverDistance =
				    FMath::Min(RiverDistance, FVector2D::Distance(FVector2D(CellX, CellY),
				                                                  FVector2D(RiverCell.X + 0.5, RiverCell.Y + 0.5)));
			}
			if (RiverDistance <= RiverBankRadiusCells)
			{
				const double BankAlpha =
				    (RiverDistance - RiverBedRadiusCells) / (RiverBankRadiusCells - RiverBedRadiusCells);
				const double RiverTarget =
				    RiverDistance <= RiverBedRadiusCells
				        ? Parameters.SeaLevelCm - 90.0
				        : FMath::Lerp(Parameters.SeaLevelCm - 90.0,
				                      bVaried ? static_cast<double>(HeightsCm[Index(SampleX, SampleY)])
				                              : Parameters.SeaLevelCm + 140.0,
				                      bVaried ? BankAlpha * BankAlpha * (3.0 - 2.0 * BankAlpha) : BankAlpha);
				TargetHeight = FMath::Min(TargetHeight, RiverTarget);
			}
			if (TargetHeight < TNumericLimits<double>::Max())
			{
				const int32 QuantizedTarget =
				    FMath::RoundToInt(TargetHeight / Parameters.QuantizationCm) * Parameters.QuantizationCm;
				HeightsCm[Index(SampleX, SampleY)] = FMath::Min(HeightsCm[Index(SampleX, SampleY)], QuantizedTarget);
			}
		}
	}

	for (int32 Y = 0; Y < GetDimensions().GetHeight(); ++Y)
	{
		for (int32 X = 0; X < GetDimensions().GetWidth(); ++X)
		{
			const FCityCellCoord Cell(X, Y);
			const double CenterHeight = GetCellCenterHeightCm(Cell);
			if (CenterHeight < Parameters.SeaLevelCm)
			{
				const int32 Depth = Parameters.SeaLevelCm - FMath::RoundToInt(CenterHeight);
				WaterClasses[CellIndex(Cell)] =
				    Depth >= 150 ? ECityWaterClass::DeepWater : ECityWaterClass::ShallowWater;
				WaterSurfaceHeightsCm[CellIndex(Cell)] = Parameters.SeaLevelCm;
			}
		}
	}

	static const FIntPoint NeighborOffsets[] = {FIntPoint(0, 1), FIntPoint(1, 0), FIntPoint(0, -1), FIntPoint(-1, 0)};
	for (int32 Y = 0; Y < GetDimensions().GetHeight(); ++Y)
	{
		for (int32 X = 0; X < GetDimensions().GetWidth(); ++X)
		{
			const FCityCellCoord Cell(X, Y);
			if (IsWaterCell(Cell))
			{
				continue;
			}
			bool bAdjacentToWater =
			    GetCellMinHeightCm(Cell) < Parameters.SeaLevelCm && GetCellMaxHeightCm(Cell) >= Parameters.SeaLevelCm;
			for (const FIntPoint& Offset : NeighborOffsets)
			{
				const FCityCellCoord Neighbor(Cell.X + Offset.X, Cell.Y + Offset.Y);
				bAdjacentToWater |= IsValidCell(Neighbor) && IsWaterCell(Neighbor);
			}
			if (bAdjacentToWater)
			{
				WaterClasses[CellIndex(Cell)] = ECityWaterClass::Shoreline;
			}
		}
	}
}

int32 FCityTerrainHeightfield::GetVertexHeightCm(const int32 X, const int32 Y) const
{
	return GetSampleHeightCm(X * SamplesPerCell, Y * SamplesPerCell);
}

int32 FCityTerrainHeightfield::GetSampleHeightCm(const int32 SampleX, const int32 SampleY) const
{
	if (HeightsCm.Num() != GetSampleCount())
	{
		return 0;
	}
	return HeightsCm[Index(FMath::Clamp(SampleX, 0, GetSampleWidth() - 1),
	                       FMath::Clamp(SampleY, 0, GetSampleHeight() - 1))];
}

int32 FCityTerrainHeightfield::GetCellCornerHeightCm(const FCityCellCoord& Cell, const int32 CornerX,
                                                     const int32 CornerY) const
{
	return GetVertexHeightCm(Cell.X + FMath::Clamp(CornerX, 0, 1), Cell.Y + FMath::Clamp(CornerY, 0, 1));
}

double FCityTerrainHeightfield::SampleHeightCm(const double CellX, const double CellY) const
{
	const double X = FMath::Clamp(CellX * SamplesPerCell, 0.0, static_cast<double>(GetSampleWidth() - 1));
	const double Y = FMath::Clamp(CellY * SamplesPerCell, 0.0, static_cast<double>(GetSampleHeight() - 1));
	const int32 X0 = FMath::Min(FMath::FloorToInt(X), GetSampleWidth() - 2);
	const int32 Y0 = FMath::Min(FMath::FloorToInt(Y), GetSampleHeight() - 2);
	const double Tx = X - X0;
	const double Ty = Y - Y0;
	return FMath::Lerp(FMath::Lerp(static_cast<double>(GetSampleHeightCm(X0, Y0)),
	                               static_cast<double>(GetSampleHeightCm(X0 + 1, Y0)), Tx),
	                   FMath::Lerp(static_cast<double>(GetSampleHeightCm(X0, Y0 + 1)),
	                               static_cast<double>(GetSampleHeightCm(X0 + 1, Y0 + 1)), Tx),
	                   Ty);
}

FVector FCityTerrainHeightfield::GetSurfaceNormal(const double CellX, const double CellY,
                                                  const double RadiusCells) const
{
	const double Radius = FMath::Max(RadiusCells, 0.01);
	const double West = SampleHeightCm(CellX - Radius, CellY);
	const double East = SampleHeightCm(CellX + Radius, CellY);
	const double South = SampleHeightCm(CellX, CellY - Radius);
	const double North = SampleHeightCm(CellX, CellY + Radius);
	return FVector(West - East, South - North, CityBoardPicking::LogicalCellSizeCm * Radius * 2.0).GetSafeNormal();
}

double FCityTerrainHeightfield::GetCellCenterHeightCm(const FCityCellCoord& Cell) const
{
	return SampleHeightCm(Cell.X + 0.5, Cell.Y + 0.5);
}

int32 FCityTerrainHeightfield::GetCellMinHeightCm(const FCityCellCoord& Cell) const
{
	if (!IsValidCell(Cell))
		return 0;
	int32 Minimum = MAX_int32;
	for (int32 Y = Cell.Y * SamplesPerCell; Y <= (Cell.Y + 1) * SamplesPerCell; ++Y)
	{
		for (int32 X = Cell.X * SamplesPerCell; X <= (Cell.X + 1) * SamplesPerCell; ++X)
		{
			Minimum = FMath::Min(Minimum, GetSampleHeightCm(X, Y));
		}
	}
	return Minimum;
}

int32 FCityTerrainHeightfield::GetCellMaxHeightCm(const FCityCellCoord& Cell) const
{
	if (!IsValidCell(Cell))
		return 0;
	int32 Maximum = MIN_int32;
	for (int32 Y = Cell.Y * SamplesPerCell; Y <= (Cell.Y + 1) * SamplesPerCell; ++Y)
	{
		for (int32 X = Cell.X * SamplesPerCell; X <= (Cell.X + 1) * SamplesPerCell; ++X)
		{
			Maximum = FMath::Max(Maximum, GetSampleHeightCm(X, Y));
		}
	}
	return Maximum;
}

ECityTerrainSlope FCityTerrainHeightfield::GetCellSlope(const FCityCellCoord& Cell) const
{
	const int32 Range = GetCellMaxHeightCm(Cell) - GetCellMinHeightCm(Cell);
	return Range <= 80 ? ECityTerrainSlope::Flat : Range <= 240 ? ECityTerrainSlope::Gentle : ECityTerrainSlope::Steep;
}

ECityWaterClass FCityTerrainHeightfield::GetWaterClass(const FCityCellCoord& Cell) const
{
	return IsValidCell(Cell) && WaterClasses.Num() == GetDimensions().GetCellCount() ? WaterClasses[CellIndex(Cell)]
	                                                                                 : ECityWaterClass::Land;
}

bool FCityTerrainHeightfield::IsWaterCell(const FCityCellCoord& Cell) const
{
	const ECityWaterClass Water = GetWaterClass(Cell);
	return Water == ECityWaterClass::ShallowWater || Water == ECityWaterClass::DeepWater;
}

bool FCityTerrainHeightfield::IsCellBuildable(const FCityCellCoord& Cell) const
{
	return IsValidCell(Cell) && GetWaterClass(Cell) == ECityWaterClass::Land &&
	       GetCellSlope(Cell) != ECityTerrainSlope::Steep;
}

int32 FCityTerrainHeightfield::GetWaterSurfaceHeightCm(const FCityCellCoord& Cell) const
{
	return IsWaterCell(Cell) && WaterSurfaceHeightsCm.Num() == GetDimensions().GetCellCount()
	           ? WaterSurfaceHeightsCm[CellIndex(Cell)]
	           : MIN_int32;
}
