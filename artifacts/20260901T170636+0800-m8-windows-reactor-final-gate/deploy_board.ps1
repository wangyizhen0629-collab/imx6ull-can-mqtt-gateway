$ErrorActionPreference = 'Stop'
$RunDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$PrivateDir = Join-Path $RunDir 'private_raw'
$BoardHost = ((Get-Content -LiteralPath (Join-Path $PrivateDir 'board_endpoint.txt') -Raw) -split '=',2)[1].Trim()
$RawPath = Join-Path $PrivateDir 'board_deploy.txt'
$PublicPath = Join-Path $RunDir 'board_deploy.redacted.txt'
$Stage = '/tmp/m8-reactor-gate-20260901T170636'
$Lines = @("started_at=$((Get-Date).ToString('o'))")

$Output = & ssh -o BatchMode=yes -o ConnectTimeout=10 "root@$BoardHost" "test ! -e '$Stage' && mkdir -p '$Stage/incoming' && chmod 700 '$Stage' '$Stage/incoming'" 2>&1 | Out-String
$Lines += 'command=ssh root@<PRIVATE> create unique non-system stage'
$Lines += "exit_code=$LASTEXITCODE"
$Lines += $Output.TrimEnd()
if ($LASTEXITCODE -ne 0) { throw 'target stage creation failed' }

$Transfers = @(
    @{ Local=(Join-Path $PrivateDir 'incoming\gatewayd'); Remote="$Stage/incoming/gatewayd" },
    @{ Local=(Join-Path $PrivateDir 'gateway.conf'); Remote="$Stage/incoming/gateway.conf" },
    @{ Local=(Join-Path $RunDir 'board_control.sh'); Remote="$Stage/board_control.sh" }
)
foreach ($Transfer in $Transfers) {
    $Output = & scp -p -o BatchMode=yes -o ConnectTimeout=10 $Transfer.Local "root@${BoardHost}:$($Transfer.Remote)" 2>&1 | Out-String
    $Lines += "command=scp $(Split-Path -Leaf $Transfer.Local) root@<PRIVATE>:$($Transfer.Remote)"
    $Lines += "exit_code=$LASTEXITCODE"
    $Lines += $Output.TrimEnd()
    if ($LASTEXITCODE -ne 0) { throw "SCP failed for $($Transfer.Local)" }
}
$Output = & ssh -o BatchMode=yes -o ConnectTimeout=10 "root@$BoardHost" "sha256sum '$Stage/incoming/gatewayd' '$Stage/incoming/gateway.conf' '$Stage/board_control.sh'; chmod 700 '$Stage/board_control.sh'" 2>&1 | Out-String
$Lines += 'command=ssh root@<PRIVATE> target deployment hash and chmod'
$Lines += "exit_code=$LASTEXITCODE"
$Lines += $Output.TrimEnd()
if ($LASTEXITCODE -ne 0) { throw 'target deployment hash audit failed' }
$Lines += "ended_at=$((Get-Date).ToString('o'))"
$Lines | Out-File -LiteralPath $RawPath -Encoding utf8
$Redacted = (Get-Content -LiteralPath $RawPath -Raw).Replace($BoardHost, '<REDACTED_BOARD_HOST>')
$Redacted | Out-File -LiteralPath $PublicPath -Encoding utf8
@(
    "raw_sha256=$((Get-FileHash $RawPath -Algorithm SHA256).Hash.ToLowerInvariant())"
    "redacted_sha256=$((Get-FileHash $PublicPath -Algorithm SHA256).Hash.ToLowerInvariant())"
    'redaction_method=将目标地址精确替换为 <REDACTED_BOARD_HOST>'
) | Out-File -LiteralPath (Join-Path $RunDir 'board_deploy.redaction-trace.txt') -Encoding utf8

