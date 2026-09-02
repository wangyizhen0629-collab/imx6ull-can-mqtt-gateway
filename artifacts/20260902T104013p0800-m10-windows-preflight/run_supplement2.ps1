[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$artifactDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$outputPath = Join-Path $artifactDir '14_supplement_corrected_validation.txt'
$exitPath = Join-Path $artifactDir 'supplement2_exit.txt'
foreach ($path in @($outputPath, $exitPath)) {
    if (Test-Path -LiteralPath $path) {
        throw "Refusing to overwrite existing evidence: $path"
    }
}

$analyzer = Get-Content -LiteralPath (Join-Path $artifactDir '12_analyzer_tests_clean_capture.txt') -Raw
$mosquitto = Get-Content -LiteralPath (Join-Path $artifactDir '13_mosquitto_versions.txt') -Raw
$processes = @(Get-Process -Name 'mosquitto' -ErrorAction SilentlyContinue)

$checks = [ordered]@{
    clean_analyzer_exit_zero = ($analyzer -match '(?m)^exit_code=0\s*$')
    clean_analyzer_extended_id_pass = ($analyzer -match 'test_extended_target_id_is_rejected.*ok')
    clean_analyzer_ran_8_tests = ($analyzer -match 'Ran 8 tests')
    clean_analyzer_result_ok = ($analyzer -match '(?m)^OK\s*$')
    broker_help_exit_zero = ($mosquitto -match '(?m)^broker_help_exit_code=0\s*$')
    broker_version_present = ($mosquitto -match 'mosquitto version 2\.1\.2')
    pub_help_expected_exit_one = ($mosquitto -match '(?m)^pub_help_exit_code=1\s*$')
    pub_version_present = ($mosquitto -match 'mosquitto_pub version 2\.1\.2 running on libmosquitto 2\.1\.0')
}

$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add('command=corrected read-only validation of clean analyzer and Mosquitto help/version evidence')
$lines.Add("started_local=$(Get-Date -Format 'o')")
$lines.Add('correction=mosquitto_pub 2.1.2 --help prints version/help and returns 1; no host/topic/message was supplied, so no Broker connection was attempted')
$lines.Add("mosquitto_process_count_observed_after_help=$($processes.Count)")
if ($processes.Count -eq 0) {
    $lines.Add('mosquitto_process_observation=no local mosquitto broker process observed')
}
else {
    foreach ($process in $processes) {
        $lines.Add("mosquitto_process_observation=pre-existing-or-external-state pid=$($process.Id); no action taken")
    }
}

$failed = 0
foreach ($item in $checks.GetEnumerator()) {
    $result = if ($item.Value) { 'PASS' } else { 'FAIL' }
    if (-not $item.Value) { $failed++ }
    $lines.Add("check=$($item.Key) result=$result")
}
$overallExit = if ($failed -eq 0) { 0 } else { 1 }
$lines.Add("failed_checks=$failed")
$lines.Add("result=$(if ($overallExit -eq 0) { 'PASS' } else { 'FAIL' })")
$lines.Add("finished_local=$(Get-Date -Format 'o')")
$lines.Add("exit_code=$overallExit")
$lines | Out-File -LiteralPath $outputPath -Encoding utf8
@(
    "local_time=$(Get-Date -Format 'o')",
    "exit_code=$overallExit"
) | Out-File -LiteralPath $exitPath -Encoding utf8

exit $overallExit
