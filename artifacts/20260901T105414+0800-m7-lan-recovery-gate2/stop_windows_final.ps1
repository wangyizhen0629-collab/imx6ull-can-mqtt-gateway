$ErrorActionPreference = 'Stop'

$runRoot = $PSScriptRoot
$phaseRoot = Join-Path $runRoot 'windows-final-stop'
$privateRoot = Join-Path $runRoot 'private_raw/windows-final-stop'
$brokerPhaseRoot = Join-Path $runRoot 'windows-phase2-reconnect'
$brokerSignal = 'E:\mosquitto\mosquitto_signal.exe'
$brokerPid = 32812
$subscriberPid = 35988

New-Item -ItemType Directory -Path $phaseRoot, $privateRoot -ErrorAction Stop | Out-Null
$broker = Get-Process -Id $brokerPid -ErrorAction Stop
$subscriber = Get-Process -Id $subscriberPid -ErrorAction Stop
if ($broker.ProcessName -ne 'mosquitto') { throw "PID $brokerPid is not mosquitto" }
if ($subscriber.ProcessName -ne 'mosquitto_sub') { throw "PID $subscriberPid is not mosquitto_sub" }
@(
    "captured_at=$((Get-Date).ToString('o'))"
    "broker_pid=$brokerPid"
    "subscriber_pid=$subscriberPid"
    'tcp_before:'
    (netstat -ano -p tcp | Select-String ':18884' | ForEach-Object { $_.Line })
) | Set-Content -LiteralPath (Join-Path $phaseRoot 'preflight.txt') -Encoding utf8
@(
    "Stop-Process -Force -Id $subscriberPid"
    "E:\mosquitto\mosquitto_signal.exe -p $brokerPid shutdown"
) | Set-Content -LiteralPath (Join-Path $phaseRoot 'commands.txt') -Encoding ascii

Stop-Process -Force -Id $subscriberPid -ErrorAction Stop
$subscriberGone = $false
for ($i = 0; $i -lt 150 -and -not $subscriberGone; $i++) {
    Start-Sleep -Milliseconds 200
    $subscriberGone = -not [bool](Get-Process -Id $subscriberPid -ErrorAction SilentlyContinue)
}
$signal = Start-Process -FilePath $brokerSignal -ArgumentList @('-p', $brokerPid, 'shutdown') -NoNewWindow -Wait -PassThru -RedirectStandardOutput (Join-Path $privateRoot 'broker_signal.stdout.log') -RedirectStandardError (Join-Path $privateRoot 'broker_signal.stderr.log')
$signalExit = $signal.ExitCode
$brokerGone = $false
for ($i = 0; $i -lt 75 -and -not $brokerGone; $i++) {
    Start-Sleep -Milliseconds 200
    $brokerGone = -not [bool](Get-Process -Id $brokerPid -ErrorAction SilentlyContinue)
}
$listeners = @(Get-NetTCPConnection -LocalPort 18884 -State Listen -ErrorAction SilentlyContinue)
$tcp = @(netstat -ano -p tcp | Select-String ':18884' | ForEach-Object { $_.Line })
@(
    "stopped_at=$((Get-Date).ToString('o'))"
    'subscriber_stop_exit=0'
    "subscriber_gone=$($subscriberGone.ToString().ToLowerInvariant())"
    "broker_signal_exit=$signalExit"
    "broker_gone=$($brokerGone.ToString().ToLowerInvariant())"
    "listeners_after=$($listeners.Count)"
    "tcp_entries_after=$($tcp.Count)"
    $tcp
) | Set-Content -LiteralPath (Join-Path $phaseRoot 'result.txt') -Encoding utf8
if (-not $subscriberGone) { throw 'Final subscriber did not stop within 30 seconds' }
if ($signalExit -ne 0) { throw "Final Broker signal exited $signalExit" }
if (-not $brokerGone) { throw 'Final Broker did not stop within 15 seconds' }
if ($listeners.Count -ne 0) { throw 'Port 18884 still has a listener' }
Write-Output "M7_WINDOWS_FINAL_STOP_PASS broker_pid=$brokerPid subscriber_pid=$subscriberPid"
