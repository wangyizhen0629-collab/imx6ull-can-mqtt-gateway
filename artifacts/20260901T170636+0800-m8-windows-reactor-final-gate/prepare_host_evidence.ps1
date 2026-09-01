$ErrorActionPreference = 'Stop'
$RunDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$PrivateDir = Join-Path $RunDir 'private_raw'
$Utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$SubscriberRaw = Join-Path $PrivateDir 'windows-capture/subscriber.stdout.log'
$SubscriberPublic = Join-Path $RunDir 'subscriber.jsonl'
$SubscriberTrace = Join-Path $RunDir 'subscriber.redaction-trace.txt'
$BrokerPublicDir = Join-Path $RunDir 'broker-logs-redacted'
$BrokerTrace = Join-Path $RunDir 'broker_logs_redaction_trace.txt'
$BrokerAccounting = Join-Path $RunDir 'broker_accounting.txt'
$BoardPublicDir = Join-Path $RunDir 'board-evidence-redacted'
$BoardTrace = Join-Path $RunDir 'board_evidence_redaction_trace.txt'
$PrivateManifest = Join-Path $RunDir 'raw_private_sha256.txt'

foreach ($Path in @($SubscriberPublic, $SubscriberTrace, $BrokerPublicDir, $BrokerTrace,
        $BrokerAccounting, $BoardPublicDir, $BoardTrace, $PrivateManifest)) {
    if (Test-Path -LiteralPath $Path) { throw "refusing to overwrite host evidence: $Path" }
}

$BoardHost = ((Get-Content (Join-Path $PrivateDir 'board_endpoint.txt') -Raw) -split '=', 2)[1].Trim()
$BrokerHost = ((Get-Content (Join-Path $PrivateDir 'broker_endpoint.txt') -Raw) -split '=', 2)[1].Trim()
$SensitiveValues = @($BoardHost, $BrokerHost) | Where-Object { $_ } | Select-Object -Unique

$SubscriberLines = @(Get-Content -LiteralPath $SubscriberRaw)
if ($SubscriberLines.Count -eq 0) { throw 'subscriber capture is empty' }
foreach ($Line in $SubscriberLines) {
    $Parsed = $Line | ConvertFrom-Json
    if ($Parsed.schema -ne 'gateway.telemetry.v1') { throw 'unexpected subscriber schema' }
    if ($Parsed.device_id -ne 'm8-gateway-20260901-170636') { throw 'unexpected subscriber device_id' }
}
[System.IO.File]::WriteAllLines($SubscriberPublic, $SubscriberLines, $Utf8NoBom)
@(
    "raw_sha256=$((Get-FileHash -LiteralPath $SubscriberRaw -Algorithm SHA256).Hash.ToLowerInvariant())"
    "public_sha256=$((Get-FileHash -LiteralPath $SubscriberPublic -Algorithm SHA256).Hash.ToLowerInvariant())"
    "raw_lines=$($SubscriberLines.Count)"
    'method=逐行 JSON 解析并以 UTF-8（无 BOM）重写；payload 本身不含 LAN 地址或凭据'
) | Out-File -LiteralPath $SubscriberTrace -Encoding utf8

