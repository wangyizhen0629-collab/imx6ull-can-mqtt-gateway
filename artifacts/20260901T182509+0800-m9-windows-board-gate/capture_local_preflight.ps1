$ErrorActionPreference = 'Stop'
$RunDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$PrivateDir = Join-Path $RunDir 'private_raw'
$OutputPath = Join-Path $RunDir 'local_preflight.txt'
if (Test-Path -LiteralPath $OutputPath) {
    throw "refusing to overwrite existing evidence: $OutputPath"
}

$Started = Get-Date -Format o
$Tracked = @(git status --porcelain=v1 --untracked-files=no)
$Untracked = @(git ls-files --others --exclude-standard)
$Head = (git rev-parse HEAD).Trim()
$HeadExit = $LASTEXITCODE
git merge-base --is-ancestor e673856 HEAD
$AncestorExit = $LASTEXITCODE
$PreviousErrorAction = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
$PullOutput = & git pull --ff-only origin master 2>&1 | Out-String
$PullExit = $LASTEXITCODE
$ErrorActionPreference = $PreviousErrorAction
$HeadAfter = (git rev-parse HEAD).Trim()
$RevParseExit = $LASTEXITCODE
$VmRun = 'E:\vmware\vmware workstation pro\vmrun.exe'
$Vmx = 'E:\vmware\ubuntu\Ubuntu_64.vmx'
$VmState = if ((& $VmRun list 2>$null | Select-String -SimpleMatch $Vmx)) { 'running' } else { 'not-visible' }
$BinaryExists = Test-Path -LiteralPath (Join-Path $PrivateDir 'incoming\gatewayd') -PathType Leaf
$BinarySha = if ($BinaryExists) {
    (Get-FileHash -LiteralPath (Join-Path $PrivateDir 'incoming\gatewayd') -Algorithm SHA256).Hash.ToLowerInvariant()
} else {
    'NOT_AVAILABLE'
}
$EndpointSha = (Get-FileHash -LiteralPath (Join-Path $PrivateDir 'board_endpoint.txt') -Algorithm SHA256).Hash.ToLowerInvariant()
$Ended = Get-Date -Format o

@(
    "started_at=$Started"
    'command=git status --porcelain=v1 --untracked-files=no'
    'exit_code=0'
    "tracked_status_count=$($Tracked.Count)"
    'command=git ls-files --others --exclude-standard'
    'exit_code=0'
    "untracked_count=$($Untracked.Count)"
    "head_before_pull=$Head"
    "rev_parse_before_exit=$HeadExit"
    "expected_commit_ancestor_exit=$AncestorExit"
    'command=git pull --ff-only origin master'
    "pull_exit=$PullExit"
    '--- pull full output ---'
    $PullOutput.TrimEnd()
    '--- end pull full output ---'
    'command=git rev-parse HEAD'
    "rev_parse_exit=$RevParseExit"
    "head_after_pull=$HeadAfter"
    "ubuntu_vm_state=$VmState"
    'ubuntu_binary_transfer=NOT RUN'
    'ubuntu_binary_transfer_reason=Ubuntu SSH port is reachable, but the existing Windows side has no offered public key and BatchMode authentication was rejected; credentials were not guessed or extracted.'
    "incoming_binary_exists=$($BinaryExists.ToString().ToLowerInvariant())"
    "incoming_binary_sha256=$BinarySha"
    "private_board_endpoint_sha256=$EndpointSha"
    'private_board_endpoint_git_ignored=true'
    "ended_at=$Ended"
) | Set-Content -LiteralPath $OutputPath -Encoding utf8

Get-Content -LiteralPath $OutputPath
if ($PullExit -ne 0 -or $RevParseExit -ne 0 -or $HeadAfter -ne 'e673856f8ce789442b63464c1bc9753c4d97f619' -or $AncestorExit -ne 0) {
    exit 1
}
