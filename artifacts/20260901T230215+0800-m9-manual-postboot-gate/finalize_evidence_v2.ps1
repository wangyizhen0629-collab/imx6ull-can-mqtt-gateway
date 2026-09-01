$ErrorActionPreference = 'Stop'
$RunDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = (Resolve-Path (Join-Path $RunDir '..\..')).Path
$RunRel = 'artifacts/20260901T230215+0800-m9-manual-postboot-gate'
$RawDir = Join-Path $RunDir 'private_raw'
$RawPath = Join-Path $RawDir 'operator_terminal_evidence.txt'
$PublicEvidence = Join-Path $RunDir 'operator_terminal_evidence.redacted.txt'
$Utf8 = [Text.UTF8Encoding]::new($false)
$Failed = $false

$Outputs = @(
    'powershell_syntax_check_v2.txt'
    'json_syntax_check_v2.txt'
    'public_whitespace_check_v2.txt'
    'evidence_marker_check_v2.txt'
    'docs_consistency_v2.txt'
    'git_diff_check_v2.txt'
    'repository_state_v2.txt'
    'raw_private_sha256_v2.txt'
    'sensitive_scan_v2.txt'
    'sensitive_scan_v2.exit.txt'
    'finalize_v2_pre_manifest.txt'
    'artifact_manifest.sha256'
    'artifact_manifest_check.txt'
)
foreach ($Name in $Outputs) {
    $Path = Join-Path $RunDir $Name
    if (Test-Path -LiteralPath $Path) { throw "refusing to overwrite evidence: $Path" }
}
if (-not (Test-Path -LiteralPath $RawPath -PathType Leaf)) { throw 'missing private raw evidence' }
if (-not (Test-Path -LiteralPath $PublicEvidence -PathType Leaf)) { throw 'missing redacted evidence' }

function Write-Lines([string]$Path, [object[]]$Lines) {
    [IO.File]::WriteAllLines($Path, [string[]]$Lines, $Utf8)
}

$PsLines = @('shell_files=NONE', 'powershell_parse_results:')
$PsErrors = 0
Get-ChildItem -LiteralPath $RunDir -Filter '*.ps1' -File | Sort-Object Name | ForEach-Object {
    $Tokens = $null
    $Errors = $null
    [void][Management.Automation.Language.Parser]::ParseFile($_.FullName, [ref]$Tokens, [ref]$Errors)
    $Result = if ($Errors.Count -eq 0) { 'PASS' } else { 'FAIL' }
    $PsLines += "$($_.Name)=$Result error_count=$($Errors.Count)"
    $PsErrors += $Errors.Count
}
$PsLines += "result=$(if ($PsErrors -eq 0) { 'PASS' } else { 'FAIL' })"
Write-Lines (Join-Path $RunDir 'powershell_syntax_check_v2.txt') $PsLines
if ($PsErrors -ne 0) { $Failed = $true }

$JsonLines = @()
$JsonErrors = 0
Get-ChildItem -LiteralPath $RunDir -Filter '*.json' -File | Sort-Object Name | ForEach-Object {
    try {
        [void](Get-Content -LiteralPath $_.FullName -Raw | ConvertFrom-Json)
        $JsonLines += "$($_.Name)=PASS"
    } catch {
        $JsonLines += "$($_.Name)=FAIL"
        $JsonErrors++
    }
}
$JsonLines += "result=$(if ($JsonErrors -eq 0) { 'PASS' } else { 'FAIL' })"
Write-Lines (Join-Path $RunDir 'json_syntax_check_v2.txt') $JsonLines
if ($JsonErrors -ne 0) { $Failed = $true }

