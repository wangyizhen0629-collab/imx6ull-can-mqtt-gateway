$ErrorActionPreference = 'Stop'
$RunDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = (Resolve-Path (Join-Path $RunDir '..\..')).Path
$PrivateDir = Join-Path $RunDir 'private_raw'
$Outputs = @(
    'powershell_syntax_check.v3.txt',
    'shell_syntax_check.v3.txt',
    'json_syntax_check.v3.txt',
    'git_diff_cached_check.v3.txt',
    'raw_private_sha256.v3.txt',
    'sensitive_scan.v3.txt',
    'sensitive_scan.v3.exit.txt',
    'artifact_manifest.v3.sha256',
    'artifact_manifest_check.v3.txt'
)
foreach ($Name in $Outputs) {
    $Path = Join-Path $RunDir $Name
    if (Test-Path -LiteralPath $Path) { throw "refusing to overwrite existing evidence: $Path" }
}

$PsLines = New-Object Collections.Generic.List[string]
$PsErrors = 0
foreach ($File in Get-ChildItem -LiteralPath $RunDir -File -Filter '*.ps1' | Sort-Object Name) {
    $Tokens = $null
    $Errors = $null
    [Management.Automation.Language.Parser]::ParseFile($File.FullName, [ref]$Tokens, [ref]$Errors) | Out-Null
    $PsLines.Add("file=$($File.Name) parse_error_count=$($Errors.Count)")
    foreach ($Error in $Errors) { $PsLines.Add("error=$($Error.Message) line=$($Error.Extent.StartLineNumber)") }
    $PsErrors += $Errors.Count
}
$PsLines.Add("total_parse_errors=$PsErrors")
[IO.File]::WriteAllLines((Join-Path $RunDir 'powershell_syntax_check.v3.txt'), $PsLines, [Text.UTF8Encoding]::new($false))

$ShellLines = New-Object Collections.Generic.List[string]
$ShellStructuralFailures = 0
$ExecutedByTarget = @(
    'board_binary_hard_gate.sh',
    'board_library_predeploy_gate.sh',
    'board_install.sh',
    'board_reload_start.sh',
    'board_reload_classification_v2.sh',
    'board_sigkill_recovery.sh',
    'board_controlled_restart.sh',
    'board_storm_test.sh',
    'board_pre_reboot.sh'
)
foreach ($File in Get-ChildItem -LiteralPath $RunDir -File -Filter '*.sh' | Sort-Object Name) {
    $Bytes = [IO.File]::ReadAllBytes($File.FullName)
    $Text = [IO.File]::ReadAllText($File.FullName)
    $HasNul = $Bytes -contains 0
    $HasShebang = $Text.StartsWith('#!')
    if ($HasNul -or -not $HasShebang) { $ShellStructuralFailures++ }
    $Execution = if ($File.Name -in $ExecutedByTarget) { 'target_busybox_ash_evidence_present' } else { 'NOT_RUN' }
    $ShellLines.Add("file=$($File.Name) shebang=$HasShebang nul_byte=$HasNul execution=$Execution")
}
$ShellLines.Add('syntax_parser=NOT_RUN; Git Bash, WSL distro and shellcheck are not installed on this Windows host')
$ShellLines.Add('scope_note=parser absence is not converted to PASS; target-executed scripts are identified individually')
$ShellLines.Add("structural_failures=$ShellStructuralFailures")
[IO.File]::WriteAllLines((Join-Path $RunDir 'shell_syntax_check.v3.txt'), $ShellLines, [Text.UTF8Encoding]::new($false))

$JsonLines = New-Object Collections.Generic.List[string]
$JsonFailures = 0
foreach ($Name in @('summary.json', 'manifest.json')) {
    try {
        Get-Content -LiteralPath (Join-Path $RunDir $Name) -Raw | ConvertFrom-Json | Out-Null
        $JsonLines.Add("file=$Name status=PASS")
    } catch {
        $JsonFailures++
        $JsonLines.Add("file=$Name status=FAIL error=$($_.Exception.Message)")
    }
}
$JsonLines.Add("total_failures=$JsonFailures")
[IO.File]::WriteAllLines((Join-Path $RunDir 'json_syntax_check.v3.txt'), $JsonLines, [Text.UTF8Encoding]::new($false))

