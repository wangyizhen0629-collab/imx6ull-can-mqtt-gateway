#!/bin/sh

set +e
BIN=/tmp/m9-staging/gatewayd
EXPECTED_LIB_SHA=b32c8ac4defb2b2920fba2e42f263869508c42e3c1719440db37ffc8d8c2f636
EXPECTED_INITTAB_SHA=77676429d4b24e2a93b8d7f7c3a9a35a37785600ab359e1c75ec6c8da11d1380
gate=0
selected_lib=

echo "remote_started_at=$(date -Ins 2>/dev/null)"
echo '--- libmosquitto candidates and symlinks ---'
for root in /tmp /opt /var/lib /lib /usr/lib; do
    [ -e "$root" ] || continue
    find "$root" \( -type f -o -type l \) -name 'libmosquitto.so*' -print 2>/dev/null
done | sort -u | while IFS= read -r candidate; do
    ls -ld "$candidate" 2>&1
    resolved=$(readlink -f "$candidate" 2>/dev/null)
    [ -n "$resolved" ] && [ -f "$resolved" ] && {
        stat -c 'mode=%a uid=%u gid=%g size=%s inode=%i path=%n' "$resolved" 2>&1
        sha256sum "$resolved" 2>&1
        file "$resolved" 2>&1
    }
done

for candidate in \
    /tmp/m7/lib/libmosquitto.so.2.0.11 \
    /tmp/m6/lib/libmosquitto.so.2.0.11 \
    /tmp/m8/lib/libmosquitto.so.2.0.11 \
    /tmp/m8-reactor-gate-20260901T170636/lib/libmosquitto.so.2.0.11 \
    /tmp/m7/lib/libmosquitto.so.1 \
    /tmp/m6/lib/libmosquitto.so.1; do
    [ -e "$candidate" ] || continue
    resolved=$(readlink -f "$candidate" 2>/dev/null)
    [ -f "$resolved" ] || continue
    candidate_sha=$(sha256sum "$resolved" 2>/dev/null | sed -n 's/ .*//p')
    if [ "$candidate_sha" = "$EXPECTED_LIB_SHA" ]; then
        selected_lib=$resolved
        break
    fi
done
echo "expected_lib_sha256=$EXPECTED_LIB_SHA"
echo "selected_lib=$selected_lib"
if [ -z "$selected_lib" ]; then
    echo 'FAIL matching libmosquitto not found in approved candidates'
    gate=1
