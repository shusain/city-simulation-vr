# City Sim VR playtest-2026.10.10.1

This playtest adds a standalone Quest APK alongside the Windows Desktop/SteamVR PCVR ZIP, and reduces repeated CPU work in city simulation and presentation. Game source and editable assets are maintained separately; these downloads contain the built game.

- Windows: extract the full named ZIP and use Play-Desktop.cmd or Play-PCVR.cmd. Play-Desktop-Windowed.cmd is the display-recovery launcher.
- Quest: download the named Quest.apk and follow TESTING.md for developer-mode USB sideloading. The current headset app label is City Sim VR Smoke Test. Update with adb install -r; ordinary updates should not uninstall the app or remove saved cities. The APK contains its game data.
- Use this release ID in feedback. Windows BUILD.txt and the APK filename identify the download; the manifest and SHA256SUMS identify its exact bytes.

## Changes

Unchanged render frames reuse services/needs and terrain meshes, road topology refreshes only after relevant edits, weighted routing uses a deterministic heap, and redundant service/half-day work is removed. Real station completions still refresh immediately. Fixed-step simulation, monthly settlement, save compatibility and the existing VR/desktop HUD contents remain authoritative.

The release retains the normal64x64 map and normal traffic budgets. Dense larger-map experiments still show long CPU pauses and reduced fast-forward speed; experimental executables are not included. Growth cadence, private zoned-development funding and demand for mixed densities are planned gameplay changes, not included in this build.

## Verified coverage

- Warnings-as-errors Editor compilation and210 rendered CitySim regressions, including paused edits, fixed-step invalidation, half-day station completion, deterministic routing and terrain-buffer reuse.
- Normal Windows and Android ARM64/Vulkan/ASTC build/cook/package. APK package identity, min32/target35, ARM64 ABI, signature and embedded cooked-content inventory checked.
- Dense native4K workstation benchmarks with real roads, buildings, residents, jobs, services and traffic across month boundaries; these are controlled mature fixtures, not ordinary-budget progression or laptop/headset frame-rate acceptance.
- Extracted Windows cold start/relaunch, menu/graphics/save checks with20 captures, runtime/rejection checks, source-free inventories and per-file/outer checksums. Published asset sizes/digests and anonymous download checks cover Windows and Quest.

Fresh physical Quest sideload/update, standalone stereo/input/lifecycle and PCVR performance checks remain pending for this exact release, as do a clean external Windows machine and representative laptop. Earlier headset appearance/control acceptance does not establish those new checks. Please report hardware, release/mode, city/month/weather/speed and reproduction through the repository's bug/feedback forms.
