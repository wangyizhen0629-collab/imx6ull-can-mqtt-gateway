param(
    [Parameter(Mandatory = $true)][string]$ScriptPath,
    [Parameter(Mandatory = $true)][string]$EvidenceName,
    [ValidateSet('/bin/sh', '/bin/ash')][string]$Shell = '/bin/sh'
)

$ErrorActionPreference = 'Stop'
$RunDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$PrivateDir = Join-Path $RunDir 'private_raw'
$EndpointFile = Join-Path $PrivateDir 'board_endpoint.txt'
$UserFile = Join-Path $PrivateDir 'board_user.txt'
$RawPath = Join-Path $PrivateDir "$EvidenceName.txt"
$PublicPath = Join-Path $RunDir "$EvidenceName.redacted.txt"
$TracePath = Join-Path $RunDir "$EvidenceName.redaction-trace.txt"

foreach ($Path in @($RawPath, $PublicPath, $TracePath)) {
    if (Test-Path -LiteralPath $Path) {
        throw "refusing to overwrite existing evidence: $Path"
    }
}
if (-not (Test-Path -LiteralPath $EndpointFile -PathType Leaf)) {
    throw "missing private endpoint file: $EndpointFile"
}
if (-not (Test-Path -LiteralPath $ScriptPath -PathType Leaf)) {
    throw "missing remote script: $ScriptPath"
}

$BoardHost = ((Get-Content -LiteralPath $EndpointFile -Raw) -split '=', 2)[1].Trim()
$BoardUser = ((Get-Content -LiteralPath $UserFile -Raw) -split '=', 2)[1].Trim()
$RemoteScript = "true`n" + (Get-Content -LiteralPath $ScriptPath -Raw -Encoding UTF8)
$Started = Get-Date -Format o
$Psi = New-Object Diagnostics.ProcessStartInfo
$Psi.FileName = (Get-Command ssh.exe).Source
$Psi.Arguments = "-o BatchMode=yes -o ConnectTimeout=10 $BoardUser@$BoardHost `"tail -c +4 | $Shell`""
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
    "command=ssh -o BatchMode=yes -o ConnectTimeout=10 <REDACTED_USER>@<PRIVATE> tail -c +4 pipe $Shell < $(Split-Path -Leaf $ScriptPath)"
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
$Redacted = [regex]::Replace($Redacted, '(?im)^(\s*(?:broker_(?:host|username|password)|password|passwd)\s*=).+$', '$1<REDACTED>')
[IO.File]::WriteAllText($PublicPath, $Redacted + "`r`n", [Text.UTF8Encoding]::new($false))

$Trace = @(
    "raw_sha256=$((Get-FileHash -LiteralPath $RawPath -Algorithm SHA256).Hash.ToLowerInvariant())"
    "redacted_sha256=$((Get-FileHash -LiteralPath $PublicPath -Algorithm SHA256).Hash.ToLowerInvariant())"
    'redaction_method=exact target endpoint replacement;all IPv4 literals replaced;credential assignments replaced'
) -join "`r`n"
[IO.File]::WriteAllText($TracePath, $Trace + "`r`n", [Text.UTF8Encoding]::new($false))

"REMOTE_EVIDENCE=$EvidenceName EXIT=$ExitCode"
exit $ExitCode
