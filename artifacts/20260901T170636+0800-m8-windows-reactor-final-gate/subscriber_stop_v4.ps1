param([Parameter(Mandatory = $true)][string]$Phase)
$ErrorActionPreference='Stop'
$RunDir=Split-Path -Parent $MyInvocation.MyCommand.Path
$PrivateDir=Join-Path $RunDir 'private_raw'
$PublicPhaseDir=Join-Path $RunDir "windows-$Phase"
$PrivatePhaseDir=Join-Path $PrivateDir "windows-$Phase"
$SubscriberPid=[int]((Get-Content (Join-Path $PublicPhaseDir 'subscriber.pid.txt'))-split '=',2)[1]
$Process=Get-Process -Id $SubscriberPid -ErrorAction Stop
$VerifiedPath=$Process.Path
if($VerifiedPath -ne 'E:\mosquitto\mosquitto_sub.exe'){throw 'subscriber PID path mismatch'}
$Connection=Get-NetTCPConnection -State Established -ErrorAction SilentlyContinue | Where-Object {$_.OwningProcess -eq $SubscriberPid -and $_.RemotePort -eq 18884}
$RawPath=Join-Path $PrivatePhaseDir 'subscriber-stop-v4.txt';$PublicPath=Join-Path $PublicPhaseDir 'subscriber-stop-v4.redacted.txt'
if((Test-Path $RawPath) -or (Test-Path $PublicPath)){throw 'refusing to overwrite v4 subscriber stop evidence'}
$Stopped=Get-Date -Format o
Stop-Process -Id $SubscriberPid -ErrorAction Stop
for($i=0;$i -lt 100 -and (Get-Process -Id $SubscriberPid -ErrorAction SilentlyContinue);$i++){Start-Sleep -Milliseconds 100}
if(Get-Process -Id $SubscriberPid -ErrorAction SilentlyContinue){throw 'subscriber did not exit'}
$ExitPath=Join-Path $PublicPhaseDir 'subscriber.exit.txt';for($i=0;$i -lt 100 -and -not (Test-Path $ExitPath);$i++){Start-Sleep -Milliseconds 100}
$WrapperExit=if(Test-Path $ExitPath){(Get-Content $ExitPath -Raw).Trim()}else{'wrapper_exit=NOT AVAILABLE'}
@(
 "stopped_at=$Stopped"
 "verified_pid=$SubscriberPid"
 "verified_path=$VerifiedPath"
 "connection_count_before=$(@($Connection).Count)"
 'command=Stop-Process -Id <VERIFIED_DEDICATED_PID>'
 $WrapperExit
 'status=PASS'
) | Out-File $RawPath -Encoding utf8
$Endpoint=@{};Get-Content (Join-Path $PrivateDir 'broker_endpoint.txt') | ForEach-Object {$p=$_ -split '=',2;if($p.Count -eq 2){$Endpoint[$p[0]]=$p[1]}}
$Bind=$Endpoint.broker_bind
(Get-Content $RawPath -Raw).Replace($Bind,'<REDACTED_BROKER_ADDRESS>') | Out-File $PublicPath -Encoding utf8
@("raw_sha256=$((Get-FileHash $RawPath -Algorithm SHA256).Hash.ToLowerInvariant())","redacted_sha256=$((Get-FileHash $PublicPath -Algorithm SHA256).Hash.ToLowerInvariant())",'redaction_method=将真实 Broker 地址精确替换为 <REDACTED_BROKER_ADDRESS>') | Out-File (Join-Path $PublicPhaseDir 'subscriber-stop-v4.redaction-trace.txt') -Encoding utf8
