$ErrorActionPreference = 'Stop'
$RunDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = (Resolve-Path (Join-Path $RunDir '..\..')).Path
$PrivateDir = Join-Path $RunDir 'private_raw'
$Outputs = @(
    'powershell_syntax_check.v4.txt',
    'git_diff_cached_check.v4.txt',
    'raw_private_sha256.v4.txt',
    'sensitive_scan.v4.txt',
    'sensitive_scan.v4.exit.txt',
    'artifact_manifest.v4.sha256',
    'artifact_manifest_check.v4.txt'
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
[IO.File]::WriteAllLines((Join-Path $RunDir 'powershell_syntax_check.v4.txt'), $PsLines, [Text.UTF8Encoding]::new($false))

$PreviousErrorAction = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
$DiffOutput = & git -C $RepoRoot diff --cached --check 2>&1 | Out-String
$DiffExit = $LASTEXITCODE
$ErrorActionPreference = $PreviousErrorAction
$DiffLines = New-Object Collections.Generic.List[string]
$DiffLines.Add('command=git diff --cached --check')
$DiffLines.Add("exit_code=$DiffExit")
$DiffLines.Add('--- full output ---')
if ($DiffOutput.Trim()) {
    foreach ($Line in ($DiffOutput.TrimEnd() -split '\r?\n')) { $DiffLines.Add($Line) }
}
[IO.File]::WriteAllLines((Join-Path $RunDir 'git_diff_cached_check.v4.txt'), $DiffLines, [Text.UTF8Encoding]::new($false))

$PrivateHashLines = New-Object Collections.Generic.List[string]
foreach ($File in Get-ChildItem -LiteralPath $PrivateDir -Recurse -File | Sort-Object FullName) {
    $Relative = $File.FullName.Substring($PrivateDir.Length + 1).Replace('\', '/')
    $Sha = (Get-FileHash -LiteralPath $File.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    $PrivateHashLines.Add("$Sha  private_raw/$Relative  bytes=$($File.Length)")
}
[IO.File]::WriteAllLines((Join-Path $RunDir 'raw_private_sha256.v4.txt'), $PrivateHashLines, [Text.UTF8Encoding]::new($false))

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
$SensitiveLines = @(
    "scanned_file_count=$($ScanFiles.Count)"
    "exact_endpoint_leak_count=$EndpointLeak"
    "endpoint_suffix_leak_count=$EndpointSuffixLeak"
    "login_username_at_leak_count=$LoginLeak"
    "private_ipv4_literal_count=$PrivateIpv4Leak"
    "private_key_marker_count=$PrivateKeyLeak"
    "nonempty_credential_assignment_count=$CredentialLeak"
    "tracked_private_raw_count=$($PrivateTracked.Count)"
    "status=$(if ($SensitiveTotal -eq 0) { 'PASS' } else { 'FAIL' })"
)
[IO.File]::WriteAllLines((Join-Path $RunDir 'sensitive_scan.v4.txt'), $SensitiveLines, [Text.UTF8Encoding]::new($false))
[IO.File]::WriteAllText((Join-Path $RunDir 'sensitive_scan.v4.exit.txt'), "$([int]($SensitiveTotal -ne 0))`r`n", [Text.Encoding]::ASCII)

$ManifestPath = Join-Path $RunDir 'artifact_manifest.v4.sha256'
$ManifestFiles = @(Get-ChildItem -LiteralPath $RunDir -File | Where-Object {
    $_.Name -notin @('artifact_manifest.v4.sha256', 'artifact_manifest_check.v4.txt')
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
[IO.File]::WriteAllLines((Join-Path $RunDir 'artifact_manifest_check.v4.txt'), $CheckLines, [Text.UTF8Encoding]::new($false))

"POWERSHELL_PARSE_ERRORS=$PsErrors"
"GIT_DIFF_CACHED_CHECK_EXIT=$DiffExit"
"SENSITIVE_TOTAL=$SensitiveTotal"
"MANIFEST_MISMATCHES=$Mismatches"
if ($PsErrors -ne 0 -or $DiffExit -ne 0 -or $SensitiveTotal -ne 0 -or $Mismatches -ne 0) { exit 1 }
