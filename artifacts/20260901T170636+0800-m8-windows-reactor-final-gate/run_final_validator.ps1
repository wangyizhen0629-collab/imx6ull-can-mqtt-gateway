$ErrorActionPreference = 'Stop'
$RunDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = (Resolve-Path (Join-Path $RunDir '../..')).Path
$InputPath = Join-Path $RunDir 'subscriber.jsonl'
$Outputs = @(
    'validator_command.txt', 'validator_version.txt', 'validator_input_sha256.txt',
    'validator_selftest.stdout.txt', 'validator_selftest.stderr.txt', 'validator_selftest.exit.txt',
    'validator.stdout.txt', 'validator.stderr.txt', 'validator.exit.txt', 'final_validator_result.txt'
)
foreach ($Name in $Outputs) {
    if (Test-Path -LiteralPath (Join-Path $RunDir $Name)) { throw "refusing to overwrite validator evidence: $Name" }
}
if (-not (Test-Path -LiteralPath $InputPath -PathType Leaf)) { throw 'subscriber input is absent' }

$Version = & py -3 --version 2>&1 | Out-String
$VersionExit = $LASTEXITCODE
$ValidatorGitHash = (& git -C $RepoRoot hash-object tools/mqtt/validate_m7_recovery.py | Out-String).Trim()
$SelfTestGitHash = (& git -C $RepoRoot hash-object tools/mqtt/test_validate_m7_recovery.py | Out-String).Trim()
@(
    "python_version=$($Version.Trim())"
    "python_version_exit=$VersionExit"
    "validator_git_blob=$ValidatorGitHash"
    "validator_selftest_git_blob=$SelfTestGitHash"
) | Out-File -LiteralPath (Join-Path $RunDir 'validator_version.txt') -Encoding utf8
@(
    "input=subscriber.jsonl"
    "sha256=$((Get-FileHash -LiteralPath $InputPath -Algorithm SHA256).Hash.ToLowerInvariant())"
    "bytes=$((Get-Item -LiteralPath $InputPath).Length)"
    "lines=$(@(Get-Content -LiteralPath $InputPath).Count)"
) | Out-File -LiteralPath (Join-Path $RunDir 'validator_input_sha256.txt') -Encoding utf8
@(
    'selftest=py -3 -B tools/mqtt/test_validate_m7_recovery.py'
    'validator=py -3 -B tools/mqtt/validate_m7_recovery.py --input artifacts/20260901T170636+0800-m8-windows-reactor-async-gate/subscriber.jsonl --device-id m8-gateway-20260901-170636 --expected-first-seq 1 --expected-last-seq 27434 --require-raw-duplicates'
) | Out-File -LiteralPath (Join-Path $RunDir 'validator_command.txt') -Encoding utf8

$SelfOut = Join-Path $RunDir 'validator_selftest.stdout.txt'
$SelfErr = Join-Path $RunDir 'validator_selftest.stderr.txt'
$Self = Start-Process -FilePath 'py' -WorkingDirectory $RepoRoot -ArgumentList @(
    '-3', '-B', 'tools/mqtt/test_validate_m7_recovery.py'
) -NoNewWindow -Wait -PassThru -RedirectStandardOutput $SelfOut -RedirectStandardError $SelfErr
"exit_code=$($Self.ExitCode)" | Out-File -LiteralPath (Join-Path $RunDir 'validator_selftest.exit.txt') -Encoding ascii
if ($Self.ExitCode -ne 0) { throw "validator self-test failed: $($Self.ExitCode)" }

$ValidatorOut = Join-Path $RunDir 'validator.stdout.txt'
$ValidatorErr = Join-Path $RunDir 'validator.stderr.txt'
$Validator = Start-Process -FilePath 'py' -WorkingDirectory $RepoRoot -ArgumentList @(
    '-3', '-B', 'tools/mqtt/validate_m7_recovery.py',
    '--input', $InputPath,
    '--device-id', 'm8-gateway-20260901-170636',
    '--expected-first-seq', '1',
    '--expected-last-seq', '27434',
    '--require-raw-duplicates'
) -NoNewWindow -Wait -PassThru -RedirectStandardOutput $ValidatorOut -RedirectStandardError $ValidatorErr
"exit_code=$($Validator.ExitCode)" | Out-File -LiteralPath (Join-Path $RunDir 'validator.exit.txt') -Encoding ascii
if ($Validator.ExitCode -ne 0) { throw "captured-data validator failed: $($Validator.ExitCode)" }
$Result = Get-Content -LiteralPath $ValidatorOut -Raw | ConvertFrom-Json
if ($Result.status -ne 'PASS' -or $Result.missing_gateway_seq -ne 0 -or
    $Result.effective_duplicate_records -ne 0 -or $Result.raw_duplicate_records -le 0) {
    throw 'validator output does not satisfy the M8 gate'
}
@(
    'result=PASS'
    "selftest_exit=$($Self.ExitCode)"
    "validator_exit=$($Validator.ExitCode)"
    "raw_batches=$($Result.raw_batches)"
    "raw_records=$($Result.raw_records)"
    "raw_duplicate_records=$($Result.raw_duplicate_records)"
    "unique_batch_seq=$($Result.unique_batch_seq)"
    "unique_gateway_seq=$($Result.unique_gateway_seq)"
    "first_gateway_seq=$($Result.first_gateway_seq)"
    "last_gateway_seq=$($Result.last_gateway_seq)"
    "missing_gateway_seq=$($Result.missing_gateway_seq)"
    "effective_duplicate_records=$($Result.effective_duplicate_records)"
    "input_sha256=$((Get-FileHash -LiteralPath $InputPath -Algorithm SHA256).Hash.ToLowerInvariant())"
) | Out-File -LiteralPath (Join-Path $RunDir 'final_validator_result.txt') -Encoding utf8
Get-Content -LiteralPath (Join-Path $RunDir 'final_validator_result.txt')
