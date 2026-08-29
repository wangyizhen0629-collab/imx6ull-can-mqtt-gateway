#!/bin/sh

set -eu

RUN_DIR="artifacts/20260829T133148+0800-m2-board-loopback"
ARCHIVE="artifacts/20260829T133148+0800-m2-board-loopback.tar"
EXPECTED_ARCHIVE_SHA="a538c92bc4aef201df3c8dd7069285d48c997af98ffd0378732b443086f54163"
EXPECTED_BINARY_SHA="be27554bafac535e45908e881117a185965470f21ae9645f2fcb0ca0a1ba5595"

pass()
{
    printf 'PASS %s\n' "$1"
}

ACTUAL_ARCHIVE_SHA=$(sha256sum "$ARCHIVE" | sed 's/[[:space:]].*$//')
[ "$ACTUAL_ARCHIVE_SHA" = "$EXPECTED_ARCHIVE_SHA" ]
pass "uploaded archive SHA256"

tar -df "$ARCHIVE" -C artifacts
pass "archive matches extracted raw files"

ACTUAL_BINARY_SHA=$(sed 's/[[:space:]].*$//' "$RUN_DIR/sha256.txt")
[ "$ACTUAL_BINARY_SHA" = "$EXPECTED_BINARY_SHA" ]
pass "board binary matches cross-build artifact"

grep -q '^version_result=PASS$' "$RUN_DIR/result.env"
grep -q '^target_result=PASS$' "$RUN_DIR/result.env"
grep -q '^non_target_result=PASS$' "$RUN_DIR/result.env"
grep -q '^dlc_result=PASS$' "$RUN_DIR/result.env"
grep -q '^restore_result=PASS$' "$RUN_DIR/result.env"
grep -q '^overall_result=PASS$' "$RUN_DIR/result.env"
pass "board runner result fields"

grep -q '^gatewayd 0.1.0-m1$' "$RUN_DIR/version.log"
pass "target dynamic load and version"

[ "$(grep -c '=0$' "$RUN_DIR/target_send.log")" -eq 6 ]
[ "$(wc -l < "$RUN_DIR/target_send.log")" -eq 6 ]
[ "$(grep -c 'M2_CAN_FRAME' "$RUN_DIR/target.log")" -eq 3 ]
ID_SEQUENCE=$(sed -n 's/.*M2_CAN_FRAME.*can_id=\(0x[0-9a-f]*\).*/\1/p' "$RUN_DIR/target.log" | tr '\n' ',')
[ "$ID_SEQUENCE" = '0x100,0x101,0x102,' ]
[ "$(grep -c 'kernel_timestamp_ns=[1-9][0-9]*' "$RUN_DIR/target.log")" -eq 3 ]
set -- $(sed -n 's/.*kernel_timestamp_ns=\([0-9][0-9]*\).*/\1/p' "$RUN_DIR/target.log")
[ "$#" -eq 3 ]
[ "$1" -lt "$2" ]
[ "$2" -lt "$3" ]
grep -q 'can_id=0x100.*data=0102030405060708' "$RUN_DIR/target.log"
grep -q 'can_id=0x101.*data=1112131415161718' "$RUN_DIR/target.log"
grep -q 'can_id=0x102.*data=2122232425262728' "$RUN_DIR/target.log"
grep -q 'M2_CAN_SUMMARY attempts=3 accepted=3 timeouts=0 rejected_length=0 rejected_dlc=0 rejected_id=0 timestamp_errors=0 receive_errors=0' "$RUN_DIR/target.log"
pass "ordered target IDs, payloads, positive increasing kernel timestamps and summary"

grep -q '^send_123_rc=0$' "$RUN_DIR/non_target_send.log"
[ "$(grep -c 'M2_CAN_FRAME' "$RUN_DIR/non_target.log")" -eq 0 ]
grep -q 'M2_CAN_SUMMARY attempts=1 accepted=0 timeouts=1 rejected_length=0 rejected_dlc=0 rejected_id=0 timestamp_errors=0 receive_errors=0' "$RUN_DIR/non_target.log"
pass "non-target ID kernel filter and expected timeout"

grep -q '^send_100_dlc3_rc=0$' "$RUN_DIR/dlc_send.log"
grep -q 'rejected frame reason=dlc' "$RUN_DIR/dlc.log"
grep -q 'M2_CAN_SUMMARY attempts=2 accepted=0 timeouts=1 rejected_length=0 rejected_dlc=1 rejected_id=0 timestamp_errors=0 receive_errors=0' "$RUN_DIR/dlc.log"
pass "invalid DLC rejection and expected timeout"

grep -q 'state DOWN' "$RUN_DIR/can_before.txt"
grep -q 'can state STOPPED' "$RUN_DIR/can_before.txt"
! grep -q 'bitrate ' "$RUN_DIR/can_before.txt"
grep -q 'can <LOOPBACK> state ERROR-ACTIVE' "$RUN_DIR/can_loopback_up.txt"
grep -q 'bitrate 500000' "$RUN_DIR/can_loopback_up.txt"
pass "CAN pre-state and active controller loopback state"

grep -Eq '^[[:space:]]*59[[:space:]]+8[[:space:]]+0[[:space:]]+0[[:space:]]+0[[:space:]]+0' "$RUN_DIR/can_after.txt"
grep -q 'state DOWN' "$RUN_DIR/can_restored.txt"
grep -q 'can state STOPPED' "$RUN_DIR/can_restored.txt"
! grep -q 'LOOPBACK' "$RUN_DIR/can_restored.txt"
grep -q 'bitrate 500000' "$RUN_DIR/can_restored.txt"
grep -Eq '^[[:space:]]*59[[:space:]]+8[[:space:]]+0[[:space:]]+0[[:space:]]+0[[:space:]]+0' "$RUN_DIR/can_restored.txt"
pass "8 TX frames/59 bytes, zero TX errors, DOWN/STOPPED and loopback-off restoration"

grep -q 'armv7l GNU/Linux' "$RUN_DIR/board_info.txt"
grep -q 'Thu Jan  1 .* 1970' "$RUN_DIR/board_info.txt"
pass "board identity and explicitly untrusted wall clock"

printf 'FINAL result=PASS limitations=board_wall_clock_uninitialized,bitrate_500000_retained_while_DOWN\n'