$WhitespaceFiles = @(
    (Get-ChildItem -LiteralPath $RunDir -File | ForEach-Object { $_.FullName })
    (Join-Path $RepoRoot 'README.md')
    (Join-Path $RepoRoot 'docs/PROJECT_SPEC.md')
    (Join-Path $RepoRoot 'docs/TEST_PLAN.md')
    (Join-Path $RepoRoot 'docs/OPEN_QUESTIONS.md')
    (Join-Path $RepoRoot 'docs/RESUME_TRACEABILITY.md')
    (Join-Path $RepoRoot 'docs/DECISION_LOG.md')
    (Join-Path $RepoRoot 'docs/PLANS.md')
    (Join-Path $RepoRoot 'docs/milestones/M9.md')
) | Sort-Object -Unique
$WhitespaceCount = 0
foreach ($File in $WhitespaceFiles) {
    foreach ($Line in ([IO.File]::ReadAllText($File) -split "`r?`n")) {
        if ($Line -match '[ \x09]+$') { $WhitespaceCount++ }
    }
}
$WhitespaceLines = @(
    "scanned_file_count=$($WhitespaceFiles.Count)"
    "trailing_whitespace_line_count=$WhitespaceCount"
    "result=$(if ($WhitespaceCount -eq 0) { 'PASS' } else { 'FAIL' })"
)
Write-Lines (Join-Path $RunDir 'public_whitespace_check_v2.txt') $WhitespaceLines
if ($WhitespaceCount -ne 0) { $Failed = $true }

$EvidenceText = [IO.File]::ReadAllText($PublicEvidence)
$Markers = @(
    '0abefcf0-9d85-4a4b-b335-f339b33b8db4'
    'status=running supervisor_pid=337 child_pid=9951'
    'start_exit=0'
    'status_5s_exit=0'
    'final_status_exit=0'
    'stable_child_pid=PASS'
    '6e8729417b3dc40c10a413459de5eca9be43ce58dfcc8a3b12e91f5c8d7ef958  /proc/9951/exe'
    'counts supervisor=1 child=1 other_gatewayd=0 test_process=0'
    'restore_exit=0'
    'bitrate 500000'
)
$MarkerLines = @()
$MarkerFailures = 0
foreach ($Marker in $Markers) {
    $Present = $EvidenceText.Contains($Marker)
    $Hasher = [Security.Cryptography.SHA256]::Create()
    try {
        $MarkerHash = ($Hasher.ComputeHash($Utf8.GetBytes($Marker)) | ForEach-Object { $_.ToString('x2') }) -join ''
    } finally {
        $Hasher.Dispose()
    }
    $MarkerLines += "marker_sha256=$MarkerHash result=$(if ($Present) { 'PASS' } else { 'FAIL' })"
    if (-not $Present) { $MarkerFailures++ }
}
$MarkerLines += "required_marker_count=$($Markers.Count)"
$MarkerLines += "missing_marker_count=$MarkerFailures"
$MarkerLines += "result=$(if ($MarkerFailures -eq 0) { 'PASS' } else { 'FAIL' })"
Write-Lines (Join-Path $RunDir 'evidence_marker_check_v2.txt') $MarkerLines
if ($MarkerFailures -ne 0) { $Failed = $true }

$DocRequirements = [ordered]@{
    'README.md' = @('M9进程监督门禁为`MET`', 'M10尚未')
    'docs/PLANS.md' = @($RunRel, 'M9 BusyBox进程监督门禁为`MET`', 'M10及')
    'docs/DECISION_LOG.md' = @('D-037', $RunRel, 'M9总门禁为`MET`', 'M10仍未开始')
    'docs/OPEN_QUESTIONS.md' = @('M9为`MET`', 'M10仍未开始')
    'docs/RESUME_TRACEABILITY.md' = @($RunRel, '关闭M9为`MET`', 'M10没有开始')
    'docs/TEST_PLAN.md' = @($RunRel, 'M9为`MET`', 'M10仍不得自动开始')
    'docs/PROJECT_SPEC.md' = @('M9', 'BusyBox进程监督门禁为`MET`', 'M10尚未开始')
    'docs/milestones/M9.md' = @($RunRel, 'M9总门禁为 **MET**', 'M10没有开始')
}
$DocLines = @()
$DocFailures = 0
foreach ($Entry in $DocRequirements.GetEnumerator()) {
    $DocText = [IO.File]::ReadAllText((Join-Path $RepoRoot $Entry.Key))
    $Missing = @($Entry.Value | Where-Object { -not $DocText.Contains($_) })
    $DocResult = if ($Missing.Count -eq 0) { 'PASS' } else { 'FAIL' }
    $DocLines += "$($Entry.Key)=$DocResult required=$($Entry.Value.Count) missing=$($Missing.Count)"
    $DocFailures += $Missing.Count
}
$GatewayDiff = @(& git -C $RepoRoot diff --name-only -- gateway)
$M10Changed = if ($GatewayDiff.Count -eq 0) { 'NO' } else { 'YES' }
$DocLines += "m10_implementation_files_changed=$M10Changed"
if ($M10Changed -eq 'YES') { $DocFailures++ }
$DocLines += "result=$(if ($DocFailures -eq 0) { 'PASS' } else { 'FAIL' })"
Write-Lines (Join-Path $RunDir 'docs_consistency_v2.txt') $DocLines
if ($DocFailures -ne 0) { $Failed = $true }

