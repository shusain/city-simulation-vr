# Local audio review and feedback

2026-10-13. The reusable player supports music tracks, loops and short sound samples. It uses one audio element, per-item decisions/notes, optional section jumps, playback timestamps and overall collection notes. Drafts save in the current browser; submission persists immutable JSON receipts on disk. Approve, Reject and Request changes are explicit reviewer choices; untouched items stay Not reviewed. Changes require a note. Partial reviews are allowed.

The final D82 review used http://127.0.0.1:8766/Review.html; all nine selected recordings are now approved. Earlier reviews at8763/8764 remain historical. For a new collection, select an item, listen, choose a decision and enter notes; use **Add playback time** for a section reference. **Submit feedback** saves the entire current review, including unreviewed items. Updates create new submissions and retain history. **Export feedback** downloads JSON and works with an extracted offline review too.

Submission saves feedback for Codex to read on the next chat turn; it does not automatically send a chat message or wake an agent. Tell the chat that feedback is submitted when ready. No cloud service, account, key or paid API is involved. The server binds only to127.0.0.1, serves its named review directory, and accepts bounded JSON submissions from its own local origin. Actual sample files and game assets are never changed by a decision.

## Reuse with any audio collection

Use a manifest with `title`, optional `task`, and `assets` (or the existing soundtrack `tracks`). Each item requires unique `id`, `title` and a local `audio` path (`mp3` also supported) within the review folder. `seconds`, `duration`, `description`/`intent`, `bpm` and `sections` are optional. A supplied `sha256`/`mp3Sha256` is checked against the actual file. Example:

```json
{
  "title": "Vehicle engine samples",
  "task": "D75",
  "assets": [
    {"id": "engine-a", "title": "Engine A", "audio": "Samples/Engine-A.wav", "seconds": 12,
     "description": "Steady idle loop", "sections": [{"name": "Loop seam", "startSeconds": 10}]},
    {"id": "engine-b", "title": "Engine B", "audio": "Samples/Engine-B.mp3", "seconds": 8}
  ]
}
```

Put the manifest and audio under a dedicated artifact directory. Build a self-contained HTML page and its catalog with the verified existing Python; then serve that directory. Use a free port for a separate review. Example from the repository root:

```powershell
$reviewPython = 'C:\Users\shaun\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
& $reviewPython Tools/AudioReview.py --manifest Artifacts/MyAudio/Manifest.json --output Artifacts/MyAudio
& $reviewPython Tools/Serve-CitySoundtrackReview.py --root Artifacts/MyAudio --port 8766
```

`Tools/AudioReview.html` owns the shared UI. `Tools/AudioReview.py` adapts manifests, verifies audio identity and writes `Review.html`/`ReviewCatalog.json`; `Build-CitySoundtrackReview.py` calls the same generator. Include the HTML, catalog and referenced audio files in a portable set. Optional `reviewDownloads` is a list of `{label,path,serverOnly}` objects for local download links. A review without BPM or section data works normally. Every decision is tied to the item ID plus actual SHA256, so changing audio creates a new review identity rather than silently carrying approvals to a revision.

## Read submitted feedback

The default receipt directory is `ROOT/Feedback/<reviewId>/<submissionId>.json`. Each record contains collection identity, server UTC receipt time, overall notes and all canonical item IDs/titles/audio hashes/decisions/notes. The local endpoint `/api/feedback/latest` returns the latest submission for the current catalog. Read that receipt and any newer browser-submitted update before revising a track; don't infer approval from a draft badge or an earlier version. The next chat can use this queue entry/path without relying on conversational memory.

Browser drafts survive reloads and switching items. A fresh browser can restore the latest server submission when no local draft is present. Retrying an unchanged submission uses its original ID and receipt; changing it creates a new ID. Failed submissions retain the browser draft and offer export. Exported/offline JSON can be supplied to chat, but is not a server submission until deliberately imported/reviewed. Browser storage availability varies; the UI reports when saving drafts fails.

Focused regression: `Tools/Test-AudioReview.py` tests partial and immutable/idempotent capture, invalid/stale/missing-note rejection, local-origin checks and audio range playback. Its D82 test collection and backend/UI receipts are in `Artifacts/D82/FeedbackReviewQA`; these are QA records, never user soundtrack decisions. `--feedback-dir` can isolate receipt output during future tests. No Editor/game build or device test is required for this authoring tool.
