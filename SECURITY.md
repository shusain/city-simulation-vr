# Security and local configuration

Report suspected credential exposure privately to the repository owner through
the existing private Gitea contact channel. Include the affected path/revision
and credential type, never the value. Do not open a public issue containing it.

## Android File Server

Tracked configuration disables Android File Server and network access. Shipping
inclusion and external shipping start are also disabled. AFS is optional; normal
ADB deployment does not require a shared source-controlled token.

For an explicitly needed local AFS session, put its settings and a fresh, unique
token in the ignored
`CitySimVR/Config/UserEngine.ini`, not `DefaultEngine.ini` or the tracked
`Config/Android/AndroidEngine.ini` renderer settings. Unreal supports this
[project user override](https://dev.epicgames.com/documentation/en-us/unreal-engine/configuration-files-in-unreal-engine).
Use USB-only access unless network access is specifically needed, and keep shipping inclusion
and shipping external start disabled. Never put the token in command-line
arguments, build logs, receipts, screenshots or issue comments. Review the
staged diff before committing; Unreal's settings UI can write project defaults.
Remove/disable your local override when the session ends. CI builds from clean
clones and does not copy this local override.

The previously committed AFS token must be treated as exposed. New builds from
this change disable the service and contain no tracked token. This does not
change existing APKs, installed devices or saved per-user settings: retire old
AFS-enabled packages, disable their service or rebuild/reinstall them with AFS
disabled (or a new local token). Device deployment requires its existing explicit
authorization. No remote credential revocation service exists for this token.

The canonical Gitea Git history still contains the old value; this clean-source
snapshot starts with a separate root commit and excludes that earlier history. Do not rely on deleting the latest
line as revocation, and do not rewrite shared history without a coordinated
decision. This snapshot was prepared from reviewed current source rather than by
rewriting Gitea history. See SOURCE_SNAPSHOT.md for its provenance. Never add
the original Gitea history to a public snapshot assuming it was scrubbed.

## Other local credentials

Keep Drone RPC secrets in the existing ignored, ACL-restricted runner config;
Git/Gitea credentials in the user's credential manager; and Android signing
identities outside source control. Existing third-party plugin restores and
player profiles remain ignored. `.env` files, local platform overrides, virtual
environments and common private-key/keystore formats are also ignored as a
second line of defense. Ignore rules do not remove already tracked history.

Use a current secret scanner with redaction against the staged/current files
and reachable history before publication. Automated scanning can miss
application-specific tokens, so also inspect configuration and build recipes.
Do not upload unredacted scanner reports to public services.
