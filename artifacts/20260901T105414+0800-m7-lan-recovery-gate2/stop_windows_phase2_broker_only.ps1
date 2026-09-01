$ErrorActionPreference = 'Stop'

$runRoot = $PSScriptRoot
$phaseRoot = Join-Path $runRoot 'windows-phase2'
$privateRoot = Join-Path $runRoot 'private_raw/windows-phase2'
$brokerSignal = 'E:\mosquitto\mosquitto_signal.exe'

function Read-RecordedPid([string]$Path, [string]$Prefix) {
    $line = (Get-Content -LiteralPath $Path -Raw).Trim()
    if ($line -notmatch ('^' + [regex]::Escape($Prefix) + '=(\d+)$')) { throw "Invalid PID record: $Path" }
    return [int]$Matches[1]
}

$brokerPid = Read-RecordedPid (Join-Path $phaseRoot 'broker_pid.txt') 'broker_pid'
$subscriberPid = Read-RecordedPid (Join-Path $phaseRoot 'subscriber_pid.txt') 'subscriber_pid'
$broker = Get-Process -Id $brokerPid -ErrorAction Stop
$subscriber = Get-Process -Id $subscriberPid -ErrorAction Stop
if ($broker.ProcessName -ne 'mosquitto') { throw "PID $brokerPid is not mosquitto" }
if ($subscriber.ProcessName -ne 'mosquitto_sub') { throw "PID $subscriberPid is not mosquitto_sub" }

@(
    "captured_at=$((Get-Date).ToString('o'))"
    "broker_pid=$brokerPid"
    "subscriber_pid=$subscriberPid"
    'subscriber_stop_command=NOT RUN'
    'tcp_before:'
    (netstat -ano -p tcp | Select-String ':18884' | ForEach-Object { $_.Line })
) | Set-Content -LiteralPath (Join-Path $phaseRoot 'reconnect_outage_preflight.txt') -Encoding utf8
"E:\mosquitto\mosquitto_signal.exe -p $brokerPid shutdown" | Set-Content -LiteralPath (Join-Path $phaseRoot 'reconnect_outage_command.txt') -Encoding ascii
$signal = Start-Process -FilePath $brokerSignal -ArgumentList @('-p', $brokerPid, 'shutdown') -NoNewWindow -Wait -PassThru -RedirectStandardOutput (Join-Path $privateRoot 'reconnect_signal.stdout.log') -RedirectStandardError (Join-Path $privateRoot 'reconnect_signal.stderr.log')
$signalExit = $signal.ExitCode
Wait-Process -Id $brokerPid -Timeout 15 -ErrorAction SilentlyContinue
Start-Sleep -Seconds 2
$brokerGone = -not [bool](Get-Process -Id $brokerPid -ErrorAction SilentlyContinue)
$subscriberAlive = [bool](Get-Process -Id $subscriberPid -ErrorAction SilentlyContinue)
$listeners = @(Get-NetTCPConnection -LocalPort 18884 -State Listen -ErrorAction SilentlyContinue)
$tcp = @(netstat -ano -p tcp | Select-String ':18884' | ForEach-Object { $_.Line })
@(
    "stopped_at=$((Get-Date).ToString('o'))"
    "broker_signal_exit=$signalExit"
    "broker_gone=$($brokerGone.ToString().ToLowerInvariant())"
    "listeners_after=$($listeners.Count)"
    "subscriber_alive_after=$($subscriberAlive.ToString().ToLowerInvariant())"
    "subscriber_pid=$subscriberPid"
    "tcp_entries_after=$($tcp.Count)"
    $tcp
) | Set-Content -LiteralPath (Join-Path $phaseRoot 'reconnect_outage_result.txt') -Encoding utf8
if ($signalExit -ne 0) { throw "Broker signal exited $signalExit" }
if (-not $brokerGone) { throw 'Broker did not stop' }
if ($listeners.Count -ne 0) { throw 'Port 18884 still has a listener' }
Write-Output "M7_WINDOWS_PHASE2_BROKER_OUTAGE_PASS broker_pid=$brokerPid subscriber_alive=$subscriberAlive subscriber_pid=$subscriberPid"
