$ErrorActionPreference = 'Stop'

$runRoot = $PSScriptRoot
$phaseRoot = Join-Path $runRoot 'windows-phase1-attempt2'
$privateRoot = Join-Path $runRoot 'private_raw/windows-phase1-attempt2'
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
if (-not (Test-Path -LiteralPath $brokerSignal -PathType Leaf)) { throw "Missing $brokerSignal" }

@(
    "captured_at=$((Get-Date).ToString('o'))"
    "subscriber_pid=$subscriberPid"
    "subscriber_name=$($subscriber.ProcessName)"
    "broker_pid=$brokerPid"
    "broker_name=$($broker.ProcessName)"
    'tcp_before:'
    (netstat -ano -p tcp | Select-String ':18884' | ForEach-Object { $_.Line })
) | Set-Content -LiteralPath (Join-Path $phaseRoot 'stop_preflight.txt') -Encoding utf8
@(
    "Stop-Process -Id $subscriberPid"
    "E:\mosquitto\mosquitto_signal.exe -p $brokerPid shutdown"
) | Set-Content -LiteralPath (Join-Path $phaseRoot 'stop_commands.txt') -Encoding ascii

Stop-Process -Id $subscriberPid -ErrorAction Stop
Wait-Process -Id $subscriberPid -Timeout 10 -ErrorAction SilentlyContinue
$subscriberGone = -not [bool](Get-Process -Id $subscriberPid -ErrorAction SilentlyContinue)

$signalProcess = Start-Process -FilePath $brokerSignal -ArgumentList @('-p', $brokerPid, 'shutdown') -NoNewWindow -Wait -PassThru -RedirectStandardOutput (Join-Path $privateRoot 'broker_signal.stdout.log') -RedirectStandardError (Join-Path $privateRoot 'broker_signal.stderr.log')
$signalExit = $signalProcess.ExitCode
Wait-Process -Id $brokerPid -Timeout 15 -ErrorAction SilentlyContinue
$brokerGone = -not [bool](Get-Process -Id $brokerPid -ErrorAction SilentlyContinue)
$listenersAfter = @(Get-NetTCPConnection -LocalPort 18884 -State Listen -ErrorAction SilentlyContinue)
$tcpAfter = @(netstat -ano -p tcp | Select-String ':18884' | ForEach-Object { $_.Line })

@(
    "stopped_at=$((Get-Date).ToString('o'))"
    'subscriber_stop_exit=0'
    "subscriber_gone=$($subscriberGone.ToString().ToLowerInvariant())"
    "broker_signal_exit=$signalExit"
    "broker_gone=$($brokerGone.ToString().ToLowerInvariant())"
    "listeners_after=$($listenersAfter.Count)"
    "tcp_entries_after=$($tcpAfter.Count)"
    $tcpAfter
) | Set-Content -LiteralPath (Join-Path $phaseRoot 'stopped.txt') -Encoding utf8

if (-not $subscriberGone) { throw 'subscriber did not stop' }
if ($signalExit -ne 0) { throw "broker signal exited $signalExit" }
if (-not $brokerGone) { throw 'broker did not stop' }
if ($listenersAfter.Count -ne 0) { throw 'port 18884 still has a listener' }
Write-Output "M7_WINDOWS_PHASE1_STOP_PASS broker_pid=$brokerPid subscriber_pid=$subscriberPid"
