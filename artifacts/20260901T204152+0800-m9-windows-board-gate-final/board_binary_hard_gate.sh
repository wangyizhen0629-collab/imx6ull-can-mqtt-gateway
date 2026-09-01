#!/bin/sh

set +e
BIN=/tmp/m9-staging/gatewayd
EXPECTED_SIZE=104860
EXPECTED_SHA=6e8729417b3dc40c10a413459de5eca9be43ce58dfcc8a3b12e91f5c8d7ef958
gate=0

echo "remote_started_at=$(date -Ins 2>/dev/null)"
echo '--- required tools ---'
for tool in stat sha256sum file readelf ldd grep sed tr readlink; do
    command -v "$tool" 2>&1
    tool_rc=$?
    echo "tool=$tool exit=$tool_rc"
    [ "$tool_rc" -eq 0 ] || gate=1
done

echo '--- exact binary input ---'
if [ ! -f "$BIN" ]; then
    echo "FAIL missing=$BIN"
    gate=1
else
    stat -c 'mode=%a uid=%u gid=%g size=%s inode=%i path=%n' "$BIN" 2>&1
    stat_rc=$?
    actual_size=$(stat -c '%s' "$BIN" 2>/dev/null)
    actual_sha=$(sha256sum "$BIN" 2>/dev/null | sed -n 's/ .*//p')
    echo "expected_size=$EXPECTED_SIZE"
    echo "actual_size=$actual_size"
    echo "expected_sha256=$EXPECTED_SHA"
    echo "actual_sha256=$actual_sha"
    [ "$stat_rc" -eq 0 ] || gate=1
    [ "$actual_size" = "$EXPECTED_SIZE" ] || gate=1
    [ "$actual_sha" = "$EXPECTED_SHA" ] || gate=1

    echo '--- file ---'
    file "$BIN" 2>&1
    file_rc=$?
    echo "file_exit=$file_rc"
    [ "$file_rc" -eq 0 ] || gate=1

    echo '--- ELF header ---'
    header=$(readelf -h "$BIN" 2>&1)
    header_rc=$?
    printf '%s\n' "$header"
    echo "readelf_header_exit=$header_rc"
    [ "$header_rc" -eq 0 ] || gate=1
    printf '%s\n' "$header" | grep -E 'Class:[[:space:]]+ELF32' >/dev/null || gate=1
    printf '%s\n' "$header" | grep -E 'Data:[[:space:]]+2.s complement, little endian' >/dev/null || gate=1
    printf '%s\n' "$header" | grep -E 'Machine:[[:space:]]+ARM' >/dev/null || gate=1
    printf '%s\n' "$header" | grep -E 'Flags:.*hard-float' >/dev/null || gate=1

    echo '--- program interpreter ---'
    program=$(readelf -l "$BIN" 2>&1)
    program_rc=$?
    printf '%s\n' "$program"
    interpreter=$(printf '%s\n' "$program" | sed -n 's/.*Requesting program interpreter: \(.*\)]/\1/p')
    echo "interpreter=$interpreter"
    echo "readelf_program_exit=$program_rc"
    [ "$program_rc" -eq 0 ] || gate=1
    [ "$interpreter" = '/lib/ld-linux-armhf.so.3' ] || gate=1

    echo '--- dynamic section ---'
    dynamic=$(readelf -d "$BIN" 2>&1)
    dynamic_rc=$?
    printf '%s\n' "$dynamic"
    echo "readelf_dynamic_exit=$dynamic_rc"
    [ "$dynamic_rc" -eq 0 ] || gate=1
    for needed in libpthread.so.0 libmosquitto.so.1 libc.so.6; do
        printf '%s\n' "$dynamic" | grep -F "Shared library: [$needed]" >/dev/null || gate=1
    done
    if printf '%s\n' "$dynamic" | grep -E '\((RPATH|RUNPATH)\)' >/dev/null; then
        echo 'FAIL unexpected_rpath_or_runpath=true'
        gate=1
    else
        echo 'rpath_runpath=ABSENT'
    fi
fi

echo "binary_hard_gate=$(if [ "$gate" -eq 0 ]; then echo PASS; else echo FAIL; fi)"
echo "remote_completed_at=$(date -Ins 2>/dev/null)"
exit "$gate"
