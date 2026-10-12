# City Sim VR integration-2026.10.12.1

Focused D58 rain and vehicle-siren mix correction; accepted D83 art and other gameplay/audio remain as reviewed.

- Four rain intensities use longer, softer droplet attacks and more continuous water wash at the existing -39/-36/-33/-30 LUFS targets.
- Responding vehicle sirens and horn/alternate accents follow camera-to-emitter distance in city coordinates. Nearby mix remains recognizable; overview/panned-away sources are much quieter and zoom changes fade smoothly. Town warnings and saved volume controls are preserved.
- Editor warnings-as-errors, five focused audio checks and all256 rendered regressions pass. Eight native decoded rain recordings and four repeated loops match source PCM with no loop gaps. Standard Windows and signed ARM64 Quest ASTC builds pass.

Try rain without an active fire, compare a responding vehicle at close zoom and city overview, then pan away and rotate/scale the board. Check horns/alternate sirens, pause/resume and city/menu transitions. Physical listening and Quest/PCVR comfort/timing remain pending until tested.

This is a local candidate. No installation or public release. The previous integration-2026.10.11.1 package remains available for comparison. Source backup dd787a6 precedes these uncommitted focused corrections.
