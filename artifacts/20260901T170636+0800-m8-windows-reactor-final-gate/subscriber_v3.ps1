param([Parameter(Mandatory = $true)][ValidateSet('start','stop')][string]$Action,[Parameter(Mandatory = $true)][string]$Phase)
$ErrorActionPreference='Stop'
$RunDir=Split-Path -Parent $MyInvocation.MyCommand.Path
$PrivateDir=Join-Path $RunDir 'private_raw'
$PublicPhaseDir=Join-Path $RunDir "windows-$Phase"
$PrivatePhaseDir=Join-Path $PrivateDir "windows-$Phase"
$Endpoint=@{}
Get-Content (Join-Path $PrivateDir 'broker_endpoint.txt') | ForEach-Object {$p=$_ -split '=',2;if($p.Count -eq 2){$Endpoint[$p[0]]=$p[1]}}
$Bind=$Endpoint.broker_bind
if($Action -eq 'start'){
 if((Test-Path $PublicPhaseDir) -or (Test-Path $PrivatePhaseDir)){throw 'subscriber phase already exists'}
 New-Item -ItemType Directory -Path $PublicPhaseDir,$PrivatePhaseDir | Out-Null
 $Wrapper=Start-Process -FilePath powershell.exe -ArgumentList @('-NoProfile','-ExecutionPolicy','Bypass','-File',(Join-Path $RunDir 'windows_service_wrapper.ps1'),'-Kind','subscriber','-Phase',$Phase) -PassThru -WindowStyle Hidden
 $PidPath=Join-Path $PublicPhaseDir 'subscriber.pid.txt'
 for($i=0;$i -lt 100 -and -not (Test-Path $PidPath);$i++){Start-Sleep -Milliseconds 100}
 if(-not (Test-Path $PidPath)){throw 'subscriber PID timeout'}
 $SubscriberPid=[int]((Get-Content $PidPath)-split '=',2)[1]
 $Connection=$null
 for($i=0;$i -lt 100;$i++){$Connection=Get-NetTCPConnection -State Established -ErrorAction SilentlyContinue | Where-Object {$_.OwningProcess -eq $SubscriberPid -and $_.RemotePort -eq 18884};if($Connection){break};Start-Sleep -Milliseconds 100}
 if(-not $Connection){throw 'subscriber connection readiness timeout'}
 $RawPath=Join-Path $PrivatePhaseDir 'subscriber-start.txt';$PublicPath=Join-Path $PublicPhaseDir 'subscriber-start.redacted.txt'
 @("started_at=$((Get-Date).ToString('o'))","wrapper_pid=$($Wrapper.Id)","subscriber_pid=$SubscriberPid","remote_address=$($Connection.RemoteAddress)","remote_port=$($Connection.RemotePort)",'status=PASS') | Out-File $RawPath -Encoding utf8
}else{
 $SubscriberPid=[int]((Get-Content (Join-Path $PublicPhaseDir 'subscriber.pid.txt'))-split '=',2)[1]
 $Process=Get-Process -Id $SubscriberPid -ErrorAction Stop
 if($Process.Path -ne 'E:\mosquitto\mosquitto_sub.exe'){throw 'subscriber PID path mismatch'}
 Stop-Process -Id $SubscriberPid -ErrorAction Stop
 for($i=0;$i -lt 100 -and (Get-Process -Id $SubscriberPid -ErrorAction SilentlyContinue);$i++){Start-Sleep -Milliseconds 100}
 if(Get-Process -Id $SubscriberPid -ErrorAction SilentlyContinue){throw 'subscriber did not exit'}
 $ExitPath=Join-Path $PublicPhaseDir 'subscriber.exit.txt';for($i=0;$i -lt 100 -and -not (Test-Path $ExitPath);$i++){Start-Sleep -Milliseconds 100}
 $WrapperExit=if(Test-Path $ExitPath){(Get-Content $ExitPath -Raw).Trim()}else{'wrapper_exit=NOT AVAILABLE'}
 $RawPath=Join-Path $PrivatePhaseDir 'subscriber-stop.txt';$PublicPath=Join-Path $PublicPhaseDir 'subscriber-stop.redacted.txt'
 @("stopped_at=$((Get-Date).ToString('o'))","verified_pid=$SubscriberPid","verified_path=$($Process.Path)",$WrapperExit,'status=PASS') | Out-File $RawPath -Encoding utf8
}
(Get-Content $RawPath -Raw).Replace($Bind,'<REDACTED_BROKER_ADDRESS>') | Out-File $PublicPath -Encoding utf8
@("raw_sha256=$((Get-FileHash $RawPath -Algorithm SHA256).Hash.ToLowerInvariant())","redacted_sha256=$((Get-FileHash $PublicPath -Algorithm SHA256).Hash.ToLowerInvariant())",'redaction_method=将真实 Broker 地址精确替换为 <REDACTED_BROKER_ADDRESS>') | Out-File (Join-Path $PublicPhaseDir "subscriber-$Action.redaction-trace.txt") -Encoding utf8
