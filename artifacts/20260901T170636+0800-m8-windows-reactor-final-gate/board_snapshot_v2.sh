#!/bin/sh
set +e
umask 077
AREA=$1
LABEL=$2
RUN_ROOT=/var/lib/gatewayd-m8-test-20260901T170636
EVIDENCE=$RUN_ROOT/evidence
EXPECTED_GATEWAY_SHA=2e3976727d57f850223ec3b0b3713c930d96f75375897f7c1fe69dcfc2e1548b

fail(){ echo "M8_SNAPSHOT_V2_FAIL: $*" >&2; exit 1; }
read_u64(){ FILE=$1; OFFSET=$2; set -- $(dd if="$FILE" bs=1 skip="$OFFSET" count=8 2>/dev/null | od -An -v -tu1); test "$#" -eq 8 || return 1; echo $(( $1 + $2 * 256 + $3 * 65536 + $4 * 16777216 + $5 * 4294967296 + $6 * 1099511627776 + $7 * 281474976710656 + $8 * 72057594037927936 )); }
case "$AREA" in
  main) SPOOL=$RUN_ROOT/main/spool.data ;;
  state) SPOOL=$RUN_ROOT/corruption/state/spool.data ;;
  *) fail 'area must be main or state' ;;
esac
test -n "$LABEL" || fail 'label is required'
STATE=${SPOOL%.*}.state
OUT=$EVIDENCE/$LABEL
test ! -e "$OUT" || fail 'snapshot label already exists'
test -f "$SPOOL" || fail "spool missing: $SPOOL"
test -f "$STATE" || fail "state missing: $STATE"
SIZE=$(wc -c < "$SPOOL")
ACK_OFFSET=$(read_u64 "$STATE" 16) || fail 'cannot decode ack offset'
ACK_SEQ=$(read_u64 "$STATE" 24) || fail 'cannot decode ack seq'
NEXT_BATCH=$(read_u64 "$STATE" 32) || fail 'cannot decode next batch'
PID=''
for PID_FILE in "$EVIDENCE"/*.gateway.pid; do
  [ -f "$PID_FILE" ] || continue
  CANDIDATE=$(cat "$PID_FILE" 2>/dev/null)
  [ -r "/proc/$CANDIDATE/comm" ] || continue
  grep -qx gatewayd "/proc/$CANDIDATE/comm" 2>/dev/null || continue
  PID=$CANDIDATE
done
{
  echo "snapshot=$LABEL"
  echo "captured_at=$(date -Ins 2>/dev/null)"
  echo "area=$AREA"
  echo "gateway_pid=${PID:-none}"
  if [ -n "$PID" ]; then
    printf 'gateway_cmdline='
    tr '\000' ' ' < "/proc/$PID/cmdline"
    echo
    EXE_SHA=$(sha256sum "/proc/$PID/exe" | awk '{print $1}')
    echo "gateway_exe_sha256=$EXE_SHA"
    test "$EXE_SHA" = "$EXPECTED_GATEWAY_SHA" || echo 'gateway_exe_sha256_match=false'
    grep '^State:' "/proc/$PID/status"
  fi
  echo "spool_size=$SIZE"
  echo "spool_entries=$((SIZE / 80))"
  echo "spool_sha256=$(sha256sum "$SPOOL" | awk '{print $1}')"
  echo "state_sha256=$(sha256sum "$STATE" | awk '{print $1}')"
  echo "ack_offset=$ACK_OFFSET"
  echo "ack_seq=$ACK_SEQ"
  echo "next_batch=$NEXT_BATCH"
  echo "pending_records=$(((SIZE - ACK_OFFSET) / 80))"
  echo "can_rx_packets=$(cat /sys/class/net/can0/statistics/rx_packets)"
  ip -details -statistics link show can0
} > "$OUT" 2>&1
cat "$OUT"
echo "M8_SNAPSHOT_V2_PASS label=$LABEL"

