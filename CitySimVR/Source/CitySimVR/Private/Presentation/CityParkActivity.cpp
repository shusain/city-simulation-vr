#include "Presentation/CityParkActivity.h"
#include "Commands/CityPlacementModel.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Simulation/CityGrowthSimulation.h"

FTransform FCityParkActivity::ToImportedMeshSpace(const FCityParkActivitySample& Sample)
{
	if (Sample.bNativeSpace)
		return FTransform(Sample.Rotation, Sample.Position, Sample.Scale);
	// The established FBX import reflects authored Y into Unreal's handedness.
	return FTransform(FRotator(Sample.Rotation.Pitch, -Sample.Rotation.Yaw, -Sample.Rotation.Roll),
	                  FVector(Sample.Position.X, -Sample.Position.Y, Sample.Position.Z), Sample.Scale);
}

void FCityParkActivity::Build(const FCityPlacementModel& City, const FCityServiceSnapshot& Services,
                              const UCityBuildingCatalog* Catalog, const double Seconds, const float Daylight,
                              TArray<FCityParkActivitySample>& OutSamples)
{
	OutSamples.Reset();
	if (!Catalog || !FMath::IsFinite(Seconds) || !FMath::IsFinite(Daylight) || Daylight <= .05f)
		return;
	const double Fade = FMath::Clamp(static_cast<double>(Daylight), 0.0, 1.0);
	int32 Parks = 0;
	for (const auto& Provider : Services.ParkProviders)
	{
		const auto* Entity = City.GetEntities().Find(Provider.EntityId);
		if (!Entity || !Catalog->FindDefinition(Entity->DefinitionId))
			continue;
		if (++Parks > MaximumParks)
			break;
		const double Time = Seconds + (Provider.EntityId % 13) * .37;
		auto Add = [&](ECityParkActivityProp Prop, const FVector& Meters, const FRotator& Rotation, double Scale)
		{ OutSamples.Add({Provider.EntityId, Prop, Meters * 100.0, Rotation, FVector(Scale * Fade)}); };
		auto Visitor = [&](const FVector& Position, const FVector& Direction, double Lean = 0.0)
		{
			Add(ECityParkActivityProp::Visitor, Position, FRotator(Lean, Direction.Rotation().Yaw, 0), .811);
			OutSamples.Last().Motion = ECityParkMotion::Walk;
			OutSamples.Last().AnimationSeconds = FMath::Fmod(Time, 1.0);
		};
		auto Authored = [&](const ECityParkMotion Motion, const FVector& Offset = FVector::ZeroVector)
		{
			FCityParkMotionSample Pose;
			if (!FCityParkMotion::Sample(Motion, Time, Pose))
				return;
			auto Native = [&](const ECityParkActivityProp Kind, const FTransform& Transform)
			{
				FCityParkActivitySample Sample;
				Sample.EntityId = Provider.EntityId;
				Sample.Prop = Kind;
				Sample.Position = Transform.GetTranslation() + FVector(Offset.X, -Offset.Y, Offset.Z) * 100;
				Sample.Rotation = Transform.Rotator();
				Sample.Scale = Transform.GetScale3D() * Fade;
				Sample.bNativeSpace = true;
				Sample.Motion = Kind == ECityParkActivityProp::Visitor ? Motion : ECityParkMotion::None;
				Sample.AnimationSeconds = Pose.AnimationSeconds;
				OutSamples.Add(Sample);
			};
			Native(ECityParkActivityProp::Visitor, Pose.Body);
			for (int32 I = 0; I < Pose.PropCount; ++I)
				Native(Pose.Kinds[I], Pose.Props[I]);
		};
		if (Entity->DefinitionId == TEXT("BasketballPark"))
		{
			Authored(ECityParkMotion::BasketballDribble, {-6, 2, 0});
			Authored(ECityParkMotion::BasketballShot);
			Visitor({-8, -4, .3}, {1, 0, 0});
			Visitor({8, 4, .3}, {-1, 0, 0});
		}
		else if (Entity->DefinitionId == TEXT("SkatePark"))
		{
			Authored(ECityParkMotion::SkateKickflip, {0, -2, 0});
			Authored(ECityParkMotion::Skate180);
			Authored(ECityParkMotion::SkateLipStall, {0, 2, 0});
		}
		else if (Entity->DefinitionId == TEXT("SoccerPark"))
		{
			Authored(ECityParkMotion::SoccerKick);
			Authored(ECityParkMotion::SoccerRun, {-8, 4, 0});
			Authored(ECityParkMotion::GoalkeeperSave, {-21, 0, 0});
			Visitor({12, -7, .2}, {-1, 0, 0});
		}
		else if (Entity->DefinitionId == TEXT("BaseballPark"))
		{
			Authored(ECityParkMotion::BaseballPitch);
			Authored(ECityParkMotion::BaseballSwing);
			Visitor({1, 1, .2}, {-1, -1, 0});
			Visitor({12 + 2 * FMath::Sin(Time * .4), 12 + 2 * FMath::Cos(Time * .4), .2}, {-1, -1, 0});
		}
		else
		{
			// Strolling visitors for pocket, fountain and city garden parks.
			const double Reach = FMath::Min(Provider.Footprint.X, Provider.Footprint.Y) * 4.0 - 5;
			for (int32 I = 0; I < 4; ++I)
			{
				const double Phase = Time * .4 + I * PI * .5;
				const FVector Position(Reach * FMath::Sin(Phase), I % 2 ? 3.0 : -3.0, .47);
				Visitor(Position, {FMath::Cos(Phase), 0, 0});
			}
		}
	}
	check(OutSamples.Num() <= MaximumSamples);
}
