# City Sim VR playtest-2026.10.08.4

Windows Desktop / SteamVR PCVR playtest. Download the Windows ZIP, extract fully to a short writable path, close any older copy, then run Play-Desktop.cmd or Play-PCVR.cmd. Launcher save profiles persist across updates. See TESTING.md and KNOWN_ISSUES.md.

## Desktop improvements

- MAIN MENU opens Continue, Load City, New City and Settings.
- Pause and 1x/3x/5x highlight the active state; redundant desktop calendar speed text is removed.
- Current build choices are highlighted, including alternative power and water facilities.
- Zoning stays open while painting. Z selects Residential, then cycles Commercial / Industrial / Residential. Esc or CLOSE dismisses the panel.
- Mouse panning and rotation are doubled again relative to the previous local candidate (four times the initial public playtest).

The prior UI round is user-accepted overall. The additional camera sensitivity still needs hands-on feedback. Shared simulation, save format and map sizes are unchanged.

## Validation and feedback

The prior UI round passed 202 rendered regressions, Windows/Android integration and native desktop checks at 1024x720, 1280x800 and 1920x1080. This camera update passes Editor compilation and both focused desktop regressions (camera bounds/direction and shared UI/action guards). The release pipeline packages Windows, validates runtime inventory/per-file/ZIP checksums, and runs the actual extracted desktop launcher with isolated synthetic menu/button/render/save checks and 18 captures at 1280x800 before publication. Publication validates GitHub asset digests and anonymous downloads.

Physical input/focus/DPI/laptop coverage and fresh PCVR headset regression remain pending. No Quest APK is included. Report the build ID, mode, hardware, city/month/weather/speed and reproduction through this repository's bug/feedback issue forms.
