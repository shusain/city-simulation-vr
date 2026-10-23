# City Sim VR playtest-2026.10.23.1

Updated Windows Desktop / SteamVR PCVR and Quest standalone playtest packages from merged Gitea main `5d9c0f92400435143b3931279fa04b9043083871`.

## Changes since the October 21 playtest

- Shoreline wash and weather-driven waves, expanded beach scenery/leisure assets, and a curled kelp variant.
- Entrance-aware walking routes and shared road crosswalks.
- Refined desktop build submenus and city library; corrected overlapping City/Settings VR drawers.
- Opening tutorial guidance, restored building inspector previews, clearer selected tool buttons, and interaction continuity fixes.
- Six reviewed vehicle replacements with preserved emergency lights, plus recorded vehicle engine audio.
- Increased starting cash for Normal and Relaxed cities.

Mountain appearance work remains in review and is not part of this release. Further citizen-car window/variety refinements and vehicle road-grade orientation remain separate work.

## Download and play

Download **CitySimVR-playtest-2026.10.23.1-Windows.zip**, extract the entire ZIP to a short writable folder, and run **Play-Desktop.cmd**. **Play-PCVR.cmd** uses the same package with SteamVR/OpenXR. GitHub's automatic Source code archives contain distribution documents, not the game.

For Quest, download **CitySimVR-playtest-2026.10.23.1-Quest.apk**. This is a signed Development sideload build with embedded game data, ARM64, minimum API32 and target API35. Follow TESTING.md; an update replaces the existing `com.shaun.citysimvr.smoketest` app. Preserve its data and do not uninstall for ordinary updates.

**Start a new city for this test.** City format72; older formats and older specialized building footprints are unsupported. Preserve earlier cities and matching builds. Desktop and PCVR profiles remain outside the extracted package.

## Verification and limits

Windows555 passed Editor compilation with warnings as errors, ten focused vehicle/traffic/service cases, and packaged menu/startup smoke. Quest557 passed Editor/focused checks, signed ARM64/min32/target35 validation and byte comparison of all embedded containers against its cook. Quest source `00e9cee3de40388e627c08eec27dacda3c593a7d` differs from the Windows source only by the two-line package allowlist correction for the required AudioCredits.txt file; gameplay/assets are identical. Drone556 also passed the correction and two native interaction/tutorial visual cases. Exported Windows ZIP inventory/file hashes, named-launcher cold start and relaunch, focused menu actions/rendering at1280x800, missing-runtime handling and tampered/source-payload rejection checks passed in an isolated profile with a path containing spaces. Six menu captures plus the cold-start capture were retained; settled setup and confirmation views were reviewed. These are focused startup/menu checks, not a new full gameplay regression or physical headset test.

Physical PCVR/Quest rendering, input, comfort and current content observations remain unconfirmed. Independent clean-machine/laptop coverage and minimum hardware remain unconfirmed. No device installation or maintained local game replacement was performed. This is an early Development prerelease; broader balance and performance research remain deferred.

Read TESTING.md and KNOWN_ISSUES.md. Report the exact release ID, mode and reproduction steps with feedback.
