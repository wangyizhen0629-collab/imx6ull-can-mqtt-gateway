$ErrorActionPreference = 'Stop'
$RunDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$PrivateDir = Join-Path $RunDir 'private_raw'
$EndpointFile = Join-Path $PrivateDir 'board_endpoint.txt'
$RawPath = Join-Path $PrivateDir 'board_final_readonly_audit.txt'
$PublicPath = Join-Path $RunDir 'board_final_readonly_audit.redacted.txt'
$TracePath = Join-Path $RunDir 'board_final_readonly_audit.redaction-trace.txt'
foreach ($Path in @($RawPath, $PublicPath, $TracePath)) {
    if (Test-Path -LiteralPath $Path) {
        throw "refusing to overwrite existing evidence: $Path"
    }
}
$BoardHost = ((Get-Content -LiteralPath $EndpointFile -Raw) -split '=', 2)[1].Trim()
$RemoteScript = @'
true
echo "remote_started_at=$(date -Ins 2>/dev/null)"
echo '--- m9 stage absence ---'
if [ -e /var/lib/m9-windows-board-gate-20260901T182509 ] || [ -e /tmp/m9-windows-board-gate-20260901T182509 ]; then
    echo 'm9_stage=UNEXPECTED_PRESENT'
else
    echo 'm9_stage=ABSENT'
fi
echo '--- process identity ---'
supervisor_count=0
gatewayd_count=0
for p in /proc/[0-9]*; do
    [ -r "$p/cmdline" ] || continue
    cmd=$(tr '\000' ' ' < "$p/cmdline" 2>/dev/null)
    comm=$(sed -n '1p' "$p/comm" 2>/dev/null)
    case "$cmd" in
        *'/etc/init.d/gatewayd supervise'*) supervisor_count=$((supervisor_count + 1));;
    esac
    if [ "$comm" = gatewayd ]; then gatewayd_count=$((gatewayd_count + 1)); fi
    case "$cmd" in
        *gatewayd*)
            pid=${p#/proc/}
            exe=$(readlink -f "$p/exe" 2>/dev/null)
            printf 'pid=%s comm=%s cmdline=%s exe=%s\n' "$pid" "$comm" "$cmd" "$exe"
            [ -f "$exe" ] && sha256sum "$exe" 2>/dev/null
            ;;
    esac
done
echo "supervisor_count=$supervisor_count"
echo "gatewayd_count=$gatewayd_count"
echo '--- managed files final ---'
for f in /etc/inittab /etc/init.d/gatewayd /etc/default/gatewayd /etc/gatewayd/gateway.conf /usr/bin/gatewayd; do
    if [ -e "$f" ]; then
        stat -c 'mode=%a uid=%u gid=%g size=%s path=%n' "$f" 2>&1
        sha256sum "$f" 2>&1
    else
        echo "MISSING $f"
    fi
done
echo '--- pid1 final ---'
printf 'pid1_comm='; cat /proc/1/comm 2>&1
printf 'pid1_cmdline='; tr '\000' ' ' < /proc/1/cmdline 2>/dev/null; echo
printf 'pid1_exe='; readlink -f /proc/1/exe 2>&1
echo '--- can0 final read-only ---'
ip -details -statistics link show can0 2>&1
echo '--- process table final ---'
ps w 2>&1
echo "remote_completed_at=$(date -Ins 2>/dev/null)"
'@
$Started = Get-Date -Format o
$Psi = New-Object Diagnostics.ProcessStartInfo
$Psi.FileName = (Get-Command ssh.exe).Source
$Psi.Arguments = "-o BatchMode=yes -o ConnectTimeout=10 root@$BoardHost sh"
$Psi.UseShellExecute = $false
$Psi.CreateNoWindow = $true
$Psi.RedirectStandardInput = $true
$Psi.RedirectStandardOutput = $true
$Psi.RedirectStandardError = $true
$Process = New-Object Diagnostics.Process
$Process.StartInfo = $Psi
if (-not $Process.Start()) { throw 'failed to start ssh process' }
$Bytes = [Text.Encoding]::ASCII.GetBytes($RemoteScript)
$Process.StandardInput.BaseStream.Write($Bytes, 0, $Bytes.Length)
$Process.StandardInput.Close()
$Stdout = $Process.StandardOutput.ReadToEnd()
$Stderr = $Process.StandardError.ReadToEnd()
$Process.WaitForExit()
$ExitCode = $Process.ExitCode
$Ended = Get-Date -Format o
$Raw = @(
    "started_at=$Started"
    'command=ssh -o BatchMode=yes -o ConnectTimeout=10 root@<PRIVATE> sh'
    "exit_code=$ExitCode"
    "ended_at=$Ended"
    '--- stdout full output ---'
    $Stdout.TrimEnd()
    '--- stderr full output ---'
    $Stderr.TrimEnd()
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
"BOARD_FINAL_AUDIT_EXIT=$ExitCode"
exit $ExitCode
