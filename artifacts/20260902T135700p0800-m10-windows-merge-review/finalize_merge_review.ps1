[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$artifactDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path (Join-Path $artifactDir '..\..')).Path
$finalScanPath = Join-Path $artifactDir '04b_sensitive_scan_final.txt'
$reviewPath = Join-Path $artifactDir '07_final_staged_review.txt'
$diffPath = Join-Path $artifactDir '08_final_diff_check.txt'
$reportPath = Join-Path $artifactDir 'test_report_final.md'
$manifestPath = Join-Path $artifactDir 'artifact_manifest.sha256'
$manifestCheckPath = Join-Path $artifactDir 'artifact_manifest_check.txt'

foreach ($path in @($finalScanPath, $reviewPath, $diffPath, $reportPath, $manifestPath, $manifestCheckPath)) {
    if (Test-Path -LiteralPath $path) {
        throw "Refusing to overwrite final evidence: $path"
    }
}

function Write-Utf8 {
    param([string]$Path, [object[]]$Lines)
    $Lines | Out-File -LiteralPath $Path -Encoding utf8
}

$allowedCredentialFindings = @{
    'gateway/src/main.c:591' = 'sink_config.broker_password = config->broker_password;'
    'gateway/src/mqtt_sink.c:542' = 'config->broker_username == NULL || config->broker_password == NULL ||'
    'gateway/tests/test_config.c:51' = '"broker_password=fake-test-secret\n"'
    'gateway/tests/test_config.c:178' = 'CHECK(gateway_config_apply_assignment(&config, "broker_password=secret",'
}
$privateKeyPattern = '-----BEGIN (RSA |OPENSSH |EC )?PRIVATE KEY-----'
$privateIpPattern = '(?<![0-9])(?:10(?:\.[0-9]{1,3}){3}|192\.168(?:\.[0-9]{1,3}){2}|172\.(?:1[6-9]|2[0-9]|3[01])(?:\.[0-9]{1,3}){2}|169\.254(?:\.[0-9]{1,3}){2})(?![0-9])'
$credentialPattern = '(?i)(password|passwd|secret|token|api[_-]?key)\s*[:=]\s*["'']?([^\s"''<>{}\[\]]+)'
$excludedDetectorSources = @(
    'artifacts/20260902T135700p0800-m10-windows-merge-review/run_merge_review.ps1',
    'artifacts/20260902T135700p0800-m10-windows-merge-review/finalize_merge_review.ps1'
)
$excludedCredentialEvidence = @(
    'artifacts/20260902T135700p0800-m10-windows-merge-review/04_sensitive_scan_interpretation.md'
)

$stagedPaths = @(git -C $repoRoot diff --cached --name-only --diff-filter=ACMR)
$actionable = @()
$reviewed = @()
foreach ($relativePath in $stagedPaths) {
    if ($relativePath -match '(?i)(^|/)(private_raw)(/|$)|(^|/)(id_rsa|id_ed25519)(\.|$)|\.(pem|key|p12|pfx)$') {
        $actionable += "forbidden_filename=$relativePath"
    }
    if ($excludedDetectorSources -contains $relativePath) {
        continue
    }
    $content = @(git -C $repoRoot show ":$relativePath" 2>$null)
    $lineNumber = 0
    foreach ($line in $content) {
        $lineNumber++
        if ($line -match $privateKeyPattern) {
            $actionable += "private_key_marker=$relativePath`:$lineNumber"
        }
        if ($line -match $privateIpPattern) {
            $actionable += "private_ip=$relativePath`:$lineNumber value=$($Matches[0])"
        }
        if ($excludedCredentialEvidence -notcontains $relativePath -and
            $line -match $credentialPattern) {
            $location = "$relativePath`:$lineNumber"
            if ($allowedCredentialFindings.ContainsKey($location) -and
                $line.Trim() -eq $allowedCredentialFindings[$location]) {
                $reviewed += "reviewed_non_secret=$location"
            }
            else {
                $actionable += "credential_assignment=$location key=$($Matches[1])"
            }
        }
    }
}
$binaryNumstat = @(git -C $repoRoot diff --cached --numstat | Where-Object { $_ -match '^-\s+-\s+' })
foreach ($line in $binaryNumstat) {
    $actionable += "binary_staged=$line"
}
$scanLines = @(
    "started_local=$(Get-Date -Format 'o')",
    'scope=all staged tracked blobs; detector-source scripts excluded from content matching only',
    "files_scanned=$($stagedPaths.Count)",
    "reviewed_non_secret=$($reviewed.Count)",
    "actionable_findings=$($actionable.Count)"
) + $reviewed + $actionable + @(
    "result=$(if ($actionable.Count -eq 0 -and $reviewed.Count -eq 4) { 'PASS' } else { 'FAIL' })",
    "finished_local=$(Get-Date -Format 'o')"
)
Write-Utf8 -Path $finalScanPath -Lines $scanLines
$scanPass = $actionable.Count -eq 0 -and $reviewed.Count -eq 4

$nameStatus = @(git -C $repoRoot diff --cached --name-status)
$privatePaths = @($stagedPaths | Where-Object { $_ -match '(?i)(^|/)private_raw(/|$)|\.(pem|key|p12|pfx)$|(^|/)(id_rsa|id_ed25519)(\.|$)' })
$reviewLines = @(
    "local_time=$(Get-Date -Format 'o')",
    "staged_paths=$($stagedPaths.Count)",
    "private_or_key_paths=$($privatePaths.Count)",
    "binary_numstat_entries=$($binaryNumstat.Count)",
    "merge_head=$(git -C $repoRoot rev-parse MERGE_HEAD)",
    "feature_head=$(git -C $repoRoot rev-parse origin/m10-spool-v2-reclaim)",
    '--- name-status ---'
) + $nameStatus
Write-Utf8 -Path $reviewPath -Lines $reviewLines

