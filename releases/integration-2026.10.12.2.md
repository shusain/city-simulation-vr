# D58 traffic and alarm follow-up — integration-2026.10.12.2

Rain and distant vehicle-siren improvements are retained following the user’s PCVR feedback.

- Fires no longer trigger the generic incident notification chime. Visible fire alerts, crackle and responding sirens remain.
- Earthquakes use half their former city-wide warning window, with the existing short coast-down. Earthquake simulation and tornado warnings retain their timing.
- Regular cars, freight trucks and garbage trucks use quiet steady driving/idle loops. Two nearby vehicles share pooled audio components, follow their real route interpolation/lane/road height and crossfade states. Unreal handles spatialization and smoothed Doppler from actual emitter/listener velocity; samples contain no passing envelope.
- The total pool is bounded at32 components (including transitions), with independent saved effects/ambience controls and existing pause/menu/load/city-switch handling.

This is a local listening candidate. PCVR/Quest subjective traffic tone, final mix and motion, physical lifecycle checks and representative dense-city/device performance remain pending. Accepted park/building/citizen art and city compatibility are retained. No headset install or source/public release is included.