$PreviousPreference = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
$DiffOutput = @(& git -C $RepoRoot diff --check 2>&1)
$DiffExit = $LASTEXITCODE
$ErrorActionPreference = $PreviousPreference
$DiffLines = @("exit_code=$DiffExit") + @($DiffOutput | ForEach-Object { $_.ToString() })
$DiffLines += "result=$(if ($DiffExit -eq 0) { 'PASS' } else { 'FAIL' })"
Write-Lines (Join-Path $RunDir 'git_diff_check_v2.txt') $DiffLines
if ($DiffExit -ne 0) { $Failed = $true }

$TrackedState = @(& git -C $RepoRoot status --short --branch --untracked-files=no)
$UntrackedCount = @(& git -C $RepoRoot ls-files --others --exclude-standard).Count
$TmpStatus = @(& git -C $RepoRoot status --short -- tmp/evidence.txt)
$ArtifactStatus = @(& git -C $RepoRoot status --short -- $RunRel)
$TrackedChangeCount = [Math]::Max(0, $TrackedState.Count - 1)
$StateLines = @(
    "captured_at=$((Get-Date).ToString('o'))"
    "head=$((& git -C $RepoRoot rev-parse HEAD).Trim())"
    "origin_master=$((& git -C $RepoRoot rev-parse origin/master).Trim())"
    "tracked_change_count=$TrackedChangeCount"
    "untracked_count=$UntrackedCount"
    "tmp_evidence_status=$($TmpStatus -join ' ')"
    "artifact_status=$($ArtifactStatus -join ' ')"
    'tracked_status:'
) + $TrackedState
Write-Lines (Join-Path $RunDir 'repository_state_v2.txt') $StateLines

