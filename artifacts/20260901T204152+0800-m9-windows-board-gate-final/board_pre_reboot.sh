#!/bin/sh

set +e
BASE=/var/lib/gatewayd/20260901T204152p0800-m9-board
EXPECTED_BIN_SHA=6e8729417b3dc40c10a413459de5eca9be43ce58dfcc8a3b12e91f5c8d7ef958
EXPECTED_INITTAB_SHA=fef0324868e70582fef1d76c8dd3bcd6025ae2159ba383a8585dc091d9dfad84
EXPECTED_SCRIPT_SHA=b75f599efe4582f4ff57437f391269b1d9ac73ab67ad1681e70232c0277c65a1
gate=0

supervisor_pid=$(sed -n '1p' /var/run/gatewayd/supervisor.pid 2>/dev/null)
child_pid=$(sed -n '1p' /var/run/gatewayd/gatewayd.pid 2>/dev/null)
supervisor_cmd=$(tr '\000' ' ' < "/proc/$supervisor_pid/cmdline" 2>/dev/null)
supervisor_exe=$(readlink -f "/proc/$supervisor_pid/exe" 2>/dev/null)
child_comm=$(sed -n '1p' "/proc/$child_pid/comm" 2>/dev/null)
child_cmd=$(tr '\000' ' ' < "/proc/$child_pid/cmdline" 2>/dev/null)
child_exe=$(readlink -f "/proc/$child_pid/exe" 2>/dev/null)
child_sha=$(sha256sum "$child_exe" 2>/dev/null | sed -n 's/ .*//p')
child_ppid=$(sed -n 's/^[^)]*) [^ ]* \([0-9][0-9]*\) .*/\1/p' "/proc/$child_pid/stat" 2>/dev/null)
boot_id=$(cat /proc/sys/kernel/random/boot_id 2>/dev/null)
inittab_sha=$(sha256sum /etc/inittab 2>/dev/null | sed -n 's/ .*//p')
script_sha=$(sha256sum /etc/init.d/gatewayd 2>/dev/null | sed -n 's/ .*//p')

echo "pre_reboot_started_at=$(date -Ins 2>/dev/null)"
echo "boot_id_before=$boot_id"
date -Ins 2>&1
cat /proc/uptime 2>&1
printf 'pid1_comm='; cat /proc/1/comm 2>&1
printf 'pid1_cmdline='; tr '\000' ' ' < /proc/1/cmdline 2>/dev/null; echo
printf 'pid1_exe='; readlink -f /proc/1/exe 2>&1
echo "supervisor_pid_before=$supervisor_pid cmdline=$supervisor_cmd exe=$supervisor_exe script_sha256=$script_sha"
echo "child_pid_before=$child_pid comm=$child_comm cmdline=$child_cmd exe=$child_exe sha256=$child_sha ppid=$child_ppid"
echo "inittab_sha256_before=$inittab_sha"
cat /etc/inittab 2>&1
/etc/init.d/gatewayd status 2>&1
status_rc=$?
echo "status_exit=$status_rc"

case "$supervisor_cmd" in '/bin/sh /etc/init.d/gatewayd supervise '|'/bin/ash /etc/init.d/gatewayd supervise ') ;; *) gate=1;; esac
[ "$script_sha" = "$EXPECTED_SCRIPT_SHA" ] || gate=1
[ "$child_comm" = gatewayd ] || gate=1
[ "$child_cmd" = '/opt/gatewayd/bin/gatewayd --config /etc/gatewayd/gateway.conf --run-mqtt ' ] || gate=1
[ "$child_exe" = /opt/gatewayd/bin/gatewayd ] || gate=1
[ "$child_sha" = "$EXPECTED_BIN_SHA" ] || gate=1
[ "$child_ppid" = "$supervisor_pid" ] || gate=1
[ "$inittab_sha" = "$EXPECTED_INITTAB_SHA" ] || gate=1
[ "$status_rc" -eq 0 ] || gate=1
[ "$(grep -F -x -c 'null::respawn:/etc/init.d/gatewayd supervise' /etc/inittab)" -eq 1 ] || gate=1

echo '--- persistent spool/backup and read-only external state before reboot ---'
find "$BASE" -maxdepth 2 -type f -exec stat -c 'mode=%a uid=%u gid=%g size=%s mtime=%Y path=%n' {} \; 2>&1
ip -details -statistics link show can0 2>&1
if command -v ss >/dev/null 2>&1; then ss -antp 2>&1; else netstat -antp 2>&1; fi
ps w 2>&1

if [ "$gate" -eq 0 ]; then
    umask 077
    marker="$BASE/pre_reboot.marker"
    if [ -e "$marker" ]; then
        echo "FAIL marker already exists=$marker"
        gate=1
    else
        {
            echo "boot_id_before=$boot_id"
            echo "supervisor_pid_before=$supervisor_pid"
            echo "child_pid_before=$child_pid"
            echo "inittab_sha256_before=$inittab_sha"
            echo "binary_sha256=$child_sha"
        } > "$marker" || gate=1
        chmod 600 "$marker" 2>/dev/null || gate=1
        sync "$marker" 2>/dev/null || sync
        stat -c 'mode=%a uid=%u gid=%g size=%s path=%n' "$marker" 2>&1
        sha256sum "$marker" 2>&1
        cat "$marker" 2>&1
    fi
fi

echo "pre_reboot_gate=$(if [ "$gate" -eq 0 ]; then echo PASS; else echo FAIL; fi)"
echo "pre_reboot_completed_at=$(date -Ins 2>/dev/null)"
exit "$gate"
