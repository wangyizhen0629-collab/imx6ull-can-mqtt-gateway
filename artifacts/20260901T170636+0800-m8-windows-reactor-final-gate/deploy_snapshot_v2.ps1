$ErrorActionPreference='Stop'
$RunDir=Split-Path -Parent $MyInvocation.MyCommand.Path
$PrivateDir=Join-Path $RunDir 'private_raw'
$BoardHost=((Get-Content (Join-Path $PrivateDir 'board_endpoint.txt') -Raw)-split '=',2)[1].Trim()
$RawPath=Join-Path $PrivateDir 'deploy_snapshot_v2.txt';$PublicPath=Join-Path $RunDir 'deploy_snapshot_v2.redacted.txt'
if((Test-Path $RawPath) -or (Test-Path $PublicPath)){throw 'refusing to overwrite deploy evidence'}
$Local=Join-Path $RunDir 'board_snapshot_v2.sh';$Remote='/tmp/m8-reactor-gate-20260901T170636/board_snapshot_v2.sh'
$Started=Get-Date -Format o
$Output=& scp -p -o BatchMode=yes -o ConnectTimeout=10 $Local "root@${BoardHost}:$Remote" 2>&1 | Out-String;$ScpExit=$LASTEXITCODE
if($ScpExit -ne 0){throw 'snapshot v2 SCP failed'}
$Audit=& ssh -o BatchMode=yes -o ConnectTimeout=10 "root@$BoardHost" "chmod 700 '$Remote'; sha256sum '$Remote'" 2>&1 | Out-String;$AuditExit=$LASTEXITCODE
if($AuditExit -ne 0){throw 'snapshot v2 target audit failed'}
@("started_at=$Started","command=scp board_snapshot_v2.sh root@${BoardHost}:$Remote","scp_exit=$ScpExit",$Output.TrimEnd(),"command=ssh root@$BoardHost chmod+sha256","audit_exit=$AuditExit",$Audit.TrimEnd(),"ended_at=$((Get-Date).ToString('o'))") | Out-File $RawPath -Encoding utf8
(Get-Content $RawPath -Raw).Replace($BoardHost,'<REDACTED_BOARD_HOST>') | Out-File $PublicPath -Encoding utf8
@("raw_sha256=$((Get-FileHash $RawPath -Algorithm SHA256).Hash.ToLowerInvariant())","redacted_sha256=$((Get-FileHash $PublicPath -Algorithm SHA256).Hash.ToLowerInvariant())",'redaction_method=将目标地址精确替换为 <REDACTED_BOARD_HOST>') | Out-File (Join-Path $RunDir 'deploy_snapshot_v2.redaction-trace.txt') -Encoding utf8

