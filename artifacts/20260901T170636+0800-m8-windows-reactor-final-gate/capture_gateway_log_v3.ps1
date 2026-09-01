param(
    [Parameter(Mandatory = $true)][string]$Phase,
    [Parameter(Mandatory = $true)][string]$EvidenceName
)
$ErrorActionPreference = 'Stop'
$RunDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$PrivateDir = Join-Path $RunDir 'private_raw'
$BoardHost = ((Get-Content -LiteralPath (Join-Path $PrivateDir 'board_endpoint.txt') -Raw) -split '=',2)[1].Trim()
$Endpoint = @{}
Get-Content -LiteralPath (Join-Path $PrivateDir 'broker_endpoint.txt') | ForEach-Object {
    $Parts = $_ -split '=',2
    if ($Parts.Count -eq 2) { $Endpoint[$Parts[0]] = $Parts[1] }
}
$BrokerAddress = $Endpoint.broker_bind
$RawPath = Join-Path $PrivateDir "$EvidenceName.txt"
$PublicPath = Join-Path $RunDir "$EvidenceName.redacted.txt"
if ((Test-Path -LiteralPath $RawPath) -or (Test-Path -LiteralPath $PublicPath)) {
    throw 'refusing to overwrite existing gateway log evidence'
}
$EvidenceRoot = '/var/lib/gatewayd-m8-test-20260901T170636/evidence'
$Remote = "echo phase=$Phase; echo gateway_pid=`$(cat $EvidenceRoot/$Phase.gateway.pid); echo ---proc---; ps | grep '[g]atewayd'; echo ---stderr-tail---; tail -n 160 $EvidenceRoot/$Phase.gateway.stderr.log"
$Started = Get-Date -Format o
$OldPreference = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
$Output = & ssh -o BatchMode=yes -o ConnectTimeout=10 "root@$BoardHost" $Remote 2>&1 | Out-String
$ExitCode = $LASTEXITCODE
$ErrorActionPreference = $OldPreference
@(
    "started_at=$Started"
    "command=ssh root@$BoardHost <READ_ONLY_GATEWAY_LOG_COMMAND>"
    "exit_code=$ExitCode"
    "ended_at=$((Get-Date).ToString('o'))"
    '--- output ---'
    $Output.TrimEnd()
) | Out-File -LiteralPath $RawPath -Encoding utf8
$Redacted = (Get-Content -LiteralPath $RawPath -Raw).Replace($BoardHost, '<REDACTED_BOARD_HOST>').Replace($BrokerAddress, '<REDACTED_BROKER_ADDRESS>')
$Redacted | Out-File -LiteralPath $PublicPath -Encoding utf8
@(
    "raw_sha256=$((Get-FileHash $RawPath -Algorithm SHA256).Hash.ToLowerInvariant())"
    "redacted_sha256=$((Get-FileHash $PublicPath -Algorithm SHA256).Hash.ToLowerInvariant())"
    'redaction_method=将目标地址和 Broker 地址精确替换为占位符'
) | Out-File -LiteralPath (Join-Path $RunDir "$EvidenceName.redaction-trace.txt") -Encoding utf8
exit $ExitCode
