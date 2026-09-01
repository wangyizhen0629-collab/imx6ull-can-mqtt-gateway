#!/bin/sh

set +e
STAGE=/tmp/m9-board-gate-20260901T204152p0800
BASE=/var/lib/gatewayd/20260901T204152p0800-m9-board
BACKUP=$BASE/backup
BIN_SOURCE=/tmp/m9-staging/gatewayd
LIB_SOURCE=/tmp/m7/lib/libmosquitto.so.2.0.11
EXPECTED_BIN_SHA=6e8729417b3dc40c10a413459de5eca9be43ce58dfcc8a3b12e91f5c8d7ef958
EXPECTED_LIB_SHA=b32c8ac4defb2b2920fba2e42f263869508c42e3c1719440db37ffc8d8c2f636
EXPECTED_INITTAB_SHA=77676429d4b24e2a93b8d7f7c3a9a35a37785600ab359e1c75ec6c8da11d1380
EXPECTED_SUPERVISOR_SHA=b75f599efe4582f4ff57437f391269b1d9ac73ab67ad1681e70232c0277c65a1
EXPECTED_FRAGMENT_SHA=7ce294625dcb80a59fb720960c1a2993453ccc6d027ca7021e7ce631573949bb
EXPECTED_TEST_SHA=507f143f5e2ca5063a45395f1fc0821291017ab8c2132744404794ec3b3ff57d
EXPECTED_FAKE_SHA=bc703ffce7d72dc96ee8492adcb3e1c64f12d81248c3d6c49b6393ea4921ec84
EXPECTED_CONFIG_SHA=0437a6ea950931da958988c47041c6392ee3a9c14b2d19f888491eb06b1fccb0
EXPECTED_ENV_SHA=ddd71394b0cbaaec01168d9fc394d14fe919d004267b5f4b7bbb57b65b0c6061
RESPAWN_LINE='null::respawn:/etc/init.d/gatewayd supervise'
gate=0

check_sha()
{
    path=$1
    expected=$2
    actual=$(sha256sum "$path" 2>/dev/null | sed -n 's/ .*//p')
    echo "path=$path expected_sha256=$expected actual_sha256=$actual"
    [ "$actual" = "$expected" ] || gate=1
}

echo "install_started_at=$(date -Ins 2>/dev/null)"
echo '--- recheck immutable inputs immediately before target writes ---'
check_sha "$BIN_SOURCE" "$EXPECTED_BIN_SHA"
check_sha "$LIB_SOURCE" "$EXPECTED_LIB_SHA"
check_sha "$STAGE/incoming/gatewayd" "$EXPECTED_SUPERVISOR_SHA"
check_sha "$STAGE/incoming/gatewayd.respawn" "$EXPECTED_FRAGMENT_SHA"
check_sha "$STAGE/incoming/test_gatewayd_supervisor.sh" "$EXPECTED_TEST_SHA"
check_sha "$STAGE/incoming/fake_gatewayd.sh" "$EXPECTED_FAKE_SHA"
check_sha "$STAGE/incoming/gateway.v2.conf" "$EXPECTED_CONFIG_SHA"
check_sha "$STAGE/incoming/gatewayd.v2.env" "$EXPECTED_ENV_SHA"
check_sha "$STAGE/incoming/board_rollback.sh" 11f02ac2110b6afc9b22337468d0c5f1ec02348007b644a9a643e036d90172c7
check_sha /etc/inittab "$EXPECTED_INITTAB_SHA"

for path in /etc/init.d/gatewayd /etc/default/gatewayd /etc/gatewayd/gateway.conf /opt/gatewayd; do
    if [ -e "$path" ]; then echo "FAIL preexisting_managed_path=$path"; gate=1; fi
done
supervisor_count=0
gatewayd_count=0
for p in /proc/[0-9]*; do
    [ -r "$p/comm" ] || continue
    comm=$(sed -n '1p' "$p/comm" 2>/dev/null)
    cmdline=$(tr '\000' ' ' < "$p/cmdline" 2>/dev/null)
    case "$cmdline" in *'/etc/init.d/gatewayd supervise'*) supervisor_count=$((supervisor_count + 1));; esac
    [ "$comm" = gatewayd ] && gatewayd_count=$((gatewayd_count + 1))
