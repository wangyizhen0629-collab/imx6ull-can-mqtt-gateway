# M8 Windows gate attempt 2 — FAIL

The first repaired ARM binary SHA-256
`c27cae52c6e746035e058cc77e78cb0dc1003f58f3036312b000579e0af9e368`
still stopped consuming continuous CAN while the Broker was absent. Evidence
showed the consume path entered synchronous `mosquitto_connect()` and blocked
in the kernel TCP connect path when Windows dropped SYN traffic. The run was
restored only to unblock and cleanly audit the failure; its one verified
SIGKILL was cleanup, not a final gate PASS. Commit
`25681d871a32ed3936962144418058d1af2700b4` replaced reconnect with an
external-loop asynchronous state machine and added the silent-listener test.

This run remains FAIL and does not contribute PASS values to the final gate.
