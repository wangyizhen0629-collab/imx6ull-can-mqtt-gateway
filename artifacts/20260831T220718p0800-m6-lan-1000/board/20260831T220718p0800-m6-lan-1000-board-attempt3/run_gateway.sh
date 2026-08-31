#!/bin/sh
BOARD_RUN='/tmp/20260831T220718p0800-m6-lan-1000-board-attempt3'
GATEWAY='/tmp/m6-arm-private-20260831T181335/bin/gatewayd'
LIB_DIR='/tmp/m6-arm-private-20260831T181335/lib'
trap ':' INT TERM
printf 'gateway_started_at=%s\n' "$(date -Ins)" > "$BOARD_RUN/gateway_lifecycle.txt"
LD_LIBRARY_PATH="$LIB_DIR" "$GATEWAY" --config "$BOARD_RUN/gateway.conf" --run-mqtt   > "$BOARD_RUN/gateway_stdout.log" 2> "$BOARD_RUN/gateway_stderr.log"
GATEWAY_EXIT=$?
trap - INT TERM
printf 'gateway_exit=%s\n' "$GATEWAY_EXIT" > "$BOARD_RUN/gateway_exit.txt"
printf 'gateway_finished_at=%s\n' "$(date -Ins)" >> "$BOARD_RUN/gateway_lifecycle.txt"
ip -details -statistics link show can0 > "$BOARD_RUN/can_after.txt" 2>&1
printf 'can_after_exit=%s\n' "$?" > "$BOARD_RUN/can_after_exit.txt"
pgrep -a gatewayd > "$BOARD_RUN/process_final.txt" 2>&1
printf 'process_final_pgrep_exit=%s\n' "$?" > "$BOARD_RUN/process_final_exit.txt"
date -Ins > "$BOARD_RUN/board_finished_at.txt" 2>&1
echo "GATEWAY_FINISHED exit=$GATEWAY_EXIT BOARD_RUN=$BOARD_RUN"
exit "$GATEWAY_EXIT"
