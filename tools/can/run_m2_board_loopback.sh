#!/bin/sh

# 只在用户明确批准后执行 M2 controller loopback。脚本不访问网络、Broker 或 /etc。
set -u

EXPECTED_SHA256="be27554bafac535e45908e881117a185965470f21ae9645f2fcb0ca0a1ba5595"
GATEWAYD="/tmp/gatewayd-m2"

if [ "$#" -ne 2 ] || [ "$1" != "--approved" ]; then
    echo "usage: $0 --approved RUN_ID" >&2
    exit 2
fi

RUN_ID="$2"
case "$RUN_ID" in
    ""|*[!A-Za-z0-9+_.-]*)
        echo "invalid RUN_ID" >&2
        exit 2
        ;;
esac

OUTPUT_DIR="/tmp/$RUN_ID"
if [ -e "$OUTPUT_DIR" ]; then
    echo "refusing to reuse existing evidence directory: $OUTPUT_DIR" >&2
    exit 3
fi
if ! mkdir "$OUTPUT_DIR"; then
    echo "failed to create evidence directory: $OUTPUT_DIR" >&2
    exit 3
fi

CONSOLE_LOG="$OUTPUT_DIR/console.log"
RESTORE_COMMAND_LOG="$OUTPUT_DIR/restore_commands.log"
CLEANUP_REQUIRED=0
CLEANUP_DONE=0
RESTORE_DOWN_RC=99
RESTORE_LOOPBACK_OFF_RC=99
RESTORE_RESULT="NOT_RUN"
VERSION_RESULT="NOT_RUN"
TARGET_RESULT="NOT_RUN"
NON_TARGET_RESULT="NOT_RUN"
DLC_RESULT="NOT_RUN"
OVERALL_RESULT="FAIL"

log()
{
    printf '%s\n' "$*" | tee -a "$CONSOLE_LOG"
}

cleanup()
{
    if [ "$CLEANUP_DONE" -eq 1 ]; then
        return
    fi
    CLEANUP_DONE=1
    if [ "$CLEANUP_REQUIRED" -ne 1 ]; then
        return
    fi

    : > "$RESTORE_COMMAND_LOG"
    ip link set can0 down >> "$RESTORE_COMMAND_LOG" 2>&1
    RESTORE_DOWN_RC=$?
    ip link set can0 type can loopback off >> "$RESTORE_COMMAND_LOG" 2>&1
    RESTORE_LOOPBACK_OFF_RC=$?
    ip -details -statistics link show can0 > "$OUTPUT_DIR/can_restored.txt" 2>&1

    if [ "$RESTORE_DOWN_RC" -eq 0 ] &&
       [ "$RESTORE_LOOPBACK_OFF_RC" -eq 0 ] &&
       grep -q "state DOWN" "$OUTPUT_DIR/can_restored.txt" &&
       grep -q "can state STOPPED" "$OUTPUT_DIR/can_restored.txt" &&
       ! grep -q "LOOPBACK" "$OUTPUT_DIR/can_restored.txt"; then
        RESTORE_RESULT="PASS"
    else
        RESTORE_RESULT="FAIL"
    fi
}

write_summary()
{
    {
        echo "run_id=$RUN_ID"
        echo "binary_sha256=$ACTUAL_SHA256"
        echo "version_result=$VERSION_RESULT"
        echo "target_result=$TARGET_RESULT"
        echo "non_target_result=$NON_TARGET_RESULT"
        echo "dlc_result=$DLC_RESULT"
        echo "restore_down_rc=$RESTORE_DOWN_RC"
        echo "restore_loopback_off_rc=$RESTORE_LOOPBACK_OFF_RC"
        echo "restore_result=$RESTORE_RESULT"
        echo "overall_result=$OVERALL_RESULT"
    } > "$OUTPUT_DIR/result.env"
}

fail_before_can_change()
{
    log "M2_BOARD_RESULT overall=FAIL reason=$1 can0_modified=no"
    write_summary
    exit 1
}

