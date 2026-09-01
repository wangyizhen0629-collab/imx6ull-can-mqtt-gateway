$ErrorActionPreference = 'Stop'
$RunDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = (Resolve-Path (Join-Path $RunDir '..\..')).Path
$PrivateDir = Join-Path $RunDir 'private_raw'
$Incoming = Join-Path $PrivateDir 'incoming'
$PublicManifest = Join-Path $RunDir 'payload_manifest.redacted.txt'
$PrivateManifest = Join-Path $PrivateDir 'payload_manifest.txt'
foreach ($Path in @($PublicManifest, $PrivateManifest)) {
    if (Test-Path -LiteralPath $Path) { throw "refusing to overwrite existing evidence: $Path" }
}

$Mappings = @(
    @{ Source = 'deploy/init.d/gatewayd'; Destination = 'gatewayd' },
    @{ Source = 'deploy/inittab/gatewayd.respawn'; Destination = 'gatewayd.respawn' },
    @{ Source = 'deploy/tests/test_gatewayd_supervisor.sh'; Destination = 'test_gatewayd_supervisor.sh' },
    @{ Source = 'deploy/tests/fixtures/fake_gatewayd.sh'; Destination = 'fake_gatewayd.sh' }
)
$Manifest = New-Object Collections.Generic.List[string]
$Manifest.Add("created_at=$((Get-Date).ToString('o'))")
$Manifest.Add('normalization=repository working-tree CRLF converted to target LF; UTF-8 without BOM')
foreach ($Mapping in $Mappings) {
    $Source = Join-Path $RepoRoot $Mapping.Source
    $Destination = Join-Path $Incoming $Mapping.Destination
    if (Test-Path -LiteralPath $Destination) { throw "refusing to overwrite: $Destination" }
    $Text = [IO.File]::ReadAllText($Source)
    $Normalized = $Text.Replace("`r`n", "`n").Replace("`r", "`n")
    [IO.File]::WriteAllText($Destination, $Normalized, [Text.UTF8Encoding]::new($false))
    $SourceHash = (Get-FileHash -LiteralPath $Source -Algorithm SHA256).Hash.ToLowerInvariant()
    $TargetHash = (Get-FileHash -LiteralPath $Destination -Algorithm SHA256).Hash.ToLowerInvariant()
    $Bytes = [IO.File]::ReadAllBytes($Destination)
    $CrCount = @($Bytes | Where-Object { $_ -eq 13 }).Count
    $LfCount = @($Bytes | Where-Object { $_ -eq 10 }).Count
    $Manifest.Add("source=$($Mapping.Source) source_sha256=$SourceHash target_payload=$($Mapping.Destination) target_sha256=$TargetHash bytes=$($Bytes.Length) cr=$CrCount lf=$LfCount")
}

$SourceConfig = Join-Path $PrivateDir 'gateway.m8-source.conf'
$TargetConfig = Join-Path $Incoming 'gateway.conf'
if (Test-Path -LiteralPath $TargetConfig) { throw "refusing to overwrite: $TargetConfig" }
$Lines = [IO.File]::ReadAllLines($SourceConfig)
$RequiredKeys = @('device_id','can_interface','broker_host','broker_port','broker_username','broker_password','mqtt_topic','queue_capacity','queue_push_timeout_ms','batch_interval_ms','mqtt_ack_timeout_ms','mqtt_reconnect_interval_ms','spool_path','log_level')
$Counts = @{}
$OutputLines = foreach ($Line in $Lines) {
    if ($Line -match '^\s*([^#=\s]+)\s*=(.*)$') {
        $Key = $Matches[1]
        if (-not $Counts.ContainsKey($Key)) { $Counts[$Key] = 0 }
        $Counts[$Key]++
        switch ($Key) {
            'device_id' { 'device_id=imx6ull-m9-20260901T204152p0800'; continue }
            'mqtt_topic' { 'mqtt_topic=m9/20260901T204152p0800/gateway'; continue }
            'spool_path' { 'spool_path=/var/lib/gatewayd/20260901T204152p0800-m9-board/spool.data'; continue }
        }
    }
    $Line
}
foreach ($Key in $RequiredKeys) {
    if (-not $Counts.ContainsKey($Key) -or $Counts[$Key] -ne 1) { throw "config key count invalid: $Key" }
}
if (($OutputLines | Where-Object { $_ -eq 'can_interface=can0' }).Count -ne 1) { throw 'can_interface changed or invalid' }
[IO.File]::WriteAllText($TargetConfig, (($OutputLines -join "`n") + "`n"), [Text.UTF8Encoding]::new($false))

$EnvPath = Join-Path $Incoming 'gatewayd.env'
if (Test-Path -LiteralPath $EnvPath) { throw "refusing to overwrite: $EnvPath" }
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

$ConfigHash = (Get-FileHash -LiteralPath $TargetConfig -Algorithm SHA256).Hash.ToLowerInvariant()
$ConfigSourceHash = (Get-FileHash -LiteralPath $SourceConfig -Algorithm SHA256).Hash.ToLowerInvariant()
$EnvHash = (Get-FileHash -LiteralPath $EnvPath -Algorithm SHA256).Hash.ToLowerInvariant()
$Manifest.Add("private_config_source_sha256=$ConfigSourceHash")
$Manifest.Add("private_config_target_sha256=$ConfigHash bytes=$((Get-Item -LiteralPath $TargetConfig).Length)")
$Manifest.Add('private_config_schema=14 required keys exactly once; can_interface preserved as can0; broker host/port/username/password preserved byte-for-value from approved M8 source; device/topic/spool changed for unique M9 run')
$Manifest.Add("gatewayd_env_sha256=$EnvHash bytes=$((Get-Item -LiteralPath $EnvPath).Length)")
$Manifest.Add('gatewayd_env=LD_LIBRARY_PATH exported; /opt binary/private library; /etc config; /var/run pid dir; production restart limit 5/stable 60/cooldown 60/stop timeout 15')
[IO.File]::WriteAllLines($PrivateManifest, $Manifest, [Text.UTF8Encoding]::new($false))
[IO.File]::WriteAllLines($PublicManifest, $Manifest, [Text.UTF8Encoding]::new($false))

Get-Content -LiteralPath $PublicManifest
