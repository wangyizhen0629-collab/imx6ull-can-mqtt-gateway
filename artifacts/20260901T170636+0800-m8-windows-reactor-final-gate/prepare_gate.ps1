$ErrorActionPreference = 'Stop'
$RunDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$PrivateDir = Join-Path $RunDir 'private_raw'
$Incoming = Join-Path $PrivateDir 'incoming\gatewayd'
$ExpectedSha = '2e3976727d57f850223ec3b0b3713c930d96f75375897f7c1fe69dcfc2e1548b'

if (-not $env:M8_BROKER_BIND) { throw 'M8_BROKER_BIND is required' }
if (-not (Test-Path -LiteralPath $Incoming -PathType Leaf)) { throw 'incoming gatewayd is missing' }
$BinarySha = (Get-FileHash -LiteralPath $Incoming -Algorithm SHA256).Hash.ToLowerInvariant()
if ($BinarySha -ne $ExpectedSha) { throw "incoming gatewayd SHA256 mismatch: $BinarySha" }
$LocalAddress = Get-NetIPAddress -AddressFamily IPv4 -ErrorAction Stop |
    Where-Object IPAddress -eq $env:M8_BROKER_BIND
if (-not $LocalAddress) { throw 'requested private Broker bind address is not configured' }
$Listeners = Get-NetTCPConnection -State Listen -ErrorAction SilentlyContinue |
    Where-Object LocalPort -eq 18884
if ($Listeners) { throw 'port 18884 already has a listener' }

New-Item -ItemType Directory -Path $PrivateDir -Force | Out-Null
@(
    "broker_bind=$($env:M8_BROKER_BIND)"
    'broker_port=18884'
) | Out-File -LiteralPath (Join-Path $PrivateDir 'broker_endpoint.txt') -Encoding ascii
@(
    "listener 18884 $($env:M8_BROKER_BIND)"
    'allow_anonymous true'
    'persistence false'
    'connection_messages true'
    'log_timestamp true'
    'log_type all'
) | Out-File -LiteralPath (Join-Path $PrivateDir 'mosquitto.conf') -Encoding ascii
@(
    'device_id=m8-gateway-20260901-170636'
    'can_interface=can0'
    "broker_host=$($env:M8_BROKER_BIND)"
    'broker_port=18884'
    'broker_username='
    'broker_password='
    'mqtt_topic=test/m8/20260901T170636/reactor'
    'queue_capacity=65536'
    'queue_push_timeout_ms=50'
    'batch_interval_ms=1000'
    'mqtt_ack_timeout_ms=5000'
    'mqtt_reconnect_interval_ms=20000'
    'spool_path=/var/lib/gatewayd-m8-test-20260901T170636/main/spool.data'
    'log_level=info'
) | Out-File -LiteralPath (Join-Path $PrivateDir 'gateway.conf') -Encoding ascii

$RawFiles = 'broker_endpoint.txt','mosquitto.conf','gateway.conf'
$Trace = @(
    "incoming_gatewayd_sha256=$BinarySha"
    "expected_gatewayd_sha256=$ExpectedSha"
)
foreach ($Name in $RawFiles) {
    $Path = Join-Path $PrivateDir $Name
    $Trace += "raw_${Name}_sha256=$((Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant())"
}
foreach ($Name in 'mosquitto.conf.redacted','gateway.conf.redacted') {
    $Path = Join-Path $RunDir $Name
    $Trace += "${Name}_sha256=$((Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant())"
}
$Trace += 'redaction_method=将真实 Broker 地址精确替换为 <REDACTED_BROKER_ADDRESS>；空用户名/密码保持原样'
$Trace | Out-File -LiteralPath (Join-Path $RunDir 'config_redaction_trace.txt') -Encoding utf8
@(
    "prepared_at=$((Get-Date).ToString('o'))"
    'status=PASS'
    "incoming_gatewayd_length=$((Get-Item $Incoming).Length)"
    "incoming_gatewayd_sha256=$BinarySha"
    'broker_address_configured=true'
    'port_18884_listener_count=0'
    'broker_started=false'
    'subscriber_started=false'
    'board_modified=false'
) | Out-File -LiteralPath (Join-Path $RunDir 'gate_preparation.txt') -Encoding utf8