trap 'cleanup' 0
trap 'cleanup; exit 130' 1 2 15

log "M2 board loopback run_id=$RUN_ID"
log "Authorization scope: /tmp/gatewayd-m2, can0 loopback, test frames; no network/Broker/etc/firmware/dependency changes"

for REQUIRED_COMMAND in sha256sum ip cansend grep sed tr tee sleep; do
    if ! command -v "$REQUIRED_COMMAND" >/dev/null 2>&1; then
        ACTUAL_SHA256="NOT_CHECKED"
        fail_before_can_change "missing_command_$REQUIRED_COMMAND"
    fi
done

if [ ! -x "$GATEWAYD" ]; then
    ACTUAL_SHA256="NOT_CHECKED"
    fail_before_can_change "gatewayd_missing_or_not_executable"
fi

ACTUAL_SHA256=$(sha256sum "$GATEWAYD" 2> "$OUTPUT_DIR/sha256_error.log" | sed 's/[[:space:]].*$//')
printf '%s  %s\n' "$ACTUAL_SHA256" "$GATEWAYD" > "$OUTPUT_DIR/sha256.txt"
if [ "$ACTUAL_SHA256" != "$EXPECTED_SHA256" ]; then
    fail_before_can_change "binary_sha256_mismatch"
fi

{
    echo "[board_date_untrusted]"
    date
    echo "[uname]"
    uname -a
} > "$OUTPUT_DIR/board_info.txt" 2>&1

ip -details -statistics link show can0 > "$OUTPUT_DIR/can_before.txt" 2>&1
if ! grep -q "state DOWN" "$OUTPUT_DIR/can_before.txt" ||
   ! grep -q "can state STOPPED" "$OUTPUT_DIR/can_before.txt"; then
    fail_before_can_change "unexpected_can0_pre_state"
fi
if grep -q "bitrate " "$OUTPUT_DIR/can_before.txt"; then
    fail_before_can_change "can0_pre_state_already_has_bitrate"
fi

"$GATEWAYD" --version > "$OUTPUT_DIR/version.log" 2>&1
VERSION_RC=$?
if [ "$VERSION_RC" -eq 0 ] && grep -q '^gatewayd ' "$OUTPUT_DIR/version.log"; then
    VERSION_RESULT="PASS"
else
    VERSION_RESULT="FAIL"
    fail_before_can_change "gatewayd_version_failed"
fi
log "gatewayd dynamic-load/version check: PASS"

ip link set can0 down > "$OUTPUT_DIR/can_configure.log" 2>&1
CONFIG_DOWN_RC=$?
if [ "$CONFIG_DOWN_RC" -ne 0 ]; then
    fail_before_can_change "can0_down_failed"
fi

# 从本命令开始可能已经写入 bit timing，因此任何退出路径都必须尝试恢复。
CLEANUP_REQUIRED=1
ip link set can0 type can bitrate 500000 loopback on >> "$OUTPUT_DIR/can_configure.log" 2>&1
CONFIG_TYPE_RC=$?
if [ "$CONFIG_TYPE_RC" -ne 0 ]; then
    log "M2_BOARD_RESULT overall=FAIL reason=can0_type_failed cleanup=scheduled"
    cleanup
    write_summary
    exit 1
fi
ip link set can0 up >> "$OUTPUT_DIR/can_configure.log" 2>&1
CONFIG_UP_RC=$?
if [ "$CONFIG_UP_RC" -ne 0 ]; then
    log "M2_BOARD_RESULT overall=FAIL reason=can0_up_failed cleanup=scheduled"
    cleanup
    write_summary
    exit 1
fi
ip -details -statistics link show can0 > "$OUTPUT_DIR/can_loopback_up.txt" 2>&1
log "can0 configured: 500000 bit/s, controller loopback on"

