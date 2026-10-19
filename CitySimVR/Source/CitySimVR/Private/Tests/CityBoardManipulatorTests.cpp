#include "Interaction/CityBoardManipulator.h"
#include "Interaction/CityBoardPicking.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityBoardManipulatorTest, "CitySim.Input.BoardManipulation",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityBoardManipulatorTest::RunTest(const FString& Parameters)
{
	FCityBoardManipulator Manipulator;
	FCityBoardPresentationPose Pose;
	Pose.Location = FVector(10.0, 20.0, 100.0);
	Pose.YawDegrees = 15.0;
	Pose.UniformScale = 200.0 / CityBoardPicking::LogicalBoardSizeCm;

	Manipulator.Reinitialize(false, true, true, true, FVector::ZeroVector, FVector(50.0, 0.0, 120.0), Pose);
	TestEqual(TEXT("Dominant grip begins one-hand mode"), Manipulator.GetMode(), ECityBoardGestureMode::OneHand);
	TestTrue(TEXT("One-hand update produces a pose"),
	         Manipulator.Update(false, true, true, true, FVector::ZeroVector, FVector(70.0, -10.0, 130.0), Pose));
	TestTrue(TEXT("One-hand drag follows controller"), Pose.Location.Equals(FVector(30.0, 10.0, 110.0), 0.01));
	TestEqual(TEXT("One-hand drag preserves yaw"), Pose.YawDegrees, 15.0);

	Manipulator.Reinitialize(true, true, true, true, FVector(-50.0, 0.0, 120.0), FVector(50.0, 0.0, 120.0), Pose);
	TestEqual(TEXT("Both grips begin two-hand mode"), Manipulator.GetMode(), ECityBoardGestureMode::TwoHand);
	TestTrue(TEXT("Two-hand update produces a pose"),
	         Manipulator.Update(true, true, true, true, FVector(0.0, -100.0, 130.0), FVector(0.0, 100.0, 130.0), Pose));
	TestTrue(TEXT("Two-hand gesture rotates about yaw"), FMath::IsNearlyEqual(Pose.YawDegrees, 105.0, 0.01));
	TestTrue(TEXT("Two-hand gesture scales uniformly"),
	         FMath::IsNearlyEqual(Pose.UniformScale, 400.0 / CityBoardPicking::LogicalBoardSizeCm, 0.000001));
	TestTrue(TEXT("Scale respects maximum clamp"), Pose.UniformScale <= FCityBoardManipulator::MaximumScale);
	TestTrue(TEXT("corner offset rotates/scales about controller midpoint rather than the actor origin"),
	         Pose.Location.Equals(FVector(-20, 60, 110), .01));

	Manipulator.Reinitialize(true, true, true, true, FVector(0.0, -100.0, 130.0), FVector(0.0, 100.0, 130.0), Pose);
	TestFalse(
	    TEXT("Releasing one grip reinitializes without a jump"),
	    Manipulator.Update(false, true, true, true, FVector(0.0, -100.0, 130.0), FVector(0.0, 100.0, 130.0), Pose));
	TestEqual(TEXT("Remaining grip continues in one-hand mode"), Manipulator.GetMode(), ECityBoardGestureMode::OneHand);
	TestFalse(
	    TEXT("Lost tracking pauses and reinitializes the gesture"),
	    Manipulator.Update(false, true, true, false, FVector(0.0, -100.0, 130.0), FVector(0.0, 100.0, 130.0), Pose));
	TestEqual(TEXT("No tracked held hand leaves no active gesture"), Manipulator.GetMode(),
	          ECityBoardGestureMode::None);
	TestFalse(
	    TEXT("Tracking recovery reinitializes without a jump"),
	    Manipulator.Update(false, true, true, true, FVector(0.0, -100.0, 130.0), FVector(0.0, 100.0, 130.0), Pose));
	TestEqual(TEXT("Recovered hand returns to one-hand mode"), Manipulator.GetMode(), ECityBoardGestureMode::OneHand);
	const auto BeforeHandoff = Pose;
	TestFalse(TEXT("same-frame right-to-left grip handoff rebases without a jump"),
	          Manipulator.Update(true, false, true, true, FVector(-25, 10, 100), FVector(0, 100, 130), Pose));
	TestTrue(TEXT("handoff retains current board transform"), Pose.ToTransform().Equals(BeforeHandoff.ToTransform()));
	Manipulator.Update(true, false, true, true, FVector(-15, 20, 110), FVector(0, 100, 130), Pose);
	TestTrue(TEXT("new single hand owns the drag"), Pose.Location.Equals(BeforeHandoff.Location + FVector(10, 10, 10)));

	Manipulator.NudgeHeight(1000.0, Pose);
	TestEqual(TEXT("Height clamps at maximum"), Pose.Location.Z, FCityBoardManipulator::MaximumHeightCm);
	Manipulator.NudgeHeight(-1000.0, Pose);
	TestEqual(TEXT("Height clamps at minimum"), Pose.Location.Z, FCityBoardManipulator::MinimumHeightCm);
	Pose.UniformScale = 0.0;
	Manipulator.Reinitialize(false, true, true, true, FVector::ZeroVector, FVector::ZeroVector, Pose);
	TestTrue(TEXT("Gesture update normalizes the presentation pose"),
	         Manipulator.Update(false, true, true, true, FVector::ZeroVector, FVector::ZeroVector, Pose));
	TestTrue(TEXT("Scale respects minimum clamp"), Pose.UniformScale >= FCityBoardManipulator::MinimumScale);

	const FVector HeadLocation(300.0, 400.0, 165.0);
	const FVector HeadForward(0.0, 1.0, 0.2);
	const FCityBoardPresentationPose Recentered = FCityBoardManipulator::MakeRecenteredPose(
	    HeadLocation, HeadForward, 200.0 / CityBoardPicking::LogicalBoardSizeCm);
	const FVector RecenteredCenter = Recentered.ToTransform().TransformPosition(
	    FVector(CityBoardPicking::LogicalBoardSizeCm * 0.5, CityBoardPicking::LogicalBoardSizeCm * 0.5, 0.0));
	TestTrue(TEXT("Recenter places board center in front of head"),
	         RecenteredCenter.Equals(FVector(300.0, 510.0, 110.0), 0.01));
	TestTrue(TEXT("Recenter keeps pitch and roll level"),
	         FMath::IsNearlyZero(Recentered.ToTransform().Rotator().Pitch) &&
	             FMath::IsNearlyZero(Recentered.ToTransform().Rotator().Roll));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityBoardGrabPivotTest, "CitySim.Input.BoardGrabPivot",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityBoardGrabPivotTest::RunTest(const FString& Parameters)
{
	const FVector LocalCenter(CityBoardPicking::LogicalBoardSizeCm * .5, CityBoardPicking::LogicalBoardSizeCm * .5, 0);
	for (const double Yaw : {0., 72., 179., -179.})
	{
		for (const double Width : {100., 200., 400.})
		{
			for (const double Ratio : {.1, .7, 1., 1.5, 8.})
			{
				FCityBoardPresentationPose Start;
				Start.Location = FVector(130, -240, 100);
				Start.YawDegrees = Yaw;
				Start.UniformScale = Width / CityBoardPicking::LogicalBoardSizeCm;
				for (const FVector Offset : {FVector::ZeroVector, FVector(40, -25, 5)})
				{
					const FVector Pivot = Start.ToTransform().TransformPosition(LocalCenter) + Offset;
					const FVector LocalGrab = Start.ToTransform().InverseTransformPosition(Pivot);
					const FVector Translation(15, -12, 0);
					const FVector Span(60, 0, 0);
					FCityBoardManipulator Gesture;
					Gesture.Reinitialize(true, true, true, true, Pivot - Span, Pivot + Span, Start);
					FCityBoardPresentationPose Result = Start;
					const FVector NewSpan = FQuat(FRotator(0, 105, 0)).RotateVector(Span * Ratio);
					TestTrue(TEXT("combined off-origin gesture updates"),
					         Gesture.Update(true, true, true, true, Pivot + Translation - NewSpan,
					                        Pivot + Translation + NewSpan, Result));
					TestTrue(
					    TEXT("same logical grabbed point follows moving midpoint across yaw wrap and scale limits"),
					    Result.ToTransform().TransformPosition(LocalGrab).Equals(Pivot + Translation, .0001));
					if (Offset.IsNearlyZero())
						TestTrue(
						    TEXT("controllers straddling center rotate/scale around center"),
						    Result.ToTransform().TransformPosition(LocalCenter).Equals(Pivot + Translation, .0001));
					const auto Held = Result;
					Gesture.Update(true, true, true, true, Pivot + Translation - NewSpan, Pivot + Translation + NewSpan,
					               Result);
					TestTrue(TEXT("unchanged controllers do not accumulate drift"),
					         Held.ToTransform().Equals(Result.ToTransform()));
					Gesture.Reinitialize(true, true, true, true, Pivot + Translation - NewSpan,
					                     Pivot + Translation + NewSpan, Result);
					Gesture.Update(true, true, true, true, Pivot + Translation - NewSpan, Pivot + Translation + NewSpan,
					               Result);
					TestTrue(TEXT("gesture restart holds pose"), Held.ToTransform().Equals(Result.ToTransform()));
				}
			}
		}
	}
	return true;
}

#endif
