# Approved recorded vehicle engines — #54 source / #55 integration

Source approval: submitted v4 form7191134b-1e9b-4c17-b401-aff3886ca92a for
car idle, truck idle and stationary truck; explicit chat approval2026-10-21 for
both v5 car-running A/B. #54 owns evidence and closes source-selection scope.
In-game mix/lifecycle/listening is still #55, not source acceptance.

Masters are the exact approved48kHz mono24-bit derivatives, with original
recording provenance/credits retained. Approval.json identifies master/import
lengths and hashes; unreal-handoff.json binds the approval manifest SHA256.
The independently recorded approval hash is on issues54/55. Original downloaded
PCM provenance is in OriginalSources.json, source/license links in CREDITS.txt.
Full raw recordings and authoring recipes remain in the retained source-review
outputs; original WAVs have not been silently substituted with MP3 previews.

Runtime copies are mono48kHz16-bit with one static attenuation from approved
masters to the old source loudness targets: car idle~ -34LUFS, car A/B~ -31,
truck idle~ -33 and truck running~ -30. Each exact gain is in the handoff and
staging manifest. PCM quantization occurs once after gain; no compression,
pitch change, invented engine layer, Doppler, distance envelope or reverb baked.
SourceWave volume/pitch remain1; existing runtime mixing/preferences apply once.
Loudness alone does not establish equal perceived balance; human review remains.

Existing native paths are reimported for the four base roles. Alternate car B
adds one wave and one duplicated Doppler/looping-wave cue. Even positive car IDs
use A, odd IDs B; selection remains stable across speed changes. Both return to
the shared approved idle when stopped. Freight/garbage trucks retain their own
approved idle/driving sources. Pool cap, transition spare, spatial/distance and
smoothed engine Doppler remain unchanged. Current traffic Sound Cue routing is
preserved for focused integration; no broad MetaSound redesign is implied.

The abuurman stationary truck take does not establish loaded-driving RPM.
Car source has natural variation; no perceived seamlessness/device claim follows
from numerical checks. Rejected road-noise source and short item11 comparison
are excluded from this selected bank. Attribution for all recorded sources must
travel with integration/distribution; licenses differ from older synthesized bank.
