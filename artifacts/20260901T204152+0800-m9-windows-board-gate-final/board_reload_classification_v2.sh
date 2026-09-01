#!/bin/sh

set +e
EXPECTED_BIN_SHA=6e8729417b3dc40c10a413459de5eca9be43ce58dfcc8a3b12e91f5c8d7ef958
EXPECTED_SCRIPT_SHA=b75f599efe4582f4ff57437f391269b1d9ac73ab67ad1681e70232c0277c65a1
EXPECTED_CONFIG_SHA=0437a6ea950931da958988c47041c6392ee3a9c14b2d19f888491eb06b1fccb0
gate=0

classify()
{
    label=$1
    supervisor_count=0
    child_count=0
    comm_gatewayd_count=0
    unrelated_gatewayd_count=0
    supervisor_pid_found=
    child_pid_found=
    for p in /proc/[0-9]*; do
        [ -r "$p/comm" ] || continue
        pid=${p#/proc/}
        comm=$(sed -n '1p' "$p/comm" 2>/dev/null)
        cmdline=$(tr '\000' ' ' < "$p/cmdline" 2>/dev/null)
        exe=$(readlink -f "$p/exe" 2>/dev/null)
        exe_sha=$(sha256sum "$exe" 2>/dev/null | sed -n 's/ .*//p')
        is_supervisor=0
        is_child=0
        case "$cmdline" in
            '/bin/sh /etc/init.d/gatewayd supervise '|'/bin/ash /etc/init.d/gatewayd supervise ')
                is_supervisor=1
                supervisor_count=$((supervisor_count + 1))
                supervisor_pid_found=$pid
                ;;
        esac
        if [ "$exe" = /opt/gatewayd/bin/gatewayd ] && [ "$exe_sha" = "$EXPECTED_BIN_SHA" ] &&
           [ "$cmdline" = '/opt/gatewayd/bin/gatewayd --config /etc/gatewayd/gateway.conf --run-mqtt ' ]; then
            is_child=1
            child_count=$((child_count + 1))
            child_pid_found=$pid
        fi
        [ "$comm" = gatewayd ] && comm_gatewayd_count=$((comm_gatewayd_count + 1))
        if [ "$comm" = gatewayd ] && [ "$is_supervisor" -eq 0 ] && [ "$is_child" -eq 0 ]; then
            unrelated_gatewayd_count=$((unrelated_gatewayd_count + 1))
        fi
        if [ "$is_supervisor" -eq 1 ] || [ "$is_child" -eq 1 ] || [ "$comm" = gatewayd ]; then
            echo "$label pid=$pid comm=$comm cmdline=$cmdline exe=$exe exe_sha256=$exe_sha class_supervisor=$is_supervisor class_child=$is_child"
        fi
    done
    echo "$label supervisor_count=$supervisor_count"
    echo "$label verified_child_count=$child_count"
    echo "$label comm_gatewayd_count=$comm_gatewayd_count"
    echo "$label unrelated_gatewayd_count=$unrelated_gatewayd_count"
    echo "$label supervisor_pid=$supervisor_pid_found child_pid=$child_pid_found"
}

echo "classification_started_at=$(date -Ins 2>/dev/null)"
classify sample1
sup1=$supervisor_pid_found
child1=$child_pid_found
[ "$supervisor_count" -eq 1 ] || gate=1
[ "$child_count" -eq 1 ] || gate=1
[ "$comm_gatewayd_count" -eq 2 ] || gate=1
[ "$unrelated_gatewayd_count" -eq 0 ] || gate=1
[ "$(sed -n '1p' /var/run/gatewayd/supervisor.pid 2>/dev/null)" = "$sup1" ] || gate=1
[ "$(sed -n '1p' /var/run/gatewayd/gatewayd.pid 2>/dev/null)" = "$child1" ] || gate=1
child_ppid=$(sed -n 's/^[^)]*) [^ ]* \([0-9][0-9]*\) .*/\1/p' "/proc/$child1/stat" 2>/dev/null)
echo "sample1 child_ppid=$child_ppid"
[ "$child_ppid" = "$sup1" ] || gate=1
[ "$(sha256sum /etc/init.d/gatewayd 2>/dev/null | sed -n 's/ .*//p')" = "$EXPECTED_SCRIPT_SHA" ] || gate=1
[ "$(sha256sum /etc/gatewayd/gateway.conf 2>/dev/null | sed -n 's/ .*//p')" = "$EXPECTED_CONFIG_SHA" ] || gate=1
/etc/init.d/gatewayd status 2>&1
status1=$?
echo "sample1_status_exit=$status1"
[ "$status1" -eq 0 ] || gate=1

sleep 5
classify sample2
sup2=$supervisor_pid_found
child2=$child_pid_found
[ "$supervisor_count" -eq 1 ] || gate=1
[ "$child_count" -eq 1 ] || gate=1
[ "$unrelated_gatewayd_count" -eq 0 ] || gate=1
echo "stable_supervisor_pid_before=$sup1 after=$sup2"
echo "stable_child_pid_before=$child1 after=$child2"
[ "$sup1" = "$sup2" ] || gate=1
[ "$child1" = "$child2" ] || gate=1

echo '--- exact identities and loaded private library ---'
tr '\000' '\n' < "/proc/$child2/environ" 2>/dev/null | grep -E '^LD_LIBRARY_PATH=|^GATEWAYD_' || true
grep -F '/opt/gatewayd/lib/libmosquitto' "/proc/$child2/maps" 2>/dev/null || gate=1
ps w 2>&1
echo "reload_classification_v2=$(if [ "$gate" -eq 0 ]; then echo PASS; else echo FAIL; fi)"
echo "classification_completed_at=$(date -Ins 2>/dev/null)"
exit "$gate"
