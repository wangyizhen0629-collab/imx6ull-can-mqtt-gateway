param(
    [Parameter(Mandatory = $true)][string]$Action,
    [Parameter(Mandatory = $true)][string]$EvidenceName,
    [string]$Label = ''
)
$ErrorActionPreference = 'Stop'
$RunDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$PrivateDir = Join-Path $RunDir 'private_raw'
$BoardHost = ((Get-Content -LiteralPath (Join-Path $PrivateDir 'board_endpoint.txt') -Raw) -split '=',2)[1].Trim()
$RawPath = Join-Path $PrivateDir "$EvidenceName.txt"
$PublicPath = Join-Path $RunDir "$EvidenceName.redacted.txt"
if ((Test-Path -LiteralPath $RawPath) -or (Test-Path -LiteralPath $PublicPath)) {
    throw 'refusing to overwrite existing board invocation evidence'
}
$Remote = "sh /tmp/m8-reactor-gate-20260901T170636/board_control.sh $Action"
if ($Label) { $Remote += " $Label" }
$Started = Get-Date -Format o
$Output = & ssh -o BatchMode=yes -o ConnectTimeout=10 "root@$BoardHost" $Remote 2>&1 | Out-String
$ExitCode = $LASTEXITCODE
$Ended = Get-Date -Format o
@(
    "started_at=$Started"
    "command=ssh root@$BoardHost $Remote"
    "exit_code=$ExitCode"
    "ended_at=$Ended"
    '--- output ---'
    $Output.TrimEnd()
) | Out-File -LiteralPath $RawPath -Encoding utf8
$Redacted = (Get-Content -LiteralPath $RawPath -Raw).Replace($BoardHost, '<REDACTED_BOARD_HOST>')
$Redacted | Out-File -LiteralPath $PublicPath -Encoding utf8
@(
    "raw_sha256=$((Get-FileHash $RawPath -Algorithm SHA256).Hash.ToLowerInvariant())"
    "redacted_sha256=$((Get-FileHash $PublicPath -Algorithm SHA256).Hash.ToLowerInvariant())"
    'redaction_method=将目标地址精确替换为 <REDACTED_BOARD_HOST>'
) | Out-File -LiteralPath (Join-Path $RunDir "$EvidenceName.redaction-trace.txt") -Encoding utf8
exit $ExitCode
