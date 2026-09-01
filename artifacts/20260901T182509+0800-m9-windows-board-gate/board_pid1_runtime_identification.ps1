$ErrorActionPreference = 'Stop'
$RunDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$PrivateDir = Join-Path $RunDir 'private_raw'
$EndpointFile = Join-Path $PrivateDir 'board_endpoint.txt'
$RawPath = Join-Path $PrivateDir 'board_pid1_runtime_identification.txt'
$PublicPath = Join-Path $RunDir 'board_pid1_runtime_identification.redacted.txt'
$TracePath = Join-Path $RunDir 'board_pid1_runtime_identification.redaction-trace.txt'
foreach ($Path in @($RawPath, $PublicPath, $TracePath)) {
    if (Test-Path -LiteralPath $Path) {
        throw "refusing to overwrite existing evidence: $Path"
    }
}
$BoardHost = ((Get-Content -LiteralPath $EndpointFile -Raw) -split '=', 2)[1].Trim()

$RemoteScript = @'
set +e
echo "remote_started_at=$(date -Ins 2>/dev/null)"
echo '--- pid1 exact identity ---'
printf 'comm='; cat /proc/1/comm 2>&1
printf 'cmdline='; tr '\000' ' ' < /proc/1/cmdline 2>/dev/null; echo
printf 'exe='; readlink -f /proc/1/exe 2>&1
stat -c 'mode=%a uid=%u gid=%g inode=%i size=%s path=%n' /sbin/init 2>&1
sha256sum /sbin/init 2>&1
file /sbin/init 2>&1
ldd /sbin/init 2>&1
echo '--- related applets and libraries ---'
for f in /bin/sh /bin/ash /bin/ls /bin/sed /bin/grep /sbin/init /lib/libbusybox* /usr/lib/libbusybox*; do
    [ -e "$f" ] || continue
    ls -li "$f" 2>&1
    file "$f" 2>&1
    sha256sum "$f" 2>&1
done
echo '--- complete init strings ---'
if command -v strings >/dev/null 2>&1; then
    strings /sbin/init 2>&1
else
    echo 'strings=NOT AVAILABLE'
fi
echo '--- libbusybox version and init/reload strings ---'
for f in /lib/libbusybox* /usr/lib/libbusybox*; do
    [ -f "$f" ] || continue
    echo "library=$f"
    strings "$f" 2>&1 | grep -E -i 'BusyBox v|Usage: init|inittab|SIGHUP|re-read|reload|respawn' | head -n 160
    echo "library_scan_exit=$?"
done
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
"BOARD_PID1_RUNTIME_IDENTIFICATION_EXIT=$ExitCode"
exit $ExitCode
