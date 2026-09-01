$ErrorActionPreference = 'Continue'
$RunDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$PrivateDir = Join-Path $RunDir 'private_raw'
$EndpointFile = Join-Path $PrivateDir 'board_endpoint.txt'
$ExpectedBinarySha = '2e3976727d57f850223ec3b0b3713c930d96f75375897f7c1fe69dcfc2e1548b'
$ExpectedLibrarySha = 'b32c8ac4defb2b2920fba2e42f263869508c42e3c1719440db37ffc8d8c2f636'

if (-not (Test-Path -LiteralPath $EndpointFile)) {
    'NOT RUN - private target endpoint is unavailable' |
        Out-File -LiteralPath (Join-Path $RunDir 'board_readonly_preflight.redacted.txt') -Encoding utf8
    exit 2
}
$BoardHost = ((Get-Content -LiteralPath $EndpointFile -Raw) -split '=', 2)[1].Trim()
$RemoteScript = @'
set +e
echo "readonly_started_at=$(date -Ins 2>/dev/null)"
uname -a
id
echo '--- mounts ---'
cat /proc/mounts
echo '--- storage ---'
df -h /var/lib /tmp 2>&1
echo '--- can0 ---'
ip -details -statistics link show can0 2>&1
echo '--- gateway processes ---'
for comm in /proc/[0-9]*/comm; do
    [ -r "$comm" ] || continue
    if grep -qx gatewayd "$comm" 2>/dev/null; then
        pid=${comm#/proc/}; pid=${pid%/comm}
        printf 'pid=%s cmdline=' "$pid"
        tr '\000' ' ' < "/proc/$pid/cmdline" 2>/dev/null
        echo
    fi
done
echo '--- gateway binary candidates ---'
for f in /tmp/*/bin/gatewayd /tmp/*/gatewayd /var/lib/*/gatewayd; do
    [ -f "$f" ] || continue
    sha256sum "$f"
done
echo '--- libmosquitto candidates ---'
for f in /tmp/*/lib/libmosquitto.so.1 /tmp/*/lib/libmosquitto.so.*; do
    [ -e "$f" ] || continue
    real=$(readlink -f "$f" 2>/dev/null)
    [ -n "$real" ] || continue
    sha256sum "$real"
done | sort -u
echo "readonly_completed_at=$(date -Ins 2>/dev/null)"
'@

$Started = Get-Date -Format o
$RawCommand = "ssh -o BatchMode=yes -o ConnectTimeout=10 root@$BoardHost sh"
$RawCommand | Out-File -LiteralPath (Join-Path $PrivateDir 'board_readonly_command.txt') -Encoding utf8
$Output = $RemoteScript | ssh -o BatchMode=yes -o ConnectTimeout=10 "root@$BoardHost" sh 2>&1 | Out-String
$ExitCode = $LASTEXITCODE
$Ended = Get-Date -Format o
$RawPath = Join-Path $PrivateDir 'board_readonly_output.txt'
@(
    "started_at=$Started"
    "command=$RawCommand"
    "exit_code=$ExitCode"
    "ended_at=$Ended"
    '--- output ---'
    $Output.TrimEnd()
) | Out-File -LiteralPath $RawPath -Encoding utf8
$Redacted = (Get-Content -LiteralPath $RawPath -Raw).Replace($BoardHost, '<REDACTED_BOARD_HOST>')
$RedactedPath = Join-Path $RunDir 'board_readonly_preflight.redacted.txt'
$Redacted | Out-File -LiteralPath $RedactedPath -Encoding utf8
@(
    "raw_command_sha256=$((Get-FileHash (Join-Path $PrivateDir 'board_readonly_command.txt') -Algorithm SHA256).Hash.ToLowerInvariant())"
    "raw_output_sha256=$((Get-FileHash $RawPath -Algorithm SHA256).Hash.ToLowerInvariant())"
    "redacted_output_sha256=$((Get-FileHash $RedactedPath -Algorithm SHA256).Hash.ToLowerInvariant())"
    'redaction_method=将目标地址精确替换为 <REDACTED_BOARD_HOST>'
    "expected_gateway_sha256=$ExpectedBinarySha"
    "expected_libmosquitto_sha256=$ExpectedLibrarySha"
) | Out-File -LiteralPath (Join-Path $RunDir 'board_readonly_redaction_trace.txt') -Encoding utf8
exit $ExitCode
