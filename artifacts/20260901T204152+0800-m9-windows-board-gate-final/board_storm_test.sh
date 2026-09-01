#!/bin/ash

set +e
STAGE=/tmp/m9-board-gate-20260901T204152p0800
STORM_ROOT=$STAGE/storm-busybox-ash
TMP_ROOT=$STORM_ROOT/tmp
EXPECTED_BIN_SHA=6e8729417b3dc40c10a413459de5eca9be43ce58dfcc8a3b12e91f5c8d7ef958
EXPECTED_SUPERVISOR_SOURCE_SHA=b75f599efe4582f4ff57437f391269b1d9ac73ab67ad1681e70232c0277c65a1
EXPECTED_TEST_SHA=507f143f5e2ca5063a45395f1fc0821291017ab8c2132744404794ec3b3ff57d
EXPECTED_FAKE_SOURCE_SHA=bc703ffce7d72dc96ee8492adcb3e1c64f12d81248c3d6c49b6393ea4921ec84
EXPECTED_FRAGMENT_SHA=7ce294625dcb80a59fb720960c1a2993453ccc6d027ca7021e7ce631573949bb
gate=0

verify_production()
{
    label=$1
    prod_supervisor=$(sed -n '1p' /var/run/gatewayd/supervisor.pid 2>/dev/null)
    prod_child=$(sed -n '1p' /var/run/gatewayd/gatewayd.pid 2>/dev/null)
    [ -d "/proc/$prod_supervisor" ] && [ -d "/proc/$prod_child" ] || return 1
    sup_cmd=$(tr '\000' ' ' < "/proc/$prod_supervisor/cmdline" 2>/dev/null)
    child_cmd=$(tr '\000' ' ' < "/proc/$prod_child/cmdline" 2>/dev/null)
    child_exe=$(readlink -f "/proc/$prod_child/exe" 2>/dev/null)
    child_sha=$(sha256sum "$child_exe" 2>/dev/null | sed -n 's/ .*//p')
    echo "$label production_supervisor=$prod_supervisor cmdline=$sup_cmd"
    echo "$label production_child=$prod_child cmdline=$child_cmd exe=$child_exe sha256=$child_sha"
    case "$sup_cmd" in '/bin/sh /etc/init.d/gatewayd supervise '|'/bin/ash /etc/init.d/gatewayd supervise ') ;; *) return 1;; esac
    [ "$child_cmd" = '/opt/gatewayd/bin/gatewayd --config /etc/gatewayd/gateway.conf --run-mqtt ' ] || return 1
    [ "$child_exe" = /opt/gatewayd/bin/gatewayd ] || return 1
    [ "$child_sha" = "$EXPECTED_BIN_SHA" ] || return 1
}

echo "storm_started_at=$(date -Ins 2>/dev/null)"
echo '--- target BusyBox ash identity ---'
printf 'outer_shell_pid=%s comm=' "$$"; cat "/proc/$$/comm" 2>&1
printf 'outer_shell_cmdline='; tr '\000' ' ' < "/proc/$$/cmdline" 2>/dev/null; echo
printf 'ash_exe='; readlink -f /bin/ash 2>&1
sha256sum /bin/ash /lib/libbusybox.so.1.31.1 2>&1
ldd /bin/ash 2>&1
strings /lib/libbusybox.so.1.31.1 2>&1 | grep -E 'BusyBox v1\.31\.1' | head -n 10

verify_production before_storm || { echo 'FAIL production identity before storm'; exit 1; }
prod_supervisor_before=$prod_supervisor
prod_child_before=$prod_child

if [ -e "$STORM_ROOT" ]; then echo "FAIL storm root already exists=$STORM_ROOT"; exit 1; fi
mkdir -p "$STORM_ROOT/repo/deploy/init.d" "$STORM_ROOT/repo/deploy/inittab" \
    "$STORM_ROOT/repo/deploy/tests/fixtures" "$TMP_ROOT" || exit 1
chmod 700 "$STORM_ROOT" "$TMP_ROOT" || exit 1
cp "$STAGE/incoming/gatewayd" "$STORM_ROOT/repo/deploy/init.d/gatewayd" || exit 1
cp "$STAGE/incoming/gatewayd.respawn" "$STORM_ROOT/repo/deploy/inittab/gatewayd.respawn" || exit 1
cp "$STAGE/incoming/test_gatewayd_supervisor.sh" "$STORM_ROOT/repo/deploy/tests/test_gatewayd_supervisor.sh" || exit 1
cp "$STAGE/incoming/fake_gatewayd.sh" "$STORM_ROOT/repo/deploy/tests/fixtures/fake_gatewayd.sh" || exit 1

echo '--- exact repository-input hashes before ash-only shebang adaptation ---'
sha256sum "$STORM_ROOT/repo/deploy/init.d/gatewayd" \
    "$STORM_ROOT/repo/deploy/inittab/gatewayd.respawn" \
    "$STORM_ROOT/repo/deploy/tests/test_gatewayd_supervisor.sh" \
    "$STORM_ROOT/repo/deploy/tests/fixtures/fake_gatewayd.sh" 2>&1
