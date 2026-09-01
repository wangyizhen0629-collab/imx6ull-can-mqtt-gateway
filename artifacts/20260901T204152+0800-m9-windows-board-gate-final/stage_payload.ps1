$ErrorActionPreference = 'Stop'
$RunDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$PrivateDir = Join-Path $RunDir 'private_raw'
$Incoming = Join-Path $PrivateDir 'incoming'
$EndpointFile = Join-Path $PrivateDir 'board_endpoint.txt'
$UserFile = Join-Path $PrivateDir 'board_user.txt'
$RawPath = Join-Path $PrivateDir 'stage_payload.txt'
$PublicPath = Join-Path $RunDir 'stage_payload.redacted.txt'
$TracePath = Join-Path $RunDir 'stage_payload.redaction-trace.txt'
foreach ($Path in @($RawPath, $PublicPath, $TracePath)) {
    if (Test-Path -LiteralPath $Path) { throw "refusing to overwrite existing evidence: $Path" }
}
$BoardHost = ((Get-Content -LiteralPath $EndpointFile -Raw) -split '=', 2)[1].Trim()
$BoardUser = ((Get-Content -LiteralPath $UserFile -Raw) -split '=', 2)[1].Trim()
$Stage = '/tmp/m9-board-gate-20260901T204152p0800'
$Lines = New-Object Collections.Generic.List[string]
$Lines.Add("started_at=$((Get-Date).ToString('o'))")

$CreateOutput = & ssh.exe -o BatchMode=yes -o ConnectTimeout=10 "$BoardUser@$BoardHost" "test ! -e '$Stage' && mkdir -p '$Stage/incoming' && chmod 700 '$Stage' '$Stage/incoming'" 2>&1 | Out-String
$CreateExit = $LASTEXITCODE
$Lines.Add('command=ssh <REDACTED_USER>@<PRIVATE> test unique stage absent; mkdir/chmod target non-system stage')
$Lines.Add("exit_code=$CreateExit")
$Lines.Add($CreateOutput.TrimEnd())
if ($CreateExit -ne 0) { throw 'target unique stage creation failed' }

$Transfers = @(
    @{ Local = (Join-Path $Incoming 'gatewayd'); Remote = "$Stage/incoming/gatewayd" },
    @{ Local = (Join-Path $Incoming 'gatewayd.respawn'); Remote = "$Stage/incoming/gatewayd.respawn" },
    @{ Local = (Join-Path $Incoming 'test_gatewayd_supervisor.sh'); Remote = "$Stage/incoming/test_gatewayd_supervisor.sh" },
    @{ Local = (Join-Path $Incoming 'fake_gatewayd.sh'); Remote = "$Stage/incoming/fake_gatewayd.sh" },
    @{ Local = (Join-Path $Incoming 'gateway.v2.conf'); Remote = "$Stage/incoming/gateway.v2.conf" },
    @{ Local = (Join-Path $Incoming 'gatewayd.v2.env'); Remote = "$Stage/incoming/gatewayd.v2.env" },
    @{ Local = (Join-Path $RunDir 'board_install.sh'); Remote = "$Stage/incoming/board_install.sh" },
    @{ Local = (Join-Path $RunDir 'board_rollback.sh'); Remote = "$Stage/incoming/board_rollback.sh" }
)
foreach ($Transfer in $Transfers) {
    if (-not (Test-Path -LiteralPath $Transfer.Local -PathType Leaf)) { throw "missing local payload: $($Transfer.Local)" }
    $LocalHash = (Get-FileHash -LiteralPath $Transfer.Local -Algorithm SHA256).Hash.ToLowerInvariant()
    $Output = & scp.exe -p -o BatchMode=yes -o ConnectTimeout=10 $Transfer.Local "${BoardUser}@${BoardHost}:$($Transfer.Remote)" 2>&1 | Out-String
    $Exit = $LASTEXITCODE
    $Lines.Add("command=scp $(Split-Path -Leaf $Transfer.Local) <REDACTED_USER>@<PRIVATE>:$($Transfer.Remote) local_sha256=$LocalHash")
    $Lines.Add("exit_code=$Exit")
    $Lines.Add($Output.TrimEnd())
    if ($Exit -ne 0) { throw "SCP failed: $($Transfer.Local)" }
}

$AuditOutput = & ssh.exe -o BatchMode=yes -o ConnectTimeout=10 "$BoardUser@$BoardHost" "chmod 700 '$Stage/incoming/board_install.sh' '$Stage/incoming/board_rollback.sh'; chmod 755 '$Stage/incoming/gatewayd' '$Stage/incoming/test_gatewayd_supervisor.sh' '$Stage/incoming/fake_gatewayd.sh'; chmod 600 '$Stage/incoming/gateway.v2.conf'; chmod 644 '$Stage/incoming/gatewayd.v2.env' '$Stage/incoming/gatewayd.respawn'; stat -c 'mode=%a uid=%u gid=%g size=%s path=%n' '$Stage'/incoming/*; sha256sum '$Stage'/incoming/*" 2>&1 | Out-String
$AuditExit = $LASTEXITCODE
$Lines.Add('command=ssh <REDACTED_USER>@<PRIVATE> set staging modes; stat and SHA256 all payload files')
$Lines.Add("exit_code=$AuditExit")
$Lines.Add($AuditOutput.TrimEnd())
if ($AuditExit -ne 0) { throw 'target staging audit failed' }
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
"STAGE_PAYLOAD_EXIT=$AuditExit"
exit $AuditExit
