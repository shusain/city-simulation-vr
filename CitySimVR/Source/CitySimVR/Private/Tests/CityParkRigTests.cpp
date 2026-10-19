#include "Misc/AutomationTest.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Presentation/CityParkActivity.h"
#include "Presentation/CityBoardActor.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "SkeletalRenderPublic.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityParkMomentumTest, "CitySim.Parks.AuthoredMomentumAndReplay",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityParkMomentumTest::RunTest(const FString& Parameters)
{
	for (const auto Motion : {ECityParkMotion::SkateKickflip, ECityParkMotion::Skate180})
	{
		FCityParkMotionSample A, B;
		for (double T = .01; T < 2.9; T += .031)
		{
			FCityParkMotion::Sample(Motion, T, A);
			FCityParkMotion::Sample(Motion, T + .01, B);
			TestTrue(TEXT("Travel stays forward through launch, spin and landing"),
			         FMath::IsNearlyEqual(B.Body.GetLocation().X - A.Body.GetLocation().X, 1.6, .002));
			TestFalse(TEXT("Body rotation never creates nonfinite transforms"), A.Body.ContainsNaN());
		}
		FCityParkMotion::Sample(Motion, 2.6, A);
		FCityParkMotion::Sample(Motion, 2.7, B);
		TestTrue(TEXT("Landing keeps board and rider traveling together"),
		         FMath::IsNearlyEqual(B.Props[0].GetLocation().X - A.Props[0].GetLocation().X, 16.0, .002));
	}
	FCityParkMotionSample Pose;
	FCityParkMotion::Sample(ECityParkMotion::Skate180, 2.6, Pose);
	TestTrue(TEXT("180 lands with opposite facing and unchanged travel direction"),
	         Pose.Body.GetRotation().RotateVector(FVector::ForwardVector).X < -.99);
	FCityParkMotion::Sample(ECityParkMotion::Skate180, 4.5, Pose);
	TestTrue(TEXT("One-shot recycling occurs only after exit has hidden rider and board"),
	         Pose.Body.GetScale3D().IsNearlyZero() && Pose.Props[0].GetScale3D().IsNearlyZero());
	TestFalse(TEXT("Invalid times produce no cosmetic data"),
	          FCityParkMotion::Sample(ECityParkMotion::Walk, std::numeric_limits<double>::quiet_NaN(), Pose));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityParkRigIntegrationTest, "CitySim.Parks.NativeRigLODAndDeckContact",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityParkRigIntegrationTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	auto* Board = World->SpawnActor<ACityBoardActor>();
	for (int32 Index = 0; Index < 6; ++Index)
	{
		const auto* Mesh = Board->ParkCitizenSkeletonMeshes[Index].Get();
		if (!TestNotNull(TEXT("Each clothing/stance family has its native rig"), Mesh))
			continue;
		TestEqual(TEXT("Native skeleton retains exactly twenty authored bones"), Mesh->GetRefSkeleton().GetNum(), 20);
		TestNotNull(TEXT("Persisted native skeleton supports actual animation playback"), Mesh->GetSkeleton());
		TestTrue(TEXT("Every palette/stance family shares the clip skeleton"),
		         Mesh->GetSkeleton() == Board->ParkCitizenSkeletonMeshes[0]->GetSkeleton());
		TestEqual(TEXT("Native rig has the paired simpler mesh"), Mesh->GetLODNum(), 2);
		const auto* Data = Mesh->GetResourceForRendering();
		if (TestNotNull(TEXT("Skinned render resources exist"), Data))
		{
			TestEqual(TEXT("Near mesh retains accepted detailed geometry"),
			          static_cast<int32>(Data->LODRenderData[0].GetTotalFaces()), 754);
			TestEqual(TEXT("Quest/distant mesh retains accepted simpler geometry"),
			          static_cast<int32>(Data->LODRenderData[1].GetTotalFaces()), 420);
		}
	}
	for (const auto& Sequence : Board->ParkCitizenAnimations)
	{
		if (TestNotNull(TEXT("Each park clip is a real native animation"), Sequence.Get()))
			TestNotNull(TEXT("Each native clip retains its persisted skeleton binding"), Sequence->GetSkeleton());
	}
	for (const auto Motion :
	     {ECityParkMotion::SkateKickflip, ECityParkMotion::Skate180, ECityParkMotion::SkateLipStall})
	{
		auto* Component = NewObject<USkeletalMeshComponent>(Board);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetSkeletalMesh(Board->ParkCitizenSkeletonMeshes[3]);
		Component->RegisterComponent();
		Component->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		Component->SetAnimation(Board->ParkCitizenAnimations[static_cast<int32>(Motion) - 1]);
		FVector FirstHead = FVector::ZeroVector;
		bool ChangedPose = false;
		for (const double T : {.01, 1.3, 2.6, 2.9})
		{
			FCityParkMotionSample Pose;
			FCityParkMotion::Sample(Motion, T, Pose);
			Component->SetWorldTransform(Pose.Body);
			Component->SetPosition(Pose.AnimationSeconds, false);
			Component->TickAnimation(0, false);
			Component->RefreshBoneTransforms();
			const FVector Head = Component->GetBoneLocation(TEXT("head"), EBoneSpaces::ComponentSpace);
			TestTrue(TEXT("Native animation retains centimeter-scale adult stature"), Head.Z > 80 && Head.Z < 180);
			AddInfo(FString::Printf(TEXT("PARKANIMATION_NATIVE_POSE Motion=%d Time=%.3f Head=%s"), static_cast<int32>(Motion), T,
			                        *Head.ToString()));
			if (T == .01)
				FirstHead = Head;
			else
				ChangedPose |= !Head.Equals(FirstHead, .1);
			if (T == 1.3)
				continue; // Bone motion sample; kickflip feet intentionally leave deck.
			TArray<FFinalSkinVertex> Vertices;
			Component->GetCPUSkinnedVertices(Vertices, 0);
			if (!TestTrue(TEXT("Native skeletal evaluation produces vertices"), !Vertices.IsEmpty()))
				continue;
			double Lowest = TNumericLimits<double>::Max();
			for (const auto& V : Vertices)
				Lowest =
				    FMath::Min(Lowest, Component->GetComponentTransform().TransformPosition(FVector(V.Position)).Z);
			// Deck top is8cm in unscaled geometry, scaled.65 by the choreography.
			const double Deck = Pose.Props[0].TransformPosition(FVector(0, 0, 8)).Z;
			TestTrue(TEXT("Real imported skinned soles meet deck at approach/landing"), FMath::Abs(Lowest - Deck) < .2);
		}
		TestTrue(FString::Printf(TEXT("Native clip%d deforms the character through real bone evaluation"),
		                         static_cast<int32>(Motion)),
		         ChangedPose);
		Component->DestroyComponent();
	}
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityParkSportsContactTest, "CitySim.Parks.RefinedSportsContactsAndGrounding",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityParkSportsContactTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	auto* Board = World->SpawnActor<ACityBoardActor>();
	for (int32 Style = 0; Style < 3; ++Style)
	{
		const TCHAR* Styles[] = {TEXT("Casual"), TEXT("Office"), TEXT("Worker")};
		const FString MeshName = FString::Printf(TEXT("SK_Park_Field_%s"), Styles[Style]);
		auto* Mesh = LoadObject<USkeletalMesh>(
		    nullptr, *FString::Printf(TEXT("/Game/CitySim/Art/Parks/Meshes/%s.%s"), *MeshName, *MeshName));
		if (!TestNotNull(TEXT("Each field palette is a persisted native mesh"), Mesh))
			continue;
		auto* Component = NewObject<USkeletalMeshComponent>(Board);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetSkeletalMesh(Mesh);
		Component->RegisterComponent();
		Component->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		for (int32 LOD = 0; LOD < 2; ++LOD)
		{
			Component->SetForcedLOD(LOD + 1);
			for (const auto Motion :
			     {ECityParkMotion::BaseballPitch, ECityParkMotion::BaseballSwing, ECityParkMotion::SoccerKick,
			      ECityParkMotion::SoccerRun, ECityParkMotion::GoalkeeperSave})
			{
				const TCHAR* Clips[] = {TEXT("BaseballPitch"), TEXT("BaseballSwing"), TEXT("SoccerKick"),
				                        TEXT("SoccerRun"), TEXT("GoalkeeperSave")};
				const FString ClipName = FString::Printf(
				    TEXT("AS_Park_%s"),
				    Clips[static_cast<int32>(Motion) - static_cast<int32>(ECityParkMotion::BaseballPitch)]);
				auto* Sequence = LoadObject<UAnimSequence>(
				    nullptr, *FString::Printf(TEXT("/Game/CitySim/Art/Parks/Animations/%s.%s"), *ClipName, *ClipName));
				if (!TestNotNull(TEXT("Refined sport retains a real native clip"), Sequence))
					continue;
				TestFalse(TEXT("Cosmetic body motion never extracts authoritative root motion"),
				          Sequence->bEnableRootMotion);
				Component->SetAnimation(Sequence);
				const double Duration =
				    Motion == ECityParkMotion::BaseballPitch || Motion == ECityParkMotion::BaseballSwing ? 3.0 : 2.0;
				TestTrue(TEXT("Refined clip has the intended duration"),
				         FMath::IsNearlyEqual(Sequence->GetPlayLength(), Duration, .002));
				for (const double T : {0.0, .5, 1.0, 35.0 / 24.0, 2.0})
				{
					const double Time = FMath::Min(T, Duration - 1.0 / 24.0);
					FCityParkMotionSample Pose;
					FCityParkMotion::Sample(Motion, Time, Pose);
					Component->SetWorldTransform(Pose.Body);
					Component->SetPosition(Pose.AnimationSeconds, false);
					Component->TickAnimation(0, false);
					Component->RefreshBoneTransforms();
					TArray<FFinalSkinVertex> Vertices;
					Component->GetCPUSkinnedVertices(Vertices, LOD);
					const auto& Reference = Component->GetSkeletalMeshAsset()
					                            ->GetResourceForRendering()
					                            ->LODRenderData[LOD]
					                            .StaticVertexBuffers.PositionVertexBuffer;
					if (!TestTrue(TEXT("Actual field skin has the selected LOD's vertices"),
					              Vertices.Num() == static_cast<int32>(Reference.GetNumVertices())))
						continue;
					FVector Hands[2] = {FVector::ZeroVector, FVector::ZeroVector};
					int32 Counts[2] = {0, 0};
					double Lowest = TNumericLimits<double>::Max();
					for (int32 I = 0; I < Vertices.Num(); ++I)
					{
						const FVector Position =
						    Component->GetComponentTransform().TransformPosition(FVector(Vertices[I].Position));
						Lowest = FMath::Min(Lowest, Position.Z);
						const auto& Rest = Reference.VertexPosition(I);
						// Low hand rings have rigid hand weights. These independent
						// mesh centroids catch the old bone-tip/palm offset error.
						if (Rest.Z >= 89.9f && Rest.Z <= 90.1f && FMath::Abs(Rest.Y) > 20)
						{
							const int32 Hand = Rest.Y < 0 ? 0 : 1; // Imported Blender .L is native -Y.
							Hands[Hand] += Position;
							++Counts[Hand];
						}
					}
					TestTrue(TEXT("Refined native soles retain contact with the field"),
					         FMath::Abs(Lowest - Pose.Body.GetTranslation().Z) < .5);
					if (!TestTrue(TEXT("Both real mesh palms are found"), Counts[0] > 0 && Counts[1] > 0))
						continue;
					for (int32 I = 0; I < 2; ++I)
						Hands[I] /= Counts[I];
					if (Motion == ECityParkMotion::BaseballPitch && Time <= 35.0 / 24.0)
						TestTrue(TEXT("Pitch ball stays with the actual throwing palm until release"),
						         Hands[1].Equals(Pose.Props[0].GetLocation(), 2.5));
					if (Motion == ECityParkMotion::BaseballSwing)
					{
						TestTrue(TEXT("Right palm holds the bat handle"),
						         Hands[1].Equals(Pose.Props[1].TransformPosition(FVector(0, 0, -42.5)), 2.5));
						TestTrue(TEXT("Left palm holds the adjacent bat grip"),
						         Hands[0].Equals(Pose.Props[1].TransformPosition(FVector(0, 0, -36)), 2.5));
					}
					if (Motion == ECityParkMotion::GoalkeeperSave && Time == 1)
						TestTrue(TEXT("Keeper catches the ball with both mesh hands"),
						         FVector::Distance(Hands[0], Pose.Props[0].GetLocation()) < 20 &&
						             FVector::Distance(Hands[1], Pose.Props[0].GetLocation()) < 20);
				}
			}
		}
		Component->DestroyComponent();
	}
	FCityParkMotionSample Pitch, Swing;
	FCityParkMotion::Sample(ECityParkMotion::BaseballPitch, 2.0, Pitch);
	FCityParkMotion::Sample(ECityParkMotion::BaseballSwing, 2.0, Swing);
	TestTrue(TEXT("Paired baseball hands off one visible ball at bat contact"),
	         Pitch.Props[0].GetScale3D().IsNearlyZero() && Swing.Props[0].GetScale3D().X > .99);
	TestTrue(TEXT("Ball meets the evaluated bat barrel at the synchronized contact sample"),
	         Swing.Props[0].GetLocation().Equals(Swing.Props[1].TransformPosition(FVector(0, 0, 27.5)), .05));
	FCityParkMotionSample KickStart, KickBefore;
	FCityParkMotion::Sample(ECityParkMotion::SoccerKick, 0, KickStart);
	FCityParkMotion::Sample(ECityParkMotion::SoccerKick, .9, KickBefore);
	TestTrue(TEXT("Soccer ball remains stationary before the foot strikes"),
	         KickStart.Props[0].GetLocation().Equals(KickBefore.Props[0].GetLocation(), .001));
	World->DestroyWorld(false);
	return true;
}
#endif
