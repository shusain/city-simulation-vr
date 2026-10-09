# Getting started

1. Download the named Windows ZIP from **Releases > Assets** and extract the entire ZIP to a short writable path, such as `C:\Games` (the ZIP contains a `CitySimVR` folder). Deeply nested extraction paths can prevent Windows from loading runtime DLLs. Do not run from inside the ZIP or copy only the executable.
2. Double-click **Play-Desktop.cmd** for keyboard/mouse, or **Play-PCVR.cmd** after starting SteamVR and connecting the headset. PCVR uses the system's active OpenXR runtime; select SteamVR as that runtime in SteamVR settings. The tested setup is Quest 3 with Steam Link Beta USB and SteamVR Beta.
3. If Windows reports a missing Visual C++ runtime, run the included `Windows/Engine/Extras/Redist/en-us/vc_redist.x64.exe`, then retry. Unreal Editor, Visual Studio and the project source are not needed.
4. Read `BUILD.txt` for the exact build ID and development configuration. Use that ID in feedback.

Windows 64-bit is the target; Windows 11 has been tested. Laptop performance and minimum hardware are still being evaluated. Both launchers select D3D11, the current tested rendering profile. Desktop play does not need an installed headset or VR runtime.

## Controls

Desktop: WASD/arrows or middle mouse drag to pan, right mouse drag to orbit, wheel to zoom, Home to reset. Left click places/selects; R rotates; Esc cancels or closes a menu. Space pauses; 1/2/3 select 1x/3x/5x. The active time button is highlighted and marked with >. Z selects residential zoning; repeated presses cycle commercial, industrial, then residential. F5 saves, F9 opens load review. Brackets change road height. Side panels have independent SHOW/HIDE toggles and HIDE buttons. MAIN MENU opens Continue/Load/New City/Settings. Most completed tool choices close their dialog. Zoning stays open so you can switch types while painting; map/camera input remains available outside that panel. Esc, CLOSE or the ZONE toggle dismisses it. Other open dialogs close on an outside map click and consume that click.

Use **ZONE** for homes and shops to grow normally. Direct BUILD HOME/BUILD SHOP is reserved for Settings > Debug Mode > Debug Tools. VR retains its controller tools and shared menus.

## First playtest

- Create a city, connect roads from the outside connection, place power/water and zone residential/commercial areas. Follow Objectives and Inspect service warnings.
- Try camera movement, tool selection, road cancellation and the side-panel toggles. Check that menus close after completed choices and do not accidentally place something behind them.
- Run at 1x/3x/5x through month boundaries. Describe any pauses, confusing income figures or unreadable warnings.
- Save, quit, relaunch with the same mode, and load the city. Try returning to the menu and switching cities.
- For PCVR, also check both hands, board scaling/rotation, pointer/menu interaction and headset/dashboard pause/resume.

## Saves, logs and updates

The launchers keep data outside the downloaded build, so extracting a newer build to another folder preserves it:

- Desktop: `%LOCALAPPDATA%/CitySimVRPlaytest/Desktop/Saved/SaveGames/Cities`
- PCVR: `%LOCALAPPDATA%/CitySimVRPlaytest/PCVR/Saved/SaveGames/Cities`
- Latest launch log: the corresponding mode's `Logs/Latest.log` (replaced on the next launch).

Desktop and PCVR have separate profiles. Keep copies of important saves when trying early builds. Removing the extracted download removes the game files; the profile folders above remain. Removing a profile folder also removes that mode's saved cities/settings.

## Feedback

Open **Issues > New issue** and choose Bug report or Playtest feedback. Include build ID, mode, hardware/headset, city/month/weather/speed, exact steps and expected versus observed behavior. A screenshot/video and the affected city JSON in a ZIP help reproduce placement/service issues. For performance, include resolution and whether menus/weather were visible.

Attach logs/saves only when relevant and review them before posting: logs can contain your Windows username and local paths. Share feedback manually; the game does not submit a GitHub issue for you.

## Optional Quest download

If a release includes a Quest APK, use an authorized sideload setup with headset developer mode. The current test application ID is `com.shaun.citysimvr.smoketest`; installing an update replaces that test app. Do not uninstall an existing copy to update it, since uninstalling removes its app data. Quest saves remain in that app's data, separate from Windows profiles. Check the release notes for device-validation status; a successful Android package is not a new headset test.
