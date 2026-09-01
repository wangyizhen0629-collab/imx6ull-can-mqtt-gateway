#!/bin/sh

set +e
BASE=/var/lib/gatewayd/20260901T204152p0800-m9-board
BACKUP=$BASE/backup
QUARANTINE=$BASE/rollback-quarantine
EXPECTED_GATEWAY_SHA=6e8729417b3dc40c10a413459de5eca9be43ce58dfcc8a3b12e91f5c8d7ef958
rc=0

echo "rollback_started_at=$(date -Ins 2>/dev/null)"
if [ ! -f "$BACKUP/inittab.before" ]; then
    echo 'FAIL missing inittab backup'
    exit 1
fi
mkdir -p "$QUARANTINE" || exit 1

cp -p "$BACKUP/inittab.before" /etc/inittab.rollback.tmp || rc=1
[ "$rc" -eq 0 ] && mv /etc/inittab.rollback.tmp /etc/inittab || rc=1
echo 'rollback_inittab_restored=true'
sha256sum /etc/inittab 2>&1

if [ "$rc" -eq 0 ]; then
    kill -HUP 1 2>&1
    echo "rollback_pid1_hup_exit=$?"
fi
sleep 2

for p in /proc/[0-9]*; do
    [ -r "$p/cmdline" ] || continue
    pid=${p#/proc/}
    cmdline=$(tr '\000' ' ' < "$p/cmdline" 2>/dev/null)
    case "$cmdline" in
        *'/etc/init.d/gatewayd supervise'*)
            echo "rollback_verified_supervisor_pid=$pid cmdline=$cmdline"
            kill -TERM "$pid" 2>&1
            echo "rollback_supervisor_term_exit=$?"
            ;;
    esac
done
sleep 2

for p in /proc/[0-9]*; do
    [ -r "$p/comm" ] || continue
    pid=${p#/proc/}
    comm=$(sed -n '1p' "$p/comm" 2>/dev/null)
    [ "$comm" = gatewayd ] || continue
    exe=$(readlink -f "$p/exe" 2>/dev/null)
    exe_sha=$(sha256sum "$exe" 2>/dev/null | sed -n 's/ .*//p')
    cmdline=$(tr '\000' ' ' < "$p/cmdline" 2>/dev/null)
    if [ "$exe" = /opt/gatewayd/bin/gatewayd ] && [ "$exe_sha" = "$EXPECTED_GATEWAY_SHA" ]; then
        echo "rollback_verified_child_pid=$pid cmdline=$cmdline exe=$exe sha256=$exe_sha"
        kill -TERM "$pid" 2>&1
        echo "rollback_child_term_exit=$?"
    else
        echo "FAIL refusing unrelated gatewayd pid=$pid exe=$exe sha256=$exe_sha"
        rc=1
    fi
done
sleep 2

for item in \
    '/etc/init.d/gatewayd:etc.init.d.gatewayd' \
    '/etc/default/gatewayd:etc.default.gatewayd' \
    '/etc/gatewayd/gateway.conf:etc.gatewayd.gateway.conf'; do
    target=${item%%:*}
    key=${item#*:}
    if [ -f "$BACKUP/$key.before" ]; then
        cp -p "$BACKUP/$key.before" "$target.rollback.tmp" || rc=1
        [ "$rc" -eq 0 ] && mv "$target.rollback.tmp" "$target" || rc=1
    elif [ -f "$BACKUP/$key.absent" ] && [ -e "$target" ]; then
        safe_name=$(printf '%s' "$key" | tr '/' '.')
        mv "$target" "$QUARANTINE/$safe_name.current" || rc=1
    fi
done

echo "rollback_exit=$rc"
echo "rollback_completed_at=$(date -Ins 2>/dev/null)"
exit "$rc"
