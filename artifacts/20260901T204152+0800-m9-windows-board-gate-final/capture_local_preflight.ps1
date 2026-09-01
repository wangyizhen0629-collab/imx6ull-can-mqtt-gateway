$ErrorActionPreference = 'Stop'
$RunDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$PrivateDir = Join-Path $RunDir 'private_raw'
$RawPath = Join-Path $PrivateDir 'local_preflight.txt'
$PublicPath = Join-Path $RunDir 'local_preflight.redacted.txt'
$TracePath = Join-Path $RunDir 'local_preflight.redaction-trace.txt'
foreach ($Path in @($RawPath, $PublicPath, $TracePath)) {
    if (Test-Path -LiteralPath $Path) { throw "refusing to overwrite existing evidence: $Path" }
}

$Started = Get-Date -Format o
$PreviousErrorAction = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
$StatusOutput = & git status --short --branch 2>&1 | Out-String
$StatusExit = $LASTEXITCODE
$PullOutput = & git pull --ff-only origin master 2>&1 | Out-String
$PullExit = $LASTEXITCODE
$HeadOutput = & git rev-parse HEAD 2>&1 | Out-String
$HeadExit = $LASTEXITCODE
& git merge-base --is-ancestor 17d698e HEAD
$AncestorExit = $LASTEXITCODE
$Tracked = @(& git status --porcelain=v1 --untracked-files=no)
$TrackedExit = $LASTEXITCODE
$Untracked = @(& git ls-files --others --exclude-standard)
$UntrackedExit = $LASTEXITCODE
$ErrorActionPreference = $PreviousErrorAction
$Ended = Get-Date -Format o

$Raw = @(
    "started_at=$Started"
    'command=git status --short --branch'
    "exit_code=$StatusExit"
    '--- full output ---'
    $StatusOutput.TrimEnd()
    '--- end full output ---'
    'command=git pull --ff-only origin master'
    "exit_code=$PullExit"
    '--- full output ---'
    $PullOutput.TrimEnd()
    '--- end full output ---'
    'command=git rev-parse HEAD'
    "exit_code=$HeadExit"
    '--- full output ---'
    $HeadOutput.TrimEnd()
    '--- end full output ---'
    'command=git merge-base --is-ancestor 17d698e HEAD'
    "exit_code=$AncestorExit"
    "tracked_status_count=$($Tracked.Count)"
    "tracked_status_exit=$TrackedExit"
    "untracked_count=$($Untracked.Count)"
    "untracked_list_exit=$UntrackedExit"
    "ended_at=$Ended"
) -join "`r`n"
[IO.File]::WriteAllText($RawPath, $Raw + "`r`n", [Text.UTF8Encoding]::new($false))

$Redacted = [regex]::Replace($Raw, '(?<![0-9])(?:[0-9]{1,3}\.){3}[0-9]{1,3}(?![0-9])', '<REDACTED_IPV4>')
$Redacted = [regex]::Replace($Redacted, '(?im)(?:password|passwd|token|secret)=\S+', '$0<REDACTED>')
[IO.File]::WriteAllText($PublicPath, $Redacted + "`r`n", [Text.UTF8Encoding]::new($false))
$Trace = @(
    "raw_sha256=$((Get-FileHash -LiteralPath $RawPath -Algorithm SHA256).Hash.ToLowerInvariant())"
    "redacted_sha256=$((Get-FileHash -LiteralPath $PublicPath -Algorithm SHA256).Hash.ToLowerInvariant())"
    'redaction_method=all IPv4 literals and credential-like assignments replaced'
) -join "`r`n"
[IO.File]::WriteAllText($TracePath, $Trace + "`r`n", [Text.UTF8Encoding]::new($false))

"STATUS_EXIT=$StatusExit"
"PULL_EXIT=$PullExit"
"HEAD_EXIT=$HeadExit"
"HEAD=$($HeadOutput.Trim())"
"ANCESTOR_17D698E_EXIT=$AncestorExit"
"TRACKED_STATUS_COUNT=$($Tracked.Count)"
"UNTRACKED_COUNT=$($Untracked.Count)"
if ($StatusExit -ne 0 -or $PullExit -ne 0 -or $HeadExit -ne 0 -or
    $AncestorExit -ne 0 -or $TrackedExit -ne 0 -or $UntrackedExit -ne 0 -or
    $Tracked.Count -ne 0) { exit 1 }
