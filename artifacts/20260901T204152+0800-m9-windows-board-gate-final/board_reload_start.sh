#!/bin/sh

set +e
EXPECTED_BIN_SHA=6e8729417b3dc40c10a413459de5eca9be43ce58dfcc8a3b12e91f5c8d7ef958
EXPECTED_SCRIPT_SHA=b75f599efe4582f4ff57437f391269b1d9ac73ab67ad1681e70232c0277c65a1
EXPECTED_CONFIG_SHA=0437a6ea950931da958988c47041c6392ee3a9c14b2d19f888491eb06b1fccb0
RESPAWN_LINE='null::respawn:/etc/init.d/gatewayd supervise'
gate=0

read_pid()
{
    sed -n '1p' "$1" 2>/dev/null
}

count_processes()
{
    supervisor_count=0
    gatewayd_count=0
    for p in /proc/[0-9]*; do
        [ -r "$p/comm" ] || continue
        comm=$(sed -n '1p' "$p/comm" 2>/dev/null)
        cmdline=$(tr '\000' ' ' < "$p/cmdline" 2>/dev/null)
        case "$cmdline" in *'/etc/init.d/gatewayd supervise'*) supervisor_count=$((supervisor_count + 1));; esac
        [ "$comm" = gatewayd ] && gatewayd_count=$((gatewayd_count + 1))
    done
}

verify_pair()
{
    label=$1
    supervisor_pid=$(read_pid /var/run/gatewayd/supervisor.pid)
    child_pid=$(read_pid /var/run/gatewayd/gatewayd.pid)
    echo "$label supervisor_pid=$supervisor_pid child_pid=$child_pid"
    case "$supervisor_pid:$child_pid" in *[!0-9:]*|:*) return 1;; esac
    [ -d "/proc/$supervisor_pid" ] || return 1
    [ -d "/proc/$child_pid" ] || return 1
    supervisor_comm=$(sed -n '1p' "/proc/$supervisor_pid/comm" 2>/dev/null)
    supervisor_cmdline=$(tr '\000' ' ' < "/proc/$supervisor_pid/cmdline" 2>/dev/null)
    supervisor_exe=$(readlink -f "/proc/$supervisor_pid/exe" 2>/dev/null)
    child_comm=$(sed -n '1p' "/proc/$child_pid/comm" 2>/dev/null)
    child_cmdline=$(tr '\000' ' ' < "/proc/$child_pid/cmdline" 2>/dev/null)
    child_exe=$(readlink -f "/proc/$child_pid/exe" 2>/dev/null)
    child_sha=$(sha256sum "$child_exe" 2>/dev/null | sed -n 's/ .*//p')
    child_ppid=$(sed -n 's/^[^)]*) [^ ]* \([0-9][0-9]*\) .*/\1/p' "/proc/$child_pid/stat" 2>/dev/null)
    script_sha=$(sha256sum /etc/init.d/gatewayd 2>/dev/null | sed -n 's/ .*//p')
    config_sha=$(sha256sum /etc/gatewayd/gateway.conf 2>/dev/null | sed -n 's/ .*//p')
    echo "$label supervisor_comm=$supervisor_comm supervisor_cmdline=$supervisor_cmdline supervisor_exe=$supervisor_exe"
    echo "$label child_comm=$child_comm child_cmdline=$child_cmdline child_exe=$child_exe child_sha256=$child_sha child_ppid=$child_ppid"
    echo "$label script_sha256=$script_sha config_sha256=$config_sha"
    tr '\000' '\n' < "/proc/$child_pid/environ" 2>/dev/null | grep -E '^LD_LIBRARY_PATH=|^GATEWAYD_' || true
    grep -F '/opt/gatewayd/lib/libmosquitto' "/proc/$child_pid/maps" 2>/dev/null || true
    case "$supervisor_cmdline" in *'/etc/init.d/gatewayd supervise'*) ;; *) return 1;; esac
    [ "$child_comm" = gatewayd ] || return 1
    [ "$child_cmdline" = '/opt/gatewayd/bin/gatewayd --config /etc/gatewayd/gateway.conf --run-mqtt ' ] || return 1
    [ "$child_exe" = /opt/gatewayd/bin/gatewayd ] || return 1
    [ "$child_sha" = "$EXPECTED_BIN_SHA" ] || return 1
    [ "$child_ppid" = "$supervisor_pid" ] || return 1
    [ "$script_sha" = "$EXPECTED_SCRIPT_SHA" ] || return 1
    [ "$config_sha" = "$EXPECTED_CONFIG_SHA" ] || return 1
    return 0
}

