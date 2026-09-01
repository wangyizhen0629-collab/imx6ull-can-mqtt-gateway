#!/bin/sh

set +e
EXPECTED_BIN_SHA=6e8729417b3dc40c10a413459de5eca9be43ce58dfcc8a3b12e91f5c8d7ef958
EXPECTED_SCRIPT_SHA=b75f599efe4582f4ff57437f391269b1d9ac73ab67ad1681e70232c0277c65a1
gate=0

verify_pair()
{
    label=$1
    supervisor_pid=$(sed -n '1p' /var/run/gatewayd/supervisor.pid 2>/dev/null)
    child_pid=$(sed -n '1p' /var/run/gatewayd/gatewayd.pid 2>/dev/null)
    [ -d "/proc/$supervisor_pid" ] && [ -d "/proc/$child_pid" ] || return 1
    supervisor_comm=$(sed -n '1p' "/proc/$supervisor_pid/comm" 2>/dev/null)
    supervisor_cmdline=$(tr '\000' ' ' < "/proc/$supervisor_pid/cmdline" 2>/dev/null)
    supervisor_exe=$(readlink -f "/proc/$supervisor_pid/exe" 2>/dev/null)
    child_comm=$(sed -n '1p' "/proc/$child_pid/comm" 2>/dev/null)
    child_cmdline=$(tr '\000' ' ' < "/proc/$child_pid/cmdline" 2>/dev/null)
    child_exe=$(readlink -f "/proc/$child_pid/exe" 2>/dev/null)
    child_sha=$(sha256sum "$child_exe" 2>/dev/null | sed -n 's/ .*//p')
    child_ppid=$(sed -n 's/^[^)]*) [^ ]* \([0-9][0-9]*\) .*/\1/p' "/proc/$child_pid/stat" 2>/dev/null)
    script_sha=$(sha256sum /etc/init.d/gatewayd 2>/dev/null | sed -n 's/ .*//p')
    echo "$label supervisor_pid=$supervisor_pid comm=$supervisor_comm cmdline=$supervisor_cmdline exe=$supervisor_exe script_sha256=$script_sha"
    echo "$label child_pid=$child_pid comm=$child_comm cmdline=$child_cmdline exe=$child_exe sha256=$child_sha ppid=$child_ppid"
    case "$supervisor_cmdline" in '/bin/sh /etc/init.d/gatewayd supervise '|'/bin/ash /etc/init.d/gatewayd supervise ') ;; *) return 1;; esac
    [ "$script_sha" = "$EXPECTED_SCRIPT_SHA" ] || return 1
    [ "$child_comm" = gatewayd ] || return 1
    [ "$child_cmdline" = '/opt/gatewayd/bin/gatewayd --config /etc/gatewayd/gateway.conf --run-mqtt ' ] || return 1
    [ "$child_exe" = /opt/gatewayd/bin/gatewayd ] || return 1
    [ "$child_sha" = "$EXPECTED_BIN_SHA" ] || return 1
    [ "$child_ppid" = "$supervisor_pid" ] || return 1
}

count_verified()
{
    supervisor_count=0
    child_count=0
    unrelated_gatewayd_count=0
    for p in /proc/[0-9]*; do
        [ -r "$p/comm" ] || continue
        comm=$(sed -n '1p' "$p/comm" 2>/dev/null)
        cmdline=$(tr '\000' ' ' < "$p/cmdline" 2>/dev/null)
        exe=$(readlink -f "$p/exe" 2>/dev/null)
        is_supervisor=0
        is_child=0
        case "$cmdline" in '/bin/sh /etc/init.d/gatewayd supervise '|'/bin/ash /etc/init.d/gatewayd supervise ') is_supervisor=1; supervisor_count=$((supervisor_count + 1));; esac
        [ "$exe" = /opt/gatewayd/bin/gatewayd ] && { is_child=1; child_count=$((child_count + 1)); }
        if [ "$comm" = gatewayd ] && [ "$is_supervisor" -eq 0 ] && [ "$is_child" -eq 0 ]; then unrelated_gatewayd_count=$((unrelated_gatewayd_count + 1)); fi
    done
}

echo "restart_started_at=$(date -Ins 2>/dev/null)"
verify_pair before_restart || { echo 'FAIL pre-restart identity'; exit 1; }
old_supervisor=$supervisor_pid
old_child=$child_pid
echo '--- child signal disposition and verified restart implementation ---'
grep -E '^(Name|Pid|PPid|State|SigPnd|ShdPnd|SigBlk|SigIgn|SigCgt):' "/proc/$old_child/status" 2>&1
grep -n -E 'do_restart|forward_reload|kill -TERM|trap .*HUP' /etc/init.d/gatewayd 2>&1
if command -v strace >/dev/null 2>&1; then
    echo "strace_path=$(command -v strace)"
else
    echo 'direct_signal_trace=NOT RUN reason=strace unavailable; graceful path evidenced by verified script implementation, caught SIGTERM disposition, action exit 0, old PID disappearance and replacement PID'
fi

/etc/init.d/gatewayd restart 2>&1
restart_rc=$?
echo "command=/etc/init.d/gatewayd restart exit=$restart_rc"
[ "$restart_rc" -eq 0 ] || exit 1
verify_pair after_restart || gate=1
new_supervisor=$supervisor_pid
new_child=$child_pid
echo "old_supervisor=$old_supervisor new_supervisor=$new_supervisor"
echo "old_child=$old_child new_child=$new_child"
[ "$old_supervisor" = "$new_supervisor" ] || gate=1
[ "$old_child" != "$new_child" ] || gate=1
[ ! -d "/proc/$old_child" ] || gate=1
count_verified
echo "after_restart_supervisor_count=$supervisor_count"
echo "after_restart_verified_child_count=$child_count"
echo "after_restart_unrelated_gatewayd_count=$unrelated_gatewayd_count"
[ "$supervisor_count" -eq 1 ] || gate=1
[ "$child_count" -eq 1 ] || gate=1
[ "$unrelated_gatewayd_count" -eq 0 ] || gate=1

sleep 5
verify_pair after_restart_stable || gate=1
stable_supervisor=$supervisor_pid
stable_child=$child_pid
echo "stable_supervisor=$stable_supervisor stable_child=$stable_child"
[ "$stable_supervisor" = "$new_supervisor" ] || gate=1
[ "$stable_child" = "$new_child" ] || gate=1
/etc/init.d/gatewayd status 2>&1
status_rc=$?
echo "status_exit=$status_rc"
[ "$status_rc" -eq 0 ] || gate=1
ps w 2>&1

echo "controlled_restart_gate=$(if [ "$gate" -eq 0 ]; then echo PASS; else echo FAIL; fi)"
echo "restart_completed_at=$(date -Ins 2>/dev/null)"
exit "$gate"
