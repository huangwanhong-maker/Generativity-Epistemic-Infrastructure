# Vendored dependencies

These upstream sources are vendored for offline builds. Their original notices
remain in the source files and accompanying licences. They are not project-authored
code. Configuration and builds do not download or update them.

| Dependency | Pin | Licence | Upstream |
|---|---|---|---|
| JSON for Modern C++ | 3.12.0 | MIT | https://github.com/nlohmann/json/releases/tag/v3.12.0 |
| SQLite amalgamation | 3.51.3 | Public domain | https://www.sqlite.org/releaselog/3_51_3.html |
| PicoSHA2 | `161cb3fc4170fa7a3eca9e582cebd27cc4d1fe29` | MIT | https://github.com/okdshin/PicoSHA2 |

`MANIFEST.json` records exact download URLs, byte sizes and SHA-256 digests.
The JSON header matched its upstream release SHA-256. `sqlite3.c` matched the
upstream release's published SHA3-256. PicoSHA2 was retrieved at an immutable
commit; its SHA-256 is a local custody checksum, not an upstream signed release
attestation. The SQLite archive is an ignored build download; only its three
library source/header files are vendored.

SQLite's source header states its public-domain dedication. See the upstream
[copyright statement](https://www.sqlite.org/copyright.html). This build omits
extension loading and uses SQLite's serialized threading mode. Those choices
do not make an arbitrary database file safe to open as an untrusted input.

To verify file custody without network access, from GR-OS:

```powershell
$grManifest = Get-Content -Raw third_party/MANIFEST.json | ConvertFrom-Json
foreach ($grArtifact in $grManifest.artifacts) {
    $grPath = Join-Path 'third_party' $grArtifact.path
    if ((Get-FileHash -Algorithm SHA256 -LiteralPath $grPath).Hash.ToLowerInvariant() -ne $grArtifact.sha256) {
        throw "Dependency hash mismatch: $grPath"
    }
}
```
