# M0 host test report

Result: **PASS** for the M0 host skeleton only.

The project configured with CMake 3.22.1, compiled with GCC 11.4.0 under
`-Wall -Wextra -Wpedantic -Werror`, and passed all three registered CTest smoke
tests. Direct runs confirmed normal default startup, readable example-config
loading, version output, and a nonzero result for an intentionally absent
configuration file.

This run does not test or support claims about ARMv7, the i.MX6ULL, SocketCAN,
STM32, MQTT, QoS/PUBACK, queues, persistence, epoll, init/respawn, reliability,
performance, or duration. Those items are `NOT RUN` and remain gated.