$started = Get-Date -Format 'o'
$savedPreference = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
$diffOutput = @(git -C $repoRoot diff --cached --check 2>&1 | ForEach-Object { $_.ToString() })
$diffExit = $LASTEXITCODE
$ErrorActionPreference = $savedPreference
Write-Utf8 -Path $diffPath -Lines (@(
    'command=git diff --cached --check',
    "started_local=$started",
    "finished_local=$(Get-Date -Format 'o')",
    "exit_code=$diffExit",
    '--- output ---'
) + $(if ($diffOutput.Count -eq 0) { @('(empty)') } else { $diffOutput }))

$analyzerPass = (Select-String -LiteralPath (Join-Path $artifactDir '02_analyzer_regression.txt') -Pattern '^exit_code=0$') -and
    (Select-String -LiteralPath (Join-Path $artifactDir '02_analyzer_regression.txt') -Pattern '^Ran 8 tests') -and
    (Select-String -LiteralPath (Join-Path $artifactDir '02_analyzer_regression.txt') -Pattern '^OK$')
$importedManifestPass = Select-String -LiteralPath (Join-Path $artifactDir '03_merged_manifest_check.txt') -Pattern '^result=PASS$'
$overallPass = [bool]$analyzerPass -and [bool]$importedManifestPass -and $scanPass -and $diffExit -eq 0 -and $privatePaths.Count -eq 0 -and $binaryNumstat.Count -eq 0
$report = @(
    '# M10 Windows merge review — final result',
    '',
    "- Run ID: $(Split-Path -Leaf $artifactDir)",
    "- Local time: $(Get-Date -Format 'o')",
    "- Windows analyzer regression, 8/8: **$(if ($analyzerPass) { 'PASS' } else { 'FAIL' })**",
    "- Nine imported/final manifests, 158 entries: **$(if ($importedManifestPass) { 'PASS' } else { 'FAIL' })**",
    "- Final staged sensitive scan: **$(if ($scanPass) { 'PASS' } else { 'FAIL' })**",
    "- Staged binary/private-path audit: **$(if ($privatePaths.Count -eq 0 -and $binaryNumstat.Count -eq 0) { 'PASS' } else { 'FAIL' })**",
    "- git diff --cached --check: **$(if ($diffExit -eq 0) { 'PASS' } else { 'FAIL' })**",
    "- Overall: **$(if ($overallPass) { 'PASS' } else { 'FAIL' })**",
    '',
    '## Finding disposition',
    '',
    '- The broad first scan was retained as FAIL. Its four matches were reviewed exactly: two C symbol/null-check lines and two fixed unit-test fixture values. None is a real credential, and none is newly introduced by the merge.',
    '- Final policy scan retained all prohibited-data rules and accepted only those four exact non-secret lines.',
    '',
    '## Scope boundary',
    '',
    '- Board, CAN, Broker, STM32, binary deployment and long-duration tests: **NOT RUN**.',
    '- M10 remains **NOT MET** and M11 was not started.',
    '- Two non-final PowerShell runner failures are preserved in the artifact and produced no test result.'
)
Write-Utf8 -Path $reportPath -Lines $report

$files = Get-ChildItem -LiteralPath $artifactDir -File | Where-Object {
    $_.Name -notin @('artifact_manifest.sha256', 'artifact_manifest_check.txt')
} | Sort-Object Name
$manifest = foreach ($file in $files) {
    $hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    "$hash  $($file.Name)"
}
$manifest | Out-File -LiteralPath $manifestPath -Encoding ascii

$manifestFailures = @()
$checked = 0
foreach ($line in Get-Content -LiteralPath $manifestPath) {
    if ($line -notmatch '^([0-9a-f]{64})  (.+)$') {
        $manifestFailures += "malformed=$line"
        continue
    }
    $actual = (Get-FileHash -LiteralPath (Join-Path $artifactDir $Matches[2]) -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actual -ne $Matches[1]) {
        $manifestFailures += "mismatch=$($Matches[2])"
    }
    $checked++
}
(@(
    "result=$(if ($manifestFailures.Count -eq 0) { 'PASS' } else { 'FAIL' })",
    "files_checked=$checked"
) + $manifestFailures) | Out-File -LiteralPath $manifestCheckPath -Encoding ascii

Write-Output "sensitive_scan=$(if ($scanPass) { 'PASS' } else { 'FAIL' }) reviewed=$($reviewed.Count) actionable=$($actionable.Count)"
Write-Output "staged_review=$(if ($privatePaths.Count -eq 0 -and $binaryNumstat.Count -eq 0) { 'PASS' } else { 'FAIL' }) paths=$($stagedPaths.Count)"
Write-Output "diff_check=$(if ($diffExit -eq 0) { 'PASS' } else { 'FAIL' })"
Write-Output "artifact_manifest=$(if ($manifestFailures.Count -eq 0) { 'PASS' } else { 'FAIL' }) files=$checked"
Write-Output "overall=$(if ($overallPass -and $manifestFailures.Count -eq 0) { 'PASS' } else { 'FAIL' })"

exit $(if ($overallPass -and $manifestFailures.Count -eq 0) { 0 } else { 1 })
