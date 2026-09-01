# M8 Windows gate attempt 3 — FAIL at strict validator

The asynchronous ARM binary completed real online PUBACK, two Broker outages,
continuous offline spool growth with frozen ACK, automatic reconnect, one
verified SIGKILL, same-spool recovery, state corruption recovery, reactor
counter assertions, graceful exits, and cleanup. However, the dedicated
Broker used no persistence and the clean-session subscriber reconnected after
the gateway. Four reconnect batches were ACKed before subscription. Later
state replay made the final unique set complete (`1..22604`, missing 0), but
those four lower sequences first appeared out of order. The unmodified
repository validator therefore exited 1 with
`deduplicated gateway seq has missing/reordered values`.

Per the M8 prompt, this run remains FAIL and M8 was not closed from it. The
new run `20260901T170636+0800-m8-windows-reactor-final-gate` repeated the full
gate with explicit subscriber-before-gateway alignment.