done
echo "preinstall_supervisor_count=$supervisor_count"
echo "preinstall_gatewayd_count=$gatewayd_count"
[ "$supervisor_count" -eq 0 ] || gate=1
[ "$gatewayd_count" -eq 0 ] || gate=1

echo '--- staging permissions, ELF and config ---'
for f in "$STAGE"/incoming/* "$BIN_SOURCE" "$LIB_SOURCE"; do
    [ -e "$f" ] || continue
    stat -c 'mode=%a uid=%u gid=%g size=%s inode=%i path=%n' "$f" 2>&1
done
file "$BIN_SOURCE" "$LIB_SOURCE" 2>&1
LD_LIBRARY_PATH=${LIB_SOURCE%/*} ldd "$BIN_SOURCE" 2>&1
echo '--- staged private config (raw private/public redacted) ---'
cat "$STAGE/incoming/gateway.v2.conf" 2>&1
echo '--- staged environment ---'
cat "$STAGE/incoming/gatewayd.v2.env" 2>&1
config_keys=$(grep -E '^[A-Za-z_][A-Za-z0-9_]*=' "$STAGE/incoming/gateway.v2.conf" | wc -l)
unique_keys=$(grep -E '^[A-Za-z_][A-Za-z0-9_]*=' "$STAGE/incoming/gateway.v2.conf" | sed 's/=.*//' | sort -u | wc -l)
echo "config_keys=$config_keys unique_keys=$unique_keys"
[ "$config_keys" -eq 14 ] && [ "$unique_keys" -eq 14 ] || gate=1

if [ "$gate" -ne 0 ]; then
    echo 'install_prewrite_gate=FAIL'
    exit 1
fi
echo 'install_prewrite_gate=PASS'

