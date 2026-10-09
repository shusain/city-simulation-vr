# City Sim VR playtest-2026.10.09.1

Windows Desktop / SteamVR PCVR playtest. Extract the complete Windows ZIP to a short writable path, close older copies, then use Play-Desktop.cmd or Play-PCVR.cmd. Existing mode profiles/cities persist across updates.

## Desktop startup and display

- Desktop first launch fills the display using native-resolution borderless fullscreen. Later launches use confirmed display preferences.
- Settings > GRAPHICS provides Fullscreen, Borderless and Windowed, resolution choices and VSync. Borderless uses desktop resolution. Apply previews a mode; Keep saves it. Revert, closing the dialog or the 15-second wall-time deadline restores the prior mode.
- Play-Desktop-Windowed.cmd provides session-only 1280x800 recovery without erasing saved preferences.
- The ZIP omits Unreal's generic Windows bootstrapper, which does not select Desktop mode. Named launchers start the inner game directly with explicit mode/profile flags.
- Missing Visual C++ runtime checks stop before starting a game or installer. Finish the included x64 runtime installation separately, then launch the game. Its installer may need administrator permission; the game executables declare asInvoker and the desktop launcher does not elevate. Windows trust warnings for unsigned downloads remain separate.

## Validation and feedback

203 rendered CitySim regressions pass, including supported-resolution filtering, preview/edit guards, confirmation and timeout/cancel recovery. Editor compilation and Windows packaging pass. Release validation checks package inventory/per-file/ZIP hashes, missing-runtime rejection, a fresh-profile entry-menu launch and a subsequent launch with real synthetic desktop menu/render/save/graphics checks (20 captures). Publication verifies server digests and anonymous downloads. Shared simulation/save/map sizes are unchanged; VR menu choices remain unchanged. No Quest APK is included.

Physical fullscreen/multi-monitor/Alt-Tab/DPI and fresh headset checks remain pending. The reported first-launch permission prompt/black screen needs user retesting with the corrected named launch path; the exact originating prompt/file has not yet been confirmed. Include the launch file, exact prompt text, mode, build, hardware and latest launch log if it recurs. Use the bug/feedback issue forms for other findings.