echo "reload_started_at=$(date -Ins 2>/dev/null)"
echo '--- PID1 behavior revalidation immediately before HUP ---'
printf 'pid1_comm='; cat /proc/1/comm 2>&1
printf 'pid1_cmdline='; tr '\000' ' ' < /proc/1/cmdline 2>/dev/null; echo
printf 'pid1_exe='; readlink -f /proc/1/exe 2>&1
ldd /sbin/init 2>&1
strings /lib/libbusybox.so.1.31.1 2>&1 | grep -E -i 'BusyBox v1\.31\.1|respawn|reloading /etc/inittab' | head -n 80
line_count=$(grep -F -x -c "$RESPAWN_LINE" /etc/inittab 2>/dev/null)
echo "respawn_exact_line_count=$line_count"
[ "$line_count" -eq 1 ] || gate=1
count_processes
echo "before_hup_supervisor_count=$supervisor_count"
echo "before_hup_gatewayd_count=$gatewayd_count"
[ "$supervisor_count" -eq 0 ] || gate=1
[ "$gatewayd_count" -eq 0 ] || gate=1
if [ "$gate" -ne 0 ]; then echo 'pre_hup_gate=FAIL'; exit 1; fi
echo 'pre_hup_gate=PASS'

kill -HUP 1 2>&1
hup_rc=$?
echo "command=kill -HUP 1 exit=$hup_rc"
[ "$hup_rc" -eq 0 ] || exit 1

attempts=30
while [ "$attempts" -gt 0 ]; do
    if verify_pair after_hup_probe >/tmp/m9-reload-probe.$$ 2>&1; then break; fi
    attempts=$((attempts - 1))
    sleep 1
done
cat /tmp/m9-reload-probe.$$ 2>&1
rm -f /tmp/m9-reload-probe.$$
if [ "$attempts" -le 0 ]; then echo 'FAIL pair did not become valid'; exit 1; fi
verify_pair after_hup || gate=1
first_supervisor=$(read_pid /var/run/gatewayd/supervisor.pid)
first_child=$(read_pid /var/run/gatewayd/gatewayd.pid)
count_processes
echo "after_hup_supervisor_count=$supervisor_count"
echo "after_hup_gatewayd_count=$gatewayd_count"
[ "$supervisor_count" -eq 1 ] || gate=1
[ "$gatewayd_count" -eq 1 ] || gate=1
/etc/init.d/gatewayd status 2>&1
status_rc=$?
echo "status_exit=$status_rc"
[ "$status_rc" -eq 0 ] || gate=1

echo '--- 10 second stability/no-respawn observation ---'
sleep 10
verify_pair after_stability || gate=1
stable_supervisor=$(read_pid /var/run/gatewayd/supervisor.pid)
stable_child=$(read_pid /var/run/gatewayd/gatewayd.pid)
echo "first_supervisor=$first_supervisor stable_supervisor=$stable_supervisor"
echo "first_child=$first_child stable_child=$stable_child"
[ "$first_supervisor" = "$stable_supervisor" ] || gate=1
[ "$first_child" = "$stable_child" ] || gate=1
count_processes
echo "stable_supervisor_count=$supervisor_count"
echo "stable_gatewayd_count=$gatewayd_count"
[ "$supervisor_count" -eq 1 ] || gate=1
[ "$gatewayd_count" -eq 1 ] || gate=1

echo '--- installed service/spool and read-only external state ---'
ls -la /var/run/gatewayd 2>&1
find /var/lib/gatewayd/20260901T204152p0800-m9-board -maxdepth 2 -type f -exec stat -c 'mode=%a uid=%u gid=%g size=%s path=%n' {} \; 2>&1
ps w 2>&1
ip -details -statistics link show can0 2>&1
if command -v ss >/dev/null 2>&1; then ss -antp 2>&1; else netstat -antp 2>&1; fi
date -Ins 2>&1
cat /proc/uptime 2>&1

echo "reload_start_gate=$(if [ "$gate" -eq 0 ]; then echo PASS; else echo FAIL; fi)"
echo "reload_completed_at=$(date -Ins 2>/dev/null)"
exit "$gate"
