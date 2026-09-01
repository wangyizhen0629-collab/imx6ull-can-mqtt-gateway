$ErrorActionPreference = 'Stop'
$RunDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$PrivateDir = Join-Path $RunDir 'private_raw'
$EndpointFile = Join-Path $PrivateDir 'board_endpoint.txt'
$RawPath = Join-Path $PrivateDir 'board_readonly_preflight.txt'
$PublicPath = Join-Path $RunDir 'board_readonly_preflight.redacted.txt'
$TracePath = Join-Path $RunDir 'board_readonly_preflight.redaction-trace.txt'
$ExpectedBinarySha = '6e8729417b3dc40c10a413459de5eca9be43ce58dfcc8a3b12e91f5c8d7ef958'

foreach ($Path in @($RawPath, $PublicPath, $TracePath)) {
    if (Test-Path -LiteralPath $Path) {
        throw "refusing to overwrite existing evidence: $Path"
    }
}
if (-not (Test-Path -LiteralPath $EndpointFile -PathType Leaf)) {
    throw 'private board endpoint is unavailable'
}
$BoardHost = ((Get-Content -LiteralPath $EndpointFile -Raw) -split '=', 2)[1].Trim()
if (-not $BoardHost) {
    throw 'private board endpoint is empty'
}

$RemoteScript = @'
set +e
echo "remote_started_at=$(date -Ins 2>/dev/null)"
echo "date_epoch=$(date +%s 2>/dev/null)"
echo '--- identity ---'
uname -a
id
echo '--- busybox ---'
busybox 2>&1 | sed -n '1p'
busybox init --help 2>&1
echo "busybox_init_help_exit=$?"
echo '--- pid1 ---'
printf 'pid1_comm='; cat /proc/1/comm 2>&1
printf 'pid1_cmdline='; tr '\000' ' ' < /proc/1/cmdline 2>/dev/null; echo
printf 'pid1_exe='; readlink -f /proc/1/exe 2>&1
printf 'pid1_exe_sha256='; p=$(readlink -f /proc/1/exe 2>/dev/null); [ -n "$p" ] && sha256sum "$p" 2>/dev/null || true
echo '--- process table ---'
ps w 2>&1
echo '--- matching process identity ---'
for p in /proc/[0-9]*; do
    [ -r "$p/cmdline" ] || continue
    cmd=$(tr '\000' ' ' < "$p/cmdline" 2>/dev/null)
    case "$cmd" in
        *gatewayd*)
            pid=${p#/proc/}
            printf 'pid=%s comm=' "$pid"; cat "$p/comm" 2>/dev/null
            printf 'pid=%s cmdline=%s\n' "$pid" "$cmd"
            exe=$(readlink -f "$p/exe" 2>/dev/null)
            printf 'pid=%s exe=%s\n' "$pid" "$exe"
            [ -f "$exe" ] && sha256sum "$exe" 2>/dev/null
            ;;
    esac
done
echo '--- inittab before ---'
if [ -f /etc/inittab ]; then
    stat -c 'mode=%a uid=%u gid=%g size=%s path=%n' /etc/inittab 2>&1
    sha256sum /etc/inittab 2>&1
    cat /etc/inittab 2>&1
else
    echo 'MISSING /etc/inittab'
fi
echo '--- managed files before ---'
for f in /etc/init.d/gatewayd /etc/default/gatewayd /etc/gatewayd/gateway.conf /usr/bin/gatewayd; do
    if [ -e "$f" ]; then
        stat -c 'mode=%a uid=%u gid=%g size=%s path=%n' "$f" 2>&1
        sha256sum "$f" 2>&1
    else
        echo "MISSING $f"
    fi
done
echo '--- target gatewayd candidates ---'
find /tmp /var/lib -maxdepth 5 -type f -name gatewayd 2>/dev/null | while IFS= read -r f; do
    stat -c 'mode=%a uid=%u gid=%g size=%s path=%n' "$f" 2>&1
    sha256sum "$f" 2>&1
done
echo '--- expected binary candidate detail ---'
find /tmp /var/lib -maxdepth 5 -type f -name gatewayd 2>/dev/null | while IFS= read -r f; do
    got=$(sha256sum "$f" 2>/dev/null | sed -n 's/ .*//p')
    [ "$got" = '6e8729417b3dc40c10a413459de5eca9be43ce58dfcc8a3b12e91f5c8d7ef958' ] || continue
    echo "expected_binary_path=$f"
    file "$f" 2>&1
    ldd "$f" 2>&1
done
echo '--- libmosquitto candidates ---'
find /tmp /var/lib /usr/lib /lib -maxdepth 6 \( -type f -o -type l \) -name 'libmosquitto.so*' 2>/dev/null | while IFS= read -r f; do
    real=$(readlink -f "$f" 2>/dev/null)
    printf 'link=%s real=%s ' "$f" "$real"
    [ -f "$real" ] && sha256sum "$real" 2>/dev/null || echo
done
echo '--- can0 read-only ---'
ip -details -statistics link show can0 2>&1
echo '--- mounts and storage ---'
cat /proc/mounts 2>&1
df -h /var/lib /tmp 2>&1
echo "remote_completed_at=$(date -Ins 2>/dev/null)"
'@

$Started = Get-Date -Format o
$PreviousOutputEncoding = $OutputEncoding
$PreviousErrorAction = $ErrorActionPreference
$OutputEncoding = [Text.UTF8Encoding]::new($false)
$ErrorActionPreference = 'Continue'
$Output = $RemoteScript | & ssh -o BatchMode=yes -o ConnectTimeout=10 "root@$BoardHost" sh 2>&1 | Out-String
$ExitCode = $LASTEXITCODE
$ErrorActionPreference = $PreviousErrorAction
$OutputEncoding = $PreviousOutputEncoding
$Ended = Get-Date -Format o
$Raw = @(
    "started_at=$Started"
    'command=ssh -o BatchMode=yes -o ConnectTimeout=10 root@<PRIVATE> sh'
    "exit_code=$ExitCode"
    "ended_at=$Ended"
    '--- full output ---'
    $Output.TrimEnd()
) -join "`r`n"
[IO.File]::WriteAllText($RawPath, $Raw + "`r`n", [Text.UTF8Encoding]::new($false))

$Redacted = $Raw.Replace($BoardHost, '<REDACTED_BOARD_HOST>')
$Redacted = [regex]::Replace($Redacted, '(?<![0-9])(?:[0-9]{1,3}\.){3}[0-9]{1,3}(?![0-9])', '<REDACTED_IPV4>')
$Redacted = [regex]::Replace($Redacted, '(?im)^(broker_(?:host|username|password)\s*=).+$', '$1<REDACTED>')
[IO.File]::WriteAllText($PublicPath, $Redacted + "`r`n", [Text.UTF8Encoding]::new($false))

@(
    "raw_sha256=$((Get-FileHash -LiteralPath $RawPath -Algorithm SHA256).Hash.ToLowerInvariant())"
    "redacted_sha256=$((Get-FileHash -LiteralPath $PublicPath -Algorithm SHA256).Hash.ToLowerInvariant())"
    'redaction_method=exact target endpoint replacement;all IPv4 literals replaced;broker host/username/password assignments replaced'
    "expected_binary_sha256=$ExpectedBinarySha"
) | Set-Content -LiteralPath $TracePath -Encoding utf8

"BOARD_PREFLIGHT_EXIT=$ExitCode"
"RAW_SHA256=$((Get-FileHash -LiteralPath $RawPath -Algorithm SHA256).Hash.ToLowerInvariant())"
"REDACTED_SHA256=$((Get-FileHash -LiteralPath $PublicPath -Algorithm SHA256).Hash.ToLowerInvariant())"
exit $ExitCode
