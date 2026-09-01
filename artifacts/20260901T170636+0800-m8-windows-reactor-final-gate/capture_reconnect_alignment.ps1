param([Parameter(Mandatory = $true)][string]$BrokerPhase,
      [Parameter(Mandatory = $true)][string]$EvidenceName)
$ErrorActionPreference = 'Stop'
$RunDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$PublicPath = Join-Path $RunDir "$EvidenceName.txt"
if (Test-Path -LiteralPath $PublicPath) { throw 'refusing to overwrite reconnect alignment evidence' }
$SubscriberPid = [int]((Get-Content -LiteralPath (Join-Path $RunDir 'windows-capture/subscriber.pid.txt')) -split '=', 2)[1]
$Subscriber = Get-Process -Id $SubscriberPid -ErrorAction Stop
if ($Subscriber.Path -ne 'E:\mosquitto\mosquitto_sub.exe') { throw 'subscriber path mismatch' }
$Connection = @(Get-NetTCPConnection -State Established -ErrorAction Stop |
    Where-Object { $_.OwningProcess -eq $SubscriberPid -and $_.RemotePort -eq 18884 })
if ($Connection.Count -ne 1) { throw 'subscriber is not uniquely connected to the dedicated Broker' }
$BrokerLog = Join-Path $RunDir "private_raw/windows-$BrokerPhase/broker.stderr.log"
$Lines = @(Get-Content -LiteralPath $BrokerLog)
$SubscriberPattern = '*New client connected*as m8-win-sub-20260901T170636*'
$GatewayPattern = '*New client connected*as gatewayd-m8-gateway-20260901-170636-*'
$SubscriberConnects = @($Lines | Where-Object { $_ -like $SubscriberPattern }).Count
$GatewayConnects = @($Lines | Where-Object { $_ -like $GatewayPattern }).Count
$SubscriberFirstIndex = -1
$GatewayFirstIndex = -1
for ($Index = 0; $Index -lt $Lines.Count; $Index++) {
    if ($SubscriberFirstIndex -lt 0 -and $Lines[$Index] -like $SubscriberPattern) { $SubscriberFirstIndex = $Index }
    if ($GatewayFirstIndex -lt 0 -and $Lines[$Index] -like $GatewayPattern) { $GatewayFirstIndex = $Index }
}
$GatewayPublishesBeforeSubscriber = if ($SubscriberFirstIndex -gt 0) {
    @($Lines[0..($SubscriberFirstIndex - 1)] | Where-Object { $_ -like '*Received PUBLISH from gatewayd-m8-gateway-20260901-170636-*' }).Count
} else { 0 }
if ($SubscriberConnects -lt 1 -or
    ($GatewayFirstIndex -ge 0 -and $SubscriberFirstIndex -ge $GatewayFirstIndex) -or
    $GatewayPublishesBeforeSubscriber -ne 0) {
    throw 'subscriber-before-gateway alignment condition is not satisfied'
}
@(
    "captured_at=$((Get-Date).ToString('o'))"
    "broker_phase=$BrokerPhase"
    "subscriber_pid=$SubscriberPid"
    'subscriber_path=E:\mosquitto\mosquitto_sub.exe'
    "subscriber_established_connections=$($Connection.Count)"
    "broker_log_subscriber_connects=$SubscriberConnects"
    "broker_log_gateway_connects=$GatewayConnects"
    "subscriber_first_line_index=$SubscriberFirstIndex"
    "gateway_first_line_index=$GatewayFirstIndex"
    "gateway_publishes_before_subscriber=$GatewayPublishesBeforeSubscriber"
    'broker_log_sha256_at_capture=NOT AVAILABLE (live Mosquitto log is exclusively locked; hash after Broker stop)'
    'alignment=PASS (subscriber subscribed before the next scheduled gateway reconnect)'
) | Out-File -LiteralPath $PublicPath -Encoding utf8
Get-Content -LiteralPath $PublicPath
