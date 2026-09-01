$ErrorActionPreference = 'Stop'
$RunDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = (Resolve-Path (Join-Path $RunDir '..\..')).Path
$Source = Join-Path $RepoRoot 'tmp/evidence.txt'
$PrivateDir = Join-Path $RunDir 'private_raw'
$RawPath = Join-Path $PrivateDir 'operator_terminal_evidence.txt'
$PublicPath = Join-Path $RunDir 'operator_terminal_evidence.redacted.txt'
$TracePath = Join-Path $RunDir 'operator_terminal_evidence.redaction-trace.txt'
$SourceInfoPath = Join-Path $RunDir 'source_info.txt'
$HostInfoPath = Join-Path $RunDir 'host_info.txt'

foreach ($Path in @($PrivateDir, $RawPath, $PublicPath, $TracePath, $SourceInfoPath, $HostInfoPath)) {
    if (Test-Path -LiteralPath $Path) { throw "refusing to overwrite evidence: $Path" }
}
if (-not (Test-Path -LiteralPath $Source -PathType Leaf)) { throw "missing source evidence: $Source" }
[void](New-Item -ItemType Directory -Path $PrivateDir)

$RawBytes = [IO.File]::ReadAllBytes($Source)
[IO.File]::WriteAllBytes($RawPath, $RawBytes)
$RawSha = (Get-FileHash -LiteralPath $RawPath -Algorithm SHA256).Hash.ToLowerInvariant()
$Text = [Text.UTF8Encoding]::new($false, $true).GetString($RawBytes)
$LineCount = @($Text -split '\r?\n').Count

$Redacted = [regex]::Replace($Text, '\[[^\]\r\n]*@[^:\]\r\n]+:[^\]\r\n]*\]', '[<REDACTED_LOGIN>@<REDACTED_TARGET>:<REDACTED_PATH>]')
$Redacted = [regex]::Replace($Redacted, '(?<![0-9])(?:10\.(?:[0-9]{1,3}\.){2}[0-9]{1,3}|192\.168\.(?:[0-9]{1,3}\.)[0-9]{1,3}|172\.(?:1[6-9]|2[0-9]|3[01])\.(?:[0-9]{1,3}\.)[0-9]{1,3})(?![0-9])', '<REDACTED_IPV4>')
$Redacted = [regex]::Replace($Redacted, '(?im)^(\s*(?:broker_(?:host|username|password)|password|passwd|token|secret)\s*=).+$', '$1<REDACTED>')
$Lines = @($Redacted -split '\r?\n')
while ($Lines.Count -gt 0 -and $Lines[-1] -eq '') {
    if ($Lines.Count -eq 1) { $Lines = @() } else { $Lines = $Lines[0..($Lines.Count - 2)] }
}
$Clean = (($Lines | ForEach-Object { $_ -replace '[ \t]+$', '' }) -join "`n") + "`n"
[IO.File]::WriteAllText($PublicPath, $Clean, [Text.UTF8Encoding]::new($false))
$PublicSha = (Get-FileHash -LiteralPath $PublicPath -Algorithm SHA256).Hash.ToLowerInvariant()

$Trace = @(
    "raw_sha256=$RawSha"
    "redacted_sha256=$PublicSha"
    "raw_bytes=$($RawBytes.Length)"
    "raw_line_count=$LineCount"
    'method=redact login prompt/target/private IPv4/credential assignments; strip public trailing whitespace only'
    'raw_location=private_raw/operator_terminal_evidence.txt (Git ignored)'
)
[IO.File]::WriteAllLines($TracePath, $Trace, [Text.UTF8Encoding]::new($false))

$SourceInfo = @(
    'source=tmp/evidence.txt'
    "source_sha256=$RawSha"
    "source_bytes=$($RawBytes.Length)"
    "source_line_count=$LineCount"
    "captured_at=$((Get-Date).ToString('o'))"
    'provenance=operator terminal copy plus explicitly marked conversation-paste addendum requested by operator'
)
[IO.File]::WriteAllLines($SourceInfoPath, $SourceInfo, [Text.UTF8Encoding]::new($false))

$HostInfo = @(
    "captured_at=$((Get-Date).ToString('o'))"
    "timezone=$([TimeZoneInfo]::Local.Id)"
    "head=$((& git -C $RepoRoot rev-parse HEAD).Trim())"
    "branch=$((& git -C $RepoRoot branch --show-current).Trim())"
    "tracked_change_count=$(@(& git -C $RepoRoot status --porcelain=v1 --untracked-files=no).Count)"
    "untracked_count=$(@(& git -C $RepoRoot ls-files --others --exclude-standard).Count)"
    'board_operations=manual by operator; Codex performed no board command in this run'
)
[IO.File]::WriteAllLines($HostInfoPath, $HostInfo, [Text.UTF8Encoding]::new($false))

"raw_sha256=$RawSha"
"redacted_sha256=$PublicSha"
"raw_bytes=$($RawBytes.Length)"
"raw_line_count=$LineCount"
