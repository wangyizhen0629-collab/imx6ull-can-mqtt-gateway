$ErrorActionPreference = 'Stop'
$RunDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$PrivateDir = Join-Path $RunDir 'private_raw'
$Utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$BrokerOut = Join-Path $RunDir 'broker-logs-redacted-v2'
$BoardOut = Join-Path $RunDir 'board-evidence-redacted-v2'
$TracePath = Join-Path $RunDir 'public_redaction_v2_trace.txt'
$ScanPath = Join-Path $RunDir 'public_redaction_v2_scan.txt'
foreach ($Path in @($BrokerOut, $BoardOut, $TracePath, $ScanPath)) {
    if (Test-Path -LiteralPath $Path) { throw "refusing to overwrite v2 redaction evidence: $Path" }
}
$PrivateIpv4 = '(?<![0-9])(?:10\.(?:[0-9]{1,3}\.){2}[0-9]{1,3}|192\.168\.(?:[0-9]{1,3}\.)[0-9]{1,3}|172\.(?:1[6-9]|2[0-9]|3[01])\.(?:[0-9]{1,3}\.)[0-9]{1,3})(?![0-9])'
$Trace = New-Object System.Collections.Generic.List[string]

New-Item -ItemType Directory -Path $BrokerOut | Out-Null
$Phases = @(
    @{ Name='baseline'; Directory='windows-baseline' },
    @{ Name='reconnect1'; Directory='windows-reconnect1' },
    @{ Name='crash-recovery'; Directory='windows-crash-recovery' }
)
foreach ($Phase in $Phases) {
    foreach ($Name in @('broker.stdout.log', 'broker.stderr.log')) {
        $RawPath = Join-Path $PrivateDir "$($Phase.Directory)/$Name"
        $PublicName = "$($Phase.Name)-$Name"
        $PublicPath = Join-Path $BrokerOut $PublicName
        $Content = [System.IO.File]::ReadAllText($RawPath)
        $Redacted = [regex]::Replace($Content, $PrivateIpv4, '<REDACTED_LAN_ADDRESS>')
        [System.IO.File]::WriteAllText($PublicPath, $Redacted, $Utf8NoBom)
        $Trace.Add("$PublicName raw_sha256=$((Get-FileHash -LiteralPath $RawPath -Algorithm SHA256).Hash.ToLowerInvariant()) redacted_sha256=$((Get-FileHash -LiteralPath $PublicPath -Algorithm SHA256).Hash.ToLowerInvariant())")
    }
}

$BoardRaw = Join-Path $PrivateDir 'board-export/extracted/gatewayd-m8-test-20260901T170636/evidence'
New-Item -ItemType Directory -Path $BoardOut | Out-Null
foreach ($RawFile in @(Get-ChildItem -LiteralPath $BoardRaw -File | Sort-Object Name)) {
    $PublicPath = Join-Path $BoardOut $RawFile.Name
    $Content = [System.IO.File]::ReadAllText($RawFile.FullName)
    $Redacted = [regex]::Replace($Content, $PrivateIpv4, '<REDACTED_LAN_ADDRESS>')
    [System.IO.File]::WriteAllText($PublicPath, $Redacted, $Utf8NoBom)
    $Trace.Add("board/$($RawFile.Name) raw_sha256=$((Get-FileHash -LiteralPath $RawFile.FullName -Algorithm SHA256).Hash.ToLowerInvariant()) redacted_sha256=$((Get-FileHash -LiteralPath $PublicPath -Algorithm SHA256).Hash.ToLowerInvariant())")
}
$Trace.Add('redaction_method=使用边界限定正则将所有 RFC1918 IPv4 地址替换为 <REDACTED_LAN_ADDRESS>；原始文件仅保存在 private_raw')
[System.IO.File]::WriteAllLines($TracePath, $Trace, $Utf8NoBom)

$Files = @(Get-ChildItem -LiteralPath $BrokerOut,$BoardOut -File)
$Matches = New-Object System.Collections.Generic.List[string]
foreach ($File in $Files) {
    $Content = [System.IO.File]::ReadAllText($File.FullName)
    if ([regex]::IsMatch($Content, $PrivateIpv4)) { $Matches.Add($File.FullName) }
}
@(
    "scanned_at=$((Get-Date).ToString('o'))"
    "file_count=$($Files.Count)"
    "rfc1918_match_count=$($Matches.Count)"
    "result=$(if($Matches.Count -eq 0){'PASS'}else{'FAIL'})"
) | Out-File -LiteralPath $ScanPath -Encoding utf8
if ($Matches.Count -ne 0) { throw 'RFC1918 address remains in v2 public evidence' }
Get-Content -LiteralPath $ScanPath
