[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$artifactDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path (Join-Path $artifactDir '..\..')).Path

$outputs = @(
    '02_analyzer_regression.txt',
    '03_merged_manifest_check.txt',
    '04_sensitive_scan.txt',
    '05_staged_name_status.txt',
    '06_diff_check.txt',
    'test_report.md'
)
foreach ($name in $outputs) {
    $path = Join-Path $artifactDir $name
    if (Test-Path -LiteralPath $path) {
        throw "Refusing to overwrite existing evidence: $path"
    }
}

function Write-Utf8 {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][object[]]$Lines
    )
    $Lines | Out-File -LiteralPath $Path -Encoding utf8
}

function Invoke-NativeCapture {
    param(
        [Parameter(Mandatory = $true)][string]$CommandText,
        [Parameter(Mandatory = $true)][scriptblock]$Action,
        [Parameter(Mandatory = $true)][string]$OutputName
    )
    $started = Get-Date -Format 'o'
    $savedPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    $lines = @(& $Action 2>&1 | ForEach-Object { $_.ToString() })
    $exitCode = $LASTEXITCODE
    $ErrorActionPreference = $savedPreference
    Write-Utf8 -Path (Join-Path $artifactDir $OutputName) -Lines (@(
        "command=$CommandText",
        "started_local=$started",
        "finished_local=$(Get-Date -Format 'o')",
        "exit_code=$exitCode",
        '--- output ---'
    ) + $(if ($lines.Count -eq 0) { @('(empty)') } else { $lines }))
    return @{ ExitCode = [int]$exitCode; Lines = $lines }
}

$context = @(
    "run_id=$(Split-Path -Leaf $artifactDir)",
    "local_time=$(Get-Date -Format 'o')",
    "repo_root=$repoRoot",
    "branch=$(git -C $repoRoot branch --show-current)",
    "head=$(git -C $repoRoot rev-parse HEAD)",
    "origin_master=$(git -C $repoRoot rev-parse origin/master)",
    "merge_head=$(git -C $repoRoot rev-parse MERGE_HEAD)",
    "feature_head=$(git -C $repoRoot rev-parse origin/m10-spool-v2-reclaim)",
    'board_operated=NO',
    'can_operated=NO',
    'broker_operated=NO',
    'stm32_operated=NO',
    'long_test_started=NO'
)
$contextPath = Join-Path $artifactDir '01_context.txt'
if (-not (Test-Path -LiteralPath $contextPath)) {
    Write-Utf8 -Path $contextPath -Lines $context
}

$analyzer = Invoke-NativeCapture `
    -CommandText 'python -B tools/protocol/test_analyze_m10_candump.py -v' `
    -Action { python -B 'tools/protocol/test_analyze_m10_candump.py' -v } `
    -OutputName '02_analyzer_regression.txt'
$analyzerText = $analyzer.Lines -join "`n"
$analyzerPass = (
    $analyzer.ExitCode -eq 0 -and
    $analyzerText -match 'Ran 8 tests' -and
    $analyzerText -match '(?m)^OK\s*$'
)

