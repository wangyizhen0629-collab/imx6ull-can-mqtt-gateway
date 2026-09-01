$ErrorActionPreference = 'Stop'
$RunDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = (Resolve-Path (Join-Path $RunDir '..\..')).Path
$Manifest = Join-Path $RunDir 'artifact_manifest.sha256'
$ManifestCheck = Join-Path $RunDir 'artifact_manifest_check.txt'
$ManifestAttempt = Join-Path $RunDir 'artifact_manifest_attempt1.sha256'
$ManifestCheckAttempt = Join-Path $RunDir 'artifact_manifest_check_attempt1.txt'
$CachedCheckAttempt = Join-Path $RunDir 'git_diff_cached_check_attempt1.txt'
$Record = Join-Path $RunDir 'public_whitespace_normalization.txt'
foreach ($Path in @($ManifestAttempt, $ManifestCheckAttempt, $CachedCheckAttempt, $Record)) {
    if (Test-Path -LiteralPath $Path) { throw "refusing to overwrite existing evidence: $Path" }
}

Copy-Item -LiteralPath $Manifest -Destination $ManifestAttempt
Copy-Item -LiteralPath $ManifestCheck -Destination $ManifestCheckAttempt
$OldManifestSha = (Get-FileHash -LiteralPath $ManifestAttempt -Algorithm SHA256).Hash.ToLowerInvariant()
$OldCheckSha = (Get-FileHash -LiteralPath $ManifestCheckAttempt -Algorithm SHA256).Hash.ToLowerInvariant()

$PreviousErrorAction = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
$CachedOutput = & git -C $RepoRoot diff --cached --check 2>&1 | Out-String
$CachedExit = $LASTEXITCODE
$ErrorActionPreference = $PreviousErrorAction
@(
    'command=git diff --cached --check'
    "exit_code=$CachedExit"
    '--- full output ---'
    $CachedOutput.TrimEnd()
) | Set-Content -LiteralPath $CachedCheckAttempt -Encoding utf8

function Normalize-PublicText([string]$Path) {
    $Text = [IO.File]::ReadAllText($Path)
    $Lines = [regex]::Split($Text, '\r?\n')
    $NormalizedLines = New-Object Collections.Generic.List[string]
    foreach ($Line in $Lines) { $NormalizedLines.Add($Line.TrimEnd(' ', "`t")) }
    while ($NormalizedLines.Count -gt 0 -and $NormalizedLines[$NormalizedLines.Count - 1] -eq '') {
        $NormalizedLines.RemoveAt($NormalizedLines.Count - 1)
    }
    $Normalized = ($NormalizedLines -join "`n") + "`n"
    [IO.File]::WriteAllText($Path, $Normalized, [Text.UTF8Encoding]::new($false))
}

$Excluded = @(
    'artifact_manifest.sha256',
    'artifact_manifest_check.txt',
    'artifact_manifest_attempt1.sha256',
    'artifact_manifest_check_attempt1.txt'
)
foreach ($File in Get-ChildItem -LiteralPath $RunDir -File) {
    if ($File.Name -notin $Excluded) { Normalize-PublicText $File.FullName }
}

$TraceMap = @(
    @{ Trace='board_readonly_preflight.redaction-trace.txt'; Public='board_readonly_preflight.redacted.txt' },
    @{ Trace='board_busybox_identification.redaction-trace.txt'; Public='board_busybox_identification.redacted.txt' },
    @{ Trace='board_pid1_runtime_identification.redaction-trace.txt'; Public='board_pid1_runtime_identification.redacted.txt' },
    @{ Trace='board_final_readonly_audit.redaction-trace.txt'; Public='board_final_readonly_audit.redacted.txt' }
)
foreach ($Map in $TraceMap) {
    $TracePath = Join-Path $RunDir $Map.Trace
    $PublicPath = Join-Path $RunDir $Map.Public
    $PublicSha = (Get-FileHash -LiteralPath $PublicPath -Algorithm SHA256).Hash.ToLowerInvariant()
    $Lines = @(Get-Content -LiteralPath $TracePath | Where-Object { $_ -notmatch '^redacted_sha256=' -and $_ -notmatch '^public_normalization=' })
    $Lines += "redacted_sha256=$PublicSha"
    $Lines += 'public_normalization=trailing spaces removed and one LF retained at EOF; private_raw is byte-for-byte unchanged'
    [IO.File]::WriteAllText($TracePath, (($Lines -join "`n") + "`n"), [Text.UTF8Encoding]::new($false))
}

