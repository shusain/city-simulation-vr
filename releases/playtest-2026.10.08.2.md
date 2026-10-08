# City Sim VR playtest-2026.10.08.2

Early development playtest for Windows desktop and SteamVR PCVR. Download the named Windows ZIP from Assets, extract fully to a short path, then use Play-Desktop.cmd or Play-PCVR.cmd. Read TESTING.md and KNOWN_ISSUES.md in the download. The automatic Source code archives do not contain the game.

This build includes compact desktop menus/side-panel controls, shared debug-only direct home/shop placement, current weather/terrain presentation and the existing shared city simulation. Homes and shops normally grow from zoning.

Verified on the development Windows11 / RTX5080 workstation: all50 bundled runtime/document/launcher files pass SHA256/inventory checks; a freshly extracted ZIP launches through Play-Desktop.cmd from a folder with spaces; packaged synthetic menu/button/render/save smoke passes with exit0 and11 native1280x800 screenshots. This checks packaged behavior and rendering, not physical input or clean-machine prerequisites. PCVR uses the established D3D11/OpenXR profile, but this particular extracted download has not had a fresh headset check. Laptop performance, clean-machine installation and external feedback remain pending. No Quest APK in this release.

Report issues using the repository's Bug report or Playtest feedback form. Include build ID playtest-2026.10.08.2, mode, hardware/runtime, reproduction steps and city/month/weather/speed. Review logs for personal paths before attaching them.

Windows ZIP:443,762,634 bytes (about423MiB). SHA256: `1F430B821363735FE78DD4EA337D7C2ABEF233E578FC0DA06F88166620B9B909`.

Release attachments: CitySimVR-playtest-2026.10.08.2-Windows.zip, release-manifest.json, SHA256SUMS.txt. Saves persist in the separate Desktop/PCVR tester profiles described in TESTING.md.
