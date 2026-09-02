$ErrorActionPreference = 'Stop'

$attemptDir = Resolve-Path (Join-Path $PSScriptRoot '..\20260902T094551+0800-m10-windows-profile-prep')
$scanPath = Join-Path $attemptDir 'sensitive_scan.txt'
$manifestPath = Join-Path $attemptDir 'artifact_manifest.sha256'
$checkPath = Join-Path $attemptDir 'artifact_manifest_check.txt'
foreach ($path in @($scanPath, $manifestPath, $checkPath)) {
    if (Test-Path -LiteralPath $path) {
        throw "Refusing to overwrite attempt-1 evidence: $path"
    }
}

$scanTargets = Get-ChildItem -LiteralPath $attemptDir -File
$patterns = @(
    '-----BEGIN (RSA |OPENSSH |EC )?PRIVATE KEY-----',
    '(?i)(password|passwd|secret|token|api[_-]?key)\s*[:=]\s*\S+',
    '(?<![0-9])(?:10|127|169\.254|172\.(?:1[6-9]|2[0-9]|3[01])|192\.168)(?:\.[0-9]{1,3}){2}(?![0-9])'
)
$findings = @()
foreach ($file in $scanTargets) {
    foreach ($pattern in $patterns) {
        if (Select-String -LiteralPath $file.FullName -Pattern $pattern -AllMatches -ErrorAction SilentlyContinue) {
            $findings += "file=$($file.Name) pattern=$pattern"
        }
    }
}
if ($findings.Count -ne 0) {
    @('result=FAIL') + $findings | Out-File -LiteralPath $scanPath -Encoding utf8
    throw 'Attempt-1 sensitive-information scan failed.'
}
@('result=PASS', "files_scanned=$($scanTargets.Count)") | Out-File -LiteralPath $scanPath -Encoding utf8

$manifestFiles = Get-ChildItem -LiteralPath $attemptDir -File | Where-Object {
    $_.Name -notin @('artifact_manifest.sha256', 'artifact_manifest_check.txt')
} | Sort-Object Name
$manifest = foreach ($file in $manifestFiles) {
    $hash = Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256
    "$($hash.Hash.ToLowerInvariant())  $($file.Name)"
}
$manifest | Out-File -LiteralPath $manifestPath -Encoding ascii

$failures = @()
foreach ($line in Get-Content -LiteralPath $manifestPath) {
    $null = $line -match '^([0-9a-f]{64})  (.+)$'
    $actual = (Get-FileHash -LiteralPath (Join-Path $attemptDir $Matches[2]) -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actual -ne $Matches[1]) { $failures += $Matches[2] }
}
if ($failures.Count -ne 0) {
    @('result=FAIL') + $failures | Out-File -LiteralPath $checkPath -Encoding ascii
    throw 'Attempt-1 manifest verification failed.'
}
'result=PASS' | Out-File -LiteralPath $checkPath -Encoding ascii
Write-Output 'attempt1_sensitive_scan=PASS'
Write-Output 'attempt1_manifest_check=PASS'