$PrivateBefore = @(Get-Content -LiteralPath (Join-Path $RunDir 'raw_private_sha256.txt'))
$PrivateFiles = @(Get-ChildItem -LiteralPath (Join-Path $RunDir 'private_raw') -Recurse -File | Sort-Object FullName)
$PrivateAfter = foreach ($File in $PrivateFiles) {
    $Relative = $File.FullName.Substring((Join-Path $RunDir 'private_raw').Length + 1).Replace('\', '/')
    $Sha = (Get-FileHash -LiteralPath $File.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    "$Sha  private_raw/$Relative  bytes=$($File.Length)"
}
$PrivateUnchanged = (($PrivateBefore -join "`n") -eq ($PrivateAfter -join "`n"))
@(
    "normalized_at=$((Get-Date).ToString('o'))"
    "initial_cached_diff_check_exit=$CachedExit"
    "preserved_attempt1_manifest_sha256=$OldManifestSha"
    "preserved_attempt1_manifest_check_sha256=$OldCheckSha"
    "private_raw_unchanged=$($PrivateUnchanged.ToString().ToLowerInvariant())"
    'scope=public artifact files only; trailing spaces removed and EOF normalized to one LF'
    'semantic_content=unchanged'
    'reason=generated ps/ip output contained trailing spaces that made the full staged git diff --check fail'
) | Set-Content -LiteralPath $Record -Encoding utf8
Normalize-PublicText $Record

$ManifestFiles = @(Get-ChildItem -LiteralPath $RunDir -File | Where-Object {
    $_.Name -notin @('artifact_manifest.sha256', 'artifact_manifest_check.txt')
} | Sort-Object Name)
$ManifestLines = foreach ($File in $ManifestFiles) {
    $Sha = (Get-FileHash -LiteralPath $File.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    "$Sha  $($File.Name)"
}
[IO.File]::WriteAllText($Manifest, (($ManifestLines -join "`n") + "`n"), [Text.ASCIIEncoding]::new())
$CheckLines = New-Object Collections.Generic.List[string]
$MismatchCount = 0
foreach ($Line in Get-Content -LiteralPath $Manifest) {
    if ($Line -notmatch '^([0-9a-f]{64})  (.+)$') { $MismatchCount++; $CheckLines.Add("MALFORMED $Line"); continue }
    $Expected = $matches[1]
    $Name = $matches[2]
    $Path = Join-Path $RunDir $Name
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { $MismatchCount++; $CheckLines.Add("MISSING $Name"); continue }
    $Actual = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($Actual -eq $Expected) { $CheckLines.Add("OK $Name") } else { $MismatchCount++; $CheckLines.Add("MISMATCH $Name") }
}
$CheckLines.Add("checked=$($ManifestLines.Count)")
$CheckLines.Add("mismatches=$MismatchCount")
$CheckLines.Add("status=$(if ($MismatchCount -eq 0) { 'PASS' } else { 'FAIL' })")
[IO.File]::WriteAllText($ManifestCheck, (($CheckLines -join "`n") + "`n"), [Text.UTF8Encoding]::new($false))

"INITIAL_CACHED_DIFF_CHECK_EXIT=$CachedExit"
"PRIVATE_RAW_UNCHANGED=$PrivateUnchanged"
"FINAL_MANIFEST_MISMATCHES=$MismatchCount"
if (-not $PrivateUnchanged -or $MismatchCount -ne 0) { exit 1 }
