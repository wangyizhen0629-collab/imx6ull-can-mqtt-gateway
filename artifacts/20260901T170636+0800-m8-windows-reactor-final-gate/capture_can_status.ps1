param([Parameter(Mandatory = $true)][string]$EvidenceName)
$ErrorActionPreference = 'Stop'
$RunDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$PrivateDir = Join-Path $RunDir 'private_raw'
$BoardHost = ((Get-Content (Join-Path $PrivateDir 'board_endpoint.txt') -Raw) -split '=', 2)[1].Trim()
$RawPath = Join-Path $PrivateDir "$EvidenceName.txt"
$PublicPath = Join-Path $RunDir "$EvidenceName.redacted.txt"
if ((Test-Path -LiteralPath $RawPath) -or (Test-Path -LiteralPath $PublicPath)) {
    throw 'refusing to overwrite CAN status evidence'
}
$Remote = "echo captured_at=`$(date -Ins 2>/dev/null); echo rx_packets=`$(cat /sys/class/net/can0/statistics/rx_packets); echo rx_errors=`$(cat /sys/class/net/can0/statistics/rx_errors); echo tx_errors=`$(cat /sys/class/net/can0/statistics/tx_errors); ip -details -statistics link show can0; echo ---spool-files---; ls -l /var/lib/gatewayd-m8-test-20260901T170636/main/spool.* 2>&1"
$Started = Get-Date -Format o
$Output = & ssh -o BatchMode=yes -o ConnectTimeout=10 "root@$BoardHost" $Remote 2>&1 | Out-String
$ExitCode = $LASTEXITCODE
@(
    "started_at=$Started"
    "command=ssh root@$BoardHost <READ_ONLY_CAN_AND_SPOOL_COMMAND>"
    "exit_code=$ExitCode"
    "ended_at=$((Get-Date).ToString('o'))"
    '--- output ---'
    $Output.TrimEnd()
) | Out-File -LiteralPath $RawPath -Encoding utf8
(Get-Content -LiteralPath $RawPath -Raw).Replace($BoardHost, '<REDACTED_BOARD_HOST>') |
    Out-File -LiteralPath $PublicPath -Encoding utf8
@(
    "raw_sha256=$((Get-FileHash -LiteralPath $RawPath -Algorithm SHA256).Hash.ToLowerInvariant())"
    "redacted_sha256=$((Get-FileHash -LiteralPath $PublicPath -Algorithm SHA256).Hash.ToLowerInvariant())"
    'redaction_method=将目标地址精确替换为 <REDACTED_BOARD_HOST>'
) | Out-File -LiteralPath (Join-Path $RunDir "$EvidenceName.redaction-trace.txt") -Encoding utf8
exit $ExitCode