else
    selected_sha=$(sha256sum "$selected_lib" 2>/dev/null | sed -n 's/ .*//p')
    selected_dir=${selected_lib%/*}
    echo "selected_lib_sha256=$selected_sha"
    stat -c 'mode=%a uid=%u gid=%g size=%s inode=%i path=%n' "$selected_lib" 2>&1
    file "$selected_lib" 2>&1
    readelf -h "$selected_lib" 2>&1
    readelf -d "$selected_lib" 2>&1
    [ "$selected_sha" = "$EXPECTED_LIB_SHA" ] || gate=1
    lib_header=$(readelf -h "$selected_lib" 2>&1)
    printf '%s\n' "$lib_header" | grep -E 'Class:[[:space:]]+ELF32' >/dev/null || gate=1
    printf '%s\n' "$lib_header" | grep -E 'Machine:[[:space:]]+ARM' >/dev/null || gate=1

    echo '--- binary dynamic resolution with selected private library ---'
    LD_LIBRARY_PATH="$selected_dir" ldd "$BIN" 2>&1
    ldd_rc=$?
    echo "ldd_exit=$ldd_rc"
    [ "$ldd_rc" -eq 0 ] || gate=1
    if LD_LIBRARY_PATH="$selected_dir" ldd "$BIN" 2>&1 | grep -F 'not found' >/dev/null; then gate=1; fi

    echo '--- binary safe version execution ---'
    LD_LIBRARY_PATH="$selected_dir" "$BIN" --version 2>&1
    version_rc=$?
    echo "version_exit=$version_rc"
    [ "$version_rc" -eq 0 ] || gate=1
fi

echo '--- PID 1 and BusyBox identity ---'
printf 'pid1_comm='; cat /proc/1/comm 2>&1
printf 'pid1_cmdline='; tr '\000' ' ' < /proc/1/cmdline 2>/dev/null; echo
printf 'pid1_exe='; readlink -f /proc/1/exe 2>&1
stat -c 'mode=%a uid=%u gid=%g size=%s inode=%i path=%n' /sbin/init 2>&1
sha256sum /sbin/init 2>&1
ldd /sbin/init 2>&1
for f in /lib/libbusybox.so.1.31.1 /bin/sh /bin/ash; do
    [ -e "$f" ] || continue
    ls -li "$f" 2>&1
    file "$f" 2>&1
    sha256sum "$f" 2>&1
done
strings /lib/libbusybox.so.1.31.1 2>&1 | grep -E -i 'BusyBox v1\.31\.1|respawn|reloading /etc/inittab' | head -n 80

echo '--- time and uptime (no UTC correctness claim) ---'
date 2>&1
date -Ins 2>&1
cat /proc/uptime 2>&1

echo '--- root filesystem and destination capacity ---'
df -T / /opt /var/lib 2>&1
df -k / /opt /var/lib 2>&1
mount 2>&1 | grep -E ' on / | on /var | on /opt ' || true
stat -f -c 'fs_type=%T path=%n' / /opt /var/lib 2>&1

echo '--- managed paths before deployment ---'
for f in /etc/inittab /etc/init.d/gatewayd /etc/default/gatewayd /etc/gatewayd/gateway.conf /opt/gatewayd; do
    if [ -e "$f" ]; then
        stat -c 'mode=%a uid=%u gid=%g size=%s inode=%i path=%n' "$f" 2>&1
        [ -f "$f" ] && sha256sum "$f" 2>&1
    else
        echo "MISSING $f"
    fi
done
inittab_sha=$(sha256sum /etc/inittab 2>/dev/null | sed -n 's/ .*//p')
echo "expected_inittab_sha256=$EXPECTED_INITTAB_SHA"
echo "actual_inittab_sha256=$inittab_sha"
[ "$inittab_sha" = "$EXPECTED_INITTAB_SHA" ] || gate=1
for f in /etc/init.d/gatewayd /etc/default/gatewayd /etc/gatewayd/gateway.conf /opt/gatewayd; do
    if [ -e "$f" ]; then
        echo "FAIL preexisting_managed_path=$f"
        gate=1
    fi
done

echo '--- complete inittab before deployment ---'
cat /etc/inittab 2>&1

echo '--- process tree and counts before deployment ---'
supervisor_count=0
gatewayd_count=0
for p in /proc/[0-9]*; do
    [ -r "$p/comm" ] || continue
    pid=${p#/proc/}
    comm=$(sed -n '1p' "$p/comm" 2>/dev/null)
    cmdline=$(tr '\000' ' ' < "$p/cmdline" 2>/dev/null)
    exe=$(readlink -f "$p/exe" 2>/dev/null)
    case "$cmdline" in *'/etc/init.d/gatewayd supervise'*) supervisor_count=$((supervisor_count + 1));; esac
    [ "$comm" = gatewayd ] && gatewayd_count=$((gatewayd_count + 1))
    case "$cmdline" in *gatewayd*) printf 'pid=%s comm=%s cmdline=%s exe=%s\n' "$pid" "$comm" "$cmdline" "$exe";; esac
done
echo "supervisor_count=$supervisor_count"
echo "gatewayd_count=$gatewayd_count"
ps w 2>&1
[ "$supervisor_count" -eq 0 ] || gate=1
[ "$gatewayd_count" -eq 0 ] || gate=1

echo '--- CAN and socket state read-only ---'
ip -details -statistics link show can0 2>&1
if command -v ss >/dev/null 2>&1; then ss -antp 2>&1; else netstat -antp 2>&1; fi

echo "library_predeploy_gate=$(if [ "$gate" -eq 0 ]; then echo PASS; else echo FAIL; fi)"
echo "remote_completed_at=$(date -Ins 2>/dev/null)"
exit "$gate"