[ "$(sha256sum "$STORM_ROOT/repo/deploy/init.d/gatewayd" | sed -n 's/ .*//p')" = "$EXPECTED_SUPERVISOR_SOURCE_SHA" ] || gate=1
[ "$(sha256sum "$STORM_ROOT/repo/deploy/inittab/gatewayd.respawn" | sed -n 's/ .*//p')" = "$EXPECTED_FRAGMENT_SHA" ] || gate=1
[ "$(sha256sum "$STORM_ROOT/repo/deploy/tests/test_gatewayd_supervisor.sh" | sed -n 's/ .*//p')" = "$EXPECTED_TEST_SHA" ] || gate=1
[ "$(sha256sum "$STORM_ROOT/repo/deploy/tests/fixtures/fake_gatewayd.sh" | sed -n 's/ .*//p')" = "$EXPECTED_FAKE_SOURCE_SHA" ] || gate=1

supervisor_body_before=$(tail -n +2 "$STORM_ROOT/repo/deploy/init.d/gatewayd" | sha256sum | sed -n 's/ .*//p')
fake_body_before=$(tail -n +2 "$STORM_ROOT/repo/deploy/tests/fixtures/fake_gatewayd.sh" | sha256sum | sed -n 's/ .*//p')
sed -i '1s|^#!/bin/sh$|#!/bin/ash|' "$STORM_ROOT/repo/deploy/init.d/gatewayd" || exit 1
sed -i '1s|^#!/bin/sh$|#!/bin/ash|' "$STORM_ROOT/repo/deploy/tests/fixtures/fake_gatewayd.sh" || exit 1
supervisor_body_after=$(tail -n +2 "$STORM_ROOT/repo/deploy/init.d/gatewayd" | sha256sum | sed -n 's/ .*//p')
fake_body_after=$(tail -n +2 "$STORM_ROOT/repo/deploy/tests/fixtures/fake_gatewayd.sh" | sha256sum | sed -n 's/ .*//p')
echo "supervisor_body_sha_before=$supervisor_body_before after=$supervisor_body_after"
echo "fake_body_sha_before=$fake_body_before after=$fake_body_after"
[ "$supervisor_body_before" = "$supervisor_body_after" ] || gate=1
[ "$fake_body_before" = "$fake_body_after" ] || gate=1
head -n 1 "$STORM_ROOT/repo/deploy/init.d/gatewayd" "$STORM_ROOT/repo/deploy/tests/fixtures/fake_gatewayd.sh" 2>&1
chmod 755 "$STORM_ROOT/repo/deploy/init.d/gatewayd" \
    "$STORM_ROOT/repo/deploy/tests/test_gatewayd_supervisor.sh" \
    "$STORM_ROOT/repo/deploy/tests/fixtures/fake_gatewayd.sh" || exit 1
[ "$gate" -eq 0 ] || { echo 'FAIL storm input gate'; exit 1; }

echo '--- complete BusyBox ash -x supervisor/fake storm trace ---'
TMPDIR="$TMP_ROOT" /bin/ash -x "$STORM_ROOT/repo/deploy/tests/test_gatewayd_supervisor.sh" 2>&1
test_rc=$?
echo "command=TMPDIR=<isolated> /bin/ash -x deploy/tests/test_gatewayd_supervisor.sh exit=$test_rc"
[ "$test_rc" -eq 0 ] || gate=1

echo '--- cleanup and production isolation audit ---'
find "$TMP_ROOT" -mindepth 1 -maxdepth 3 -print 2>&1
remaining_tmp=$(find "$TMP_ROOT" -mindepth 1 -print 2>/dev/null | wc -l)
test_process_count=0
for p in /proc/[0-9]*; do
    [ -r "$p/cmdline" ] || continue
    cmdline=$(tr '\000' ' ' < "$p/cmdline" 2>/dev/null)
    case "$cmdline" in
        *"$STORM_ROOT"*|*'gatewayd-m9-test.'*)
            pid=${p#/proc/}
            echo "remaining_test_process pid=$pid cmdline=$cmdline"
            test_process_count=$((test_process_count + 1))
            ;;
    esac
done
echo "remaining_test_tmp_entries=$remaining_tmp"
echo "remaining_test_process_count=$test_process_count"
[ "$remaining_tmp" -eq 0 ] || gate=1
[ "$test_process_count" -eq 0 ] || gate=1

verify_production after_storm || gate=1
echo "production_supervisor_before=$prod_supervisor_before after=$prod_supervisor"
echo "production_child_before=$prod_child_before after=$prod_child"
[ "$prod_supervisor_before" = "$prod_supervisor" ] || gate=1
[ "$prod_child_before" = "$prod_child" ] || gate=1
/etc/init.d/gatewayd status 2>&1
status_rc=$?
echo "production_status_exit=$status_rc"
[ "$status_rc" -eq 0 ] || gate=1
ps w 2>&1

echo "busybox_storm_gate=$(if [ "$gate" -eq 0 ]; then echo PASS; else echo FAIL; fi)"
echo "storm_completed_at=$(date -Ins 2>/dev/null)"
exit "$gate"
