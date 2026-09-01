$ErrorActionPreference = 'Stop'
$RunDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = (Resolve-Path (Join-Path $RunDir '..\..')).Path
$PrivateDir = Join-Path $RunDir 'private_raw'
$Outputs = @(
    'powershell_syntax_check.txt',
    'git_diff_check.txt',
    'repository_state.txt',
    'raw_private_sha256.txt',
    'sensitive_scan.txt',
    'sensitive_scan.exit.txt',
    'artifact_manifest.sha256',
    'artifact_manifest_check.txt'
)
foreach ($Name in $Outputs) {
    $Path = Join-Path $RunDir $Name
    if (Test-Path -LiteralPath $Path) {
        throw "refusing to overwrite existing evidence: $Path"
    }
}

$ScriptFiles = @(Get-ChildItem -LiteralPath $RunDir -File -Filter '*.ps1')
$SyntaxLines = New-Object Collections.Generic.List[string]
$SyntaxErrorCount = 0
foreach ($File in $ScriptFiles) {
    $Tokens = $null
    $Errors = $null
    [Management.Automation.Language.Parser]::ParseFile($File.FullName, [ref]$Tokens, [ref]$Errors) | Out-Null
    $SyntaxLines.Add("file=$($File.Name) parse_error_count=$($Errors.Count)")
    foreach ($Error in $Errors) {
        $SyntaxLines.Add("error=$($Error.Message) line=$($Error.Extent.StartLineNumber)")
    }
    $SyntaxErrorCount += $Errors.Count
}
$SyntaxLines.Add("total_parse_errors=$SyntaxErrorCount")
$SyntaxLines | Set-Content -LiteralPath (Join-Path $RunDir 'powershell_syntax_check.txt') -Encoding utf8

$PreviousErrorAction = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
$DiffOutput = & git -C $RepoRoot diff --check 2>&1 | Out-String
$DiffExit = $LASTEXITCODE
$ErrorActionPreference = $PreviousErrorAction
@(
    'command=git diff --check'
    "exit_code=$DiffExit"
    '--- full output ---'
    $DiffOutput.TrimEnd()
) | Set-Content -LiteralPath (Join-Path $RunDir 'git_diff_check.txt') -Encoding utf8

$TrackedStatus = @(& git -C $RepoRoot status --porcelain=v1 --untracked-files=no)
$Untracked = @(& git -C $RepoRoot ls-files --others --exclude-standard)
$RunPublic = @(Get-ChildItem -LiteralPath $RunDir -File | Sort-Object Name)
@(
    "captured_at=$((Get-Date).ToString('o'))"
    "head=$((& git -C $RepoRoot rev-parse HEAD).Trim())"
    "branch=$((& git -C $RepoRoot branch --show-current).Trim())"
    "tracked_status_count=$($TrackedStatus.Count)"
    $TrackedStatus
    "untracked_total_count=$($Untracked.Count)"
    "current_run_public_file_count=$($RunPublic.Count)"
    'unrelated_untracked_files=preserved and intentionally not enumerated in this artifact'
) | Set-Content -LiteralPath (Join-Path $RunDir 'repository_state.txt') -Encoding utf8

