# City Sim VR playtest-2026.10.21.1

Updated Windows Desktop / SteamVR PCVR tester build.

## Changes since the October 19 playtest

- Reports now offers actual Hourly, Daily, five-day Weekly and Monthly observations, with Monthly as the default. Financial settlement remains daily; aggregation follows the longer game clock. Hourly replaces Annual, with no backfilled hourly data.
- Reports explains that charts appear after the first daily settlement. Time ranges and period/metric controls have separate button rows. Housing remains in City.
- Building placement shows the rotated footprint, aligns to nearby roads and explains rejected sites, including required full entrance frontage.
- Includes the accepted residential, commercial, industrial, civic, technology and utility asset/foundation/path updates from the previous release.
- Both automation fixtures are repaired: full smoke uses real daily settlements and safe report selection; shopping/outpatient regression cities now match current building footprints.

## Download and play

Download **CitySimVR-playtest-2026.10.21.1-Windows.zip** under Assets, extract the whole ZIP to a short writable folder and run **Play-Desktop.cmd**. **Play-PCVR.cmd** uses the same Windows package with SteamVR/OpenXR. GitHub's automatic Source code archives contain distribution documents, not the game.

**Start a new city for this test.** City format72; game revision `dev-32bac1e3b65eef95ba3d`. Previous specialized plot sizes may fail validation even when the format matches. Preserve older cities and matching builds. Existing Desktop/PCVR save profiles are retained by the launchers.

## Verification and limits

Drone399 passed Editor compilation, the captured-city shopping/outpatient regressions, full packaged smoke at1280x800 and1024x720, and packaged report-fixture checks at both sizes. Full smoke exercises real daily/monthly/hourly history, report navigation, saving and live inspector previews. The reports and placement UI were previously accepted in Windows gameplay. Separate exported-ZIP/launcher QA was intentionally skipped for this release; please report any extraction/startup problems with this build ID.

Unsigned Development prerelease. Physical PCVR/Quest behavior, independent clean-machine/laptop coverage and minimum hardware remain unconfirmed. Very steep vehicle driveway grades remain under separate review. No new Quest APK is included. Read TESTING.md and KNOWN_ISSUES.md; report the release ID, mode and reproduction steps with feedback.
