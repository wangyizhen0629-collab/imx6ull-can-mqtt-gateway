$ErrorActionPreference = 'Stop'

$runRoot = $PSScriptRoot
$phaseRoot = Join-Path $runRoot 'windows-phase1'
$privateRoot = Join-Path $runRoot 'private_raw/windows-phase1'
$previousPrivateConfig = Join-Path (Split-Path $runRoot -Parent) '20260901T103725+0800-m7-lan-recovery-gate/private_raw/windows-phase1/mosquitto.conf'
$mosquitto = 'E:\mosquitto\mosquitto.exe'
$mosquittoSub = 'E:\mosquitto\mosquitto_sub.exe'
$port = 18884
$topic = 'test/m7/20260901T105414/recovery'
$subscriberOutput = Join-Path $runRoot 'subscriber-phase1.jsonl'

New-Item -ItemType Directory -Path $phaseRoot, $privateRoot -ErrorAction Stop | Out-Null
if (-not (Test-Path -LiteralPath $previousPrivateConfig -PathType Leaf)) { throw 'Previous private Broker config is unavailable' }
if (-not (Test-Path -LiteralPath $mosquitto -PathType Leaf)) { throw 'mosquitto.exe is unavailable' }
if (-not (Test-Path -LiteralPath $mosquittoSub -PathType Leaf)) { throw 'mosquitto_sub.exe is unavailable' }
if (Get-NetTCPConnection -LocalPort $port -State Listen -ErrorAction SilentlyContinue) { throw "Port $port already has a listener" }

$previousText = Get-Content -LiteralPath $previousPrivateConfig
$listenerLine = $previousText | Where-Object { $_ -match '^listener\s+18884\s+(.+)$' } | Select-Object -First 1
if (-not $listenerLine -or $listenerLine -notmatch '^listener\s+18884\s+(.+)$') { throw 'Cannot recover the approved private listener address' }
$brokerHost = $Matches[1].Trim()
if ($brokerHost -notmatch '^\d{1,3}(\.\d{1,3}){3}$') { throw 'Recovered listener address is not IPv4' }

$privateConfig = Join-Path $privateRoot 'mosquitto.conf'
@(
    "listener $port $brokerHost"
    'allow_anonymous true'
    'persistence false'
    'connection_messages true'
    'log_type all'
    'log_dest stdout'
) | Set-Content -LiteralPath $privateConfig -Encoding ascii
@(
    "listener $port <REDACTED_LAN_ADDRESS>"
    'allow_anonymous true'
    'persistence false'
    'connection_messages true'
    'log_type all'
    'log_dest stdout'
) | Set-Content -LiteralPath (Join-Path $phaseRoot 'mosquitto.conf.redacted') -Encoding ascii

$configTestOut = Join-Path $phaseRoot 'config_test.stdout.txt'
$configTestErr = Join-Path $phaseRoot 'config_test.stderr.txt'
$configTest = Start-Process -FilePath $mosquitto -ArgumentList @('-c', $privateConfig, '-t') -NoNewWindow -Wait -PassThru -RedirectStandardOutput $configTestOut -RedirectStandardError $configTestErr
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

$brokerWrapperProcess = Start-Process -FilePath 'cmd.exe' -ArgumentList @('/d', '/c', ('"' + $brokerWrapper + '"')) -WindowStyle Hidden -PassThru
"broker_wrapper_pid=$($brokerWrapperProcess.Id)" | Set-Content -LiteralPath (Join-Path $phaseRoot 'broker_wrapper_pid.txt') -Encoding ascii
$listener = $null
for ($i = 0; $i -lt 50 -and -not $listener; $i++) {
    Start-Sleep -Milliseconds 100
    $listener = Get-NetTCPConnection -LocalPort $port -State Listen -ErrorAction SilentlyContinue | Select-Object -First 1
}
if (-not $listener) { throw 'Dedicated Broker listener did not become ready' }
$brokerPid = [int]$listener.OwningProcess
$brokerProcess = Get-Process -Id $brokerPid -ErrorAction Stop
if ($brokerProcess.ProcessName -ne 'mosquitto') { throw 'Listener owner is not mosquitto' }
"broker_pid=$brokerPid" | Set-Content -LiteralPath (Join-Path $phaseRoot 'broker_pid.txt') -Encoding ascii

$beforeSubscriber = @(Get-Process -Name 'mosquitto_sub' -ErrorAction SilentlyContinue | ForEach-Object Id)
$subscriberStderr = Join-Path $privateRoot 'subscriber.stderr.log'
$subscriberWrapper = Join-Path $privateRoot 'subscriber_wrapper.cmd'
@"
@echo off
"$mosquittoSub" -h $brokerHost -p $port -q 1 -t "$topic" -i "m7-win-sub-phase1-20260901T105414" -F "%%p" 1>"$subscriberOutput" 2>"$subscriberStderr"
set "subscriber_rc=%ERRORLEVEL%"
>"$(Join-Path $phaseRoot 'subscriber_exit.txt')" echo subscriber_exit=%subscriber_rc%
exit /b %subscriber_rc%
"@ | Set-Content -LiteralPath $subscriberWrapper -Encoding ascii
$subscriberWrapperProcess = Start-Process -FilePath 'cmd.exe' -ArgumentList @('/d', '/c', ('"' + $subscriberWrapper + '"')) -WindowStyle Hidden -PassThru
"subscriber_wrapper_pid=$($subscriberWrapperProcess.Id)" | Set-Content -LiteralPath (Join-Path $phaseRoot 'subscriber_wrapper_pid.txt') -Encoding ascii
$subscriberProcess = $null
for ($i = 0; $i -lt 50 -and -not $subscriberProcess; $i++) {
    Start-Sleep -Milliseconds 100
    $subscriberProcess = Get-Process -Name 'mosquitto_sub' -ErrorAction SilentlyContinue | Where-Object { $beforeSubscriber -notcontains $_.Id } | Select-Object -First 1
}
if (-not $subscriberProcess) { throw 'Dedicated subscriber did not start' }
$subscriberPid = $subscriberProcess.Id
"subscriber_pid=$subscriberPid" | Set-Content -LiteralPath (Join-Path $phaseRoot 'subscriber_pid.txt') -Encoding ascii

@(
    'broker_command=mosquitto.exe -c <PRIVATE_CONFIG>'
    "subscriber_command=mosquitto_sub.exe -h <REDACTED_LAN_ADDRESS> -p $port -q 1 -t $topic -i m7-win-sub-phase1-20260901T105414 -F %p"
) | Set-Content -LiteralPath (Join-Path $phaseRoot 'start_commands.redacted.txt') -Encoding ascii
@(
    'broker_listener=READY'
    'subscriber_process=READY'
    "broker_pid=$brokerPid"
    "subscriber_pid=$subscriberPid"
    "ready_at=$((Get-Date).ToString('o'))"
) | Set-Content -LiteralPath (Join-Path $phaseRoot 'ready.txt') -Encoding utf8

Write-Output "M7_WINDOWS_PHASE1_START_PASS broker_pid=$brokerPid subscriber_pid=$subscriberPid"
