$ErrorActionPreference = 'Stop'
$RunDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$PrivateDir = Join-Path $RunDir 'private_raw'
$RawPath = Join-Path $PrivateDir 'reboot_recovery_poll.txt'
$EndpointFile = Join-Path $PrivateDir 'board_endpoint.txt'
$PublicPath = Join-Path $RunDir 'reboot_recovery_poll.v2.redacted.txt'
$TracePath = Join-Path $RunDir 'reboot_recovery_poll.v2.redaction-trace.txt'
foreach ($Path in @($PublicPath, $TracePath)) {
    if (Test-Path -LiteralPath $Path) { throw "refusing to overwrite existing evidence: $Path" }
}
$BoardHost = ((Get-Content -LiteralPath $EndpointFile -Raw) -split '=', 2)[1].Trim()
$Raw = Get-Content -LiteralPath $RawPath -Raw
$Redacted = $Raw.Replace($BoardHost, '<REDACTED_BOARD_HOST>')
$Redacted = [regex]::Replace($Redacted, '(?<![0-9])(?:[0-9]{1,3}\.){1,3}[0-9]{1,3}(?![0-9])', '<REDACTED_DOTTED_NUMERIC>')
[IO.File]::WriteAllText($PublicPath, $Redacted, [Text.UTF8Encoding]::new($false))
$ExactLeak = ([regex]::Matches($Redacted, [regex]::Escape($BoardHost))).Count
$DottedLeak = ([regex]::Matches($Redacted, '(?<![0-9])(?:[0-9]{1,3}\.){1,3}[0-9]{1,3}(?![0-9])')).Count
$Trace = @(
    "raw_sha256=$((Get-FileHash -LiteralPath $RawPath -Algorithm SHA256).Hash.ToLowerInvariant())"
    "redacted_sha256=$((Get-FileHash -LiteralPath $PublicPath -Algorithm SHA256).Hash.ToLowerInvariant())"
    'redaction_method=exact endpoint;all 2-to-4-component dotted numeric sequences'
    "exact_endpoint_leak_count=$ExactLeak"
    "dotted_numeric_leak_count=$DottedLeak"
    "status=$(if ($ExactLeak -eq 0 -and $DottedLeak -eq 0) { 'PASS' } else { 'FAIL' })"
) -join "`r`n"
[IO.File]::WriteAllText($TracePath, $Trace + "`r`n", [Text.UTF8Encoding]::new($false))
Get-Content -LiteralPath $TracePath
if ($ExactLeak -ne 0 -or $DottedLeak -ne 0) { exit 1 }
