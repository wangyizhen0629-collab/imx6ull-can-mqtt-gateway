#!/bin/sh
set +e
umask 077

ACTION=$1
LABEL=$2
STAGE=/tmp/m8-reactor-gate-20260901T170636
RUN_ROOT=/var/lib/gatewayd-m8-test-20260901T170636
EVIDENCE=$RUN_ROOT/evidence
MAIN=$RUN_ROOT/main
STATE_DIR=$RUN_ROOT/corruption/state
GATEWAY=$STAGE/bin/gatewayd
LIB_DIR=/tmp/m7/lib
LIB_LINK=$LIB_DIR/libmosquitto.so.1
EXPECTED_GATEWAY_SHA=2e3976727d57f850223ec3b0b3713c930d96f75375897f7c1fe69dcfc2e1548b
EXPECTED_LIBRARY_SHA=b32c8ac4defb2b2920fba2e42f263869508c42e3c1719440db37ffc8d8c2f636

fail()
{
    echo "M8_BOARD_FAIL action=$ACTION: $*" >&2
    exit 1
}

read_state_u64()
{
    FILE=$1
    OFFSET=$2
    set -- $(dd if="$FILE" bs=1 skip="$OFFSET" count=8 2>/dev/null | od -An -v -tu1)
    test "$#" -eq 8 || return 1
    echo $(( $1 + $2 * 256 + $3 * 65536 + $4 * 16777216 + $5 * 4294967296 + $6 * 1099511627776 + $7 * 281474976710656 + $8 * 72057594037927936 ))
}

assert_no_gateway()
{
    for COMM in /proc/[0-9]*/comm; do
        [ -r "$COMM" ] || continue
        grep -qx gatewayd "$COMM" 2>/dev/null && fail "existing gatewayd process found at $COMM"
    done
}

phase_config()
{
    case "$1" in
        main1|main2) echo "$MAIN/gateway.conf" ;;
        state1) echo "$STATE_DIR/gateway.conf" ;;
        *) return 1 ;;
    esac
}

snapshot_to_file()
{
    NAME=$1
    CONFIG=$2
    SPOOL=$(sed -n 's/^spool_path=//p' "$CONFIG")
    STATE=${SPOOL%.*}.state
    OUT=$EVIDENCE/$NAME
    test ! -e "$OUT" || fail "snapshot already exists: $NAME"
    test -f "$SPOOL" || fail "spool is missing for snapshot: $SPOOL"
    test -f "$STATE" || fail "state is missing for snapshot: $STATE"
    SIZE=$(wc -c < "$SPOOL")
    ACK_OFFSET=$(read_state_u64 "$STATE" 16) || fail 'cannot decode ack offset'
    ACK_SEQ=$(read_state_u64 "$STATE" 24) || fail 'cannot decode ack seq'
    NEXT_BATCH=$(read_state_u64 "$STATE" 32) || fail 'cannot decode next batch'
    PID=''
    for PID_FILE in "$EVIDENCE"/*.gateway.pid; do
        [ -f "$PID_FILE" ] || continue
        CANDIDATE=$(cat "$PID_FILE" 2>/dev/null)
        [ -r "/proc/$CANDIDATE/comm" ] || continue
        grep -qx gatewayd "/proc/$CANDIDATE/comm" 2>/dev/null || continue
        PID=$CANDIDATE
    done
    {
        echo "snapshot=$NAME"
        echo "captured_at=$(date -Ins 2>/dev/null)"
        echo "gateway_pid=${PID:-none}"
        if [ -n "$PID" ]; then
            printf 'gateway_cmdline='
            tr '\000' ' ' < "/proc/$PID/cmdline"
            echo
            echo "gateway_exe_sha256=$(sha256sum "/proc/$PID/exe" | awk '{print $1}')"
            grep '^State:' "/proc/$PID/status"
        fi
        echo "spool_path=$SPOOL"
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
}

start_phase()
{
    PHASE=$1
    CONFIG=$(phase_config "$PHASE") || fail "invalid phase $PHASE"
    assert_no_gateway
    test -f "$CONFIG" || fail "config is missing: $CONFIG"
    test ! -e "$EVIDENCE/$PHASE.gateway.pid" || fail "phase PID file already exists: $PHASE"
    sh "$RUN_ROOT/run_gateway_phase.sh" "$PHASE" "$CONFIG" > "$EVIDENCE/$PHASE.wrapper.stdout.log" 2> "$EVIDENCE/$PHASE.wrapper.stderr.log" &
    WRAPPER_PID=$!
    echo "$WRAPPER_PID" > "$EVIDENCE/$PHASE.wrapper.pid"
    COUNT=0
    while [ ! -s "$EVIDENCE/$PHASE.gateway.pid" ] && [ "$COUNT" -lt 50 ]; do
        sleep 1
        COUNT=$((COUNT + 1))
    done
    test -s "$EVIDENCE/$PHASE.gateway.pid" || fail "gateway PID was not recorded: $PHASE"
    PID=$(cat "$EVIDENCE/$PHASE.gateway.pid")
    test -r "/proc/$PID/comm" || fail "gateway exited during startup: $PHASE"
    grep -qx gatewayd "/proc/$PID/comm" || fail "recorded PID is not gatewayd: $PID"
    test "$(sha256sum "/proc/$PID/exe" | awk '{print $1}')" = "$EXPECTED_GATEWAY_SHA" || fail 'running binary hash mismatch'
    sleep 3
    test -r "/proc/$PID/comm" || fail "gateway exited during readiness: $PHASE"
    printf 'gateway_cmdline='
    tr '\000' ' ' < "/proc/$PID/cmdline"
    echo
    echo "M8_BOARD_START_PASS phase=$PHASE wrapper_pid=$WRAPPER_PID gateway_pid=$PID"
}

summary_metric()
{
    KEY=$1
    echo "$SUMMARY" | sed -n "s/.* $KEY=\([0-9][0-9]*\).*/\1/p"
}

