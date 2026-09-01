#!/bin/sh

set +e
BASE=/var/lib/gatewayd/20260901T204152p0800-m9-board
EXPECTED_BIN_SHA=6e8729417b3dc40c10a413459de5eca9be43ce58dfcc8a3b12e91f5c8d7ef958
EXPECTED_LIB_SHA=b32c8ac4defb2b2920fba2e42f263869508c42e3c1719440db37ffc8d8c2f636
EXPECTED_INITTAB_SHA=fef0324868e70582fef1d76c8dd3bcd6025ae2159ba383a8585dc091d9dfad84
EXPECTED_SCRIPT_SHA=b75f599efe4582f4ff57437f391269b1d9ac73ab67ad1681e70232c0277c65a1
EXPECTED_ENV_SHA=ddd71394b0cbaaec01168d9fc394d14fe919d004267b5f4b7bbb57b65b0c6061
EXPECTED_CONFIG_SHA=0437a6ea950931da958988c47041c6392ee3a9c14b2d19f888491eb06b1fccb0
gate=0

classify()
{
    label=$1
    supervisor_count=0
    child_count=0
    unrelated_gatewayd_count=0
    supervisor_pid_found=
    child_pid_found=
    for p in /proc/[0-9]*; do
        [ -r "$p/comm" ] || continue
        pid=${p#/proc/}
        comm=$(sed -n '1p' "$p/comm" 2>/dev/null)
        cmdline=$(tr '\000' ' ' < "$p/cmdline" 2>/dev/null)
        exe=$(readlink -f "$p/exe" 2>/dev/null)
        is_supervisor=0
        is_child=0
        case "$cmdline" in
            '/bin/sh /etc/init.d/gatewayd supervise '|'/bin/ash /etc/init.d/gatewayd supervise ')
                is_supervisor=1; supervisor_count=$((supervisor_count + 1)); supervisor_pid_found=$pid;;
        esac
        if [ "$exe" = /opt/gatewayd/bin/gatewayd ]; then
            exe_sha=$(sha256sum "$exe" 2>/dev/null | sed -n 's/ .*//p')
            if [ "$exe_sha" = "$EXPECTED_BIN_SHA" ] && [ "$cmdline" = '/opt/gatewayd/bin/gatewayd --config /etc/gatewayd/gateway.conf --run-mqtt ' ]; then
                is_child=1; child_count=$((child_count + 1)); child_pid_found=$pid
            fi
        else
            exe_sha=
        fi
        if [ "$comm" = gatewayd ] && [ "$is_supervisor" -eq 0 ] && [ "$is_child" -eq 0 ]; then unrelated_gatewayd_count=$((unrelated_gatewayd_count + 1)); fi
        if [ "$is_supervisor" -eq 1 ] || [ "$is_child" -eq 1 ] || [ "$comm" = gatewayd ]; then
            echo "$label pid=$pid comm=$comm cmdline=$cmdline exe=$exe exe_sha256=$exe_sha class_supervisor=$is_supervisor class_child=$is_child"
        fi
    done
    echo "$label supervisor_count=$supervisor_count verified_child_count=$child_count unrelated_gatewayd_count=$unrelated_gatewayd_count"
    echo "$label supervisor_pid=$supervisor_pid_found child_pid=$child_pid_found"
}

echo "post_reboot_started_at=$(date -Ins 2>/dev/null)"
echo '--- boot identity and persistent marker ---'
boot_id_after=$(cat /proc/sys/kernel/random/boot_id 2>/dev/null)
boot_id_before=$(sed -n 's/^boot_id_before=//p' "$BASE/pre_reboot.marker" 2>/dev/null)
echo "boot_id_before=$boot_id_before"
echo "boot_id_after=$boot_id_after"
[ -n "$boot_id_before" ] && [ -n "$boot_id_after" ] && [ "$boot_id_before" != "$boot_id_after" ] || gate=1
cat "$BASE/pre_reboot.marker" 2>&1
date 2>&1
date -Ins 2>&1
cat /proc/uptime 2>&1

echo '--- PID1 BusyBox identity after real reboot ---'
printf 'pid1_comm='; cat /proc/1/comm 2>&1
printf 'pid1_cmdline='; tr '\000' ' ' < /proc/1/cmdline 2>/dev/null; echo
printf 'pid1_exe='; readlink -f /proc/1/exe 2>&1
ldd /sbin/init 2>&1
strings /lib/libbusybox.so.1.31.1 2>&1 | grep -E -i 'BusyBox v1\.31\.1|respawn|reloading /etc/inittab' | head -n 80

echo '--- installed files, exact inittab and dynamic dependencies after reboot ---'
for f in /etc/inittab /etc/init.d/gatewayd /etc/default/gatewayd /etc/gatewayd/gateway.conf /opt/gatewayd/bin/gatewayd /opt/gatewayd/lib/libmosquitto.so.2.0.11 /opt/gatewayd/lib/libmosquitto.so.1; do
    ls -ld "$f" 2>&1
    stat -c 'mode=%a uid=%u gid=%g size=%s inode=%i path=%n' "$f" 2>&1
    [ -f "$f" ] && sha256sum "$f" 2>&1