$RawHashLines = @()
Get-ChildItem -LiteralPath $RawDir -File -Recurse | Sort-Object FullName | ForEach-Object {
    $Relative = $_.FullName.Substring($RunDir.Length + 1).Replace('\', '/')
    $Hash = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    $RawHashLines += "$Hash  $Relative"
}
Write-Lines (Join-Path $RunDir 'raw_private_sha256_v2.txt') $RawHashLines

$RawText = [IO.File]::ReadAllText($RawPath)
$Login = [regex]::Match($RawText, '\[(?<user>[^@\\\]\r\n]+)\\?@(?<target>[^:\\\]\r\n]+)\\?:')
$SensitiveFiles = @(
    (Get-ChildItem -LiteralPath $RunDir -File | ForEach-Object { $_.FullName })
    (Join-Path $RepoRoot 'README.md')
    (Join-Path $RepoRoot 'docs/PROJECT_SPEC.md')
    (Join-Path $RepoRoot 'docs/TEST_PLAN.md')
    (Join-Path $RepoRoot 'docs/OPEN_QUESTIONS.md')
    (Join-Path $RepoRoot 'docs/RESUME_TRACEABILITY.md')
    (Join-Path $RepoRoot 'docs/DECISION_LOG.md')
    (Join-Path $RepoRoot 'docs/PLANS.md')
    (Join-Path $RepoRoot 'docs/milestones/M9.md')
) | Sort-Object -Unique
$LeakCount = 0
if ($Login.Success) {
    $User = $Login.Groups['user'].Value
    $Target = $Login.Groups['target'].Value
    foreach ($File in $SensitiveFiles) {
        $Text = [IO.File]::ReadAllText($File)
        if ($Text.Contains("$User@$Target") -or $Text.Contains("$User\@$Target") -or $Text.Contains("@$Target")) { $LeakCount++ }
    }
}
$PrivateIps = @([regex]::Matches($RawText, '(?<![0-9])(?:10\.(?:[0-9]{1,3}\.){2}[0-9]{1,3}|192\.168\.(?:[0-9]{1,3}\.)[0-9]{1,3}|172\.(?:1[6-9]|2[0-9]|3[01])\.(?:[0-9]{1,3}\.)[0-9]{1,3})(?![0-9])') | ForEach-Object { $_.Value } | Sort-Object -Unique)
foreach ($File in $SensitiveFiles) {
    $Text = [IO.File]::ReadAllText($File)
    foreach ($Ip in $PrivateIps) { if ($Text.Contains($Ip)) { $LeakCount++ } }
    if ($Text -match '-----BEGIN (?:OPENSSH|RSA|EC|DSA) PRIVATE KEY-----') { $LeakCount++ }
    if ($Text -match '(?im)^\s*(?:broker_(?:host|username|password)|password|passwd|token|secret)\s*=\s*(?!<REDACTED>\s*$)\S.+$') { $LeakCount++ }
}
$TrackedPrivate = @(& git -C $RepoRoot ls-files -- "$RunRel/private_raw").Count
if ($TrackedPrivate -ne 0) { $LeakCount += $TrackedPrivate }
$SensitiveResult = if ($LeakCount -eq 0 -and $Login.Success) { 'PASS' } else { 'FAIL' }
$SensitiveLines = @(
    "scanned_file_count=$($SensitiveFiles.Count)"
    "raw_identity_available=$(if ($Login.Success) { 'YES' } else { 'NO' })"
    "raw_private_ipv4_value_count=$($PrivateIps.Count)"
    "public_sensitive_match_count=$LeakCount"
    "tracked_private_raw_count=$TrackedPrivate"
    "result=$SensitiveResult"
)
Write-Lines (Join-Path $RunDir 'sensitive_scan_v2.txt') $SensitiveLines
$SensitiveExit = if ($SensitiveResult -eq 'PASS') { 0 } else { 1 }
Write-Lines (Join-Path $RunDir 'sensitive_scan_v2.exit.txt') @("exit_code=$SensitiveExit")
if ($SensitiveExit -ne 0) { $Failed = $true }

Write-Lines (Join-Path $RunDir 'finalize_v2_pre_manifest.txt') @(
    "captured_at=$((Get-Date).ToString('o'))"
    "pre_manifest_result=$(if ($Failed) { 'FAIL' } else { 'PASS' })"
    'v1_partial_outputs_preserved=YES'
)

$ManifestPath = Join-Path $RunDir 'artifact_manifest.sha256'
$ManifestCheckPath = Join-Path $RunDir 'artifact_manifest_check.txt'
$ManifestLines = @()
Get-ChildItem -LiteralPath $RunDir -File -Recurse | Where-Object {
    $_.FullName -notlike "$RawDir\*" -and
    $_.FullName -ne $ManifestPath -and
    $_.FullName -ne $ManifestCheckPath
} | Sort-Object FullName | ForEach-Object {
    $Relative = $_.FullName.Substring($RunDir.Length + 1).Replace('\', '/')
    $Hash = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    $ManifestLines += "$Hash  $Relative"
}
Write-Lines $ManifestPath $ManifestLines

$ManifestFailures = 0
foreach ($Line in $ManifestLines) {
    $Parts = $Line -split '  ', 2
    $Actual = (Get-FileHash -LiteralPath (Join-Path $RunDir $Parts[1]) -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($Actual -ne $Parts[0]) { $ManifestFailures++ }
}
Write-Lines $ManifestCheckPath @(
    "entry_count=$($ManifestLines.Count)"
    "mismatch_count=$ManifestFailures"
    "result=$(if ($ManifestFailures -eq 0) { 'PASS' } else { 'FAIL' })"
)
if ($ManifestFailures -ne 0) { $Failed = $true }

if ($Failed) { exit 1 }
exit 0
