# City Sim VR playtest — playtest-2026.10.12.3

This update refines the Windows Desktop interface. The Windows ZIP also supports SteamVR PCVR; the separate Quest APK is the unchanged validated standalone build from playtest-2026.10.12.2.

- Bottom main-tool buttons have fixed slots. Editing tools (Inspect, Build, Zone, Demolish) are grouped before overlays, city policies and settings. Selected labels and contextual messages do not resize or shift these controls.
- Active-tool details, prices and placement/affordability warnings appear in their own bounded, scrollable strip above the tools. Rotate, road-height and cancel controls sit with that context.
- Time controls and city/menu/save/load actions use separate header rows. Selected-lot Inspect/upgrade information has a distinct section within City Info, keeping it separate from city-wide summaries.
- Gameplay rules, Reports data, shared actions and keyboard shortcuts are retained. This is an early Development playtest; future save/asset compatibility is optional and releases may require a fresh city. This UI update introduces no intentional save-format break.

The Editor and affected Windows package build pass. Focused Desktop smoke covers actual UMG button bounds, fixed-label fit, separate context ownership, menu/button/graphics/save behavior and native views at compact1024×720 and3840×2160. The compact pass preceded a header-clearance correction; final4K validates that correction and release extraction checks1280×800 on the final package. No full regression suite, new performance/thermal matrix or Android rebuild is part of this Desktop-only refinement. The retained signed Quest APK is checked for ARM64, embedded content and min32/target35. Native automation uses synthetic button input; hands-on review of the new Desktop arrangement, long warnings and hover/resize behavior remains pending. Previously reported Desktop/PCVR gameplay, art/audio and Reports acceptance remains recorded.

Download the named Windows ZIP and use Play-Desktop.cmd or Play-PCVR.cmd. Quest users may use the separate APK; no new Quest UI behavior is included. Automatic source-code archives contain distribution documents/helpers only.