stop_phase()
{
    PHASE=$1
    PID=$(cat "$EVIDENCE/$PHASE.gateway.pid" 2>/dev/null)
    test -n "$PID" || fail "missing PID for $PHASE"
    test -r "/proc/$PID/comm" || fail "gateway is not running for $PHASE"
    grep -qx gatewayd "/proc/$PID/comm" || fail 'PID comm mismatch'
    test "$(sha256sum "/proc/$PID/exe" | awk '{print $1}')" = "$EXPECTED_GATEWAY_SHA" || fail 'PID executable hash mismatch'
    printf 'verified_cmdline=' > "$EVIDENCE/$PHASE.stop.command.txt"
    tr '\000' ' ' < "/proc/$PID/cmdline" >> "$EVIDENCE/$PHASE.stop.command.txt"
    echo >> "$EVIDENCE/$PHASE.stop.command.txt"
    echo "command=kill -TERM $PID" >> "$EVIDENCE/$PHASE.stop.command.txt"
    echo "issued_at=$(date -Ins 2>/dev/null)" >> "$EVIDENCE/$PHASE.stop.command.txt"
    kill -TERM "$PID"
    TERM_EXIT=$?
    echo "kill_term_exit=$TERM_EXIT" > "$EVIDENCE/$PHASE.stop.signal-exit.txt"
    test "$TERM_EXIT" -eq 0 || fail 'SIGTERM failed'
    COUNT=0
    while [ -e "/proc/$PID" ] && [ "$COUNT" -lt 60 ]; do sleep 1; COUNT=$((COUNT + 1)); done
    test ! -e "/proc/$PID" || fail 'gateway did not exit within 60 seconds'
    COUNT=0
    while [ ! -s "$EVIDENCE/$PHASE.wait-exit.txt" ] && [ "$COUNT" -lt 20 ]; do sleep 1; COUNT=$((COUNT + 1)); done
    grep -q '^wait_exit=0$' "$EVIDENCE/$PHASE.wait-exit.txt" || fail 'wrapper wait exit is not zero'
    SUMMARY=$(grep 'M8_MQTT_SUMMARY' "$EVIDENCE/$PHASE.gateway.stderr.log" | tail -n 1)
    test -n "$SUMMARY" || fail 'M8 summary is missing'
    test "$(summary_metric reactor_enabled)" -eq 1 || fail 'reactor_enabled is not one'
    for KEY in reactor_epoll_waits reactor_wake_events reactor_timer_expirations reactor_socket_events reactor_loop_read reactor_loop_write reactor_loop_misc; do
        VALUE=$(summary_metric "$KEY")
        test -n "$VALUE" && test "$VALUE" -gt 0 || fail "$KEY is not nonzero"
    done
    test "$(summary_metric puback_unexpected)" -eq 0 || fail 'unexpected PUBACK is nonzero'
    test "$(summary_metric queue_drop)" -eq 0 || fail 'queue drop is nonzero'
    test "$(summary_metric spool_pending)" -eq 0 || fail 'spool pending is nonzero'
    if [ "$PHASE" = state1 ]; then
        test "$(summary_metric spool_state_recoveries)" -eq 1 || fail 'state recovery count is not one'
    fi
    {
        echo "stopped_at=$(date -Ins 2>/dev/null)"
        echo "$SUMMARY"
        cat "$EVIDENCE/$PHASE.stop.signal-exit.txt"
        cat "$EVIDENCE/$PHASE.wait-exit.txt"
        echo 'summary_assertions=PASS'
    } > "$EVIDENCE/$PHASE.stop.result.txt"
    cat "$EVIDENCE/$PHASE.stop.result.txt"
    echo "M8_BOARD_STOP_PASS phase=$PHASE"
}