done
cat /etc/inittab 2>&1
[ "$(sha256sum /etc/inittab | sed -n 's/ .*//p')" = "$EXPECTED_INITTAB_SHA" ] || gate=1
[ "$(sha256sum /etc/init.d/gatewayd | sed -n 's/ .*//p')" = "$EXPECTED_SCRIPT_SHA" ] || gate=1
[ "$(sha256sum /etc/default/gatewayd | sed -n 's/ .*//p')" = "$EXPECTED_ENV_SHA" ] || gate=1
[ "$(sha256sum /etc/gatewayd/gateway.conf | sed -n 's/ .*//p')" = "$EXPECTED_CONFIG_SHA" ] || gate=1
[ "$(sha256sum /opt/gatewayd/bin/gatewayd | sed -n 's/ .*//p')" = "$EXPECTED_BIN_SHA" ] || gate=1
[ "$(sha256sum /opt/gatewayd/lib/libmosquitto.so.2.0.11 | sed -n 's/ .*//p')" = "$EXPECTED_LIB_SHA" ] || gate=1
[ "$(grep -F -x -c 'null::respawn:/etc/init.d/gatewayd supervise' /etc/inittab)" -eq 1 ] || gate=1
[ "$(find /etc/init.d -maxdepth 1 -type f -name 'S??gatewayd' 2>/dev/null | wc -l)" -eq 0 ] || gate=1
LD_LIBRARY_PATH=/opt/gatewayd/lib ldd /opt/gatewayd/bin/gatewayd 2>&1

echo '--- wait for and classify boot-started service ---'
attempts=60
while [ "$attempts" -gt 0 ]; do
    classify boot_probe >/tmp/m9-post-reboot-probe.$$ 2>&1
    if [ "$supervisor_count" -eq 1 ] && [ "$child_count" -eq 1 ] && [ "$unrelated_gatewayd_count" -eq 0 ]; then break; fi
    attempts=$((attempts - 1))
    sleep 1
done
cat /tmp/m9-post-reboot-probe.$$ 2>&1
rm -f /tmp/m9-post-reboot-probe.$$
[ "$attempts" -gt 0 ] || gate=1
classify boot_sample1
sup1=$supervisor_pid_found
child1=$child_pid_found
[ "$supervisor_count" -eq 1 ] || gate=1
[ "$child_count" -eq 1 ] || gate=1
[ "$unrelated_gatewayd_count" -eq 0 ] || gate=1
[ "$(sed -n '1p' /var/run/gatewayd/supervisor.pid 2>/dev/null)" = "$sup1" ] || gate=1
[ "$(sed -n '1p' /var/run/gatewayd/gatewayd.pid 2>/dev/null)" = "$child1" ] || gate=1
child_ppid=$(sed -n 's/^[^)]*) [^ ]* \([0-9][0-9]*\) .*/\1/p' "/proc/$child1/stat" 2>/dev/null)
echo "boot_sample1 child_ppid=$child_ppid"
[ "$child_ppid" = "$sup1" ] || gate=1
grep -F '/opt/gatewayd/lib/libmosquitto' "/proc/$child1/maps" 2>/dev/null || gate=1
tr '\000' '\n' < "/proc/$child1/environ" 2>/dev/null | grep -E '^LD_LIBRARY_PATH=|^GATEWAYD_' || true
/etc/init.d/gatewayd status 2>&1
status1=$?
echo "boot_sample1_status_exit=$status1"
[ "$status1" -eq 0 ] || gate=1

echo '--- 10 second final stability and uniqueness ---'
sleep 10
classify boot_sample2
sup2=$supervisor_pid_found
child2=$child_pid_found
[ "$supervisor_count" -eq 1 ] || gate=1
[ "$child_count" -eq 1 ] || gate=1
[ "$unrelated_gatewayd_count" -eq 0 ] || gate=1
echo "final_supervisor_pid_before=$sup1 after=$sup2"
echo "final_child_pid_before=$child1 after=$child2"
[ "$sup1" = "$sup2" ] || gate=1
[ "$child1" = "$child2" ] || gate=1

echo '--- final persistent and external read-only state ---'
find "$BASE" -maxdepth 2 -type f -exec stat -c 'mode=%a uid=%u gid=%g size=%s mtime=%Y path=%n' {} \; 2>&1
if [ -e /tmp/m9-board-gate-20260901T204152p0800 ]; then echo 'tmp_stage_after_reboot=PRESENT'; else echo 'tmp_stage_after_reboot=ABSENT'; fi
test_process_count=0
for p in /proc/[0-9]*; do
    [ -r "$p/cmdline" ] || continue
    cmdline=$(tr '\000' ' ' < "$p/cmdline" 2>/dev/null)
    case "$cmdline" in *'storm-busybox-ash'*|*'gatewayd-m9-test.'*) test_process_count=$((test_process_count + 1)); echo "unexpected_test_process=$cmdline";; esac
done
echo "final_test_process_count=$test_process_count"
[ "$test_process_count" -eq 0 ] || gate=1
ip -details -statistics link show can0 2>&1
if command -v ss >/dev/null 2>&1; then ss -antp 2>&1; else netstat -antp 2>&1; fi
ps w 2>&1
date -Ins 2>&1
cat /proc/uptime 2>&1

echo "post_reboot_autostart_gate=$(if [ "$gate" -eq 0 ]; then echo PASS; else echo FAIL; fi)"
echo "post_reboot_completed_at=$(date -Ins 2>/dev/null)"
exit "$gate"