echo '--- create persistent per-run backup/spool root ---'
mkdir -p "$BACKUP" || exit 1
chmod 700 "$BASE" "$BACKUP" || exit 1
if [ -e "$BACKUP/inittab.before" ]; then echo 'FAIL backup already exists'; exit 1; fi
cp -p /etc/inittab "$BACKUP/inittab.before" || exit 1
sha256sum "$BACKUP/inittab.before" 2>&1
for item in \
    '/etc/init.d/gatewayd:etc.init.d.gatewayd' \
    '/etc/default/gatewayd:etc.default.gatewayd' \
    '/etc/gatewayd/gateway.conf:etc.gatewayd.gateway.conf'; do
    target=${item%%:*}
    key=${item#*:}
    if [ -e "$target" ]; then
        cp -p "$target" "$BACKUP/$key.before" || exit 1
    else
        : > "$BACKUP/$key.absent" || exit 1
    fi
done
cp "$STAGE/incoming/board_rollback.sh" "$BACKUP/rollback_m9.sh" || exit 1
chown 0:0 "$BACKUP/rollback_m9.sh" || exit 1
chmod 700 "$BACKUP/rollback_m9.sh" || exit 1

echo '--- install binary, private library and four managed files ---'
mkdir -p /opt/gatewayd/bin /opt/gatewayd/lib /etc/default /etc/gatewayd || exit 1
cp "$BIN_SOURCE" /opt/gatewayd/bin/gatewayd.tmp || exit 1
chown 0:0 /opt/gatewayd/bin/gatewayd.tmp || exit 1
chmod 755 /opt/gatewayd/bin/gatewayd.tmp || exit 1
mv /opt/gatewayd/bin/gatewayd.tmp /opt/gatewayd/bin/gatewayd || exit 1

cp "$LIB_SOURCE" /opt/gatewayd/lib/libmosquitto.so.2.0.11.tmp || exit 1
chown 0:0 /opt/gatewayd/lib/libmosquitto.so.2.0.11.tmp || exit 1
chmod 644 /opt/gatewayd/lib/libmosquitto.so.2.0.11.tmp || exit 1
mv /opt/gatewayd/lib/libmosquitto.so.2.0.11.tmp /opt/gatewayd/lib/libmosquitto.so.2.0.11 || exit 1
ln -s libmosquitto.so.2.0.11 /opt/gatewayd/lib/libmosquitto.so.1 || exit 1

cp "$STAGE/incoming/gatewayd" /etc/init.d/gatewayd.tmp || exit 1
chown 0:0 /etc/init.d/gatewayd.tmp || exit 1
chmod 755 /etc/init.d/gatewayd.tmp || exit 1
mv /etc/init.d/gatewayd.tmp /etc/init.d/gatewayd || exit 1

cp "$STAGE/incoming/gatewayd.v2.env" /etc/default/gatewayd.tmp || exit 1
chown 0:0 /etc/default/gatewayd.tmp || exit 1
chmod 644 /etc/default/gatewayd.tmp || exit 1
mv /etc/default/gatewayd.tmp /etc/default/gatewayd || exit 1

cp "$STAGE/incoming/gateway.v2.conf" /etc/gatewayd/gateway.conf.tmp || exit 1
chown 0:0 /etc/gatewayd/gateway.conf.tmp || exit 1
chmod 600 /etc/gatewayd/gateway.conf.tmp || exit 1
mv /etc/gatewayd/gateway.conf.tmp /etc/gatewayd/gateway.conf || exit 1

inittab_mode=$(stat -c '%a' /etc/inittab)
inittab_uid=$(stat -c '%u' /etc/inittab)
inittab_gid=$(stat -c '%g' /etc/inittab)
cp /etc/inittab /etc/inittab.m9.tmp || exit 1
printf '\n%s\n' "$RESPAWN_LINE" >> /etc/inittab.m9.tmp || exit 1
chown "$inittab_uid:$inittab_gid" /etc/inittab.m9.tmp || exit 1
chmod "$inittab_mode" /etc/inittab.m9.tmp || exit 1
mv /etc/inittab.m9.tmp /etc/inittab || exit 1

echo '--- installed permission/hash/dynamic audit ---'
for f in /etc/inittab /etc/init.d/gatewayd /etc/default/gatewayd /etc/gatewayd/gateway.conf /opt/gatewayd/bin/gatewayd /opt/gatewayd/lib/libmosquitto.so.2.0.11 /opt/gatewayd/lib/libmosquitto.so.1 "$BACKUP/rollback_m9.sh"; do
    ls -ld "$f" 2>&1
    stat -c 'mode=%a uid=%u gid=%g size=%s inode=%i path=%n' "$f" 2>&1
    [ -f "$f" ] && sha256sum "$f" 2>&1
done
LD_LIBRARY_PATH=/opt/gatewayd/lib ldd /opt/gatewayd/bin/gatewayd 2>&1
LD_LIBRARY_PATH=/opt/gatewayd/lib /opt/gatewayd/bin/gatewayd --version 2>&1
echo "installed_version_exit=$?"

echo '--- complete inittab after install, before reload ---'
cat /etc/inittab 2>&1
line_count=$(grep -F -x -c "$RESPAWN_LINE" /etc/inittab 2>/dev/null)
rcs_count=$(find /etc/init.d -maxdepth 1 -type f -name 'S??gatewayd' 2>/dev/null | wc -l)
echo "respawn_exact_line_count=$line_count"
echo "rcs_gatewayd_count=$rcs_count"
[ "$line_count" -eq 1 ] || gate=1
[ "$rcs_count" -eq 0 ] || gate=1

echo '--- process state remains stopped before explicit PID1 reload ---'
/etc/init.d/gatewayd status 2>&1
status_rc=$?
echo "service_status_exit=$status_rc"
supervisor_count=0
gatewayd_count=0
for p in /proc/[0-9]*; do
    [ -r "$p/comm" ] || continue
    comm=$(sed -n '1p' "$p/comm" 2>/dev/null)
    cmdline=$(tr '\000' ' ' < "$p/cmdline" 2>/dev/null)
    case "$cmdline" in *'/etc/init.d/gatewayd supervise'*) supervisor_count=$((supervisor_count + 1));; esac
    [ "$comm" = gatewayd ] && gatewayd_count=$((gatewayd_count + 1))
done
echo "postinstall_prereload_supervisor_count=$supervisor_count"
echo "postinstall_prereload_gatewayd_count=$gatewayd_count"
[ "$supervisor_count" -eq 0 ] || gate=1
[ "$gatewayd_count" -eq 0 ] || gate=1

echo "install_gate=$(if [ "$gate" -eq 0 ]; then echo PASS; else echo FAIL; fi)"
echo "install_completed_at=$(date -Ins 2>/dev/null)"
exit "$gate"
