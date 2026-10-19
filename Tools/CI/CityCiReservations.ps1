# Shared CI owners are exempt from the local-owner guard. Other owners remain
# blocking, including malformed or merely similar prefixes.
function Get-CityLocalNativeClaims([string]$Plan) {
    @([regex]::Matches($Plan, '(?m)^\*\*(?<owner>[^*\r\n]*\bwindow CLAIMED[^*\r\n]*)\*\*') |
        Where-Object { $_.Groups['owner'].Value -notmatch '^Drone CI\b' })
}
