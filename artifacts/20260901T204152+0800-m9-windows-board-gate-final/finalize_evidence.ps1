$ErrorActionPreference = 'Stop'
$RunDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = (Resolve-Path (Join-Path $RunDir '..\..')).Path
$PrivateDir = Join-Path $RunDir 'private_raw'
$Outputs = @(
    'powershell_syntax_check.txt',
    'shell_syntax_check.txt',
    'json_syntax_check.txt',
    'git_diff_check.txt',
    'repository_state.txt',
    'raw_private_sha256.txt',
    'docs_consistency.txt',
    'sensitive_scan.txt',
    'sensitive_scan.exit.txt',
    'artifact_manifest.sha256',
    'artifact_manifest_check.txt'
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
[IO.File]::WriteAllLines((Join-Path $RunDir 'powershell_syntax_check.txt'), $PsLines, [Text.UTF8Encoding]::new($false))

$Bash = 'C:\Program Files\Git\bin\bash.exe'
$ShellLines = New-Object Collections.Generic.List[string]
$ShellFailures = 0
if (Test-Path -LiteralPath $Bash -PathType Leaf) {
    foreach ($File in Get-ChildItem -LiteralPath $RunDir -File -Filter '*.sh' | Sort-Object Name) {
        $Relative = $File.FullName.Substring($RepoRoot.Length + 1).Replace('\', '/')
        $Output = & $Bash -n $Relative 2>&1 | Out-String
        $Exit = $LASTEXITCODE
        $ShellLines.Add("file=$($File.Name) command=git-bash -n exit=$Exit")
        if ($Output.Trim()) { $ShellLines.Add($Output.TrimEnd()) }
        if ($Exit -ne 0) { $ShellFailures++ }
    }
} else {
    $ShellLines.Add('git_bash=NOT_AVAILABLE')
    $ShellFailures++
}
$ShellLines.Add("total_failures=$ShellFailures")
[IO.File]::WriteAllLines((Join-Path $RunDir 'shell_syntax_check.txt'), $ShellLines, [Text.UTF8Encoding]::new($false))

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
[IO.File]::WriteAllLines((Join-Path $RunDir 'json_syntax_check.txt'), $JsonLines, [Text.UTF8Encoding]::new($false))

$PreviousErrorAction = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
$DiffOutput = & git -C $RepoRoot diff --check 2>&1 | Out-String
$DiffExit = $LASTEXITCODE
$ErrorActionPreference = $PreviousErrorAction
$DiffText = @(
    'command=git diff --check'
    "exit_code=$DiffExit"
    '--- full output ---'
    $DiffOutput.TrimEnd()
) -join "`r`n"
[IO.File]::WriteAllText((Join-Path $RunDir 'git_diff_check.txt'), $DiffText + "`r`n", [Text.UTF8Encoding]::new($false))

$TrackedStatus = @(& git -C $RepoRoot status --porcelain=v1 --untracked-files=no)
$Untracked = @(& git -C $RepoRoot ls-files --others --exclude-standard)
$RepoText = @(
    "captured_at=$((Get-Date).ToString('o'))"
    "head=$((& git -C $RepoRoot rev-parse HEAD).Trim())"
    "branch=$((& git -C $RepoRoot branch --show-current).Trim())"
    "tracked_status_count=$($TrackedStatus.Count)"
    $TrackedStatus
    "untracked_total_count=$($Untracked.Count)"
    'unrelated_untracked_files=preserved; not enumerated here; complete start status is in private/public local_preflight evidence'
) -join "`r`n"
[IO.File]::WriteAllText((Join-Path $RunDir 'repository_state.txt'), $RepoText + "`r`n", [Text.UTF8Encoding]::new($false))

$PrivateHashLines = New-Object Collections.Generic.List[string]
foreach ($File in Get-ChildItem -LiteralPath $PrivateDir -Recurse -File | Sort-Object FullName) {
    $Relative = $File.FullName.Substring($PrivateDir.Length + 1).Replace('\', '/')
    $Sha = (Get-FileHash -LiteralPath $File.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    $PrivateHashLines.Add("$Sha  private_raw/$Relative  bytes=$($File.Length)")
}
[IO.File]::WriteAllLines((Join-Path $RunDir 'raw_private_sha256.txt'), $PrivateHashLines, [Text.UTF8Encoding]::new($false))

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
$DocsLines = New-Object Collections.Generic.List[string]
$DocsFailures = 0
foreach ($Relative in $ChangedDocs) {
    $Text = Get-Content -LiteralPath (Join-Path $RepoRoot $Relative) -Raw
    $RunRefs = ([regex]::Matches($Text, '20260901T204152\+0800-m9-windows-board-gate-final')).Count
    $NotMet = ([regex]::Matches($Text, '(?i)M9.{0,80}(?:NOT.?MET|总门禁.{0,20}NOT.?MET)', 'Singleline')).Count
    $M10 = ([regex]::Matches($Text, '(?i)M10.{0,40}(?:未开始|不得开始|没有开始|尚未开始)', 'Singleline')).Count
    $Status = if ($RunRefs -gt 0 -and $NotMet -gt 0 -and $M10 -gt 0) { 'PASS' } else { 'FAIL' }
    if ($Status -eq 'FAIL') { $DocsFailures++ }
    $DocsLines.Add("file=$Relative run_refs=$RunRefs m9_not_met_matches=$NotMet m10_not_started_matches=$M10 status=$Status")
}
$DocsLines.Add("total_failures=$DocsFailures")
[IO.File]::WriteAllLines((Join-Path $RunDir 'docs_consistency.txt'), $DocsLines, [Text.UTF8Encoding]::new($false))

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
    $CredentialLeak += ([regex]::Matches($Text, '(?im)^\s*(?:broker_(?:host|username|password)|password|passwd)\s*=\s*(?!\s*$|<REDACTED>\s*$).+$')).Count
}
$PrivateTracked = @(& git -C $RepoRoot ls-files -- "artifacts/20260901T204152+0800-m9-windows-board-gate-final/private_raw")
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
[IO.File]::WriteAllText((Join-Path $RunDir 'sensitive_scan.txt'), $SensitiveText + "`r`n", [Text.UTF8Encoding]::new($false))
[IO.File]::WriteAllText((Join-Path $RunDir 'sensitive_scan.exit.txt'), "$([int]($SensitiveTotal -ne 0))`r`n", [Text.Encoding]::ASCII)

$ManifestPath = Join-Path $RunDir 'artifact_manifest.sha256'
$ManifestFiles = @(Get-ChildItem -LiteralPath $RunDir -File | Where-Object {
    $_.Name -notin @('artifact_manifest.sha256', 'artifact_manifest_check.txt')
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
[IO.File]::WriteAllLines((Join-Path $RunDir 'artifact_manifest_check.txt'), $CheckLines, [Text.UTF8Encoding]::new($false))

"POWERSHELL_PARSE_ERRORS=$PsErrors"
"SHELL_SYNTAX_FAILURES=$ShellFailures"
"JSON_FAILURES=$JsonFailures"
"GIT_DIFF_CHECK_EXIT=$DiffExit"
"DOCS_CONSISTENCY_FAILURES=$DocsFailures"
"SENSITIVE_TOTAL=$SensitiveTotal"
"MANIFEST_MISMATCHES=$Mismatches"
if ($PsErrors -ne 0 -or $ShellFailures -ne 0 -or $JsonFailures -ne 0 -or $DiffExit -ne 0 -or
    $DocsFailures -ne 0 -or $SensitiveTotal -ne 0 -or $Mismatches -ne 0) { exit 1 }
