$ErrorActionPreference = 'Stop'
$RunDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$PrivateDir = Join-Path $RunDir 'private_raw'
$Incoming = Join-Path $PrivateDir 'incoming'
$SourceConfig = Join-Path $PrivateDir 'gateway.m8-source.conf'
$TargetConfig = Join-Path $Incoming 'gateway.v2.conf'
$EnvPath = Join-Path $Incoming 'gatewayd.v2.env'
$PrivateManifest = Join-Path $PrivateDir 'payload_manifest_v2.txt'
$PublicManifest = Join-Path $RunDir 'payload_manifest_v2.redacted.txt'
foreach ($Path in @($TargetConfig, $EnvPath, $PrivateManifest, $PublicManifest)) {
    if (Test-Path -LiteralPath $Path) { throw "refusing to overwrite existing evidence: $Path" }
}

$RequiredKeys = @('device_id','can_interface','broker_host','broker_port','broker_username','broker_password','mqtt_topic','queue_capacity','queue_push_timeout_ms','batch_interval_ms','mqtt_ack_timeout_ms','mqtt_reconnect_interval_ms','spool_path','log_level')
$SourceValues = @{}
$TargetValues = @{}
$Output = New-Object Collections.Generic.List[string]
foreach ($Line in [IO.File]::ReadAllLines($SourceConfig)) {
    if ($Line -notmatch '^\s*([^#=\s]+)\s*=(.*)$') {
        $Output.Add($Line)
        continue
    }
    $Key = $Matches[1]
    $Value = $Matches[2]
    if ($SourceValues.ContainsKey($Key)) { throw "duplicate source config key: $Key" }
    $SourceValues[$Key] = $Value
    if ($Key -eq 'device_id') {
        $NewValue = 'imx6ull-m9-20260901T204152p0800'
    } elseif ($Key -eq 'mqtt_topic') {
        $NewValue = 'm9/20260901T204152p0800/gateway'
    } elseif ($Key -eq 'spool_path') {
        $NewValue = '/var/lib/gatewayd/20260901T204152p0800-m9-board/spool.data'
    } else {
        $NewValue = $Value
    }
    $TargetValues[$Key] = $NewValue
    $Output.Add("$Key=$NewValue")
}
foreach ($Key in $RequiredKeys) {
    if (-not $SourceValues.ContainsKey($Key) -or -not $TargetValues.ContainsKey($Key)) { throw "missing config key: $Key" }
}
if ($SourceValues.Count -ne $RequiredKeys.Count -or $TargetValues.Count -ne $RequiredKeys.Count) { throw 'unexpected config keys present' }
foreach ($Key in @('can_interface','broker_host','broker_port','broker_username','broker_password','queue_capacity','queue_push_timeout_ms','batch_interval_ms','mqtt_ack_timeout_ms','mqtt_reconnect_interval_ms','log_level')) {
    if ($SourceValues[$Key] -cne $TargetValues[$Key]) { throw "unauthorized config value change: $Key" }
}
if ($TargetValues['can_interface'] -cne 'can0') { throw 'can_interface is not can0' }
[IO.File]::WriteAllText($TargetConfig, (($Output -join "`n") + "`n"), [Text.UTF8Encoding]::new($false))

$ResultCounts = @{}
foreach ($Line in [IO.File]::ReadAllLines($TargetConfig)) {
    if ($Line -match '^\s*([^#=\s]+)\s*=') {
        $Key = $Matches[1]
        if (-not $ResultCounts.ContainsKey($Key)) { $ResultCounts[$Key] = 0 }
        $ResultCounts[$Key]++
    }
}
foreach ($Key in $RequiredKeys) {
    if (-not $ResultCounts.ContainsKey($Key) -or $ResultCounts[$Key] -ne 1) { throw "target config key count invalid: $Key" }
}

$Env = @(
    'LD_LIBRARY_PATH=/opt/gatewayd/lib'
    'export LD_LIBRARY_PATH'
    'GATEWAYD_BIN=/opt/gatewayd/bin/gatewayd'
    'GATEWAYD_CONFIG=/etc/gatewayd/gateway.conf'
    'GATEWAYD_RUN_DIR=/var/run/gatewayd'
    'GATEWAYD_RESTART_LIMIT=5'
    'GATEWAYD_STABLE_SEC=60'
    'GATEWAYD_COOLDOWN_SEC=60'
    'GATEWAYD_STOP_TIMEOUT_SEC=15'
) -join "`n"
[IO.File]::WriteAllText($EnvPath, $Env + "`n", [Text.UTF8Encoding]::new($false))

$ConfigBytes = [IO.File]::ReadAllBytes($TargetConfig)
$EnvBytes = [IO.File]::ReadAllBytes($EnvPath)
$Lines = @(
    "created_at=$((Get-Date).ToString('o'))"
    'attempt=2 status=PASS'
    "source_config_sha256=$((Get-FileHash -LiteralPath $SourceConfig -Algorithm SHA256).Hash.ToLowerInvariant())"
    "target_config_sha256=$((Get-FileHash -LiteralPath $TargetConfig -Algorithm SHA256).Hash.ToLowerInvariant()) bytes=$($ConfigBytes.Length) cr=$(@($ConfigBytes | Where-Object { $_ -eq 13 }).Count) lf=$(@($ConfigBytes | Where-Object { $_ -eq 10 }).Count)"
    "target_env_sha256=$((Get-FileHash -LiteralPath $EnvPath -Algorithm SHA256).Hash.ToLowerInvariant()) bytes=$($EnvBytes.Length) cr=$(@($EnvBytes | Where-Object { $_ -eq 13 }).Count) lf=$(@($EnvBytes | Where-Object { $_ -eq 10 }).Count)"
    'target_config_key_count=14; each required key exactly once'
    'preserved_exactly=can_interface,broker_host,broker_port,broker_username,broker_password,queue and timeout settings,log_level'
    'changed_only=device_id,mqtt_topic,spool_path for unique M9 run'
    'private_values=NOT PRINTED'
)
[IO.File]::WriteAllLines($PrivateManifest, $Lines, [Text.UTF8Encoding]::new($false))
[IO.File]::WriteAllLines($PublicManifest, $Lines, [Text.UTF8Encoding]::new($false))
Get-Content -LiteralPath $PublicManifest
