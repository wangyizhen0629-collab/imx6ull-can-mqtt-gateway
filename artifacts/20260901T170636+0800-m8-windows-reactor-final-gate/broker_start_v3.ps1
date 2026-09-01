param([Parameter(Mandatory = $true)][string]$Phase)
$ErrorActionPreference = 'Stop'
$RunDir=Split-Path -Parent $MyInvocation.MyCommand.Path
$PrivateDir=Join-Path $RunDir 'private_raw'
$PublicPhaseDir=Join-Path $RunDir "windows-$Phase"
$PrivatePhaseDir=Join-Path $PrivateDir "windows-$Phase"
if((Test-Path $PublicPhaseDir) -or (Test-Path $PrivatePhaseDir)){throw 'phase directory already exists'}
New-Item -ItemType Directory -Path $PublicPhaseDir,$PrivatePhaseDir | Out-Null
$Endpoint=@{}
Get-Content (Join-Path $PrivateDir 'broker_endpoint.txt') | ForEach-Object {$p=$_ -split '=',2;if($p.Count -eq 2){$Endpoint[$p[0]]=$p[1]}}
$Bind=$Endpoint.broker_bind
$Existing=Get-NetTCPConnection -State Listen -ErrorAction SilentlyContinue | Where-Object LocalPort -eq 18884
if($Existing){throw 'refusing to start: port 18884 already has a listener'}
$ConfigTest=Start-Process -FilePath 'E:\mosquitto\mosquitto.exe' -ArgumentList @('--test-config','-c',(Join-Path $PrivateDir 'mosquitto.conf')) -PassThru -Wait -WindowStyle Hidden -RedirectStandardOutput (Join-Path $PrivatePhaseDir 'config-test.stdout.log') -RedirectStandardError (Join-Path $PrivatePhaseDir 'config-test.stderr.log')
if($ConfigTest.ExitCode -ne 0){throw 'Mosquitto config test failed'}
$Wrapper=Start-Process -FilePath powershell.exe -ArgumentList @('-NoProfile','-ExecutionPolicy','Bypass','-File',(Join-Path $RunDir 'windows_service_wrapper.ps1'),'-Kind','broker','-Phase',$Phase) -PassThru -WindowStyle Hidden
$PidPath=Join-Path $PublicPhaseDir 'broker.pid.txt'
for($i=0;$i -lt 100 -and -not (Test-Path $PidPath);$i++){Start-Sleep -Milliseconds 100}
if(-not (Test-Path $PidPath)){throw 'Broker PID file timeout'}
$BrokerPid=[int]((Get-Content $PidPath)-split '=',2)[1]
$Listener=$null
for($i=0;$i -lt 100;$i++){$Listener=Get-NetTCPConnection -State Listen -ErrorAction SilentlyContinue | Where-Object {$_.LocalPort -eq 18884 -and $_.OwningProcess -eq $BrokerPid};if($Listener){break};Start-Sleep -Milliseconds 100}
if(-not $Listener){throw 'Broker listener readiness timeout'}
$RawPath=Join-Path $PrivatePhaseDir 'broker-start.txt'
$PublicPath=Join-Path $PublicPhaseDir 'broker-start.redacted.txt'
@(
 "started_at=$((Get-Date).ToString('o'))"
 "config_test_exit=$($ConfigTest.ExitCode)"
 "wrapper_pid=$($Wrapper.Id)"
 "broker_pid=$BrokerPid"
 "listener_address=$($Listener.LocalAddress)"
 "listener_port=$($Listener.LocalPort)"
 "listener_owner=$($Listener.OwningProcess)"
 'status=PASS'
) | Out-File $RawPath -Encoding utf8
(Get-Content $RawPath -Raw).Replace($Bind,'<REDACTED_BROKER_ADDRESS>') | Out-File $PublicPath -Encoding utf8
@("raw_sha256=$((Get-FileHash $RawPath -Algorithm SHA256).Hash.ToLowerInvariant())","redacted_sha256=$((Get-FileHash $PublicPath -Algorithm SHA256).Hash.ToLowerInvariant())",'redaction_method=将真实 Broker 地址精确替换为 <REDACTED_BROKER_ADDRESS>') | Out-File (Join-Path $PublicPhaseDir 'broker-start.redaction-trace.txt') -Encoding utf8

