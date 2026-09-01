$ErrorActionPreference = 'Stop'
$RunDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$PrivateDir = Join-Path $RunDir 'private_raw'
$EndpointFile = Join-Path $PrivateDir 'board_endpoint.txt'
$UserFile = Join-Path $PrivateDir 'board_user.txt'
$PreRebootEvidence = Join-Path $RunDir 'board_pre_reboot.redacted.txt'
$RawPath = Join-Path $PrivateDir 'reboot_recovery_poll.txt'
$PublicPath = Join-Path $RunDir 'reboot_recovery_poll.redacted.txt'
$TracePath = Join-Path $RunDir 'reboot_recovery_poll.redaction-trace.txt'
foreach ($Path in @($RawPath, $PublicPath, $TracePath)) {
    if (Test-Path -LiteralPath $Path) { throw "refusing to overwrite existing evidence: $Path" }
}
$BoardHost = ((Get-Content -LiteralPath $EndpointFile -Raw) -split '=', 2)[1].Trim()
$BoardUser = ((Get-Content -LiteralPath $UserFile -Raw) -split '=', 2)[1].Trim()
$PreText = Get-Content -LiteralPath $PreRebootEvidence -Raw
if ($PreText -notmatch '(?m)^boot_id_before=([0-9a-f-]+)$') { throw 'pre-reboot boot id unavailable' }
$BeforeBootId = $Matches[1]
$Lines = New-Object Collections.Generic.List[string]
$Lines.Add("started_at=$((Get-Date).ToString('o'))")
$Lines.Add('mode=recovery poll only; NO reboot command present or executed')
$Lines.Add("boot_id_before=$BeforeBootId")
$Changed = $false
$AfterBootId = ''

for ($Attempt = 1; $Attempt -le 48; $Attempt++) {
    $ProbeTime = Get-Date -Format o
    $PreviousErrorAction = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    $ProbeOutput = & ssh.exe -o BatchMode=yes -o ConnectTimeout=5 "$BoardUser@$BoardHost" 'cat /proc/sys/kernel/random/boot_id; cat /proc/uptime; date -Ins; cat /proc/1/comm' 2>&1 | Out-String
    $ProbeExit = $LASTEXITCODE
    $ErrorActionPreference = $PreviousErrorAction
    $Lines.Add("probe=$Attempt local_time=$ProbeTime exit_code=$ProbeExit")
    $Lines.Add($ProbeOutput.TrimEnd())
    if ($ProbeExit -eq 0) {
        $Candidate = (($ProbeOutput -split "`r?`n")[0]).Trim()
        if ($Candidate -and $Candidate -ne $BeforeBootId) {
            $AfterBootId = $Candidate
            $Changed = $true
            break
        }
    }
    Start-Sleep -Seconds 5
}
$Lines.Add("boot_id_after=$AfterBootId")
$Lines.Add("boot_id_changed=$($Changed.ToString().ToLowerInvariant())")
$Lines.Add("ended_at=$((Get-Date).ToString('o'))")

$Raw = $Lines -join "`r`n"
[IO.File]::WriteAllText($RawPath, $Raw + "`r`n", [Text.UTF8Encoding]::new($false))
$Redacted = $Raw.Replace($BoardHost, '<REDACTED_BOARD_HOST>')
$Redacted = [regex]::Replace($Redacted, '(?<![0-9])(?:[0-9]{1,3}\.){3}[0-9]{1,3}(?![0-9])', '<REDACTED_IPV4>')
[IO.File]::WriteAllText($PublicPath, $Redacted + "`r`n", [Text.UTF8Encoding]::new($false))
$Trace = @(
    "raw_sha256=$((Get-FileHash -LiteralPath $RawPath -Algorithm SHA256).Hash.ToLowerInvariant())"
    "redacted_sha256=$((Get-FileHash -LiteralPath $PublicPath -Algorithm SHA256).Hash.ToLowerInvariant())"
    'redaction_method=exact target endpoint replacement;all IPv4 literals replaced'
) -join "`r`n"
[IO.File]::WriteAllText($TracePath, $Trace + "`r`n", [Text.UTF8Encoding]::new($false))
"RECOVERY_ONLY=true"
"BOOT_ID_CHANGED=$Changed"
"BOOT_ID_BEFORE=$BeforeBootId"
"BOOT_ID_AFTER=$AfterBootId"
if (-not $Changed) { exit 1 }
