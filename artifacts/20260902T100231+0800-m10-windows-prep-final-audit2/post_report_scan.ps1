$ErrorActionPreference = 'Stop'
$artifactDir = $PSScriptRoot
$repoRoot = (Resolve-Path (Join-Path $artifactDir '..\..')).Path
$output = Join-Path $artifactDir 'sensitive_scan_v2.txt'
if (Test-Path -LiteralPath $output) { throw "Refusing to overwrite $output" }
$files = Get-ChildItem -LiteralPath $artifactDir -File | Where-Object {
    $_.Name -notin @('sensitive_scan.txt', 'sensitive_scan_v2.txt', 'artifact_manifest.sha256', 'artifact_manifest_check.txt')
}
$patterns = @(
    '-----BEGIN (RSA |OPENSSH |EC )?PRIVATE KEY-----',
    '(?i)(password|passwd|secret|token|api[_-]?key)\s*[:=]\s*\S+',
    '(?<![0-9])(?:10|169\.254|172\.(?:1[6-9]|2[0-9]|3[01])|192\.168)(?:\.[0-9]{1,3}){2}(?![0-9])'
)
$findings = @()
foreach ($file in $files) {
    foreach ($pattern in $patterns) {
        if (Select-String -LiteralPath $file.FullName -Pattern $pattern -AllMatches -ErrorAction SilentlyContinue) {
            $findings += "file=$($file.Name) pattern=$pattern"
        }
    }
}
if ($findings.Count -ne 0) {
    @('result=FAIL') + $findings | Out-File -LiteralPath $output -Encoding utf8
    throw 'Post-report sensitive scan failed.'
}
@('result=PASS', "files_scanned=$($files.Count)") | Out-File -LiteralPath $output -Encoding utf8
Write-Output 'post_report_sensitive_scan=PASS'
