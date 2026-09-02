[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$artifactDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$analyzerPath = Join-Path $artifactDir '12_analyzer_tests_clean_capture.txt'
$mosquittoPath = Join-Path $artifactDir '13_mosquitto_versions.txt'
$exitPath = Join-Path $artifactDir 'supplement_exit.txt'

foreach ($path in @($analyzerPath, $mosquittoPath, $exitPath)) {
    if (Test-Path -LiteralPath $path) {
        throw "Refusing to overwrite existing evidence: $path"
    }
}

function Invoke-CmdCapture {
    param(
        [Parameter(Mandatory = $true)][string]$CommandText,
        [Parameter(Mandatory = $true)][string]$OutputPath
    )

    $started = Get-Date -Format 'o'
    $lines = @(& cmd.exe /d /c $CommandText | ForEach-Object { $_.ToString() })
    $exitCode = $LASTEXITCODE
    @(
        "command=$CommandText",
        "started_local=$started",
        "finished_local=$(Get-Date -Format 'o')",
        "exit_code=$exitCode",
        '--- output ---'
    ) + $lines | Out-File -LiteralPath $OutputPath -Encoding utf8
    return @{ ExitCode = [int]$exitCode; Lines = $lines }
}

$analyzer = Invoke-CmdCapture `
    -CommandText 'python tools\protocol\test_analyze_m10_candump.py -v 2>&1' `
    -OutputPath $analyzerPath

$mosquittoStarted = Get-Date -Format 'o'
$brokerHelp = @(& cmd.exe /d /c 'E:\mosquitto\mosquitto.exe -h 2>&1' | ForEach-Object { $_.ToString() })
$brokerHelpExit = $LASTEXITCODE
$pubHelp = @(& cmd.exe /d /c 'E:\mosquitto\mosquitto_pub.exe --help 2>&1' | ForEach-Object { $_.ToString() })
$pubHelpExit = $LASTEXITCODE
@(
    'command=E:\mosquitto\mosquitto.exe -h; E:\mosquitto\mosquitto_pub.exe --help',
    "started_local=$mosquittoStarted",
    "finished_local=$(Get-Date -Format 'o')",
    "broker_help_exit_code=$brokerHelpExit",
    "pub_help_exit_code=$pubHelpExit",
    'broker_started=NO (help/version mode only)',
    '--- mosquitto broker help output ---'
) + $brokerHelp + @(
    '--- mosquitto_pub help output ---'
) + $pubHelp | Out-File -LiteralPath $mosquittoPath -Encoding utf8

$analyzerText = $analyzer.Lines -join "`n"
$analyzerPass = (
    $analyzer.ExitCode -eq 0 -and
    $analyzerText -match 'test_extended_target_id_is_rejected.*ok' -and
    $analyzerText -match 'Ran 8 tests' -and
    $analyzerText -match '(?m)^OK\s*$'
)
$versionPass = (
    $brokerHelpExit -eq 0 -and
    $pubHelpExit -eq 0 -and
    (($brokerHelp + $pubHelp) -join "`n") -match 'mosquitto version'
)
$overallExit = if ($analyzerPass -and $versionPass) { 0 } else { 1 }
@(
    "local_time=$(Get-Date -Format 'o')",
    "clean_analyzer_capture=$(if ($analyzerPass) { 'PASS' } else { 'FAIL' })",
    "mosquitto_version_capture=$(if ($versionPass) { 'PASS' } else { 'FAIL' })",
    'broker_started=NO',
    "exit_code=$overallExit"
) | Out-File -LiteralPath $exitPath -Encoding utf8

exit $overallExit
