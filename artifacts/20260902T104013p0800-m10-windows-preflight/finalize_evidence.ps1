[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$artifactDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path (Join-Path $artifactDir '..\..')).Path
$scanPath = Join-Path $artifactDir 'sensitive_scan.txt'
$diffPath = Join-Path $artifactDir 'git_diff_check.txt'
$manifestPath = Join-Path $artifactDir 'artifact_manifest.sha256'
$manifestCheckPath = Join-Path $artifactDir 'artifact_manifest_check.txt'

foreach ($path in @($scanPath, $diffPath, $manifestPath, $manifestCheckPath)) {
    if (Test-Path -LiteralPath $path) {
        throw "Refusing to overwrite existing final evidence: $path"
    }
}

$patterns = @(
    '-----BEGIN (RSA |OPENSSH |EC )?PRIVATE KEY-----',
    '(?i)(password|passwd|secret|token|api[_-]?key)\s*[:=]\s*\S+',
    '(?<![0-9])(?:10|169\.254|172\.(?:1[6-9]|2[0-9]|3[01])|192\.168)(?:\.[0-9]{1,3}){2}(?![0-9])'
)
$scanFiles = Get-ChildItem -LiteralPath $artifactDir -File | Where-Object {
    $_.Name -notin @('sensitive_scan.txt', 'artifact_manifest.sha256', 'artifact_manifest_check.txt')
}
$findings = @()
foreach ($file in $scanFiles) {
    foreach ($pattern in $patterns) {
        if (Select-String -LiteralPath $file.FullName -Pattern $pattern -AllMatches -ErrorAction SilentlyContinue) {
            $findings += "file=$($file.Name) pattern=$pattern"
        }
    }
}
if ($findings.Count -eq 0) {
    @(
        'result=PASS',
        "files_scanned=$($scanFiles.Count)",
        'scope=private-key markers, credential assignments, private/link-local IPv4 literals'
    ) | Out-File -LiteralPath $scanPath -Encoding utf8
}
else {
    @('result=FAIL') + $findings | Out-File -LiteralPath $scanPath -Encoding utf8
    throw 'Sensitive information scan failed.'
}

$started = Get-Date -Format 'o'
$savedErrorActionPreference = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
$diffOutput = @(git -C $repoRoot diff --check 2>&1 | ForEach-Object { $_.ToString() })
$diffExit = $LASTEXITCODE
$ErrorActionPreference = $savedErrorActionPreference
@(
    'command=git diff --check',
    "started_local=$started",
    "finished_local=$(Get-Date -Format 'o')",
    "exit_code=$diffExit",
    '--- output ---'
) + $(if ($diffOutput.Count -eq 0) { @('(empty)') } else { $diffOutput }) |
    Out-File -LiteralPath $diffPath -Encoding utf8
if ($diffExit -ne 0) {
    throw 'git diff --check failed.'
}

$files = Get-ChildItem -LiteralPath $artifactDir -File | Where-Object {
    $_.Name -notin @('artifact_manifest.sha256', 'artifact_manifest_check.txt')
} | Sort-Object Name
$manifest = foreach ($file in $files) {
    $hash = Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256
    "$($hash.Hash.ToLowerInvariant())  $($file.Name)"
}
$manifest | Out-File -LiteralPath $manifestPath -Encoding ascii

$failures = @()
$checked = 0
foreach ($line in Get-Content -LiteralPath $manifestPath) {
    if ($line -notmatch '^([0-9a-f]{64})  (.+)$') {
        $failures += "malformed=$line"
        continue
    }
    $actual = (Get-FileHash -LiteralPath (Join-Path $artifactDir $Matches[2]) -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actual -ne $Matches[1]) {
        $failures += "mismatch=$($Matches[2])"
    }
    $checked++
}
if ($failures.Count -eq 0) {
    @(
        'result=PASS',
        "files_checked=$checked"
    ) | Out-File -LiteralPath $manifestCheckPath -Encoding ascii
}
else {
    @('result=FAIL') + $failures | Out-File -LiteralPath $manifestCheckPath -Encoding ascii
    throw 'Artifact manifest verification failed.'
}

Write-Output 'sensitive_scan=PASS'
Write-Output 'git_diff_check=PASS'
Write-Output "manifest_files=$checked"
Write-Output 'manifest_check=PASS'
