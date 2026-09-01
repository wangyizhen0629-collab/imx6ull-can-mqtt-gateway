param(
    [Parameter(Mandatory = $true)][ValidateSet('broker','subscriber')][string]$Kind,
    [Parameter(Mandatory = $true)][string]$Phase
)
$ErrorActionPreference = 'Stop'
$RunDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$PrivateDir = Join-Path $RunDir 'private_raw'
$PublicPhaseDir = Join-Path $RunDir "windows-$Phase"
$PrivatePhaseDir = Join-Path $PrivateDir "windows-$Phase"
New-Item -ItemType Directory -Path $PublicPhaseDir,$PrivatePhaseDir -Force | Out-Null

if ($Kind -eq 'broker') {
    $Executable = 'E:\mosquitto\mosquitto.exe'
    $Arguments = @('-c', (Join-Path $PrivateDir 'mosquitto.conf'))
}
else {
    $Endpoint = @{}
    Get-Content -LiteralPath (Join-Path $PrivateDir 'broker_endpoint.txt') | ForEach-Object {
        $Pair = $_ -split '=', 2
        if ($Pair.Count -eq 2) { $Endpoint[$Pair[0]] = $Pair[1] }
    }
    $Executable = 'E:\mosquitto\mosquitto_sub.exe'
    $Arguments = @('-h', $Endpoint.broker_bind, '-p', '18884', '-q', '1',
        '-t', 'test/m8/20260901T170636/reactor',
        '-i', 'm8-win-sub-20260901T170636', '-F', '%p')
}

$Stdout = Join-Path $PrivatePhaseDir "$Kind.stdout.log"
$Stderr = Join-Path $PrivatePhaseDir "$Kind.stderr.log"
$Lifecycle = Join-Path $PublicPhaseDir "$Kind.lifecycle.txt"
@("wrapper_started_at=$((Get-Date).ToString('o'))", "kind=$Kind", "phase=$Phase") |
    Out-File -LiteralPath $Lifecycle -Encoding utf8
$Process = Start-Process -FilePath $Executable -ArgumentList $Arguments -PassThru `
    -WindowStyle Hidden -RedirectStandardOutput $Stdout -RedirectStandardError $Stderr
"pid=$($Process.Id)" | Out-File -LiteralPath (Join-Path $PublicPhaseDir "$Kind.pid.txt") -Encoding ascii
"process_started_at=$((Get-Date).ToString('o'))" | Out-File -LiteralPath $Lifecycle -Encoding utf8 -Append
$Process.WaitForExit()
"exit_code=$($Process.ExitCode)" | Out-File -LiteralPath (Join-Path $PublicPhaseDir "$Kind.exit.txt") -Encoding ascii
"process_ended_at=$((Get-Date).ToString('o'))" | Out-File -LiteralPath $Lifecycle -Encoding utf8 -Append

