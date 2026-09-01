param([Parameter(Mandatory=$true)][ValidateSet('main','state')][string]$Area,[Parameter(Mandatory=$true)][string]$Label,[Parameter(Mandatory=$true)][string]$EvidenceName)
$ErrorActionPreference='Stop'
$RunDir=Split-Path -Parent $MyInvocation.MyCommand.Path
$PrivateDir=Join-Path $RunDir 'private_raw'
$BoardHost=((Get-Content (Join-Path $PrivateDir 'board_endpoint.txt') -Raw)-split '=',2)[1].Trim()
$RawPath=Join-Path $PrivateDir "$EvidenceName.txt";$PublicPath=Join-Path $RunDir "$EvidenceName.redacted.txt"
if((Test-Path $RawPath) -or (Test-Path $PublicPath)){throw 'refusing to overwrite snapshot evidence'}
$Remote="sh /tmp/m8-reactor-gate-20260901T170636/board_snapshot_v2.sh $Area $Label"
$Started=Get-Date -Format o
$Output=& ssh -o BatchMode=yes -o ConnectTimeout=10 "root@$BoardHost" $Remote 2>&1 | Out-String;$ExitCode=$LASTEXITCODE
@("started_at=$Started","command=ssh root@$BoardHost $Remote","exit_code=$ExitCode","ended_at=$((Get-Date).ToString('o'))",'--- output ---',$Output.TrimEnd()) | Out-File $RawPath -Encoding utf8
(Get-Content $RawPath -Raw).Replace($BoardHost,'<REDACTED_BOARD_HOST>') | Out-File $PublicPath -Encoding utf8
@("raw_sha256=$((Get-FileHash $RawPath -Algorithm SHA256).Hash.ToLowerInvariant())","redacted_sha256=$((Get-FileHash $PublicPath -Algorithm SHA256).Hash.ToLowerInvariant())",'redaction_method=将目标地址精确替换为 <REDACTED_BOARD_HOST>') | Out-File (Join-Path $RunDir "$EvidenceName.redaction-trace.txt") -Encoding utf8
exit $ExitCode