case "$ACTION" in
setup)
    test -f "$STAGE/incoming/gatewayd" || fail 'incoming gatewayd is missing'
    test -f "$STAGE/incoming/gateway.conf" || fail 'incoming config is missing'
    test "$(sha256sum "$STAGE/incoming/gatewayd" | awk '{print $1}')" = "$EXPECTED_GATEWAY_SHA" || fail 'incoming gatewayd hash mismatch'
    test -e "$LIB_LINK" || fail 'target libmosquitto link is missing'
    LIB_REAL=$(readlink -f "$LIB_LINK")
    test "$(sha256sum "$LIB_REAL" | awk '{print $1}')" = "$EXPECTED_LIBRARY_SHA" || fail 'target libmosquitto hash mismatch'
    test ! -e "$RUN_ROOT" || fail 'refusing to overwrite existing run root'
    assert_no_gateway
    mkdir -p "$EVIDENCE" "$MAIN" "$RUN_ROOT/corruption" "$STAGE/bin" || fail 'mkdir failed'
    chmod 700 "$RUN_ROOT" "$EVIDENCE" "$MAIN" "$RUN_ROOT/corruption" "$STAGE" "$STAGE/bin" || fail 'chmod failed'
    cp "$STAGE/incoming/gatewayd" "$GATEWAY" || fail 'binary deployment copy failed'
    cp "$STAGE/incoming/gateway.conf" "$MAIN/gateway.conf" || fail 'config deployment copy failed'
    chmod 700 "$GATEWAY"
    chmod 600 "$MAIN/gateway.conf"
    cat > "$RUN_ROOT/run_gateway_phase.sh" <<'M8_WRAPPER'
