# M8 Windows gate attempt 1 — FAIL

The ARM binary SHA-256 `2c3841e6a18ea80a470bf7d2bb8deaed314fdd1a495dc8c2b5c9a4021a8a9a6b`
ran on the real board and completed online CAN/MQTT and offline spool growth,
but did not reconnect while continuous CAN kept the consumer non-idle. The
root cause was that `gateway_mqtt_sink_poll()` only ran on the consumer idle
path. This run is not a gate PASS. Its dedicated processes were cleaned up;
one verified SIGKILL was used only for failed-run cleanup and is not reused as
final gate evidence. Commit `6c2ed510f75fe8dc762e2ac3586c7cc5a750645e`
added consume-path polling and a continuous-input regression test.

Raw private evidence remains Git-ignored. Public files known to contain an
unredacted LAN address are intentionally excluded from staging.
