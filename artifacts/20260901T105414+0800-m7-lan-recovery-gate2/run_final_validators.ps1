$ErrorActionPreference = 'Stop'

$runRoot = $PSScriptRoot
$phase1 = Join-Path $runRoot 'subscriber-phase1.jsonl'
$phase2 = Join-Path $runRoot 'subscriber-phase2.jsonl'
$combined = Join-Path $runRoot 'combined.jsonl'
$repoRoot = (Resolve-Path (Join-Path $runRoot '../../..')).Path

if (Test-Path -LiteralPath $combined) { throw 'Refusing to overwrite combined.jsonl' }
foreach ($source in @($phase1, $phase2)) {
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) { throw "Missing source: $source" }
    $bytes = [System.IO.File]::ReadAllBytes($source)
    if ($bytes.Length -eq 0) { throw "Empty source: $source" }
    if ($bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF) { throw "UTF-8 BOM is not allowed: $source" }
    if ($bytes[$bytes.Length - 1] -ne 0x0A) { throw "Source does not end with LF: $source" }
}

$destination = [System.IO.File]::Open($combined, [System.IO.FileMode]::CreateNew, [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
try {
    foreach ($source in @($phase1, $phase2)) {
        $input = [System.IO.File]::Open($source, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::Read)
        try { $input.CopyTo($destination) } finally { $input.Dispose() }
    }
    $destination.Flush($true)
} finally {
    $destination.Dispose()
}

@(
    'concatenation_order=1 subscriber-phase1.jsonl'
    'concatenation_order=2 subscriber-phase2.jsonl'
    "phase1_bytes=$((Get-Item -LiteralPath $phase1).Length)"
    "phase2_bytes=$((Get-Item -LiteralPath $phase2).Length)"
    "combined_bytes=$((Get-Item -LiteralPath $combined).Length)"
) | Set-Content -LiteralPath (Join-Path $runRoot 'combined_sources.txt') -Encoding ascii
Get-FileHash -Algorithm SHA256 -LiteralPath $phase1, $phase2, $combined | ForEach-Object { "$($_.Hash.ToLowerInvariant())  $($_.Path.Substring($runRoot.Length + 1))" } | Set-Content -LiteralPath (Join-Path $runRoot 'subscriber_sha256.txt') -Encoding ascii

$testOut = Join-Path $runRoot 'test_validate_m7_recovery.stdout.log'
$testErr = Join-Path $runRoot 'test_validate_m7_recovery.stderr.log'
$test = Start-Process -FilePath 'py' -WorkingDirectory $repoRoot -ArgumentList @('-3', '-B', 'tools/mqtt/test_validate_m7_recovery.py') -NoNewWindow -Wait -PassThru -RedirectStandardOutput $testOut -RedirectStandardError $testErr
"test_exit=$($test.ExitCode)" | Set-Content -LiteralPath (Join-Path $runRoot 'test_validate_m7_recovery_exit.txt') -Encoding ascii
if ($test.ExitCode -ne 0) { throw "M7 validator self-test failed with exit $($test.ExitCode)" }

$validatorOut = Join-Path $runRoot 'validate_m7_recovery.stdout.log'
$validatorErr = Join-Path $runRoot 'validate_m7_recovery.stderr.log'
$validator = Start-Process -FilePath 'py' -WorkingDirectory $repoRoot -ArgumentList @(
    '-3', '-B', 'tools/mqtt/validate_m7_recovery.py',
    '--input', $combined,
    '--device-id', 'm7-gateway-20260901-105414',
    '--expected-first-seq', '1',
    '--expected-last-seq', '35644',
    '--require-raw-duplicates'
) -NoNewWindow -Wait -PassThru -RedirectStandardOutput $validatorOut -RedirectStandardError $validatorErr
"validator_exit=$($validator.ExitCode)" | Set-Content -LiteralPath (Join-Path $runRoot 'validate_m7_recovery_exit.txt') -Encoding ascii
if ($validator.ExitCode -ne 0) { throw "M7 captured-data validator failed with exit $($validator.ExitCode)" }

Write-Output 'M7_FINAL_VALIDATORS_PASS'
