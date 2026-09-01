$ErrorActionPreference = 'Stop'

$runRoot = $PSScriptRoot
$combined = Join-Path $runRoot 'combined.jsonl'
$repoRoot = (Resolve-Path (Join-Path $runRoot '../..')).Path
$expectedHash = '572cc25085bdb75a77acb56761090e302c192e6ca1e0e251fc9219a05bad0c38'
if (-not (Test-Path -LiteralPath $combined -PathType Leaf)) { throw 'combined.jsonl is absent' }
$actualHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $combined).Hash.ToLowerInvariant()
if ($actualHash -ne $expectedHash) { throw 'combined.jsonl changed after attempt 1' }
if (-not (Test-Path -LiteralPath (Join-Path $repoRoot 'tools/mqtt/test_validate_m7_recovery.py') -PathType Leaf)) { throw 'Corrected repository root is invalid' }

$testOut = Join-Path $runRoot 'test_validate_m7_recovery_attempt2.stdout.log'
$testErr = Join-Path $runRoot 'test_validate_m7_recovery_attempt2.stderr.log'
$test = Start-Process -FilePath 'py' -WorkingDirectory $repoRoot -ArgumentList @('-3', '-B', 'tools/mqtt/test_validate_m7_recovery.py') -NoNewWindow -Wait -PassThru -RedirectStandardOutput $testOut -RedirectStandardError $testErr
"test_exit=$($test.ExitCode)" | Set-Content -LiteralPath (Join-Path $runRoot 'test_validate_m7_recovery_attempt2_exit.txt') -Encoding ascii
if ($test.ExitCode -ne 0) { throw "M7 validator self-test failed with exit $($test.ExitCode)" }

$validatorOut = Join-Path $runRoot 'validate_m7_recovery_attempt2.stdout.log'
$validatorErr = Join-Path $runRoot 'validate_m7_recovery_attempt2.stderr.log'
$validator = Start-Process -FilePath 'py' -WorkingDirectory $repoRoot -ArgumentList @(
    '-3', '-B', 'tools/mqtt/validate_m7_recovery.py',
    '--input', $combined,
    '--device-id', 'm7-gateway-20260901-105414',
    '--expected-first-seq', '1',
    '--expected-last-seq', '35644',
    '--require-raw-duplicates'
) -NoNewWindow -Wait -PassThru -RedirectStandardOutput $validatorOut -RedirectStandardError $validatorErr
"validator_exit=$($validator.ExitCode)" | Set-Content -LiteralPath (Join-Path $runRoot 'validate_m7_recovery_attempt2_exit.txt') -Encoding ascii
if ($validator.ExitCode -ne 0) { throw "M7 captured-data validator failed with exit $($validator.ExitCode)" }
Write-Output 'M7_FINAL_VALIDATORS_ATTEMPT2_PASS'