#!/bin/sh
set +e
umask 077
RUN_ROOT=/var/lib/gatewayd-m8-test-20260901T170636
EVIDENCE=$RUN_ROOT/evidence
GATEWAY=/tmp/m8-reactor-gate-20260901T170636/bin/gatewayd
LIB_DIR=/tmp/m7/lib
PHASE=$1
CONFIG=$2
echo "phase=$PHASE" > "$EVIDENCE/$PHASE.lifecycle.txt"
echo "wrapper_pid=$$" >> "$EVIDENCE/$PHASE.lifecycle.txt"
echo "started_at=$(date -Ins 2>/dev/null)" >> "$EVIDENCE/$PHASE.lifecycle.txt"
LD_LIBRARY_PATH="$LIB_DIR" "$GATEWAY" --config "$CONFIG" --run-mqtt > "$EVIDENCE/$PHASE.gateway.stdout.log" 2> "$EVIDENCE/$PHASE.gateway.stderr.log" &
PID=$!
echo "$PID" > "$EVIDENCE/$PHASE.gateway.pid"
echo "gateway_pid=$PID" >> "$EVIDENCE/$PHASE.lifecycle.txt"
wait "$PID"
WAIT_EXIT=$?
echo "wait_exit=$WAIT_EXIT" > "$EVIDENCE/$PHASE.wait-exit.txt"
echo "ended_at=$(date -Ins 2>/dev/null)" >> "$EVIDENCE/$PHASE.lifecycle.txt"
exit "$WAIT_EXIT"
M8_WRAPPER
    chmod 700 "$RUN_ROOT/run_gateway_phase.sh" || fail 'wrapper chmod failed'
    {
        echo "setup_at=$(date -Ins 2>/dev/null)"
        uname -a
        id
        cat /proc/mounts
        df -h "$RUN_ROOT" /tmp
        ls -ld "$RUN_ROOT" "$EVIDENCE" "$MAIN" "$STAGE" "$STAGE/bin"
    } > "$EVIDENCE/board_and_storage.txt" 2>&1
    grep -q '^/dev/root / ext4 rw,' "$EVIDENCE/board_and_storage.txt" || fail 'persistent run root is not proven writable ext4'
    sha256sum "$GATEWAY" "$LIB_REAL" "$MAIN/gateway.conf" > "$EVIDENCE/deployed_sha256.txt" || fail 'sha256 audit failed'
    file "$GATEWAY" "$LIB_REAL" > "$EVIDENCE/deployed_file.txt" 2>&1
    readelf -h "$GATEWAY" > "$EVIDENCE/gateway_readelf_header.txt" 2>&1
    readelf -l "$GATEWAY" > "$EVIDENCE/gateway_readelf_program.txt" 2>&1
    readelf -d "$GATEWAY" > "$EVIDENCE/gateway_readelf_dynamic.txt" 2>&1
    LD_LIBRARY_PATH="$LIB_DIR" ldd "$GATEWAY" > "$EVIDENCE/gateway_ldd.txt" 2>&1 || fail 'ldd failed'
    grep -q 'not found' "$EVIDENCE/gateway_ldd.txt" && fail 'ldd reports missing library'
    ip -details -statistics link show can0 > "$EVIDENCE/can_before.txt" 2>&1 || fail 'can0 read-only query failed'
    grep -Eq '<[^>]*UP' "$EVIDENCE/can_before.txt" || fail 'can0 is not UP'
    grep -q 'bitrate 500000' "$EVIDENCE/can_before.txt" || fail 'can0 bitrate does not match existing baseline'
    LD_LIBRARY_PATH="$LIB_DIR" "$GATEWAY" --config "$MAIN/gateway.conf" --print-config > "$EVIDENCE/config_check.stdout.log" 2> "$EVIDENCE/config_check.stderr.log"
    CONFIG_EXIT=$?
    echo "config_check_exit=$CONFIG_EXIT" > "$EVIDENCE/config_check.exit.txt"
    test "$CONFIG_EXIT" -eq 0 || fail 'config check failed'
    echo 'M8_BOARD_SETUP_PASS'
    ;;
start-main1) start_phase main1 ;;
start-main2) start_phase main2 ;;
start-state1) start_phase state1 ;;
snapshot-main)
    test -n "$LABEL" || fail 'snapshot label is required'
    snapshot_to_file "$LABEL" "$MAIN/gateway.conf"
    ;;
snapshot-state)
    test -n "$LABEL" || fail 'snapshot label is required'
    snapshot_to_file "$LABEL" "$STATE_DIR/gateway.conf"
    ;;
