# City Sim VR playtest-2026.10.19.1

Updated Windows playtest with the accepted building, terrain-access and civic asset work since the October15 release.

## Changes

- Updated residential, commercial and industrial buildings with visible entrances, concrete foundations beneath the structures, terrain-following pedestrian paths, stairs/railings and service/loading approaches.
- Updated technology, education, police, fire, hospital, power, water, waste and recycling buildings with approved detailed models, materials and distant LODs. Wind-turbine casing/rotor faces and civic detail face winding are corrected.
- Buildings with wide service entrances require roads along the full entrance frontage. Specialized plots are sized for their buildings; ordinary low-density zoning keeps its2x2 footprint.
- Includes intervening startup/menu-focus, technology navigation and household shopping/service fixes.

## Download and play

Download **CitySimVR-playtest-2026.10.19.1-Windows.zip** under Assets, extract fully to a short writable folder, then run **Play-Desktop.cmd**. **Play-PCVR.cmd** uses SteamVR/OpenXR with the same Windows package; current headset behavior is unconfirmed. GitHub’s automatic Source code archives contain documents, not the game.

**Start a new city.** This build uses city format72, game revision `dev-9553954bf7e67bf990a5`. Older formats and cities using previous specialized plot sizes may fail validation. Existing city files are preserved; retain copies and the matching older build if you want to revisit them. Desktop and PCVR keep their separate persistent save profiles.

## Verification and limits

The building integration was accepted in Windows gameplay. Automated checks cover the focused Editor cases,34 reviewed native building views, a saved17-site hilly review city, packaged menu/startup and focused Client checks at1280x800 and1024x720. All224 required cooked packages and53 candidate runtime files were verified. Distribution-specific archive, launcher and download results are recorded separately with this release.

This is an unsigned Development prerelease. Independent clean-machine/laptop coverage, physical PCVR/Quest observations and minimum hardware remain unconfirmed. Very steep vehicle driveways still need a separate grade review. No new Quest APK is included. Read TESTING.md and KNOWN_ISSUES.md before testing; include this release ID with feedback.