$PrivateFiles = @(Get-ChildItem -LiteralPath $PrivateDir -Recurse -File | Sort-Object FullName)
$PrivateHashLines = foreach ($File in $PrivateFiles) {
    $Relative = $File.FullName.Substring($PrivateDir.Length + 1).Replace('\', '/')
    $Sha = (Get-FileHash -LiteralPath $File.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    "$Sha  private_raw/$Relative  bytes=$($File.Length)"
}
$PrivateHashLines | Set-Content -LiteralPath (Join-Path $RunDir 'raw_private_sha256.txt') -Encoding utf8

$ScanFiles = New-Object Collections.Generic.List[IO.FileInfo]
foreach ($File in Get-ChildItem -LiteralPath $RunDir -File) { $ScanFiles.Add($File) }
$ChangedDocs = @(
    'README.md',
    'deploy/README.md',
    'docs/DECISION_LOG.md',
    'docs/OPEN_QUESTIONS.md',
    'docs/PLANS.md',
    'docs/PROJECT_SPEC.md',
    'docs/RESUME_TRACEABILITY.md',
    'docs/TEST_PLAN.md',
    'docs/milestones/M9.md'
)
foreach ($Relative in $ChangedDocs) { $ScanFiles.Add((Get-Item -LiteralPath (Join-Path $RepoRoot $Relative))) }
$Endpoint = ((Get-Content -LiteralPath (Join-Path $PrivateDir 'board_endpoint.txt') -Raw) -split '=', 2)[1].Trim()
$EndpointLeakCount = 0
$PrivateIpv4Count = 0
$PrivateKeyMarkerCount = 0
$CredentialAssignmentCount = 0
foreach ($File in $ScanFiles) {
    $Text = Get-Content -LiteralPath $File.FullName -Raw
    if ($Endpoint) { $EndpointLeakCount += ([regex]::Matches($Text, [regex]::Escape($Endpoint))).Count }
    $PrivateIpv4Count += ([regex]::Matches($Text, '(?<![0-9])(?:10\.(?:[0-9]{1,3}\.){2}[0-9]{1,3}|192\.168\.(?:[0-9]{1,3}\.)[0-9]{1,3}|172\.(?:1[6-9]|2[0-9]|3[01])\.(?:[0-9]{1,3}\.)[0-9]{1,3})(?![0-9])')).Count
    $PrivateKeyMarkerCount += ([regex]::Matches($Text, '-----BEGIN (?:OPENSSH |RSA |EC |DSA )?PRIVATE KEY-----')).Count
    $CredentialAssignmentCount += ([regex]::Matches($Text, '(?im)^\s*(?:broker_(?:host|username|password)|password|passwd)\s*=\s*(?!\s*$|<REDACTED>\s*$).+$')).Count
}
$SensitiveTotal = $EndpointLeakCount + $PrivateIpv4Count + $PrivateKeyMarkerCount + $CredentialAssignmentCount
@(
    "scanned_file_count=$($ScanFiles.Count)"
    "exact_endpoint_leak_count=$EndpointLeakCount"
    "private_ipv4_literal_count=$PrivateIpv4Count"
    "private_key_marker_count=$PrivateKeyMarkerCount"
    "nonempty_credential_assignment_count=$CredentialAssignmentCount"
    "status=$(if ($SensitiveTotal -eq 0) { 'PASS' } else { 'FAIL' })"
) | Set-Content -LiteralPath (Join-Path $RunDir 'sensitive_scan.txt') -Encoding utf8
"$([int]($SensitiveTotal -ne 0))" | Set-Content -LiteralPath (Join-Path $RunDir 'sensitive_scan.exit.txt') -Encoding ascii

$ManifestPath = Join-Path $RunDir 'artifact_manifest.sha256'
$ManifestFiles = @(Get-ChildItem -LiteralPath $RunDir -File | Where-Object {
    $_.Name -notin @('artifact_manifest.sha256', 'artifact_manifest_check.txt')
} | Sort-Object Name)
$ManifestLines = foreach ($File in $ManifestFiles) {
    $Sha = (Get-FileHash -LiteralPath $File.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    "$Sha  $($File.Name)"
}
$ManifestLines | Set-Content -LiteralPath $ManifestPath -Encoding ascii
$CheckLines = New-Object Collections.Generic.List[string]
$MismatchCount = 0
foreach ($Line in Get-Content -LiteralPath $ManifestPath) {
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
$CheckLines | Set-Content -LiteralPath (Join-Path $RunDir 'artifact_manifest_check.txt') -Encoding utf8

"POWERSHELL_PARSE_ERRORS=$SyntaxErrorCount"
"GIT_DIFF_CHECK_EXIT=$DiffExit"
"SENSITIVE_TOTAL=$SensitiveTotal"
"MANIFEST_CHECK_MISMATCHES=$MismatchCount"
if ($SyntaxErrorCount -ne 0 -or $DiffExit -ne 0 -or $SensitiveTotal -ne 0 -or $MismatchCount -ne 0) { exit 1 }
