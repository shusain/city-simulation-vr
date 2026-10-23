#include "Presentation/CitySoundscapeComponent.h"
#include "Presentation/CityBoardActor.h"
#include "Presentation/CityAudioSettings.h"
#include "Presentation/CityAtmosphereMotion.h"
#include "Interaction/CityBoardPicking.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Simulation/CityParkSimulation.h"
#include "Components/AudioComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "IXRTrackingSystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	bool IsSiren(ECityAudioCue Cue)
	{
		return Cue <= ECityAudioCue::FireHorn;
	}
	bool IsAccent(ECityAudioCue Cue)
	{
		return Cue == ECityAudioCue::PoliceAlternate || Cue == ECityAudioCue::FireHorn;
	}
	FVector EntityPosition(const FCityPlacedEntity& Entity)
	{
		return FVector(Entity.Anchor.X + Entity.FootprintSize.X * .5, Entity.Anchor.Y + Entity.FootprintSize.Y * .5, 0);
	}
} // namespace
UCitySoundscapeComponent::UCitySoundscapeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	for (const auto& Entry : CityAudioAssets)
	{
		const FString Path = FString::Printf(TEXT("%s/%s.%s"), CityAudioGraphFolder, Entry.GraphName, Entry.GraphName);
		ConstructorHelpers::FObjectFinder<USoundBase> Source(*Path);
		Sources.Add(Source.Object);
	}
}
bool UCitySoundscapeComponent::IsReady() const
{
	return Sources.Num() == UE_ARRAY_COUNT(CityAudioAssets) && !Sources.Contains(nullptr);
}
USoundBase* UCitySoundscapeComponent::GetCueSound(ECityAudioCue Cue) const
{
	return Sources.IsValidIndex(static_cast<int32>(Cue)) ? Sources[static_cast<int32>(Cue)].Get() : nullptr;
}
void UCitySoundscapeComponent::EnsurePool()
{
	if (!Players.IsEmpty() || !IsReady())
		return;
	// 16 sirens,10 beds/transitions, two traffic engines + one transition, three shots =32.
	for (int32 Index = 0; Index < CitySoundscape::PoolSize; ++Index)
	{
		auto* Player = NewObject<UAudioComponent>(GetOwner());
		Player->bAutoActivate = false;
		Player->bAutoDestroy = false;
		Player->bIsUISound = true; // Cancellation is explicitly governed by the city lifecycle.
		Player->bOverrideAttenuation = true;
		Player->AttenuationOverrides.bAttenuate = false;
		Player->AttenuationOverrides.bSpatialize = true;
		Player->RegisterComponent();
		Players.Add(Player);
		Voices.AddDefaulted();
	}
}
void UCitySoundscapeComponent::StopAll()
{
	bWasBlocked = true;
	for (int32 Index = 0; Index < Players.Num(); ++Index)
		Retire(Index, CitySoundscape::InterruptSeconds);
}
void UCitySoundscapeComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	for (const auto& Player : Players)
		Player->Stop();
	Super::EndPlay(Reason);
}
int32 UCitySoundscapeComponent::GetPlayingVoiceCount() const
{
	int32 Count = 0;
	for (const auto& Player : Players)
		Count += Player->IsPlaying();
	return Count;
}
float UCitySoundscapeComponent::GetMusicDuckGain() const
{
	if (GetDefault<UCityAudioSettings>()->GetAlarmVolume() <= 0)
		return 1.f;
	float Gain = 1.f;
	for (int32 Index = 0; Index < Players.Num(); ++Index)
	{
		const auto& Voice = Voices[Index];
		if (Players[Index]->IsPlaying() && !Voice.bAmbience && !Voice.bRetiring &&
		    Voice.Gain * Voice.DistanceGain * Voice.DuckGain > .01f)
			Gain = FMath::Min(Gain, Voice.Key.Channel == ECityAudioChannel::Shot ? .55f : .35f);
	}
	return Gain;
}
int32 UCitySoundscapeComponent::FindVoice(FCityAudioVoiceKey Key) const
{
	for (int32 Index = 0; Index < Voices.Num(); ++Index)
		if (Players[Index]->IsPlaying() && !Voices[Index].bRetiring && Voices[Index].Key == Key)
			return Index;
	return INDEX_NONE;
}
void UCitySoundscapeComponent::Retire(int32 Index, float Seconds)
{
	if (!Voices[Index].bRetiring && Players[Index]->IsPlaying())
	{
		Players[Index]->FadeOut(Seconds, 0);
		Voices[Index].bRetiring = true;
	}
}
float UCitySoundscapeComponent::VoiceDistanceGain(const FCityAudioVoice& Voice) const
{
	if (!CitySoundscape::IsVehicleSiren(Voice.Cue) && !CitySoundscape::IsTrafficLoop(Voice.Cue))
		return 1;
	const FVector Source(Voice.GridPosition.X * CityBoardPicking::LogicalCellSizeCm,
	                     Voice.GridPosition.Y * CityBoardPicking::LogicalCellSizeCm, Voice.GridPosition.Z + 100);
	const FVector Camera = BoardTransform.InverseTransformPosition(Listener);
	const double Distance = FVector::Distance(Camera, Source) / CityBoardPicking::LogicalCellSizeCm;
	return CitySoundscape::IsTrafficLoop(Voice.Cue) ? CitySoundscape::AreaWeight(FMath::Max(0.0, Distance - 8), 16)
	                                                : CitySoundscape::VehicleSirenGain(Distance);
}
bool UCitySoundscapeComponent::StartVoice(FCityAudioVoice Request, float FadeSeconds)
{
	const int32 Cue = static_cast<int32>(Request.Cue);
	if (!Sources.IsValidIndex(Cue) || !Sources[Cue])
		return false;
	int32 Sirens = 0, Accents = 0, Beds = 0, Engines = 0;
	for (int32 Index = 0; Index < Voices.Num(); ++Index)
		if (Players[Index]->IsPlaying())
		{
			Sirens += IsSiren(Voices[Index].Cue);
			Accents += IsAccent(Voices[Index].Cue);
			Beds += Voices[Index].Key.Channel == ECityAudioChannel::Bed && !Voices[Index].bRetiring;
			Engines += Voices[Index].Key.Channel == ECityAudioChannel::Traffic && !Voices[Index].bRetiring;
		}
	if ((IsSiren(Request.Cue) && Sirens >= CitySoundscape::SirenLimit) || (IsAccent(Request.Cue) && Accents >= 2))
		return false;
	if (Request.Key.Channel == ECityAudioChannel::Bed && Beds >= CitySoundscape::BedLimit)
		return false;
	// A state change may allocate one replacement before retiring its old loop.
	if (Request.Key.Channel == ECityAudioChannel::Traffic && Engines >= CitySoundscape::TrafficLimit &&
	    FindVoice(Request.Key) == INDEX_NONE)
		return false;
	const int32 Start =
	    Request.Key.Channel == ECityAudioChannel::Bed ? CitySoundscape::SirenLimit
	    : Request.Key.Channel == ECityAudioChannel::Traffic || Request.Key.Channel == ECityAudioChannel::Shot
	        ? CitySoundscape::SirenLimit + CitySoundscape::BedLimit + 2 +
	              (Request.Key.Channel == ECityAudioChannel::Shot ? CitySoundscape::TrafficSlots : 0)
	        : 0;
	const int32 End = Request.Key.Channel == ECityAudioChannel::Bed
	                      ? CitySoundscape::SirenLimit + CitySoundscape::BedLimit + 2
	                  : Request.Key.Channel == ECityAudioChannel::Traffic
	                      ? CitySoundscape::SirenLimit + CitySoundscape::BedLimit + 2 + CitySoundscape::TrafficSlots
	                  : Request.Key.Channel == ECityAudioChannel::Shot ? Players.Num()
	                                                                   : CitySoundscape::SirenLimit;
	for (int32 Index = Start; Index < End; ++Index)
		if (!Players[Index]->IsPlaying())
		{
			Voices[Index] = Request;
			Voices[Index].StartSeconds = WallSeconds;
			Voices[Index].DistanceGain = VoiceDistanceGain(Request);
			Voices[Index].EndSeconds = CityAudioAssets[Cue].bLoop ? 0 : WallSeconds + CityAudioAssets[Cue].Seconds;
			auto* Player = Players[Index].Get();
			Player->SetSound(Sources[Cue]);
			Player->bAllowSpatialization = Request.bSpatial;
			Player->SetPitchMultiplier(1);
			const auto* Settings = GetDefault<UCityAudioSettings>();
			Player->SetVolumeMultiplier(
			    Request.Gain * Voices[Index].DistanceGain *
			    (Request.bAmbience ? Settings->GetAmbienceVolume() : Settings->GetAlarmVolume()));
			const FVector Local(Request.GridPosition.X * CityBoardPicking::LogicalCellSizeCm,
			                    Request.GridPosition.Y * CityBoardPicking::LogicalCellSizeCm,
			                    Request.GridPosition.Z + 100);
			const auto World = BoardTransform.TransformPosition(Local);
			const auto FocusWorld = BoardTransform.TransformPosition(Focus * CityBoardPicking::LogicalCellSizeCm);
			Player->SetWorldLocation(bHeadTracked ? World : Listener + (World - FocusWorld) * .05);
			Player->FadeIn(FadeSeconds, 1.f);
			return true;
		}
	return false;
}
void UCitySoundscapeComponent::Synchronize(FCityAudioVoice Request, bool bIntro)
{
	if (Request.Gain <= .005f)
		return;
	Wanted.Add(Request.Key);
	const int32 Existing = FindVoice(Request.Key);
	if (Existing == INDEX_NONE)
	{
		if (bIntro)
			Request.Cue = static_cast<ECityAudioCue>(static_cast<uint8>(Request.Cue) - 1);
		StartVoice(Request, Request.bAmbience ? .6f : CitySoundscape::PhaseBlendSeconds);
		return;
	}
	auto& Voice = Voices[Existing];
	Voice.GridPosition =
	    Voice.Key.Channel == ECityAudioChannel::Bed
	        ? FMath::Lerp(Voice.GridPosition, Request.GridPosition,
	                      static_cast<double>(CitySoundscape::Approach(0, 1, GetWorld()->GetDeltaSeconds())))
	        : Request.GridPosition;
	Voice.Gain = CitySoundscape::Approach(Voice.Gain, Request.Gain, GetWorld()->GetDeltaSeconds());
	if (Voice.Cue != Request.Cue && (CityAudioAssets[static_cast<int32>(Voice.Cue)].bLoop ||
	                                 WallSeconds >= Voice.EndSeconds - CitySoundscape::PhaseBlendSeconds))
	{
		// Retire only after the replacement is allocated; a saturated pool cannot strand an eligible loop.
		const float Blend =
		    Request.Key.Channel == ECityAudioChannel::Traffic ? .15f : CitySoundscape::PhaseBlendSeconds;
		if (StartVoice(Request, Blend))
			Retire(Existing, Blend);
	}
}
void UCitySoundscapeComponent::PlayShot(ECityAudioCue Cue, const FVector& Position, bool bSpatial, bool bAmbience,
                                        float Gain, uint64 ResponseId)
{
	FCityAudioVoice Request;
	Request.Key = {ECityAudioChannel::Shot, ++ShotSequence};
	Request.Cue = Cue;
	Request.GridPosition = Position;
	Request.bSpatial = bSpatial;
	Request.bAmbience = bAmbience;
	Request.Gain = Gain;
	Request.ResponseId = ResponseId;
	StartVoice(Request, .015f);
}
void UCitySoundscapeComponent::RebuildAreas(const UCitySimulationSubsystem& Simulation)
{
	Areas.Reset();
	TMap<uint64, FVector> NextEntities;
	const auto* Catalog = Simulation.GetBuildingCatalog();
	for (const auto& Pair : Simulation.GetPlacementModel().GetEntities())
	{
		const auto& Entity = Pair.Value;
		auto Position = EntityPosition(Entity);
		Position.Z = Simulation.GetTerrain().SampleHeightCm(Position.X, Position.Y);
		NextEntities.Add(Pair.Key, Position);
		FCityAudioArea Area;
		Area.Id = Pair.Key;
		Area.Definition = Entity.DefinitionId;
		Area.GridPosition = Position;
		Area.bPark = FCityParkEvaluator::Policy(Entity.DefinitionId, Simulation.GetEconomyTuning(), Catalog).bIsPark;
		Area.bIndustry = Entity.DefinitionId == TEXT("Factory");
		Area.bMarket = Entity.DefinitionId == TEXT("Shop");
		if (Area.bPark || Area.bIndustry || Area.bMarket)
			Areas.Add(Area);
		if (!bWasBlocked && !KnownEntities.Contains(Pair.Key) && WallSeconds >= NextActivity)
		{
			PlayShot(ECityAudioCue::ConstructionTap, Position, true, false, .5f);
			NextActivity = WallSeconds + 3;
		}
	}
	if (!bWasBlocked && WallSeconds >= NextActivity)
		for (const auto& Pair : KnownEntities)
			if (!NextEntities.Contains(Pair.Key))
			{
				PlayShot(ECityAudioCue::DemolitionImpact, Pair.Value, true, false, .55f);
				NextActivity = WallSeconds + 3;
				break;
			}
	KnownEntities = MoveTemp(NextEntities);
	Areas.Sort([](const auto& A, const auto& B) { return A.Id < B.Id; });
	AreaRevision = Simulation.GetPlacementModel().GetRevision();
	NextAreaCheck = 0;
}
void UCitySoundscapeComponent::Update(ACityBoardActor& Board, UCitySimulationSubsystem& Simulation, float Seconds,
                                      bool bBackgrounded)
{
	if (!IsReady())
		return;
	EnsurePool();
	WallSeconds = FPlatformTime::Seconds();
	const auto* Settings = GetDefault<UCityAudioSettings>();
	const bool Blocked = bBackgrounded || Simulation.IsEntryMenuSuspended() || Simulation.IsSimulationPaused() ||
	                     (Settings->GetAlarmVolume() <= 0 && Settings->GetAmbienceVolume() <= 0);
	const uint64 NewSession = Simulation.GetCitySessionGeneration();
	const bool Restore = NewSession != Session || bWasBlocked;
	if (NewSession != Session)
	{
		StopAll();
		Session = NewSession;
		Eligible.Reset();
		Areas.Reset();
		KnownEntities.Reset();
		ParkWeight = IndustryWeight = MarketWeight = 0;
		TargetPark = TargetIndustry = TargetMarket = 0;
		ParkPosition = IndustryPosition = MarketPosition = FVector::ZeroVector;
		AccentTimes.Reset();
		AreaRevision = MAX_uint64;
		bWasBlocked = true;
		NextAmbienceEvent = WallSeconds + 8;
		NextThunder = WallSeconds + 12;
	}
	BoardTransform = Board.GetActorTransform();
	auto* Controller = GetWorld()->GetFirstPlayerController();
	FRotator View = FRotator::ZeroRotator;
	if (Controller)
		Controller->GetPlayerViewPoint(Listener, View);
	bHeadTracked =
	    GEngine && GEngine->XRSystem.IsValid() && GEngine->XRSystem->IsHeadTrackingAllowedForWorld(*GetWorld());
	const auto Local = BoardTransform.InverseTransformPosition(Listener) / CityBoardPicking::LogicalCellSizeCm;
	const auto Direction = BoardTransform.InverseTransformVectorNoScale(View.Vector());
	Focus = FMath::Abs(Direction.Z) > .01 ? Local - Direction * (Local.Z / Direction.Z) : Local;
	Focus.Z = 0;
	const auto Dimensions = Simulation.GetPlacementModel().GetGrid().GetDimensions();
	Focus.X = FMath::Clamp(Focus.X, 0.0, static_cast<double>(Dimensions.GetWidth()));
	Focus.Y = FMath::Clamp(Focus.Y, 0.0, static_cast<double>(Dimensions.GetHeight()));
	Detail = CitySoundscape::Approach(Detail, CitySoundscape::LocalDetail(FMath::Abs(Local.Z)), Seconds);
	Daylight = 1;
	for (TActorIterator<ACitySandboxEnvironment> It(GetWorld()); It; ++It)
	{
		Daylight = It->GetParkDaylightFactor();
		break;
	}
	if (AreaRevision != Simulation.GetPlacementModel().GetRevision())
		RebuildAreas(Simulation);
	// Fire has its visible alert, local crackle and responding engines; no notification chime.
	const int64 Incidents = Simulation.GetDisasterSnapshot().TotalEvents;
	const auto& Needs = Simulation.GetNeedsSnapshot();
	const int64 ServiceProblem = (Needs.PowerShortage > 0 ? 1 : 0) | (Needs.UnservedServiceDemand > 0 ? 2 : 0);
	const int64 Objectives = Simulation.GetGuidedProgress().CompletedIds.Num();
	const bool EffectsBlocked = Blocked || Restore || Settings->GetAlarmVolume() <= 0;
	const bool NewIncident = IncidentGate.Observe(Session, EffectsBlocked, Incidents) && Incidents > PriorIncidents;
	const bool ServiceChanged = ServiceGate.Observe(Session, EffectsBlocked, ServiceProblem);
	const bool ObjectiveChanged =
	    ObjectiveGate.Observe(Session, EffectsBlocked, Objectives) && Objectives > PriorObjectiveCount;
	PriorIncidents = Incidents;
	PriorObjectiveCount = Objectives;
	const auto& DisasterState = Simulation.GetDisasterSnapshot();
	const auto* CurrentDisaster = DisasterState.Current();
	const int64 ActiveQuake = CurrentDisaster && CurrentDisaster->Kind == ECityNaturalDisaster::Earthquake &&
	                                  DisasterState.LastFixedStepId >= CurrentDisaster->StartStep
	                              ? CurrentDisaster->Id
	                              : 0;
	const bool QuakeStarted = QuakeGate.Observe(Session, EffectsBlocked, ActiveQuake) && ActiveQuake > 0;
	Wanted.Reset();
	TSet<FCityAudioVoiceKey> NowEligible;
	const auto& Traffic = Simulation.GetTrafficSnapshot();
	const auto& Network = Simulation.GetRoadNetworkSnapshot();
	const auto& Previous = Simulation.GetPreviousTrafficSnapshot();
	int32 Responders = 0;
	for (const auto& Vehicle : Traffic.Vehicles)
	{
		if (!CitySoundscape::IsResponding(Vehicle))
			continue;
		const FCityAudioVoiceKey Key{ECityAudioChannel::Vehicle, Vehicle.Id};
		NowEligible.Add(Key);
		if (Blocked || Responders >= CitySoundscape::VehicleLimit || Settings->GetAlarmVolume() <= 0)
			continue;
		FVector Position, Facing;
		const auto* Old = Previous.Vehicles.FindByPredicate([&](const auto& Other) { return Other.Id == Vehicle.Id; });
		if (!FCityTrafficSimulation::SamplePosition(Vehicle, Old, Network, Simulation.GetSimulationStepProgress(),
		                                            Position, Facing))
			continue;
		++Responders;
		FCityAudioVoice Request;
		Request.Key = Key;
		Request.Cue = CitySoundscape::ResponseLoop(Vehicle.VehicleType);
		Request.GridPosition = Position + FVector(.5, .5, 0);
		Request.GridPosition.Z =
		    Board
		        .GetCellSurfaceCenter(FCityCellCoord(FMath::FloorToInt(Position.X), FMath::FloorToInt(Position.Y),
		                                             FMath::RoundToInt(Position.Z)))
		        .Z;
		Request.Gain = .9f;
		Synchronize(Request, !Restore && !Eligible.Contains(Key));
		const bool AccentEligible =
		    Vehicle.VehicleType != ECityTripVehicleType::Ambulance &&
		    Traffic.Vehicles.ContainsByPredicate(
		        [&](const auto& Other)
		        {
			        return Other.VehicleType == ECityTripVehicleType::Car && !Other.bCompleted &&
			               Other.Route.IsValidIndex(Other.RouteLegIndex) &&
			               Vehicle.Route.IsValidIndex(Vehicle.RouteLegIndex) &&
			               Other.Route[Other.RouteLegIndex].EdgeId == Vehicle.Route[Vehicle.RouteLegIndex].EdgeId &&
			               FMath::Abs(Other.DistanceUnits - Vehicle.DistanceUnits) < 2000;
		        });
		const double Cooldown = Vehicle.VehicleType == ECityTripVehicleType::PoliceCar ? 6 : 8;
		auto& LastAccent = AccentTimes.FindOrAdd(Vehicle.Id);
		if (Restore || LastAccent == 0)
			LastAccent = WallSeconds;
		if (AccentEligible && WallSeconds - LastAccent >= Cooldown)
		{
			if (Random.RandRange(0, 2) == 0)
				PlayShot(Vehicle.VehicleType == ECityTripVehicleType::PoliceCar ? ECityAudioCue::PoliceAlternate
				                                                                : ECityAudioCue::FireHorn,
				         Request.GridPosition, true, false, Request.Gain * .8f, Vehicle.Id);
			LastAccent = WallSeconds;
		}
	}
	const auto& Disasters = Simulation.GetDisasterSnapshot();
	const auto* Event = Disasters.Current();
	if (Event && Disasters.LastFixedStepId >= Event->WarningStep &&
	    Disasters.LastFixedStepId < CitySoundscape::TownAlarmEndStep(*Event))
	{
		FCityAudioVoice Request;
		Request.Key = {ECityAudioChannel::Town, static_cast<uint64>(Event->Id)};
		NowEligible.Add(Request.Key);
		if (!Blocked && Settings->GetAlarmVolume() > 0)
		{
			Request.Cue = ECityAudioCue::TownLoop;
			Request.GridPosition = FVector(Event->Center.X + .5, Event->Center.Y + .5, 0);
			Request.bSpatial = false;
			Request.Gain = .75f;
			Synchronize(Request, !Restore && !Eligible.Contains(Request.Key));
		}
	}
	Eligible = MoveTemp(NowEligible);
	for (auto It = AccentTimes.CreateIterator(); It; ++It)
		if (!Eligible.Contains({ECityAudioChannel::Vehicle, It.Key()}))
			It.RemoveCurrent();
	if (Blocked)
	{
		if (!bWasBlocked)
			StopAll();
		bWasBlocked = true;
		return;
	}
	bWasBlocked = false;
	if (QuakeStarted)
		PlayShot(ECityAudioCue::EarthquakeRumble, FVector(CurrentDisaster->Center.X, CurrentDisaster->Center.Y, 0),
		         true, false, .6f);
	if (Settings->GetAlarmVolume() > 0 && WallSeconds >= NextNotification &&
	    (NewIncident || ServiceChanged || ObjectiveChanged))
	{
		PlayShot(NewIncident      ? ECityAudioCue::IncidentNotice
		         : ServiceChanged ? (ServiceProblem ? ECityAudioCue::ServiceAttention : ECityAudioCue::ServiceRecovered)
		                          : ECityAudioCue::ObjectiveComplete,
		         Focus, false, false, .75f);
		NextNotification = WallSeconds + 4;
	}
	if (WallSeconds >= NextAreaCheck)
	{
		NextAreaCheck = WallSeconds + .2;
		TargetPark = TargetIndustry = TargetMarket = 0;
		for (const auto& Area : Areas)
		{
			if (!Simulation.GetDevelopmentSnapshot().IsOperational(Area.Id) ||
			    Simulation.GetFireSnapshot().IsDisabled(Area.Id))
				continue;
			const float Weight =
			    CitySoundscape::AreaWeight(FVector::Dist2D(Focus, Area.GridPosition), Area.bPark ? 14 : 10);
			if (Area.bPark && Weight > TargetPark)
			{
				TargetPark = Weight;
				ParkPosition = Area.GridPosition;
			}
			if (Area.bIndustry && Weight > TargetIndustry)
			{
				TargetIndustry = Weight;
				IndustryPosition = Area.GridPosition;
			}
			if (Area.bMarket && Weight > TargetMarket)
			{
				TargetMarket = Weight;
				MarketPosition = Area.GridPosition;
			}
		}
	}
	ParkWeight = CitySoundscape::Approach(ParkWeight, TargetPark * Detail, Seconds);
	IndustryWeight = CitySoundscape::Approach(IndustryWeight, TargetIndustry * Detail, Seconds);
	MarketWeight = CitySoundscape::Approach(MarketWeight, TargetMarket * Detail, Seconds);
	const auto& Weather = Simulation.GetEnvironmentSnapshot();
	const bool Rain = Weather.bPrecipitation && !FCityAtmosphereMotion::IsSnow(Weather);
	auto Bed = [&](ECityAudioCue Cue, float Gain, const FVector& Position, bool bAmbience = true)
	{
		if ((bAmbience ? Settings->GetAmbienceVolume() : Settings->GetAlarmVolume()) <= 0)
			return;
		FCityAudioVoice Request;
		Request.Key = {ECityAudioChannel::Bed, static_cast<uint64>(Cue)};
		Request.Cue = Cue;
		Request.Gain = Gain;
		Request.GridPosition = Position;
		Request.bAmbience = bAmbience;
		Request.bSpatial = Cue != ECityAudioCue::CityDistant && Cue != ECityAudioCue::StormRoar;
		Synchronize(Request);
	};
	// Mutually exclusive weather/intensity beds keep steady ambience within eight layers.
	Bed(ECityAudioCue::CityDistant, Simulation.GetServiceSnapshot().Residents > 0 ? .35f * (1 - Detail * .6f) : 0,
	    Focus);
	Bed(Weather.WindStrength >= 2 ? ECityAudioCue::WindGusts : ECityAudioCue::WindLight,
	    .18f + Weather.WindStrength * .08f, Focus);
	if (Rain)
		Bed(Weather.WindStrength >= 3   ? ECityAudioCue::RainDownpour
		    : Weather.WindStrength == 2 ? ECityAudioCue::RainHeavy
		    : Weather.WindStrength == 1 ? ECityAudioCue::RainModerate
		                                : ECityAudioCue::RainLight,
		    .65f, Focus);
	Bed(ECityAudioCue::ParkFoliage, ParkWeight * .65f * (Rain ? .45f : 1), ParkPosition);
	Bed(ECityAudioCue::ParkFountain,
	    Simulation.GetFireSnapshot().ActiveIncidents > 0 ? 0 : ParkWeight * .35f * Daylight, ParkPosition);
	Bed(Daylight > .35f && Needs.GoodsProduced > 0 ? ECityAudioCue::IndustryWorking : ECityAudioCue::IndustryIdle,
	    IndustryWeight * .55f, IndustryPosition);
	Bed(ECityAudioCue::MarketMurmur, MarketWeight * .45f * Daylight, MarketPosition);
	Bed(Weather.Temperature == ECityTemperatureBand::Hot ? ECityAudioCue::NightCricketsDense
	                                                     : ECityAudioCue::NightCricketsLight,
	    ParkWeight * (1 - Daylight) * (Rain || Weather.Temperature == ECityTemperatureBand::Cold ? 0 : .7f),
	    ParkPosition);
	if (Simulation.GetFireSnapshot().ActiveIncidents > 0)
	{
		const auto* Incident = Simulation.GetFireSnapshot().RecentIncidents.FindByPredicate(
		    [](const auto& Item) { return Item.ResolvedStep < 0; });
		if (Incident)
			if (const auto* Position = KnownEntities.Find(Incident->EntityId))
				Bed(ECityAudioCue::FireCrackle, .5f * CitySoundscape::AreaWeight(FVector::Dist2D(Focus, *Position), 18),
				    *Position, false);
	}
	if (WallSeconds >= NextAmbienceEvent && Settings->GetAmbienceVolume() > 0)
	{
		if (ParkWeight > .15f && !Rain && Weather.Temperature != ECityTemperatureBand::Cold && Daylight > .3f)
			PlayShot(static_cast<ECityAudioCue>(static_cast<uint8>(ECityAudioCue::BirdChirpA) + Random.RandRange(0, 2)),
			         ParkPosition, true, true, ParkWeight * .6f);
		else if (IndustryWeight > .15f && Daylight > .2f)
			PlayShot(ECityAudioCue::IndustryClank, IndustryPosition, true, true, IndustryWeight * .4f);
		NextAmbienceEvent = WallSeconds + Random.FRandRange(5, 12);
	}
	if (Rain && Weather.WindStrength >= 2 && WallSeconds >= NextThunder && Settings->GetAmbienceVolume() > 0)
	{
		PlayShot(Weather.WindStrength >= 3 ? ECityAudioCue::ThunderNear : ECityAudioCue::ThunderDistant, Focus, false,
		         true, .65f);
		NextThunder = WallSeconds + Random.FRandRange(15, 30);
	}
	// Steady engine stems follow the same interpolation/lane/height as rendered vehicles.
	// Retain selected IDs slightly longer at boundaries to avoid swapping engines every frame.
	struct FEngineCandidate
	{
		FCityAudioVoice Voice;
		double Rank;
	};
	TArray<FEngineCandidate> Engines;
	if (Settings->GetAmbienceVolume() > 0 &&
	    Traffic.NetworkFingerprint == FCityTrafficSimulation::GetNetworkFingerprint(Network))
		for (const auto& Vehicle : Traffic.Vehicles)
		{
			if (!CitySoundscape::HasEngineAudio(Vehicle))
				continue;
			if (Vehicle.SourceId &&
			    ((Vehicle.OriginEntityId &&
			      !Simulation.GetPlacementModel().GetEntities().Contains(Vehicle.OriginEntityId)) ||
			     (Vehicle.DestinationEntityId &&
			      !Simulation.GetPlacementModel().GetEntities().Contains(Vehicle.DestinationEntityId))))
				continue;
			const auto* Old =
			    Previous.NetworkFingerprint == Traffic.NetworkFingerprint
			        ? Previous.Vehicles.FindByPredicate([&](const auto& Other) { return Other.Id == Vehicle.Id; })
			        : nullptr;
			FVector Position, Facing;
			const double Alpha = Simulation.GetSimulationStepProgress();
			if (!FCityTrafficSimulation::SamplePosition(Vehicle, Old, Network, Alpha, Position, Facing))
				continue;
			const FCityCellCoord Cell(FMath::RoundToInt(Position.X), FMath::RoundToInt(Position.Y),
			                          FMath::RoundToInt(Position.Z));
			const double Lane = FCityTrafficSimulation::SampleMotorLaneOffsetCm(
			                        Vehicle, Old, Alpha, Simulation.GetPlacementModel().GetRoadDefinitionId(Cell)) /
			                    CityBoardPicking::LogicalCellSizeCm;
			FCityAudioVoice Request;
			Request.Key = {ECityAudioChannel::Traffic, Vehicle.Id};
			Request.Cue = CitySoundscape::TrafficLoop(Vehicle);
			Request.GridPosition = Position + FVector(.5 - Facing.Y * Lane, .5 + Facing.X * Lane, 0);
			Request.GridPosition.Z =
			    Board.GetRoadProfileHeight(Position.X + .5, Position.Y + .5, Position.Z > 0 ? 1 : 0) + 5;
			Request.bAmbience = true;
			Request.Gain = CitySoundscape::EngineGain(Request.Cue);
			if (VoiceDistanceGain(Request) <= .01f)
				continue;
			const double Rank =
			    FVector::DistSquared2D(Request.GridPosition, Focus) * (FindVoice(Request.Key) != INDEX_NONE ? .8 : 1.0);
			Engines.Add({Request, Rank});
		}
	Engines.Sort([](const auto& A, const auto& B)
	             { return A.Rank == B.Rank ? A.Voice.Key.Id < B.Voice.Key.Id : A.Rank < B.Rank; });
	for (int32 Index = 0; Index < FMath::Min(Engines.Num(), CitySoundscape::TrafficLimit); ++Index)
		Synchronize(Engines[Index].Voice);
	// Update bounded players each frame; no finished delegates can revive stopped phases.
	TMap<uint64, ECityAudioCue> ActiveAccents;
	for (int32 Index = 0; Index < Voices.Num(); ++Index)
	{
		auto& Voice = Voices[Index];
		if (!Voice.ResponseId || !Players[Index]->IsPlaying() || Voice.bRetiring)
			continue;
		const FCityAudioVoiceKey Response{ECityAudioChannel::Vehicle, Voice.ResponseId};
		if (!Eligible.Contains(Response))
		{
			Retire(Index, CitySoundscape::InterruptSeconds);
			continue;
		}
		const int32 Primary = FindVoice(Response);
		if (Primary != INDEX_NONE)
			Voice.GridPosition = Voices[Primary].GridPosition;
		ActiveAccents.Add(Voice.ResponseId, Voice.Cue);
	}
	for (int32 Index = 0; Index < Voices.Num(); ++Index)
	{
		auto& Voice = Voices[Index];
		auto* Player = Players[Index].Get();
		if (!Player->IsPlaying() || Voice.bRetiring)
			continue;
		const float Volume = Voice.bAmbience ? Settings->GetAmbienceVolume() : Settings->GetAlarmVolume();
		if (Volume <= 0 || (Voice.Key.Channel != ECityAudioChannel::Shot && !Wanted.Contains(Voice.Key)))
		{
			if (Volume > 0 && Voice.Key.Channel == ECityAudioChannel::Town && !Eligible.Contains(Voice.Key) && !Restore)
			{
				auto Outro = Voice;
				Outro.Key = {ECityAudioChannel::Shot, ++ShotSequence};
				Outro.Cue = ECityAudioCue::TownOutro;
				StartVoice(Outro, CitySoundscape::PhaseBlendSeconds);
			}
			Retire(Index, Voice.bAmbience && Volume > 0 ? .6f : CitySoundscape::InterruptSeconds);
			continue;
		}
		const auto* Accent =
		    Voice.Key.Channel == ECityAudioChannel::Vehicle ? ActiveAccents.Find(Voice.Key.Id) : nullptr;
		const float DuckTarget = Accent ? CitySoundscape::AccentPrimaryGain(Voice.Cue, *Accent) : 1.f;
		if (!FMath::IsNearlyEqual(DuckTarget, Voice.DuckTarget))
		{
			Voice.DuckTarget = DuckTarget;
			Voice.DuckRate = FMath::Abs(Voice.DuckGain - DuckTarget) / (DuckTarget < Voice.DuckGain ? .04f : .15f);
		}
		Voice.DuckGain = FMath::FInterpConstantTo(Voice.DuckGain, Voice.DuckTarget, Seconds, Voice.DuckRate);
		Voice.DistanceGain = CitySoundscape::Approach(Voice.DistanceGain, VoiceDistanceGain(Voice), Seconds, .1);
		Player->SetVolumeMultiplier(Voice.Gain * Voice.DistanceGain * Voice.DuckGain * Volume);
		const auto World = BoardTransform.TransformPosition(
		    FVector(Voice.GridPosition.X * CityBoardPicking::LogicalCellSizeCm,
		            Voice.GridPosition.Y * CityBoardPicking::LogicalCellSizeCm, Voice.GridPosition.Z + 100));
		const auto FocusWorld = BoardTransform.TransformPosition(Focus * CityBoardPicking::LogicalCellSizeCm);
		Player->SetWorldLocation(bHeadTracked ? World : Listener + (World - FocusWorld) * .05);
	}
}
