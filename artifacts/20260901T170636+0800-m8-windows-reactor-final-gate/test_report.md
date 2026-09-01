# M8 Windows Broker / i.MX6ULL final reactor gate

## Conclusion

- Run: `20260901T170636+0800-m8-windows-reactor-final-gate`
- Repository HEAD used by the run: `68c6628887dc5accfe67d15ec75486a1c51220cd`
- M8 implementation commit: `25681d871a32ed3936962144418058d1af2700b4`
- ARM binary: 198576 bytes, SHA-256
  `2e3976727d57f850223ec3b0b3713c930d96f75375897f7c1fe69dcfc2e1548b`
- Target libmosquitto SHA-256:
  `b32c8ac4defb2b2920fba2e42f263869508c42e3c1719440db37ffc8d8c2f636`
- M7 prerequisite: `MET`
- M8 exit gate: `MET`
- M9 and later: not started

This run used a real i.MX6ULL, physical CAN input, a dedicated Windows
Mosquitto 2.1.2 Broker, an ext4 test spool, and the M8 external-loop binary.
It completed online PUBACK, two Broker outages, automatic reconnect, one
verified SIGKILL, same-spool restart, state corruption recovery, graceful
shutdown, three-sided Broker accounting, and strict subscriber validation.
The result is a single controlled functional gate, not a power-loss,
performance, latency, or reliability result.

## Ubuntu fix/build prerequisite

The first Windows attempts exposed two M8-only defects: continuous CAN could
starve explicit MQTT polling, and a synchronous reconnect could block in the
kernel. Commits `6c2ed510f75fe8dc762e2ac3586c7cc5a750645e` and
`25681d871a32ed3936962144418058d1af2700b4` fixed these paths. Fresh Ubuntu
evidence under
`artifacts/20260901T160813+0800-m8-silent-listener-reconnect-fix-ubuntu/`
records warning-clean builds, full CTest 17/17, M8 2/2, ASan/UBSan 2/2,
ARMv7 build/API/RPATH checks, and the binary above. LeakSanitizer remained
`NOT RUN`.

## Identity, deployment, and storage

- The binary ran from `/tmp/m8-reactor-gate-20260901T170636/bin/gatewayd`;
  the spool lived under the independent
  `/var/lib/gatewayd-m8-test-20260901T170636/` root on writable
  `/dev/root` ext4. `/tmp` was not used for spool durability evidence.
- Board `file`/`readelf`/`ldd` evidence identifies ARMv7 EABI5 hard-float and
  resolves the private target libmosquitto without missing dependencies.
- The private test configuration used `mqtt_reconnect_interval_ms=20000` so
  the clean-session subscriber could be proven subscribed before the next
  scheduled gateway reconnect. This changed test timing only; source,
  production defaults, Broker persistence, network, and CAN state were not
  changed.
- Initial and final CAN snapshots were `UP`, 500 kbit/s, `ERROR-ACTIVE`,
  berr tx/rx 0/0. Kernel RX errors/dropped remained 0/0; cumulative
  error-warn/error-pass/bus-off stayed 347/4/0. RX packets increased from
  7545298 to 7599658. These counts only prove live input during the gate.

## Broker outage and automatic reconnect

The first outage snapshots show the same live PID and binary while CAN RX and
spool continued to grow. Between the two snapshots, pending records increased
from 239 to 974; `ack_offset=213120`, `ack_seq=2664`, and the state SHA-256
remained unchanged. Repeated `async reconnect wait ... timeout` messages prove
the external-loop reconnect path stayed responsive instead of blocking the
consumer.

After a timeout, the Broker was restarted. Broker log ordering proves the
subscriber connected at line index 10 before the gateway at index 18, with
zero gateway PUBLISH before subscriber subscription. The same gateway PID
then logged `connected`; its cursor advanced to `ack_seq=17322` with 49
records pending under continuing CAN input.

## Verified SIGKILL and same-spool recovery

The second outage again froze `ack_seq=19325` and the state hash while pending
grew from 274 to 1013 and CAN RX increased. The one-shot control verified PID,
`comm`, executable hash, and this run's config path before issuing exactly one
`kill -9`. The signal returned 0, the process disappeared, and the wrapper
recorded `wait_exit=137`. Spool data grew by ten records between the pre-kill
capture and process disappearance because CAN ingestion was concurrent; the
state hash stayed frozen. No claim of byte-for-byte data immutability is made.

The Broker and subscriber were established before `main2` started with the
same binary/config/spool. Its cursor advanced beyond the frozen sequence and
the graceful-stop summary reported `records_acked=4764`,
`last_gateway_seq=24089`, `spool_pending=0`, `queue_drop=0`, and
`puback_unexpected=0`.

## State corruption and reactor counters

On a dedicated copy, state magic byte 0 changed from `0x47` to `0x00`; the
spool data hash remained unchanged and the state hash changed. `state1`
performed safe replay and exited with `spool_state_recoveries=1`,
`records_acked=27434`, `last_gateway_seq=27434`, `spool_pending=0`,
`queue_drop=0`, and `puback_unexpected=0`.

Both graceful summaries have `reactor_enabled=1`. `main2` recorded
epoll/wake/timer/socket/read/write/misc counts
3696/47/43/48/47/1/43; `state1` recorded
3617/135/48/136/135/1/48. All mandatory real-run counters were nonzero.

## Subscriber validator and Broker accounting

The three Broker phases independently reconcile:

| Phase | Gateway PUBLISH/PUBACK | Broker to subscriber/PUBACK |
| --- | ---: | ---: |
| baseline | 31 / 31 | 31 / 31 |
| reconnect1 | 112 / 112 | 112 / 112 |
| crash/state recovery | 180 / 180 | 180 / 180 |
| total | 323 / 323 | 323 / 323 |

The subscriber contains 323 JSON lines, equal to all four Broker totals. The
repository validator self-tests passed 5/5 and the captured-data command exited
0 with `--require-raw-duplicates`:

| Metric | Value |
| --- | ---: |
| raw batches | 323 |
| raw records | 51523 |
| raw duplicate records | 24089 |
| unique batch seq | 189 |
| unique gateway seq | 27434 |
| first / last gateway seq | 1 / 27434 |
| missing gateway seq | 0 |
| effective duplicate records | 0 |

`validator_command.txt` inherited an incorrect human-readable directory
suffix during mechanical scaffolding; `validator_command_correction.txt`
records the actual final-gate input. The executed script used the absolute
`$InputPath`, whose SHA-256 is
`76be0e3567f8b12fc22f40f8e2695ae332b0f1d065f50268079ed7ae9c6ea3a7`.

## Cleanup, evidence, and limits

- Graceful gateway exits both returned 0. Final board audit found zero
  `gatewayd` processes. The subscriber and all three dedicated Broker PIDs
  exited; Windows port 18884 had zero listeners.
- The board tar was 6139904 bytes. Remote and local SHA-256 both equal
  `f48b28a83d63a0e964a1e443c0377d3537f1b304beac70eaa0298cc8b292df46`.
  All 78 tar entries passed absolute/drive/parent-traversal checks; 73 files
  were extracted under Git-ignored `private_raw` only.
- A first public redaction attempt left a LAN address because of a parser bug;
  it is preserved but excluded from staging. The v2 public evidence replaces
  every RFC1918 address and scans 69 files with zero matches.
- Correct UTC, real power loss, storage power-fail guarantees, performance,
  latency, throughput, CPU/RSS, long-duration reliability, media lifetime,
  capacity thresholds, and compaction are `NOT RUN`. M9 was not started.