: > "$OUTPUT_DIR/target_send.log"
"$GATEWAYD" --can-receive 3 --can-timeout-ms 5000 > "$OUTPUT_DIR/target.log" 2>&1 &
TARGET_PID=$!
sleep 1
cansend can0 123#1122334455667788 >> "$OUTPUT_DIR/target_send.log" 2>&1
TARGET_SEND_123_RC=$?
echo "send_123_rc=$TARGET_SEND_123_RC" >> "$OUTPUT_DIR/target_send.log"
cansend can0 100#0102030405060708 >> "$OUTPUT_DIR/target_send.log" 2>&1
TARGET_SEND_100_RC=$?
echo "send_100_rc=$TARGET_SEND_100_RC" >> "$OUTPUT_DIR/target_send.log"
cansend can0 124#1122334455667788 >> "$OUTPUT_DIR/target_send.log" 2>&1
TARGET_SEND_124_RC=$?
echo "send_124_rc=$TARGET_SEND_124_RC" >> "$OUTPUT_DIR/target_send.log"
cansend can0 101#1112131415161718 >> "$OUTPUT_DIR/target_send.log" 2>&1
TARGET_SEND_101_RC=$?
echo "send_101_rc=$TARGET_SEND_101_RC" >> "$OUTPUT_DIR/target_send.log"
cansend can0 125#1122334455667788 >> "$OUTPUT_DIR/target_send.log" 2>&1
TARGET_SEND_125_RC=$?
echo "send_125_rc=$TARGET_SEND_125_RC" >> "$OUTPUT_DIR/target_send.log"
cansend can0 102#2122232425262728 >> "$OUTPUT_DIR/target_send.log" 2>&1
TARGET_SEND_102_RC=$?
echo "send_102_rc=$TARGET_SEND_102_RC" >> "$OUTPUT_DIR/target_send.log"
wait "$TARGET_PID"
TARGET_RC=$?

TARGET_FRAME_COUNT=$(grep -c 'M2_CAN_FRAME' "$OUTPUT_DIR/target.log")
TARGET_TIMESTAMP_COUNT=$(grep -c 'kernel_timestamp_ns=[1-9][0-9]*' "$OUTPUT_DIR/target.log")
TARGET_ID_SEQUENCE=$(sed -n 's/.*M2_CAN_FRAME.*can_id=\(0x[0-9a-f]*\).*/\1/p' "$OUTPUT_DIR/target.log" | tr '\n' ',')
if [ "$TARGET_RC" -eq 0 ] &&
   [ "$TARGET_SEND_123_RC" -eq 0 ] &&
   [ "$TARGET_SEND_100_RC" -eq 0 ] &&
   [ "$TARGET_SEND_124_RC" -eq 0 ] &&
   [ "$TARGET_SEND_101_RC" -eq 0 ] &&
   [ "$TARGET_SEND_125_RC" -eq 0 ] &&
   [ "$TARGET_SEND_102_RC" -eq 0 ] &&
   [ "$TARGET_FRAME_COUNT" -eq 3 ] &&
   [ "$TARGET_TIMESTAMP_COUNT" -eq 3 ] &&
   [ "$TARGET_ID_SEQUENCE" = "0x100,0x101,0x102," ] &&
   [ "$(grep -c 'can_id=0x100' "$OUTPUT_DIR/target.log")" -eq 1 ] &&
   [ "$(grep -c 'can_id=0x101' "$OUTPUT_DIR/target.log")" -eq 1 ] &&
   [ "$(grep -c 'can_id=0x102' "$OUTPUT_DIR/target.log")" -eq 1 ] &&
   grep -q 'M2_CAN_SUMMARY.*accepted=3.*timeouts=0.*rejected_length=0.*rejected_dlc=0.*rejected_id=0.*timestamp_errors=0.*receive_errors=0' "$OUTPUT_DIR/target.log"; then
    TARGET_RESULT="PASS"
else
    TARGET_RESULT="FAIL"
fi
log "target ID/timestamp case: $TARGET_RESULT (gateway_rc=$TARGET_RC frames=$TARGET_FRAME_COUNT timestamps=$TARGET_TIMESTAMP_COUNT)"