New-Item -ItemType Directory -Path $BrokerPublicDir | Out-Null
$BrokerTraceLines = New-Object System.Collections.Generic.List[string]
$PhaseDefinitions = @(
    @{ Name = 'baseline'; Directory = 'windows-baseline' },
    @{ Name = 'reconnect1'; Directory = 'windows-reconnect1' },
    @{ Name = 'crash-recovery'; Directory = 'windows-crash-recovery' }
)
$Accounting = New-Object System.Collections.Generic.List[string]
$TotalGatewayPublish = 0
$TotalGatewayPuback = 0
$TotalSubscriberPublish = 0
$TotalSubscriberPuback = 0
foreach ($Phase in $PhaseDefinitions) {
    $RawDir = Join-Path $PrivateDir $Phase.Directory
    foreach ($Name in @('broker.stdout.log', 'broker.stderr.log')) {
        $RawPath = Join-Path $RawDir $Name
        $PublicName = "$($Phase.Name)-$Name"
        $PublicPath = Join-Path $BrokerPublicDir $PublicName
        $Content = [System.IO.File]::ReadAllText($RawPath)
        foreach ($Value in $SensitiveValues) { $Content = $Content.Replace($Value, '<REDACTED_LAN_ADDRESS>') }
        [System.IO.File]::WriteAllText($PublicPath, $Content, $Utf8NoBom)
        $BrokerTraceLines.Add("$PublicName raw_sha256=$((Get-FileHash -LiteralPath $RawPath -Algorithm SHA256).Hash.ToLowerInvariant()) redacted_sha256=$((Get-FileHash -LiteralPath $PublicPath -Algorithm SHA256).Hash.ToLowerInvariant())")
    }
    $BrokerLog = Join-Path $RawDir 'broker.stderr.log'
    $Lines = @(Get-Content -LiteralPath $BrokerLog)
    $GatewayPublish = @($Lines | Where-Object { $_ -like '*Received PUBLISH from gatewayd-m8-gateway-20260901-170636-*' }).Count
    $GatewayPuback = @($Lines | Where-Object { $_ -like '*Sending PUBACK to gatewayd-m8-gateway-20260901-170636-*' }).Count
    $SubscriberPublish = @($Lines | Where-Object { $_ -like '*Sending PUBLISH to m8-win-sub-20260901T170636*' }).Count
    $SubscriberPuback = @($Lines | Where-Object { $_ -like '*Received PUBACK from m8-win-sub-20260901T170636*' }).Count
    $Accounting.Add("$($Phase.Name)_gateway_received_publish=$GatewayPublish")
    $Accounting.Add("$($Phase.Name)_gateway_sent_puback=$GatewayPuback")
    $Accounting.Add("$($Phase.Name)_subscriber_sent_publish=$SubscriberPublish")
    $Accounting.Add("$($Phase.Name)_subscriber_received_puback=$SubscriberPuback")
    $TotalGatewayPublish += $GatewayPublish
    $TotalGatewayPuback += $GatewayPuback
    $TotalSubscriberPublish += $SubscriberPublish
    $TotalSubscriberPuback += $SubscriberPuback
}
$BrokerTraceLines.Add('redaction_method=将本次私有 board/Broker 地址精确替换为 <REDACTED_LAN_ADDRESS>；完整日志逐字保留')
[System.IO.File]::WriteAllLines($BrokerTrace, $BrokerTraceLines, $Utf8NoBom)
$Accounting.Add("total_gateway_received_publish=$TotalGatewayPublish")
$Accounting.Add("total_gateway_sent_puback=$TotalGatewayPuback")
$Accounting.Add("total_subscriber_sent_publish=$TotalSubscriberPublish")
$Accounting.Add("total_subscriber_received_puback=$TotalSubscriberPuback")
$Accounting.Add("subscriber_json_lines=$($SubscriberLines.Count)")
$Accounting.Add("gateway_vs_subscriber_difference=$($TotalGatewayPublish - $SubscriberLines.Count)")
if ($TotalGatewayPublish -ne $TotalGatewayPuback -or
    $TotalGatewayPublish -ne $TotalSubscriberPublish -or
    $TotalGatewayPublish -ne $TotalSubscriberPuback -or
    $TotalGatewayPublish -ne $SubscriberLines.Count) {
    $Accounting.Add('result=FAIL')
    [System.IO.File]::WriteAllLines($BrokerAccounting, $Accounting, $Utf8NoBom)
    throw 'Broker/gateway/subscriber accounting mismatch'
}
$Accounting.Add('result=PASS')
[System.IO.File]::WriteAllLines($BrokerAccounting, $Accounting, $Utf8NoBom)

$BoardEvidenceRoot = Join-Path $PrivateDir 'board-export/extracted/gatewayd-m8-test-20260901T170636/evidence'
if (-not (Test-Path -LiteralPath $BoardEvidenceRoot -PathType Container)) { throw 'extracted board evidence is absent' }
New-Item -ItemType Directory -Path $BoardPublicDir | Out-Null
$BoardTraceLines = New-Object System.Collections.Generic.List[string]
$BoardFiles = @(Get-ChildItem -LiteralPath $BoardEvidenceRoot -File | Sort-Object Name)
foreach ($RawFile in $BoardFiles) {
    $PublicPath = Join-Path $BoardPublicDir $RawFile.Name
    $Content = [System.IO.File]::ReadAllText($RawFile.FullName)
    foreach ($Value in $SensitiveValues) { $Content = $Content.Replace($Value, '<REDACTED_LAN_ADDRESS>') }
    [System.IO.File]::WriteAllText($PublicPath, $Content, $Utf8NoBom)
    $BoardTraceLines.Add("$($RawFile.Name) raw_sha256=$((Get-FileHash -LiteralPath $RawFile.FullName -Algorithm SHA256).Hash.ToLowerInvariant()) redacted_sha256=$((Get-FileHash -LiteralPath $PublicPath -Algorithm SHA256).Hash.ToLowerInvariant())")
}
$BoardTraceLines.Add('redaction_method=将本次私有 board/Broker 地址精确替换为 <REDACTED_LAN_ADDRESS>；spool/config 等私有文件不复制到公开目录')
[System.IO.File]::WriteAllLines($BoardTrace, $BoardTraceLines, $Utf8NoBom)

$PublicScanFiles = @($SubscriberPublic) + @(Get-ChildItem -LiteralPath $BrokerPublicDir -File).FullName + @(Get-ChildItem -LiteralPath $BoardPublicDir -File).FullName
foreach ($Path in $PublicScanFiles) {
    $Text = [System.IO.File]::ReadAllText($Path)
    foreach ($Value in $SensitiveValues) {
        if ($Text.Contains($Value)) { throw "sensitive LAN value remains in $Path" }
    }
}

$ManifestLines = New-Object System.Collections.Generic.List[string]
$PrivateFiles = @(Get-ChildItem -LiteralPath $PrivateDir -Recurse -File | Sort-Object FullName)
foreach ($File in $PrivateFiles) {
    $Relative = $File.FullName.Substring($PrivateDir.Length + 1).Replace('\', '/')
    $Hash = (Get-FileHash -LiteralPath $File.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    $ManifestLines.Add("$Hash  private_raw/$Relative")
}
[System.IO.File]::WriteAllLines($PrivateManifest, $ManifestLines, $Utf8NoBom)

Write-Output "subscriber_lines=$($SubscriberLines.Count)"
Write-Output "broker_accounted_batches=$TotalGatewayPublish"
Write-Output "board_evidence_files=$($BoardFiles.Count)"
Write-Output "private_manifest_files=$($PrivateFiles.Count)"
Write-Output 'M8_HOST_EVIDENCE_PREPARE_PASS'
