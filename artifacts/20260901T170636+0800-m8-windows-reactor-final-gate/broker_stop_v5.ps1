param([Parameter(Mandatory = $true)][string]$Phase)
$ErrorActionPreference='Stop'
$RunDir=Split-Path -Parent $MyInvocation.MyCommand.Path
$PrivateDir=Join-Path $RunDir 'private_raw'
$PublicPhaseDir=Join-Path $RunDir "windows-$Phase"
$PrivatePhaseDir=Join-Path $PrivateDir "windows-$Phase"
$BrokerPid=[int]((Get-Content (Join-Path $PublicPhaseDir 'broker.pid.txt'))-split '=',2)[1]
$Process=Get-Process -Id $BrokerPid -ErrorAction Stop
$VerifiedPath=$Process.Path
if($VerifiedPath -ne 'E:\mosquitto\mosquitto.exe'){throw 'Broker PID path mismatch'}
$Listener=Get-NetTCPConnection -State Listen -ErrorAction Stop | Where-Object {$_.LocalPort -eq 18884 -and $_.OwningProcess -eq $BrokerPid}
if(-not $Listener){throw 'verified Broker does not own listener 18884'}
$VerifiedAddress=$Listener.LocalAddress
$VerifiedPort=$Listener.LocalPort
$RawPath=Join-Path $PrivatePhaseDir 'broker-stop-v5.txt';$PublicPath=Join-Path $PublicPhaseDir 'broker-stop-v5.redacted.txt'
if((Test-Path $RawPath) -or (Test-Path $PublicPath)){throw 'refusing to overwrite v5 stop evidence'}
$Started=Get-Date -Format o
Stop-Process -Id $BrokerPid -ErrorAction Stop
for($i=0;$i -lt 100 -and (Get-Process -Id $BrokerPid -ErrorAction SilentlyContinue);$i++){Start-Sleep -Milliseconds 100}
if(Get-Process -Id $BrokerPid -ErrorAction SilentlyContinue){throw 'Stop-Process did not terminate dedicated Broker'}
$Remaining=Get-NetTCPConnection -State Listen -ErrorAction SilentlyContinue | Where-Object LocalPort -eq 18884
if($Remaining){throw 'listener remains after Stop-Process'}
$ExitPath=Join-Path $PublicPhaseDir 'broker.exit.txt'
for($i=0;$i -lt 100 -and -not (Test-Path $ExitPath);$i++){Start-Sleep -Milliseconds 100}
$WrapperExit=if(Test-Path $ExitPath){(Get-Content $ExitPath -Raw).Trim()}else{'wrapper_exit=NOT AVAILABLE'}
@(
 "started_at=$Started"
 "verified_pid=$BrokerPid"
 "verified_path=$VerifiedPath"
 "verified_listener_address=$VerifiedAddress"
 "verified_listener_port=$VerifiedPort"
 'command=Stop-Process -Id <VERIFIED_DEDICATED_PID>'
 'stop_process_result=PASS'
 'listener_count_after=0'
 $WrapperExit
 "ended_at=$((Get-Date).ToString('o'))"
 'status=PASS'
) | Out-File $RawPath -Encoding utf8
$Endpoint=@{};Get-Content (Join-Path $PrivateDir 'broker_endpoint.txt') | ForEach-Object {$p=$_ -split '=',2;if($p.Count -eq 2){$Endpoint[$p[0]]=$p[1]}}
$Bind=$Endpoint.broker_bind
(Get-Content $RawPath -Raw).Replace($Bind,'<REDACTED_BROKER_ADDRESS>') | Out-File $PublicPath -Encoding utf8
@("raw_sha256=$((Get-FileHash $RawPath -Algorithm SHA256).Hash.ToLowerInvariant())","redacted_sha256=$((Get-FileHash $PublicPath -Algorithm SHA256).Hash.ToLowerInvariant())",'redaction_method=将真实 Broker 地址精确替换为 <REDACTED_BROKER_ADDRESS>') | Out-File (Join-Path $PublicPhaseDir 'broker-stop-v5.redaction-trace.txt') -Encoding utf8
