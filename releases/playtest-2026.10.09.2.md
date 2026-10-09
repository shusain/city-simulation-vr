# playtest-2026.10.09.2

Desktop native-resolution rendering and graphics quality controls.

- Windows Desktop is DPI aware, so a4K monitor uses native3840x2160 rather than a resolution reduced by Windows scaling. Existing Borderless preferences refresh to the current desktop size.
- Borderless fills the display. Click MODE in Settings > GRAPHICS to choose Fullscreen/Windowed and expose display-resolution controls.
- SCENE SCALE offers50/75/100/125%;100% is native detail,125% supersamples. UI stays at display resolution. ANTI-ALIASING offers MSAA2x/4x/8x, default4x. Try8x for smoother object edges; reduce scene scale on slower hardware.
- APPLY previews, KEEP saves, and REVERT/close/Back or the15-second deadline restores previous settings, including quality.
- PCVR launchers preserve the previous mirror-window DPI behavior. VR input/HUD/quality and current64x64 maps/saves remain unchanged. Higher traffic budgets are benchmark-only.

Validation:206 rendered regressions; Windows and Android integration; compact1024x720 and3840x2160 packaged UI checks with40 captures; actual native4K startup and old Borderless-preference recovery. Android is not included or newly installed on a headset. Physical4K/AA appearance and multi-monitor/exclusive resolution transitions still need tester feedback. Previously reported fullscreen Alt-Tab works according to the developer's hands-on check.

On the developer's9800X3D/RTX5080, controlled native4K basic-city tests average155 FPS at300 buildings/1290 residents and67 FPS at744/3200 with normal1x traffic budgets. At5x the dense city averages44 FPS; expanded traffic can fall below30 and causes long CPU pauses. These are offscreen Development measurements, not a performance guarantee or a larger-map release. CPU work needs refinement before enabling larger sizes/richer traffic defaults.

Extract the complete ZIP and launch Play-Desktop.cmd or Play-PCVR.cmd. Play-Desktop-Windowed.cmd is the recovery option. Keep existing player profiles to retain saves/settings; source/editable assets are not part of this release. The first-launch permission/black-screen report's exact originating prompt remains unconfirmed; report the launch file, screenshot and log if it recurs.
