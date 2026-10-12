# City Sim VR playtest — playtest-2026.10.12.1

Windows Desktop/SteamVR PCVR and standalone Quest packages include the integrated building/park/citizen refinements and current soundscape.

- Smoother utility cylinders and skate ramps, refined park trees, smaller skateboards and revised forward-moving skate tricks. Park visitors use rigged citizen meshes with bounded animation and platform LODs; baseball/soccer poses and skate board/body choreography are retained.
- Industrial areas have quiet idle/working machinery beds and occasional clanks. All park definitions, including the simple/corner park, receive shared park ambience. Daytime birds and nighttime crickets vary with light and suitable weather; wind, rain and distant city ambience blend with camera distance. Sport-specific ball/board action sound effects are not part of the shared park ambience set.
- Rain sounds smoother and vehicle sirens are quieter at overview/panned-away distances. Fires do not play the generic incident notification chime; separate service alerts remain. Earthquake alarm windows are halved, with the short coast-down retained.
- Regular cars/freight/garbage trucks use steady driving/idle loops following actual route/lane/road-height motion, with Unreal spatialization and smoothed Doppler. Shared pools remain bounded; effects and ambience controls are saved independently.
- Current city compatibility and normal64 gameplay are preserved. Bigger-city backend/presentation work does not expose a larger playable map option.

Local checks include six audio/257 rendered regressions, native decoded playback/repeated loops, actual vehicle/alarm and lifecycle checks, Windows/Quest cooks/packages, packaged4K and extracted launcher/menu/save/cold/relaunch checks. The signed Quest payload is ARM64/min32/target35. The user confirms improved rain and siren isolation in PCVR; new mix/localization/comfort, remaining physical lifecycle/park checks, laptop and representative dense/device cost remain pending. No automatic headset installation occurs.

Download the named Windows ZIP (Desktop and PCVR share it) or the named Quest APK under Assets. The automatic source-code archives contain only distribution documentation and helpers.
