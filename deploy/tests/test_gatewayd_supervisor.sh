#!/bin/sh

set -u

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/../.." && pwd)
SUPERVISOR=$REPO_ROOT/deploy/init.d/gatewayd
FAKE_GATEWAY=$SCRIPT_DIR/fixtures/fake_gatewayd.sh
INITTAB_FRAGMENT=$REPO_ROOT/deploy/inittab/gatewayd.respawn
TEST_ROOT=$(mktemp -d "${TMPDIR:-/tmp}/gatewayd-m9-test.XXXXXX") || exit 1
supervisor_pid=

cleanup()
{
    if [ -n "$supervisor_pid" ] && kill -0 "$supervisor_pid" 2>/dev/null; then
        kill -TERM "$supervisor_pid" 2>/dev/null || true
        wait "$supervisor_pid" 2>/dev/null || true
    fi
    rm -rf "$TEST_ROOT"
}
trap 'cleanup' EXIT INT TERM

fail()
{
    printf 'FAIL: %s\n' "$*" >&2
    exit 1
}

wait_for_count()
{
    pattern=$1
    expected=$2
    file=$3
    attempts=100
    while [ "$attempts" -gt 0 ]; do
        actual=$(grep -c "$pattern" "$file" 2>/dev/null || true)
        [ "$actual" -ge "$expected" ] && return 0
        sleep 0.1
        attempts=$((attempts - 1))
    done
    return 1
}

wait_for_file()
{
    target=$1
    attempts=100
    while [ "$attempts" -gt 0 ]; do
        [ -s "$target" ] && return 0
        sleep 0.1
        attempts=$((attempts - 1))
    done
    return 1
}

write_environment()
{
    run_dir=$1
    environment_file=$2
    mode_file=$3
    gateway_log=$4
    once_file=$5
    config_file=$6
    {
        printf 'GATEWAYD_BIN=%s\n' "$FAKE_GATEWAY"
        printf 'GATEWAYD_CONFIG=%s\n' "$config_file"
        printf 'GATEWAYD_RUN_DIR=%s\n' "$run_dir"
        printf 'GATEWAYD_RESTART_LIMIT=3\n'
        printf 'GATEWAYD_STABLE_SEC=10\n'
        printf 'GATEWAYD_COOLDOWN_SEC=2\n'
        printf 'GATEWAYD_STOP_TIMEOUT_SEC=5\n'
        printf 'FAKE_GATEWAY_MODE=%s\n' "$mode_file"
        printf 'FAKE_GATEWAY_LOG=%s\n' "$gateway_log"
        printf 'FAKE_GATEWAY_ONCE=%s\n' "$once_file"
        printf 'export FAKE_GATEWAY_MODE FAKE_GATEWAY_LOG FAKE_GATEWAY_ONCE\n'
    } > "$environment_file"
}

run_service()
{
    env_file=$1
    action=$2
    GATEWAYD_ENV_FILE=$env_file "$SUPERVISOR" "$action"
}

[ -x "$SUPERVISOR" ] || fail "supervisor is not executable"
[ -x "$FAKE_GATEWAY" ] || fail "fake gateway is not executable"

if ! grep -F -x 'null::respawn:/etc/init.d/gatewayd supervise' "$INITTAB_FRAGMENT" >/dev/null; then
    fail "BusyBox respawn entry is missing"
fi
if grep -r -n -i 'systemd\|systemctl' "$REPO_ROOT/deploy/init.d" \
        "$REPO_ROOT/deploy/inittab" >/dev/null; then
    fail "systemd reference found in deployment payload"
fi

# 一次异常退出后必须由同一 supervisor 拉起新子进程。
case1=$TEST_ROOT/case1
mkdir -p "$case1/run"
printf '%s\n' crash_once > "$case1/mode"
: > "$case1/gateway.conf"
: > "$case1/gateway.log"
write_environment "$case1/run" "$case1/env" "$case1/mode" \
    "$case1/gateway.log" "$case1/once" "$case1/gateway.conf"
GATEWAYD_ENV_FILE=$case1/env "$SUPERVISOR" supervise > "$case1/supervisor.log" 2>&1 &
supervisor_pid=$!
wait_for_count '^start ' 2 "$case1/gateway.log" || fail "abnormal exit was not restarted"
grep -F 'status=42' "$case1/supervisor.log" >/dev/null || fail "crash status was not logged"

# 受控 restart 必须替换子 PID；stop 后 supervisor 保持但不再启动子进程；start 可恢复。
old_pid=$(sed -n '1p' "$case1/run/gatewayd.pid")
run_service "$case1/env" restart >/dev/null || fail "controlled restart command failed"
new_pid=$(sed -n '1p' "$case1/run/gatewayd.pid")
[ "$new_pid" != "$old_pid" ] || fail "controlled restart kept the old child pid"
run_service "$case1/env" stop >/dev/null || fail "stop command failed"
[ -f "$case1/run/disabled" ] || fail "stop did not create the disabled marker"
starts_after_stop=$(grep -c '^start ' "$case1/gateway.log")
sleep 1
[ "$(grep -c '^start ' "$case1/gateway.log")" -eq "$starts_after_stop" ] ||
    fail "disabled service restarted unexpectedly"
run_service "$case1/env" start >/dev/null || fail "start command failed"
wait_for_count '^start ' $((starts_after_stop + 1)) "$case1/gateway.log" ||
    fail "start did not re-enable the service"
kill -TERM "$supervisor_pid"
wait "$supervisor_pid" || fail "supervisor did not exit cleanly"
supervisor_pid=

# 三次快速失败后必须先冷却；冷却结束后仍可恢复，避免永久锁死。
case2=$TEST_ROOT/case2
mkdir -p "$case2/run"
printf '%s\n' crash > "$case2/mode"
: > "$case2/gateway.conf"
: > "$case2/gateway.log"
write_environment "$case2/run" "$case2/env" "$case2/mode" \
    "$case2/gateway.log" "$case2/once" "$case2/gateway.conf"
GATEWAYD_ENV_FILE=$case2/env "$SUPERVISOR" supervise > "$case2/supervisor.log" 2>&1 &
supervisor_pid=$!
wait_for_count '^start ' 3 "$case2/gateway.log" || fail "rapid failures were not observed"
wait_for_count 'event=restart_storm' 1 "$case2/supervisor.log" || fail "restart storm was not detected"
starts_at_cooldown=$(grep -c '^start ' "$case2/gateway.log")
sleep 1
[ "$(grep -c '^start ' "$case2/gateway.log")" -eq "$starts_at_cooldown" ] ||
    fail "cooldown did not suppress immediate respawn"
printf '%s\n' hold > "$case2/mode"
wait_for_count '^start ' $((starts_at_cooldown + 1)) "$case2/gateway.log" ||
    fail "service did not recover after cooldown"
wait_for_file "$case2/run/gatewayd.pid" || fail "recovered child pid is missing"
kill -TERM "$supervisor_pid"
wait "$supervisor_pid" || fail "storm-case supervisor did not exit cleanly"
supervisor_pid=

printf '%s\n' 'M9_BUSYBOX_SUPERVISOR_TEST PASS'
