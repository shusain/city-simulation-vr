# Google Material Symbols

This directory vendors an approved icon subset of Google Material Symbols from
https://github.com/google/material-design-icons at the upstream revision recorded
in `UPSTREAM_COMMIT`.

The icons are licensed under the Apache License, Version 2.0. The complete
upstream license is retained in `LICENSE`. The game may use, modify, and
commercially distribute these icons subject to that license.

Vendored outlined 48 px symbols:

- `download` — Save city
- `upload` — Load city
- `play_arrow` — Start/resume
- `restart_alt` — Reset
- `arrow_back` — Back
- `close` — Close
- `settings` — Game settings (menu positioning, persistence, and reset)
- `bar_chart` — Show/hide city needs and summary
- `build` — Building needs repair
- `construction` — Repair underway
- `local_police` — Active crime incident

The PNG files under `Derived` and Unreal assets under `/Game/CitySim/UI/Icons`
are mechanical derivatives of these SVGs and remain covered by the same license.


## D116 condition family (v1, approved 2026-10-15)

The additional thirteen outlined 48 px symbols use the same pinned upstream revision and Apache-2.0 license.
`D116-Sources-v1.json` records exact upstream paths, source hashes and semantic mappings;
`D116-Approval-v1.json` records the user's approval of all thirteen IDs and reviewed derivative hashes.
The Sources manifest retains its original preview status to preserve the exact approved bytes; the separate approval supersedes that status.
The review sheet and transparent 256 px validation are under `Artifacts/D116/SymbolReview/v1`.
`Tools/Import-D116ConditionIcons.py` verifies both source and derivative hashes before using the existing M_CityUiIcon contract.