kill-main1)
    MARKER=$EVIDENCE/kill9_once.marker
    PID=$(cat "$EVIDENCE/main1.gateway.pid" 2>/dev/null)
    test -n "$PID" || fail 'main1 PID is missing'
    test -r "/proc/$PID/comm" || fail 'main1 gateway is not alive'
    grep -qx gatewayd "/proc/$PID/comm" || fail 'main1 PID comm mismatch'
    test "$(sha256sum "/proc/$PID/exe" | awk '{print $1}')" = "$EXPECTED_GATEWAY_SHA" || fail 'main1 executable hash mismatch'
    tr '\000' ' ' < "/proc/$PID/cmdline" > "$EVIDENCE/main1.kill9.process.txt"
    echo >> "$EVIDENCE/main1.kill9.process.txt"
    grep -Fq "$MAIN/gateway.conf" "$EVIDENCE/main1.kill9.process.txt" || fail 'main1 command line is outside this run'
    test ! -e "$MARKER" || fail 'kill9 marker exists; refusing a second SIGKILL'
    SPOOL=$MAIN/spool.data
    STATE=$MAIN/spool.state
    {
        echo "captured_at=$(date -Ins 2>/dev/null)"
        echo "gateway_pid=$PID"
        echo "spool_size=$(wc -c < "$SPOOL")"
        sha256sum "$SPOOL" "$STATE"
        cat "$EVIDENCE/main1.kill9.process.txt"
    } > "$EVIDENCE/main1.kill9.before.txt"
    ( set -C; : > "$MARKER" ) 2>/dev/null || fail 'cannot create one-shot SIGKILL marker'
    echo "command=kill -9 $PID" > "$EVIDENCE/main1.kill9.command.txt"
    echo "issued_at=$(date -Ins 2>/dev/null)" >> "$EVIDENCE/main1.kill9.command.txt"
    kill -9 "$PID"
    KILL_EXIT=$?
    echo "kill9_exit=$KILL_EXIT" > "$EVIDENCE/main1.kill9.signal-exit.txt"
    test "$KILL_EXIT" -eq 0 || fail 'authorized SIGKILL failed; do not retry'
    COUNT=0
    while [ -e "/proc/$PID" ] && [ "$COUNT" -lt 30 ]; do sleep 1; COUNT=$((COUNT + 1)); done
    test ! -e "/proc/$PID" || fail 'SIGKILL target did not disappear'
    COUNT=0
    while [ ! -s "$EVIDENCE/main1.wait-exit.txt" ] && [ "$COUNT" -lt 20 ]; do sleep 1; COUNT=$((COUNT + 1)); done
    grep -q '^wait_exit=137$' "$EVIDENCE/main1.wait-exit.txt" || fail 'SIGKILL wrapper wait exit is not 137'
    {
        echo "captured_at=$(date -Ins 2>/dev/null)"
        echo 'gateway_process_gone=true'
        echo "spool_size=$(wc -c < "$SPOOL")"
        sha256sum "$SPOOL" "$STATE"
        cat "$EVIDENCE/main1.kill9.signal-exit.txt"
        cat "$EVIDENCE/main1.wait-exit.txt"
    } > "$EVIDENCE/main1.kill9.after.txt"
    cat "$EVIDENCE/main1.kill9.before.txt" "$EVIDENCE/main1.kill9.after.txt"
    echo 'M8_BOARD_SIGKILL_PASS'
    ;;