$manifestPaths = @(
    'artifacts/20260902T104013p0800-m10-windows-preflight/artifact_manifest.sha256',
    'artifacts/20260902T121148+0800-m10-spool-v2-source-audit/artifact_manifest.v2.sha256',
    'artifacts/20260902T121149+0800-m10-spool-v2-host-final/artifact_manifest.v2.sha256',
    'artifacts/20260902T121150+0800-m10-spool-v2-asan-ubsan/artifact_manifest.sha256',
    'artifacts/20260902T121151+0800-m10-spool-v2-arm-relwithdebinfo/artifact_manifest.v2.sha256',
    'artifacts/20260902T133021+0800-m10-spool-v2-recovery-source-audit/artifact_manifest.sha256',
    'artifacts/20260902T133022+0800-m10-spool-v2-recovery-host-final/artifact_manifest.sha256',
    'artifacts/20260902T133023+0800-m10-spool-v2-recovery-asan-ubsan/artifact_manifest.sha256',
    'artifacts/20260902T133024+0800-m10-spool-v2-recovery-arm-relwithdebinfo/artifact_manifest.sha256'
)
$manifestLines = @("started_local=$(Get-Date -Format 'o')")
$manifestBad = 0
$manifestEntries = 0
foreach ($relativeManifest in $manifestPaths) {
    $manifest = Join-Path $repoRoot $relativeManifest
    $base = Split-Path -Parent $manifest
    $manifestChecked = 0
    $manifestFailures = @()
    foreach ($line in Get-Content -LiteralPath $manifest) {
        if ($line -notmatch '^([0-9a-f]{64})  (.+)$') {
            $manifestFailures += "malformed=$line"
            continue
        }
        $expected = $Matches[1]
        $entry = $Matches[2] -replace '^\.[\\/]', ''
        $entryPath = Join-Path $base $entry
        if (-not (Test-Path -LiteralPath $entryPath -PathType Leaf)) {
            $manifestFailures += "missing=$entry"
            continue
        }
        $actual = (Get-FileHash -LiteralPath $entryPath -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($actual -ne $expected) {
            $manifestFailures += "mismatch=$entry expected=$expected actual=$actual"
        }
        $manifestChecked++
        $manifestEntries++
    }
    if ($manifestFailures.Count -eq 0) {
        $manifestLines += "manifest=$relativeManifest result=PASS entries=$manifestChecked"
    }
    else {
        $manifestBad += $manifestFailures.Count
        $manifestLines += "manifest=$relativeManifest result=FAIL entries=$manifestChecked"
        $manifestLines += $manifestFailures
    }
}
$manifestLines += @(
    "finished_local=$(Get-Date -Format 'o')",
    "manifests_checked=$($manifestPaths.Count)",
    "entries_checked=$manifestEntries",
    "failures=$manifestBad",
    "result=$(if ($manifestBad -eq 0) { 'PASS' } else { 'FAIL' })"
)
Write-Utf8 -Path (Join-Path $artifactDir '03_merged_manifest_check.txt') -Lines $manifestLines
$manifestPass = $manifestBad -eq 0

$stagedPaths = @(git -C $repoRoot diff --cached --name-only --diff-filter=ACMR)
$sensitiveLines = @(
    "started_local=$(Get-Date -Format 'o')",
    'scope=staged tracked files before this review artifact is staged',
    'patterns=private-key marker, private/link-local IPv4, non-redacted credential assignment, forbidden filename'
)
$sensitiveFindings = @()
$privateKeyPattern = '-----BEGIN (RSA |OPENSSH |EC )?PRIVATE KEY-----'
$privateIpPattern = '(?<![0-9])(?:10(?:\.[0-9]{1,3}){3}|192\.168(?:\.[0-9]{1,3}){2}|172\.(?:1[6-9]|2[0-9]|3[01])(?:\.[0-9]{1,3}){2}|169\.254(?:\.[0-9]{1,3}){2})(?![0-9])'
$credentialPattern = '(?i)(password|passwd|secret|token|api[_-]?key)\s*[:=]\s*["'']?([^\s"''<>{}\[\]]+)'
foreach ($relativePath in $stagedPaths) {
    if ($relativePath -match '(?i)(^|/)(private_raw)(/|$)|(^|/)(id_rsa|id_ed25519)(\.|$)|\.(pem|key|p12|pfx)$') {
        $sensitiveFindings += "forbidden_filename=$relativePath"
    }
    $content = @(git -C $repoRoot show ":$relativePath" 2>$null)
    $lineNumber = 0
    foreach ($line in $content) {
        $lineNumber++
        if ($line -match $privateKeyPattern) {
            $sensitiveFindings += "private_key_marker=$relativePath`:$lineNumber"
        }
        if ($line -match $privateIpPattern) {
            $sensitiveFindings += "private_ip=$relativePath`:$lineNumber value=$($Matches[0])"
        }
        if ($line -match $credentialPattern) {
            $value = $Matches[2]
            if ($value -notmatch '^(?i:redacted|masked|changeme|none|null|not_run|not-run|n/a)$') {
                $sensitiveFindings += "credential_assignment=$relativePath`:$lineNumber key=$($Matches[1])"
            }
        }
    }
}
$binaryNumstat = @(git -C $repoRoot diff --cached --numstat | Where-Object { $_ -match '^-\s+-\s+' })
foreach ($line in $binaryNumstat) {
    $sensitiveFindings += "binary_staged=$line"
}
$sensitiveLines += @(
    "files_scanned=$($stagedPaths.Count)",
    "findings=$($sensitiveFindings.Count)"
)
if ($sensitiveFindings.Count -eq 0) {
    $sensitiveLines += 'result=PASS'
}
else {
    $sensitiveLines += 'result=FAIL'
    $sensitiveLines += $sensitiveFindings
}
$sensitiveLines += "finished_local=$(Get-Date -Format 'o')"
Write-Utf8 -Path (Join-Path $artifactDir '04_sensitive_scan.txt') -Lines $sensitiveLines
$sensitivePass = $sensitiveFindings.Count -eq 0

$nameStatus = Invoke-NativeCapture `
    -CommandText 'git diff --cached --name-status' `
    -Action { git -C $repoRoot diff --cached --name-status } `
    -OutputName '05_staged_name_status.txt'

$diffCheck = Invoke-NativeCapture `
    -CommandText 'git diff --cached --check' `
    -Action { git -C $repoRoot diff --cached --check } `
    -OutputName '06_diff_check.txt'
$diffPass = $diffCheck.ExitCode -eq 0

$overallPass = $analyzerPass -and $manifestPass -and $sensitivePass -and $diffPass -and $nameStatus.ExitCode -eq 0
$report = @(
    '# M10 Windows merge review',
    '',
    "- Run ID: $(Split-Path -Leaf $artifactDir)",
    "- Local time: $(Get-Date -Format 'o')",
    "- Windows analyzer regression (8 tests): **$(if ($analyzerPass) { 'PASS' } else { 'FAIL' })**",
    "- Imported/final artifact manifests ($($manifestPaths.Count), $manifestEntries entries): **$(if ($manifestPass) { 'PASS' } else { 'FAIL' })**",
    "- Staged sensitive-information scan ($($stagedPaths.Count) files): **$(if ($sensitivePass) { 'PASS' } else { 'FAIL' })**",
    "- Staged git diff --cached --check: **$(if ($diffPass) { 'PASS' } else { 'FAIL' })**",
    "- Overall: **$(if ($overallPass) { 'PASS' } else { 'FAIL' })**",
    '',
    '## Scope boundary',
    '',
    '- Board, CAN, Broker and STM32 operations: **NOT RUN** (outside the authorized merge-review scope).',
    '- Long-duration and real-hardware tests: **NOT RUN**.',
    '- M10 gate remains **NOT MET**; this review does not start M11.',
    '',
    '## Notes',
    '',
    '- The only merge conflict was `.gitattributes`; the Windows preflight rule and all eight feature-branch artifact rules were retained.',
    '- The analyzer was run with `-B` to avoid creating Python bytecode files.',
    '- The sensitive scan read the staged Git blobs, not historical untracked directories.',
    '- A first invocation stopped at PowerShell parse time, and a second stopped in output-function parameter binding before the analyzer began. Both runner failures are retained; no test result was claimed from either invocation.'
)
Write-Utf8 -Path (Join-Path $artifactDir 'test_report.md') -Lines $report

Write-Output "analyzer=$(if ($analyzerPass) { 'PASS' } else { 'FAIL' })"
Write-Output "merged_manifests=$(if ($manifestPass) { 'PASS' } else { 'FAIL' }) manifests=$($manifestPaths.Count) entries=$manifestEntries"
Write-Output "sensitive_scan=$(if ($sensitivePass) { 'PASS' } else { 'FAIL' }) files=$($stagedPaths.Count) findings=$($sensitiveFindings.Count)"
Write-Output "diff_check=$(if ($diffPass) { 'PASS' } else { 'FAIL' })"
Write-Output "overall=$(if ($overallPass) { 'PASS' } else { 'FAIL' })"

exit $(if ($overallPass) { 0 } else { 1 })
