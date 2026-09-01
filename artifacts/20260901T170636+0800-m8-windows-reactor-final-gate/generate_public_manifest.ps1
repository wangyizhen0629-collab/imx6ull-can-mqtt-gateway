$ErrorActionPreference = 'Stop'
$RunDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$Manifest = Join-Path $RunDir 'artifact_manifest.sha256'
$Check = Join-Path $RunDir 'artifact_manifest_check.txt'
$Scan = Join-Path $RunDir 'sensitive_scan.txt'
foreach ($Path in @($Manifest, $Check, $Scan)) {
    if (Test-Path -LiteralPath $Path) { throw "refusing to overwrite final manifest evidence: $Path" }
}
$ExcludedRoots = @(
    (Join-Path $RunDir 'private_raw'),
    (Join-Path $RunDir 'board-evidence-redacted'),
    (Join-Path $RunDir 'broker-logs-redacted')
)
$ExcludedFiles = @(
    (Join-Path $RunDir 'board_evidence_redaction_trace.txt'),
    (Join-Path $RunDir 'broker_logs_redaction_trace.txt')
)
$Files = @(Get-ChildItem -LiteralPath $RunDir -Recurse -File | Where-Object {
    $Full = $_.FullName
    -not ($ExcludedRoots | Where-Object { $Full.StartsWith($_ + [IO.Path]::DirectorySeparatorChar) }) -and
    $Full -notin $ExcludedFiles -and $Full -notin @($Manifest, $Check, $Scan)
} | Sort-Object FullName)
if ($Files.Count -eq 0) { throw 'public manifest candidate list is empty' }

$PrivateIpv4 = '(?<![0-9])(?:10\.(?:[0-9]{1,3}\.){2}[0-9]{1,3}|192\.168\.(?:[0-9]{1,3}\.)[0-9]{1,3}|172\.(?:1[6-9]|2[0-9]|3[01])\.(?:[0-9]{1,3}\.)[0-9]{1,3})(?![0-9])'
$Findings = New-Object System.Collections.Generic.List[string]
foreach ($File in $Files) {
    $Content = [System.IO.File]::ReadAllText($File.FullName)
    if ([regex]::IsMatch($Content, $PrivateIpv4)) { $Findings.Add("RFC1918 $($File.FullName)") }
    if ($Content -match '-----BEGIN (?:RSA |OPENSSH |EC )?PRIVATE KEY-----') { $Findings.Add("PRIVATE_KEY $($File.FullName)") }
}
@(
    "scanned_at=$((Get-Date).ToString('o'))"
    "candidate_file_count=$($Files.Count)"
    'excluded=private_raw/**; board-evidence-redacted/**; broker-logs-redacted/**; v1 board/broker trace files'
    "rfc1918_or_private_key_findings=$($Findings.Count)"
    "result=$(if($Findings.Count -eq 0){'PASS'}else{'FAIL'})"
    $Findings
) | Out-File -LiteralPath $Scan -Encoding utf8
if ($Findings.Count -ne 0) { throw 'sensitive data found in public manifest candidates' }

$ManifestLines = New-Object System.Collections.Generic.List[string]
foreach ($File in $Files) {
    $Relative = $File.FullName.Substring($RunDir.Length + 1).Replace('\', '/')
    $Hash = (Get-FileHash -LiteralPath $File.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    $ManifestLines.Add("$Hash  $Relative")
}
[System.IO.File]::WriteAllLines($Manifest, $ManifestLines, (New-Object System.Text.UTF8Encoding($false)))
$Mismatches = 0
foreach ($Line in $ManifestLines) {
    $Parts = $Line -split '  ', 2
    $Actual = (Get-FileHash -LiteralPath (Join-Path $RunDir $Parts[1]) -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($Actual -ne $Parts[0]) { $Mismatches++ }
}
@(
    "checked_at=$((Get-Date).ToString('o'))"
    "manifest_entries=$($ManifestLines.Count)"
    "mismatches=$Mismatches"
    "manifest_sha256=$((Get-FileHash -LiteralPath $Manifest -Algorithm SHA256).Hash.ToLowerInvariant())"
    "result=$(if($Mismatches -eq 0){'PASS'}else{'FAIL'})"
) | Out-File -LiteralPath $Check -Encoding utf8
if ($Mismatches -ne 0) { throw 'public artifact manifest verification failed' }
Get-Content -LiteralPath $Scan
Get-Content -LiteralPath $Check