stop-main2) stop_phase main2 ;;
make-state-copy)
    assert_no_gateway
    test ! -e "$STATE_DIR" || fail 'refusing to overwrite state-copy directory'
    test -f "$MAIN/spool.data" && test -f "$MAIN/spool.state" || fail 'main spool/state missing'
    mkdir -p "$STATE_DIR" || fail 'state-copy mkdir failed'
    chmod 700 "$STATE_DIR"
    cp -p "$MAIN/spool.data" "$STATE_DIR/spool.data" || fail 'spool copy failed'
    cp -p "$MAIN/spool.state" "$STATE_DIR/spool.state" || fail 'state copy failed'
    cp -p "$MAIN/gateway.conf" "$STATE_DIR/gateway.conf" || fail 'config copy failed'
    sed -i 's#^spool_path=.*#spool_path=/var/lib/gatewayd-m8-test-20260901T170636/corruption/state/spool.data#' "$STATE_DIR/gateway.conf" || fail 'state config rewrite failed'
    cp -p "$STATE_DIR/spool.data" "$STATE_DIR/spool.data.before"
    cp -p "$STATE_DIR/spool.state" "$STATE_DIR/spool.state.before"
    cp -p "$STATE_DIR/gateway.conf" "$STATE_DIR/gateway.conf.before"
    BEFORE_BYTE=$(dd if="$STATE_DIR/spool.state" bs=1 skip=0 count=1 2>/dev/null | od -An -v -tx1 | tr -d ' ')
    test "$BEFORE_BYTE" = 47 || fail 'state magic first byte is not G'
    {
        echo 'injection=replace first state magic byte'
        echo 'offset=0'
        echo "before_byte_hex=$BEFORE_BYTE"
        echo 'after_byte_hex=00'
        sha256sum "$STATE_DIR/spool.data" "$STATE_DIR/spool.state" "$STATE_DIR/gateway.conf"
    } > "$EVIDENCE/state_injection.before.txt"
    printf '\000' | dd of="$STATE_DIR/spool.state" bs=1 seek=0 count=1 conv=notrunc 2> "$EVIDENCE/state_injection.dd.stderr.log"
    DD_EXIT=$?
    echo "dd_exit=$DD_EXIT" > "$EVIDENCE/state_injection.dd-exit.txt"
    test "$DD_EXIT" -eq 0 || fail 'state corruption write failed'
    sync
    AFTER_BYTE=$(dd if="$STATE_DIR/spool.state" bs=1 skip=0 count=1 2>/dev/null | od -An -v -tx1 | tr -d ' ')
    test "$AFTER_BYTE" = 00 || fail 'state corrupted byte mismatch'
    {
        echo "after_byte_hex=$AFTER_BYTE"
        sha256sum "$STATE_DIR/spool.data" "$STATE_DIR/spool.state" "$STATE_DIR/gateway.conf"
        cat "$EVIDENCE/state_injection.dd-exit.txt"
    } > "$EVIDENCE/state_injection.after.txt"
    LD_LIBRARY_PATH="$LIB_DIR" "$GATEWAY" --config "$STATE_DIR/gateway.conf" --print-config > "$EVIDENCE/state_config_check.stdout.log" 2> "$EVIDENCE/state_config_check.stderr.log"
    test "$?" -eq 0 || fail 'state-copy config check failed'
    cat "$EVIDENCE/state_injection.before.txt" "$EVIDENCE/state_injection.after.txt"
    echo 'M8_BOARD_STATE_COPY_PASS'
    ;;
stop-state1) stop_phase state1 ;;
final-audit)
    assert_no_gateway
    ip -details -statistics link show can0 > "$EVIDENCE/can_after.txt" 2>&1 || fail 'can0 final read failed'
    {
        echo "audited_at=$(date -Ins 2>/dev/null)"
        echo 'gateway_process_count=0'
        ls -l "$MAIN" "$STATE_DIR"
        sha256sum "$GATEWAY" "$LIB_DIR/libmosquitto.so.2.0.11" "$MAIN/spool.data" "$MAIN/spool.state" "$STATE_DIR/spool.data" "$STATE_DIR/spool.state"
    } > "$EVIDENCE/final_process_and_hashes.txt" 2>&1
    cat "$EVIDENCE/final_process_and_hashes.txt" "$EVIDENCE/can_after.txt"
    echo 'M8_BOARD_FINAL_AUDIT_PASS'
    ;;
export)
    assert_no_gateway
    ARCHIVE=$STAGE/board-export.tar
    test ! -e "$ARCHIVE" || fail 'refusing to overwrite board export archive'
    tar -cf "$ARCHIVE" -C /var/lib "$(basename "$RUN_ROOT")" || fail 'board export tar failed'
    sha256sum "$ARCHIVE" > "$STAGE/board-export.tar.sha256" || fail 'archive sha256 failed'
    ls -l "$ARCHIVE"
    cat "$STAGE/board-export.tar.sha256"
    echo 'M8_BOARD_EXPORT_PASS'
    ;;
*) fail "unsupported action: $ACTION" ;;
esac

