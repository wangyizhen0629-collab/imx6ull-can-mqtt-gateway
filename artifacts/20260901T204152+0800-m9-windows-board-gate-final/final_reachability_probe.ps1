$ErrorActionPreference = 'Stop'
$RunDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$PrivateDir = Join-Path $RunDir 'private_raw'
$EndpointFile = Join-Path $PrivateDir 'board_endpoint.txt'
$UserFile = Join-Path $PrivateDir 'board_user.txt'
$RawPath = Join-Path $PrivateDir 'final_reachability_probe.txt'
$PublicPath = Join-Path $RunDir 'final_reachability_probe.redacted.txt'
$TracePath = Join-Path $RunDir 'final_reachability_probe.redaction-trace.txt'
foreach ($Path in @($RawPath, $PublicPath, $TracePath)) {
    if (Test-Path -LiteralPath $Path) { throw "refusing to overwrite existing evidence: $Path" }
}
$BoardHost = ((Get-Content -LiteralPath $EndpointFile -Raw) -split '=', 2)[1].Trim()
$BoardUser = ((Get-Content -LiteralPath $UserFile -Raw) -split '=', 2)[1].Trim()
$Started = Get-Date -Format o
$PreviousErrorAction = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
$Output = & ssh.exe -o BatchMode=yes -o ConnectTimeout=10 "$BoardUser@$BoardHost" 'cat /proc/sys/kernel/random/boot_id; cat /proc/uptime; date -Ins; cat /proc/1/comm' 2>&1 | Out-String
$ExitCode = $LASTEXITCODE
$ErrorActionPreference = $PreviousErrorAction
$Ended = Get-Date -Format o
$Raw = @(
    "started_at=$Started"
    'mode=single final read-only reachability probe; NO reboot/network/rollback command'
    'command=ssh <REDACTED_USER>@<PRIVATE> cat boot_id,uptime,date,pid1_comm'
    "exit_code=$ExitCode"
    "ended_at=$Ended"
    '--- full output ---'
    $Output.TrimEnd()
) -join "`r`n"
[IO.File]::WriteAllText($RawPath, $Raw + "`r`n", [Text.UTF8Encoding]::new($false))
$Redacted = $Raw.Replace($BoardHost, '<REDACTED_BOARD_HOST>')
$Redacted = [regex]::Replace($Redacted, '(?<![0-9])(?:[0-9]{1,3}\.){1,3}[0-9]{1,3}(?![0-9])', '<REDACTED_DOTTED_NUMERIC>')
[IO.File]::WriteAllText($PublicPath, $Redacted + "`r`n", [Text.UTF8Encoding]::new($false))
$ExactLeak = ([regex]::Matches($Redacted, [regex]::Escape($BoardHost))).Count
$DottedLeak = ([regex]::Matches($Redacted, '(?<![0-9])(?:[0-9]{1,3}\.){1,3}[0-9]{1,3}(?![0-9])')).Count
$Trace = @(
    "raw_sha256=$((Get-FileHash -LiteralPath $RawPath -Algorithm SHA256).Hash.ToLowerInvariant())"
    "redacted_sha256=$((Get-FileHash -LiteralPath $PublicPath -Algorithm SHA256).Hash.ToLowerInvariant())"
    'redaction_method=exact endpoint;all 2-to-4-component dotted numeric sequences'
    "exact_endpoint_leak_count=$ExactLeak"
    "dotted_numeric_leak_count=$DottedLeak"
) -join "`r`n"
[IO.File]::WriteAllText($TracePath, $Trace + "`r`n", [Text.UTF8Encoding]::new($false))
"FINAL_REACHABILITY_EXIT=$ExitCode"
"EXACT_LEAK_COUNT=$ExactLeak"
"DOTTED_LEAK_COUNT=$DottedLeak"
exit $ExitCode