$PreviousErrorAction = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
$DiffOutput = & git -C $RepoRoot diff --cached --check 2>&1 | Out-String
$DiffExit = $LASTEXITCODE
$ErrorActionPreference = $PreviousErrorAction
$DiffText = @(
    'command=git diff --cached --check'
    "exit_code=$DiffExit"
    '--- full output ---'
    $DiffOutput.TrimEnd()
) -join "`r`n"
[IO.File]::WriteAllText((Join-Path $RunDir 'git_diff_cached_check.v3.txt'), $DiffText + "`r`n", [Text.UTF8Encoding]::new($false))

$PrivateHashLines = New-Object Collections.Generic.List[string]
foreach ($File in Get-ChildItem -LiteralPath $PrivateDir -Recurse -File | Sort-Object FullName) {
    $Relative = $File.FullName.Substring($PrivateDir.Length + 1).Replace('\', '/')
    $Sha = (Get-FileHash -LiteralPath $File.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    $PrivateHashLines.Add("$Sha  private_raw/$Relative  bytes=$($File.Length)")
}
[IO.File]::WriteAllLines((Join-Path $RunDir 'raw_private_sha256.v3.txt'), $PrivateHashLines, [Text.UTF8Encoding]::new($false))

$ChangedDocs = @(
    'README.md',
    'docs/DECISION_LOG.md',
    'docs/OPEN_QUESTIONS.md',
    'docs/PLANS.md',
    'docs/PROJECT_SPEC.md',
    'docs/RESUME_TRACEABILITY.md',
    'docs/TEST_PLAN.md',
    'docs/milestones/M9.md'
)
$Endpoint = ((Get-Content -LiteralPath (Join-Path $PrivateDir 'board_endpoint.txt') -Raw) -split '=', 2)[1].Trim()
$LoginUser = ((Get-Content -LiteralPath (Join-Path $PrivateDir 'board_user.txt') -Raw) -split '=', 2)[1].Trim()
$EndpointParts = @($Endpoint -split '\.')
$EndpointSuffix = if ($EndpointParts.Count -ge 2) { ($EndpointParts[($EndpointParts.Count - 2)..($EndpointParts.Count - 1)] -join '.') } else { $Endpoint }
$ScanFiles = New-Object Collections.Generic.List[IO.FileInfo]
foreach ($File in Get-ChildItem -LiteralPath $RunDir -File) { $ScanFiles.Add($File) }
foreach ($Relative in $ChangedDocs) { $ScanFiles.Add((Get-Item -LiteralPath (Join-Path $RepoRoot $Relative))) }
$EndpointLeak = 0
$EndpointSuffixLeak = 0
$LoginLeak = 0
$PrivateIpv4Leak = 0
$PrivateKeyLeak = 0
$CredentialLeak = 0
foreach ($File in $ScanFiles) {
    $Text = Get-Content -LiteralPath $File.FullName -Raw
    if ($Endpoint) { $EndpointLeak += ([regex]::Matches($Text, [regex]::Escape($Endpoint))).Count }
    if ($EndpointSuffix) { $EndpointSuffixLeak += ([regex]::Matches($Text, '(?<![0-9])' + [regex]::Escape($EndpointSuffix) + '(?![0-9])')).Count }
    if ($LoginUser) { $LoginLeak += ([regex]::Matches($Text, [regex]::Escape($LoginUser + '@'))).Count }
    $PrivateIpv4Leak += ([regex]::Matches($Text, '(?<![0-9])(?:10\.(?:[0-9]{1,3}\.){2}[0-9]{1,3}|192\.168\.(?:[0-9]{1,3}\.)[0-9]{1,3}|172\.(?:1[6-9]|2[0-9]|3[01])\.(?:[0-9]{1,3}\.)[0-9]{1,3})(?![0-9])')).Count
    $PrivateKeyLeak += ([regex]::Matches($Text, '-----BEGIN (?:OPENSSH |RSA |EC |DSA )?PRIVATE KEY-----')).Count
    foreach ($Line in ($Text -split '\r?\n')) {
        if ($Line -match '^\s*(?:broker_(?:host|username|password)|password|passwd)\s*=\s*(.*)$') {
            $Value = $Matches[1].Trim()
            if ($Value -and $Value -notmatch '^<(?:REDACTED|PRIVATE)>$') { $CredentialLeak++ }
        }
    }
}
$PrivateTracked = @(& git -C $RepoRoot ls-files -- 'artifacts/20260901T204152+0800-m9-windows-board-gate-final/private_raw')
$SensitiveTotal = $EndpointLeak + $EndpointSuffixLeak + $LoginLeak + $PrivateIpv4Leak + $PrivateKeyLeak + $CredentialLeak + $PrivateTracked.Count
$SensitiveText = @(
    "scanned_file_count=$($ScanFiles.Count)"
    "exact_endpoint_leak_count=$EndpointLeak"
    "endpoint_suffix_leak_count=$EndpointSuffixLeak"
    "login_username_at_leak_count=$LoginLeak"
    "private_ipv4_literal_count=$PrivateIpv4Leak"
    "private_key_marker_count=$PrivateKeyLeak"
    "nonempty_credential_assignment_count=$CredentialLeak"
    "tracked_private_raw_count=$($PrivateTracked.Count)"
    "status=$(if ($SensitiveTotal -eq 0) { 'PASS' } else { 'FAIL' })"
) -join "`r`n"
[IO.File]::WriteAllText((Join-Path $RunDir 'sensitive_scan.v3.txt'), $SensitiveText + "`r`n", [Text.UTF8Encoding]::new($false))
[IO.File]::WriteAllText((Join-Path $RunDir 'sensitive_scan.v3.exit.txt'), "$([int]($SensitiveTotal -ne 0))`r`n", [Text.Encoding]::ASCII)

$ManifestPath = Join-Path $RunDir 'artifact_manifest.v3.sha256'
$ManifestFiles = @(Get-ChildItem -LiteralPath $RunDir -File | Where-Object {
    $_.Name -notin @('artifact_manifest.v3.sha256', 'artifact_manifest_check.v3.txt')
} | Sort-Object Name)
$ManifestLines = foreach ($File in $ManifestFiles) {
    $Sha = (Get-FileHash -LiteralPath $File.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    "$Sha  $($File.Name)"
}
[IO.File]::WriteAllLines($ManifestPath, $ManifestLines, [Text.Encoding]::ASCII)
$CheckLines = New-Object Collections.Generic.List[string]
$Mismatches = 0
foreach ($Line in [IO.File]::ReadAllLines($ManifestPath)) {
    if ($Line -notmatch '^([0-9a-f]{64})  (.+)$') { $Mismatches++; $CheckLines.Add("MALFORMED $Line"); continue }
    $Expected = $Matches[1]
    $Name = $Matches[2]
    $Path = Join-Path $RunDir $Name
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { $Mismatches++; $CheckLines.Add("MISSING $Name"); continue }
    $Actual = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($Actual -eq $Expected) { $CheckLines.Add("OK $Name") } else { $Mismatches++; $CheckLines.Add("MISMATCH $Name") }
}
$CheckLines.Add("checked=$($ManifestLines.Count)")
$CheckLines.Add("mismatches=$Mismatches")
$CheckLines.Add("status=$(if ($Mismatches -eq 0) { 'PASS' } else { 'FAIL' })")
[IO.File]::WriteAllLines((Join-Path $RunDir 'artifact_manifest_check.v3.txt'), $CheckLines, [Text.UTF8Encoding]::new($false))

"POWERSHELL_PARSE_ERRORS=$PsErrors"
"SHELL_PARSER=NOT_RUN"
"SHELL_STRUCTURAL_FAILURES=$ShellStructuralFailures"
"JSON_FAILURES=$JsonFailures"
"GIT_DIFF_CACHED_CHECK_EXIT=$DiffExit"
"SENSITIVE_TOTAL=$SensitiveTotal"
"MANIFEST_MISMATCHES=$Mismatches"
if ($PsErrors -ne 0 -or $ShellStructuralFailures -ne 0 -or $JsonFailures -ne 0 -or $DiffExit -ne 0 -or
    $SensitiveTotal -ne 0 -or $Mismatches -ne 0) { exit 1 }