: > "$OUTPUT_DIR/non_target_send.log"
"$GATEWAYD" --can-receive 1 --can-timeout-ms 1500 > "$OUTPUT_DIR/non_target.log" 2>&1 &
NON_TARGET_PID=$!
sleep 1
cansend can0 123#1122334455667788 >> "$OUTPUT_DIR/non_target_send.log" 2>&1
NON_TARGET_SEND_RC=$?
echo "send_123_rc=$NON_TARGET_SEND_RC" >> "$OUTPUT_DIR/non_target_send.log"
wait "$NON_TARGET_PID"
NON_TARGET_RC=$?
NON_TARGET_FRAME_COUNT=$(grep -c 'M2_CAN_FRAME' "$OUTPUT_DIR/non_target.log")
if [ "$NON_TARGET_RC" -ne 0 ] &&
   [ "$NON_TARGET_SEND_RC" -eq 0 ] &&
   [ "$NON_TARGET_FRAME_COUNT" -eq 0 ] &&
   grep -q 'M2_CAN_SUMMARY.*accepted=0.*timeouts=1.*rejected_length=0.*rejected_dlc=0.*rejected_id=0.*timestamp_errors=0.*receive_errors=0' "$OUTPUT_DIR/non_target.log"; then
    NON_TARGET_RESULT="PASS"
else
    NON_TARGET_RESULT="FAIL"
fi
log "non-target kernel-filter case: $NON_TARGET_RESULT (expected_nonzero_gateway_rc=$NON_TARGET_RC frames=$NON_TARGET_FRAME_COUNT)"

: > "$OUTPUT_DIR/dlc_send.log"
"$GATEWAYD" --can-receive 1 --can-timeout-ms 1500 > "$OUTPUT_DIR/dlc.log" 2>&1 &
DLC_PID=$!
sleep 1
cansend can0 100#010203 >> "$OUTPUT_DIR/dlc_send.log" 2>&1
DLC_SEND_RC=$?
echo "send_100_dlc3_rc=$DLC_SEND_RC" >> "$OUTPUT_DIR/dlc_send.log"
wait "$DLC_PID"
DLC_RC=$?
if [ "$DLC_RC" -ne 0 ] &&
   [ "$DLC_SEND_RC" -eq 0 ] &&
   grep -q 'rejected frame reason=dlc' "$OUTPUT_DIR/dlc.log" &&
   grep -q 'M2_CAN_SUMMARY.*accepted=0.*timeouts=1.*rejected_length=0.*rejected_dlc=1.*rejected_id=0.*timestamp_errors=0.*receive_errors=0' "$OUTPUT_DIR/dlc.log"; then
    DLC_RESULT="PASS"
else
    DLC_RESULT="FAIL"
fi
log "invalid DLC rejection case: $DLC_RESULT (expected_nonzero_gateway_rc=$DLC_RC)"

ip -details -statistics link show can0 > "$OUTPUT_DIR/can_after.txt" 2>&1
cleanup
log "can0 restore: $RESTORE_RESULT (down_rc=$RESTORE_DOWN_RC loopback_off_rc=$RESTORE_LOOPBACK_OFF_RC)"

if [ "$VERSION_RESULT" = "PASS" ] &&
   [ "$TARGET_RESULT" = "PASS" ] &&
   [ "$NON_TARGET_RESULT" = "PASS" ] &&
   [ "$DLC_RESULT" = "PASS" ] &&
   [ "$RESTORE_RESULT" = "PASS" ]; then
    OVERALL_RESULT="PASS"
fi

write_summary
log "M2_BOARD_RESULT overall=$OVERALL_RESULT version=$VERSION_RESULT target=$TARGET_RESULT non_target=$NON_TARGET_RESULT dlc=$DLC_RESULT restore=$RESTORE_RESULT"
log "Evidence directory: $OUTPUT_DIR"

if [ "$OVERALL_RESULT" = "PASS" ]; then
    exit 0
fi
exit 1
