$ErrorActionPreference = 'Stop'
$RunDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$PrivateDir = Join-Path $RunDir 'private_raw'
$EndpointFile = Join-Path $PrivateDir 'board_endpoint.txt'
$RawPath = Join-Path $PrivateDir 'board_busybox_identification.txt'
$PublicPath = Join-Path $RunDir 'board_busybox_identification.redacted.txt'
$TracePath = Join-Path $RunDir 'board_busybox_identification.redaction-trace.txt'
foreach ($Path in @($RawPath, $PublicPath, $TracePath)) {
    if (Test-Path -LiteralPath $Path) {
        throw "refusing to overwrite existing evidence: $Path"
    }
}
$BoardHost = ((Get-Content -LiteralPath $EndpointFile -Raw) -split '=', 2)[1].Trim()

$RemoteScript = @'
set +e
echo "remote_started_at=$(date -Ins 2>/dev/null)"
echo '--- pid1 identity ---'
printf 'comm='; cat /proc/1/comm 2>&1
printf 'cmdline='; tr '\000' ' ' < /proc/1/cmdline 2>/dev/null; echo
printf 'exe='; readlink -f /proc/1/exe 2>&1
ls -li /sbin/init /bin/busybox /bin/busybox.* /sbin/busybox /sbin/busybox.* 2>&1
sha256sum /sbin/init 2>&1
file /sbin/init 2>&1
echo '--- embedded BusyBox/version/reload strings ---'
if command -v strings >/dev/null 2>&1; then
    strings /sbin/init 2>&1 | grep -E -i 'BusyBox v|inittab|SIGHUP|HUP|reload' | head -n 80
    echo "strings_scan_exit=$?"
else
    echo 'strings=NOT AVAILABLE'
fi
echo '--- inittab syntax evidence ---'
sed -n '1,80p' /etc/inittab 2>&1
echo '--- signal names ---'
kill -l 2>&1
echo "remote_completed_at=$(date -Ins 2>/dev/null)"
'@

$Started = Get-Date -Format o
$PreviousOutputEncoding = $OutputEncoding
$PreviousErrorAction = $ErrorActionPreference
$OutputEncoding = [Text.ASCIIEncoding]::new()
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
[IO.File]::WriteAllText($PublicPath, $Redacted + "`r`n", [Text.UTF8Encoding]::new($false))
@(
    "raw_sha256=$((Get-FileHash -LiteralPath $RawPath -Algorithm SHA256).Hash.ToLowerInvariant())"
    "redacted_sha256=$((Get-FileHash -LiteralPath $PublicPath -Algorithm SHA256).Hash.ToLowerInvariant())"
    'redaction_method=exact target endpoint replacement;all IPv4 literals replaced'
) | Set-Content -LiteralPath $TracePath -Encoding utf8
"BOARD_BUSYBOX_IDENTIFICATION_EXIT=$ExitCode"
exit $ExitCode
