$ErrorActionPreference = 'Stop'

$runRoot = $PSScriptRoot
$phaseRoot = Join-Path $runRoot 'windows-phase2-reconnect'
$privateRoot = Join-Path $runRoot 'private_raw/windows-phase2-reconnect'
$sourcePrivateConfig = Join-Path $runRoot 'private_raw/windows-phase2/mosquitto.conf'
$mosquitto = 'E:\mosquitto\mosquitto.exe'
$port = 18884
$subscriberPid = 35988

New-Item -ItemType Directory -Path $phaseRoot, $privateRoot -ErrorAction Stop | Out-Null
if (-not (Test-Path -LiteralPath $sourcePrivateConfig -PathType Leaf)) { throw 'Phase2 private Broker config is unavailable' }
if (Get-NetTCPConnection -LocalPort $port -State Listen -ErrorAction SilentlyContinue) { throw "Port $port already has a listener" }
$subscriber = Get-Process -Id $subscriberPid -ErrorAction Stop
if ($subscriber.ProcessName -ne 'mosquitto_sub') { throw "PID $subscriberPid is not mosquitto_sub" }

$privateConfig = Join-Path $privateRoot 'mosquitto.conf'
Copy-Item -LiteralPath $sourcePrivateConfig -Destination $privateConfig -ErrorAction Stop
@(
    "listener $port <REDACTED_LAN_ADDRESS>"
    'allow_anonymous true'
    'persistence false'
    'connection_messages true'
    'log_type all'
    'log_dest stdout'
) | Set-Content -LiteralPath (Join-Path $phaseRoot 'mosquitto.conf.redacted') -Encoding ascii
$configTest = Start-Process -FilePath $mosquitto -ArgumentList @('-c', $privateConfig, '--test-config') -NoNewWindow -Wait -PassThru -RedirectStandardOutput (Join-Path $phaseRoot 'config_test.stdout.txt') -RedirectStandardError (Join-Path $phaseRoot 'config_test.stderr.txt')
"config_test_exit=$($configTest.ExitCode)" | Set-Content -LiteralPath (Join-Path $phaseRoot 'config_test_exit.txt') -Encoding ascii
if ($configTest.ExitCode -ne 0) { throw 'Mosquitto config test failed' }

$brokerStdout = Join-Path $privateRoot 'broker.stdout.log'
$brokerStderr = Join-Path $privateRoot 'broker.stderr.log'
$brokerWrapper = Join-Path $privateRoot 'broker_wrapper.cmd'
@"
@echo off
"$mosquitto" -c "$privateConfig" 1>"$brokerStdout" 2>"$brokerStderr"
set "broker_rc=%ERRORLEVEL%"
>"$(Join-Path $phaseRoot 'broker_exit.txt')" echo broker_exit=%broker_rc%
exit /b %broker_rc%
"@ | Set-Content -LiteralPath $brokerWrapper -Encoding ascii
$wrapper = Start-Process -FilePath 'cmd.exe' -ArgumentList @('/d', '/c', ('"' + $brokerWrapper + '"')) -WindowStyle Hidden -PassThru
"broker_wrapper_pid=$($wrapper.Id)" | Set-Content -LiteralPath (Join-Path $phaseRoot 'broker_wrapper_pid.txt') -Encoding ascii
$listener = $null
for ($i = 0; $i -lt 50 -and -not $listener; $i++) {
    Start-Sleep -Milliseconds 100
    $listener = Get-NetTCPConnection -LocalPort $port -State Listen -ErrorAction SilentlyContinue | Select-Object -First 1
}
if (-not $listener) { throw 'Reconnect Broker listener did not become ready' }
$brokerPid = [int]$listener.OwningProcess
$brokerProcess = Get-Process -Id $brokerPid -ErrorAction Stop
if ($brokerProcess.ProcessName -ne 'mosquitto') { throw 'Listener owner is not mosquitto' }
"broker_pid=$brokerPid" | Set-Content -LiteralPath (Join-Path $phaseRoot 'broker_pid.txt') -Encoding ascii

$subscriberConnected = $false
for ($i = 0; $i -lt 100 -and -not $subscriberConnected; $i++) {
    Start-Sleep -Milliseconds 200
    if (-not (Get-Process -Id $subscriberPid -ErrorAction SilentlyContinue)) { throw 'Existing subscriber exited before reconnect' }
    $subscriberConnected = [bool](Get-NetTCPConnection -OwningProcess $subscriberPid -RemotePort $port -State Established -ErrorAction SilentlyContinue)
}
@(
    'broker_command=mosquitto.exe -c <PRIVATE_CONFIG>'
    "subscriber_start_command=NOT RUN; retained existing PID $subscriberPid"
    "broker_pid=$brokerPid"
    "subscriber_pid=$subscriberPid"
    "subscriber_reconnected=$($subscriberConnected.ToString().ToLowerInvariant())"
    "ready_at=$((Get-Date).ToString('o'))"
) | Set-Content -LiteralPath (Join-Path $phaseRoot 'ready.txt') -Encoding utf8
if (-not $subscriberConnected) { throw 'Existing subscriber did not reconnect within 20 seconds' }
Write-Output "M7_WINDOWS_PHASE2_RECONNECT_BROKER_PASS broker_pid=$brokerPid subscriber_pid=$subscriberPid"
