$ErrorActionPreference = 'Stop'
$artifactDir = $PSScriptRoot
$manifestPath = Join-Path $artifactDir 'artifact_manifest.sha256'
$checkPath = Join-Path $artifactDir 'artifact_manifest_check.txt'
if ((Test-Path -LiteralPath $manifestPath) -or (Test-Path -LiteralPath $checkPath)) {
    throw 'Refusing to overwrite final-audit manifest evidence.'
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
foreach ($line in Get-Content -LiteralPath $manifestPath) {
    if ($line -notmatch '^([0-9a-f]{64})  (.+)$') {
        $failures += "malformed=$line"
        continue
    }
    $actual = (Get-FileHash -LiteralPath (Join-Path $artifactDir $Matches[2]) -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actual -ne $Matches[1]) { $failures += "mismatch=$($Matches[2])" }
}
if ($failures.Count -ne 0) {
    @('result=FAIL') + $failures | Out-File -LiteralPath $checkPath -Encoding ascii
    throw 'Final-audit manifest verification failed.'
}
'result=PASS' | Out-File -LiteralPath $checkPath -Encoding ascii
Write-Output "manifest_files=$($files.Count)"
Write-Output 'manifest_check=PASS'
